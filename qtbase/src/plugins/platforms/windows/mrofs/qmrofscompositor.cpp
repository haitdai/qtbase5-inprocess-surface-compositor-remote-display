#include "qmrofscompositor.h"
#include "qmrofswaveform.h"
#include "qwindowswindow.h"
#include "qwindowsbackingstore.h"
#include "qwindowsscreen.h"
#include "qwindowsnativeimage.h"
#include "qwindowscontext.h"
#include <qpa/qwindowsysteminterface.h>
#include <QtCore/qglobal.h>
#include <QtCore/qmetaobject.h>

/*!
    windows specific:
    no care about cursor which handled by windows itself.
    no care about iview stuff.
*/

QT_BEGIN_NAMESPACE

QMrOfsCompositorThread::QMrOfsCompositorThread(QObject* parent, QMrOfsCompositor *cmptr) 
    : QThread(parent), compositor(cmptr)
#if 0    
    , id(0)
#endif    
{

}

QMrOfsCompositorThread::~QMrOfsCompositorThread()
{

}

void QMrOfsCompositorThread::run()
{
    /* 
    QTimer's hightest freuency seems to be 30Hz
    if self refresh, uncomment this:
    */
#if 0
    QTimer timer;
    timer.setTimerType(Qt::PreciseTimer);
    timer.start(10);             //much faster than wave timer
    m_timer = &timer;
    connect(&timer, SIGNAL(timeout()), compositor, SLOT(composite3()), Qt::AutoConnection);   //should direct
#endif

#if 0
    //INITIALIZE SCHED POLICY and PRIORITY
    struct sched_param   sched_param;
    int                             sched_policy = SCHED_OTHER;  //SCHED_FIFO, SCHED_RR, SCHED_OTHER
    const int                     default_pri;
    //range: sched_get_priority_min(sched_policy), sched_get_priority_max(sched_policy) 
    if (SCHED_FIFO == sched_policy || SCHED_RR == sched_policy) {
        default_pri = sched_get_priority_max(sched_policy) - 2;
    } else {
        default_pri = 0;
    }
    sched_param.sched_priority = default_pri;
    id = pthread_self();
    pthread_setschedparam(id, sched_policy, &sched_param);
#endif

    (void) exec();
}

QMrOfsCompositor::QMrOfsCompositor(QPlatformWindow *plat_win, QPlatformBackingStore *plat_bs) :
#ifdef QMROFS_THREAD_COMPOSITOR    
    m_th(NULL),
    m_mutex_bs(QMutex::Recursive),
    m_mutex_wv(QMutex::Recursive),    
    m_mutex_iv(QMutex::Recursive),
    m_mutex_wc(QMutex::Recursive),
#endif    
    m_win_scrn(NULL), m_win_win(plat_win), m_win_bs(plat_bs), m_wc(NULL), m_enabled(false), 
    m_conn_type(Qt::BlockingQueuedConnection),
    m_wv_flush_enabled(true), m_iv_flush_enabled(false), m_bs_flush_enabled(true), m_cr_flush_enabled(false), 
    m_qt_blit_rgn(QRegion())
{
    m_win_scrn = static_cast<QWindowsScreen *>(plat_win->screen());
    
    m_wc = new QMrOfsWaveController(plat_win);

    m_win_win->setCompositor(this);
    
#ifdef QMROFS_THREAD_COMPOSITOR
    m_conn_type = Qt::BlockingQueuedConnection;
    m_th = new QMrOfsCompositorThread(NULL, this);
    this->moveToThread(m_th);
    m_th->start();    
#else
    m_conn_type = Qt::DirectConnection;
#endif    
}

/*!
    [ui thread], called by QWindowsBackingStore::~QWindowsBackingStore
    \note
    first delete window, then backingstore, ref. QWidgetPrivate::deleteTLSysExtra
*/
QMrOfsCompositor::~QMrOfsCompositor()
{
    delete m_wc;
    
#ifdef QMROFS_THREAD_COMPOSITOR
    m_th->exit(0);
    delete m_th;
#endif
    m_qt_blit_rgn = QRegion();
}

QAbstractMrOfs::WV_ID QMrOfsCompositor::openWaveview()
{
    qWarning("NOT IMPLEMENTED, please call openWaveview(const QWidget&, const QRect&)");
    return -1;
}

void QMrOfsCompositor::closeWaveview(QAbstractMrOfs::WV_ID vid)
{
    lockWaveController();
    QMrOfsWaveview *wv = m_wc->fromVid(vid);  
    m_wc->closeWaveview(wv);
    unlockWaveController();      
}

QAbstractMrOfs::WV_ID QMrOfsCompositor::setWaveviewLayer(QAbstractMrOfs::WV_ID vid, int layer)
{
    Q_UNUSED(vid);
    Q_UNUSED(layer);
    qWarning("NOT IMPLEMENTED, please call QMrOfsCompositor::openPopup/closepopUp");
    return -1;
}

void QMrOfsCompositor::setWaveViewport(QAbstractMrOfs::WV_ID vid, const QRect& vpt)
{
    QMrOfsWaveview *wv = m_wc->fromVid(vid);
    m_wc->setWaveviewport(wv, vpt);
}

QAbstractMrOfs::WV_ID QMrOfsCompositor::createWaveform(QAbstractMrOfs::WV_ID vid)
{
    Q_UNUSED(vid);
    qWarning("NOT IMPLEMENTED, please call QMrOfsCompositor::createWaveform(QAbstractMrOfs::WV_ID vid, const QRect&)");
    return -1;
}

