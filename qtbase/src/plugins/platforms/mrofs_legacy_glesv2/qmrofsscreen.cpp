#include "qmrofsdefs.h"
#include <private/qcore_unix_p.h> // overrides QT_OPEN
#include <QtGui/QPainter>

#if !defined(QT_NO_EVDEV) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK))
#include <QtPlatformSupport/private/qdevicediscovery_p.h>
#endif


#include "qmrofsscreen.h"
#include "qmrofscompositor.h"
#include <qpa/qplatformscreen.h>
#include <qpa/qwindowsysteminterface.h>

QT_BEGIN_NAMESPACE

extern "C" {
/*
    \note dpi is relative to font point size ONLY.
    \note window/widget geometry is in pixel unit, BUT qpianter can be scaled.
    \note the actual pixel value and pixel localtion is determinated during qt's rasteriazation procedure.
*/
static QSizeF determinePhysicalSize(const fb_var_screeninfo &vinfo, const QSize &mmSize, const QSize &res)
{
    int mmWidth = mmSize.width(), mmHeight = mmSize.height();
    
    if (mmWidth <= 0 && mmHeight <= 0) {                                                  /*< usersize not OK, see vinfo  */
        if (vinfo.width != 0 && vinfo.height != 0                                           /*< vinfo OK */  
            && vinfo.width != UINT_MAX && vinfo.height != UINT_MAX) {
            mmWidth = vinfo.width;
            mmHeight = vinfo.height;
        } else {                                                                                       /*<vinfo not OK, use fix DPI instead */
            const int dpi = 72;      /*< typical value: dubhe 160, desktop 102, ip4s: 325 */
            mmWidth = qRound(res.width() * 25.4 / dpi);
            mmHeight = qRound(res.height() * 25.4 / dpi);
        }
    } else if (mmWidth > 0 && mmHeight <= 0) {
        mmHeight = res.height() * mmWidth/res.width();
    } else if (mmHeight > 0 && mmWidth <= 0) {
        mmWidth = res.width() * mmHeight/res.height();
    }
    return QSize(mmWidth, mmHeight);
}

static int openTtyDevice(const QString &device)
{
    const char *const devs[] = { "/dev/tty0", "/dev/tty", "/dev/console", 0 };

    int fd = -1;
    if (device.isEmpty()) {
        for (const char * const *dev = devs; *dev; ++dev) {
            fd = QT_OPEN(*dev, O_RDWR);
            if (fd != -1)
                break;
        }
    } else {
        fd = QT_OPEN(QFile::encodeName(device).constData(), O_RDWR);
    }

    return fd;
}

/*! 
    ref. VT_PROCESS
*/
static bool switchToGraphicsMode(int ttyfd, int *oldMode)
{
    ioctl(ttyfd, KDGETMODE, oldMode);
    if (*oldMode != KD_GRAPHICS) {
       if (ioctl(ttyfd, KDSETMODE, KD_GRAPHICS) != 0)
            return false;
    }

    // No blankin' screen, no blinkin' cursor!, no cursor!
    const char termctl[] = "\033[9;0]\033[?33l\033[?25l\033[?1c";
    QT_WRITE(ttyfd, termctl, sizeof(termctl));
    return true;
}

static void resetTty(int ttyfd, int oldMode)
{
    ioctl(ttyfd, KDSETMODE, oldMode);

    // Blankin' screen, blinkin' cursor!
    const char termctl[] = "\033[9;15]\033[?33h\033[?25h\033[?0c";
    QT_WRITE(ttyfd, termctl, sizeof(termctl));

    QT_CLOSE(ttyfd);
}

static void blankScreen(int fd, bool on)
{
    //TODO: rewrite with egl
    ioctl(fd, FBIOBLANK, on ? VESA_POWERDOWN : VESA_NO_BLANKING);
}

/*!
    \brief size in mm
*/
static int openFramebufferDevice(const QString &dev)
{
    int fd = -1;

    if (access(dev.toLatin1().constData(), R_OK|W_OK) == 0)
        fd = QT_OPEN(dev.toLatin1().constData(), O_RDWR);

    if (fd == -1) {
        if (access(dev.toLatin1().constData(), R_OK) == 0)
            fd = QT_OPEN(dev.toLatin1().constData(), O_RDONLY);
    }

    return fd;
}

} //extern "C"

