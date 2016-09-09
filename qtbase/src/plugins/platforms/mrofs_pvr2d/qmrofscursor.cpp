#include "qmrofscursor.h"
#include "qmrofsscreen.h"
#include "qmrofscompositor.h"

QT_BEGIN_NAMESPACE

/*!
    \note
    所有和cursor相关的资源一旦分配就不释放，
    直到qt程序结束自动释放(xserver端的资源由xserver自动回收)，这些资源包括:
    QMrOfsCursorImage相关: pix, pic, qimage
*/
QMrOfsCursor::QMrOfsCursor(QMrOfsScreen *screen)
        : mCompositor(NULL), mScreen(screen), mCurrentRect(QRect()), mPrevRect(QRect())
{
    mGraphic = new QMrOfsCursorImage(screen, NULL, 0, 0, 0, 0, 0);
    setCursor(Qt::ArrowCursor);
}

/*!
    rect in screen coordinate
*/
QRect QMrOfsCursor::getCurrentRect()
{
    QRect rect = mGraphic->image()->rect().translated(-mGraphic->hotspot().x(),
                                                     -mGraphic->hotspot().y());

    // rect in TLW coordinate instead of screen's                                                     
    rect.translate(this->pos());
    
    return rect;
}

void QMrOfsCursor::pointerEvent(const QMouseEvent & e)
{
    mCompositor->pointerEvent(e);   
}

/*!
    [compositor thread]
    \note 
    1,
    by Qt default, mScreen is always qt primary-screen(i,e. xcb's primary output), 
    but, the top-most of which depends xrandr's -primary option
    2, ev.pos() is always in TLW coordinate
*/
void QMrOfsCursor::doPointerEvent(const QMouseEvent &e)
{
        Q_UNUSED(e);
    //    qDebug("QMrOfsCursor::pointerEvent");
    
        // setPos with logical position in mouse event which is in TLW coordinate (ref. QGuiApplicationPrivate.processMouseEvent)
        setPos(e.pos()); 
        
        mCurrentRect = getCurrentRect();
    
        // see if mCurrentRect is in current TLW
        if (mCompositor) {
            QRect tlwRc = mCompositor->tlw()->geometry();       // in screen's coordinate
            tlwRc = QRect(0, 0, tlwRc.width(), tlwRc.height());         // in TLW's coordinate      
            if(mOnScreen || tlwRc.intersects(mCurrentRect)) {
                setDirty();
    //            qDebug("pointerEvent: setDirty, mCurrentRect(%d,%d)", mCurrentRect.x(), mCurrentRect.y());
            } else {
    //            qDebug("pointerEvent: NOT setDirty, mCurrentRect(%d,%d)", mCurrentRect.x(), mCurrentRect.y());
            }
        }

}

/*!
    new cursor range
    这里不再绘制光标，仅切换光标当前位置，并返回这个位置。
    合成器在这个位置上合成光标。
    [compositor thread]
*/
QRect QMrOfsCursor::drawCursor()
{
//    qDebug("QMrOfsCursor::drawCursor");

    mDirty = false;
    if (mCurrentRect.isNull() || !mCompositor)
        return QRect();

    QRect tlwRc = mCompositor->tlw()->geometry();                   // in screen's coordinate
    tlwRc = QRect(0, 0, tlwRc.width(), tlwRc.height());             // in TLW's coorinate
    if(!mCurrentRect.intersects(tlwRc))
        return QRect();
    
    mPrevRect = mCurrentRect;
    
    mOnScreen = true;

//    qDebug("QMrOfsCursor::drawCursor: mCurrentRect(%d,%d)", mCurrentRect.x(), mCurrentRect.y());    
    return mCurrentRect;
}

QMrOfsShmImage *QMrOfsCursor::picture()
{
    return mGraphic->picture();
}

/*!
    old cursor range
    合成器在这个位置上恢复被光标遮挡的内容
    [compositor thread]
*/
QRect QMrOfsCursor::dirtyRect()
{
//    qDebug("QMrOfsCursor::dirtyRect");
    if (mOnScreen) {
        mOnScreen = false;
//        qDebug("QMrOfsCursor::dirtyRect: mPrevRect(%d,%d)", mPrevRect.x(), mPrevRect.y());    
        return mPrevRect;
    }
    return QRect();
}

void QMrOfsCursor::setCursor(Qt::CursorShape shape)
{
//    qDebug("QMrOfsCursor::setCursor");
    mGraphic->set(shape);
}

void QMrOfsCursor::setCursor(const QImage &image, int hotx, int hoty)
{
//    qDebug("QMrOfsCursor::setCursor");
    mGraphic->set(image, hotx, hoty);
}

void QMrOfsCursor::setCursor(const uchar *data, const uchar *mask, int width, int height, int hotX, int hotY)
{
//    qDebug("QMrOfsCursor::setCursor");
    mGraphic->set(data, mask, width, height, hotX, hotY);
}

#ifndef QT_NO_CURSOR
void QMrOfsCursor::changeCursor(QCursor * widgetCursor, QWindow *window)
{
    mCompositor->changeCursor(widgetCursor, window);
}

/*!
    [compositor thread]
    对qt 实例而言，光标是唯一的，因此改变window的cursor，实际上就是改变screen的cursor。
    如果要获得window内外不同光标形状的效果，应用应自行在进入和退出window时调用QWidget::setCursor。
    \ref  QWidget::setCursor
*/
void QMrOfsCursor::doChangeCursor(QCursor * widgetCursor, QWindow *window)
{
//    qDebug("QMrOfsCursor::changeCursor");
    if(!mCompositor) {
        qWarning("QMrOfsCursor::doChangeCursor: mCompositor is NULL");
        return;
    }
    
    Q_UNUSED(window);
    const Qt::CursorShape shape = widgetCursor ? widgetCursor->shape() : Qt::ArrowCursor;

    if (shape == Qt::BitmapCursor) {
        // application supplied cursor
        QPoint spot = widgetCursor->hotSpot();
        setCursor(widgetCursor->pixmap().toImage(), spot.x(), spot.y());
    } else {
        // system cursor
        setCursor(shape);
    }
    mCurrentRect = getCurrentRect();
    QRect tlwRc = mCompositor->tlw()->geometry();
    tlwRc = QRect(0, 0, tlwRc.width(), tlwRc.height());           // in screen's coordinate
    if(mOnScreen || mCurrentRect.intersects(tlwRc))             // in TLW's coordinate
        setDirty();
}
#endif

/*!
    [compositor thread]
*/
void QMrOfsCursor::setDirty()
{
    if (!mDirty) {
        mDirty = true;
        if(mCompositor)             //mCompositor 要到compositor 创建 之后才ok,  而cursor 随screen 创建，比compositor 早
            mCompositor->flushCursor();
    }
}

QPoint QMrOfsCursor::pos() const
{
    return mlastCursorPosition;
}

void QMrOfsCursor::setPos(const QPoint &pos)
{
    mlastCursorPosition = pos;
}

/*
    [ui thread]
*/
void QMrOfsCursor::setCompositor(QMrOfsCompositor *compositor)
{
    mCompositor = compositor;
    compositor->setCursor(this);
}

QT_END_NAMESPACE
