#include <QtPlatformSupport/private/qgenericunixfontdatabase_p.h>
#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>
#include "qmrofsdefs.h"
#include "qmrofsscreen.h"
#include "qmrofswaveform.h"
#include "qmrofsbackingstore.h"
#include "qmrofswindow.h"
#include "qmrofscursor.h"
#include "qmrofsintegration.h"
#include "qmrofscompositor.h"
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qpixmap_raster_p.h>

#if !defined(QT_NO_EVDEV) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK))
#include <QtPlatformSupport/private/qevdevmousemanager_p.h>
#include <QtPlatformSupport/private/qevdevkeyboardmanager_p.h>
#include <QtPlatformSupport/private/qevdevtouch_p.h>
#endif

#include "qmrofswaveform.h"

QT_BEGIN_NAMESPACE

QMrOfsIntegration::QMrOfsIntegration(const QStringList &paramList)
    : m_fontDb(new QGenericUnixFontDatabase())
{
    m_primaryScreen = new QMrOfsScreen(paramList);
}

QMrOfsIntegration::~QMrOfsIntegration()
{
    delete m_primaryScreen; 
}

//DHT 若要多屏支持，可在此开始扩充: m_primaryScreen
void QMrOfsIntegration::initialize()
{
#ifdef MROFS_USE_GETSCROLLRECT
    qWarning("mrofs: scroll acceleration version!!!");
#else
    qWarning("mrofs: non-scroll acceleration version!!!");    
#endif

    //instanate compositor before screen added to Qt
    QMrOfsCompositor *compositor = new QMrOfsCompositor();
    if (m_primaryScreen->initialize(compositor))       /*< QMrOfsScreen now know compositor */
        screenAdded(m_primaryScreen, compositor);   /*< QtGui now know compositor */
    else
        qWarning("mrofs: Failed to initialize screen");
    
    //create input handlers:
#if !defined(QT_NO_EVDEV) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK))
    new QEvdevKeyboardManager(QLatin1String("EvdevKeyboard"), QString() /* spec */, this);
    new QEvdevMouseManager(QLatin1String("EvdevMouse"), QString() /* spec */, this);
    new QEvdevTouchScreenHandlerThread(QString() /* spec */, this);
#endif       
}

bool QMrOfsIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    //DHT, FIXME
    switch (cap) {
    case ThreadedPixmaps: return true;
    case WindowManagement: return false;    
    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformPixmap *QMrOfsIntegration::createPlatformPixmap(QPlatformPixmap::PixelType type) const
{
    return new QRasterPlatformPixmap(type);
}

QPlatformBackingStore *QMrOfsIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QMrOfsBackingStore(window);
}

QPlatformWindow *QMrOfsIntegration::createPlatformWindow(QWindow *window) const
{
    return new QMrOfsWindow(window);
}

QAbstractEventDispatcher *QMrOfsIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

/*!
    \brief only 1 screen supported under mrofs
*/
QList<QPlatformScreen *> QMrOfsIntegration::screens() const
{
    QList<QPlatformScreen *> list;
    list.append(m_primaryScreen);
    return list;
}

QPlatformFontDatabase *QMrOfsIntegration::fontDatabase() const
{
    return m_fontDb;
}

QPlatformNativeInterface *QMrOfsIntegration::nativeInterface() const
{
    return const_cast<QMrOfsIntegration *>(this);
}

//todo: style hint (force root window to full screen)
//todo: inputcontext
//todo: genericunixservices

QT_END_NAMESPACE
