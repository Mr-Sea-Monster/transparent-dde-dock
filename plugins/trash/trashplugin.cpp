/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
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

#include "trashplugin.h"

TrashPlugin::TrashPlugin(QObject *parent)
    : QObject(parent),
      m_trashWidget(new TrashWidget),
      m_tipsLabel(new QLabel)
{
    m_tipsLabel->setObjectName("trash");
    m_tipsLabel->setStyleSheet("color:white;"
                               "padding: 0 3px;");

    connect(m_trashWidget, &TrashWidget::requestContextMenu, this, &TrashPlugin::showContextMenu);
}

const QString TrashPlugin::pluginName() const
{
    return "trash";
}

void TrashPlugin::init(PluginProxyInterface *proxyInter)
{
    m_proxyInter = proxyInter;

    displayModeChanged(displayMode());
}

QWidget *TrashPlugin::itemWidget(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    return m_trashWidget;
}

QWidget *TrashPlugin::itemTipsWidget(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    const int count = m_trashWidget->trashItemCount();
    if (count < 2)
        m_tipsLabel->setText(tr("Trash - %1 file").arg(count));
    else
        m_tipsLabel->setText(tr("Trash - %1 files").arg(count));

    return m_tipsLabel;
}

QWidget *TrashPlugin::itemPopupApplet(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    return nullptr;
//    return m_trashWidget->popupApplet();
}

const QString TrashPlugin::itemCommand(const QString &itemKey)
{
    Q_UNUSED(itemKey);

//    return QString();
    return "gvfs-open trash:///";
}

const QString TrashPlugin::itemContextMenu(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    return m_trashWidget->contextMenu();
}

void TrashPlugin::refershIcon(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    m_trashWidget->updateIcon();
}

void TrashPlugin::invokedMenuItem(const QString &itemKey, const QString &menuId, const bool checked)
{
    Q_UNUSED(itemKey);

    m_trashWidget->invokeMenuItem(menuId, checked);
}

int TrashPlugin::itemSortKey(const QString &itemKey)
{
    Q_UNUSED(itemKey);

    // always place at last
    return -1;
}

void TrashPlugin::displayModeChanged(const Dock::DisplayMode displayMode)
{
    if (displayMode == Dock::Fashion)
        m_proxyInter->itemAdded(this, QString());
    else
        m_proxyInter->itemRemoved(this, QString());
}

void TrashPlugin::showContextMenu()
{
    m_proxyInter->requestContextMenu(this, QString());
}