void QMrOfsCompositor::destroyWaveform(QAbstractMrOfs::WV_ID wid)
{
    lockWaveController();	
    QMrOfsWaveform *wf = m_wc->fromWid(wid);
    m_wc->removeWaveform(wf);
    unlockWaveController();
}

QWaveData* QMrOfsCompositor::setWaveformSize(QAbstractMrOfs::WV_ID wid, const QSize& sz)
{
    QMrOfsWaveform *wf = m_wc->fromWid(wid);
    if(!wf) {
        qWarning("QMrOfsCompositor::setWaveformSize: waveform(%p) NOT found, wid(%d)", wf, wid);
        return NULL;
    }

    QMrOfsWaveview *wv = m_wc->viewOfWaveform(wf);
    if(!wv) {
        qWarning("QMrOfsCompositor::setWaveformSize: waveview(%p) NOT found, wf(%p), wid(%d)", wv, wf, wid);
        return NULL;    
    }
    
    if(wv->isIView()) {                    //iview does not support resize dynamically, the only way is create-time setting by hdmi configuration
        return NULL;
    }
    
    QRect rc = wf->geometry();
    rc.setSize(sz);                        //position untouched
    m_wc->moveWaveform(wf, rc);

    return wf->data();                     //return new allocated QWaveData
}

void QMrOfsCompositor::bindWaveform(QAbstractMrOfs::WV_ID wid, QAbstractMrOfs::WV_ID vid)
{
    Q_UNUSED(wid);
    Q_UNUSED(vid);
    qWarning("NOT IMPLEMENTED: QMrOfsCompositor::bindWaveform");
}

QWaveData* QMrOfsCompositor::waveformRenderBuffer(QAbstractMrOfs::WV_ID wid)
{
    QMrOfsWaveform *wf = m_wc->fromWid(wid);
    if(NULL == wf) {
        qWarning("QMrOfsCompositor::waveformRenderBuffer: waveform NOT found, wid(%d)", wid);
        return NULL;
    }
    return wf->data();
}

QPoint QMrOfsCompositor::setWaveformPos(QAbstractMrOfs::WV_ID wid, const QPoint& pos)
{
    QMrOfsWaveform *wf = m_wc->fromWid(wid);
    if(!wf) {
        qWarning("QMrOfsCompositor::setWaveformPos: waveform(%p) NOT found, wid(%d)", wf, wid);
        return QPoint();
    }
    QRect rc = wf->geometry();
    rc.moveTo(pos.x(), pos.y());           //size untouched
    QRect rc_old = m_wc->moveWaveform(wf, rc);
    return QPoint(rc_old.x(), rc_old.y());
}

/*!
    rgn useless
    dirty region of waveforms have been already set by addWaveformClipRegion
*/
void QMrOfsCompositor::flushWaves(const QRegion& rgn)
{
    Q_UNUSED(rgn);
    qWarning("QMrOfsCompositor::flushWaves(const QRegion& rgn) not IMPLEMENTED, use null args version instead");
}

void QMrOfsCompositor::flushWaves()
{
    if(!m_wv_flush_enabled)
        return;
        
    //wave data has already been flushed before unlocking    
    composite(TRIGGERED_BY_WAVEFORMS);
}

/*!
    !!!wave thread!!!, must in the range of beginUpdateWaves and endUpateWaves
    BUG 28751, 28402, 28403
    1, 波形/iview 线程在此函数以及addWaveformClipRegion, submitIViewData 中读wv_widget_map，
    2, 波形/iview 线程不修改 wv_widget_map，
    3, 合成器线程(ui线程触发)在closeWaveview, openWaveview 中修改wv_widget_map，
    4, 合成器线程(ui或波形或iview线程触发)在drawWaveforms, updateEffectiveClips, cleanEffectiveClips, ... 中读wv_wiget_map
    因此，只需要保护合成器线程修改wv_widget_map 且波形/iview 线程读wv_widge_map 的情况(1 + 3)。
    进一步的，如果ui, iview, wv 线程都拥有自己的私有数据，
    只在flushBackingStore, flushWaves, flipIViewData 时同步数据到合成器线程，
    那么m_mutex_wv, m_mutex_iv, m_mutex_bs, m_mutex_wc 这4把锁都可以不要。
*/
Q_INVOKABLE void QMrOfsCompositor::submitWaveData()
{
    lockWaveController();
    m_wc->flushWaves();                                    //flush Data   
    unlockWaveController();
}

QRgb QMrOfsCompositor::getColorKey() const
{
    qWarning("NOT IMPLEMENTED: QMrOfsCompositor::getColorKey");
    return QRgb(0x0);           //transparent black
}

bool QMrOfsCompositor::enableComposition(bool enable)
{
    bool old = m_enabled;
    m_enabled = enable;
    return old;
}

void QMrOfsCompositor::lockWaveController()
{
#ifdef QMROFS_THREAD_COMPOSITOR
        m_mutex_wc.lock();
#endif
}

void QMrOfsCompositor::unlockWaveController()
{
#ifdef QMROFS_THREAD_COMPOSITOR
        m_mutex_wc.unlock();
#endif
}

/*!
    container belongs to a popup
*/
QAbstractMrOfs::WV_ID QMrOfsCompositor::openWaveview(const QWidget& container, const QRect& vpt)
{
    lockWaveController();
    QMrOfsWaveview* wv = m_wc->openWaveview(&container, vpt, false);
    unlockWaveController();    
    return wv->vid();
}

