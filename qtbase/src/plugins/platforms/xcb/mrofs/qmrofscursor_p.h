#ifndef QMROFSCURSOR_P_H
#define QMROFSCURSOR_P_H

#include <qpa/qplatformcursor.h>
#include "qmrofsdefs.h"
#include "qmrofscursorimage.h"

QT_BEGIN_NAMESPACE

class QXcbScreen;
class QMrOfsCompositor;

/*!
    \note
    argb format instead of black-white-transparent format (render limitation)
    \note
    1,
    use Qt::BlankCursor to hide cursor    
    2, 
    for multiple outputs situation, 
    ONLY cursor in qt primary screen is used since qt binds every TLW in it's primary screen (which is the same as xcb's primary output)
    \ref
    QApplication::setOverrideCursor/restoreOverrideCursor
    QWidget::setCursor
*/
class QMrOfsCursor : public QPlatformCursor
{
public:
    QMrOfsCursor(QXcbScreen *screen);
    
    // output methods
    QRect dirtyRect();                      //old range
    virtual QRect drawCursor();      //current range
    xcb_xfixes_region_t cursorRectXCB(const QRect &rc);   //current range XCB, cursor coordinate
    xcb_render_picture_t    picture();          //for compositor
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
    
    QMrOfsCompositor    *mCompositor;
    QXcbScreen                *mScreen;
    QRect                            mCurrentRect;      // next place to draw the cursor, screen coordinate
    QRect                            mPrevRect;           // last place the cursor was drawn, screen coordinate
    bool                              mDirty;                  //screen coordinate
    bool                              mOnScreen;
    QMrOfsCursorImage *mGraphic;              //cursur image and bits
    xcb_xfixes_region_t         mCurrentRectXCB; //mCurrentRect, cursor coordinate
    QPoint                          mlastCursorPosition;
};

QT_END_NAMESPACE

#endif // QMROFSCURSOR_P_H

