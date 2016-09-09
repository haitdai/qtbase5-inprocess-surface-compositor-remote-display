/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwindowsbackingstore.h"
#include "qwindowswindow.h"
#include "qwindowsnativeimage.h"
#include "qwindowscontext.h"

#include <QtGui/QWindow>
#include <QtGui/QPainter>

#include <QtCore/QDebug>

#ifdef QT_MROFS
#include "mrofs/qmrofscompositor.h"
#endif

QT_BEGIN_NAMESPACE

/*!
    \class QWindowsBackingStore
    \brief Backing store for windows.
    \internal
    \ingroup qt-lighthouse-win
*/

QWindowsBackingStore::QWindowsBackingStore(QWindow *window) :
    QPlatformBackingStore(window)
#ifdef QT_MROFS    
    , m_compositor(NULL), m_dirty(QRegion())
#endif    
{
     if (QWindowsContext::verboseBackingStore)
         qDebug() << __FUNCTION__ << this << window;

#ifdef QT_MROFS    
    //无论是否为foreign window，都需要一个compositor， 在backingstore 创建时创建    
    QPlatformWindow *plat_win = static_cast<QWindowsWindow*>(window->handle());    
    m_compositor = new QMrOfsCompositor(plat_win, this);            
#endif         
}

QWindowsBackingStore::~QWindowsBackingStore()
{
    if (QWindowsContext::verboseBackingStore)
        qDebug() << __FUNCTION__ << this;

#ifdef QT_MROFS    
    delete m_compositor;    
    m_compositor = NULL;
#endif        
}

QPaintDevice *QWindowsBackingStore::paintDevice()
{
    Q_ASSERT(!m_image.isNull());
    return m_image->image();
}

