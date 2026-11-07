/*
 *  SPDX-FileCopyrightText: 2026 Orita
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "colormixing_dock.h"

#include <QButtonGroup>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QStandardPaths>
#include <QToolButton>
#include <QVBoxLayout>

#include <klocalizedstring.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

#include <KoColor.h>
#include <KoColorSpaceRegistry.h>
#include <kis_icon.h>
#include <KisViewManager.h>
#include <kis_canvas_resource_provider.h>
#include <kis_scratch_pad.h>

namespace {

const QColor swatchColors[] = {
    QColor(255, 51, 64),   // red
    QColor(255, 148, 51),  // orange
    QColor(255, 236, 64),  // yellow
    QColor(72, 210, 80),   // green
    QColor(0, 96, 110),    // teal
    QColor(40, 110, 255),  // blue
    QColor(40, 210, 255),  // cyan
    QColor(150, 60, 255),  // purple
    QColor(255, 120, 170), // pink
    QColor(120, 70, 20),   // brown
    QColor(255, 205, 190), // skin
    QColor(255, 255, 255), // white
    QColor(0, 0, 0),       // black
};

} // namespace

ColorMixingDock::ColorMixingDock()
    : QDockWidget(i18n("色彩混合"))
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    // Mode / action buttons
    QHBoxLayout *tools = new QHBoxLayout();
    tools->setSpacing(2);

    m_btnBrush = new QToolButton(page);
    m_btnBrush->setIcon(KisIconUtils::loadIcon("krita_tool_freehand"));
    m_btnBrush->setToolTip(i18n("畫顏色（左鍵畫，右鍵吸色）"));
    m_btnBrush->setCheckable(true);
    m_btnBrush->setChecked(true);
    m_btnBrush->setAutoRaise(true);

    m_btnSampler = new QToolButton(page);
    m_btnSampler->setIcon(KisIconUtils::loadIcon("krita_tool_color_sampler"));
    m_btnSampler->setToolTip(i18n("吸色（用繪圖筆時方便）"));
    m_btnSampler->setCheckable(true);
    m_btnSampler->setAutoRaise(true);

    QButtonGroup *modeGroup = new QButtonGroup(page);
    modeGroup->addButton(m_btnBrush);
    modeGroup->addButton(m_btnSampler);
    modeGroup->setExclusive(true);

    QToolButton *btnClear = new QToolButton(page);
    btnClear->setIcon(KisIconUtils::loadIcon("edit-delete"));
    btnClear->setToolTip(i18n("清除"));
    btnClear->setAutoRaise(true);

    tools->addWidget(m_btnBrush);
    tools->addWidget(m_btnSampler);
    tools->addStretch();
    tools->addWidget(btnClear);
    layout->addLayout(tools);

    // Painting area
    m_scratchPad = new KisScratchPad(page);
    m_scratchPad->setMinimumSize(120, 80);
    m_scratchPad->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_scratchPad->setEnabled(false);
    layout->addWidget(m_scratchPad, 1);

    // Basic color swatches
    QHBoxLayout *swatches = new QHBoxLayout();
    swatches->setSpacing(2);
    for (const QColor &color : swatchColors) {
        QToolButton *swatch = new QToolButton(page);
        swatch->setFixedSize(18, 18);
        swatch->setToolTip(color.name());
        swatch->setStyleSheet(QString("QToolButton { background-color: %1; border: 1px solid #222; border-radius: 3px; }")
                              .arg(color.name()));
        connect(swatch, &QToolButton::clicked, this, [this, color] () {
            if (m_viewManager && m_viewManager->canvasResourceProvider()) {
                m_viewManager->canvasResourceProvider()->slotSetFGColor(
                    KoColor(color, KoColorSpaceRegistry::instance()->rgb8()));
            }
        });
        swatches->addWidget(swatch);
    }
    swatches->addStretch();
    layout->addLayout(swatches);

    setWidget(page);

    connect(m_btnBrush, &QToolButton::toggled, this, [this] (bool checked) {
        if (checked) setSamplingMode(false);
    });
    connect(m_btnSampler, &QToolButton::toggled, this, [this] (bool checked) {
        if (checked) setSamplingMode(true);
    });
    connect(btnClear, &QToolButton::clicked, this, [this] () {
        if (!m_isSetUp) return;
        m_scratchPad->fillDefault();
        saveContent();
    });

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(1500);
    connect(&m_saveTimer, SIGNAL(timeout()), this, SLOT(saveContent()));
    connect(m_scratchPad, SIGNAL(contentChanged()), &m_saveTimer, SLOT(start()));
}

ColorMixingDock::~ColorMixingDock()
{
    if (m_saveTimer.isActive()) {
        saveContent();
    }
}

void ColorMixingDock::setViewManager(KisViewManager *viewManager)
{
    m_viewManager = viewManager;

    if (!m_isSetUp && viewManager && viewManager->canvasResourceProvider()) {
        m_scratchPad->setupScratchPad(viewManager->canvasResourceProvider(), Qt::transparent);
        m_scratchPad->setCanvasZoomLink(false);
        m_isSetUp = true;

        QImage saved(contentFilePath());
        if (!saved.isNull()) {
            KConfigGroup cfg = KSharedConfig::openConfig()->group("OritaColorMixing");
            m_scratchPad->loadScratchpadImageAt(saved, cfg.readEntry("offset", QPoint()));
        }
    }
}

void ColorMixingDock::setCanvas(KoCanvasBase *canvas)
{
    m_scratchPad->setEnabled(canvas && m_isSetUp);
}

void ColorMixingDock::unsetCanvas()
{
    m_scratchPad->setEnabled(false);
}

void ColorMixingDock::setSamplingMode(bool sampling)
{
    if (sampling) {
        m_scratchPad->setModeManually(true);
        m_scratchPad->setModeType("colorsampling");
    } else {
        m_scratchPad->setModeType("painting");
        // left button paints, right button samples
        m_scratchPad->setModeManually(false);
    }
}

QString ColorMixingDock::contentFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath("orita-color-mixing.png");
}

void ColorMixingDock::saveContent()
{
    m_saveTimer.stop();
    if (!m_isSetUp) return;

    const QImage image = m_scratchPad->copyScratchpadImageData();
    if (image.isNull()) return;

    const QString path = contentFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    image.save(path, "PNG");

    KConfigGroup cfg = KSharedConfig::openConfig()->group("OritaColorMixing");
    cfg.writeEntry("offset", m_scratchPad->scratchpadImageDataOffset());
}
