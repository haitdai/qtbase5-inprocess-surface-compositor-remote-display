#ifndef QMROFS_COMPOSITOR_H
#define QMROFS_COMPOSITOR_H

#include "qmrofsdefs.h"
#include <qpa/qplatformwindow.h>
#include <qpa/qplatformbackingstore.h>
#include <QtPlatformSupport/private/qplatformcompositor_p.h>        //#include <QtWidgets/qplatformcompositor.h>
#include <QtCore/QThread>
#include <QtCore/QMutex>

class QPlatformScreen;
class QMrOfsWaveform;
class QMrOfsWaveController;
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

#if 0    
    pthread_t  id;
#endif

    friend class QMrOfsCompositor;
};

/*!
    \brief TLW 内部的合成器
    \note
    在V1(QMrOfsCompositor) 中，将实例绑定到screen，但同时实例本身只支持win内的合成，虽然增加了1个screen内只允许1个win存在的检查保证了
    运行的正确定，但从设计上看起来很奇怪。
    在(QMrOfsCompositor)中，职责不变(win内合成)，同时将实例绑定到win，保留1个screen只允许1个可见win存在的检查。
*/
class QMrOfsCompositor : public QPlatformCompositorBase
{
    Q_OBJECT
 
public:
    QMrOfsCompositor(QPlatformWindow *plat_win, QPlatformBackingStore* bs);    
    ~QMrOfsCompositor();
    
public:
    //QMrOfs legacy interface {{{
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
    //flushWaves 和flushBackingStore 的作用完全一样，并非只触发wv 脏区域的合成    
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
    
    //QMrOfs interface additional {{{
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
    
    //for QPlatformBackingStore calling {{{
public:
    //ui/compositor thread
    virtual void lockBackingStoreResources();
    //ui/compositor thread
    virtual void unlockBackingStoreResources();
    //ui thread -> compositor thread, sync, called after unlockBackingStoreWaveResources, before clean dirty region
    //flushBackingStore 和flushWaves 的作用完全一样，并非只触发bs 脏区域的合成
    void flushBackingStore(const QRegion& dirty = QRegion());
protected:
    Q_INVOKABLE void flushBackingStore2(const QRegion& dirty);
public:
    //ui thread -> compositor thread
    void resizeBackingStore(QPlatformBackingStore *bs, const QSize &size, const QRegion &rgn);
protected:
    Q_INVOKABLE void resizeBackingStore2(QPlatformBackingStore *bs, const QSize &size, const QRegion &rgn);
    //}}}

    //for QPlatformWindow calling {{{
public:    
    virtual void setWindowGeometry(const QRect &wmRc);
protected:
    Q_INVOKABLE void setWindowGeometry2(const QRect &wmRc);
    //}}}

    //QMrOfsWaveController callback {{{
public:
    void visitWaveform(QMrOfsWaveform* wf);           //worker (anything else to visit: waveviews, containerwidgets ?)
    void visitIViewform(QMrOfsWaveform* wf);

    //screen calling [ui thread -> compositor thread] {{{
public:    
    QRect initialize(QPlatformScreen *scrn);                        // plugin loading
protected:    
    Q_INVOKABLE QRect initialize2(QPlatformScreen *scrn);    
    // }}}

    //cursor {{{
    //ui thread -> compositor thread
public:    
    void setCursor(QPlatformCursor *cursor);
    void pointerEvent(const QMouseEvent &e);
    void changeCursor(QCursor * widgetCursor, QWindow *window);
protected:
    Q_INVOKABLE void setCursor2(QPlatformCursor *cursor);        
    Q_INVOKABLE void pointerEvent2(const QMouseEvent &e);
    Q_INVOKABLE void changeCursor2(QCursor * widgetCursor, QWindow *window);     
public:    
    void flushCursor();
    QPlatformWindow *tlw() {return m_win_win;}
    //}}}

    //flushing control - 如果有高频率的更新(如iview), 则其它低频率的更新可以不触发
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
    QRect drawCursor();
    void sync(const QRegion& compositeRgn, const QRegion& curRgn);
	
private:
#ifdef QMROFS_THREAD_COMPOSITOR
    QMrOfsCompositorThread              *m_th;                  /*apartment thread */
    QMutex                               m_mutex_bs;
    QMutex                               m_mutex_wv;
    QMutex                               m_mutex_iv;
    QMutex                               m_mutex_wc;
#endif
    QPlatformScreen                     *m_win_scrn;            /*screen info*/
    QPlatformWindow                     *m_win_win;             /*ui geometry && compositing destination */
    QPlatformBackingStore               *m_win_bs;        
    QMrOfsWaveController                *m_wc;                  /* everthing about waveforms */
    
    /* misc */
    bool                                 m_enabled;             /* compositing switch */    
    Qt::ConnectionType                   m_conn_type;           /*ref QMrOfsPrivate.connType */    
    
    bool                                 m_wv_flush_enabled;    //wv
    bool                                 m_iv_flush_enabled;    //iv
    bool                                 m_bs_flush_enabled;    //bs
    bool                                 m_cr_flush_enabled;    //cursor

    QRegion                              m_qt_blit_rgn;
};

QT_END_NAMESPACE

#endif  //QMROFS_COMPOSITOR_H
