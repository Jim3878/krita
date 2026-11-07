/*
 *  SPDX-FileCopyrightText: 2026 Orita
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_ERASER_H
#define KIS_TOOL_ERASER_H

#include "kis_tool_brush.h"

/**
 * CSP-style standalone eraser tool.
 *
 * It paints like the freehand brush tool, but always in eraser mode and with
 * its own brush preset: activating it switches to the last preset used for
 * erasing, deactivating it switches back to the preset that was used before.
 */
class KisToolEraser : public KisToolBrush
{
    Q_OBJECT
public:
    KisToolEraser(KoCanvasBase *canvas);
    ~KisToolEraser() override;

public Q_SLOTS:
    void activate(const QSet<KoShape*> &shapes) override;
    void deactivate() override;

private:
    void switchPresets(bool toEraser);
};

class KisToolEraserFactory : public KisToolBrushFactory
{
public:
    KisToolEraserFactory();
    ~KisToolEraserFactory() override;

    KoToolBase *createTool(KoCanvasBase *canvas) override;
    QList<QAction *> createActionsImpl(KoToolManager *toolManager) override;
};

#endif // KIS_TOOL_ERASER_H
