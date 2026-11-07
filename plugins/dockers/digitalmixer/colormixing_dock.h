/*
 *  SPDX-FileCopyrightText: 2026 Orita
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef COLOR_MIXING_DOCK_H
#define COLOR_MIXING_DOCK_H

#include <QDockWidget>
#include <QPointer>
#include <QTimer>

#include <kis_mainwindow_observer.h>

class KisScratchPad;
class KisViewManager;
class KoCanvasBase;
class QToolButton;

/**
 * CSP-style "color mixing" palette: a small area to paint and mix colors
 * with the current brush, and to sample colors from it.
 *
 * Left button paints, right button samples (or use the eyedropper toggle,
 * e.g. with a pen). The content is kept between sessions.
 */
class ColorMixingDock : public QDockWidget, public KisMainwindowObserver
{
    Q_OBJECT
public:
    ColorMixingDock();
    ~ColorMixingDock() override;

    QString observerName() override { return "ColorMixingDock"; }
    void setViewManager(KisViewManager *viewManager) override;
    void setCanvas(KoCanvasBase *canvas) override;
    void unsetCanvas() override;

private Q_SLOTS:
    void saveContent();

private:
    void setSamplingMode(bool sampling);
    QString contentFilePath() const;

private:
    KisScratchPad *m_scratchPad {nullptr};
    QToolButton *m_btnBrush {nullptr};
    QToolButton *m_btnSampler {nullptr};
    QPointer<KisViewManager> m_viewManager;
    bool m_isSetUp {false};
    QTimer m_saveTimer;
};

#endif // COLOR_MIXING_DOCK_H
