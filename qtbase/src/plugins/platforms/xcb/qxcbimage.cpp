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

#include "qxcbimage.h"
#include <QtGui/QColor>
#include <QtGui/private/qimage_p.h>
#include <QtGui/private/qdrawhelper_p.h>
#ifdef XCB_USE_RENDER
#include <xcb/render.h>
// 'template' is used as a function argument name in xcb_renderutil.h
#define template template_param
// extern "C" is missing too
extern "C" {
#include <xcb/xcb_renderutil.h>
}
#undef template
#endif

#include <stdio.h>
#include <errno.h>
#include <qdebug.h>


QT_BEGIN_NAMESPACE

QImage::Format qt_xcb_imageFormatForVisual(QXcbConnection *connection, uint8_t depth,
                                           const xcb_visualtype_t *visual)
{
    const xcb_format_t *format = connection->formatForDepth(depth);

    if (!visual || !format)
        return QImage::Format_Invalid;

    if (depth == 32 && format->bits_per_pixel == 32 && visual->red_mask == 0xff0000
        && visual->green_mask == 0xff00 && visual->blue_mask == 0xff)
        return QImage::Format_ARGB32_Premultiplied;

    if (depth == 24 && format->bits_per_pixel == 32 && visual->red_mask == 0xff0000
        && visual->green_mask == 0xff00 && visual->blue_mask == 0xff)
        return QImage::Format_RGB32;

    if (depth == 16 && format->bits_per_pixel == 16 && visual->red_mask == 0xf800
        && visual->green_mask == 0x7e0 && visual->blue_mask == 0x1f)
        return QImage::Format_RGB16;

    return QImage::Format_Invalid;
}

/*!
    xcb pixmap -> xcb image -> QPixmap
*/
QPixmap qt_xcb_pixmapFromXPixmap(QXcbConnection *connection, xcb_pixmap_t pixmap,
                                 int width, int height, int depth,
                                 const xcb_visualtype_t *visual)
{
    xcb_connection_t *conn = connection->xcb_connection();

    xcb_get_image_cookie_t get_image_cookie =
        xcb_get_image_unchecked(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, pixmap,
                      0, 0, width, height, 0xffffffff);

    xcb_get_image_reply_t *image_reply =
        xcb_get_image_reply(conn, get_image_cookie, NULL);

    if (!image_reply) {
        return QPixmap();
    }

    uint8_t *data = xcb_get_image_data(image_reply);
    uint32_t length = xcb_get_image_data_length(image_reply);

    QPixmap result;

    QImage::Format format = qt_xcb_imageFormatForVisual(connection, depth, visual);
    if (format != QImage::Format_Invalid) {
        uint32_t bytes_per_line = length / height;
        QImage image(const_cast<uint8_t *>(data), width, height, bytes_per_line, format);
        uint8_t image_byte_order = connection->setup()->image_byte_order;

        // we may have to swap the byte order
        if ((QSysInfo::ByteOrder == QSysInfo::LittleEndian && image_byte_order == XCB_IMAGE_ORDER_MSB_FIRST)
            || (QSysInfo::ByteOrder == QSysInfo::BigEndian && image_byte_order == XCB_IMAGE_ORDER_LSB_FIRST))
        {
            for (int i=0; i < image.height(); i++) {
                switch (format) {
                case QImage::Format_RGB16: {
                    ushort *p = (ushort*)image.scanLine(i);
                    ushort *end = p + image.width();
                    while (p < end) {
                        *p = ((*p << 8) & 0xff00) | ((*p >> 8) & 0x00ff);
                        p++;
                    }
                    break;
                }
                case QImage::Format_RGB32: // fall-through
                case QImage::Format_ARGB32_Premultiplied: {
                    uint *p = (uint*)image.scanLine(i);
                    uint *end = p + image.width();
                    while (p < end) {
                        *p = ((*p << 24) & 0xff000000) | ((*p << 8) & 0x00ff0000)
                            | ((*p >> 8) & 0x0000ff00) | ((*p >> 24) & 0x000000ff);
                        p++;
                    }
                    break;
                }
                default:
                    Q_ASSERT(false);
                }
            }
        }

        // fix-up alpha channel
        if (format == QImage::Format_RGB32) {
            QRgb *p = (QRgb *)image.bits();
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x)
                    p[x] |= 0xff000000;
                p += bytes_per_line / 4;
            }
        }

        result = QPixmap::fromImage(image.copy());
    }

    free(image_reply);
    return result;
}

