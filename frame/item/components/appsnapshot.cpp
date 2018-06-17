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

#include "appsnapshot.h"
#include "previewcontainer.h"

#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <QX11Info>
#include <QPainter>
#include <QVBoxLayout>

AppSnapshot::AppSnapshot(const WId wid, QWidget *parent)
    : QWidget(parent),

      m_wid(wid),

      m_title(new QLabel),
      m_closeBtn(new DImageButton),

      m_wmHelper(DWindowManagerHelper::instance())
{
    m_closeBtn->setFixedSize(24, 24);
    m_closeBtn->setNormalPic(":/icons/resources/close_round_normal.svg");
    m_closeBtn->setHoverPic(":/icons/resources/close_round_hover.svg");
    m_closeBtn->setPressPic(":/icons/resources/close_round_press.svg");
    m_closeBtn->setVisible(false);
    m_title->setStyleSheet("color: white;");

    QHBoxLayout *centralLayout = new QHBoxLayout;
    centralLayout->addWidget(m_title);
    centralLayout->addWidget(m_closeBtn);
    centralLayout->setSpacing(5);
    centralLayout->setMargin(0);

    setLayout(centralLayout);
    setAcceptDrops(true);
    resize(SNAP_WIDTH, SNAP_HEIGHT);

    connect(m_closeBtn, &DImageButton::clicked, this, &AppSnapshot::closeWindow, Qt::QueuedConnection);
    connect(m_wmHelper, &DWindowManagerHelper::hasCompositeChanged, this, &AppSnapshot::compositeChanged, Qt::QueuedConnection);

    QTimer::singleShot(1, this, &AppSnapshot::compositeChanged);
}

void AppSnapshot::closeWindow() const
{
    const auto display = QX11Info::display();

    XEvent e;

    memset(&e, 0, sizeof(e));
    e.xclient.type = ClientMessage;
    e.xclient.window = m_wid;
    e.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", true);
    e.xclient.format = 32;
    e.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", false);
    e.xclient.data.l[1] = CurrentTime;

    XSendEvent(display, m_wid, false, NoEventMask, &e);
    XFlush(display);
}

void AppSnapshot::compositeChanged() const
{
    const bool composite = m_wmHelper->hasComposite();

    m_title->setVisible(!composite);

    QTimer::singleShot(1, this, &AppSnapshot::fetchSnapshot);
}

void AppSnapshot::setWindowInfo(const WindowInfo &info)
{
    m_windowInfo = info;

    m_title->setText(m_windowInfo.title);
}

void AppSnapshot::dragEnterEvent(QDragEnterEvent *e)
{
    QWidget::dragEnterEvent(e);

    if (m_wmHelper->hasComposite())
        emit entered(m_wid);
}

void AppSnapshot::fetchSnapshot()
{
    if (!m_wmHelper->hasComposite())
        return;

    const auto display = QX11Info::display();

    Window unused_window;
    int unused_int;
    unsigned unused_uint, w, h;
    XGetGeometry(display, m_wid, &unused_window, &unused_int, &unused_int, &w, &h, &unused_uint, &unused_uint);
    XImage *ximage = XGetImage(display, m_wid, 0, 0, w, h, AllPlanes, ZPixmap);
    if (!ximage)
    {
        emit requestCheckWindow();
        return;
    }

    const QImage qimage((const uchar*)(ximage->data), ximage->width, ximage->height, ximage->bytes_per_line, QImage::Format_RGB32);
    Q_ASSERT(!qimage.isNull());

    const Atom gtk_frame_extents = XInternAtom(display, "_GTK_FRAME_EXTENTS", true);
    Atom actual_type_return;
    int actual_format_return;
    unsigned long n_items_return;
    unsigned long bytes_after_return;
    unsigned char *prop_to_return;

    const auto r = XGetWindowProperty(display, m_wid, gtk_frame_extents, 0, 4, false, XA_CARDINAL,
                                      &actual_type_return, &actual_format_return, &n_items_return, &bytes_after_return, &prop_to_return);
    if (!r && prop_to_return && n_items_return == 4 && actual_format_return == 32)
    {
        const unsigned long *extents = reinterpret_cast<const unsigned long *>(prop_to_return);
        const int left = extents[0];
        const int right = extents[1];
        const int top = extents[2];
        const int bottom = extents[3];
        const int width = qimage.width();
        const int height = qimage.height();

        m_snapshot = qimage.copy(left, top, width - left - right, height - top - bottom);
    } else {
        m_snapshot = qimage.copy();
    }

    const auto size = rect().marginsRemoved(QMargins(8, 8, 8, 8)).size();
    const auto ratio = devicePixelRatioF();
    m_snapshot = m_snapshot.scaled(size * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_snapshot.setDevicePixelRatio(ratio);

    XDestroyImage(ximage);
    XFree(prop_to_return);

    update();
}

void AppSnapshot::enterEvent(QEvent *e)
{
    QWidget::enterEvent(e);

    if (!m_wmHelper->hasComposite())
        m_closeBtn->setVisible(true);
    else
        emit entered(m_wid);
}

void AppSnapshot::leaveEvent(QEvent *e)
{
    QWidget::leaveEvent(e);

    m_closeBtn->setVisible(false);
}

void AppSnapshot::paintEvent(QPaintEvent *e)
{
    QWidget::paintEvent(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!m_wmHelper->hasComposite())
    {
        if (underMouse())
            painter.fillRect(rect(), QColor(255, 255, 255, 255 * .2));
        return;
    }

    if (m_snapshot.isNull())
        return;

    const QRect r = rect().marginsRemoved(QMargins(8, 8, 8, 8));
    const auto ratio = devicePixelRatioF();

    // draw attention background
    if (m_windowInfo.attention)
    {
        painter.setBrush(QColor(241, 138, 46, 255 * .8));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect(), 5, 5);
    }

    // draw image
    const QImage &im = m_snapshot;

    const QRect ir = im.rect();
    const int offset_x = r.x() + r.width() / 2 - ir.width() / ratio / 2;
    const int offset_y = r.y() + r.height() / 2 - ir.height() / ratio / 2;
    painter.drawImage(offset_x, offset_y, im);
}

void AppSnapshot::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);

    QTimer::singleShot(1, this, &AppSnapshot::fetchSnapshot);
}

void AppSnapshot::mousePressEvent(QMouseEvent *e)
{
    QWidget::mousePressEvent(e);

    emit clicked(m_wid);
}
