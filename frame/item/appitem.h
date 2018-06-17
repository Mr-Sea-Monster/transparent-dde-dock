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

#ifndef APPITEM_H
#define APPITEM_H

#include "dockitem.h"
#include "components/previewcontainer.h"
#include "dbus/dbusclientmanager.h"

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsItemAnimation>

#include <com_deepin_dde_daemon_dock_entry.h>

using DockEntryInter = com::deepin::dde::daemon::dock::Entry;

class AppItem : public DockItem
{
    Q_OBJECT

public:
    explicit AppItem(const QDBusObjectPath &entry, QWidget *parent = nullptr);
    ~AppItem();

    const QString appId() const;
    void updateWindowIconGeometries();
    static void setIconBaseSize(const int size);
    static int iconBaseSize();
    static int itemBaseHeight();
    static int itemBaseWidth();

    inline ItemType itemType() const { return App; }

signals:
    void requestActivateWindow(const WId wid) const;
    void requestPreviewWindow(const WId wid) const;
    void requestCancelPreview() const;

private:
    void moveEvent(QMoveEvent *e);
    void paintEvent(QPaintEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void wheelEvent(QWheelEvent *e);
    void resizeEvent(QResizeEvent *e);
    void dragEnterEvent(QDragEnterEvent *e);
    void dragMoveEvent(QDragMoveEvent *e);
    void dropEvent(QDropEvent *e);
    void leaveEvent(QEvent *e);

    void showHoverTips();
    void invokedMenuItem(const QString &itemId, const bool checked);
    const QString contextMenu() const;
    QWidget *popupTips();

    void startDrag();
    bool hasAttention() const;

private slots:
    void updateWindowInfos(const WindowInfoMap &info);
    void refershIcon();
    void activeChanged();
    void showPreview();
    void cancelAndHidePreview();
    void playSwingEffect();
    void stopSwingEffect();
    void checkAttentionEffect();

private:
    QLabel *m_appNameTips;
    PreviewContainer *m_appPreviewTips;
    DockEntryInter *m_itemEntryInter;

    QGraphicsView *m_swingEffectView;
    QGraphicsScene *m_itemScene;
    QGraphicsItemAnimation m_itemAnimation;

    bool m_dragging;
    bool m_active;
    WindowInfoMap m_windowInfos;
    QString m_id;
    QPixmap m_appIcon;
    QPixmap m_horizontalIndicator;
    QPixmap m_verticalIndicator;
    QPixmap m_activeHorizontalIndicator;
    QPixmap m_activeVerticalIndicator;

    QTimer *m_updateIconGeometryTimer;

    QFutureWatcher<QPixmap> *m_smallWatcher;
    QFutureWatcher<QPixmap> *m_largeWatcher;

    static int IconBaseSize;
    static QPoint MousePressPos;
};

#endif // APPITEM_H
