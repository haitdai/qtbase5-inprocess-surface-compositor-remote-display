#ifndef QMROFS_CURSOR_IMAGE_H
#define QMROFS_CURSOR_IMAGE_H

#include <QtCore/QList>
#include <QtGui/QImage>
#include <QtGui/QMouseEvent>
#include <QtCore/QWeakPointer>
#include <QtCore/QObject>
#include <qpa/qplatformscreen.h>
#include <QtGui/QCursor>
#include "qxcbscreen.h"
#include "qmrofsdefs.h"

QT_BEGIN_NAMESPACE

class Q_GUI_EXPORT QMrOfsCursorImage {
public:
    QMrOfsCursorImage(QXcbScreen *screen, const uchar *data, const uchar *mask, int width, int height, int hotX, int hotY);
    QImage * image();
    xcb_render_picture_t picture();
    QPoint hotspot() const;
    void set(const uchar *data, const uchar *mask, int width, int height, int hotX, int hotY);
    void set(const QImage &image, int hx, int hy);
    void set(Qt::CursorShape);

private:
    void createSystemCursor(int id);
    xcb_render_picture_t createCursorXRender(xcb_pixmap_t *pix, xcb_render_picture_t *pic);
    QImage cursorImage;
    QPoint hot;
    QXcbScreen          *xcbScreen;
    xcb_pixmap_t         xcbPix;
    xcb_render_picture_t xcbPic;
};

QT_END_NAMESPACE

#endif    //QMROFS_CURSOR_IMAGE_H
