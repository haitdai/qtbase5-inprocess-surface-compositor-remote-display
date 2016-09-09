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

#ifndef QXCBIMAGE_H
#define QXCBIMAGE_H

#include "qxcbscreen.h"
#include <QtCore/QPair>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <xcb/xcb_image.h>
#ifdef QT_MROFS
#include "mrofs/qmrofsdefs.h"
#include <QtPlatformSupport/private/qplatformcompositor_p.h>    // QPlatformPicture
#else
#include "qxcbstd.h"
#endif
QT_BEGIN_NAMESPACE

QImage::Format qt_xcb_imageFormatForVisual(QXcbConnection *connection,
                                           uint8_t depth, const xcb_visualtype_t *visual);
QPixmap qt_xcb_pixmapFromXPixmap(QXcbConnection *connection, xcb_pixmap_t pixmap,
                                 int width, int height, int depth,
                                 const xcb_visualtype_t *visual);
xcb_pixmap_t qt_xcb_XPixmapFromBitmap(QXcbScreen *screen, const QImage &image);
xcb_cursor_t qt_xcb_createCursorXRender(QXcbScreen *screen, const QImage &image,
                                        const QPoint &spot);

/*!
    X 没有提供获取pixmap pixel buffer 的接口。  
    想要qt直接修改pixmap pixel buffer，且兼容xrender，则必须修改xserver/xcb中和pixmap 相关的接口和实现。
    为简化实现，这里先不修改xserver/xcb, 而是将xshmimage 的dirty region 拷贝给pixmap，再合成这些pixmap。
    如果不满足iView 的性能要求，再修改。
*/

QT_BEGIN_NAMESPACE
class QXcbShmImage : public QXcbObject
#ifdef QT_MROFS
    , public QPlatformImage
#endif
{
public:
    QXcbShmImage(QXcbScreen *screen, const QSize &size, uint depth, QMrOfs::Format format);
    virtual ~QXcbShmImage() { destroy(); }

    QImage *image() { return &m_qimage; }                                              //called by bs, as paintdevice
    const QImage *image() const { return &m_qimage; }
    QSize size() const { return m_qimage.size(); }

    //xcb_window_t 和xcb_pixmap_t 类型相同，无法overload
    void put(xcb_drawable_t drawable, const QPoint &dst, const QRect &source);
    void preparePaint(const QRegion &region);                                           //called by bs
    
private:
    void destroy();

    //buffers {{{
    xcb_shm_segment_info_t  m_shm_info;
    xcb_image_t            *m_xcb_image;
    QImage                  m_qimage;                     //render target of WAVEFORM and BACKINGSTORE, single buffer
    // }}}
    
    xcb_gcontext_t m_gc;
    xcb_drawable_t m_gc_drawable;  //for QT_MROFS, drawable is pixmap; otherwise, drawable is window
    
#ifdef QT_MROFS
public:
    xcb_pixmap_t pixmap() { return m_xcb_pixmap_shadow; }                      //called by bs
    xcb_render_picture_t picture() { return m_xcb_picture_shadow; }             //called by compositor
    const QRegion& bsEffectiveClip() {return m_dirty2; }                            //TLW coordinates, xcb version, called by bs by compositor, ONLY for BS, DIRTY!!!
    xcb_xfixes_region_t bsEffectiveClipRegion() { return m_xcb_rgn; }     //xcb version of bsEffectiveClip
    void setBSEffectiveClip(const QRegion &region);
    void cleanBSEffectiveClip();
private:
    //xcbshmimage 的shadow pixmap，flush dirty region 到此(CPU完成，原因见emgd 实现)
    xcb_pixmap_t                    m_xcb_pixmap_shadow;
    xcb_render_picture_t            m_xcb_picture_shadow;
    QRegion                         m_dirty2;         //compositing use, only for BS, DIRTY!!!
    xcb_xfixes_region_t             m_xcb_rgn;        //xcb version of m_dirty2,  USELESS
#endif    //QT_MROFS
};

QT_END_NAMESPACE

#endif
