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
 * One-shot helper for a single stroke.
 *
 * Construct it in the GUI thread when the stroke is started (it prepares the
 * layer merge, which needs a temporary image that must be created in the GUI
 * thread). Then call computeSelection() from a stroke job.
 */
class KRITAUI_EXPORT KisReferenceLineClipJob
{
public:
    KisReferenceLineClipJob(KisImageSP image,
                            const QPoint &startPos,
                            const KisReferenceLineClipOptions &options);
    ~KisReferenceLineClipJob();

    /**
     * Compute the clipping selection. Must be called from a stroke job.
     *
     * @param targetDevice the device that is going to be painted on
     * @param existingSelection the currently active selection (may be null);
     *        the result is intersected with it
     * @return the selection to use for the stroke, or null if the stroke
     *         should not be clipped (e.g. there are no reference layers)
     */
    KisSelectionSP computeSelection(KisPaintDeviceSP targetDevice,
                                    KisSelectionSP existingSelection);

private:
    void releaseMergeCommand();

    struct Private;
    QScopedPointer<Private> m_d;
};

using KisReferenceLineClipJobSP = QSharedPointer<KisReferenceLineClipJob>;

#endif // KIS_REFERENCE_LINE_CLIP_H