xcb_pixmap_t qt_xcb_XPixmapFromBitmap(QXcbScreen *screen, const QImage &image)
{
    xcb_connection_t *conn = screen->xcb_connection();
    QImage bitmap = image.convertToFormat(QImage::Format_MonoLSB);
    const QRgb c0 = QColor(Qt::black).rgb();
    const QRgb c1 = QColor(Qt::white).rgb();
    if (bitmap.color(0) == c0 && bitmap.color(1) == c1) {
        bitmap.invertPixels();
        bitmap.setColor(0, c1);
        bitmap.setColor(1, c0);
    }
    const int width = bitmap.width();
    const int height = bitmap.height();
    const int bytesPerLine = bitmap.bytesPerLine();
    int destLineSize = width / 8;
    if (width % 8)
        ++destLineSize;
    const uchar *map = bitmap.bits();
    uint8_t *buf = new uint8_t[height * destLineSize];
    for (int i = 0; i < height; i++)
        memcpy(buf + (destLineSize * i), map + (bytesPerLine * i), destLineSize);
    xcb_pixmap_t pm = xcb_create_pixmap_from_bitmap_data(conn, screen->root(), buf,
                                                         width, height, 1, 0, 0, 0);
    delete[] buf;
    return pm;
}

xcb_cursor_t qt_xcb_createCursorXRender(QXcbScreen *screen, const QImage &image,
                                        const QPoint &spot)
{  
#ifdef XCB_USE_RENDER
    xcb_connection_t *conn = screen->xcb_connection();
    const int w = image.width();
    const int h = image.height();
    xcb_generic_error_t *error = 0;
    xcb_render_query_pict_formats_cookie_t formatsCookie = xcb_render_query_pict_formats(conn);
    xcb_render_query_pict_formats_reply_t *formatsReply = xcb_render_query_pict_formats_reply(conn,
                                                                                              formatsCookie,
                                                                                              &error);
    if (!formatsReply || error) {
        qWarning("qt_xcb_createCursorXRender: query_pict_formats failed");
        free(formatsReply);
        free(error);
        return XCB_NONE;
    }
    xcb_render_pictforminfo_t *fmt = xcb_render_util_find_standard_format(formatsReply, stdPictFormatForDepth(32)); //32BPP std pict format is argb_premultiplied
    if (!fmt) {
        qWarning("qt_xcb_createCursorXRender: Failed to find format PICT_STANDARD_ARGB_32");
        free(formatsReply);
        return XCB_NONE;
    }

    QImage img = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    xcb_image_t *xi = xcb_image_create(w, h, XCB_IMAGE_FORMAT_Z_PIXMAP,
                                       32, 32, 32, 32,
                                       QSysInfo::ByteOrder == QSysInfo::BigEndian ? XCB_IMAGE_ORDER_MSB_FIRST : XCB_IMAGE_ORDER_LSB_FIRST,
                                       XCB_IMAGE_ORDER_MSB_FIRST,
                                       0, 0, 0);
    if (!xi) {
        qWarning("qt_xcb_createCursorXRender: xcb_image_create failed");
        free(formatsReply);
        return XCB_NONE;
    }
    xi->data = (uint8_t *) malloc(xi->stride * h);
    if (!xi->data) {
        qWarning("qt_xcb_createCursorXRender: Failed to malloc() image data");
        xcb_image_destroy(xi);
        free(formatsReply);
        return XCB_NONE;
    }
    memcpy(xi->data, img.constBits(), img.byteCount());       //DHT 把image bits 拷贝到 XImage bits

    xcb_pixmap_t pix = xcb_generate_id(conn);
    xcb_create_pixmap(conn, 32, pix, screen->root(), w, h);

    xcb_render_picture_t pic = xcb_generate_id(conn);
    xcb_render_create_picture(conn, pic, pix, fmt->id, 0, 0);

    xcb_gcontext_t gc = xcb_generate_id(conn);
    xcb_create_gc(conn, gc, pix, 0, 0);
    xcb_image_put(conn, pix, gc, xi, 0, 0, 0);                        //DHT 把XImage bits 拷贝到pixmap
    xcb_free_gc(conn, gc);

    xcb_cursor_t cursor = xcb_generate_id(conn);
    xcb_render_create_cursor(conn, cursor, pic, spot.x(), spot.y());    //DHT  xrender 按照pixmap/picture 的方式操作cursor

    free(xi->data);
    xcb_image_destroy(xi);
    error = xcb_request_check(conn, xcb_render_free_picture_checked(conn, pic));
    if (error) {
        qFatal("qt_xcb_createCursorXRender: xcb_render_free_picture failed(%d)", pic);
    }

    xcb_free_pixmap(conn, pix);
    free(formatsReply);
    return cursor;

#else
    Q_UNUSED(screen);
    Q_UNUSED(image);
    Q_UNUSED(spot);
    return XCB_NONE;
#endif
}

