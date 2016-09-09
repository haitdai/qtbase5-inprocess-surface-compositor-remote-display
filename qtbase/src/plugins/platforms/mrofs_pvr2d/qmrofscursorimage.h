#ifndef QMROFS_CURSOR_IMAGE_H
#define QMROFS_CURSOR_IMAGE_H

#include <QtCore/QList>
#include <QtGui/QImage>
#include <QtGui/QMouseEvent>
#include <QtCore/QWeakPointer>
#include <QtCore/QObject>
#include <qpa/qplatformscreen.h>
#include <QtGui/QCursor>
#include "qmrofsscreen.h"
#include "qmrofsdefs.h"

QT_BEGIN_NAMESPACE

class QMrOfsShmImage;

class Q_GUI_EXPORT QMrOfsCursorImage {
public:
    QMrOfsCursorImage(QMrOfsScreen *screen, const uchar *data, const uchar *mask, int width, int height, int hotX, int hotY);
    QImage * image();
    QMrOfsShmImage *picture();    
    QPoint hotspot() const;
    void set(const uchar *data, const uchar *mask, int width, int height, int hotX, int hotY);
    void set(const QImage &image, int hx, int hy);
    void set(Qt::CursorShape);

private:
    void createSystemCursor(int id);
    void createCursorPVR2D(int width, int height);
    QPoint               hot;
    QImage               qImage;         // 保持原有语义不变
    QMrOfsShmImage    *pvr2DImage;     // 同步到此作为合成的source
    QMrOfsScreen      *screen;
};

QT_END_NAMESPACE

#endif    //QMROFSV2_CURSOR_IMAGE_H
