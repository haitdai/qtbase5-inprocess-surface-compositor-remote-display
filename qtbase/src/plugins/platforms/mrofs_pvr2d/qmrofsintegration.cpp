#include "qmrofsintegration.h"
#include "qmrofsscreen.h"

#include <QtPlatformSupport/private/qgenericunixfontdatabase_p.h>
#include <QtPlatformSupport/private/qgenericunixeventdispatcher_p.h>
#include "qmrofsbackingstore.h"
#include "qmrofswindow.h"
#include "qmrofscursor.h"

#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/private/qpixmap_raster_p.h>

#if !defined(QT_NO_EVDEV) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK))
#include <QtPlatformSupport/private/qevdevmousemanager_p.h>
#include <QtPlatformSupport/private/qevdevkeyboardmanager_p.h>
#include <QtPlatformSupport/private/qevdevtouch_p.h>
#endif

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

void QMrOfsIntegration::initialize()
{
    qWarning("QMrOfsIntegration::initialize!!!");
    if (m_primaryScreen->initialize())
        screenAdded(m_primaryScreen);
    else
        qWarning("qmrofs: Failed to initialize screen");

    // inputmgr replace all evdev stuff        
#if 0
#if !defined(QT_NO_EVDEV) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK))
    new QEvdevKeyboardManager(QLatin1String("EvdevKeyboard"), QString() /* spec */, this);
    new QEvdevMouseManager(QLatin1String("EvdevMouse"), QString() /* spec */, this);
    new QEvdevTouchScreenHandlerThread(QString() /* spec */, this);
#endif   
#endif
}

bool QMrOfsIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
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
    printf("QMrOfsIntegration::createPlatformBackingStore\n");
    return new QMrOfsBackingStore(window);
}

QPlatformWindow *QMrOfsIntegration::createPlatformWindow(QWindow *window) const
{
    printf("QMrOfsIntegration::createPlatformWindow\n");
    return new QMrOfsWindow(window);
}

QAbstractEventDispatcher *QMrOfsIntegration::createEventDispatcher() const
{
    printf("QMrOfsIntegration::createEventDispatcher\n");
    return createUnixEventDispatcher();
}

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

QT_END_NAMESPACE
