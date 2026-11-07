/*
 *  SPDX-FileCopyrightText: 2026 Orita
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisToolEraser.h"

#include <QWeakPointer>

#include <klocalizedstring.h>
#include <ksharedconfig.h>
#include <kconfiggroup.h>

#include <KoCanvasResourceProvider.h>
#include <KisResourceModel.h>
#include <KisResourceTypes.h>

#include "kis_paintop_preset.h"
#include "kis_paintop_box.h"
#include "canvas/kis_canvas2.h"
#include "KisViewManager.h"

namespace {

// Presets are shared by all views, so the "remembered" presets are global too.
// Weak pointers keep unsaved (dirty) changes to a preset during the session,
// the names are used as a fallback and between sessions.
QWeakPointer<KisPaintOpPreset> s_eraserPreset;
QWeakPointer<KisPaintOpPreset> s_brushPreset;

const char *ConfigGroup = "OritaEraserTool";
const char *DefaultEraserPreset = "a) Eraser Circle";

KisPaintOpPresetSP findPreset(const QString &name)
{
    if (name.isEmpty()) return KisPaintOpPresetSP();

    KisResourceModel model(ResourceType::PaintOpPresets);
    const QVector<KoResourceSP> resources = model.resourcesForName(name);
    for (const KoResourceSP &resource : resources) {
        KisPaintOpPresetSP preset = resource.dynamicCast<KisPaintOpPreset>();
        if (preset) return preset;
    }
    return KisPaintOpPresetSP();
}

} // namespace

KisToolEraser::KisToolEraser(KoCanvasBase *canvas)
    : KisToolBrush(canvas)
{
    setObjectName("tool_orita_eraser");
}

KisToolEraser::~KisToolEraser()
{
}

void KisToolEraser::activate(const QSet<KoShape*> &shapes)
{
    switchPresets(true);
    KisToolBrush::activate(shapes);
}

void KisToolEraser::deactivate()
{
    KisToolBrush::deactivate();
    switchPresets(false);
}

void KisToolEraser::switchPresets(bool toEraser)
{
    KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2*>(canvas());
    if (!kisCanvas || !kisCanvas->viewManager()) return;

    KisPaintopBox *paintOpBox = kisCanvas->viewManager()->paintOpBox();
    KoCanvasResourceProvider *resources = canvas()->resourceManager();
    if (!paintOpBox || !resources) return;

    KConfigGroup cfg = KSharedConfig::openConfig()->group(ConfigGroup);

    KisPaintOpPresetSP current =
        resources->resource(KoCanvasResource::CurrentPaintOpPreset).value<KisPaintOpPresetSP>();

    if (toEraser) {
        if (current) {
            s_brushPreset = current;
            cfg.writeEntry("brushPreset", current->name());
        }

        KisPaintOpPresetSP eraser = s_eraserPreset.toStrongRef();
        if (!eraser) {
            eraser = findPreset(cfg.readEntry("eraserPreset", QString(DefaultEraserPreset)));
        }
        if (!eraser) {
            eraser = findPreset(DefaultEraserPreset);
        }

        if (eraser && eraser != current) {
            paintOpBox->resourceSelected(eraser);
        }

        resources->setResource(KoCanvasResource::EraserMode, true);
    } else {
        if (current) {
            s_eraserPreset = current;
            cfg.writeEntry("eraserPreset", current->name());
        }

        resources->setResource(KoCanvasResource::EraserMode, false);

        KisPaintOpPresetSP brush = s_brushPreset.toStrongRef();
        if (!brush) {
            brush = findPreset(cfg.readEntry("brushPreset", QString()));
        }

        if (brush && brush != current) {
            paintOpBox->resourceSelected(brush);
        }
    }
}

KisToolEraserFactory::KisToolEraserFactory()
    : KisToolBrushFactory("KritaShape/OritaToolEraser")
{
    setToolTip(i18n("橡皮擦工具"));
    setSection(ToolBoxSection::Shape);
    setIconName(koIconNameCStr("draw-eraser"));
    setShortcut(QKeySequence(Qt::Key_E));
    setPriority(1);
    setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
}

KisToolEraserFactory::~KisToolEraserFactory()
{
}

KoToolBase *KisToolEraserFactory::createTool(KoCanvasBase *canvas)
{
    return new KisToolEraser(canvas);
}

QList<QAction *> KisToolEraserFactory::createActionsImpl(KoToolManager *toolManager)
{
    // The brush tool already registers the smoothing actions
    Q_UNUSED(toolManager);
    return QList<QAction *>();
}
