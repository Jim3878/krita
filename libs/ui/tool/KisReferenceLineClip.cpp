/*
 *  SPDX-FileCopyrightText: 2026 Orita fork
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisReferenceLineClip.h"

#include <QStringList>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <ksharedconfig.h>
#include <kconfiggroup.h>

#include "kis_image.h"
#include "kis_paint_device.h"
#include "kis_selection.h"
#include "kis_pixel_selection.h"
#include "kis_fill_painter.h"
#include "kis_global.h"
#include "kis_default_bounds.h"
#include "KisImageResolutionProxy.h"
#include "KisDeleteLaterWrapper.h"
#include <commands_new/KisMergeLabeledLayersCommand.h>

namespace {
const char *configGroupName = "ReferenceLineClip";
}

KisReferenceLineClipOptions KisReferenceLineClipOptions::loadFromConfig()
{
    KisReferenceLineClipOptions o;
    KConfigGroup cfg = KSharedConfig::openConfig()->group(configGroupName);

    o.enabled = cfg.readEntry("enabled", o.enabled);
    o.threshold = qBound(1, cfg.readEntry("threshold", o.threshold), 100);
    o.grow = qBound(0, cfg.readEntry("grow", o.grow), 20);
    o.closeGap = qBound(0, cfg.readEntry("closeGap", o.closeGap), 32);

    if (cfg.hasKey("colorLabels")) {
        o.colorLabels.clear();
        const QStringList labels =
            cfg.readEntry("colorLabels", QString()).split(',', Qt::SkipEmptyParts);
        for (const QString &s : labels) {
            bool ok = false;
            const int label = s.toInt(&ok);
            if (ok) {
                o.colorLabels << label;
            }
        }
    }
    return o;
}

void KisReferenceLineClipOptions::saveToConfig() const
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group(configGroupName);
    cfg.writeEntry("enabled", enabled);
    cfg.writeEntry("threshold", threshold);
    cfg.writeEntry("grow", grow);
    cfg.writeEntry("closeGap", closeGap);

    QStringList labels;
    for (int label : colorLabels) {
        labels << QString::number(label);
    }
    cfg.writeEntry("colorLabels", labels.join(','));
}

namespace {
/**
 * Image bounds that always report the full-resolution level of detail.
 *
 * While a level-of-detail (preview) stroke is running, every device whose
 * default bounds point to the image reads and writes its LOD plane. The
 * clipping selection is computed in full resolution, so the temporary devices
 * used for the computation use these bounds instead.
 */
class FullResolutionBounds : public KisDefaultBounds
{
public:
    FullResolutionBounds(KisImageWSP image) : KisDefaultBounds(image) {}
    int currentLevelOfDetail() const override { return 0; }
};
}

struct KisReferenceLineClipJob::Private
{
    QMutex mutex;

    QPoint startPos;
    QRect imageBounds;
    KisReferenceLineClipOptions options;
    KisDefaultBoundsBaseSP fullResBounds;
    KisSelectionSP existingSelection;
    KisPaintDeviceSP referenceDevice;
    QScopedPointer<KisMergeLabeledLayersCommand> mergeCommand;

    bool computed {false};
    KisSelectionSP selection;
    QSet<int> uploadedLods;
};

KisReferenceLineClipJob::KisReferenceLineClipJob(KisImageSP image,
                                                 const QPoint &startPos,
                                                 const KisReferenceLineClipOptions &options,
                                                 KisSelectionSP existingSelection)
    : m_d(new Private)
{
    m_d->startPos = startPos;
    m_d->imageBounds = image->bounds();
    m_d->options = options;
    m_d->fullResBounds = new FullResolutionBounds(image);
    m_d->existingSelection = existingSelection;
    m_d->referenceDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(
        image, "Reference Line Clip Reference Device");

    // The command creates a temporary image, so it must be constructed here,
    // in the GUI thread. The actual merge happens in computeSelection().
    m_d->mergeCommand.reset(new KisMergeLabeledLayersCommand(
        image,
        m_d->referenceDevice,
        options.colorLabels,
        KisMergeLabeledLayersCommand::GroupSelectionPolicy_SelectIfColorLabeled));
}

