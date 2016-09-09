#include "qmrofscursor_p.h"
#include "qxcbscreen.h"
#include "qmrofscompositor.h"

QT_BEGIN_NAMESPACE

/*!
    \note
    ���к�cursor��ص���Դһ������Ͳ��ͷţ�
    ֱ��qt��������Զ��ͷ�(xserver�˵���Դ��xserver�Զ�����)����Щ��Դ����:
    QMrOfsCursorImage���: pix, pic, qimage
    mCurrentRectXCB

    WATCH OUT: 
    since qt binds every TLW in qt primary screen(which is the same as xcb's primary output) wherever the TLW sit in,
    the mScreen is NOT relilable for caculating real cursor position
*/
QMrOfsCursor::QMrOfsCursor(QXcbScreen *screen)
        : mCompositor(NULL), mScreen(screen), mCurrentRect(QRect()), mPrevRect(QRect())
{
    mGraphic = new QMrOfsCursorImage(screen, NULL, 0, 0, 0, 0, 0);
    setCursor(Qt::ArrowCursor);

    mCurrentRectXCB  = xcb_generate_id(screen->xcb_connection());
    xcb_xfixes_create_region (screen->xcb_connection(), mCurrentRectXCB, 0, NULL); 
}

/*!
    rect in screen coordinate
    [ui thread]
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
    [inputmgr thread]
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
    ���ﲻ�ٻ��ƹ�꣬���л���굱ǰλ�ã����������λ�á�
    �ϳ��������λ���Ϻϳɹ�ꡣ
    [compositor thread]
*/
QRect QMrOfsCursor::drawCursor()
{
    mDirty = false;
    if (mCurrentRect.isNull() || !mCompositor)
        return QRect();

    QRect tlwRc = mCompositor->tlw()->geometry();       // in xcb screen's coordinate
    tlwRc = QRect(0, 0, tlwRc.width(), tlwRc.height());         // in TLW's coorinate
    if(!mCurrentRect.intersects(tlwRc))
        return QRect();

    qtRegion2XcbRegion(mScreen->xcb_connection(), mCurrentRect, mCurrentRectXCB);       //for compositor using
    
    mPrevRect = mCurrentRect;
    
    mOnScreen = true;
    
    return mCurrentRect;
}

xcb_xfixes_region_t QMrOfsCursor::cursorRectXCB(const QRect &rcInCursor)
{
    qtRegion2XcbRegion(mScreen->xcb_connection(), rcInCursor, mCurrentRectXCB);       //for compositor using
    return mCurrentRectXCB;
}

xcb_render_picture_t QMrOfsCursor::picture()
{
    return mGraphic->picture();
}

/*!
    old cursor range
    �ϳ��������λ���ϻָ�������ڵ�������
    [compositor thread]
*/
QRect QMrOfsCursor::dirtyRect()
{
//    qWarning("QMrOfsCursor::dirtyRect");
    if (mOnScreen) {
        mOnScreen = false;
        return mPrevRect;
    }
    return QRect();
}

void QMrOfsCursor::setCursor(Qt::CursorShape shape)
{
//    qWarning("QMrOfsCursor::setCursor1");
    mGraphic->set(shape);
}

void QMrOfsCursor::setCursor(const QImage &image, int hotx, int hoty)
{
//    qWarning("QMrOfsCursor::setCursor3");
    mGraphic->set(image, hotx, hoty);
}

void QMrOfsCursor::setCursor(const uchar *data, const uchar *mask, int width, int height, int hotX, int hotY)
{
//    qWarning("QMrOfsCursor::setCursor6");
    mGraphic->set(data, mask, width, height, hotX, hotY);
}

#ifndef QT_NO_CURSOR
void QMrOfsCursor::changeCursor(QCursor * widgetCursor, QWindow *window)
{
    mCompositor->changeCursor(widgetCursor, window);
}

/*!
    [compositor thread]
    ��qt ʵ�����ԣ������Ψһ�ģ���˸ı�window��cursor��ʵ���Ͼ��Ǹı�screen��cursor��
    ���Ҫ���window���ⲻͬ�����״��Ч����Ӧ��Ӧ�����ڽ�����˳�windowʱ����QWidget::setCursor��
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
    tlwRc = QRect(0, 0, tlwRc.width(), tlwRc.height());           // in xcb screen's coordinate
    if(mOnScreen || mCurrentRect.intersects(tlwRc))             // in TLW's coordinate
        setDirty();
}
#endif

/*!
    [ui thread]
*/
void QMrOfsCursor::setDirty()
{
    if (!mDirty) {
        mDirty = true;
        if(mCompositor)             //mCompositor Ҫ��compositor ���� ֮���ok,  ��cursor ��screen ��������compositor ��
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

void QMrOfsCursor::setCompositor(QMrOfsCompositor *compositor)
{
    mCompositor = compositor;
    compositor->setCursor(this);
}

QT_END_NAMESPACE
