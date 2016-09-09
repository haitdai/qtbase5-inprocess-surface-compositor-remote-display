#ifndef QMROFSWINDOW_H
#define QMROFSWINDOW_H

#include <qpa/qplatformwindow.h>

QT_BEGIN_NAMESPACE

class QMrOfsBackingStore;
class QMrOfsScreen;
class QMrOfsCompositor;

class QMrOfsWindow : public QPlatformWindow
{
public:
    QMrOfsWindow(QWindow *window);
    ~QMrOfsWindow();

    virtual void raise();
    virtual void lower();

    virtual void setGeometry(const QRect &rect);                 //for saving old geometry
    virtual void setVisible(bool visible);

    virtual void setWindowState(Qt::WindowState state);
    virtual void setWindowFlags(Qt::WindowFlags type);
    virtual Qt::WindowFlags windowFlags() const;

    WId winId() const { return mWindowId; }

    void setBackingStore(QMrOfsBackingStore *store) { mBackingStore = store; }
    QMrOfsBackingStore *backingStore() const { return mBackingStore; }
    QMrOfsScreen *platformScreen() const;

    virtual void repaint(const QRegion&);

protected:
    QMrOfsBackingStore *mBackingStore;
    QRect mOldGeometry;
    Qt::WindowFlags mWindowFlags;
    Qt::WindowState mWindowState;

    WId mWindowId;
};

QT_END_NAMESPACE

#endif // QMROFSWINDOW_H


