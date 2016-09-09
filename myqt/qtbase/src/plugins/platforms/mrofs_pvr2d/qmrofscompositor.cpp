#include "qmrofscompositor.h"
#include "qmrofswaveform.h"
#include "qmrofswindow.h"
#include "qmrofsbackingstore.h"
#include "qmrofsscreen.h"
#include <qpa/qwindowsysteminterface.h>
#if !defined(QT_NO_EVDEV) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK))
#include <QtPlatformSupport/private/qdevicediscovery_p.h>
#endif
#include <QtCore/qglobal.h>
#include <QtCore/qmetaobject.h>
#include "qmrofscursor.h"

#ifdef DEBUG_CPU_BLT
#include <QtGui/QPainter>
#include <QtGui/private/qdrawhelper_p.h>
#endif
#include "cmem.h"

/*!
ALGORITHMs:
    z order:
    bs above wv(wvs's z order set by openPopup)
    pixmap:
    bs(1x src), wvs(Nx src), fb(1x target)
    ������κ�һ�㲨�εĸ��£����������в����ĺϳɡ�
    ���ڵ����ֵĲ����ںϳ�ʱ���ü����������Ҫ͸����ʾ���ڵ��Ĳ��μ����棬��Ӧ�ü���Щ����
    �����Ҫ��͸��͸����ʾ���ڵ��Ĳ��Σ���Ӧ����alpha mask ��popups �����alpha��
    FindYoungestAncestorPopup: ��view �����������(Ҳ�Ǿ����������)popup
    Upper: ����z����popup ֮�ϵ�popups 
    Union: region union
    Subtract: region subtract
    DirtyRegion: view ��������(bs ��������bs ����)
    SetClip: ���úϳɵĲü�����(��Ч����)
    bs: backingstore
    bs_pix: pixmap of bs
    wf: 1 waveform
    wf_pix: pixmap of waveform
    wfs: collection of all waveforms
    wfs.z: z order of wfs
    ÿ�ζ��ϳ����в�
    SetClip(Union(DirtyRegion(wfs), DirtyRegion(bs)));
    Src(fb, null, bs_pix)  (dst, src, msk. ��֤ÿ�ζ������µ�bs)             [�����������������Ĳ������]
    wf = wfs.z.bottom;
    while(wf) {
      popup = FindYoungestAncestorPopup(wf)
      SetClip(Union(DirtyRegion(wf), DirtyRegion(bs)).Subtract(Union(Upper(popup)))           [�����ò��κͽ���������Ĳ�����ȥpopups����ϳ�]
      SrcOver(fb, null, wf_pix)
      wf = wfs.z.next;                     [from bottom to top]
    }
    
LIMITATIONs:    
    1, native widget(window) ��TLW ��������QPlatformWindow, 2 �ߵ���������: native widget(window) û��backingstore ��TLW �С�
    2, xcb ��˲�����TLWs �ĸ�����
    3, mrofs compositor ֻ��TLW �ڲ������ã����ܴ�����TLWs �ĺϳɡ�
    4, �����Ҫ֧�ֶ��TLW + ���εĺϳɣ���mrofs compositor Ӧ֧�ֶ��backingstore��ͬʱ��Ҫ֪����TLWs ��z-order,
        ��ʱmrofs �൱�ڲ�����Ϊoverlay ����Ļ�ϳ�����
    5, TLW �ڲ���native widget �����ϲ�Ӱ��ϳɵĽ��(��Ϊ���Ǻ�TLW ����1��backingstore)��
    6,                     format:                                           raster ops:                                                    regions:
            ui:            PICT_r8g8b8a8                                 PictOpSrc                                                      ui dirty + old cursor range + wave dirty
            wave:       PICT_r8g8b8a8                                 PictOpOver                                                    ui dirty + old cursor range + wave dirty - popups(include iview)
            iview:        PICT_x8b8g8r8(H2C: RGBx LSBFirst)     PictOpSrc                                                      iview full - popups above iview
            cursor:      PICT_r8g8b8a8                                 PictOpSrc+PictOpOver(prev), PictOpOver(cur)      new cursor range
    7, waveview, iview Ӧλ��ĳ��popup

DONE:
    * independent compositing thread and it's considerations;
        �ϲ��������߳�   
        resolve: ���������ĺϳ����߳�(to. be. implemented.)
        
    * waveforms ֮�䲻����

    *  wavform ֮�䲻������ʾ

    * һ����flushWaves

    *popup ֻ��visible widgets ��Ч

    *�Զ�clean dirty waveforms regions, ǰ�˳����������
    
    * independent wave thread
    �߳�ģ��: 1x ui + 1x compositor + 1x wave
    1, ui ��wave ��ÿ�θ��±�����ɵı��ϳɲ��ܶ�ʧ�����flushBackingStore ��flushWaves ���Ϊͬ���ӿ�(��������й�������)��
    2, ui �����ĺϳɽ���ʱ��wave ���ܷ�����£� ���Ҫ��compositor��wave�������Դ������
    3, wave�����ĺϳɽ���ʱ��uiͬ�����Ҫ��compositor��ui�������Դ������
    4, ��Ҫ��������Դ����dirty region(ui/waveд��compositor��) ��pixmap(ui/waveд��compositor��)��
    4.1, ���Ϊregion/pixmap �ֱ�����������˫�����붼˳��ӽ�ȫ��������˿ɺϲ�Ϊ1����������
    5, �ݹ�����ǵݹ��������ԣ�Ĭ���õݹ�����
    6, ����
    6.1 addWaveformClipRegion, flushWaves, cleanAllWaveformClipRegion ��wave�߳�ִ�С�
    6.2 beginPaint, flushBackingStore, endPaint ��ui�߳�ִ�С�
    6.3, composite ��compositor�߳�ִ�У���ui/wave�̵߳��ã���composite Ϊͬ���ӿڡ�
    6.4, ��lockBackingStore/unlock ����ui/compositor 2�̶߳�bs(dirty region/pixmap)�Ķ�д��
    6.5, ��lockWaveformRenderBuffer ����wave/compositor 2�̶߳�wave(dirty region/pixmap)�Ķ�д��
    6.6, ��������: ui/wave �ȴ�compositor֮ǰ����unlock������������
    6.7, ui��wave ֮��û�о������ʡ�

    * render buffer pointer dangling issue
    resize backingstore/waveform ʱ:
    1, enableComposition(false), ���������enableComposition(true)
    2, ���³�ʼ��QPainter(QPainter �Ỻ��backingstore image �ĵ�ַ)
    3, ���»�ȡQWaveData (waveformRenderBuffer/setWaveformSize)

    *[20150420]: iview ʹ��RGB ����, xv �������Ѳ�����
    ��xserver ʵ�ֵ�����(xv put imageֻ�������window drawable),  iview(xv) Ӧֱ������� root window �ϣ����Ǿ���compositor, ���ü���Ϣ�Կɺ�waveview/popup ����

    * iview composition; (YUV/RGB)
        ��waveform ��z ��ϳɣ�����ΪXRenderComposite/X[v]ShmPutImage������Ҫ��͸��Ч��:
            YUV: XvShmPutImage -> XRenderComposite(alpha mask, OP == OVER)
            RGBx: ���ISP ֱ�����alpha ͨ������Ͳ�����ͬ
                      ����XRenderComposite(alpha mask, OP == OVER)
        1��form���buffers, ��Ҫflip
        dirty ʼ��Ϊ����container widget ��Χ, ����ָ���ض���dirty region
        �����waveform ��ͬ      
    ע��: 
        1, Ϊ���Ϳ���, iview �沨�κϳɣ�FPS == 25, ʵ��FPS 60 ����cpu ��gpu�����Կɽ���
        2, iview Ӳ�����ػ���ĸ�ʽ(LSBFirst)��: Format_xBGR32, ref. gen7_get_card_format in emgd_sna
    
    * cursor composition
        ׼��2��rect: prev, cur, �ֱ��ʾ����ƶ�ǰ���2��λ�úʹ�С(��С����)
        ��ʼʱ, cur = original rect, prev = empty;
        �ƶ���, prev = cur, cur = new rect; ����prev ���뵽ui ��wave ����������; �����ϳ�(ע�ⲻ����upate)
        �ϳ�ʱ, ��prev ��Χ���ػ�ui/wave, ��current ��Χ�ڻ����
        ע��: 
            1, ���cursor ����������������cursor��������, Ŀǰ�ĺϳɲ�����:
                1.1, cursor λ�ñ仯(Ĭ�Ϲرգ�enableCRFlushing��)
                1.2, ��ui
                1.3, �沨��
            2, cursor ���������
                2.1, cursor �ĺϳɺ�������һ����������tlw����ϵ
                2.2, qt ά����tlw���ڵ�QScreen �Ǵ���ģ�����tlwĬ�϶���qt primary screen 
                2.3, inputmgr �ϱ������궼����xcb output ����ϵ, 
                    ����QGuiApplication.processMouseEvent ת����պ���tlw  ����ϵ, 
                    ������һ���������ȷ���ɺ�(ref. QGuiApplication::topLevelAt, QXcbScreen::topLevelAt)
                2.4, ����, ֱ��ʹ��tlw ����ϳɹ��
                ����: ��tlw ��top-left ����QScreen ��(0,0) ʱ���ϳ�λ�ý�������Ԥ��
                2.5, �����Ľ�������ǰѸ�tlw ��������ȷ��QScreen �У�������Ķ�̫��, �ݲ�ʵʩ
            3, cursor format ������
                qt �Դ���cursor��Դ��bitmap( 1 bit per pixel), ��Ҫ����ת����argb (32bit per pixel)
                
    * screen tearing
        emgd Tear Free ��̫�ɿ��������Լ���Tear-Free, ref QMrOfsCompositor::sync

    * cpu/gpu ���� ��ͬ��
        TODO

    * ISP byte order && alpha channel, ��iview composition

DOING:
    nothing
        
TODO:
    *multiple native window checking(???)
        �ϳ���ֻӰ�챾tlw�ڵ���Ϊ������ж��tlw����tlw�ڲ�Ӱ�죬��ʱ���޸ġ�
        
    * get widgets z-order info by children(QObjectData.children) instead of popups calling sequence (QMrOfs.openPopup) ???

    * ת��
*/

