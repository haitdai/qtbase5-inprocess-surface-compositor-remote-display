#ifndef QMROFSCURSOR_H
#define QMROFSCURSOR_H

#include <qpa/qplatformcursor.h>
#include "qmrofsdefs.h"
#include "qmrofscursorimage.h"

QT_BEGIN_NAMESPACE

class QMrOfsScreen;
class QMrOfsCompositor;

/*!
    \note
    argb format instead of black-white-transparent format (render limitation)
    \note
    use Qt::BlankCursor to hide cursor    
    \ref
    QApplication::setOverrideCursor/restoreOverrideCursor
    QWidget::setCursor
*/
class QMrOfsCursor : public QPlatformCursor
{
public:
    QMrOfsCursor(QMrOfsScreen *screen);
    
    // output methods
    QRect dirtyRect();                     //old range
    virtual QRect drawCursor();          //current range
    QMrOfsShmImage *picture();            //for compositor
    // input methods
    virtual void pointerEvent(const QMouseEvent &event);
    virtual void doPointerEvent(const QMouseEvent &e);
#ifndef QT_NO_CURSOR
    virtual void changeCursor(QCursor *widgetCursor, QWindow *window);
    virtual void doChangeCursor(QCursor *widgetCursor, QWindow *window);
#endif

    virtual void setDirty();
    virtual bool isDirty() const { return mDirty; }
    virtual bool isOnScreen() const { return mOnScreen; }
    virtual QRect lastPainted() const { return mPrevRect; }
    void setCompositor(QMrOfsCompositor *compositor);
    virtual QPoint pos() const;
    virtual void setPos(const QPoint &pos);
    
private:
    void setCursor(const uchar *data, const uchar *mask, int width, int height, int hotX, int hotY);
    void setCursor(Qt::CursorShape shape);
    void setCursor(const QImage &image, int hotx, int hoty);
    QRect getCurrentRect();
    
    QMrOfsCompositor        *mCompositor;
    QMrOfsScreen             *mScreen;
    QRect                       mCurrentRect;           // next place to draw the cursor, screen coordinate
    QRect                       mPrevRect;              // last place the cursor was drawn, screen coordinate
    bool                        mDirty;                 // screen coordinate
    bool                        mOnScreen;
    QMrOfsCursorImage        *mGraphic;               // cursur image and bits
    QPoint                      mlastCursorPosition;
};

QT_END_NAMESPACE

#endif // QMROFSCURSOR_H

