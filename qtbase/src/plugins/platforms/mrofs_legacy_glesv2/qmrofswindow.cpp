#include "qmrofswindow.h"
#include "qmrofsscreen.h"
#include "qmrofscompositor.h"
#include <QtGui/QScreen>
#include <qpa/qwindowsysteminterface.h>

QT_BEGIN_NAMESPACE

QMrOfsWindow::QMrOfsWindow(QWindow *window)
    : QPlatformWindow(window), mBackingStore(0), mWindowState(Qt::WindowNoState)
{
    static QAtomicInt winIdGenerator(1);
    mWindowId = winIdGenerator.fetchAndAddRelaxed(1);
}

QMrOfsWindow::~QMrOfsWindow()
{
    
}

QMrOfsScreen *QMrOfsWindow::platformScreen() const
{
    return static_cast<QMrOfsScreen *>(window()->screen()->handle());
}

QMrOfsCompositor *QMrOfsWindow::platformCompositor() const
{
    return static_cast<QMrOfsCompositor *>(window()->screen()->compositor());
}

/*!
    \brief called by QWidget.resize(QWindow.setGeometry)  or  QWindow.resize or 
*/
void QMrOfsWindow::setGeometry(const QRect &rect)
{
    // store previous geometry for screen update
    mOldGeometry = geometry();

    QWindowSystemInterface::handleGeometryChange(window(), rect);

    QPlatformWindow::setGeometry(rect);
}
 
void QMrOfsWindow::setVisible(bool visible)
{
    if (visible) {
        if (mWindowState & Qt::WindowFullScreen)
            setGeometry(platformScreen()->geometry());
        else if (mWindowState & Qt::WindowMaximized)
            setGeometry(platformScreen()->availableGeometry());
    }
    QPlatformWindow::setVisible(visible);

    if (visible)
        platformCompositor()->addWindow(this);
    else
        platformCompositor()->removeWindow(this);
}


void QMrOfsWindow::setWindowState(Qt::WindowState state)
{
    QPlatformWindow::setWindowState(state);
    mWindowState = state;
}


void QMrOfsWindow::setWindowFlags(Qt::WindowFlags flags)
{
    mWindowFlags = flags;
}

Qt::WindowFlags QMrOfsWindow::windowFlags() const
{
    return mWindowFlags;
}

void QMrOfsWindow::raise()
{
    platformCompositor()->raise(this);
}

void QMrOfsWindow::lower()
{
    platformCompositor()->lower(this);
}

/*! 
    \brief region is the dirty region needs to be synced 
    \note region is in window coordinate
*/
void QMrOfsWindow::repaint(const QRegion &region)
{
#if 0
    //dirty the whole window(layer)
    platformCompositor()->setDirty(geometry());
#else
    QRect currentGeometry = geometry();                                        /*< window geometry is in global coordinate(multiple screens) */

    QRect dirtyClient = region.boundingRect();
    QRect dirtyRegion(currentGeometry.left() + dirtyClient.left(),          /*< translate to global coordinate */
                      currentGeometry.top() + dirtyClient.top(),
                      dirtyClient.width(),
                      dirtyClient.height());
    QRect mOldGeometryLocal = mOldGeometry;
    mOldGeometry = currentGeometry;
    #if  0                   //DHT, to review this
    // If this is a move, redraw the previous location
    if (mOldGeometryLocal != currentGeometry)
        platformCompositor()->setDirty(mOldGeometryLocal);
    #endif
    platformCompositor()->setDirty(dirtyRegion);                                       /*<global coordinates */
#endif
}

QT_END_NAMESPACE

