/*
 *  SPDX-FileCopyrightText: 2026 Orita
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisBleedConfig.h"

#include <QDomDocument>
#include <QDomElement>
#include <QVariantMap>

#include "KisDocument.h"
#include "kis_image.h"

namespace {
const char *PropertyName = "oritaBleedConfig";
}

KisBleedConfig KisBleedConfig::fromDocument(const KisDocument *doc)
{
    KisBleedConfig cfg;
    if (!doc) return cfg;

    const QVariantMap map = doc->property(PropertyName).toMap();
    if (map.isEmpty()) return cfg;

    cfg.enabled = map.value("enabled", cfg.enabled).toBool();
    cfg.visible = map.value("visible", cfg.visible).toBool();
    cfg.trimWidthMm = map.value("trimWidthMm", cfg.trimWidthMm).toDouble();
    cfg.trimHeightMm = map.value("trimHeightMm", cfg.trimHeightMm).toDouble();
    cfg.bleedMm = map.value("bleedMm", cfg.bleedMm).toDouble();
    cfg.safeMarginMm = map.value("safeMarginMm", cfg.safeMarginMm).toDouble();
    return cfg;
}

void KisBleedConfig::storeInDocument(KisDocument *doc) const
{
    if (!doc) return;

    QVariantMap map;
    map["enabled"] = enabled;
    map["visible"] = visible;
    map["trimWidthMm"] = trimWidthMm;
    map["trimHeightMm"] = trimHeightMm;
    map["bleedMm"] = bleedMm;
    map["safeMarginMm"] = safeMarginMm;
    doc->setProperty(PropertyName, map);
}

QDomElement KisBleedConfig::saveToXml(QDomDocument &doc, const QString &tag) const
{
    QDomElement e = doc.createElement(tag);
    e.setAttribute("enabled", enabled ? 1 : 0);
    e.setAttribute("visible", visible ? 1 : 0);
    e.setAttribute("trimWidthMm", QString::number(trimWidthMm));
    e.setAttribute("trimHeightMm", QString::number(trimHeightMm));
    e.setAttribute("bleedMm", QString::number(bleedMm));
    e.setAttribute("safeMarginMm", QString::number(safeMarginMm));
    return e;
}

KisBleedConfig KisBleedConfig::loadFromXml(const QDomElement &e)
{
    KisBleedConfig cfg;
    cfg.enabled = e.attribute("enabled", "0").toInt() != 0;
    cfg.visible = e.attribute("visible", "1").toInt() != 0;
    cfg.trimWidthMm = e.attribute("trimWidthMm", QString::number(cfg.trimWidthMm)).toDouble();
    cfg.trimHeightMm = e.attribute("trimHeightMm", QString::number(cfg.trimHeightMm)).toDouble();
    cfg.bleedMm = e.attribute("bleedMm", QString::number(cfg.bleedMm)).toDouble();
    cfg.safeMarginMm = e.attribute("safeMarginMm", QString::number(cfg.safeMarginMm)).toDouble();
    return cfg;
}

double KisBleedConfig::mmToPixels(double mm, double xResPerPt)
{
    return mm / 25.4 * 72.0 * xResPerPt;
}

QRectF KisBleedConfig::trimRect(const KisImage *image) const
{
    if (!image) return QRectF();

    const double w = mmToPixels(trimWidthMm, image->xRes());
    const double h = mmToPixels(trimHeightMm, image->yRes());
    const QPointF center = QRectF(image->bounds()).center();
    return QRectF(center.x() - w / 2.0, center.y() - h / 2.0, w, h);
}

QRectF KisBleedConfig::bleedRect(const KisImage *image) const
{
    if (!image) return QRectF();

    const double dx = mmToPixels(bleedMm, image->xRes());
    const double dy = mmToPixels(bleedMm, image->yRes());
    return trimRect(image).adjusted(-dx, -dy, dx, dy);
}

QRectF KisBleedConfig::safeRect(const KisImage *image) const
{
    if (!image) return QRectF();

    const double dx = mmToPixels(safeMarginMm, image->xRes());
    const double dy = mmToPixels(safeMarginMm, image->yRes());
    return trimRect(image).adjusted(dx, dy, -dx, -dy);
}