/*****************************************************************

    QXcbShmImage

*****************************************************************/

//DHT depth for xserver AND format for qt 必须保持一致
//xserver@emgd:  Depth 24, bpp 32 ref imageFormatForDepth in xcbwindow.cpp
//xserver 使用depth == 24, 但buffer 是32bpp 的，因此qt也使用RGB32保存depth==24 的RGB
QXcbShmImage::QXcbShmImage(QXcbScreen *screen, const QSize &size, uint depth, QMrOfs::Format format)
    : QXcbObject(screen->connection())         //setConnection
    , m_gc(0)
    , m_gc_drawable(0)
#ifdef QT_MROFS
    , m_xcb_pixmap_shadow(XCB_NONE)
    , m_xcb_picture_shadow(XCB_NONE)
    , m_dirty2(QRegion())
    , m_xcb_rgn(XCB_NONE)
#endif
{
    xcb_generic_error_t *error = NULL;
    
    Q_XCB_NOOP(connection());

    qWarning("%s, size(%d,%d), depth(%d), format(0x%x)", __FUNCTION__, size.width(), size.height(), depth, format);
    
    //xcb image, shm  {{{
    m_xcb_image = xcb_image_create_native(xcb_connection(),
                                          size.width(),
                                          size.height(),
                                          XCB_IMAGE_FORMAT_Z_PIXMAP,      //ZPixmap!!!
                                          depth,
                                          0,                       // base == 0 && bytes==~0 && data == 0时，xcb 不分配 bits, 直接返回一个空 image !!!      (否则xcb 调用malloc)
                                          ~0,                      // 
                                          0);                      //

    const int segmentSize = m_xcb_image->stride * m_xcb_image->height;
    if (!segmentSize) {
        qWarning("%s, invalid segmentSize(%d)", __FUNCTION__, segmentSize);
        return;
    }
    
    //创建shm对象和存储空间(大小为segmentSize)
    int id = shmget(IPC_PRIVATE, segmentSize, IPC_CREAT | 0600);
    if (id == -1) {
        qWarning("QXcbShmImage: shmget() failed (%d) for size %d (%dx%d)",
                 errno, segmentSize, size.width(), size.height());
    } else {
        m_shm_info.shmid = id;
    }
    
    m_shm_info.shmaddr = m_xcb_image->data = (quint8 *)shmat(m_shm_info.shmid, 0, 0);
    m_shm_info.shmseg = xcb_generate_id(xcb_connection());
    
    error = xcb_request_check(xcb_connection(), xcb_shm_attach_checked(xcb_connection(), m_shm_info.shmseg, m_shm_info.shmid, false));
    if (error) {
        free(error);
        xcb_image_destroy(m_xcb_image);
        shmdt(m_shm_info.shmaddr);
        shmctl(m_shm_info.shmid, IPC_RMID, 0);
        m_shm_info.shmaddr = 0;
        //shm failure is unacceptable
        qFatal("QXcbShmImage::QXcbShmImage: shm attaching failed, segment(%x), id(%d)", m_shm_info.shmseg, m_shm_info.shmid);
    } else {
        //shm succeeded
        if (shmctl(m_shm_info.shmid, IPC_RMID, 0) == -1)
            qWarning() << "QXcbBackingStore: Error while marking the shared memory segment to be destroyed";
    }
    
    //QImage is the render target of bs and wf; it's undefined if used with iviewform(QImage with YUV format is INVALID) {{{
    m_qimage = QImage((uchar*) m_xcb_image->data, m_xcb_image->width, m_xcb_image->height, m_xcb_image->stride, qtFormatFromMainFormat(format));
    // }}}    
    
#ifdef QT_MROFS
    //pixmap 及其picture, 按m_xcb_image[0] 的尺寸创建 {{{
    //PERFORMANCE ISSUE: 对于bs/wf, 仅刷新dirty; 对于iView, dirty 为整个pixmap
    m_xcb_pixmap_shadow = xcb_generate_id(xcb_connection());
    error = xcb_request_check(xcb_connection(), 
        xcb_create_pixmap_checked(xcb_connection(), depth, m_xcb_pixmap_shadow, screen->root(), m_xcb_image->width, m_xcb_image->height));
    if (error) {
        qFatal("QXcbShmImage::QXcbShmImage, xcb_create_pixmap_checked failed: pixmap(%d), depth(%d), width(%d), height(%d)", 
            m_xcb_pixmap_shadow, depth, m_xcb_image->width, m_xcb_image->height);
    }
        
    //re-generate picture
    if(m_xcb_picture_shadow != XCB_NONE) {
        error = xcb_request_check(xcb_connection(), xcb_render_free_picture_checked(xcb_connection(), m_xcb_picture_shadow));
            if (error) {
                qWarning("QXcbShmImage::QXcbShmImage, xcb_render_free_picture failed: pict(%d)", 
                    m_xcb_picture_shadow);
            }
        m_xcb_picture_shadow = XCB_NONE;
    }

    //TODO: 如果边缘锯齿，打开PolyEdgeSmooth
    xcb_render_pictformat_t fmtId;
    if(format == QMrOfs::Format_ARGB32_Premultiplied) {
        fmtId = pictFormatFromStd(xcb_connection(), stdPictFormatForDepth(32));     //standard xrender argb
    } else if (format == QMrOfs::Format_xBGR32) {
        fmtId = pictFormatFromxBGR(xcb_connection());                               //non-standard xbgr(iview)
    }
    m_xcb_picture_shadow = xcb_generate_id(xcb_connection());
    error = xcb_request_check(xcb_connection(), 
        xcb_render_create_picture_checked(xcb_connection(), m_xcb_picture_shadow, m_xcb_pixmap_shadow, fmtId, 0, 0));    //origion: (0, 0), as bs' coord is the same as tlw's
    if (error) {
        qFatal("QXcbShmImage::QXcbShmImage, xcb_render_create_picture failed: error code(%d), pict(0x%x), pixmap(0x%x), fmtId(%d)", 
            error->error_code, m_xcb_picture_shadow, m_xcb_pixmap_shadow, fmtId);
    }
    // }}}

    // xcb region {{{
    m_xcb_rgn = xcb_generate_id(xcb_connection());
    xcb_xfixes_create_region (xcb_connection(), m_xcb_rgn, 0, NULL);
    // }}}
#endif   //QT_MROFS
}

