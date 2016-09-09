#ifndef QMROFS_H
#define QMROFS_H

#include <QtCore/QObject>
#include <QtGui/qrgb.h>
#include <QtGui/qregion.h>

/*!
	qmrofs header files
	\note ONLY include Qt's public header files here
*/

QT_BEGIN_NAMESPACE

#define MROFS_VERSION  "0.3.0"

class QMrOfsPrivate;

class QImage;
/*!
    \class QWaveData
    \brief the raster data of wave form
    \note if we can use vertex buffers ...
*/
typedef QImage QWaveData;

class Q_GUI_EXPORT QAbstractMrOfsLegacy {
public:
    typedef int WV_ID;
    virtual WV_ID openWaveview() = 0;
    virtual void closeWaveview(WV_ID vid) = 0;
    virtual void setWaveViewport(WV_ID vid, const QRect& vpt) = 0;
    virtual WV_ID createWaveform(WV_ID vid) = 0;
    virtual QWaveData* setWaveformSize(WV_ID wid, const QSize& sz) = 0;
    virtual void destroyWaveform(WV_ID wid) = 0;
    virtual void bindWaveform(WV_ID wid, WV_ID vid) = 0;
    virtual QWaveData* waveformRenderBuffer(WV_ID wid) = 0;
    virtual void lockWaveformRenderBuffer(WV_ID wid) = 0;
    virtual void unlockWaveformRenderBuffer(WV_ID wid) = 0;
    virtual QPoint setWaveformPos(WV_ID wid, const QPoint& pos) = 0;
    virtual void flushWaves(const QRegion& rgn = QRegion()) = 0;
    virtual QRgb getColorKey() const = 0;
    virtual bool enableComposition(bool enable) = 0;
    virtual const char* getVersion() = 0;
};

