#ifndef QMROFSCURSOR_P_H
#define QMROFSCURSOR_P_H

#include <qpa/qplatformcursor.h>

QT_BEGIN_NAMESPACE

class QMrOfsScreen;
class QMrOfsCompositor;

class QMrOfsCursor : public QPlatformCursor
{
public:
    QMrOfsCursor(QMrOfsScreen *screen, QMrOfsCompositor *compositor);

    // output methods
    QRect dirtyRect();
    virtual QRect drawCursor(QPainter &painter);

    // input methods
    virtual void pointerEvent(const QMouseEvent &event);
#ifndef QT_NO_CURSOR
    virtual void changeCursor(QCursor *widgetCursor, QWindow *window);
#endif

    virtual void setDirty();
    virtual bool isDirty() const { return mDirty; }
    virtual bool isOnScreen() const { return mOnScreen; }
    virtual QRect lastPainted() const { return mPrevRect; }

private:
    void setCursor(const uchar *data, const uchar *mask, int width, int height, int hotX, int hotY);
    void setCursor(Qt::CursorShape shape);
    void setCursor(const QImage &image, int hotx, int hoty);
    QRect getCurrentRect();

    QMrOfsScreen *mScreen;
    QMrOfsCompositor *mCompositor;
    QRect mCurrentRect;      // next place to draw the cursor
    QRect mPrevRect;         // last place the cursor was drawn
    bool mDirty;
    bool mOnScreen;
    QPlatformCursorImage *mGraphic;
};

QT_END_NAMESPACE

#endif // QFBCURSOR_P_H

