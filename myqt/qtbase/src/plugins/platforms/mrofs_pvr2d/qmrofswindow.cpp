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
        platformScreen()->addWindow(this);
    else
        platformScreen()->removeWindow(this);
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
    platformScreen()->raise(this);
}

void QMrOfsWindow::lower()
{
    platformScreen()->lower(this);
}

/*! 
    called by bs flush
*/
void QMrOfsWindow::repaint(const QRegion &region)
{
    // FIXME: in - screen compositor to support multiple windows...
    Q_UNUSED(region);
}

QT_END_NAMESPACE


