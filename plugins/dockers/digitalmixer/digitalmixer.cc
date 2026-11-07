/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "digitalmixer.h"
#include "digitalmixer_dock.h"
#include "colormixing_dock.h"

#include <kpluginfactory.h>

#include <KoDockFactoryBase.h>
#include <KoDockRegistry.h>

K_PLUGIN_FACTORY_WITH_JSON(DigitalMixerPluginFactory, "krita_digitalmixer.json", registerPlugin<DigitalMixerPlugin>();)

class DigitalMixerDockFactory : public KoDockFactoryBase {
public:
    DigitalMixerDockFactory()
    {
    }

    QString id() const override
    {
        return QString( "DigitalMixer" );
    }

    virtual Qt::DockWidgetArea defaultDockWidgetArea() const
    {
        return Qt::RightDockWidgetArea;
    }

    QDockWidget* createDockWidget() override
    {
        DigitalMixerDock * dockWidget = new DigitalMixerDock();
        
        dockWidget->setObjectName(id());

        return dockWidget;
    }

    DockPosition defaultDockPosition() const override
    {
        return DockMinimized;
    }
private:


};


// Orita: CSP-style color mixing palette
class ColorMixingDockFactory : public KoDockFactoryBase {
public:
    QString id() const override
    {
        return QString("OritaColorMixing");
    }

    QDockWidget* createDockWidget() override
    {
        ColorMixingDock *dockWidget = new ColorMixingDock();
        dockWidget->setObjectName(id());
        return dockWidget;
    }

    DockPosition defaultDockPosition() const override
    {
        return DockRight;
    }
};

DigitalMixerPlugin::DigitalMixerPlugin(QObject *parent, const QVariantList &)
    : QObject(parent)
{
    KoDockRegistry::instance()->add(new DigitalMixerDockFactory());
    KoDockRegistry::instance()->add(new ColorMixingDockFactory());
}

DigitalMixerPlugin::~DigitalMixerPlugin()
{
}

#include "digitalmixer.moc"
