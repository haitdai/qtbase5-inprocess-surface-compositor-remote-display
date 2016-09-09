#ifndef QMROFS_COMPOSITOR_H
#define QMROFS_COMPOSITOR_H

//#include "qmrofsdefs.h"
#include "qmrofswindow.h"
#include "qmrofsbackingstore.h"
#include <QtPlatformSupport/private/qplatformcompositor_p.h>        //#include <QtWidgets/qplatformcompositor.h>
#include <QtCore/QThread>
#include <QtCore/QMutex>

#ifdef QT_MRDP
#include  <QtPlatformSupport/private/mrdp_p.h>
#endif //QT_MRDP

class QMrOfsScreen;
class QMrOfsWaveform;
class QMrOfsWaveController;
class QMrOfsBackingStore;
class QMrOfsCursor;
class QMrOfsCompositor;

QT_BEGIN_NAMESPACE

class QMrOfsCompositorThread : public QThread
{
    Q_OBJECT
        
public:
    QMrOfsCompositorThread(QObject* parent = NULL, QMrOfsCompositor *cmptr = NULL);
    ~QMrOfsCompositorThread();
protected:
    virtual void run();    
private:
    QMrOfsCompositor  *compositor;
    int priority;
    pthread_t  id;

    friend class QMrOfsCompositor;
};

/*!
    \brief TLW �ڲ��ĺϳ���
    \note
    ��V1(QMrOfsCompositor) �У���ʵ���󶨵�screen����ͬʱʵ������ֻ֧��win�ڵĺϳɣ���Ȼ������1��screen��ֻ����1��win���ڵļ�鱣֤��
    ���е���ȷ������������Ͽ���������֡�
    ��(QMrOfsCompositor)�У�ְ�𲻱�(win�ںϳ�)��ͬʱ��ʵ���󶨵�win������1��screenֻ����1���ɼ�win���ڵļ�顣
*/
class QMrOfsCompositor : public QPlatformCompositorBase
{
    Q_OBJECT
 
public:
    /*!
        \note a screen - a compositor - a process (qt instance)
        win, bs, screen��Ϣ����ͨ��win���
    */
    QMrOfsCompositor(QMrOfsWindow *win, QMrOfsBackingStore* bs);    
    ~QMrOfsCompositor();
    
public:
    //QMrOfs legacy interface(in qtgui) {{{
    //ui thread -> compositor thread, NOT IMPLEMENTED
    Q_INVOKABLE QAbstractMrOfs::WV_ID openWaveview();
    //ui thread -> compositor thread
    Q_INVOKABLE void closeWaveview(QAbstractMrOfs::WV_ID vid);
    //ui thread -> compositor thread, NOT IMPLEMENTED
    Q_INVOKABLE QAbstractMrOfs::WV_ID setWaveviewLayer(QAbstractMrOfs::WV_ID vid, int layer);
    //ui thread -> compositor thread    
    Q_INVOKABLE void setWaveViewport(QAbstractMrOfs::WV_ID vid, const QRect& vpt);
    //ui thread -> compositor thread, NOT IMPLEMENTED    
    Q_INVOKABLE QAbstractMrOfs::WV_ID createWaveform(QAbstractMrOfs::WV_ID vid);
    //ui thread -> compositor thread    
    Q_INVOKABLE void destroyWaveform(QAbstractMrOfs::WV_ID wid);
    //ui thread -> compositor thread    
    Q_INVOKABLE QWaveData* setWaveformSize(QAbstractMrOfs::WV_ID wid, const QSize& sz);
    //ui thread -> compositor thread, NOT IMPLEMENTED    
    Q_INVOKABLE void bindWaveform(QAbstractMrOfs::WV_ID wid, QAbstractMrOfs::WV_ID vid);
    //wv thread -> compositor thread    
    Q_INVOKABLE QWaveData* waveformRenderBuffer(QAbstractMrOfs::WV_ID wid);
    //ui thread -> compositor thread, NOT IMPLEMENTED        
    Q_INVOKABLE QPoint setWaveformPos(QAbstractMrOfs::WV_ID wid, const QPoint& pos);
    //wv thread -> compositor thread, sync, called after unlockWaveResources, before clean dirty region
    //flushWaves ��flushBackingStore ��������ȫһ��������ֻ����wv ������ĺϳ�    
    Q_INVOKABLE void flushWaves(const QRegion& rgn);
    //wv/ui thread -> compositor thread, NOT IMPLEMENTED        
    Q_INVOKABLE QRgb getColorKey() const;
    //wv/ui thread -> compositor thread
    Q_INVOKABLE bool enableComposition(bool enable);
    //wv thread -> compositor thread, protected wv_widget_map      [dirty patch !!!]
    Q_INVOKABLE void lockWaveController();
    //wv thread -> compositor thread, protected wv_widget_map      [dirty patch !!!]
    Q_INVOKABLE void unlockWaveController();
    //}}}
    