/*!
    \class QMrOfs
    \brief main controller class for MROFS extension, including:
    wave view controlling,
    wave form controlling,
    animation controlling.
    screen orientation controlling is done by QScreen::setOrientation
    \note  to support multi-threaded wave rendering, the calling path of QMrOfs should be:
    1, app(wave thread) -> QMrOfs::YYY(wave thread/direct call) -> QMrOfsIntegration::XXX(wave thread/direct call) -> QMrOfsCompositor::XXX(comp thread/invoke), where XXX is:
        [flushWaves,  lockWaveformRenderBuffer,  unlockWaveformRenderBuffer]
    2, app(ui thread) -> QrMofs::YYY(ui thread) -> QMrOfsIntegration::YYY(ui thread) -> QMrOfsCompositor::YYY(comp thread/invoke), where YYY is :
        [openWaveview,  and other no in XXX]
    3, wavedata should be locked/unlocked since which read by compositor(comp thread) and write by app(wave thread)
    note QMrOfs should be created ONLY AFTER primary Screen is ready.
    \sa QScreen
*/
class  Q_GUI_EXPORT QMrOfsLegacy : public QObject, QAbstractMrOfsLegacy {
    Q_OBJECT
    Q_DECLARE_PRIVATE(QMrOfs)
    //apps use colorKey to fill up waveform widgets
    Q_PROPERTY(QRgb colorKey READ getColorKey)
    Q_PROPERTY(const char* version READ getVersion)
    
public:
    typedef int WV_ID;
    explicit QMrOfs(QObject *parent = 0);
    /*!
        \brief for QMrOfsV2 deriving
        \note auto virtual
    */
    ~QMrOfs();
    
public Q_SLOTS:
    /*!
        \fn     WV_ID setWaveviewLayer(WV_ID vid, int layer);
        \brief set view layer, return the old
    */
    virtual WV_ID setWaveviewLayer(WV_ID vid, int layer);
    
Q_SIGNALS:
    /*!
        \fn void wavesFlushed(bool bOK);
        this signal is emitted as confirmation of flushWaves command
    */
    void wavesFlushed(bool bOK);
    
public:
    /*!
        \fn     WV_ID openWaveview();
        \brief open a wave view
        \return wave view identifier(vid)
        \note if app wants to display another wave window in pop-ups, it needs to call openWaveview again
    */
    virtual WV_ID openWaveview();
    /*!
        \fn void closeWaveview(WV_ID vid);
        \brief close view which id is vid
    */        
    virtual void closeWaveview(WV_ID vid);
    /*!
        \fn     void setWaveViewport(WV_ID vid, const QRect& vpt);
        \brief set the top/left/width/height of the wave view represented by vid, 
        \note vpt is in SCREEN COORDINATES since app requires it
        \note deprecated in MROFS_USE_GETSCROLLRECT, use GetScrollRect instead.
    */
    virtual void setWaveViewport(WV_ID vid, const QRect& vpt);
    /*!
        \fn     WV_ID createWaveform(WV_ID vid);
        \brief create a wave form, then bind to vid
        \note the binding of wid and vid must be UNIQUE, that is, 
        if wid is binded to vid already, unbinds it from this vid is needed before re-binding it to another vid 
    */
    virtual WV_ID createWaveform(WV_ID vid);
    /*!
        \fn     QWaveData* setWaveformSize(WV_ID wid, const QSize& sz);
        \brief really allocate wave data which size is sz 
        \return QWaveData pointer which intends to be modified by app
        \note app can ONLY write to the returned QWaveData, other ops is NOT supported, such as reading, deleting(which will cause memory coruption)
    */
    virtual QWaveData* setWaveformSize(WV_ID wid, const QSize& sz);
    /*!
        \fn     void destroyWaveform(WV_ID wid);
        \brief free wave data and itself
        \note currently do nothing, all wave forms and it's data is freed automatically when binded wave view has been closed
    */
    virtual void destroyWaveform(WV_ID wid);
    /*!
        \fn     void bindWaveform(WV_ID wid, WV_ID vid);
        \brief bind wid to vid, wid will now use vid's viewport and layer info
        \sa createWaveform for UNIQUE binding limitation
    */
    virtual void bindWaveform(WV_ID wid, WV_ID vid);
    /*!
        \fn    QWaveData* waveformRenderBuffer(WV_ID wid);
        \brief get waveform data, redered by app, app must NOT delete retrun value
        \note for convinient using, the return value is the same as setWaveformSize's
        \sa setWaveformSize
    */
    virtual QWaveData* waveformRenderBuffer(WV_ID wid);
    /*!
        \fn     void lockRenderBuffer();
        \brief lock the waveform render buffer
        \note app may modify the waveform render buffer in a independent thread, if so, waveform render buffer needs to be locked before modifying
        \note after locking, app should unlock this buffer ASAP!
        \sa unlockWaveformRenderBuffer
    */
    virtual void lockWaveformRenderBuffer(WV_ID wid);    
    /*!
        \fn     void unlockWaveformRenderBuffer();
        \brief unlock the waveform render buffer
        \note 
    */    
    virtual void unlockWaveformRenderBuffer(WV_ID wid);
    /*! 
        \fn     QPoint setWaveformPos(WV_ID wid, const QPoint& pos);
        \brief update new position when scrolling in current waveview, top/left, size not changed
        \note pos MUST be in screen coordinate since app requires it
        \note raster buffer, do we need vertex buffer here? 
        \note DHT 20141212, deprecated in MROFS_USE_GETSCROLLRECT, use GetScrollRect instead.
    */
    virtual QPoint setWaveformPos(WV_ID wid, const QPoint& pos);
    /*!
        \fn     void flushWaves(const QRegion& rgn = QRegion());
        \brief submit all wave data in all wave views, re-compositing is needed
    */
    virtual void flushWaves(const QRegion& rgn = QRegion());
    /*!
        \fn     QRgb getColorKey() const;
        \brief return the colorKey preserved by compositor
        \note app can use key color in nowhere EXCEPT Wave Widget range
    */
    virtual QRgb getColorKey() const;

    /*!
        \brief enable/disable composition
        \note disable composition during resizing/re-layout
    */
    virtual bool enableComposition(bool enable);

    /*
        \brief version 
    */
    virtual const char* getVersion();
private:
    Q_DISABLE_COPY(QMrOfs);
};

QT_END_NAMESPACE

#endif