QAbstractMrOfs::WV_ID QMrOfsCompositor::createWaveform(QAbstractMrOfs::WV_ID vid, const QRect& rc)
{
    QMrOfsWaveview *wv = m_wc->fromVid(vid);
    if(!wv)
        return -1;

    //format == QMrOfs::Format_ARGB32_Premultiplied, bufCount == 1
    QMrOfsWaveform *wf = m_wc->addWaveform(wv, rc);
    if(!wf)
        return -1;
    
    return wf->wid();
}

void QMrOfsCompositor::unbindWaveform(QAbstractMrOfs::WV_ID wid, QAbstractMrOfs::WV_ID vid)
{
    Q_UNUSED(wid);
    Q_UNUSED(vid);    
    qWarning("NOT IMPLEMENTED: QMrOfsCompositor::unbindWaveform");
}

/*!
    \note root widget 必须最先调用,确保它位于z order 的最下!!!
*/
void QMrOfsCompositor::openPopup(const QWidget& popup)
{
    m_wc->openPopup(&popup);
}

void QMrOfsCompositor::closePopup(const QWidget& popup)
{
    m_wc->closePopup(&popup);
}

/*!
    wave/iview thread!!!, must run in the range of beginUpdateWaves and endUpdateWaves
*/
void QMrOfsCompositor::addWaveformClipRegion(QAbstractMrOfs::WV_ID wid, const QRegion &rgn)
{
    lockWaveController();
    QMrOfsWaveform *wf = m_wc->fromWid(wid);
    unlockWaveController();    
    m_wc->addWaveformClipRegion(wf, rgn);
}

void QMrOfsCompositor::addWaveformClipRegion(QAbstractMrOfs::WV_ID wid, const QRect &rc)
{
    lockWaveController();
    QMrOfsWaveform *wf = m_wc->fromWid(wid);
    unlockWaveController();    
    m_wc->addWaveformClipRegion(wf, rc);
}

/*!
    wave/iview thread, must run in the range of beginUpdateWaves and endUpdateWaves
*/
void QMrOfsCompositor::cleanWaveformClipRegion(QAbstractMrOfs::WV_ID wid)
{
    Q_UNUSED(wid);
    qWarning("QMrOfsCompositor::cleanWaveformClipRegion: NOT IMPLEMENTED");    
}

/*!
    wave/iview thread, must run in the range of beginUpdateWaves and endUpdateWaves
*/
void QMrOfsCompositor::cleanAllWaveformClipRegion()
{
    qWarning("QMrOfsCompositor::cleanAllWaveformClipRegion: NOT IMPLEMENTED");
}

/*!
    nothing to do
*/
QAbstractMrOfs::WV_ID QMrOfsCompositor::openIView(const QWidget &container, const QRect& vpt)
{
    Q_UNUSED(container);
    Q_UNUSED(vpt);
    return -1;
}

/*!
    nothing to do
*/
void QMrOfsCompositor::closeIView(QAbstractMrOfs::WV_ID vid) 
{
    Q_UNUSED(vid);
}

/*!
    nothing to do
*/
QAbstractMrOfs::WV_ID QMrOfsCompositor::createIViewform(QAbstractMrOfs::WV_ID vid, const HDMI_CONFIG hcfg, QMrOfs::Format fmt)
{
    Q_UNUSED(vid);
    Q_UNUSED(hcfg);
    Q_UNUSED(fmt);
    return -1;
}

/*!
    nothing to do
*/
void QMrOfsCompositor::submitIViewData(QAbstractMrOfs::WV_ID vid, QAbstractMrOfs::WV_ID wid, 
                                                                          const unsigned char *bits)
{
    Q_UNUSED(vid);
    Q_UNUSED(wid);
    Q_UNUSED(bits);
}

/*!
    nothing to do
*/
void QMrOfsCompositor::flushIView(QAbstractMrOfs::WV_ID wid)
{
    Q_UNUSED(wid);
}

void QMrOfsCompositor::lockIViewResources()
{
    m_mutex_iv.lock();
}

void QMrOfsCompositor::unlockIViewResources()
{
    m_mutex_iv.unlock();
}

#ifdef PAINT_WAVEFORM_IN_QT
/*! 
    [wave thread]
*/
void QMrOfsCompositor::paintWaveformLines(
    QImage* image,
    QAbstractMrOfs::XCOORDINATE xStart,
    const QAbstractMrOfs::YCOORDINATE* y,
    const QAbstractMrOfs::YCOORDINATE* yMax,
    const QAbstractMrOfs::YCOORDINATE* yMin,
    short yFillBaseLine,
    unsigned int pointsCount,
    const QColor& color,
    QAbstractMrOfs::LINEMODE mode)
{
    if (mode == QAbstractMrOfs::LINEMODE_DRAW) {        
        drawWaveformLines(image,
            xStart,
            y,
            yMax,
            yMin,
            pointsCount,
            color);    
    } else if (mode == QAbstractMrOfs::LINEMODE_FILL) {
        fillWaveformLines(image,
        xStart,
        y,
        yMin,
        yFillBaseLine,
        pointsCount,
        color);    
    } else {    
        // do nothing
    }
}

// restore left shift outside
static inline short rightShift8Bits(int y) {
    return (y >> 8);
}