QT_BEGIN_NAMESPACE

QMrOfsCompositorThread::QMrOfsCompositorThread(QObject* parent, QMrOfsCompositor *cmptr) 
    : QThread(parent), compositor(cmptr), id(0)
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

QMrOfsCompositor::QMrOfsCompositor(QMrOfsWindow *win, QMrOfsBackingStore *bs) :
#ifdef QMROFS_THREAD_COMPOSITOR    
    m_th(NULL),
    m_mutex_bs(QMutex::Recursive),
    m_mutex_wv(QMutex::Recursive),    
    m_mutex_iv(QMutex::Recursive),
    m_mutex_wc(QMutex::Recursive),
#endif    
    m_scrn(NULL), m_win(win), m_bs(bs), m_wc(NULL), m_cursor(NULL), m_enabled(false), 
    m_conn_type(Qt::BlockingQueuedConnection),
    m_wv_flush_enabled(true), m_iv_flush_enabled(false), m_bs_flush_enabled(true), m_cr_flush_enabled(false),
    m_fb_shadow_pict(NULL)
#ifdef QT_MRDP
    ,
    m_mrdp(&MRDPConnMgr::instance()),
    m_mutex_screen(QMutex::Recursive)
#endif
{
    m_scrn = static_cast<QMrOfsScreen *>(m_win->screen());

#ifdef QT_MRDP
    m_mrdp->setScreenGeometry(m_scrn->geometry());
    m_mrdp->setScreenDepth(m_scrn->depth());
#endif

    initialize(m_scrn);    
    
    m_wc = new QMrOfsWaveController(win);
    
#if 0
    qDebug("QMrOfsCompositor::QMrOfsCompositor: create shadow image");
    m_fb_shadow_pict = new QMrOfsShmImage(m_scrn, 
        QSize(m_scrn->geometry().width(), m_scrn->geometry().height()), 
        depthFromMainFormat(QMROFS_FORMAT_DEFAULT), 
        QMROFS_FORMAT_DEFAULT);
#endif

    memset(&m_blt_inf, 0, sizeof(m_blt_inf));
    
#ifdef QMROFS_THREAD_COMPOSITOR
    m_conn_type = Qt::BlockingQueuedConnection;
    m_th = new QMrOfsCompositorThread(NULL, this);
    this->moveToThread(m_th);
    m_th->start();    
#else
    m_conn_type = Qt::DirectConnection;
#endif    
}

QMrOfsCompositor::~QMrOfsCompositor()
{
    delete m_wc;
    
#ifdef QMROFS_THREAD_COMPOSITOR
    m_th->exit(0);
    delete m_th;
#endif
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
    
    if(wv->isIView()) {                     //iview does not support resize dynamically, the only way is create-time setting by hdmi configuration
        return NULL;
    }
    
    QRect rc = wf->geometry();
    rc.setSize(sz);                            //position untouched
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
    1, ����/iview �߳��ڴ˺����Լ�addWaveformClipRegion, submitIViewData �ж�wv_widget_map��
    2, ����/iview �̲߳��޸� wv_widget_map��
    3, �ϳ����߳�(ui�̴߳���)��closeWaveview, openWaveview ���޸�wv_widget_map��
    4, �ϳ����߳�(ui���λ�iview�̴߳���)��drawWaveforms, updateEffectiveClips, cleanEffectiveClips, ... �ж�wv_wiget_map
    ��ˣ�ֻ��Ҫ�����ϳ����߳��޸�wv_widget_map �Ҳ���/iview �̶߳�wv_widge_map �����(1 + 3)��
    ��һ���ģ����ui, iview, wv �̶߳�ӵ���Լ���˽�����ݣ�
    ֻ��flushBackingStore, flushWaves, flipIViewData ʱͬ�����ݵ��ϳ����̣߳�
    ��ôm_mutex_wv, m_mutex_iv, m_mutex_bs, m_mutex_wc ��4���������Բ�Ҫ��
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

    //format == QMROFS_FORMAT_DEFAULT, bufCount == 1
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
    \note root widget �������ȵ���,ȷ����λ��z order ������!!!
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
    m_wc->addWaveformClipRegion(wf, rgn);       // waveform(and it's dirty region) is protected by m_mutex_wv
}

