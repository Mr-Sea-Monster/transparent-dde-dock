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

#include "appitem.h"

#include "util/themeappicon.h"
#include "util/imagefactory.h"
#include "xcb/xcb_misc.h"

#include <X11/X.h>
#include <X11/Xlib.h>

#include <QPainter>
#include <QDrag>
#include <QMouseEvent>
#include <QApplication>
#include <QHBoxLayout>
#include <QGraphicsScene>
#include <QTimeLine>
#include <QX11Info>

#define APP_DRAG_THRESHOLD      20

const static qreal Frames[] = { 0,
                                0.327013,
                                0.987033,
                                1.77584,
                                2.61157,
                                3.45043,
                                4.26461,
                                5.03411,
                                5.74306,
                                6.37782,
                                6.92583,
                                7.37484,
                                7.71245,
                                7.92557,
                                8, 7.86164,
                                7.43184,
                                6.69344,
                                5.64142,
                                4.2916,
                                2.68986,
                                0.91694,
                                -0.91694,
                                -2.68986,
                                -4.2916,
                                -5.64142,
                                -6.69344,
                                -7.43184,
                                -7.86164,
                                -8,
                                -7.86164,
                                -7.43184,
                                -6.69344,
                                -5.64142,
                                -4.2916,
                                -2.68986,
                                -0.91694,
                                0.91694,
                                2.68986,
                                4.2916,
                                5.64142,
                                6.69344,
                                7.43184,
                                7.86164,
                                8,
                                7.93082,
                                7.71592,
                                7.34672,
                                6.82071,
                                6.1458,
                                5.34493,
                                4.45847,
                                3.54153,
                                2.65507,
                                1.8542,
                                1.17929,
                                0.653279,
                                0.28408,
                                0.0691776,
                                0,
                              };

int AppItem::IconBaseSize;
QPoint AppItem::MousePressPos;

AppItem::AppItem(const QDBusObjectPath &entry, QWidget *parent)
    : DockItem(parent),
      m_appNameTips(new QLabel(this)),
      m_appPreviewTips(new PreviewContainer(this)),
      m_itemEntryInter(new DockEntryInter("com.deepin.dde.daemon.Dock", entry.path(), QDBusConnection::sessionBus(), this)),

      m_swingEffectView(new QGraphicsView(this)),
      m_itemScene(new QGraphicsScene(this)),

      m_dragging(false),

      m_appIcon(QPixmap()),

      m_horizontalIndicator(QPixmap(":/indicator/resources/indicator.png")),
      m_verticalIndicator(QPixmap(":/indicator/resources/indicator_ver.png")),
      m_activeHorizontalIndicator(QPixmap(":/indicator/resources/indicator_active.png")),
      m_activeVerticalIndicator(QPixmap(":/indicator/resources/indicator_active_ver.png")),
      m_updateIconGeometryTimer(new QTimer(this)),

      m_smallWatcher(new QFutureWatcher<QPixmap>(this)),
      m_largeWatcher(new QFutureWatcher<QPixmap>(this))
{
    m_swingEffectView->setAttribute(Qt::WA_TransparentForMouseEvents);

    QHBoxLayout *centralLayout = new QHBoxLayout;
    centralLayout->addWidget(m_swingEffectView);
    centralLayout->setMargin(0);
    centralLayout->setSpacing(0);

    setAccessibleName(m_itemEntryInter->name());
    setAcceptDrops(true);
    setLayout(centralLayout);

    m_swingEffectView->setScene(m_itemScene);
    m_swingEffectView->setAlignment(Qt::AlignCenter);
    m_swingEffectView->setVisible(false);
    m_swingEffectView->setFrameStyle(QFrame::NoFrame);
    m_swingEffectView->setContentsMargins(0, 0, 0, 0);
    m_swingEffectView->setRenderHints(QPainter::SmoothPixmapTransform);
    m_swingEffectView->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    m_swingEffectView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_swingEffectView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_id = m_itemEntryInter->id();
    m_active = m_itemEntryInter->isActive();

    m_appNameTips->setObjectName(m_itemEntryInter->name());
    m_appNameTips->setAccessibleName(m_itemEntryInter->name() + "-tips");
    m_appNameTips->setVisible(false);
    m_appNameTips->setStyleSheet("color:white;"
                                 "padding:0px 3px;");

    m_updateIconGeometryTimer->setInterval(500);
    m_updateIconGeometryTimer->setSingleShot(true);

    m_appPreviewTips->setVisible(false);

    connect(m_itemEntryInter, &DockEntryInter::IsActiveChanged, this, &AppItem::activeChanged);
    connect(m_itemEntryInter, &DockEntryInter::IsActiveChanged, this, static_cast<void (AppItem::*)()>(&AppItem::update));
    connect(m_itemEntryInter, &DockEntryInter::WindowInfosChanged, this, &AppItem::updateWindowInfos, Qt::QueuedConnection);
    connect(m_itemEntryInter, &DockEntryInter::IconChanged, this, &AppItem::refershIcon);

    connect(m_updateIconGeometryTimer, &QTimer::timeout, this, &AppItem::updateWindowIconGeometries, Qt::QueuedConnection);

    connect(m_appPreviewTips, &PreviewContainer::requestActivateWindow, this, &AppItem::requestActivateWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestPreviewWindow, this, &AppItem::requestPreviewWindow, Qt::QueuedConnection);
    connect(m_appPreviewTips, &PreviewContainer::requestCancelAndHidePreview, this, &AppItem::cancelAndHidePreview);
    connect(m_appPreviewTips, &PreviewContainer::requestCheckWindows, m_itemEntryInter, &DockEntryInter::Check);

    updateWindowInfos(m_itemEntryInter->windowInfos());
    refershIcon();
}

