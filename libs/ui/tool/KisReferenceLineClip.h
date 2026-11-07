/*
 *  SPDX-FileCopyrightText: 2026 Orita fork
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_REFERENCE_LINE_CLIP_H
#define KIS_REFERENCE_LINE_CLIP_H

#include <QList>
#include <QPoint>
#include <QSharedPointer>
#include <QScopedPointer>

#include "kis_types.h"
#include "kritaui_export.h"

/**
 * "Don't go over the lines of the reference layers" (similar to Clip Studio
 * Paint's "Do not exceed reference layer lines").
 *
 * When enabled, at the moment a freehand stroke starts, the color-labeled
 * layers are merged into a reference device and a flood fill is done from the
 * pen-down position. The resulting region is used as a temporary selection for
 * that single stroke, so the stroke can't paint outside the enclosed area.
 */
struct KRITAUI_EXPORT KisReferenceLineClipOptions
{
    bool enabled {false};

    /// Color label indices of the layers used as reference (0 = no label)
    QList<int> colorLabels {1, 2, 3, 4, 5, 6, 7, 8};

    /// Color similarity threshold for the flood fill (1..100)
    int threshold {8};

    /// Grow the region by this many pixels so paint goes under the lines
    int grow {1};

    /// Close gaps in the line art up to this many pixels
    int closeGap {0};

    /// Load from / save to the Krita config (shared by all freehand tools)
    static KisReferenceLineClipOptions loadFromConfig();
    void saveToConfig() const;
};

/**
 * Helper shared by the strokes (the level-of-detail preview stroke and the
 * full-resolution stroke) of a single user stroke.
 *
 * Construct it in the GUI thread when the stroke is started (it prepares the
 * layer merge, which needs a temporary image that must be created in the GUI
 * thread). Then call selectionForLod() from a stroke job.
 */
class KRITAUI_EXPORT KisReferenceLineClipJob
{
public:
    /**
     * @param image image containing the reference layers
     * @param startPos pen-down position in full-resolution image coordinates
     * @param options clipping options
     * @param existingSelection the selection that is active when the stroke
     *        starts (may be null); the result is intersected with it
     */
    KisReferenceLineClipJob(KisImageSP image,
                            const QPoint &startPos,
                            const KisReferenceLineClipOptions &options,
                            KisSelectionSP existingSelection);
    ~KisReferenceLineClipJob();

    /**
     * Returns the clipping selection, usable in a stroke working on the level
     * of detail @p levelOfDetail (0 = full resolution). The selection is
     * computed in full resolution on the first call; for lod > 0 its
     * level-of-detail plane is generated as well.
     *
     * Must be called from a stroke job.
     *
     * @param targetDevice the device that is going to be painted on
     * @return the selection, or null if the stroke should not be clipped
     *         (e.g. there are no reference layers)
     */
    KisSelectionSP selectionForLod(KisPaintDeviceSP targetDevice, int levelOfDetail);

private:
    void computeFullResolutionSelection(KisPaintDeviceSP targetDevice);
    void releaseMergeCommand();

    struct Private;
    QScopedPointer<Private> m_d;
};

using KisReferenceLineClipJobSP = QSharedPointer<KisReferenceLineClipJob>;

#endif // KIS_REFERENCE_LINE_CLIP_H