void QMrOfsCompositor::addWaveformClipRegion(QAbstractMrOfs::WV_ID wid, const QRect &rc)
{
    lockWaveController();
    QMrOfsWaveform *wf = m_wc->fromWid(wid);
    unlockWaveController();    
    m_wc->addWaveformClipRegion(wf, rc);        // waveform(and it's dirty region) is protected by m_mutex_wv
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
    \ref openWaveview
*/
QAbstractMrOfs::WV_ID QMrOfsCompositor::openIView(const QWidget &container, const QRect& vpt)
{
    lockWaveController();
    QMrOfsWaveview* wv = m_wc->openWaveview(&container, vpt, true);
    unlockWaveController();
    return wv->vid();    
}

/*!
    iviewforms destroyed automatically, the same as waveview
    ref. closeWaveView
*/
void QMrOfsCompositor::closeIView(QAbstractMrOfs::WV_ID vid) 
{
    lockWaveController();
    QMrOfsWaveview *wv = m_wc->fromVid(vid);
    m_wc->closeWaveview(wv);
    unlockWaveController();
}

/*!
    \note
    if format is YUV, then xv takes charge, otherwise, xrender does no matter bufCount is greater than 1 or not
*/
QAbstractMrOfs::WV_ID QMrOfsCompositor::createIViewform(QAbstractMrOfs::WV_ID vid, const HDMI_CONFIG hcfg, QMrOfs::Format fmt)
{
    QMrOfsWaveview *wv = m_wc->fromVid(vid);
    if(!wv)
        return -1;

    QRect rc(0, 0, 1, 1);
    switch (hcfg) {
    case HDMI_CONFIG_1280x720:
    default:
        rc.setWidth(1280);
        rc.setHeight(720);
    break;
    case HDMI_CONFIG_1024x768:
        rc.setWidth(1024);
        rc.setHeight(768);    
    break;
    case HDMI_CONFIG_1680x1050:
        rc.setWidth(1696);                  //ref resolution_array[2] in iview.cpp
        rc.setHeight(1050);        
    break;
    case HDMI_CONFIG_1280x800:
        rc.setWidth(1280);
        rc.setHeight(800);            
    break;
    case HDMI_CONFIG_800x600:
        rc.setWidth(800);
        rc.setHeight(600);
    break;
    case HDMI_CONFIG_1050x1680:
        rc.setWidth(1056);                  //ref resolution_array[5] in iview.cpp
        rc.setHeight(1680);
    break;
    }
    QMrOfsWaveform *wf = m_wc->addWaveform(wv, rc, fmt);
    if(!wf)
        return -1;
    
    return wf->wid();
}

/*!
    bits -> shmimage -> pixmap
    [iview thread]!!!
*/
void QMrOfsCompositor::submitIViewData(QAbstractMrOfs::WV_ID vid, QAbstractMrOfs::WV_ID wid, 
                                                                          const unsigned char *bits)
{
    lockWaveController();

    QMrOfsWaveview *iv = m_wc->fromVid(vid);
    if(!iv) {
        unlockWaveController();
        return;
    }
        
    QMrOfsWaveform *wf = m_wc->fromWid(wid);
    if(!wf) {
        unlockWaveController();
        return;
    }
    
    m_wc->flushIView(iv, wf, bits);
    
    // waveform(for iview) Ҳ��Ҫ����, ref QMrOfsCompositor::destroyWaveform
    // ˵��: ��flushIView ����m_mutex_wc �ı���֮�£���ʹ�ϲ����û�д��� QMrOfs::closeIView/closeWaveView ��QMrOfs::flipIView ��ͬ��,Ҳ����������:
    // ����1, closeIView/closeWaveView ֮�л�֮�����flipIView, ��ʱ:
    // 1.1, wv_widget_map �л�����iview��vid(dirty, pixel data Ҳ����),������ˢ��iview, ��ȫ(m_mutex_wc��֤��wv_widget_map��������);
    // 1.2, wv_widget_map ����ɾ��iview��vid����ʱ����iview��ˢ��, ��ȫ(fromVid ����NULL);
    // ����2, closeIView/closeWaveView ֮�л�֮���ٵ���flipIView, ��ʱ��Զ��ȫ(��򵥵����)��    
    unlockWaveController();    
}

/*!
    compositor thread
*/
void QMrOfsCompositor::flushIView(QAbstractMrOfs::WV_ID wid)
{
    QMrOfsWaveform *wf = m_wc->fromWid(wid);
    if(!wf)
        return;

    if(!m_iv_flush_enabled)
        return;
        
    composite(TRIGGERED_BY_IVIEWFORMS);
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
#ifdef QMROFS_THREAD_COMPOSITOR
    struct sched_param sched_param;
    int                           sched_policy;
    
    if (pthread_getschedparam(m_th->id, &sched_policy, &sched_param) != 0) {
        qWarning("QMrOfsCompositor::setCompositorThreadPriority: Cannot get scheduler parameters");
        return -1;
    }
    
    if(newpri > sched_get_priority_max(sched_policy) || newpri < sched_get_priority_min(sched_policy)) {
        qWarning("QMrOfsCompositor::setCompositorThreadPriority: illegal priority(%d) for current policy(%d)",
            newpri, sched_policy);
        return -1;    
    }
    
    sched_param.sched_priority = newpri;
    pthread_setschedparam(m_th->id, sched_policy, &sched_param);
#else
    Q_UNUSED(newpri);
#endif    
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

    //data has been flushed in QMrOfsBackingStore
    composite(TRIGGERED_BY_BACKINGSTORE);
}

/*!
    \note �̺߳ϳ���ʱ����blocked��ȷ��compositor ���ж�bs���ݵķ��������·���bs �洢ʱ�����
    \note compositor �ڲ��ܱ��ֶ�bs ���ݵ�ַ�����ã�����resize ����ܲ���Ұָ��    
*/
void QMrOfsCompositor::resizeBackingStore(QMrOfsBackingStore *bs, const QSize &size, const QRegion &rgn) 
{
    int at = this->metaObject()->indexOfMethod("resizeBackingStore2(QMrOfsBackingStore*,QSize,QRegion)");
    if (at == -1) {
        qWarning("QMrOfsCompositor::resizeBackingStore2: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    
    method.invoke(this, this->m_conn_type, Q_ARG(QMrOfsBackingStore*, bs), Q_ARG(QSize, size), Q_ARG(QRegion, rgn));
}

void QMrOfsCompositor::resizeBackingStore2(QMrOfsBackingStore *bs, const QSize &size, const QRegion &rgn) 
{
    bs->doResize(size, rgn);
}

/*!
    \note
    RANGE:  all dirty waveforms region + dirty backingstore region, TLW coordinate
    OP:       SRC
    qt_dirty_in_bs:  TLW coordinate == bs coordinate
*/
//#define DEBUG_PIXEL_BACKINGSTORE
//#define DEBUG_GPU_BLT_INF_BACKINGSTORE
const QRegion& QMrOfsCompositor::drawBackingStore() 
{
    //setup compositing clip region
    const QRegion & dirty_in_bs = m_wc->compositeRegion();    

    //backingstore's coordinate is the same as tlw's, what we realy want here is region in backingstore's coordinatre,     
    PVR2DERROR err = PVR2D_OK;
    QMrOfsShmImage *dstPic = m_scrn->picture();       // m_fb_shadow_pict   OPT
    QMrOfsShmImage *srcPic = reinterpret_cast<QMrOfsShmImage*>(m_bs->platformImage());

    if(!srcPic || !dstPic) {            // !srcPic:   backingstore has not been resized when something else(waves, cursors, ...) want to composite
        return dirty_in_bs;
    }
    QVector<QRect> rects = dirty_in_bs.rects();

#if defined(DEBUG_GPU_BLT) && defined(BATCH_BLT)
    static PVR2DRECT pvrRects[64];
    int realNum = rects.size() > 64 ? 64 : rects.size();
    if(rects.size() > 64) {
        qWarning("drawBackingStore: rects num exceeds 64");
    }
    for(int i = 0; i < realNum; i++) {
        const QRect& rc = rects.at(i);
        pvrRects[i].left = rc.left();
        pvrRects[i].top = rc.top();
        pvrRects[i].right = rc.right();
        pvrRects[i].bottom = rc.bottom();
#ifdef DEBUG_BACKINGSTORE
        qDebug("drawBackingstore rc_num(%d), index(%d), rc(%d,%d,%d,%d), dst(%d,%d),src(%d,%d)", 
            rects.size(), i,
            rc.left(), rc.top(), rc.width(), rc.height(),
            dstPic->image()->size().width(), dstPic->image()->size().height(),
            srcPic->image()->size().width(), srcPic->image()->size().height());
#endif        
    }
    m_blt_inf.CopyCode = PVR2DROPcopy;
    m_blt_inf.BlitFlags = PVR2D_BLIT_DISABLE_ALL;               // SRC OP     
    m_blt_inf.pDstMemInfo = dstPic->pvr2DMem(); 
    m_blt_inf.DstSurfWidth = dstPic->size().width(); 
    m_blt_inf.DstSurfHeight = dstPic->size().height();   
    m_blt_inf.DstStride = dstPic->stride();  
    m_blt_inf.DstFormat = dstPic->pvr2DFormat();
    m_blt_inf.DSizeX = srcPic->size().width();      
    m_blt_inf.DSizeY = srcPic->size().height();     
    m_blt_inf.DstX = 0;
    m_blt_inf.DstY = 0;
    
    m_blt_inf.pSrcMemInfo = srcPic->pvr2DMem();
    m_blt_inf.SrcSurfWidth = srcPic->size().width();
    m_blt_inf.SrcSurfHeight = srcPic->size().height(); 
    m_blt_inf.SrcStride = srcPic->stride();
    m_blt_inf.SrcFormat = srcPic->pvr2DFormat();
    m_blt_inf.SizeX = m_blt_inf.DSizeX;
    m_blt_inf.SizeY = m_blt_inf.DSizeY;
    m_blt_inf.SrcX = m_blt_inf.DstX;
    m_blt_inf.SrcY = m_blt_inf.DstY;

    if(!m_blt_inf.pDstMemInfo || !m_blt_inf.pSrcMemInfo) {
        return;
    }

    if((err = PVR2DBltClipped(m_scrn->deviceContext(), &m_blt_inf, realNum, pvrRects)) != PVR2D_OK) {
        qWarning("drawBackingStore: PVR2DBltClipped failed: error(%d)", err);
    }
    
    return dirty_in_bs;
#endif  // BATCH_BLT
    
    for(int i = 0; i < rects.size(); i ++) {
        const QRect& rc = rects.at(i);

#ifdef DEBUG_BACKINGSTORE
        qDebug("drawBackingstore rc_num(%d), index(%d), rc(%d,%d,%d,%d), dst(%d,%d),src(%d,%d)", 
            rects.size(), i,
            rc.left(), rc.top(), rc.width(), rc.height(),
            dstPic->image()->size().width(), dstPic->image()->size().height(),
            srcPic->image()->size().width(), srcPic->image()->size().height());
#endif

#if defined(DEBUG_CPU_BLT) || defined(DEBUG_PIXEL_BACKINGSTORE)
        QImage *srcImg = srcPic->image();
        unsigned int *srcBits = (unsigned int*)(srcImg->bits());
        QImage *dstImg = dstPic->image();
        unsigned int *dstBits = (unsigned int*)(dstImg->bits());
        QRect intersect = rc.intersected(QRect(0, 0, dstImg->width(), dstImg->height()));
        intersect = intersect.intersected(QRect(0, 0, srcImg->width(), srcImg->height()));
#endif

#ifdef DEBUG_PIXEL_BACKINGSTORE
        static int k;
        k++;
        int start_m = rc.top();
        int end_m = rc.top() + rc.height();
        int start_n = rc.left();
        int end_n = rc.left() + rc.width();
        if(k%2) {
                   for(int m = start_m; m < end_m; m++) {
                    for(int n = start_n; n < end_n; n ++) {
                            srcBits[m * srcImg->width() + n] = 0xFF9F9DEF; //;0xFFFF0000
                        }
                    }
        } else {
               for(int m = start_m; m < end_m; m++) {
                for(int n = start_n; n < end_n; n ++) {
                        srcBits[m * srcImg->width() + n] = 0xFFEFEBE7; //;0xFF0000FF
                    }
                }
        }
#endif      // DEBUG_PIXEL_BACKINGSTORE

#ifdef DEBUG_GPU_BLT       // GPU BLT
        m_blt_inf.CopyCode = PVR2DROPcopy;
        m_blt_inf.BlitFlags = PVR2D_BLIT_DISABLE_ALL;               // SRC OP     
        m_blt_inf.pDstMemInfo = dstPic->pvr2DMem(); 
        m_blt_inf.DstSurfWidth = dstPic->size().width(); 
        m_blt_inf.DstSurfHeight = dstPic->size().height();   
        m_blt_inf.DstStride = dstPic->stride();  
        m_blt_inf.DstFormat = dstPic->pvr2DFormat();       
        m_blt_inf.DSizeX = rc.width();      
        m_blt_inf.DSizeY = rc.height();     
        m_blt_inf.DstX = rc.x();
        m_blt_inf.DstY = rc.y();
        
        m_blt_inf.pSrcMemInfo = srcPic->pvr2DMem();
        m_blt_inf.SrcSurfWidth = srcPic->size().width();
        m_blt_inf.SrcSurfHeight = srcPic->size().height(); 
        m_blt_inf.SrcStride = srcPic->stride();
        m_blt_inf.SrcFormat = srcPic->pvr2DFormat();
        m_blt_inf.SizeX = m_blt_inf.DSizeX;
        m_blt_inf.SizeY = m_blt_inf.DSizeY;
        m_blt_inf.SrcX = rc.x();
        m_blt_inf.SrcY = rc.y();

        if(!m_blt_inf.pDstMemInfo || !m_blt_inf.pSrcMemInfo) {
            return;
        }
        if((err = PVR2DBlt(m_scrn->deviceContext(), &m_blt_inf)) != PVR2D_OK) {
            qWarning("drawBackingStore: PVR2DBlt failed: error(%d),     \
            CopyCode(%lu), BlitFlags(0x%x),     \
            DstSurfWidth(%lu), DstSurfHeight(%lu), DstStride(%ld), DstFormat(0x%x),  \
            DSizeX(%ld), DSizeY(%ld), DstX(%ld), DstY(%ld),     \
            SrcSurfWidth(%lu), SrcSurfHeight(%lu), SrcStride(%ld), SrcFormat(0x%x),  \
            SizeX(%ld), SizeY(%ld), SrcX(%ld), SrcY(%ld)",
            err, 
            m_blt_inf.CopyCode, static_cast<uint>(m_blt_inf.BlitFlags), 
            m_blt_inf.DstSurfWidth, m_blt_inf.DstSurfHeight, m_blt_inf.DstStride, static_cast<uint>(m_blt_inf.DstFormat), 
            m_blt_inf.DSizeX, m_blt_inf.DSizeY, m_blt_inf.DstX, m_blt_inf.DstY, 
            m_blt_inf.SrcSurfWidth, m_blt_inf.SrcSurfHeight, m_blt_inf.SrcStride, static_cast<uint>(m_blt_inf.SrcFormat), 
            m_blt_inf.SizeX, m_blt_inf.SizeY, m_blt_inf.SrcX, m_blt_inf.SrcY);
        }      
#   ifdef DEBUG_GPU_BLT_INF_BACKINGSTORE
        qDebug("bltinf: srcmeminf(%p), srcsurfw(%d), srcsufh(%d), srcstride(%d), srcfmt(%d), srcSX(%d), srcSY(%d), srcX(%d), srcY(%d)",
            m_blt_inf.pSrcMemInfo->pBase, m_blt_inf.SrcSurfWidth, m_blt_inf.SrcSurfHeight, m_blt_inf.SrcStride, m_blt_inf.SrcFormat, m_blt_inf.SizeX, m_blt_inf.SizeY, m_blt_inf.SrcX, m_blt_inf.SrcY);
        qDebug("bltinf: dstmeminf(%p), dstsurfw(%d), dstsufh(%d), dststride(%d), dstfmt(%d), dstSX(%d), dstSY(%d), dstX(%d), dstY(%d)",
            m_blt_inf.pDstMemInfo->pBase, m_blt_inf.DstSurfWidth, m_blt_inf.DstSurfHeight, m_blt_inf.DstStride, m_blt_inf.DstFormat, m_blt_inf.DSizeX, m_blt_inf.DSizeY, m_blt_inf.DstX, m_blt_inf.DstY);
#   endif  //DEBUG_GPU_BLT_INF_BACKINGSTORE
#else
        for(int m = intersect.top(); m < intersect.top() + intersect.height(); m++) {
            for(int n = intersect.left(); n < intersect.left() + intersect.width(); n ++) {
                 dstBits[m * dstImg->width() + n] = srcBits[m * srcImg->width() + n];
             }
         }
#endif  // DEBUG_CPU_BLT

#ifdef DEBUG_PIXEL_BACKINGSTORE
           for(int m = intersect.top(); m < intersect.top() + intersect.height(); m++) {
            for(int n = intersect.left(); n < intersect.left() + intersect.width(); n ++) {
                    if(!(srcBits[m * srcImg->width() + n] == 0xFF9F9DEF || srcBits[m * srcImg->width() + n] == 0xFFEFEBE7)) {
                        qDebug("src bits pixel abnormal: 0x%x", srcBits[m * srcImg->width() + n]);
                    }
                    if(!(dstBits[m * srcImg->width() + n] == 0xFF9F9DEF || dstBits[m * srcImg->width() + n] == 0xFFEFEBE7)) {
                        qDebug("dst bits pixel abnormal: 0x%x", srcBits[m * srcImg->width() + n]);
                    }
                }
            }
        qDebug("1 pixel: SRC argb(0x%x); DST argb(0x%x)", 
            srcBits[(intersect.top() + intersect.height() / 2) * srcImg->width() + intersect.left() + intersect.width() / 2],
            dstBits[(intersect.top() + intersect.height() / 2) * srcImg->width() + intersect.left() + intersect.width() / 2]);
#endif  //DEBUG_PIXEL_BACKINGSTORE
    }   
    
    return dirty_in_bs;
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
    dirty_in_wf: WF coordinate
*/
//#define DEBUG_GPU_BLT_INF_WAVEFORM    1
void QMrOfsCompositor::visitWaveform(QMrOfsWaveform* wf)
{
    //do draw wfs {{{
    //setup compositing clip region
    if(!wf)
        return;
        
    const QRegion &dirty_in_wf = wf->effectiveDirtyRegion();        // WF coordinates
    QMrOfsShmImage* srcPic = static_cast<QMrOfsShmImage*>(wf->platformImage());

    PVR2DERROR err = PVR2D_OK;        
    QMrOfsShmImage *dstPic = m_scrn->picture();       // m_fb_shadow_pict   OPT
    QVector<QRect> rects = dirty_in_wf.rects();

#if defined(DEBUG_GPU_BLT) && defined(BATCH_BLT)
        PVR2DRECT pvrRects[64];
        int real_num = rects.size() > 64 ? 64 : rects.size();
        for(int i = 0; i < real_num; i++) {
            const QRect& rc = rects.at(i);
            pvrRects[i].left = rc.left() + wf->originInTLW().x();
            pvrRects[i].top = rc.top() + wf->originInTLW().y();
            pvrRects[i].right = rc.right();
            pvrRects[i].bottom = rc.bottom();
        }
        m_blt_inf.CopyCode = PVR2DROPcopy;
        m_blt_inf.BlitFlags = PVR2D_BLIT_PERPIXEL_ALPHABLEND_ENABLE;
        m_blt_inf.AlphaBlendingFunc = PVR2D_ALPHA_OP_SRCP_DSTINV;
        m_blt_inf.pDstMemInfo = dstPic->pvr2DMem();
        m_blt_inf.DstSurfWidth = dstPic->size().width();
        m_blt_inf.DstSurfHeight = dstPic->size().height();
        m_blt_inf.DstStride = dstPic->stride();
        m_blt_inf.DstFormat = dstPic->pvr2DFormat();
        m_blt_inf.DSizeX = m_blt_inf.DstSurfWidth;
        m_blt_inf.DSizeY = m_blt_inf.DstSurfHeight;
        m_blt_inf.DstX = 0 + wf->originInTLW().x();       // tlw coordinate
        m_blt_inf.DstY = 0 + wf->originInTLW().y();

        m_blt_inf.pSrcMemInfo = srcPic->pvr2DMem();
        m_blt_inf.SrcSurfWidth = srcPic->size().width();	
        m_blt_inf.SrcSurfHeight = srcPic->size().height();	
        m_blt_inf.SrcStride = srcPic->stride();
        m_blt_inf.SrcFormat = srcPic->pvr2DFormat();
        m_blt_inf.SizeX = m_blt_inf.SrcSurfWidth;
        m_blt_inf.SizeY = m_blt_inf.SrcSurfHeight;
        m_blt_inf.SrcX = 0;                               // wf coordinate
        m_blt_inf.SrcY = 0;  

        if(!m_blt_inf.pDstMemInfo || !m_blt_inf.pSrcMemInfo) {
            return;
        }
        if((err = PVR2DBltClipped(m_scrn->deviceContext(), &m_blt_inf, real_num, pvrRects)) != PVR2D_OK) {
            qWarning("visitWaveform: PVR2DBltClipped failed: error(%d)", err);
        }
        return;
#endif

    
    for(int i = 0; i < rects.size(); i ++) {
        const QRect& rc = rects.at(i);
#if defined(DEBUG_CPU_BLT) || defined(DEBUG_PIXEL_WAVEFORM)
        QImage *srcImg = srcPic->image();
        unsigned int *srcBits = (unsigned int*)(srcImg->bits());
        QImage *dstImg = dstPic->image();
        unsigned int *dstBits = (unsigned int*)(dstImg->bits());
        QRect rcInTLW = rc.translated(wf->originInTLW());   // tlw coordinate
        QRect intersect = rcInTLW.intersected(QRect(0, 0, dstImg->width(), dstImg->height()));  // tlw coordinate
        intersect = intersect.intersected(QRect(wf->originInTLW().x(), wf->originInTLW().y(), srcImg->width(), srcImg->height()));
#endif
 
#ifdef DEBUG_GPU_BLT
        m_blt_inf.CopyCode = PVR2DROPcopy;
        m_blt_inf.BlitFlags = PVR2D_BLIT_PERPIXEL_ALPHABLEND_ENABLE;
        m_blt_inf.AlphaBlendingFunc = PVR2D_ALPHA_OP_SRCP_DSTINV;		// src over dst, with premultiplied-alpha
        m_blt_inf.pDstMemInfo = dstPic->pvr2DMem();
        m_blt_inf.DstSurfWidth = dstPic->size().width();
        m_blt_inf.DstSurfHeight = dstPic->size().height();
        m_blt_inf.DstStride = dstPic->stride();
        m_blt_inf.DstFormat = dstPic->pvr2DFormat();
        m_blt_inf.DSizeX = rc.width();
        m_blt_inf.DSizeY = rc.height();
        m_blt_inf.DstX = rc.x() + wf->originInTLW().x();       // tlw coordinate
        m_blt_inf.DstY = rc.y() + wf->originInTLW().y();

        m_blt_inf.pSrcMemInfo = srcPic->pvr2DMem();
        m_blt_inf.SrcSurfWidth = srcPic->size().width();	
        m_blt_inf.SrcSurfHeight = srcPic->size().height();	
        m_blt_inf.SrcStride = srcPic->stride();
        m_blt_inf.SrcFormat = srcPic->pvr2DFormat();
        m_blt_inf.SizeX = rc.width();
        m_blt_inf.SizeY = rc.height();
        m_blt_inf.SrcX = rc.x();                               // wf coordinate
        m_blt_inf.SrcY = rc.y();

        if(!m_blt_inf.pDstMemInfo || !m_blt_inf.pSrcMemInfo) {
            return;
        }
        if((err = PVR2DBlt(m_scrn->deviceContext(), &m_blt_inf)) != PVR2D_OK) {
            qWarning("visitWaveform: PVR2DBlt failed: error(%d),     \
            CopyCode(%lu), BlitFlags(0x%x),     \
            DstSurfWidth(%lu), DstSurfHeight(%lu), DstStride(%ld), DstFormat(0x%x),  \
            DSizeX(%ld), DSizeY(%ld), DstX(%ld), DstY(%ld),     \
            SrcSurfWidth(%lu), SrcSurfHeight(%lu), SrcStride(%ld), SrcFormat(0x%x),  \
            SizeX(%ld), SizeY(%ld), SrcX(%ld), SrcY(%ld)",
            err, 
            m_blt_inf.CopyCode, static_cast<uint>(m_blt_inf.BlitFlags), 
            m_blt_inf.DstSurfWidth, m_blt_inf.DstSurfHeight, m_blt_inf.DstStride, static_cast<uint>(m_blt_inf.DstFormat), 
            m_blt_inf.DSizeX, m_blt_inf.DSizeY, m_blt_inf.DstX, m_blt_inf.DstY, 
            m_blt_inf.SrcSurfWidth, m_blt_inf.SrcSurfHeight, m_blt_inf.SrcStride, static_cast<uint>(m_blt_inf.SrcFormat), 
            m_blt_inf.SizeX, m_blt_inf.SizeY, m_blt_inf.SrcX, m_blt_inf.SrcY);
        }
		    
#   ifdef DEBUG_GPU_BLT_INF_WAVEFORM
        qDebug("bltinf: srcmeminf(%p), srcsurfw(%d), srcsufh(%d), srcstride(%d), srcfmt(%d), srcSX(%d), srcSY(%d), srcX(%d), srcY(%d)",
            m_blt_inf.pSrcMemInfo->pBase, m_blt_inf.SrcSurfWidth, m_blt_inf.SrcSurfHeight, m_blt_inf.SrcStride, m_blt_inf.SrcFormat, m_blt_inf.SizeX, m_blt_inf.SizeY, m_blt_inf.SrcX, m_blt_inf.SrcY);
        qDebug("bltinf: dstmeminf(%p), dstsurfw(%d), dstsufh(%d), dststride(%d), dstfmt(%d), dstSX(%d), dstSY(%d), dstX(%d), dstY(%d)",
            m_blt_inf.pDstMemInfo->pBase, m_blt_inf.DstSurfWidth, m_blt_inf.DstSurfHeight, m_blt_inf.DstStride, m_blt_inf.DstFormat, m_blt_inf.DSizeX, m_blt_inf.DSizeY, m_blt_inf.DstX, m_blt_inf.DstY);
#   endif  //DEBUG_GPU_BLT_INF_WAVEFORM
#else
        for(int m = intersect.top(); m < intersect.top() + intersect.height(); m++) {
         for(int n = intersect.left(); n < intersect.left() + intersect.width(); n ++) {
                 int mInWF = m - wf->originInTLW().y();
                 int nInWF = n - wf->originInTLW().x();
                uint s = srcBits[mInWF * srcImg->width() + nInWF];
                if (s >= 0xff000000)
                    dstBits[m * dstImg->width() + n] = s;
                else if (s != 0)
                    dstBits[m * dstImg->width() + n] = s + BYTE_MUL(dstBits[m * dstImg->width() + n], qAlpha(~s));
             }
         }
#endif  // DEBUG_CPU_BLT
    }

    // }}}
    //anything else we can do ?
}

/*!
    \note
    IVIEW use SRC, not OVER
    NOTE: waiting finished here maybe unnecessary since iview has multiple buffers in mipi-csi
*/
void QMrOfsCompositor::visitIViewform(QMrOfsWaveform* wf)
{
    Q_UNUSED(wf);
}

/*!
    old position has already been re-composited during drawBackingStore and drawWaveforms
    curRect: TLW coordinate
    rcInCursor: CURSOR coordinate
*/
QRect QMrOfsCompositor::drawCursor(const QRegion& composeRgn)
{
    if(!m_cursor) {
        qWarning("drawCursor: m_cursor is NULL");
        return QRect();
    }

    QRect curRect = m_cursor->drawCursor();     //new position, TLW coordinate

    if(curRect.isEmpty()) {
        qWarning("drawCursor: curRect isEmpty or NOT in composite region");
        return QRect();
    }

    QRect rcInCursor(0, 0, curRect.width(), curRect.height());      // cursor image coordinate
    QMrOfsShmImage *srcPic = m_cursor->picture();

    PVR2DERROR err = PVR2D_OK;
    QMrOfsShmImage *dstPic = m_scrn->picture();       // m_fb_shadow_pict   OPT
#if defined(DEBUG_CPU_BLT) || defined(DEBUG_PIXEL_CURSOR)
        QImage *srcImg = srcPic->image();
        unsigned int *srcBits = (unsigned int*)(srcImg->bits());
        QImage *dstImg = dstPic->image();
        unsigned int *dstBits = (unsigned int*)(dstImg->bits());
    QRect intersect = curRect.intersected(QRect(0, 0, dstImg->width(), dstImg->height()));
#endif

#ifdef DEBUG_GPU_BLT
    m_blt_inf.CopyCode = PVR2DROPcopy;
    m_blt_inf.BlitFlags = PVR2D_BLIT_PERPIXEL_ALPHABLEND_ENABLE; 
    m_blt_inf.AlphaBlendingFunc = PVR2D_ALPHA_OP_SRCP_DSTINV;           // PVR2D_ALPHA_OP_SRC_DSTINV
    
    m_blt_inf.pDstMemInfo = dstPic->pvr2DMem();        
    m_blt_inf.DstSurfWidth = dstPic->size().width(); 
    m_blt_inf.DstSurfHeight = dstPic->size().height();       
    m_blt_inf.DstStride = dstPic->stride();           
    m_blt_inf.DstFormat = dstPic->pvr2DFormat();
    m_blt_inf.DSizeX = rcInCursor.width();
    m_blt_inf.DSizeY = rcInCursor.height();
    m_blt_inf.DstX = curRect.x();
    m_blt_inf.DstY = curRect.y();
    
    m_blt_inf.pSrcMemInfo = srcPic->pvr2DMem();
    m_blt_inf.SrcSurfWidth = srcPic->size().width();    
    m_blt_inf.SrcSurfHeight = srcPic->size().height();  
    m_blt_inf.SrcStride = srcPic->stride();
    m_blt_inf.SrcFormat = srcPic->pvr2DFormat();
    m_blt_inf.SizeX = rcInCursor.width();
    m_blt_inf.SizeY = rcInCursor.height();
    m_blt_inf.SrcX = 0;
    m_blt_inf.SrcY = 0;

    if(!m_blt_inf.pDstMemInfo || !m_blt_inf.pSrcMemInfo) {
        return;
    }

    if((err = PVR2DBlt(m_scrn->deviceContext(), &m_blt_inf)) != PVR2D_OK) {
        qWarning("drawCursor: PVR2DBlt failed: error(%d),     \
        CopyCode(%lu), BlitFlags(0x%x),     \
        DstSurfWidth(%lu), DstSurfHeight(%lu), DstStride(%ld), DstFormat(0x%x),  \
        DSizeX(%ld), DSizeY(%ld), DstX(%ld), DstY(%ld),     \
        SrcSurfWidth(%lu), SrcSurfHeight(%lu), SrcStride(%ld), SrcFormat(0x%x),  \
        SizeX(%ld), SizeY(%ld), SrcX(%ld), SrcY(%ld)",
        err, 
        m_blt_inf.CopyCode, static_cast<uint>(m_blt_inf.BlitFlags), 
        m_blt_inf.DstSurfWidth, m_blt_inf.DstSurfHeight, m_blt_inf.DstStride, static_cast<uint>(m_blt_inf.DstFormat), 
        m_blt_inf.DSizeX, m_blt_inf.DSizeY, m_blt_inf.DstX, m_blt_inf.DstY, 
        m_blt_inf.SrcSurfWidth, m_blt_inf.SrcSurfHeight, m_blt_inf.SrcStride, static_cast<uint>(m_blt_inf.SrcFormat), 
        m_blt_inf.SizeX, m_blt_inf.SizeY, m_blt_inf.SrcX, m_blt_inf.SrcY);

    }
#else
    QPainter p(dstPic->image());
    qDebug("drawCursor: curRect(%d,%d,%d,%d), srcPic(%d,%d)",
        curRect.x(), curRect.y(), curRect.width(), curRect.height(),
        srcPic->image()->size().width(), srcPic->image()->size().height());
    p.drawImage(curRect, *srcPic->image());
#if 0    
    for(int m = intersect.top(); m < intersect.top() + intersect.height(); m++) {
     for(int n = intersect.left(); n < intersect.left() + intersect.width(); n ++) {
             int mInCursor = m - intersect.top();
             int nInCursor = n - intersect.left();
            uint s = srcBits[mInCursor * srcImg->width() + nInCursor];
            if (s >= 0xff000000)
                dstBits[m * dstImg->width() + n] = s;
            else if (s != 0)
                dstBits[m * dstImg->width() + n] = s + BYTE_MUL(dstBits[m * dstImg->width() + n], qAlpha(~s));
         }
     }
#endif     
#endif  // DEBUG_CPU_BLT

    return curRect;
}

/*
    Region: compositeRegion + cursorRegion(new position) in TLW coordinate
    OP:    OP_SRC
    Src:   m_fb_shadow_pict
    Dst:   fb
*/
void QMrOfsCompositor::sync(const QRegion& compositeRgn, const QRegion& curRgn)
{
    // hope the execution sequence of GPU is FIFO, which means waiting for the last one to complete is just fine
    PVR2DQueryBlitsComplete(m_scrn->deviceContext(), m_blt_inf.pDstMemInfo, 1);
    m_screen_dirty_rgn = compositeRgn + curRgn;                  // src/dst: TLW coordinate
    m_screen_dirty_rgn &= m_scrn->geometry();
    
#if QT_MRDP
    m_mrdp->tryNotifyScreenUpdated(m_screen_dirty_rgn);
#endif

#if 0
    QVector<QRect> rects = m_screen_dirty_rgn.rects();
    int round = rects.size() / BATCH_RC_MAX_NUM;    // total_round = round + remainder(0 or 1)
    int r = 0;              // rounding index
    int j = 0;              // batch buffer index
    // max num batch    
    for (r = 0; r < round; r ++) {
        for (int i = r * BATCH_RC_MAX_NUM; i < (r + 1) * BATCH_RC_MAX_NUM; i ++) {
            const QRect &rc = rects[i];
            j = i - (r * BATCH_RC_MAX_NUM);
            m_batchRcs[j].left = rc.x();
            m_batchRcs[j].top = rc.y();
            m_batchRcs[j].right = rc.x() + rc.width();
            m_batchRcs[j].bottom = rc.y() + rc.height();
        }
        for (int i = r * BATCH_RC_MAX_NUM; i < (r + 1) * BATCH_RC_MAX_NUM; i += 4) {
            int numClip = (r + 1) * BATCH_RC_MAX_NUM - i;
            j = i - (r * BATCH_RC_MAX_NUM);
            if (numClip > 4)            /* No more than 4 clip rects at a time */
                numClip = 4;
            PVR2DSetPresentBltProperties(m_scrn->deviceContext(),
                 PVR2D_PRESENT_PROPERTY_SRCSTRIDE |
                 PVR2D_PRESENT_PROPERTY_DSTSIZE |
                 PVR2D_PRESENT_PROPERTY_DSTPOS |
                 PVR2D_PRESENT_PROPERTY_CLIPRECTS,
                 m_fb_shadow_pict->stride(),
                 m_batchRcs[j].right - m_batchRcs[j].left, 
                 m_batchRcs[j].top - m_batchRcs[j].bottom, 
                 m_batchRcs[j].left, 
                 m_batchRcs[j].top,       // dst: TLW coordinate
                 numClip, 
                 m_batchRcs + j,          // src: TLW coordinate
                 0);
            PVR2DPresentBlt(m_scrn->deviceContext(), m_fb_shadow_pict->pvr2DMem(), 0);       // m_fb_shadow_pict -> fb
        }       
    }
    
    //remainder batch
    for (int i = r * BATCH_RC_MAX_NUM; i < rects.size(); i ++) {
        const QRect &rc = rects[i];
        j = i - (r * BATCH_RC_MAX_NUM);
        m_batchRcs[j].left = rc.x();
        m_batchRcs[j].top = rc.y();
        m_batchRcs[j].right = rc.x() + rc.width();
        m_batchRcs[j].bottom = rc.y() + rc.height();
    }
    for (int i = r * BATCH_RC_MAX_NUM; i < rects.size(); i += 4) {
        j = i - (r * BATCH_RC_MAX_NUM);
        int numClip = rects.size() - i;
        if (numClip > 4)                /* No more than 4 clip rects at a time */
            numClip = 4;
        PVR2DSetPresentBltProperties(m_scrn->deviceContext(),
             PVR2D_PRESENT_PROPERTY_SRCSTRIDE |
             PVR2D_PRESENT_PROPERTY_DSTSIZE |
             PVR2D_PRESENT_PROPERTY_DSTPOS |
             PVR2D_PRESENT_PROPERTY_CLIPRECTS,
             m_fb_shadow_pict->stride(),
             m_batchRcs[j].right - m_batchRcs[j].left, 
             m_batchRcs[j].top - m_batchRcs[j].bottom, 
             m_batchRcs[j].left, 
             m_batchRcs[j].top,       // dst: TLW coordinate
             numClip, 
             m_batchRcs + j,          // src: TLW coordinate
             0);
        PVR2DPresentBlt(m_scrn->deviceContext(), m_fb_shadow_pict->pvr2DMem(), 0);       // m_fb_shadow_pict -> fb
    }
    PVR2DQueryBlitsComplete(m_scrn->deviceContext(), m_fb_shadow_pict->pvr2DMem(), 1);
#endif
}

/*!
    everytime ui changes, or visible waveform changes, re-composie them all
    \note
    1, ����ǽ��� ���´����ĺϳ�, ��
    1.1 ����û��������
    1.2 ������������
    2, ����ǲ��θ��д����ĺϳɣ���
    2.1 ����û��������
    2.2 ������������
*/
void QMrOfsCompositor::composite(Reason reason)
{
#if defined(MROFS__PROFILING)
    static int frames;
    static struct timeval lastTime;
    struct timeval startTime, endTime;
    unsigned long diffTime;
    gettimeofday(&startTime, NULL);    
    //qWarning("MROFS__PROFILING: composite before: %ld:%ld", startTime.tv_sec, startTime.tv_usec);
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
    } else if(TRIGGERED_BY_CURSOR == reason) {
        lockBackingStoreResources();
        lockWaveResources();
        lockIViewResources();        
    }

#ifdef QT_MRDP
    lockScreenResources();      // in case mrdp snoop
#endif

    if(updateClipRegions()) {             //update in use
        const QRegion &compositeRgn = drawBackingStore();   //layer 0(topleast)
        drawWaveforms();                                    //layer 1(maybe more than 1 sub layers: Nx waveforms + 1x iviewform)
        QRect curRc;
        if(m_cursor && (m_cursor->isDirty() || compositeRgn.intersects(m_cursor->lastPainted()))) {
            curRc = drawCursor(compositeRgn);               //layer 2(topmost)
        }
        sync(compositeRgn, curRc);                          //wait until fb buffer switched
    }
    cleanClipRegions();                   //clean bs+wv dirty regions processed this time

#ifdef QT_MRDP
    unlockScreenResources();        // in case of mrdp snoop
#endif
    
    if (TRIGGERED_BY_BACKINGSTORE == reason) {
        unlockWaveResources();
        unlockIViewResources();
    } else if (TRIGGERED_BY_WAVEFORMS == reason) {
        unlockBackingStoreResources();
        unlockIViewResources();
    } else if (TRIGGERED_BY_IVIEWFORMS == reason) {
        unlockWaveResources();
        unlockBackingStoreResources();
    } else if(TRIGGERED_BY_CURSOR == reason) {
        unlockBackingStoreResources();
        unlockWaveResources();
        unlockIViewResources();        
    }

#if defined(MROFS__PROFILING)
    gettimeofday(&endTime, NULL);
    diffTime = tv_diff(&startTime, &endTime);
    //qWarning("MROFS__PROFILING: composite after: %ld:%ld, diff: %ld", endTime.tv_sec, endTime.tv_usec, diffTime);

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
    const QRegion &bs_clip = m_bs->effectiveClip();
    QRect old_cursor_rgn;

    if (m_cursor && m_cursor->isDirty() && m_cursor->isOnScreen()) {
        //translate curRect from screen coordinate to tlw coordinate in which case tlw is always in primary screen coordinate    
        old_cursor_rgn = m_cursor->dirtyRect();
    }

    return m_wc->updateEffectiveClips(bs_clip, old_cursor_rgn);
}

/*!
    �ѱ��κϳ��Ѵ��������clip region ���
*/
void QMrOfsCompositor::cleanClipRegions()
{
    //clean bs clip region
    m_bs->cleanEffectiveClip();
    
    //clean wv clip region
    m_wc->cleanEffectiveClips();           //clean wfs clip region and composite region
}

void QMrOfsCompositor::setCursor(QMrOfsCursor *cursor)
{
    int at = this->metaObject()->indexOfMethod("setCursor2(QMrOfsCursor*)");
    if (at == -1) {
        qWarning("QMrOfsCompositor::setCursor2: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->m_conn_type, Q_ARG(QMrOfsCursor*, cursor));
}

void QMrOfsCompositor::setCursor2(QMrOfsCursor *cursor)
{
    m_cursor = cursor;
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
    if(m_cursor) {
        m_cursor->doPointerEvent(e);
    } else {
        qWarning("QMrOfsCompositor::pointerEvent2: no cursor found");
    }
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
    if(m_cursor) {
        m_cursor->doChangeCursor(widgetCursor, window);
    } else {
        qWarning("QMrOfsCompositor::changeCursor2: no cursor found");
    }
}

/*!
    [compositor thread]
    cursor updating reason is the same as backingstore
*/
void QMrOfsCompositor::flushCursor()
{    
    if(!m_cr_flush_enabled)
        return;    
        
    composite(TRIGGERED_BY_CURSOR);
}

QRect QMrOfsCompositor::initialize(QMrOfsScreen *scrn)
{
    Q_UNUSED(scrn);
    // TODO: initialize compositor after screen initialized if want a in-screen compositor
}

QRect QMrOfsCompositor::initialize2(QMrOfsScreen *scrn)
{
    Q_UNUSED(scrn);
    // TODO: in-screen compositor
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

#ifdef QT_MRDP
Q_INVOKABLE bool QMrOfsCompositor::startMRDPService()
{
    if(!m_mrdp) {
        qWarning("MRDP Connection Manager not exist: %s", __FUNCTION__);
    }

    m_mrdp->startService(this); // now mrdp service can take advantage of me
}

Q_INVOKABLE void QMrOfsCompositor::stopMRDPService()
{
    if(!m_mrdp) {
        qWarning("MRDP Connection Manager not exist: %s", __FUNCTION__);
    }

    m_mrdp->stopService();
}

Q_INVOKABLE bool QMrOfsCompositor::MRDPServiceRunning()
{
    if(!m_mrdp) {
        qWarning("MRDP Connection Manager not exist: %s", __FUNCTION__);
    }

    return m_mrdp->serviceRunning();
}

const QRegion& QMrOfsCompositor::screenDirtyRegion()
{
    return m_screen_dirty_rgn;
}

const QImage& QMrOfsCompositor::screenImage()
{
    QMrOfsShmImage *scrnPic = m_scrn->picture();    // scrnPic as src
    if(!scrnPic) {
        qWarning("screen picture is NULL :%s", __FUNCTION__);
        return;
    }
    
    return scrnPic->image() ? *(scrnPic->image()) : QPlatformCompositorBase::screenImage();
}

/*!
    lockScreenResources ������: ��֤��mrdp snoop screen(by CPU) ʱ, 
    1, screen dirty region ������д(by CPU in compositor thread)
    2, screen dirty pixels ������д(by GPU in compositor thread)
*/
void QMrOfsCompositor::lockScreenResources()
{
    m_mutex_screen.lock();
}

void QMrOfsCompositor::unlockScreenResources()
{   
    m_mutex_screen.unlock();
}
#endif //QT_MRDP

QT_END_NAMESPACE