/*!
    \note 
    qt internal use ARGB format, to avoid kind of "ARGB2RGBA" op, mBSFormat use RGB32 which does not need it.
    if we also want to use UI's alpha, set mBSFormat to QImage::Format_ARGB32.
    highly recommend UI use RGB32 instead of ARGB32(which implys ARGB32_premultiplied by QT) to render waveform buffer, so avoiding INV_PREMUL op.
*/
QMrOfsScreen::QMrOfsScreen(const QStringList &args) : mGeometry(), mDepth(QMROFS_SCREEN_DEPTH), mFormat(QMROFS_SCREEN_FORMAT)/*< ALPHA ALWAYS 0xFF */, 
    mPhysicalSize(), mOrientation(Qt::PortraitOrientation), mCompositor(NULL), mArgs(args), mFbFd(-1), mTtyFd(-1)
{
    if (mFbFd != -1) {
        close(mFbFd);
    }

    if (mTtyFd != -1) {
        resetTty(mTtyFd, mOldTtyMode);
        close(mTtyFd);
    }    
}

QMrOfsScreen::~QMrOfsScreen()
{
    
}

QPlatformCursor *QMrOfsScreen::cursor() const
{ 
    return mCompositor->cursor(); 
}
    
QWindow *QMrOfsScreen::topLevelAt(const QPoint & p) const
{
    QWindow *tlw = mCompositor->topLevelAt(p);
    return tlw;
}

Qt::ScreenOrientation QMrOfsScreen::setOrientation(Qt::ScreenOrientation orientation)
{
    Qt::ScreenOrientation  old_orientation = mOrientation;
    int scrn_border_longer, scrn_border_shorter;
       
    if (Qt::PrimaryOrientation >= orientation || Qt::InvertedLandscapeOrientation < orientation) 
        return old_orientation; 
    
    mOrientation = orientation;
    scrn_border_longer = mGeometry.height() > mGeometry.width() ? mGeometry.height() : mGeometry.width();           /*< longer border length */
    scrn_border_shorter = mGeometry.height() > mGeometry.width() ?  mGeometry.width() : mGeometry.height();         /*< shorter border length */
    
    //re-set screen geometry
    switch(orientation) {
        case Qt::PortraitOrientation:
            qWarning("[QMrOfsScreen::setOrientation] PortraitOrientation");                        
            setGeometry(QRect(0, 0, scrn_border_shorter, scrn_border_longer));    
            break;
        case Qt::LandscapeOrientation:
            qWarning("[QMrOfsScreen::setOrientation] LandscapeOrientation");                        
            setGeometry(QRect(0, 0, scrn_border_longer, scrn_border_shorter));
            //rotate 90 degree anti-clockwised around Z axis
            break;
        case Qt::InvertedPortraitOrientation:
            qWarning("[QMrOfsScreen::setOrientation] InvertedPortraitOrientation");
            setGeometry(QRect(0, 0, scrn_border_shorter, scrn_border_longer));      
            //rotate 180 degree anti-clockwised around Z axis
            break;
        case Qt::InvertedLandscapeOrientation:
            qWarning("[QMrOfsScreen::setOrientation] InvertedLandscapeOrientation");
            setGeometry(QRect(0, 0, scrn_border_longer, scrn_border_shorter));                    
            //rotate 90 degree clockwised around Z axis
            break;
        default:
            break;
    }
    
    //update screen orientation matrix in compositor 
    mCompositor->setScreenOrientation(orientation);
    
    QWindowSystemInterface::handleScreenOrientationChange(QPlatformScreen::screen(), mOrientation);
    
    //nothing to do with DPI ...
    
    return old_orientation;
}

/*!
    \note NOT used at present
*/
void QMrOfsScreen::setPhysicalSize(const QSize &size)
{
    qWarning("[QMrOfsScreen::setPhysicalSize] rect(%d,%d)", size.width(), size.height());
    mPhysicalSize = size;
}

/*!
    \note non-maximized tlws must set proper size by itself, here only update screen geometry and maximized tlws !!
*/
void QMrOfsScreen::setGeometry(const QRect &rect)
{
    qWarning("[QMrOfsScreen::setGeometry] rect(%d,%d,%d,%d)", rect.x(), rect.y(), rect.width(), rect.height());
    mGeometry = rect;
    
    //update screen geometry in compositor
    mCompositor->setScreenGeometry(rect);
    
    //desktopwidget will trigger workAreaResized when any SCREEN's geometry is changed ...
    QWindowSystemInterface::handleScreenGeometryChange(QPlatformScreen::screen(), geometry());    /*< will trigger primary orientation updating */
    QWindowSystemInterface::handleScreenAvailableGeometryChange(QPlatformScreen::screen(), availableGeometry());
    
    //maximized windows' geometry MUST be updated automatically
    resizeMaximizedWindows();
}