void QMrOfsCompositor::drawWaveformLines(QImage* image,
    QAbstractMrOfs::XCOORDINATE xStart,
    const QAbstractMrOfs::YCOORDINATE* y,
    const QAbstractMrOfs::YCOORDINATE* yMax,
    const QAbstractMrOfs::YCOORDINATE* yMin,
    unsigned int pointsCount,
    const QColor& color) 
{ 
    if (image != 0) {
        uchar *dataPtr = const_cast<uchar*>( const_cast<const QImage*>(image)->bits() );        
        int w = image->width();        
        int h = image->height();
        for(int index = 0; index < pointsCount; index++) {
            if (xStart + index >= 0 && xStart + index < w) {
                if (rightShift8Bits(y[index]) >= 0 && rightShift8Bits(y[index]) < h && 
                    rightShift8Bits(yMax[index]) >= 0 && rightShift8Bits(yMax[index]) < h) {
                    int r = color.red() << 8;
                    int g = color.green() << 8;
                    int b = color.blue() << 8;
                    int diff = rightShift8Bits(yMax[index]) - rightShift8Bits(y[index]) + 1;
                    int rd = r / diff;                    
                    int gd = g / diff;                    
                    int bd = b / diff;                    
                    unsigned char * base = dataPtr + rightShift8Bits(y[index]) * w * 4 + (xStart + index) * 4;                                
                    for( int yy = rightShift8Bits(y[index]); yy < rightShift8Bits(yMax[index]) + 1; yy++) {
                        *(base + 0) = (unsigned char)(b >> 8);
                        *(base + 1) = (unsigned char)(g >> 8);
                        *(base + 2) = (unsigned char)(r >> 8);
                        *(base + 3) = 0xFF;
                        base += w * 4;
                        r -= rd;
                        g -= gd;
                        b -= bd;
                    }                
                }                
                if (rightShift8Bits(y[index]) >= 0 && rightShift8Bits(y[index]) < h && 
                    rightShift8Bits(yMin[index]) >= 0 && rightShift8Bits(yMin[index]) < h ) {                    
                    int r = color.red() << 8;                    
                    int g = color.green() << 8;                    
                    int b = color.blue() << 8;
                    int diff = rightShift8Bits(y[index]) - rightShift8Bits(yMin[index]) + 1;
                    int rd = r / diff;                    
                    int gd = g / diff;                    
                    int bd = b / diff;                    
                    unsigned char * base = dataPtr + (rightShift8Bits(y[index]) -1 ) * w * 4 + (xStart + index) * 4;                                
                    for (int yy = rightShift8Bits(y[index]) - 1; yy > rightShift8Bits(yMin[index]) - 1; yy--) {
                        *(base + 0) = (unsigned char)(b >> 8);
                        *(base + 1) = (unsigned char)(g >> 8);
                        *(base + 2) = (unsigned char)(r >> 8);
                        *(base + 3) = 0xFF;
                        base -= w * 4;
                        r -= rd;
                        g -= gd;
                        b -= bd; 
                    }
                }
            }
        }
    }
}
    
void QMrOfsCompositor::fillWaveformLines(QImage* image,                     
    QAbstractMrOfs::XCOORDINATE xStart,                     
    const QAbstractMrOfs::YCOORDINATE* y,                     
    const QAbstractMrOfs::YCOORDINATE* yMin,                    
    short yFillBaseLine,                    
    unsigned int pointsCount,                    
    const QColor& color) {    
    if (image != 0) {
        uchar *dataPtr = const_cast<uchar*>(const_cast<const QImage*>(image)->bits());        
        int w = image->width();        
        int h = image->height();        
        for (int index = 0; index < pointsCount; index++) {            
            if (xStart + index >= 0 && xStart + index < w) {                
                if (rightShift8Bits(y[index]) >= 0 && rightShift8Bits(y[index]) < h && 
                    rightShift8Bits(yMin[index]) >= 0 && rightShift8Bits(yMin[index]) < h) {
                    int r = color.red() << 8;                    
                    int g = color.green() << 8;                    
                    int b = color.blue() << 8;
                    int diff = rightShift8Bits(y[index]) - rightShift8Bits(yMin[index]) + 1;                    
                    int rd = r / diff;                    
                    int gd = g / diff;                    
                    int bd = b / diff;                    
                    unsigned char * base = dataPtr + rightShift8Bits(y[index])  * w * 4 + (xStart + index) * 4;                                
                    for (int yy = rightShift8Bits(y[index]); yy > rightShift8Bits(yMin[index]) - 1; yy--) {                         
                        *(base + 0) = (unsigned char)(b >> 8);                         
                        *(base + 1) = (unsigned char)(g >> 8);                         
                        *(base + 2) = (unsigned char)(r >> 8);                         
                        *(base + 3) = 0xFF;                        
                        base -= w * 4;                        
                        r -= rd;                        
                        g -= gd;                        
                        b -= bd;                    
                    }                
                }
                if (rightShift8Bits(y[index]) >= 0 && rightShift8Bits(y[index]) < h && 
                    yFillBaseLine >= 0 && yFillBaseLine < h ) {                      
                    unsigned char * base = dataPtr + ( rightShift8Bits(y[index]) + 1)  * w * 4 + (xStart + index) * 4;                                
                    for( int yy = rightShift8Bits(y[index]) + 1; yy < yFillBaseLine; yy++) {                         
                        *(base + 0) = color.blue();                         
                        *(base + 1) = color.green();                         
                        *(base + 2) = color.red();                         
                        *(base + 3) = 0xFF;                        
                        base += w * 4;                    
                    }                
                }            
            }        
        }    
    }
}

