#include "qmrofscursor.h"
#include "qmrofsscreen.h"
#include "qmrofscompositor.h"
#include <QtGui/QPainter>

QT_BEGIN_NAMESPACE

QMrOfsCursor::QMrOfsCursor(QMrOfsScreen *screen, QMrOfsCompositor *compositor)
        : mScreen(screen), mCompositor(compositor), mCurrentRect(QRect()), mPrevRect(QRect())
{
    mGraphic = new QPlatformCursorImage(0, 0, 0, 0, 0, 0);
    setCursor(Qt::ArrowCursor);
}

QRect QMrOfsCursor::getCurrentRect()
{
    QRect rect = mGraphic->image()->rect().translated(-mGraphic->hotspot().x(),
                                                     -mGraphic->hotspot().y());
    rect.translate(QCursor::pos());
    QPoint mScreenOffset = mScreen->geometry().topLeft();
    rect.translate(-mScreenOffset);  // global to local translation
    return rect;
}


void QMrOfsCursor::pointerEvent(const QMouseEvent & e)
{
    Q_UNUSED(e);
    QPoint mScreenOffset = mScreen->geometry().topLeft();
    mCurrentRect = getCurrentRect();
    // global to local translation
    if (mOnScreen || mScreen->geometry().intersects(mCurrentRect.translated(mScreenOffset))) {
        setDirty();
    }
}

QRect QMrOfsCursor::drawCursor(QPainter & painter)
{
    mDirty = false;
    if (mCurrentRect.isNull())
        return QRect();

    // We need this because the cursor might be mDirty due to moving off mScreen
    QPoint mScreenOffset = mScreen->geometry().topLeft();
    // global to local translation
    if (!mCurrentRect.translated(mScreenOffset).intersects(mScreen->geometry()))
        return QRect();

    mPrevRect = mCurrentRect;
    painter.drawImage(mPrevRect, *mGraphic->image());
    mOnScreen = true;
    return mPrevRect;
}

QRect QMrOfsCursor::dirtyRect()
{
    if (mOnScreen) {
        mOnScreen = false;
        return mPrevRect;
    }
    return QRect();
}

void QMrOfsCursor::setCursor(Qt::CursorShape shape)
{
    mGraphic->set(shape);
}

void QMrOfsCursor::setCursor(const QImage &image, int hotx, int hoty)
{
    mGraphic->set(image, hotx, hoty);
}

void QMrOfsCursor::setCursor(const uchar *data, const uchar *mask, int width, int height, int hotX, int hotY)
{
    mGraphic->set(data, mask, width, height, hotX, hotY);
}

#ifndef QT_NO_CURSOR
void QMrOfsCursor::changeCursor(QCursor * widgetCursor, QWindow *window)
{
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
    QPoint mScreenOffset = mScreen->geometry().topLeft(); // global to local translation
    if (mOnScreen || mScreen->geometry().intersects(mCurrentRect.translated(mScreenOffset)))
        setDirty();
}
#endif

void QMrOfsCursor::setDirty()
{
    if (!mDirty) {
        mDirty = true;
        mCompositor->doComposite();
    }
}

QT_END_NAMESPACE