/*!
    \param
        region: window 坐标系(分window == TLW 和window != TLW 2 种情况)
        offset: window 相对TLW 的偏移(分window == TLW 和window != TLW 2 种情况)
    \note: 不同native widget 和 backingstore 的关系:
    tlw - 有backingstore
    native child - 无backingstore, 合并到tlw 的backingstore 显示
    注意
    native child 可以有native child 或alien child.
    offset 仅当native child 时非 0(window != TLW, 此时region 为native child coordinate), ref ==> qt_flush <==    
    \note: windows use bounding rect but others use rects list(xcb, mrofs, ...) of backingstore dirty region
    区别
    qt 自带的实现是提交bounding rect ;
    mrofs 的实现是提交真正脏的部分
*/
void QWindowsBackingStore::flush(QWindow *window, const QRegion &region,
                                        const QPoint &offset)
{
    Q_ASSERT(window);

    // 注意: 和 mrofs_pvr2d, mrofs_xcb  不同, mrofs_windows 直接使用 bounding rect 作为 dirty region
    const QRect br = region.boundingRect();
    if (QWindowsContext::verboseBackingStore > 1)
        qDebug() << __FUNCTION__ << window << offset << br;
    QWindowsWindow *rw = QWindowsWindow::baseWindowOf(window);
    
#ifdef QT_MROFS

    m_compositor->lockBackingStoreResources();    
#ifndef Q_OS_WINCE
    const bool hasAlpha = rw->format().hasAlpha();
    const Qt::WindowFlags flags = window->flags();
    if ((flags & Qt::FramelessWindowHint) && QWindowsWindow::setWindowLayered(rw->handle(), flags, hasAlpha, rw->opacity()) && hasAlpha) {
        QPoint frameOffset(window->frameMargins().left(), window->frameMargins().top());
        QVector<QRect> rects = region.rects();
        for(int i = 0; i < rects.size(); i ++) {
            const QRect &rc = rects.at(i);
            m_dirty += rc.translated(offset + frameOffset);
        }        
    } else {
#endif
        QVector<QRect> rects = region.rects();
        for(int i = 0; i < rects.size(); i ++) {
            const QRect &rc = rects.at(i);
            m_dirty += rc.translated(offset);
        }
#ifndef Q_OS_WINCE
    }
#endif

        if (QWindowsContext::verboseBackingStore > 0) {
            QVector<QRect> rects = m_dirty.rects();
//            qDebug() << "dirty backingstore rects begin, num: =============" << rects.size();
            for(int i = 0; i < rects.size(); i ++) {
                const QRect &rc = rects.at(i);
//                qDebug() << rc.x() << rc.y() << rc.width() << rc.height();
            }
//            qDebug() << "dirty backingstore rects end =============";
        }

        m_compositor->unlockBackingStoreResources();
        m_compositor->flushBackingStore(m_dirty);    
#else   // bs image(windows native image) -> hwnd

#ifndef Q_OS_WINCE
    const bool hasAlpha = rw->format().hasAlpha();      // if and ONLY if Qt::WA_TranslucentBackground set on this TLW, per-pixel alpha is impossible(because WA_TranslucentBackground means alpha == 0)
    const Qt::WindowFlags flags = window->flags();
    // 2 种情况WA_TranslucentBackground +  FramelessWindowHint (全透明用UpdateLayeredWindow), 否则用 BitBlt
    if ((flags & Qt::FramelessWindowHint) && QWindowsWindow::setWindowLayered(rw->handle(), flags, hasAlpha, rw->opacity()) && hasAlpha) {
        // Windows with alpha: Use blend function to update.
        QRect r = window->frameGeometry();
        QPoint frameOffset(window->frameMargins().left(), window->frameMargins().top());
        QRect dirtyRect = br.translated(offset + frameOffset);

        SIZE size = {r.width(), r.height()};
        POINT ptDst = {r.x(), r.y()};
        POINT ptSrc = {0, 0};
        BLENDFUNCTION blend = {AC_SRC_OVER, 0, (BYTE)(255.0 * rw->opacity()), AC_SRC_ALPHA};
        if (QWindowsContext::user32dll.updateLayeredWindowIndirect) {
            RECT dirty = {dirtyRect.x(), dirtyRect.y(),
                dirtyRect.x() + dirtyRect.width(), dirtyRect.y() + dirtyRect.height()};
            UPDATELAYEREDWINDOWINFO info = {
                sizeof(info),           // size of this structure
                NULL,                   // dc of screen, could be NULL
                &ptDst,                 // new layered wnd position in screen (dst)
                &size,                  // new layered wnd size
                m_image->hdc(),         // src dc
                &ptSrc,                 // src layer position in src dc
                0,                      // color key, not used
                &blend,                 // blend function
                ULW_ALPHA,              // use blend function flag
                &dirty                  // dirty rect in src dc
            };
            QWindowsContext::user32dll.updateLayeredWindowIndirect(rw->handle(), &info);
        } else {
            QWindowsContext::user32dll.updateLayeredWindow(rw->handle(), NULL, &ptDst, &size, m_image->hdc(), &ptSrc, 0, &blend, ULW_ALPHA);
        }
    } else {
#endif
        const HDC dc = rw->getDC();        // window 的DC, window 不一定是 TLW
        if (!dc) {
            qErrnoWarning("%s: GetDC failed", __FUNCTION__);
            return;
        }

        if (!BitBlt(
                dc, 
                br.x(),                     // dst x - in client coord
                br.y(),                     // dst y - in client coord
                br.width(),                 // dst w
                br.height(),                // dst h
                m_image->hdc(), 
                br.x() + offset.x(),        // src x - in TLW coord (如果 native 位于TLW 之内, 则把画在tlw 上的内容拷贝到native 窗口上)
                br.y() + offset.y(),        // src y - in TLW coord
                SRCCOPY
        ))
            qErrnoWarning("%s: BitBlt failed", __FUNCTION__);
        rw->releaseDC();
#ifndef Q_OS_WINCE
    }
#endif
#endif  // QT_MROFS
    // Write image for debug purposes.
    if (QWindowsContext::verboseBackingStore > 2) {
        static int n = 0;
        const QString fileName = QString::fromLatin1("win%1_%2.png").
                arg(rw->winId()).arg(n++);
        m_image->image()->save(fileName);
        qDebug() << "Wrote " << m_image->image()->size() << fileName;
    }
}

