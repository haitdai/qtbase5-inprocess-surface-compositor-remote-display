#include <private/qcore_unix_p.h> // overrides QT_OPEN
#include <QtGui/QPainter>

#if !defined(QT_NO_EVDEV) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK))
#include <QtPlatformSupport/private/qdevicediscovery_p.h>
#endif

#include "qmrofsscreen.h"
#include "qmrofscompositor.h"
#include "qmrofswindow.h"
#include "qmrofscursor.h"
#include <qpa/qwindowsysteminterface.h>
#include "cmem.h"

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
        if (vinfo.width != 0 && vinfo.height != 0                                         /*< vinfo OK */  
            && vinfo.width != UINT_MAX && vinfo.height != UINT_MAX) {
            mmWidth = vinfo.width;
            mmHeight = vinfo.height;
        } else {                                                                          /*<vinfo not OK, use fix DPI instead */
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
QMrOfsScreen::QMrOfsScreen(const QStringList &args) : mGeometry(), mDepth(depthFromMainFormat(QMROFS_FORMAT_DEFAULT)), 
    mFormat(qtFormatFromMainFormat(QMROFS_FORMAT_DEFAULT)), mFormatPVR2D(PVR2D_ARGB8888),
    mPhysicalSize(), mOrientation(Qt::PortraitOrientation), mArgs(args), mFbFd(-1), mTtyFd(-1), m_pvr2DContext(0), m_pvr2DfbPic(NULL)
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
    if(m_pvr2DfbPic) {
        delete m_pvr2DfbPic;
        m_pvr2DfbPic = NULL;
    }
    if(this->m_pvr2DContext) {
        PVR2DDestroyDeviceContext(this->m_pvr2DContext);
        this->m_pvr2DContext = 0;
    }
}

QPlatformCursor *QMrOfsScreen::cursor() const
{ 
    return m_cursor;
}

void QMrOfsScreen::addWindow(QMrOfsWindow *window)
{
    if(m_winStack.size() >= 1) {
        //generating coredump
        qFatal("\n!!!ONLY 1 TLW IS SUPPORTED!!![existed(%p), new(%p)] \n", m_winStack[0], window);
    }
    m_winStack.prepend(window);
    if (!m_backingStores.isEmpty()) {
        for (int i = 0; i < m_backingStores.size(); ++i) {
            QMrOfsBackingStore *bs = m_backingStores.at(i);
            if (bs->window() == window->window()) {
                window->setBackingStore(bs); 
                m_backingStores.removeAt(i);
                break;
            }
        }
    }
    setDirty(window->geometry());
    QWindow *w = topWindow();
    topWindowChanged(w);

}

void QMrOfsScreen::removeWindow(QMrOfsWindow *window)
{
    m_winStack.removeOne(window);    
    setDirty(window->geometry());
    QWindow *w = topWindow();
    topWindowChanged(w);

}

void QMrOfsScreen::raise(QMrOfsWindow *window)
{
    int index = m_winStack.indexOf(window);
    if (index <= 0)
        return;
    
    m_winStack.move(index, 0);
    setDirty(window->geometry());
    QWindow *w = topWindow();
    topWindowChanged(w);
}

void QMrOfsScreen::lower(QMrOfsWindow *window)
{
    int index = m_winStack.indexOf(window);
    if (index == -1 || index == (m_winStack.size() - 1))
        return;
    m_winStack.move(index, m_winStack.size() - 1);
    setDirty(window->geometry());
    QWindow *w = topWindow();
    topWindowChanged(w);
}

void QMrOfsScreen::topWindowChanged(QWindow *window)
{
    Q_UNUSED(window);
    // nothing to do
}

QWindow *QMrOfsScreen::topWindow() const
{
    QWindow *tlw = 0;
    foreach (QMrOfsWindow *w, m_winStack) {
        if (w->window()->type() == Qt::Window || w->window()->type() == Qt::Dialog)
            tlw = w->window();
    }
    return tlw;
}

void QMrOfsScreen::setDirty(const QRect &rect)
{
    Q_UNUSED(rect);
    // do nothing when we use a tlw compositor
    // FIXME: if want a screen compositor
}

/* 
    visible TLWs
*/ 
QWindow *QMrOfsScreen::topLevelAt(const QPoint & p) const
{
    foreach (QMrOfsWindow *w, m_winStack) {
        if (w->geometry().contains(p, false) && w->window()->isVisible())
            return w->window();
    }
    return 0;
}

