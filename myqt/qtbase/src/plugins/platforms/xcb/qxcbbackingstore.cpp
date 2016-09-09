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

#include "qxcbbackingstore.h"

#include "qxcbconnection.h"
#include "qxcbscreen.h"
#include "qxcbwindow.h"

#include <stdio.h>
#include <errno.h>

#include <qdebug.h>
#include <qpainter.h>
#include <qscreen.h>

#include <algorithm>

#ifdef QT_MROFS
#include "mrofs/qmrofscompositor.h"
#include "mrofs/qmrofscursor_p.h"
#endif

QT_BEGIN_NAMESPACE

QXcbBackingStore::QXcbBackingStore(QWindow *window)
    : QPlatformBackingStore(window)                                                  //DHT  window.handle 一定存在了吗??
    , m_image(0)
    , m_syncingResize(false)
#ifdef QT_MROFS
    , m_compositor(NULL)
#endif
{
    QXcbScreen *screen = static_cast<QXcbScreen *>(window->screen()->handle());
    setConnection(screen->connection());

#ifdef Q_XCB_DEBUG
        qDebug("QXcbBackingStore::ctor, m_image(%p)", m_image);
#endif      
#ifdef QT_MROFS
    //无论是否为foreign window，都需要一个compositor， 在backingstore 创建时创建
    QXcbWindow *xcb_win = static_cast<QXcbWindow*>(window->handle());
    m_compositor = new QMrOfsCompositor(xcb_win, this);        

    QMrOfsCursor *cursor = reinterpret_cast<QMrOfsCursor*>(screen->cursor());
    cursor->setCompositor(m_compositor);            //now cursor has compositor
#endif
}

QXcbBackingStore::~QXcbBackingStore()
{
    delete m_image;
    m_image = NULL;

#ifdef Q_XCB_DEBUG
    qDebug("QXcbBackingStore::dtor, m_image(%p)", m_image);
#endif 

#ifdef QT_MROFS
    delete m_compositor;
    m_compositor = NULL;
#endif
}

QPaintDevice *QXcbBackingStore::paintDevice()
{
    return m_image ? m_image->image() : 0;
}

/*!
    \note 
    region is in TLW coordinate
    as for compositor thread, beginPaint + flush is an ATOM operation
    \ref QWidgetBackingStore.sync(toClean), QWidgetBacking.beginPaint
*/
void QXcbBackingStore::beginPaint(const QRegion &region)
{
    if (!m_image)
        return;
    
    m_image->preparePaint(region);

    if (m_image->image()->hasAlphaChannel()) {
        QPainter p(m_image->image());
        p.setCompositionMode(QPainter::CompositionMode_Source);  // 准备一个(0,0,0,0,)的背景,qt 在这样一个带alpha的背景上画自己的内容(Composition_Mode)
        const QVector<QRect> rects = region.rects();
        const QColor blank = Qt::transparent;                                 //rgba(0,0,0,0) transparent black
        for (QVector<QRect>::const_iterator it = rects.begin(); it != rects.end(); ++it) {
            p.fillRect(*it, blank);
        }
    }
}

/*!
    \note
    在这里清dirty region 是错误的，原因在于QXcbBackingStore.flush 在QXcbBackingStore.endPaint 之后执行，而flush 要依赖bs 的drity region 信息；
    在这里unlockBackingStoreResource 也是错误的，原因在于flush 中对res 有更新(更新过程应lock)。
    因此这里最好什么也不做。
    \ref QWidgetBackingStore.endPaint
*/
void QXcbBackingStore::endPaint()
{
    //nothing could be done
}

/*!
    \note 仅当 flush native widget  (dirtyOnScreenWidgets) 时 offset 才可能不为0；
    如果除tlw外全部为alien widget，则offset 永远为0。
    dirty 信息永远基于TLW coordinate, 用offset 区别上述2种情况。
    region is in TLW coordinate(alien widget) 或non-TLW native coordinate(native widget)
*/    
void QXcbBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    if (!m_image || m_image->size().isEmpty())
        return;

    QSize imageSize = m_image->size();

    QRegion clipped = region;
    // 不超过 window 范围
    clipped &= QRect(0, 0, window->width(), window->height());
    
    //region 在window坐标系；m_image 在bs.tlw坐标系
    //offset 的本意是window 可能不是bs.tlw，因此需要转换region的坐标系再和m_image 裁剪
    //但实际上没有发现window 不是bs.tlw的情况(ref QWidgetBackingStore::markDirtyOnScreen)
    // 不超过 imageSize 范围, 注意是在 clipped 坐标系中计算
    clipped &= QRect(0, 0, imageSize.width(), imageSize.height()).translated(-offset);

    QRect bounds = clipped.boundingRect();

    if (bounds.isNull())
        return;

    Q_XCB_NOOP(connection());

    QXcbWindow *platformWindow = static_cast<QXcbWindow *>(window->handle());
    if (!platformWindow) {
        qWarning("QXcbBackingStore::flush: QWindow has no platform window (QTBUG-32681)");
        return;
    }

#ifdef QT_MROFS
    m_compositor->lockBackingStoreResources();       //将lock 从beginPaint 转移到这里，尽量缩短ui 线程的锁定区间