void QXcbShmImage::destroy()
{
    xcb_generic_error_t *error = NULL;
    
    //xcb image
    const int segmentSize = m_xcb_image ? (m_xcb_image->stride * m_xcb_image->height) : 0;
    if (segmentSize && m_shm_info.shmaddr)
        Q_XCB_CALL(xcb_shm_detach(xcb_connection(), m_shm_info.shmseg));
    xcb_image_destroy(m_xcb_image);
    
    //shm
    if (segmentSize) {
        if (m_shm_info.shmaddr) {
            shmdt(m_shm_info.shmaddr);
            shmctl(m_shm_info.shmid, IPC_RMID, 0);
        }
    }
    
#ifdef QT_MROFS
        if(m_xcb_picture_shadow != XCB_NONE) {  
            error = xcb_request_check(xcb_connection(), xcb_render_free_picture_checked(xcb_connection(), m_xcb_picture_shadow));
            if (error) {
                qFatal("QXcbShmImage::destroy, xcb_render_free_picture failed: pict(%d)", m_xcb_picture_shadow);
            }
            m_xcb_picture_shadow = XCB_NONE;
        }
        xcb_free_pixmap(xcb_connection(), m_xcb_pixmap_shadow);
        m_xcb_pixmap_shadow = XCB_NONE;
    //xcb rgn
    xcb_xfixes_destroy_region(xcb_connection(), m_xcb_rgn);
#endif

    if (m_gc)
        Q_XCB_CALL(xcb_free_gc(xcb_connection(), m_gc));
}

