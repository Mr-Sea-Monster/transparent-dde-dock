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

#ifndef DOCKPOPUPWINDOW_H
#define DOCKPOPUPWINDOW_H

#include "dbus/dbusdisplay.h"

#include <darrowrectangle.h>
#include <dregionmonitor.h>
#include <DWindowManagerHelper>

DWIDGET_USE_NAMESPACE

class DockPopupWindow : public Dtk::Widget::DArrowRectangle
{
    Q_OBJECT

public:
    explicit DockPopupWindow(QWidget *parent = 0);
    ~DockPopupWindow();

    bool model() const;

    void setContent(QWidget *content);

public slots:
    void show(const QPoint &pos, const bool model = false);
    void show(const int x, const int y);
    void hide();

signals:
    void accept() const;

protected:
    void showEvent(QShowEvent *e);
    void enterEvent(QEvent *e);
    bool eventFilter(QObject *o, QEvent *e);

private slots:
    void onGlobMouseRelease(const QPoint &mousePos, const int flag);
    void compositeChanged();
    void ensureRaised();

private:
    bool m_model;
    QPoint m_lastPoint;

    QTimer *m_acceptDelayTimer;

    DRegionMonitor *m_regionInter;
    DBusDisplay *m_displayInter;
    DWindowManagerHelper *m_wmHelper;
};

#endif // DOCKPOPUPWINDOW_H
