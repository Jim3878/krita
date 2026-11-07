/*
 *  SPDX-FileCopyrightText: 2026 Orita
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_BLEED_MANAGER_H
#define KIS_BLEED_MANAGER_H

#include <QObject>
#include <QPointer>

#include "kritaui_export.h"

class KisView;
class KisAction;
class KisActionManager;

/**
 * Handles the CSP-style bleed frame: the View menu actions, the settings
 * dialog and the canvas decoration of the current view.
 */
class KRITAUI_EXPORT KisBleedManager : public QObject
{
    Q_OBJECT
public:
    explicit KisBleedManager(QObject *parent = nullptr);
    ~KisBleedManager() override;

    void setup(KisActionManager *actionManager);
    void setView(QPointer<KisView> view);

private Q_SLOTS:
    void slotShowBleedToggled(bool value);
    void slotOpenSettings();

private:
    void syncActions();
    void updateCanvas();

private:
    QPointer<KisView> m_view;
    KisAction *m_showAction {nullptr};
    KisAction *m_settingsAction {nullptr};
};

#endif // KIS_BLEED_MANAGER_H