/*! 
    [wave thread]
*/
void QMrOfsCompositor::eraseRect(QImage* image, const QRect& rect)
{
    if ( image != 0) {        
        uchar *dataPtr = const_cast<uchar*>( const_cast<const QImage*>(image)->bits());        
        int w = image->width();        
        int h = image->height();        
        for (int y = rect.top(); y < rect.bottom(); y++) {
            if ( y >= 0 && y < h )            {                
                if ( rect.left() >= 0 && rect.right() < w )                {                    
                    uchar *d = dataPtr + (y * w * 4) + rect.left() * 4;                     
                    memset( d, 0, rect.width() * 4);                
                }
            }
        }
    }
}

/*! 
    [wave thread]
*/
void QMrOfsCompositor::drawVerticalLine(QImage* image, 
    QAbstractMrOfs::XCOORDINATE x, 
    QAbstractMrOfs::YCOORDINATE y1, 
    QAbstractMrOfs::YCOORDINATE y2, 
    const QColor& color)
{    
    if ( image != 0)    {        
        uchar *dataPtr = const_cast<uchar*>( const_cast<const QImage*>(image)->bits() );        
        int w = image->width();        
        int h = image->height();        
        if (y1 > y2) {            
            uint temp = y1;            
            y1 = y2;            
            y2 = temp;        
        }        
        if ( x >= 0 && x < w ) {            
            y1= 0.1, y2 = 0.2;
            int y = ( y1 >> 8 ); 
            if ( y >= 0 && y < h ) {                
                if ( (y1 & 0xFF ) != 0 )                {                    
                    uchar t = 256 - (y1 & 0xFF);                    
                    uchar *    d = dataPtr + ( y * w * 4 ) + x * 4;                     
                    *d       = (color.red() * t) >> 8;                    
                    *(d + 1 )= (color.green() * t) >> 8;                    
                    *(d + 2 )= (color.blue() * t) >> 8;                    
                    *(d + 3 )= 0xFF;                    
                    y++;                
                } else {                
                
                }            
            }            
            for( ; y < (y2 >> 8); y++) {
                if ( y >= 0 && y < h ) {                    
                    uchar *    d = dataPtr + (y * w * 4 ) + x * 4;                     
                    *d       = color.red();                    
                    *(d + 1 )= color.green();                    
                    *(d + 2 )= color.blue();                    
                    *(d + 3 )= 0xFF;                
                }            
            }            
            if ( (y2 & 0xFF ) != 0) {                
                y++;
                if (y >= 0 && y < h) {                    
                    uchar t = (y2 & 0xFF);                    
                    uchar *    d = dataPtr + ( y * w * 4 ) + x * 4;                     
                    *d       = (color.red() * t) >> 8;                    
                    *(d + 1 )= (color.green() * t) >> 8;                    
                    *(d + 2 )= (color.blue() * t) >> 8;                    
                    *(d + 3 )= 0xFF;                
                }
            }
        }
    }
}
#endif

/*!
    only take effects for rt threads
    if want to change nice value for non-rt thread(all threads in specified process or process group), ref. setpriority, nice.
    if want to change nice value for single non-rt thread, ref. sched_setattr (first supported in kernel 3.14)
*/
int QMrOfsCompositor::setCompositorThreadPriority(int newpri)
{
    // TODO: setPriority(QThread::Priority);
    Q_UNUSED(newpri);
    return 0;
}

void QMrOfsCompositor::lockWaveResources()
{
#ifdef QMROFS_THREAD_COMPOSITOR
    m_mutex_wv.lock();
#endif
}

void QMrOfsCompositor::unlockWaveResources()
{
#ifdef QMROFS_THREAD_COMPOSITOR
    m_mutex_wv.unlock();
#endif
}

void QMrOfsCompositor::lockBackingStoreResources()
{
#ifdef QMROFS_THREAD_COMPOSITOR
    m_mutex_bs.lock();
#endif
}

void QMrOfsCompositor::unlockBackingStoreResources()
{
#ifdef QMROFS_THREAD_COMPOSITOR
    m_mutex_bs.unlock();    
#endif
}