AppItem::~AppItem()
{
    stopSwingEffect();

    m_appNameTips->deleteLater();
    m_appPreviewTips->deleteLater();
}

const QString AppItem::appId() const
{
    return m_id;
}

// Update _NET_WM_ICON_GEOMETRY property for windows that every item
// that manages, so that WM can do proper animations for specific
// window behaviors like minimization.
void AppItem::updateWindowIconGeometries()
{
    const QRect r(mapToGlobal(QPoint(0, 0)),
                  mapToGlobal(QPoint(width(),height())));
    auto *xcb_misc = XcbMisc::instance();

    for (auto it(m_windowInfos.cbegin()); it != m_windowInfos.cend(); ++it)
        xcb_misc->set_window_icon_geometry(it.key(), r);
}

void AppItem::setIconBaseSize(const int size)
{
    IconBaseSize = size;
}

int AppItem::iconBaseSize()
{
    return IconBaseSize;
}

int AppItem::itemBaseWidth()
{
    if (DockDisplayMode == Dock::Fashion)
        return itemBaseHeight() * 1.1;
    else
        return itemBaseHeight() * 1.4;
}

void AppItem::moveEvent(QMoveEvent *e)
{
    DockItem::moveEvent(e);

    m_updateIconGeometryTimer->start();
}

int AppItem::itemBaseHeight()
{
    if (DockDisplayMode == Efficient)
        return IconBaseSize * 1.2;
    else
        return IconBaseSize * 1.5;
}