    //QMrOfs interface(in qtwidgets) {{{
    //ui thread -> compositor thread
    Q_INVOKABLE QAbstractMrOfs::WV_ID openWaveview(const QWidget& container, const QRect& vpt);
    //ui thread -> compositor thread    
    Q_INVOKABLE QAbstractMrOfs::WV_ID createWaveform(QAbstractMrOfs::WV_ID vid, const QRect& rc);
    //ui thread -> compositor thread, NOT IMPLEMENTED
    Q_INVOKABLE void unbindWaveform(QAbstractMrOfs::WV_ID wid, QAbstractMrOfs::WV_ID vid);
    //ui thread -> compositor thread    
    Q_INVOKABLE void openPopup(const QWidget& popup);
    //ui thread -> compositor thread    
    Q_INVOKABLE void closePopup(const QWidget& popup);
    //wave thread    
    Q_INVOKABLE void addWaveformClipRegion(QAbstractMrOfs::WV_ID wid, const QRegion &rgn);
    Q_INVOKABLE void addWaveformClipRegion(QAbstractMrOfs::WV_ID wid, const QRect &rc);
    //wave thread
    Q_INVOKABLE void cleanWaveformClipRegion(QAbstractMrOfs::WV_ID wid);
    //wave thread
    Q_INVOKABLE void cleanAllWaveformClipRegion();
    //ui thread -> compositor thread
    Q_INVOKABLE int setCompositorThreadPriority(int newpri);
    //wave/compositor thread
    Q_INVOKABLE void lockWaveResources();
    //wave/compositor thread
    Q_INVOKABLE void unlockWaveResources(); 
    //wave thread -> compositor thread
    Q_INVOKABLE void flushWaves();
    //wave thread
    Q_INVOKABLE void submitWaveData();
    //ui thread -> compositor thread    
    Q_INVOKABLE QAbstractMrOfs::WV_ID openIView(const QWidget &container, const QRect& vpt);
    //ui thread -> compositor thread    
    Q_INVOKABLE void closeIView(QAbstractMrOfs::WV_ID vid);        
    //ui thread -> compositor thread    
    Q_INVOKABLE QAbstractMrOfs::WV_ID createIViewform(QAbstractMrOfs::WV_ID vid, const HDMI_CONFIG hcfg, QAbstractMrOfs::Format format);
    //iview thread -> compositor thread    
    Q_INVOKABLE void submitIViewData(QAbstractMrOfs::WV_ID vid, QAbstractMrOfs::WV_ID wid, const unsigned char *bits);
    //iview thread -> compositor thread
    Q_INVOKABLE void flushIView(QAbstractMrOfs::WV_ID vid);
    //iview/compositor thread
    Q_INVOKABLE void lockIViewResources();
    //iview/compositor thread
    Q_INVOKABLE void unlockIViewResources();     
    // }}}

#ifdef PAINT_WAVEFORM_IN_QT
    // interface additional [wave thread] {{{
    Q_INVOKABLE void paintWaveformLines(
        QImage* image,
        QAbstractMrOfs::XCOORDINATE xStart,
        const QAbstractMrOfs::YCOORDINATE* y,
        const QAbstractMrOfs::YCOORDINATE* yMax,
        const QAbstractMrOfs::YCOORDINATE* yMin,
        short yFillBaseLine,
        unsigned int pointsCount,
        const QColor& color,
        QAbstractMrOfs::LINEMODE mode);
    Q_INVOKABLE void eraseRect(QImage* image, const QRect& rect);
    Q_INVOKABLE void drawVerticalLine(QImage* image, 
        QAbstractMrOfs::XCOORDINATE x, 
        QAbstractMrOfs::YCOORDINATE y1, 
        QAbstractMrOfs::YCOORDINATE y2, 
        const QColor& color);

protected:
    void drawWaveformLines(QImage* image,
        QAbstractMrOfs::XCOORDINATE xStart,
        const QAbstractMrOfs::YCOORDINATE* y,
        const QAbstractMrOfs::YCOORDINATE* yMax,
        const QAbstractMrOfs::YCOORDINATE* yMin,
        unsigned int pointsCount,
        const QColor& color);
    void fillWaveformLines(QImage* image,                     
        QAbstractMrOfs::XCOORDINATE xStart,                     
        const QAbstractMrOfs::YCOORDINATE* y,                     
        const QAbstractMrOfs::YCOORDINATE* yMin,                    
        short yFillBaseLine,                    
        unsigned int pointsCount,                    
        const QColor& color);
#endif    
    // }}}
    
    //for QMrOfsBackingStore calling {{{
public:
    //ui/compositor thread
    virtual void lockBackingStoreResources();
    //ui/compositor thread
    virtual void unlockBackingStoreResources();
    //ui thread -> compositor thread, sync, called after unlockBackingStoreWaveResources, before clean dirty region
    //flushBackingStore ��flushWaves ��������ȫһ��������ֻ����bs ������ĺϳ�
    //ע�⣬ui ��֧�ֶ��backingstore���κ�һ����flush ��������bs �ϳ�(�����㲻��)
    void flushBackingStore(const QRegion& dirty = QRegion());
protected:
    Q_INVOKABLE void flushBackingStore2(const QRegion& dirty);
public:
    //ui thread -> compositor thread
    void resizeBackingStore(QMrOfsBackingStore *bs, const QSize &size, const QRegion &rgn);
protected:    
    Q_INVOKABLE void resizeBackingStore2(QMrOfsBackingStore *bs, const QSize &size, const QRegion &rgn);   
    //}}}