//drawable 通吃 pixmap/window
//注意: QMrOfsCompositor::doComposite2 执行过程中，bs pixmap 和wv pixmap 的内容不能修改
//方法1: 将bs pixmap 的修改串行化，如全部放入compositor 线程
//方法2: put, doComposite2 用一把锁保护
//方法3: 将compositor 放入ui 线程(先用这种方法)
//注意: SNA 时，put 可省掉(ref. sna_pixmap_create_shm)
void QXcbShmImage::put(xcb_drawable_t drawable, const QPoint &target, const QRect &source)
{
    Q_XCB_NOOP(connection());
    if (m_gc_drawable != drawable) {
        if (m_gc)
            Q_XCB_CALL(xcb_free_gc(xcb_connection(), m_gc));

        m_gc = xcb_generate_id(xcb_connection());
        Q_XCB_CALL(xcb_create_gc(xcb_connection(), m_gc, drawable, 0, NULL));         //created and cached until drawable changed
        m_gc_drawable = drawable;
    }
    
    Q_XCB_NOOP(connection());
    
    Q_ASSERT(m_shm_info.shmaddr);

    //DHT 20150907 BEGIN
    //issue: 
    // 1, ui flicker while doing some animation( QWidget.update will be called at very high frequence - 60~80Hz )
    // 2, ui updating latency and unexpcected combination( 10 times updating combined in about 3 )
    //the best place to sync with xserver is right here, but xcb 1.18 did not provide checked version of xcb_image_shm_put, 
    //so we have to handle this condition after compositing, ref QMrOfsCompositor.drawBackingStore(xcb_render_composite_checked)
    //DHT 20150907 END
    //put m_shm_info[m_buf_current] into drawable
    xcb_image_shm_put(xcb_connection(),
                      drawable,
                      m_gc,
                      m_xcb_image,
                      m_shm_info,                   //xcb_shm_segment_info_t
                      source.x(),
                      source.y(),
                      target.x(),
                      target.y(),
                      source.width(),
                      source.height(),
                      false);

    Q_XCB_NOOP(connection());

    //DHT flush client command output buffer, but xserver may still be in defered state after xcb_flush...
    xcb_flush(xcb_connection());
    Q_XCB_NOOP(connection());
}

void QXcbShmImage::preparePaint(const QRegion &region)
{
    // do nothing
}

#ifdef QT_MROFS
/*!
    \note ui thread
*/
void QXcbShmImage::setBSEffectiveClip(const QRegion &region)
{
    m_dirty2 |= region;
    m_xcb_rgn = qtRegion2XcbRegion(xcb_connection(), m_dirty2, m_xcb_rgn);
}

/*!
    \note compositor thread!!!
    \note 
    if ui trigger compositing, ui thread is waiting during when m_dirty2+m_xcb_rgn being handled in compositor thread, so m_dirty2+m_xcb_rgn is safe
    else(wave,cursor,iv triggered), bs lock can protect m_dirty2+m_xcb_rgn 
    ref. QMrOfsCompositor::lockBackingStoreResources, QXcbShmImage::setBSEffectiveClip
*/
void QXcbShmImage::cleanBSEffectiveClip()
{
    //empty qt and xcb region
    m_dirty2 = QRegion();
    xcb_xfixes_set_region(xcb_connection(), m_xcb_rgn, 0, NULL);
}

#endif



QT_END_NAMESPACE