void AppItem::paintEvent(QPaintEvent *e)
{
    DockItem::paintEvent(e);

    if (m_dragging || (m_swingEffectView->isVisible() && DockDisplayMode != Fashion))
        return;

    QPainter painter(this);
    if (!painter.isActive())
        return;
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF itemRect = rect();

    // draw background
    QRectF backgroundRect = itemRect;

    if (DockDisplayMode == Efficient)
    {
        backgroundRect = itemRect.marginsRemoved(QMargins(1, 1, 1, 1));

        if (m_active)
        {
            painter.fillRect(backgroundRect, QColor(44, 167, 248, 255 * 0.3));

            const int activeLineWidth = itemRect.height() > 50 ? 4 : 2;
            QRectF activeRect = backgroundRect;
            switch (DockPosition)
            {
            case Top:       activeRect.setBottom(activeRect.top() + activeLineWidth);   break;
            case Bottom:    activeRect.setTop(activeRect.bottom() - activeLineWidth);   break;
            case Left:      activeRect.setRight(activeRect.left() + activeLineWidth);   break;
            case Right:     activeRect.setLeft(activeRect.right() - activeLineWidth);   break;
            }

            painter.fillRect(activeRect, QColor(44, 167, 248, 255));
        }
        else if (!m_windowInfos.isEmpty())
        {
            if (hasAttention())
                painter.fillRect(backgroundRect, QColor(241, 138, 46, 255 * .8));
            else
                painter.fillRect(backgroundRect, QColor(255, 255, 255, 255 * 0.2));
        }
    }
    else
    {
        if (!m_windowInfos.isEmpty())
        {
            QPoint p;
            QPixmap pixmap;
            QPixmap activePixmap;
            switch (DockPosition)
            {
            case Top:
                pixmap = m_horizontalIndicator;
                activePixmap = m_activeHorizontalIndicator;
                p.setX((itemRect.width() - pixmap.width()) / 2);
                p.setY(1);
                break;
            case Bottom:
                pixmap = m_horizontalIndicator;
                activePixmap = m_activeHorizontalIndicator;
                p.setX((itemRect.width() - pixmap.width()) / 2);
                p.setY(itemRect.height() - pixmap.height() - 1);
                break;
            case Left:
                pixmap = m_verticalIndicator;
                activePixmap = m_activeVerticalIndicator;
                p.setX(1);
                p.setY((itemRect.height() - pixmap.height()) / 2);
                break;
            case Right:
                pixmap = m_verticalIndicator;
                activePixmap = m_activeVerticalIndicator;
                p.setX(itemRect.width() - pixmap.width() - 1);
                p.setY((itemRect.height() - pixmap.height()) / 2);
                break;
            }

            if (m_active)
                painter.drawPixmap(p, activePixmap);
            else
                painter.drawPixmap(p, pixmap);
        }
    }

    if (m_swingEffectView->isVisible())
        return;

    // icon
    const QPixmap &pixmap = m_appIcon;
    if (pixmap.isNull())
        return;

    // icon pos
    const auto ratio = qApp->devicePixelRatio();
    const int iconX = itemRect.center().x() - pixmap.rect().center().x() / ratio;
    const int iconY = itemRect.center().y() - pixmap.rect().center().y() / ratio;

    painter.drawPixmap(iconX, iconY, pixmap);
}

void AppItem::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton) {
        m_itemEntryInter->NewInstance(QX11Info::getTimestamp());
    } else if (e->button() == Qt::LeftButton) {

        m_itemEntryInter->Activate(QX11Info::getTimestamp());

        // play launch effect
        if (m_windowInfos.isEmpty())
            playSwingEffect();
    }
}

void AppItem::mousePressEvent(QMouseEvent *e)
{
    m_updateIconGeometryTimer->stop();
    hidePopup();

    if (e->button() == Qt::RightButton)
    {
        if (perfectIconRect().contains(e->pos()))
        {
            QMetaObject::invokeMethod(this, "showContextMenu", Qt::QueuedConnection);
            return;
        } else {
            return QWidget::mousePressEvent(e);
        }
    }

    if (e->button() == Qt::LeftButton)
        MousePressPos = e->pos();
}