/*!
    FIXME: orientation
*/
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
    // TODO: notify compositor screen orientation changed
    // FIXME: in - screen compositor !!!!
    
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
    // TODO: notify compositor screen geometry changed
    
    //desktopwidget will trigger workAreaResized when any SCREEN's geometry is changed ...
    QWindowSystemInterface::handleScreenGeometryChange(QPlatformScreen::screen(), geometry());    /*< will trigger primary orientation updating */
    QWindowSystemInterface::handleScreenAvailableGeometryChange(QPlatformScreen::screen(), availableGeometry());
    
    //maximized windows' geometry MUST be updated automatically
    resizeMaximizedWindows();

#ifdef QT_MRDP
    // TODO: notify mrdp screen geometry changed
#endif
}

/*!
    pending bss will be set to window properly when QMrOfsWindow.addWindow called
*/
void QMrOfsScreen::addBackingStore(QMrOfsBackingStore *bs)
{
    m_backingStores << bs;
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

    // vinfo.yres > display.height 用于da8xx_pan_display 切换缓冲(eglSwapBuffers), 这里用fb shadow 替代, 因此yres 可以和display.height 相同
    // fb shadow 由pvr2D blt 到fb并等待完成
    qWarning("fb sync 0x%x, vmod 0x%x, fbdepth %dd, geometry %dd, %dd", vinfo.sync, vinfo.vmode, vinfo.bits_per_pixel, vinfo.xres, vinfo.yres);
    mFbDepth = vinfo.bits_per_pixel;
    mGeometry = QRect(0, 0, vinfo.xres, vinfo.yres);    // re-set by initialzePVR2DDev
    if (vinfo.bits_per_pixel == 16) {
        if (vinfo.red.length == 5 && vinfo.green.length == 6 &&
            vinfo.blue.length == 5 && vinfo.red.offset == 11 &&
            vinfo.green.offset == 5 && vinfo.blue.offset == 0) {
            mFormatPVR2D = PVR2D_RGB565;
        }
        if (vinfo.red.length == 4 && vinfo.green.length == 4 &&
            vinfo.blue.length == 4 && vinfo.transp.length == 4 &&
            vinfo.red.offset == 8 && vinfo.green.offset == 4 &&
            vinfo.blue.offset == 0 && vinfo.transp.offset == 12) {
            mFormatPVR2D = PVR2D_ARGB4444;
        }
    } else if (vinfo.bits_per_pixel == 32) {                      // SGX:  ARGB 
        if (vinfo.red.length == 8 && vinfo.green.length == 8 &&
            vinfo.blue.length == 8 && vinfo.transp.length == 8 &&
            vinfo.red.offset == 16 && vinfo.green.offset == 8 &&
            vinfo.blue.offset == 0 && vinfo.transp.offset == 24) {
            mFormatPVR2D = PVR2D_ARGB8888;
        }
    }
    
    mPhysicalSize = determinePhysicalSize(vinfo, userMmSize, mGeometry.size());
    return true;
}