/*! 
    \brief  try to get physical size by FBDEV, if failed, use arguments instead.
*/
bool QMrOfsScreen::initializeFbDev(const QString & fbDevice, const QSize & userMmSize)
{
    QString fbDevice2 = fbDevice;
    if (fbDevice2.isEmpty()) {
        fbDevice2 = QLatin1String("/dev/fb0");
        if (!QFile::exists(fbDevice2))
            fbDevice2 = QLatin1String("/dev/graphics/fb0");
        if (!QFile::exists(fbDevice2)) {
            qWarning("Unable to figure out framebuffer device. Specify it manually.");
            return false;
        }
    }

    // Open the device
    mFbFd = openFramebufferDevice(fbDevice2);
    if (mFbFd == -1) {
        qWarning("Failed to open framebuffer %s : %s", qPrintable(fbDevice2), strerror(errno));
        return false;
    }

    // Read the fixed and variable screen information
    fb_fix_screeninfo finfo;
    fb_var_screeninfo vinfo;
    memset(&vinfo, 0, sizeof(vinfo));
    memset(&finfo, 0, sizeof(finfo));

    if (ioctl(mFbFd, FBIOGET_FSCREENINFO, &finfo) != 0) {
        qWarning("Error reading fixed information: %s", strerror(errno));
        return false;
    }
    qWarning("fb physical addr: 0x%lx(%dd), mmio addr:0x%lx(%dd)", finfo.smem_start, finfo.smem_len, finfo.mmio_start, finfo.mmio_len);

    if (ioctl(mFbFd, FBIOGET_VSCREENINFO, &vinfo)) {
        qWarning("Error reading variable information: %s", strerror(errno));
        return false;
    }
    qWarning("fb sync 0x%x, vmod 0x%x, fbdepth %dd, geometry %dd, %dd", vinfo.sync, vinfo.vmode, vinfo.bits_per_pixel, vinfo.xres, vinfo.yres);
    mFbDepth = vinfo.bits_per_pixel;
    mGeometry = QRect(0, 0, vinfo.xres, vinfo.yres);
    
    mPhysicalSize = determinePhysicalSize(vinfo, userMmSize, mGeometry.size());
    return true;
}

/*! 
    \brief  called by QMrOfsIntegrationPlugin.initialize() called by QGuiApplication::eventDispatcherReady()  \
    called by QCoreApplication::init() called by QCoreApplication ctor called by QGuiApplication ctor called by QApplication ctor
*/
bool QMrOfsScreen::initialize(QMrOfsCompositor *compositor)
{
    QRegExp ttyRx(QLatin1String("tty=(.*)"));
    QRegExp fbRx(QLatin1String("fb=(.*)"));
    QRegExp mmSizeRx(QLatin1String("mmsz=(\\d+)x(\\d+)"));

    QString fbDevice, ttyDevice;
    QSize userMmSize;                                                  /*< size in mm */
    bool doSwitchToGraphicsMode = true;

    // Parse arguments
    foreach (const QString &arg, mArgs) {
        if (arg == QLatin1String("nographicsmodeswitch"))       /*< tty controlling */
            doSwitchToGraphicsMode = false;
        else if (ttyRx.indexIn(arg) != -1)
            ttyDevice = ttyRx.cap(1);
        else if (fbRx.indexIn(arg) != -1)
            fbDevice = fbRx.cap(1);
        else if (mmSizeRx.indexIn(arg) != -1)
            userMmSize = QSize(mmSizeRx.cap(1).toInt(), mmSizeRx.cap(2).toInt());   /*dubhe:  48.96x73.44mm^2*/
    }

    mCompositor = compositor;
    //depends on mGeometry    
    initializeFbDev(fbDevice, userMmSize); 
    //initialize compositor, depends fb depth
    mGeometry = mCompositor->initialize(this);
    //handle tty stuff
    mTtyFd = openTtyDevice(ttyDevice);
    if (mTtyFd == -1)
        qWarning() << "Failed to open tty" << strerror(errno);

    if (doSwitchToGraphicsMode && !switchToGraphicsMode(mTtyFd, &mOldTtyMode))
        qWarning() << "Failed to set graphics mode" << strerror(errno);

    //reset screen pixels
    blankScreen(mFbFd, false);

    return true;
}

QT_END_NAMESPACE

