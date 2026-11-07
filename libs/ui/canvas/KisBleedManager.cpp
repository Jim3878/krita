/*
 *  SPDX-FileCopyrightText: 2026 Orita
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisBleedManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

#include <klocalizedstring.h>

#include "KisBleedConfig.h"
#include "KisDocument.h"
#include "KisView.h"
#include "kis_action.h"
#include "kis_action_manager.h"
#include "kis_canvas2.h"
#include "kis_canvas_decoration.h"
#include "kis_coordinates_converter.h"
#include "kis_image.h"

namespace {

const QString BLEED_DECORATION_ID = "orita-bleed-decoration";

class KisBleedDecoration : public KisCanvasDecoration
{
public:
    KisBleedDecoration(QPointer<KisView> view)
        : KisCanvasDecoration(BLEED_DECORATION_ID, view)
    {
        setVisible(true);
        setPriority(5);
    }

protected:
    void drawDecoration(QPainter &gc, const QRectF &updateArea,
                        const KisCoordinatesConverter *converter, KisCanvas2 *canvas) override
    {
        Q_UNUSED(updateArea);
        Q_UNUSED(canvas);

        if (!view()) return;
        const KisBleedConfig cfg = KisBleedConfig::fromDocument(view()->document());
        if (!cfg.enabled || !cfg.visible) return;

        KisImageSP image = view()->image();
        if (!image) return;

        const QRectF bleed = converter->imageToWidget(cfg.bleedRect(image.data()));
        const QRectF trim = converter->imageToWidget(cfg.trimRect(image.data()));
        const QRectF safe = converter->imageToWidget(cfg.safeRect(image.data()));

        gc.save();
        gc.setTransform(QTransform());
        gc.setRenderHint(QPainter::Antialiasing, false);
        gc.setBrush(Qt::NoBrush);

        const int thickness = decorationThickness();

        // Shade the area that will be cut off (outside the trim frame)
        {
            QPainterPath outside;
            outside.addRect(converter->imageToWidget(QRectF(image->bounds())).united(bleed));
            QPainterPath inner;
            inner.addRect(trim);
            gc.fillPath(outside.subtracted(inner), QColor(0, 90, 200, 28));
        }

        // Bleed frame (outer)
        gc.setPen(QPen(QColor(0, 160, 230), thickness, Qt::SolidLine));
        gc.drawRect(bleed);

        // Trim (finished size) frame
        gc.setPen(QPen(QColor(40, 70, 220), thickness, Qt::SolidLine));
        gc.drawRect(trim);

        // Safe area (inner) frame
        if (safe.isValid()) {
            gc.setPen(QPen(QColor(40, 70, 220), thickness, Qt::DashLine));
            gc.drawRect(safe);
        }

        gc.restore();
    }
};

struct PaperPreset {
    const char *name;
    double width;
    double height;
};

const PaperPreset presets[] = {
    {"B5（漫畫原稿 182×257）", 182.0, 257.0},
    {"A4（210×297）", 210.0, 297.0},
    {"A5（148×210）", 148.0, 210.0},
    {"B4（257×364）", 257.0, 364.0},
    {"B6（128×182）", 128.0, 182.0},
};

} // namespace

KisBleedManager::KisBleedManager(QObject *parent)
    : QObject(parent)
{
}

KisBleedManager::~KisBleedManager()
{
}

void KisBleedManager::setup(KisActionManager *actionManager)
{
    m_showAction = actionManager->createAction("orita_show_bleed");
    connect(m_showAction, SIGNAL(toggled(bool)), this, SLOT(slotShowBleedToggled(bool)));

    m_settingsAction = actionManager->createAction("orita_bleed_settings");
    connect(m_settingsAction, SIGNAL(triggered()), this, SLOT(slotOpenSettings()));

    syncActions();
}

void KisBleedManager::setView(QPointer<KisView> view)
{
    m_view = view;

    if (m_view && m_view->canvasBase()) {
        if (!m_view->canvasBase()->decoration(BLEED_DECORATION_ID)) {
            m_view->canvasBase()->addDecoration(new KisBleedDecoration(m_view));
        }
    }

    syncActions();
}

void KisBleedManager::syncActions()
{
    if (!m_showAction) return;

    const KisBleedConfig cfg =
        KisBleedConfig::fromDocument(m_view ? m_view->document() : nullptr);

    const bool blocked = m_showAction->blockSignals(true);
    m_showAction->setEnabled(m_view && cfg.enabled);
    m_showAction->setChecked(cfg.enabled && cfg.visible);
    m_showAction->blockSignals(blocked);
}

void KisBleedManager::updateCanvas()
{
    if (m_view && m_view->canvasBase()) {
        m_view->canvasBase()->updateCanvas();
    }
}

void KisBleedManager::slotShowBleedToggled(bool value)
{
    if (!m_view) return;

    KisBleedConfig cfg = KisBleedConfig::fromDocument(m_view->document());
    if (!cfg.enabled) return;

    cfg.visible = value;
    cfg.storeInDocument(m_view->document());
    m_view->document()->setModified(true);
    updateCanvas();
}

void KisBleedManager::slotOpenSettings()
{
    if (!m_view) return;

    KisDocument *document = m_view->document();
    KisImageSP image = m_view->image();
    if (!document || !image) return;

    KisBleedConfig cfg = KisBleedConfig::fromDocument(document);

    QDialog dlg(m_view);
    dlg.setWindowTitle(i18n("出血框設定"));

    QVBoxLayout *mainLayout = new QVBoxLayout(&dlg);

    QCheckBox *chkEnabled = new QCheckBox(i18n("使用出血框"), &dlg);
    chkEnabled->setChecked(cfg.enabled);
    mainLayout->addWidget(chkEnabled);

    QFormLayout *form = new QFormLayout();
    mainLayout->addLayout(form);

    QComboBox *cmbPreset = new QComboBox(&dlg);
    cmbPreset->addItem(i18n("自訂"));
    for (const PaperPreset &p : presets) {
        cmbPreset->addItem(QString::fromUtf8(p.name));
    }
    form->addRow(i18n("紙張:"), cmbPreset);

    auto makeSpin = [&dlg] (double value, double max) {
        QDoubleSpinBox *spin = new QDoubleSpinBox(&dlg);
        spin->setRange(0.0, max);
        spin->setDecimals(1);
        spin->setSingleStep(1.0);
        spin->setSuffix(" mm");
        spin->setValue(value);
        return spin;
    };

    QDoubleSpinBox *spinWidth = makeSpin(cfg.trimWidthMm, 2000.0);
    QDoubleSpinBox *spinHeight = makeSpin(cfg.trimHeightMm, 2000.0);
    QDoubleSpinBox *spinBleed = makeSpin(cfg.bleedMm, 100.0);
    QDoubleSpinBox *spinSafe = makeSpin(cfg.safeMarginMm, 500.0);

    form->addRow(i18n("完成尺寸 寬:"), spinWidth);
    form->addRow(i18n("完成尺寸 高:"), spinHeight);
    form->addRow(i18n("出血寬度:"), spinBleed);
    form->addRow(i18n("安全範圍（往內）:"), spinSafe);

    QLabel *lblInfo = new QLabel(&dlg);
    lblInfo->setWordWrap(true);
    mainLayout->addWidget(lblInfo);

    QPushButton *btnResize = new QPushButton(i18n("把畫布大小設成「完成尺寸＋出血」"), &dlg);
    btnResize->setToolTip(i18n("以畫布中心為準，加大或裁切畫布。可以復原。"));
    mainLayout->addWidget(btnResize);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    mainLayout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto currentConfig = [&] () {
        KisBleedConfig c = cfg;
        c.enabled = chkEnabled->isChecked();
        c.trimWidthMm = spinWidth->value();
        c.trimHeightMm = spinHeight->value();
        c.bleedMm = spinBleed->value();
        c.safeMarginMm = spinSafe->value();
        if (c.enabled && !cfg.enabled) {
            c.visible = true;
        }
        return c;
    };

    auto neededCanvasSize = [&] (const KisBleedConfig &c) {
        return c.bleedRect(image.data()).size().toSize();
    };

    auto updateInfo = [&] () {
        const KisBleedConfig c = currentConfig();
        const QSize needed = neededCanvasSize(c);
        const double dpi = image->xRes() * 72.0;
        lblInfo->setText(i18n("解析度 %1 dpi。完成尺寸＋出血 = %2 × %3 px，目前畫布 %4 × %5 px。",
                              qRound(dpi),
                              needed.width(), needed.height(),
                              image->width(), image->height()));

        const bool on = chkEnabled->isChecked();
        cmbPreset->setEnabled(on);
        spinWidth->setEnabled(on);
        spinHeight->setEnabled(on);
        spinBleed->setEnabled(on);
        spinSafe->setEnabled(on);
        btnResize->setEnabled(on && needed != image->size() && !needed.isEmpty());
    };

    auto syncPreset = [&] () {
        int index = 0;
        for (int i = 0; i < int(sizeof(presets) / sizeof(presets[0])); i++) {
            if (qFuzzyCompare(presets[i].width, spinWidth->value()) &&
                qFuzzyCompare(presets[i].height, spinHeight->value())) {
                index = i + 1;
                break;
            }
        }
        const bool blocked = cmbPreset->blockSignals(true);
        cmbPreset->setCurrentIndex(index);
        cmbPreset->blockSignals(blocked);
    };

    connect(cmbPreset, QOverload<int>::of(&QComboBox::currentIndexChanged), &dlg, [&] (int index) {
        if (index <= 0) return;
        const PaperPreset &p = presets[index - 1];
        spinWidth->setValue(p.width);
        spinHeight->setValue(p.height);
    });

    for (QDoubleSpinBox *spin : {spinWidth, spinHeight, spinBleed, spinSafe}) {
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), &dlg, [&] () {
            syncPreset();
            updateInfo();
        });
    }
    connect(chkEnabled, &QCheckBox::toggled, &dlg, [&] () { updateInfo(); });

    connect(btnResize, &QPushButton::clicked, &dlg, [&] () {
        const QSize needed = neededCanvasSize(currentConfig());
        if (needed.isEmpty()) return;

        // Keep the canvas center in place: the rect is in old image coordinates
        const QRect newRect((image->width() - needed.width()) / 2,
                            (image->height() - needed.height()) / 2,
                            needed.width(), needed.height());
        image->resizeImage(newRect);
        image->waitForDone();
        updateInfo();
    });

    syncPreset();
    updateInfo();

    if (dlg.exec() != QDialog::Accepted) return;

    currentConfig().storeInDocument(document);
    document->setModified(true);

    syncActions();
    updateCanvas();
}