void AppItem::mouseMoveEvent(QMouseEvent *e)
{
    e->accept();

    // handle preview
//    if (e->buttons() == Qt::NoButton)
//        return showPreview();

    // handle drag
    if (e->buttons() != Qt::LeftButton)
        return;

    const QPoint pos = e->pos();
    if (!rect().contains(pos))
        return;

    const QPoint distance = pos - MousePressPos;
    if (distance.manhattanLength() > APP_DRAG_THRESHOLD)
        return startDrag();
}

void AppItem::wheelEvent(QWheelEvent *e)
{
    QWidget::wheelEvent(e);

    m_itemEntryInter->PresentWindows();
}

void AppItem::resizeEvent(QResizeEvent *e)
{
    DockItem::resizeEvent(e);

    refershIcon();
}

void AppItem::dragEnterEvent(QDragEnterEvent *e)
{
    // ignore drag from panel
    if (e->source())
        return;

    // ignore request dock event
    if (e->mimeData()->formats().contains("RequestDock"))
        return e->ignore();

    e->accept();
}

void AppItem::dragMoveEvent(QDragMoveEvent *e)
{
    DockItem::dragMoveEvent(e);

    if (m_windowInfos.isEmpty())
        return;

    if (!PopupWindow->isVisible() || PopupWindow->getContent() != m_appPreviewTips)
        showPreview();
}

void AppItem::dropEvent(QDropEvent *e)
{
    QStringList uriList;
    for (auto uri : e->mimeData()->urls()) {
        uriList << uri.toEncoded();
    }

    qDebug() << "accept drop event with URIs: " << uriList;
    m_itemEntryInter->HandleDragDrop(QX11Info::getTimestamp(), uriList);
}

void AppItem::leaveEvent(QEvent *e)
{
    DockItem::leaveEvent(e);

    if (m_appPreviewTips->isVisible())
        m_appPreviewTips->prepareHide();
}

void AppItem::showHoverTips()
{
    if (!m_windowInfos.isEmpty())
        return showPreview();

    DockItem::showHoverTips();
}

void AppItem::invokedMenuItem(const QString &itemId, const bool checked)
{
    Q_UNUSED(checked);

    m_itemEntryInter->HandleMenuItem(QX11Info::getTimestamp(), itemId);
}

const QString AppItem::contextMenu() const
{
    return m_itemEntryInter->menu();
}

QWidget *AppItem::popupTips()
{
    if (m_dragging)
        return nullptr;

    if (!m_windowInfos.isEmpty())
    {
        const quint32 currentWindow = m_itemEntryInter->currentWindow();
        Q_ASSERT(m_windowInfos.contains(currentWindow));
        m_appNameTips->setText(m_windowInfos[currentWindow].title);
    } else {
        m_appNameTips->setText(m_itemEntryInter->name());
    }

    return m_appNameTips;
}

void AppItem::startDrag()
{
    m_dragging = true;
    update();

    const QPixmap &dragPix = m_appIcon;

    QDrag *drag = new QDrag(this);
    drag->setPixmap(dragPix);
    drag->setHotSpot(dragPix.rect().center() / dragPix.devicePixelRatioF());
    drag->setMimeData(new QMimeData);

    emit dragStarted();
    const Qt::DropAction result = drag->exec(Qt::MoveAction);
    Q_UNUSED(result);

    // drag out of dock panel
    if (!drag->target())
        m_itemEntryInter->RequestUndock();

    m_dragging = false;
    setVisible(true);
    update();
}

bool AppItem::hasAttention() const
{
    for (const auto &info : m_windowInfos)
        if (info.attention)
            return true;
    return false;
}

void AppItem::updateWindowInfos(const WindowInfoMap &info)
{
    m_windowInfos = info;
    m_appPreviewTips->setWindowInfos(m_windowInfos);
    m_updateIconGeometryTimer->start();

    // process attention effect
    if (hasAttention())
    {
        if (DockDisplayMode == DisplayMode::Fashion)
            playSwingEffect();
    } else {
        stopSwingEffect();
    }

    update();
}