#if 1       // clip rects
    m_image->setBSEffectiveClip(clipped);      //update dirty region in shm image (by clipped)
    QVector<QRect> rects = clipped.rects();              //update pixmap in shm image (by xcb_image_t/QImage data)
    for (int i = 0; i < rects.size(); ++i) {
        // src: TLW 坐标系, dst: window 坐标系
        m_image->put(m_image->pixmap(), rects.at(i).topLeft(), rects.at(i).translated(offset));    //use default buf index(0)
    }    
#else    // bounding rect
    m_image->setBSEffectiveClip(bounds);      //update dirty region in shm image (by clipped)
    m_image->put(m_image->pixmap(), bounds.topLeft(), bounds.translated(offset));
#endif
    m_compositor->unlockBackingStoreResources();    //flush bs 之前必须unlock否则死锁，ui thread 所有对dirty region 和pixmap 的更新已经完成
    m_compositor->flushBackingStore(clipped);           //触发合成，必须在finishPaint 之前调用，且为同步调用
                                                                               //cleanup dirty regions  必须在合成器线程且flush 返回前执行, 否则下一次wave触发的合成将和它竞争
#else
    QVector<QRect> rects = clipped.rects();
    for (int i = 0; i < rects.size(); ++i) {
        //non-compositing: update window drawable
        m_image->put(platformWindow->xcb_window(), rects.at(i).topLeft(), rects.at(i).translated(offset));  //use default buf index(0)
    }
#endif

    Q_XCB_NOOP(connection());

    if (m_syncingResize) {
        xcb_flush(xcb_connection());
        connection()->sync();
        m_syncingResize = false;
        platformWindow->updateSyncRequestCounter();
    }
}

/*!
    \note
    resize 之前，APP应保证
    1, compositor 之前的工作已做完，且没有新的compositing 请求
    2, ui 中之前使用的painter 都已释放(painter 会缓存old backingstore image的指针)
    以避免dangling pointer to bs QImage
    \ref QMrOfs::setWaveformSize, QMrOfs::enableComposition
    \ref  QWidget.resize, QWidgetPrivate.setGeometry_sys
*/
void QXcbBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    if (m_image && size == m_image->size())
        return;

    Q_XCB_NOOP(connection());
    
    //create xcb window if NOT
    QPlatformWindow *pw = window()->handle();
    if (!pw) {
        window()->create();
        pw = window()->handle();
    }

#ifdef Q_XCB_DEBUG
    qDebug("QXcbBackingStore::resize, old m_image(%p), size(%d, %d)", m_image,
        m_image ? m_image->size().width() : -1 , m_image ? m_image->size().height() : -1);
#endif      
    
#ifdef QT_MROFS    
    m_compositor->resizeBackingStore(this, size, staticContents);    //callback into doResize in compositor thread, wait here...
#else
    QXcbScreen *screen = static_cast<QXcbScreen *>(window()->screen()->handle());
    QXcbWindow* win = static_cast<QXcbWindow *>(pw);
    delete m_image;    
    m_image = new QXcbShmImage(screen, size, win->depth(), win->imageFormat());        //depth 24, buf_count == 1
#endif

#ifdef Q_XCB_DEBUG
    qDebug("QXcbBackingStore::resize, new m_image(%p), size(%d, %d)", m_image, 
        m_image ? m_image->size().width() : -1 , m_image ? m_image->size().height() : -1);
#endif      

    Q_XCB_NOOP(connection());

    m_syncingResize = true;
}

QSize QXcbBackingStore::size()
{
    return m_image->size();
}

#ifdef QT_MROFS
QPlatformImage *QXcbBackingStore::platformImage() 
{
    return m_image;
}

/*!
    create xcb_render_picture and xcb_pixmap(for pict bits storage); 
    create QImage and xcb_image(for QImage bits storage):
    in compositor thread
*/
void QXcbBackingStore::doResize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(staticContents);
    delete m_image;    
    QXcbScreen *screen = static_cast<QXcbScreen *>(window()->screen()->handle());
    m_image = new QXcbShmImage(screen, size, 32, QMrOfs::Format_ARGB32_Premultiplied);
}

/*!
    获取compositor, called by qt/gui/QMrOfs module
*/
QPlatformCompositor* QXcbBackingStore::compositor()
{
    return this->m_compositor;
}

static const QRegion s_xcb_dummy_rgn;
const QRegion& QXcbBackingStore::effectiveClip()
{
    if(!m_image) {
        return s_xcb_dummy_rgn;
    }
    
    return this->m_image->bsEffectiveClip();
}

void QXcbBackingStore::cleanEffectiveClip()
{
    if(!m_image) {
        return;
    }
    
    this->m_image->cleanBSEffectiveClip();
}

#endif //QT_MROFS

extern void qt_scrollRectInImage(QImage &img, const QRect &rect, const QPoint &offset);

/*!
    area 为滚动前的滚动区域，dx/dy 为滚动距离(下正，右正)
    1， 对于opaque widgets, 新出现的范围重绘，其余部分可以拷贝老的；
    2，对于non-opaque widgets, 全部重绘。
    这里只对1有效。
    \ref QWidget.scroll, QWidgetPrivate::scrollRect
*/
bool QXcbBackingStore::scroll(const QRegion &area, int dx, int dy)
{
    if (!m_image || m_image->image()->isNull())
        return false;

    m_image->preparePaint(area);

    const QVector<QRect> rects = area.rects();
    for (int i = 0; i < rects.size(); ++i)
        qt_scrollRectInImage(*m_image->image(), rects.at(i), QPoint(dx, dy));

    return true;
}

QT_END_NAMESPACE