void QMrOfsCompositor::flushBackingStore(const QRegion& dirty)
{
    int at = this->metaObject()->indexOfMethod("flushBackingStore2(QRegion)");
    if (at == -1) {
        qWarning("QMrOfsCompositor::flushBackingStore2: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->m_conn_type, Q_ARG(QRegion, dirty));
}

/*!
    \note
    bs effective clip region MUST BE clean in compositor thread in flushBackingStore calling, 
    otherwise it will be in race condition with next compositing triggered by flushWaves. the same as wave effective region cleaning.
    \ref flushWaves, QMrOfsWaveController.cleanAllWaveformClipRegion
*/
void QMrOfsCompositor::flushBackingStore2(const QRegion& dirty) 
{
    Q_UNUSED(dirty);

    if(!m_bs_flush_enabled)
        return;

    //data has been flushed in QWindowsBackingStore
    composite(TRIGGERED_BY_BACKINGSTORE);
}

/*!
    \note 线程合成器时必须blocked，确保compositor 所有对bs数据的访问在重新分配bs 存储时已完成
    \note compositor 内不能保持对bs 数据地址的引用，否则resize 后可能产生野指针    
*/
void QMrOfsCompositor::resizeBackingStore(QPlatformBackingStore *bs, const QSize &size, const QRegion &rgn) 
{
    int at = this->metaObject()->indexOfMethod("resizeBackingStore2(QPlatformBackingStore*,QSize,QRegion)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::resizeBackingStore2: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    
    method.invoke(this, this->m_conn_type, Q_ARG(QPlatformBackingStore*, bs), Q_ARG(QSize, size), Q_ARG(QRegion, rgn));
}

void QMrOfsCompositor::resizeBackingStore2(QPlatformBackingStore *bs, const QSize &size, const QRegion &rgn) 
{
    bs->doResize(size, rgn);
}

void QMrOfsCompositor::setWindowGeometry(const QRect &wmRc)
{
    int at = this->metaObject()->indexOfMethod("setWindowGeometry2(QRect)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::setWindowGeometry: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    
    method.invoke(this, this->m_conn_type, Q_ARG(QRect, wmRc));    
}

void QMrOfsCompositor::setWindowGeometry2(const QRect &wmRc)
{
    // re-generate shadow
    m_win_win->destroyShadowImage();
    m_win_win->createShadowImage(wmRc);
}

/*!
    backingstore -> shadow
    RANGE:  all dirty waveforms region + dirty backingstore region, TLW coordinate
    OP:       SRC
*/
const QRegion& QMrOfsCompositor::drawBackingStore() 
{  
    //backingstore's coordinate is the same as tlw's, what we realy want here is region in backingstore's coordinatre, 
    const QRegion &qt_dirty_in_bs = m_wc->compositeRegion();

    QWindowsNativeImage *bsImg = reinterpret_cast<QWindowsNativeImage*>(m_win_bs->platformImage());
    QWindowsNativeImage *shadowImg = reinterpret_cast<QWindowsNativeImage*>(m_win_win->shadowImage());
    if (!bsImg || !shadowImg) {
        qErrnoWarning("%s: get platform image failed: bsImg(%p), shadowImg(%p)", __FUNCTION__, bsImg, shadowImg);
        return qt_dirty_in_bs;
    }
    
    QVector<QRect> rects = qt_dirty_in_bs.rects();
    for(int i = 0; i < rects.size(); i ++) {
        const QRect &rc = rects.at(i);
        if (!BitBlt(
                shadowImg->hdc(),      // dst DC
                rc.x(),                // dst x, [offset == 0, ref. QWindowsBackingStore.flush]
                rc.y(),                // dst y
                rc.width(),            // dst w
                rc.height(),           // dst h
                bsImg->hdc(),          // src DC
                rc.x(),                // src X
                rc.y(),                // src Y
                SRCCOPY)) {
            qErrnoWarning("%s: BitBlt failed", __FUNCTION__);
        }
    }    
    
    return qt_dirty_in_bs;
}

/*!
     wave controller(as object structure) will accept the visiting of it's waveform(as elements) from compositor(as visitor)
*/
void QMrOfsCompositor::drawWaveforms() 
{
    //traverse all waveforms via z-order bottom to top
    m_wc->accept(this); 
}

/*!
    visiting action to wave controller from compositor
    callback by QMrOfsWaveController
    \note
    RANGE: this waveform effective clip region, TLW coordinate
    OP:       PICT_OP_OVER    
*/
#include <QtGui>
void QMrOfsCompositor::visitWaveform(QMrOfsWaveform* wf)
{
    QWindowsNativeImage *wfImg = reinterpret_cast<QWindowsNativeImage*>(wf->platformImage());
    QWindowsNativeImage *shadowImg = reinterpret_cast<QWindowsNativeImage*>(m_win_win->shadowImage());
    if(!wfImg || !shadowImg) {
        qErrnoWarning("%s: get platform image failed: wfImg(%p), shadowImg(%p)", 
            __FUNCTION__, wfImg, shadowImg);
        return;
    }
    BLENDFUNCTION bfunc = {
        AC_SRC_OVER,    // OP, as for windows, ONLY over is supported
        0,              // flags, as for windows, ONLY 0 is supported
        0xFF,           // source const alpha, 0xFF means no
        AC_SRC_ALPHA,   // alpha format: premultiplied alpha
    };
    const QRegion &dirty_in_wf = wf->effectiveDirtyRegion();        // WF coordinates
    QVector<QRect> rects = dirty_in_wf.rects();
    for(int i = 0; i < rects.size(); i ++) {
        const QRect& rc = rects.at(i);       
        if(!AlphaBlend(
            shadowImg->hdc(),
            rc.x() + wf->originInTLW().x(), 
            rc.y() + wf->originInTLW().y(),
            rc.width(),
            rc.height(),
            wfImg->hdc(),
            rc.x(),
            rc.y(),
            rc.width(),
            rc.height(),
            bfunc
        )) {
            qErrnoWarning("%s: AlphaBlend Failed:(%d,%d,%d,%d), originInTLW(%d,%d), wfImage(%d,%d)", __FUNCTION__, 
                rc.x(),rc.y(),rc.width(),rc.height(),wf->originInTLW().x(), wf->originInTLW().y(), wfImg->width(), wfImg->height());
        }
    }
}

/*!
    \note
    IVIEW use SRC, not OVER
    NOTE: waiting finished here maybe unnecessary since iview has multiple buffers in mipi-csi
*/
void QMrOfsCompositor::visitIViewform(QMrOfsWaveform* wf)
{
    // nothing to do
    Q_UNUSED(wf);
}

/*!
    old position has already been re-composited during drawBackingStore and drawWaveforms
*/
QRect QMrOfsCompositor::drawCursor()
{
    return QRect();
}

/*
    Region: compositeRegion + cursorRegion(new position) in TLW coordinate
    Src:   shadow image
    Dst:   win_win DC or screen(UpdateLayeredWindowXXX)
*/
void QMrOfsCompositor::sync(const QRegion& compositeRgn, const QRegion& curRgn)
{
    m_qt_blit_rgn = compositeRgn + curRgn;

    QVector<QRect> rects = 
#ifdef QMROFS_SYNC_BOUNDING_RECT
    QVector<QRect>(1, m_qt_blit_rgn.boundingRect());
#else
    m_qt_blit_rgn.rects();
#endif
    if (rects.size() < 1)        
        return;
        
    QWindowsNativeImage *shadowImg = reinterpret_cast<QWindowsNativeImage*>(m_win_win->shadowImage());      // src   
    if(!shadowImg) {
        qErrnoWarning("%s: get shadow image failed: shadowImg(%p)", 
            __FUNCTION__, shadowImg);
        return;
    }
        
#ifndef Q_OS_WINCE
    const bool hasAlpha = m_win_win->format().hasAlpha();
    const Qt::WindowFlags flags = m_win_win->window()->flags();
    if ((flags & Qt::FramelessWindowHint) && QWindowsWindow::setWindowLayered(
        (reinterpret_cast<QWindowsWindow*>(m_win_win))->handle(), flags, hasAlpha, 
        (reinterpret_cast<QWindowsWindow*>(m_win_win))->opacity()) && hasAlpha) {
        QRect winFrameGeomtry = m_win_win->window()->frameGeometry();   // in TLW's parent (which is screen) coordinate
        SIZE size = {winFrameGeomtry.width(), winFrameGeomtry.height()};
        POINT ptDst = {winFrameGeomtry.x(), winFrameGeomtry.y()};  
        POINT ptSrc = {0, 0};
        BLENDFUNCTION blend = {
            AC_SRC_OVER, 
            0, 
            (BYTE)(255.0 * (reinterpret_cast<QWindowsWindow*>(m_win_win))->opacity()), 
            AC_SRC_ALPHA
        };           
        for(int i = 0; i < rects.size(); i ++) {        
            const QRect& rc = rects.at(i);
            if (QWindowsContext::user32dll.updateLayeredWindowIndirect) {
                RECT dirty = {rc.x(), rc.y(),
                    rc.x() + rc.width(), rc.y() + rc.height()};
                UPDATELAYEREDWINDOWINFO info = {
                    sizeof(info),           // size of this structure
                    NULL,                   // dc of screen, could be NULL
                    &ptDst,                 // new layered wnd position in screen (dst)
                    &size,                  // new layered wnd size in screen (dst)
                    shadowImg->hdc(),       // src dc
                    &ptSrc,                 // src layer position in src dc
                    0,                      // color key, not used
                    &blend,                 // blend function
                    ULW_ALPHA,              // use blend function flag
                    &dirty                  // dirty rect in src dc
                };
                QWindowsContext::user32dll.updateLayeredWindowIndirect((reinterpret_cast<QWindowsWindow*>(m_win_win))->handle(), &info);
            } else {
                QWindowsContext::user32dll.updateLayeredWindow(
                    (reinterpret_cast<QWindowsWindow*>(m_win_win))->handle(), NULL, &ptDst, &size, shadowImg->hdc(), &ptSrc, 0, &blend, ULW_ALPHA);
            }
        }
    } else {
#endif
        const HDC dc = (reinterpret_cast<QWindowsWindow*>(m_win_win))->getDC();
        if (!dc) {
            qErrnoWarning("%s: GetDC failed", __FUNCTION__);
            return;
        }
        for(int i = 0; i < rects.size(); i ++) {        
            const QRect& rc = rects.at(i); 
            if (!BitBlt(
                dc, 
                rc.x(),                     // dst x
                rc.y(),                     // dst y
                rc.width(),                 // dst w
                rc.height(),                // dst h
                shadowImg->hdc(),
                rc.x(),        // src x
                rc.y(),        // src y
                SRCCOPY)) {
                qErrnoWarning("%s: BitBlt failed", __FUNCTION__);
            }
        }
        (reinterpret_cast<QWindowsWindow*>(m_win_win))->releaseDC();
#ifndef Q_OS_WINCE
    }
#endif
}

/*!
    everytime ui changes, or visible waveform changes, re-composie them all
    \note
    1, 如果是界面 更新触发的合成, 则
    1.1 波形没有脏区域
    1.2 波形有脏区域
    2, 如果是波形更行触发的合成，则
    2.1 界面没有脏区域
    2.2 界面有脏区域
*/
void QMrOfsCompositor::composite(Reason reason)
{
#if defined(MROFS_PROFILING)
    static int frames;
    static struct timeval lastTime;
    struct timeval startTime, endTime;
    unsigned long diffTime;
    gettimeofday(&startTime, NULL);    
    //qWarning("MROFS_PROFILING: composite before: %ld:%ld", startTime.tv_sec, startTime.tv_usec);
#endif

    if (!m_enabled)            //switch off
        return;
    
    if (TRIGGERED_BY_BACKINGSTORE == reason) {        //ui thread must be waiting, locking wave resources enough
        lockWaveResources();
        lockIViewResources();
    } else if (TRIGGERED_BY_WAVEFORMS == reason) {    //wave thread must be waiting, locking bs resource is enough
        lockBackingStoreResources();
        lockIViewResources();
    } else if (TRIGGERED_BY_IVIEWFORMS == reason) {
        lockWaveResources();
        lockBackingStoreResources();
    } else if (TRIGGERED_BY_CURSOR == reason) {
        lockBackingStoreResources();
        lockWaveResources();
        lockIViewResources();
    }

    if(updateClipRegions()) {        //update in use
        const QRegion &compositeRgn = drawBackingStore();   //layer 0(topleast)
        drawWaveforms();                                    //layer 1(maybe more than 1 sub layers: Nx waveforms + 1x iviewform)
        QRect curRc = drawCursor();                         //layer 2(topmost)
        sync(compositeRgn, curRc);                          //win's shadow pixmap -> win ( OR. screen )
    }
    cleanClipRegions();              //clean bs+wv dirty regions processed this time
    
    if (TRIGGERED_BY_BACKINGSTORE == reason) {
        unlockWaveResources();
        unlockIViewResources();
    } else if (TRIGGERED_BY_WAVEFORMS == reason) {
        unlockBackingStoreResources();
        unlockIViewResources();
    } else if (TRIGGERED_BY_IVIEWFORMS == reason) {
        unlockWaveResources();
        unlockBackingStoreResources();
    } else if (TRIGGERED_BY_CURSOR == reason) {
        unlockBackingStoreResources();
        unlockWaveResources();
        unlockIViewResources();    
    }

#if defined(MROFS_PROFILING)
    gettimeofday(&endTime, NULL);
    diffTime = tv_diff(&startTime, &endTime);
    //qWarning("MROFS_PROFILING: composite after: %ld:%ld, diff: %ld", endTime.tv_sec, endTime.tv_usec, diffTime);

    //FPS
    static bool fpsDebug = qgetenv("QT_DEBUG_COMP_FPS").toInt();    
    if(fpsDebug) {
        frames ++;
        diffTime = tv_diff(&lastTime, &endTime);
        if(diffTime > 5000) {
            qWarning("compositing FPS: %ld, in seconds(%ld), frames(%ld)", (frames*1000)/diffTime, diffTime/1000, frames);
            lastTime = endTime;
            frames = 0;
        }
    }
#endif    
}

/*!
    iview layer can be more efficient since it's opaque(bs layer may omit rgn_iv_popup - rgns_above_iv_popup)
*/
bool QMrOfsCompositor::updateClipRegions()
{
    const QRegion &bs_clip = m_win_bs->effectiveClip();    
    return m_wc->updateEffectiveClips(bs_clip);
}

/*!
    把本次合成已处理的所有clip region 清空
*/
void QMrOfsCompositor::cleanClipRegions()
{
    //clean bs clip region
    m_win_bs->cleanEffectiveClip();
    
    //clean wv clip region
    m_wc->cleanEffectiveClips();           //clean wfs clip region and composite region
}

void QMrOfsCompositor::setCursor(QPlatformCursor *cursor)
{
    int at = this->metaObject()->indexOfMethod("setCursor2(QMrOfsCursor*)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::setCursor2: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->m_conn_type, Q_ARG(QPlatformCursor*, cursor));
}

void QMrOfsCompositor::setCursor2(QPlatformCursor *cursor)
{
    Q_UNUSED(cursor);
    // nothing to do
}

void QMrOfsCompositor::pointerEvent(const QMouseEvent &e)
{
    int at = this->metaObject()->indexOfMethod("pointerEvent2(QMouseEvent)");
    if (at == -1) {
        qWarning("QMrOfsCompositor::pointerEvent2: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->m_conn_type, Q_ARG(QMouseEvent, e));

}

void QMrOfsCompositor::pointerEvent2(const QMouseEvent &e)
{
    Q_UNUSED(e);

    // nothing to do
}

void QMrOfsCompositor::changeCursor(QCursor * widgetCursor, QWindow *window)
{
    int at = this->metaObject()->indexOfMethod("changeCursor2(QCursor*,QWindow*)");
    if (at == -1) {
        qWarning("QMrOfsCompositor::changeCursor2: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->m_conn_type, Q_ARG(QCursor*, widgetCursor), Q_ARG(QWindow*, window));

}

void QMrOfsCompositor::changeCursor2(QCursor * widgetCursor, QWindow *window)
{
    Q_UNUSED(widgetCursor);
    Q_UNUSED(window);
    // nothing to do
}

/*!
    ui thread -> compositor thread
*/
void QMrOfsCompositor::flushCursor()
{
    int at = this->metaObject()->indexOfMethod("flushCursor2()");
    if (at == -1) {
        qFatal("QMrOfsCompositor::flushCursor2: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->m_conn_type);
}

QRect QMrOfsCompositor::initialize(QPlatformScreen *scrn)
{
    // nothing to do
    Q_UNUSED(scrn);
    return QRect();
}

QRect QMrOfsCompositor::initialize2(QPlatformScreen *scrn)
{
    // nothing to do
    Q_UNUSED(scrn);
    return QRect();
}

bool QMrOfsCompositor::enableBSFlushing(bool enable)
{
    bool ret = m_bs_flush_enabled;
    m_bs_flush_enabled = enable;
    return ret;
}

bool QMrOfsCompositor::enableWVFlushing(bool enable)
{
    bool ret = m_wv_flush_enabled;
    m_wv_flush_enabled = enable;
    return ret;
}

bool QMrOfsCompositor::enableIVFlushing(bool enable)
{
    bool ret = m_iv_flush_enabled;
    m_iv_flush_enabled = enable;
    return ret;
}

bool QMrOfsCompositor::enableCRFlushing(bool enable)
{
    bool ret = m_cr_flush_enabled;
    m_cr_flush_enabled = enable;
    return ret;
}

QT_END_NAMESPACE