void AppItem::refershIcon()
{
    const QString icon = m_itemEntryInter->icon();
    const int iconSize = qMin(width(), height());

    if (DockDisplayMode == Efficient)
        m_appIcon = ThemeAppIcon::getIcon(icon, iconSize * 0.7);
    else
        m_appIcon = ThemeAppIcon::getIcon(icon, iconSize * 0.8);

    update();

    m_updateIconGeometryTimer->start();
}

void AppItem::activeChanged()
{
    m_active = !m_active;
}

void AppItem::showPreview()
{
    if (m_windowInfos.isEmpty())
        return;

    // test cursor position
//    const QRect r = rect();
//    const QPoint p = mapFromGlobal(QCursor::pos());

//    switch (DockPosition)
//    {
//    case Top:       if (p.y() != r.top())       return;     break;
//    case Left:      if (p.x() != r.left())      return;     break;
//    case Right:     if (p.x() != r.right())     return;     break;
//    case Bottom:    if (p.y() != r.bottom())    return;     break;
//    default:        return;
//    }

    showPopupWindow(m_appPreviewTips, true);

    m_appPreviewTips->setWindowInfos(m_windowInfos);
    m_appPreviewTips->updateSnapshots();
    m_appPreviewTips->updateLayoutDirection(DockPosition);
}

void AppItem::cancelAndHidePreview()
{
    hidePopup();
    emit requestCancelPreview();
}

void AppItem::playSwingEffect()
{
    // NOTE(sbw): return if animation view already playing
    if (m_swingEffectView->isVisible())
        return;

    stopSwingEffect();
    if (!m_itemAnimation.timeLine())
    {
        QTimeLine *tl = new QTimeLine(1200, this);
        tl->setFrameRange(0, 60);
        tl->setLoopCount(1);
        tl->setEasingCurve(QEasingCurve::Linear);
        tl->setStartFrame(0);

        m_itemAnimation.setTimeLine(tl);
    }

    const auto ratio = qApp->devicePixelRatio();
    const QRect r = rect();
    const QPixmap &icon = m_appIcon;

    QGraphicsPixmapItem *item = m_itemScene->addPixmap(icon);
    item->setTransformationMode(Qt::SmoothTransformation);
    item->setPos(QPointF(r.center()) - QPointF(icon.rect().center()) / ratio);

    m_itemAnimation.setItem(item);
    m_itemScene->setSceneRect(r);
    m_swingEffectView->setSceneRect(r);
    m_swingEffectView->setFixedSize(r.size());

    const int px = qreal(-icon.rect().center().x()) / ratio;
    const int py = qreal(-icon.rect().center().y()) / ratio - 18.;
    const QPoint pos = r.center() + QPoint(0, 18);
    for (int i(0); i != 60; ++i)
    {
        m_itemAnimation.setPosAt(i / 60.0, pos);
        m_itemAnimation.setTranslationAt(i / 60.0, px, py);
        m_itemAnimation.setRotationAt(i / 60.0, Frames[i]);
    }

    QTimeLine *tl = m_itemAnimation.timeLine();
    connect(tl, &QTimeLine::finished, m_swingEffectView, &QGraphicsView::hide);
    connect(tl, &QTimeLine::finished, this, &AppItem::checkAttentionEffect);

    tl->start();
    m_swingEffectView->setVisible(true);
}

void AppItem::stopSwingEffect()
{
    // stop swing effect
    m_swingEffectView->setVisible(false);

    if (m_itemAnimation.timeLine() && m_itemAnimation.timeLine()->state() != QTimeLine::NotRunning)
        m_itemAnimation.timeLine()->stop();
    m_itemAnimation.clear();
    if (!m_itemScene->items().isEmpty())
        m_itemScene->clear();
}

void AppItem::checkAttentionEffect()
{
    QTimer::singleShot(1000, this, [=] {
        if (DockDisplayMode == DisplayMode::Fashion && hasAttention())
            playSwingEffect();
    });
}

