/*
 *  SPDX-FileCopyrightText: 2026 Orita
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_BLEED_CONFIG_H
#define KIS_BLEED_CONFIG_H

#include <QRectF>
#include <QString>

#include "kritaui_export.h"

class KisDocument;
class KisImage;
class QDomDocument;
class QDomElement;

/**
 * CSP-style print frame ("bleed frame") of a document.
 *
 * The finished (trim) size is centered on the canvas. Around it there is
 * the bleed area, and inside it the safe area. Sizes are in millimeters
 * and converted to pixels with the image resolution.
 *
 * The config is stored per document (as a dynamic property of KisDocument)
 * and saved in the .kra file.
 */
struct KRITAUI_EXPORT KisBleedConfig
{
    bool enabled {false};
    bool visible {true};
    double trimWidthMm {182.0};   // B5
    double trimHeightMm {257.0};
    double bleedMm {3.0};
    double safeMarginMm {10.0};

    static KisBleedConfig fromDocument(const KisDocument *doc);
    void storeInDocument(KisDocument *doc) const;

    QDomElement saveToXml(QDomDocument &doc, const QString &tag) const;
    static KisBleedConfig loadFromXml(const QDomElement &e);

    /// @param xResPerPt image resolution in pixels per point (KisImage::xRes())
    static double mmToPixels(double mm, double xResPerPt);

    /// Rects in image pixel coordinates
    QRectF trimRect(const KisImage *image) const;
    QRectF bleedRect(const KisImage *image) const;
    QRectF safeRect(const KisImage *image) const;
};

#endif // KIS_BLEED_CONFIG_H