bool QMrOfsScreen::initializePVR2DDev()
{
#ifdef QMROFS_MEM_ALLOCATED_CMEM
    CMEM_init();
#endif

    int numDevs = PVR2DEnumerateDevices(0); 
    if (numDevs <= 0) {
        qWarning("initializePVR2DDev: PVR2DEnumerateDevices failed [%d]", numDevs);
        return false;
    }
    PVR2DDEVICEINFO *pDevsInf = (PVR2DDEVICEINFO*) malloc(numDevs * sizeof(PVR2DDEVICEINFO));   
    if (!pDevsInf) {
        qWarning("initializePVR2DDev: malloc failed");
        return false;
    }
    if (PVR2DEnumerateDevices(pDevsInf) != PVR2D_OK) {
        qWarning("initializePVR2DDev: PVR2DEnumerateDevices failed");
        free(pDevsInf);
        return 0;
    }   
    free(pDevsInf);
    
    qWarning("PVR2DCreateDeviceContext!!!");
    if (PVR2DCreateDeviceContext (numDevs, &this->m_pvr2DContext, 0) != PVR2D_OK) {
        qWarning("initializePVR2DDev: PVR2DCreateDeviceContext failed");  
        return false;
    }
    

    if (PVR2DGetDeviceInfo(this->m_pvr2DContext, &m_displayInfo) != PVR2D_OK) {
        qWarning("initializePVR2DDev: PVR2DGetDeviceInfo failed");
    } else {
        qDebug("PVR2DGetDeviceInfo: maxflipchains(%lu), maxbuffersinchain(%lu), format(%u), width(%lu), height(%lu), stride(%ld), minflipinterval(%lu), maxflipinterval(%lu)",
            m_displayInfo.ulMaxFlipChains,
            m_displayInfo.ulMaxBuffersInChain,
            static_cast<uint>(m_displayInfo.eFormat),
            m_displayInfo.ulWidth,
            m_displayInfo.ulHeight,
            m_displayInfo.lStride,
            m_displayInfo.ulMinFlipInterval,
            m_displayInfo.ulMaxFlipInterval);
    }

    // Get display geometry instead of fb geometry
    PVR2DFORMAT format;
    PVR2D_LONG displayWidth, displayHeight, stride;
    PVR2D_INT refreshRate;
    if (PVR2DGetScreenMode(this->m_pvr2DContext, &format, &displayWidth, &displayHeight, &stride, &refreshRate)) {
            qWarning("initializePVR2DDev: PVR2DGetScreenMode failed");
            return false;
    }

    Q_ASSERT(format == mFormatPVR2D);
    
    // 更新screen geometry
    mGeometry.setX(0);
    mGeometry.setY(0);
    mGeometry.setWidth(displayWidth);
    mGeometry.setHeight(displayHeight);

    mFormat = qtFormatFromPVR2DFormat(mFormatPVR2D);
    if(QImage::Format_Invalid == mFormat) {
        qWarning("initializePVR2DDev: invalid format, pvr2d format(%u)", static_cast<uint>(mFormatPVR2D));
        return false;
    }
    // [not needs mumap] OR. mmap(fbdev) + PVR2DMemWrap [nees mumap]
    PVR2DMEMINFO *memWrapped;
    if(PVR2DGetFrameBuffer(this->m_pvr2DContext, PVR2D_FB_PRIMARY_SURFACE, &memWrapped) != PVR2D_OK) {
        qWarning("initializePVR2DDev: PVR2DGetFrameBuffer failed");
        return false;
    }
    // wrapped in QMrOfsShmImage
    QAbstractMrOfs::Format mfmt = mainFormatFromQtFormat(mFormat);
    qDebug("QMrOfsScreen::initializePVR2DDev: create fb picture");
    m_pvr2DfbPic = new QMrOfsShmImage(this, QSize(displayWidth, displayHeight), 
        depthFromMainFormat(mfmt), mfmt, memWrapped);
    if(!m_pvr2DfbPic) {
        qWarning("initializePVR2DDev: QMrOfsShmImage no memory!");
        PVR2DMemFree(this->m_pvr2DContext, memWrapped);
    }

    qDebug("QMrOfsScreen::initializePVR2DDev: qtformat(%u), pvrformat(%u), stride(%ld), fbmemstride(%u)", 
        static_cast<uint>(mFormat), static_cast<uint>(mFormatPVR2D), stride, m_pvr2DfbPic->stride());
    Q_ASSERT(stride == m_pvr2DfbPic->stride());     // in bytes
    return true;
}

/*! 
    \brief  called by QMrOfsIntegrationPlugin.initialize() called by QGuiApplication::eventDispatcherReady()  \
    called by QCoreApplication::init() called by QCoreApplication ctor called by QGuiApplication ctor called by QApplication ctor
*/
bool QMrOfsScreen::initialize(QMrOfsCompositor *compositor)
{
    Q_UNUSED(compositor);

    QRegExp ttyRx(QLatin1String("tty=(.*)"));
    QRegExp fbRx(QLatin1String("fb=(.*)"));
    QRegExp mmSizeRx(QLatin1String("mmsz=(\\d+)x(\\d+)"));

    QString fbDevice, ttyDevice;
    QSize userMmSize;                                           /*< size in mm */
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
    
    //depends on mGeometry    
    initializeFbDev(fbDevice, userMmSize); 
       
    //handle tty stuff
    mTtyFd = openTtyDevice(ttyDevice);
    if (mTtyFd == -1)
        qWarning() << "Failed to open tty" << strerror(errno);

    if (doSwitchToGraphicsMode && !switchToGraphicsMode(mTtyFd, &mOldTtyMode))
        qWarning() << "Failed to set graphics mode" << strerror(errno);

    //reset screen pixels
    blankScreen(mFbFd, false);
    
    // PVR2D initialization 
    initializePVR2DDev();
    
    //initialize compositor, depends fb depth

    // FIXME: if want screen compositor, notify compositor to initialize
    
    // initialize OK, create the cursor
    m_cursor = new QMrOfsCursor(this);    // will be set to compositor when 1st bs created
    return true;
}

PVR2DCONTEXTHANDLE QMrOfsScreen::deviceContext()
{
    return m_pvr2DContext;
}

QMrOfsShmImage *QMrOfsScreen::picture()
{
    return m_pvr2DfbPic;
}

/*!
    memory stride != width_in_pixel * bytesPerLine !!!
*/
int QMrOfsScreen::stride()
{
    return (m_pvr2DfbPic->stride());
}


QT_END_NAMESPACE