    //QMrOfsWaveController callback {{{
public:
    void visitWaveform(QMrOfsWaveform* wf);           //worker (anything else to visit: waveviews, containerwidgets ?)
    void visitIViewform(QMrOfsWaveform* wf);
    // }}}

    //screen calling [ui thread -> compositor thread] {{{
public:    
    QRect initialize(QMrOfsScreen *scrn);                        // plugin loading
protected:    
    Q_INVOKABLE QRect initialize2(QMrOfsScreen *scrn);
    //}}}

    // cursor calling [ui/inputmgr thread -> compositor thread] {{{
public:    
    void setCursor(QMrOfsCursor *cursor);
    void pointerEvent(const QMouseEvent &e);
    void changeCursor(QCursor * widgetCursor, QWindow *window);
protected:
    Q_INVOKABLE void setCursor2(QMrOfsCursor *cursor);        
    Q_INVOKABLE void pointerEvent2(const QMouseEvent &e);
    Q_INVOKABLE void changeCursor2(QCursor * widgetCursor, QWindow *window);    
public:    
    void flushCursor();
public:    
    QMrOfsWindow *tlw() {return m_win;}
    // }}}

    //flushing control - ����и�Ƶ�ʵĸ���(��iview), ��������Ƶ�ʵĸ��¿��Բ�����
    Q_INVOKABLE bool enableBSFlushing(bool enable);
    Q_INVOKABLE bool enableWVFlushing(bool enable);
    Q_INVOKABLE bool enableIVFlushing(bool enable);
    Q_INVOKABLE bool enableCRFlushing(bool enable);
    
private:
    enum Reason{
        TRIGGERED_BY_BACKINGSTORE,      // from ui thread 
        TRIGGERED_BY_WAVEFORMS,         // from wv thread
        TRIGGERED_BY_IVIEWFORMS,        // from iv thread
        TRIGGERED_BY_CURSOR,            // from inputmgr thread
    };
    void composite(Reason reason);        
    const QRegion&  drawBackingStore();             // 1 bs 
    void drawWaveforms();                 // and many waveforms
    bool updateClipRegions();
    void cleanClipRegions();
    QRect drawCursor(const QRegion& composeRgn);
    void sync(const QRegion& compositeRgn, const QRegion& curRgn);

#ifdef QT_MRDP
public:
    Q_INVOKABLE bool startMRDPService();
    Q_INVOKABLE void stopMRDPService();
    Q_INVOKABLE bool MRDPServiceRunning();
    const QRegion& screenDirtyRegion();
    const QImage& screenImage();
    void lockScreenResources();
    void unlockScreenResources();
#endif //QT_MRDP

private:
#ifdef QMROFS_THREAD_COMPOSITOR
    QMrOfsCompositorThread    *m_th;              /* apartment thread */
    QMutex                     m_mutex_bs;
    QMutex                     m_mutex_wv;
    QMutex                     m_mutex_iv;
    QMutex                     m_mutex_wc;
#endif
    QMrOfsScreen              *m_scrn;            /* screen info*/    
    QMrOfsWindow              *m_win;             /* ui geometry && compositing destination */
    QMrOfsBackingStore        *m_bs;              /* only 1 bs supported */
    QMrOfsWaveController      *m_wc;              /* everthing about waveforms */
    QMrOfsCursor              *m_cursor;          /* mouse cursor */
    
    /* misc */
    bool                       m_enabled;         /* compositing switch */    
    Qt::ConnectionType         m_conn_type;       /*ref QMrOfsPrivate.connType */    
    
    /* flushing switches */
    bool                       m_wv_flush_enabled; // wv
    bool                       m_iv_flush_enabled; // iv
    bool                       m_bs_flush_enabled; // bs
    bool                       m_cr_flush_enabled; // cursor

    /* double buffer */
    QMrOfsShmImage            *m_fb_shadow_pict;   /* screen shadow to avoid flicker */
    QRegion                    m_screen_dirty_rgn; /* screen shadow to avoid flicker */  
    #define                    BATCH_RC_MAX_NUM  32
    PVR2DRECT                  m_batchRcs[BATCH_RC_MAX_NUM];
    PVR2DBLTINFO               m_blt_inf;

#ifdef QT_MRDP
    MRDPConnMgr               *m_mrdp;             /* mrdp service provider */
    QMutex                     m_mutex_screen;     /* screen dirty region && pixels */
#endif // QT_MRDP
};

QT_END_NAMESPACE

#endif  //QMROFS_COMPOSITOR_H
