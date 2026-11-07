/*
 *  SPDX-FileCopyrightText: 2026 Orita fork
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisReferenceLineClip.h"

#include <QStringList>
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

struct KisReferenceLineClipJob::Private
{
    QPoint startPos;
    QRect imageBounds;
    KisReferenceLineClipOptions options;
    KisPaintDeviceSP referenceDevice;
    QScopedPointer<KisMergeLabeledLayersCommand> mergeCommand;
};

KisReferenceLineClipJob::KisReferenceLineClipJob(KisImageSP image,
                                                 const QPoint &startPos,
                                                 const KisReferenceLineClipOptions &options)
    : m_d(new Private)
{
    m_d->startPos = startPos;
    m_d->imageBounds = image->bounds();
    m_d->options = options;
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

KisSelectionSP KisReferenceLineClipJob::computeSelection(KisPaintDeviceSP targetDevice,
                                                         KisSelectionSP existingSelection)
{
    if (!m_d->mergeCommand || !m_d->imageBounds.contains(m_d->startPos)) {
        return nullptr;
    }

    m_d->mergeCommand->redo();
    releaseMergeCommand();

    // no color-labeled layers (or they are empty): nothing to clip against
    if (m_d->referenceDevice->exactBounds().isEmpty()) {
        return nullptr;
    }

    KisPixelSelectionSP region =
        new KisPixelSelection(new KisSelectionDefaultBounds(targetDevice));

    KisFillPainter fillPainter(targetDevice);
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

    if (existingSelection) {
        region->applySelection(existingSelection->pixelSelection(), SELECTION_INTERSECT);
    }

    KisSelectionSP selection =
        new KisSelection(new KisSelectionDefaultBounds(targetDevice),
                         KisImageResolutionProxy::identity());
    selection->pixelSelection()->applySelection(region, SELECTION_REPLACE);
    selection->updateProjection();

    return selection;
}
