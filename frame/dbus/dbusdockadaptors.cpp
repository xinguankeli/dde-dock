/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dbusdockadaptors.h"
#include "utils.h"
#include "dockitemmanager.h"
#include "windowmanager.h"
#include "proxyplugincontroller.h"
#include "quicksettingcontroller.h"
#include "pluginsitem.h"

#include <QScreen>
#include <QDebug>
#include <QGSettings>

DBusDockAdaptors::DBusDockAdaptors(WindowManager* parent)
    : QDBusAbstractAdaptor(parent)
    , m_gsettings(Utils::SettingsPtr("com.deepin.dde.dock.mainwindow", QByteArray(), this))
    , m_windowManager(parent)
{
    connect(parent, &WindowManager::panelGeometryChanged, this, [ = ] {
        emit DBusDockAdaptors::geometryChanged(geometry());
    });

    if (m_gsettings) {
        connect(m_gsettings, &QGSettings::changed, this, [ = ] (const QString &key) {
            if (key == "onlyShowPrimary") {
                Q_EMIT showInPrimaryChanged(m_gsettings->get(key).toBool());
            }
        });
    }

    QList<PluginsItemInterface *> allPlugin = plugins();
    connect(DockItemManager::instance(), &DockItemManager::itemInserted, this, [ = ] (const int index, DockItem *item) {
        Q_UNUSED(index);
        if (item->itemType() == DockItem::Plugins
                || item->itemType() == DockItem::FixedPlugin) {
            PluginsItem *pluginItem = static_cast<PluginsItem *>(item);
            for (auto *p : allPlugin) {
                if (p->pluginName() == pluginItem->pluginName()) {
                    Q_EMIT pluginVisibleChanged(p->pluginDisplayName(), getPluginVisible(p->pluginDisplayName()));
                }
            }
        }
    });

    connect(DockItemManager::instance(), &DockItemManager::itemRemoved, this, [ = ] (DockItem *item) {
        if (item->itemType() == DockItem::Plugins
                || item->itemType() == DockItem::FixedPlugin) {
            PluginsItem *pluginItem = static_cast<PluginsItem *>(item);
            for (auto *p : allPlugin) {
                if (p->pluginName() == pluginItem->pluginName()) {
                    Q_EMIT pluginVisibleChanged(p->pluginDisplayName(), getPluginVisible(p->pluginDisplayName()));
                }
            }
        }
    });
}

DBusDockAdaptors::~DBusDockAdaptors()
{

}

void DBusDockAdaptors::callShow()
{
    m_windowManager->callShow();
}

void DBusDockAdaptors::ReloadPlugins()
{
    if (qApp->property("PLUGINSLOADED").toBool())
        return;

    // 发送事件，通知代理来加载插件
    PluginLoadEvent event;
    QCoreApplication::sendEvent(qApp, &event);

    qApp->setProperty("PLUGINSLOADED", true);
    // 退出安全模式
    qApp->setProperty("safeMode", false);
}

QStringList DBusDockAdaptors::GetLoadedPlugins()
{
    QList<PluginsItemInterface *> allPlugin = plugins();
    QStringList nameList;
    QMap<QString, QString> map;
    for (auto plugin : allPlugin) {
        // 托盘本身也是一个插件，这里去除掉这个特殊的插件,还有一些没有实际名字的插件
        if (plugin->pluginName() == "tray"
                || plugin->pluginDisplayName().isEmpty()
                || !isPluginValid(plugin->pluginName()))
            continue;

        nameList << plugin->pluginName();
        map.insert(plugin->pluginName(), plugin->pluginDisplayName());
    }

    // 排序,保持和原先任务栏右键菜单中的插件列表顺序一致
    std::sort(nameList.begin(), nameList.end(), [ = ] (const QString &name1, const QString &name2) {
        return name1 > name2;
    });

    QStringList newList;
    for (auto name : nameList) {
        newList.push_back(map[name]);
    }

    return newList;
}

void DBusDockAdaptors::resizeDock(int offset, bool dragging)
{
    m_windowManager->resizeDock(offset, dragging);
}

// 返回每个插件的识别Key(所以此值应始终不变)，供个性化插件根据key去匹配每个插件对应的图标
QString DBusDockAdaptors::getPluginKey(const QString &pluginName)
{
    QList<PluginsItemInterface *> allPlugin = plugins();
    for (auto plugin : allPlugin) {
        if (plugin->pluginDisplayName() == pluginName)
            return plugin->pluginName();
    }

    return QString();
}

bool DBusDockAdaptors::getPluginVisible(const QString &pluginName)
{
    QList<PluginsItemInterface *> allPlugin = plugins();
    for (auto *p : allPlugin) {
        if (!p->pluginIsAllowDisable())
            continue;

        const QString &display = p->pluginDisplayName();
        if (display != pluginName)
            continue;

        const QString &name = p->pluginName();
        if (!isPluginValid(name))
            continue;

        return !p->pluginIsDisable();
    }

    qInfo() << "Unable to get information about this plugin";
    return false;
}

void DBusDockAdaptors::setPluginVisible(const QString &pluginName, bool visible)
{
    QList<PluginsItemInterface *> allPlugin = plugins();
    for (auto *p : allPlugin) {
        if (!p->pluginIsAllowDisable())
            continue;

        const QString &display = p->pluginDisplayName();
        if (display != pluginName)
            continue;

        const QString &name = p->pluginName();
        if (!isPluginValid(name))
            continue;

        if (p->pluginIsDisable() == visible) {
            p->pluginStateSwitched();
            Q_EMIT pluginVisibleChanged(pluginName, visible);
        }
        return;
    }

    qInfo() << "Unable to set information for this plugin";
}

QRect DBusDockAdaptors::geometry() const
{
    return m_windowManager->geometry();
}

bool DBusDockAdaptors::showInPrimary() const
{
    return Utils::SettingValue("com.deepin.dde.dock.mainwindow", QByteArray(), "onlyShowPrimary", false).toBool();
}

void DBusDockAdaptors::setShowInPrimary(bool showInPrimary)
{
    if (this->showInPrimary() == showInPrimary)
        return;

    if (Utils::SettingSaveValue("com.deepin.dde.dock.mainwindow", QByteArray(), "onlyShowPrimary", showInPrimary)) {
        Q_EMIT showInPrimaryChanged(showInPrimary);
    }
}

bool DBusDockAdaptors::isPluginValid(const QString &name)
{
    // 插件被全局禁用时，理应获取不到此插件的任何信息
    if (!Utils::SettingValue("com.deepin.dde.dock.module." + name, QByteArray(), "enable", true).toBool())
        return false;

    // 未开启窗口特效时，不显示多任务视图插件
    if (name == "multitasking" && !DWindowManagerHelper::instance()->hasComposite())
        return false;

    // 录屏插件不显示,插件名如果有变化，建议发需求，避免任务栏反复适配
    if (name == "deepin-screen-recorder-plugin")
        return false;

    return true;
}

QList<PluginsItemInterface *> DBusDockAdaptors::plugins() const
{
    return QuickSettingController::instance()->pluginInSettings();
}
