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
    : QPlatformBackingStore(window)                                                  //DHT  window.handle һ����������??
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
    //�����Ƿ�Ϊforeign window������Ҫһ��compositor�� ��backingstore ����ʱ����
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
        p.setCompositionMode(QPainter::CompositionMode_Source);  // ׼��һ��(0,0,0,0,)�ı���,qt ������һ����alpha�ı����ϻ��Լ�������(Composition_Mode)
        const QVector<QRect> rects = region.rects();
        const QColor blank = Qt::transparent;                                 //rgba(0,0,0,0) transparent black
        for (QVector<QRect>::const_iterator it = rects.begin(); it != rects.end(); ++it) {
            p.fillRect(*it, blank);
        }
    }
}

/*!
    \note
    ��������dirty region �Ǵ���ģ�ԭ������QXcbBackingStore.flush ��QXcbBackingStore.endPaint ֮��ִ�У���flush Ҫ����bs ��drity region ��Ϣ��
    ������unlockBackingStoreResource Ҳ�Ǵ���ģ�ԭ������flush �ж�res �и���(���¹���Ӧlock)��
    ����������ʲôҲ������
    \ref QWidgetBackingStore.endPaint
*/
void QXcbBackingStore::endPaint()
{
    //nothing could be done
}

/*!
    \note ���� flush native widget  (dirtyOnScreenWidgets) ʱ offset �ſ��ܲ�Ϊ0��
    �����tlw��ȫ��Ϊalien widget����offset ��ԶΪ0��
    dirty ��Ϣ��Զ����TLW coordinate, ��offset ��������2�������
    region is in TLW coordinate(alien widget) ��non-TLW native coordinate(native widget)
*/    
void QXcbBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    if (!m_image || m_image->size().isEmpty())
        return;

    QSize imageSize = m_image->size();

    QRegion clipped = region;
    // ������ window ��Χ
    clipped &= QRect(0, 0, window->width(), window->height());
    
    //region ��window����ϵ��m_image ��bs.tlw����ϵ
    //offset �ı�����window ���ܲ���bs.tlw�������Ҫת��region������ϵ�ٺ�m_image �ü�
    //��ʵ����û�з���window ����bs.tlw�����(ref QWidgetBackingStore::markDirtyOnScreen)
    // ������ imageSize ��Χ, ע������ clipped ����ϵ�м���
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
    m_compositor->lockBackingStoreResources();       //��lock ��beginPaint ת�Ƶ������������ui �̵߳���������

#if 1       // clip rects
    m_image->setBSEffectiveClip(clipped);      //update dirty region in shm image (by clipped)
    QVector<QRect> rects = clipped.rects();              //update pixmap in shm image (by xcb_image_t/QImage data)
    for (int i = 0; i < rects.size(); ++i) {
        // src: TLW ����ϵ, dst: window ����ϵ
        m_image->put(m_image->pixmap(), rects.at(i).topLeft(), rects.at(i).translated(offset));    //use default buf index(0)
    }    
#else    // bounding rect
    m_image->setBSEffectiveClip(bounds);      //update dirty region in shm image (by clipped)
    m_image->put(m_image->pixmap(), bounds.topLeft(), bounds.translated(offset));
#endif
    m_compositor->unlockBackingStoreResources();    //flush bs ֮ǰ����unlock����������ui thread ���ж�dirty region ��pixmap �ĸ����Ѿ����
    m_compositor->flushBackingStore(clipped);           //�����ϳɣ�������finishPaint ֮ǰ���ã���Ϊͬ������
                                                                               //cleanup dirty regions  �����ںϳ����߳���flush ����ǰִ��, ������һ��wave�����ĺϳɽ���������
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
    resize ֮ǰ��APPӦ��֤
    1, compositor ֮ǰ�Ĺ��������꣬��û���µ�compositing ����
    2, ui ��֮ǰʹ�õ�painter �����ͷ�(painter �Ỻ��old backingstore image��ָ��)
    �Ա���dangling pointer to bs QImage
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
    ��ȡcompositor, called by qt/gui/QMrOfs module
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
    area Ϊ����ǰ�Ĺ�������dx/dy Ϊ��������(����������)
    1�� ����opaque widgets, �³��ֵķ�Χ�ػ棬���ಿ�ֿ��Կ����ϵģ�
    2������non-opaque widgets, ȫ���ػ档
    ����ֻ��1��Ч��
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