/*!
    note for region
    staticContents: 当bs 大小变化时, 已有的内容不重绘(因此需要拷贝到新的 image )
*/
void QWindowsBackingStore::resize(const QSize &size, const QRegion &region)
{
    if (m_image.isNull() || m_image->image()->size() != size) {
#ifndef QT_NO_DEBUG_OUTPUT
        if (QWindowsContext::verboseBackingStore) {
            QDebug nsp = qDebug().nospace();
            nsp << __FUNCTION__ << ' ' << rasterWindow()->window()
                 << ' ' << size << ' ' << region;
            if (!m_image.isNull())
                nsp << " from: " << m_image->image()->size();
        }
#endif


#ifdef QT_MROFS
        m_compositor->resizeBackingStore(this, size, region);
#else
        QImage::Format format = QWindowsNativeImage::systemFormat();
        if (format == QImage::Format_RGB32 && rasterWindow()->window()->format().hasAlpha())
            format = QImage::Format_ARGB32_Premultiplied;

        QWindowsNativeImage *oldwni = m_image.data();
        QWindowsNativeImage *newwni = new QWindowsNativeImage(size.width(), size.height(), format);

        if (oldwni && !region.isEmpty()) {
            const QImage *oldimg(oldwni->image());
            QImage *newimg(newwni->image());
            QRegion staticRegion(region);
            staticRegion &= QRect(0, 0, oldimg->width(), oldimg->height());
            staticRegion &= QRect(0, 0, newimg->width(), newimg->height());
            QPainter painter(newimg);
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            foreach (const QRect &rect, staticRegion.rects())
                painter.drawImage(rect, *oldimg, rect);
        }

        m_image.reset(newwni);
#endif
    }
}

Q_GUI_EXPORT void qt_scrollRectInImage(QImage &img, const QRect &rect, const QPoint &offset);

bool QWindowsBackingStore::scroll(const QRegion &area, int dx, int dy)
{
    if (m_image.isNull() || m_image->image()->isNull())
        return false;

    const QVector<QRect> rects = area.rects();
    for (int i = 0; i < rects.size(); ++i)
        qt_scrollRectInImage(*m_image->image(), rects.at(i), QPoint(dx, dy));

    return true;
}

void QWindowsBackingStore::beginPaint(const QRegion &region)
{
    if (QWindowsContext::verboseBackingStore > 1)
        qDebug() << __FUNCTION__;

    if (m_image->image()->hasAlphaChannel()) {
        QPainter p(m_image->image());
        p.setCompositionMode(QPainter::CompositionMode_Source);
        const QColor blank = Qt::transparent;
        foreach (const QRect &r, region.rects())
            p.fillRect(r, blank);
    }
}

QWindowsWindow *QWindowsBackingStore::rasterWindow() const
{
    if (const QWindow *w = window())
        if (QPlatformWindow *pw = w->handle())
            return static_cast<QWindowsWindow *>(pw);
    return 0;
}

/*!
    the same as get picture
*/
HDC QWindowsBackingStore::getDC() const
{
    if (!m_image.isNull())
        return m_image->hdc();
    return 0;
}

#ifdef QT_MROFS
/*!
    对于offscreen 的情况, 总是使用 argb_p
*/
void QWindowsBackingStore::doResize(const QSize &size, const QRegion &staticContents)
{
    QWindowsNativeImage *oldwni = m_image.data();
    QWindowsNativeImage *newwni = new QWindowsNativeImage(size.width(), size.height(), QImage::Format_ARGB32_Premultiplied);

    if (oldwni && !staticContents.isEmpty()) {
        const QImage *oldimg(oldwni->image());
        QImage *newimg(newwni->image());
        QRegion staticRegion(staticContents);
        staticRegion &= QRect(0, 0, oldimg->width(), oldimg->height());
        staticRegion &= QRect(0, 0, newimg->width(), newimg->height());
        QPainter painter(newimg);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        foreach (const QRect &rect, staticRegion.rects())
            painter.drawImage(rect, *oldimg, rect);
    }

    m_image.reset(newwni);    
}

QPlatformCompositor* QWindowsBackingStore::compositor()
{
    return m_compositor;
}

QPlatformImage *QWindowsBackingStore::platformImage()
{
    return m_image.data();
}

/*!
    [compositor thread]
*/
const QRegion& QWindowsBackingStore::effectiveClip()
{
    return m_dirty;
}

/*!
    [compositor thread]
*/
void QWindowsBackingStore::cleanEffectiveClip()
{
    m_dirty = QRegion();
}
#endif


QT_END_NAMESPACE