KisReferenceLineClipJob::~KisReferenceLineClipJob()
{
    releaseMergeCommand();
}

void KisReferenceLineClipJob::releaseMergeCommand()
{
    // The command may still own a temporary KisImage, which must be deleted
    // in the GUI thread (it has timers), while we may be in a stroke job here.
    if (m_d->mergeCommand) {
        makeKisDeleteLaterWrapper(m_d->mergeCommand.take())->deleteLater();
    }
}

KisSelectionSP KisReferenceLineClipJob::selectionForLod(KisPaintDeviceSP targetDevice,
                                                        int levelOfDetail)
{
    QMutexLocker l(&m_d->mutex);

    if (!m_d->computed) {
        m_d->computed = true;
        computeFullResolutionSelection(targetDevice);
    }

    if (m_d->selection && levelOfDetail > 0 && !m_d->uploadedLods.contains(levelOfDetail)) {
        // generate the preview (LOD) plane of the selection, the same way
        // KisSyncLodCacheStrokeStrategy does it for the image devices
        KisPixelSelectionSP pixelSelection = m_d->selection->pixelSelection();
        QScopedPointer<KisPaintDevice::LodDataStruct> data(
            pixelSelection->createLodDataStruct(levelOfDetail));
        pixelSelection->updateLodDataStruct(data.data(), m_d->imageBounds);
        pixelSelection->uploadLodDataStruct(data.data());
        m_d->uploadedLods.insert(levelOfDetail);
    }

    return m_d->selection;
}

void KisReferenceLineClipJob::computeFullResolutionSelection(KisPaintDeviceSP targetDevice)
{
    if (!m_d->mergeCommand || !m_d->imageBounds.contains(m_d->startPos)) {
        return;
    }

    // layer clones used by the merge always take the full-resolution data
    m_d->mergeCommand->redo();
    releaseMergeCommand();

    // no color-labeled layers (or they are empty): nothing to clip against
    if (m_d->referenceDevice->exactBounds().isEmpty()) {
        return;
    }

    KisPixelSelectionSP region = new KisPixelSelection(m_d->fullResBounds);

    KisFillPainter fillPainter(m_d->referenceDevice);
    fillPainter.setWidth(m_d->imageBounds.width());
    fillPainter.setHeight(m_d->imageBounds.height());
    fillPainter.setRegionFillingMode(KisFillPainter::RegionFillingMode_FloodFill);
    fillPainter.setFillThreshold(m_d->options.threshold);
    fillPainter.setOpacitySpread(100);
    fillPainter.setAntiAlias(true);
    fillPainter.setFeather(0);
    fillPainter.setCloseGap(m_d->options.closeGap);
    fillPainter.setSizemod(m_d->options.grow);
    fillPainter.setStopGrowingAtDarkestPixel(false);
    fillPainter.setUseCompositing(true);
    fillPainter.setUseSelectionAsBoundary(false);
    fillPainter.createFloodSelection(region,
                                     m_d->startPos.x(), m_d->startPos.y(),
                                     m_d->referenceDevice,
                                     nullptr);

    if (m_d->existingSelection) {
        // the copy takes the full-resolution data of the existing selection
        KisPixelSelectionSP existing =
            new KisPixelSelection(*m_d->existingSelection->projection());
        existing->setDefaultBounds(m_d->fullResBounds);
        region->applySelection(existing, SELECTION_INTERSECT);
    }

    KisSelectionSP selection =
        new KisSelection(m_d->fullResBounds, KisImageResolutionProxy::identity());
    selection->pixelSelection()->applySelection(region, SELECTION_REPLACE);

    // from now on the selection follows the level of detail of the painted
    // device, so that the preview stroke reads its LOD plane
    selection->setDefaultBounds(new KisSelectionDefaultBounds(targetDevice));
    selection->updateProjection();

    m_d->selection = selection;
}
