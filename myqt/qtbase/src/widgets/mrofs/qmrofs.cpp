#include <QtCore/qglobal.h>
#include <QtCore/qmetaobject.h>
#include <QtGui/qwindow.h>
#include <QtGui/qbackingstore.h>
#include <QtWidgets/qplatformcompositor.h>
#include <QtWidgets/qmrofs.h>
#include "qmrofs_p.h"

QT_BEGIN_NAMESPACE

QAbstractMrOfs *QAbstractMrOfs::self = NULL;

QAbstractMrOfs* QAbstractMrOfs::createInstance(QWidget *tlw, QObject *parent)
{
    if(!self)
        self = new QMrOfs(tlw, parent);

    return self;
}

const char* QMrOfs::getVersion()
{
    return MROFS_VERSION;
}

QMrOfs::QMrOfs(QWidget *tlw, QObject *parent) : QObject(*new QMrOfsPrivate, parent), QAbstractMrOfs()
{
    Q_D(QMrOfs);

    if (!tlw) {              //has NOT been created
        qFatal("QMrOfs::QMrOfs, if you want to new QMrOfs in TLW's constructor or before, please call QCoreApplication::setAttribute(Qt::AA_ImmediateWidgetCreation) before your new;  \
                   OR you should new QMrOfs after TLW's first show");
    }
    
    //in case of non TLW
    if (!tlw->isTopLevel()) {
        qWarning("QMrOfs::QMrOfs, tlw is NOT TLW, force to TLW");
        tlw = tlw->window();
    }
    
    QBackingStore *bs = tlw->backingStore();
    QPlatformBackingStore *platform_bs = NULL;    
    if(!bs) {
        qFatal("QMrOfs::QMrOfs,  backingstore not found.  \
                if you want to new QMrOfs in TLW's constructor or before, please call QCoreApplication::setAttribute(Qt::AA_ImmediateWidgetCreation) before your new;  \
                OR you should new QMrOfs after TLW's first show");
    }
    
    platform_bs = bs->handle();
    if(!platform_bs) {
        qFatal("QMrOfs::QMrOfs, platform backingstore not found.  \
                  if you want to new QMrOfs in TLW's constructor or before, please call QCoreApplication::setAttribute(Qt::AA_ImmediateWidgetCreation) before your new;  \
                  OR you should new QMrOfs after TLW's first show");
    }
    
    //override v1 set
    d->compositor = static_cast<QPlatformCompositor*>(platform_bs->compositor());

    //override v1 set
    d->connType = Qt::BlockingQueuedConnection;
    //FIXME: if  QMROFS_V2_THREAD_COMPOSITOR(invisible in ui)  not defined in xcb/mrofs, use this:
    //d->connType = Qt::DirectConnection;
}

/*!
    ui thread
*/
QMrOfs::~QMrOfs() {}

QAbstractMrOfs::WV_ID QMrOfs::openWaveview()
{
    qWarning("QMrOfs::openWaveview: NOT supproted, please use openWaveview(const QWidget&, const QRect&) instead");
    return -1;
}

/*!
    ui thread -> compositor thread
*/
void QMrOfs::closeWaveview(WV_ID vid)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::closeWaveview: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("closeWaveview(QAbstractMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::closeWaveview: indexOfMethod failed");
        return;
    }
    
    QMetaMethod closeWaveview = d->compositor->metaObject()->method(at);
    closeWaveview.invoke(d->compositor, d->connType, Q_ARG(QAbstractMrOfs::WV_ID, vid));
}

/*!
    override to null
*/
QAbstractMrOfs::WV_ID QMrOfs::setWaveviewLayer(WV_ID vid, int layer)
{
    Q_UNUSED(vid);
    Q_UNUSED(layer);    
    qWarning("QMrOfs::setWaveviewLayer: NOT supported, please use createWaveview and openPopup instead!!!");
    return -1;
}

/*!
    ui thread -> compositor thread
*/
void QMrOfs::setWaveViewport(WV_ID vid, const QRect& vpt)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::setWaveViewport: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("setWaveViewport(QAbstractMrOfs::WV_ID,QRect)");
    if (at == -1) {
        qWarning("QMrOfs::setWaveViewport: failed");
        return;
    }

    QRect rc = vpt;
    QMetaMethod setWaveViewport = d->compositor->metaObject()->method(at);
    setWaveViewport.invoke(d->compositor, d->connType, Q_ARG(int, vid), Q_ARG(QRect, rc));
}

QAbstractMrOfs::WV_ID QMrOfs::createWaveform(WV_ID vid)
{
    Q_UNUSED(vid);
    qWarning("QMrOfs::createWaveform(QMrOfs::WV_ID): NOT supported, please use createWaveform(QAbstractMrOfs::WV_ID, const QSize &sz) instead");
    return -1;
}

/*!
    ui thread -> compositor thread
*/
void QMrOfs::destroyWaveform(WV_ID wid)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::destroyWaveform: compositor object is NULL");
        return;
    }
    
    int at = d->compositor->metaObject()->indexOfMethod("destroyWaveform(QAbstractMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::destroyWaveform: indexOfMethod failed");    
        return;
    }

    QMetaMethod destroyWaveform = d->compositor->metaObject()->method(at);
    destroyWaveform.invoke(d->compositor, d->connType, Q_ARG(QAbstractMrOfs::WV_ID, wid));
}

/*!
    resize waveform
    \note waveform data address may change after calling
    ui thread -> compositor thread
*/
QWaveData* QMrOfs::setWaveformSize(WV_ID wid, const QSize& sz)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::setWaveformSize: compositor object is NULL");
        return NULL;
    }

    QWaveData *ret = NULL;
    int at = d->compositor->metaObject()->indexOfMethod("setWaveformSize(QAbstractMrOfs::WV_ID,QSize)");
    if (at == -1) {
        qWarning("QMrOfs::setWaveformSize: indexOfMethod failed");    
        return ret;
    }

    QMetaMethod setWaveformSize = d->compositor->metaObject()->method(at);
    setWaveformSize.invoke(d->compositor, d->connType, Q_RETURN_ARG(QWaveData*, ret),
                                        Q_ARG(QAbstractMrOfs::WV_ID, wid), Q_ARG(QSize, sz));

    return ret;
}

void QMrOfs::bindWaveform(WV_ID wid, WV_ID vid)
{
    Q_UNUSED(wid);
    Q_UNUSED(vid);    
    qWarning("QMrOfs::bindWaveform NOT supproted!!!");
}


/*!
    wid to QWaveData
    ui thread -> compositor thread
*/
QWaveData* QMrOfs::waveformRenderBuffer(WV_ID wid)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::waveformRenderBuffer: compositor object is NULL");
        return NULL;
    }

    QWaveData *ret = NULL;	
    int at = d->compositor->metaObject()->indexOfMethod("waveformRenderBuffer(QAbstractMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::waveformRenderBuffer: indexOfMethod failed");    
        return ret;
    }

    QMetaMethod waveformRenderBuffer = d->compositor->metaObject()->method(at);
    waveformRenderBuffer.invoke(d->compositor, d->connType, Q_RETURN_ARG(QWaveData*, ret), Q_ARG(QAbstractMrOfs::WV_ID, wid));

    return ret;	    
}

void QMrOfs::lockWaveformRenderBuffer(WV_ID wid)
{
    Q_UNUSED(wid);
    qWarning("QMrOfs::lockWaveformRenderBuffer NOT supproted!!!, please call beginUpdateWaves");
}

void QMrOfs::unlockWaveformRenderBuffer(WV_ID wid)
{
    Q_UNUSED(wid);
    qWarning("QMrOfs::unlockWaveformRenderBuffer NOT supproted!!!, please call endUpdateWaves");
}


/*!
    ui thread -> compositor thread
*/
QPoint QMrOfs::setWaveformPos(WV_ID wid, const QPoint& pos)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::setWaveformPos: compositor object is NULL");
        return QPoint();
    }

    QPoint old = QPoint();
    int at = d->compositor->metaObject()->indexOfMethod("setWaveformPos(QAbstractMrOfs::WV_ID,QPoint)");
    if (at == -1) {
        qWarning("QMrOfs::setWaveformPos: indexOfMethod failed");    
        return old;
    }
    
    QMetaMethod setWaveformPos = d->compositor->metaObject()->method(at);
    setWaveformPos.invoke(d->compositor, d->connType, Q_RETURN_ARG(QPoint, old), Q_ARG(QAbstractMrOfs::WV_ID, wid), Q_ARG(QPoint, pos));

    return old;
}

void QMrOfs::flushWaves(const QRegion& rgn)
{
    Q_UNUSED(rgn);
    qWarning("QMrOfs::flushWaves NOT supproted!!!, please call endUpdateWaves");
}

/*!
    wave thread
*/
void QMrOfs::beginUpdateWaves() 
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::beginUpdateWaves: compositor object is NULL");
    	 return;
    }
    
    int at = d->compositor->metaObject()->indexOfMethod("lockWaveResources()");
    if (at == -1) {
        qWarning("QMrOfs::beginUpdateWaves: indexOfMethod failed(lockWaveResources)");    
        return;
    }

    //lock wave resources
    QMetaMethod lockWaveResources = d->compositor->metaObject()->method(at);
    // d->connType == Qt::BlockingQueuedConnection
    lockWaveResources.invoke(d->compositor, Qt::DirectConnection);      //<<<  
}

/*!
    submit/unlock: wave thread
    flushWaves: compositor thread
    \note 
    submitWaveData MUST BE CALLED before unlockWaveResources, otherwise, real wave contents will be missed in the reuslt of composition, since stale pixmap
    unlockWaveResources MUST BE CALLED  before flushWaves,  otherwise, DEADLOCK:
    ui -> compositor -> wave   by lockWaveResources 
    wave -> compositor           by flushWaves
*/
void QMrOfs::endUpdateWaves() 
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::endUpdateWaves: compositor object is NULL");
    	 return;
    }

    //0, submit wave data before unlocking(wave thread)
    int at = d->compositor->metaObject()->indexOfMethod("submitWaveData()");
    if (at == -1) {
        qWarning("QMrOfs::endUpdateWaves: indexOfMethod failed(submitWaveData)");    
        return;
    }
    QMetaMethod submitWaveData = d->compositor->metaObject()->method(at);
    // d->connType == Qt::BlockingQueuedConnection
    submitWaveData.invoke(d->compositor, Qt::DirectConnection);            //<<<

    //1, unlock wave resources (wave thread)
    at = d->compositor->metaObject()->indexOfMethod("unlockWaveResources()");
    if (at == -1) {
        qWarning("QMrOfs::endUpdateWaves: indexOfMethod failed(unlockWaveResources)");    
        return;
    }
    QMetaMethod unlockWaveResources = d->compositor->metaObject()->method(at);
    // d->connType == Qt::BlockingQueuedConnection
    unlockWaveResources.invoke(d->compositor, Qt::DirectConnection);     //<<<

    //2, flushwaves in compositor thread
    at = d->compositor->metaObject()->indexOfMethod("flushWaves()");
    if (at == -1) {
        qWarning("QMrOfs::flushWaves: indexOfMethod failed");    
        return;
    }

    QMetaMethod flushWaves = d->compositor->metaObject()->method(at);
    // d->connType == Qt::BlockingQueuedConnection
    flushWaves.invoke(d->compositor, d->connType);                               //<<<
    
    emit wavesFlushed(true);     /*< emitting in calling thread */
}

QRgb QMrOfs::getColorKey() const
{
    qWarning("QMrOfs::getColorKey NOT supported because premultiplied-RGBA is used!!!");
    return QRgb(0xFFFFFFFF);
}

/*!
    ui thread -> compositor thread
*/
bool QMrOfs::enableComposition(bool enable) 
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::enableComposition: compositor object is NULL");
        return false;
    }

    bool ret = false;    
    int at = d->compositor->metaObject()->indexOfMethod("enableComposition(bool)");
    if (at == -1) {
        qWarning("QMrOfs::enableComposition: indexOfMethod failed");    
        return ret;
    }
    
    QMetaMethod enableComposition = d->compositor->metaObject()->method(at);
    enableComposition.invoke(d->compositor, d->connType, Q_RETURN_ARG(bool, ret), Q_ARG(bool, enable));
    return ret;
}


/*********************************************************************************/
/*********************************V2 ADDITIONAL***********************************/
/*********************************************************************************/
/*!
    ui thread -> compositor thread
*/
QAbstractMrOfs::WV_ID QMrOfs::openWaveview(const QWidget& container, const QRect& vpt)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::openWaveview: compositor object is NULL");
        return -1;
    }

    WV_ID vid = -1;
    int at = d->compositor->metaObject()->indexOfMethod("openWaveview(QWidget,QRect)");
    if (at == -1) {
        qWarning("QMrOfs::openWaveview: indexOfMethod failed");
        return -1;
    }
    
    QMetaMethod openWaveview = d->compositor->metaObject()->method(at);
    openWaveview.invoke(d->compositor, d->connType, Q_RETURN_ARG(QAbstractMrOfs::WV_ID, vid),
                                    Q_ARG(QWidget, container), Q_ARG(QRect, vpt));
                                    
    return vid;
}
/*!
    ui thread -> compositor thread
*/
QAbstractMrOfs::WV_ID QMrOfs::createWaveform(WV_ID vid, const QRect& rc)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::createWaveform: compositor object is NULL");
        return -1;
    }
    
    WV_ID ret = -1;	
    int at = d->compositor->metaObject()->indexOfMethod("createWaveform(QAbstractMrOfs::WV_ID,QRect)");
    if (at == -1) {
        qWarning("QMrOfs::createWaveform: indexOfMethod failed");           
        return ret;
    }

    QMetaMethod createWaveform = d->compositor->metaObject()->method(at);
    createWaveform.invoke(d->compositor, d->connType, Q_RETURN_ARG(QAbstractMrOfs::WV_ID, ret),
                                        Q_ARG(QAbstractMrOfs::WV_ID, vid), Q_ARG(QRect, rc));
                                  
    return ret;      
}

void QMrOfs::unbindWaveform(WV_ID wid, WV_ID vid)
{
    Q_UNUSED(wid);
    Q_UNUSED(vid);    
    qWarning("QMrOfs::unbindWaveform NOT supproted!!!");    
}

/*!
    ui thread -> compositor thread
*/
void QMrOfs::openPopup(const QWidget& self)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::openPopup: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("openPopup(QWidget)");
    if (at == -1) {
        qWarning("QMrOfs::openPopup: indexOfMethod failed");
        return;
    }
    
    QMetaMethod openPopup = d->compositor->metaObject()->method(at);
    openPopup.invoke(d->compositor, d->connType, Q_ARG(QWidget, self));
}

/*!
    ui thread -> compositor thread
*/
void QMrOfs::closePopup(const QWidget& self)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::closePopup: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("closePopup(QWidget)");
    if (at == -1) {
        qWarning("QMrOfs::closePopup: indexOfMethod failed");
        return;
    }
    
    QMetaMethod closePopup = d->compositor->metaObject()->method(at);
    closePopup.invoke(d->compositor, d->connType, Q_ARG(QWidget, self));
}

/*!
    wave thread, otherwise DEADLOCK if double trigger
*/    
void QMrOfs::addWaveformClipRegion(WV_ID wid, const QRegion &rgn)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::addWaveformClipRegion: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("addWaveformClipRegion(QAbstractMrOfs::WV_ID,QRegion)");
    if (at == -1) {
        qWarning("QMrOfs::addWaveformClipRegion: indexOfMethod failed");
        return;
    }
    
    QMetaMethod addWaveformClipRegion = d->compositor->metaObject()->method(at);
    addWaveformClipRegion.invoke(d->compositor, Qt::DirectConnection, Q_ARG(QAbstractMrOfs::WV_ID, wid), Q_ARG(QRegion, rgn));       //<<<
}

void QMrOfs::addWaveformClipRegion(WV_ID wid, const QRect &rc)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::addWaveformClipRegion: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("addWaveformClipRegion(QAbstractMrOfs::WV_ID,QRect)");
    if (at == -1) {
        qWarning("QMrOfs::addWaveformClipRegion: indexOfMethod failed");
        return;
    }
    
    QMetaMethod addWaveformClipRegion = d->compositor->metaObject()->method(at);
    addWaveformClipRegion.invoke(d->compositor, Qt::DirectConnection, Q_ARG(QAbstractMrOfs::WV_ID, wid), Q_ARG(QRect, rc));       //<<<
}


/*!
    wave thread
*/  
void QMrOfs::cleanWaveformClipRegion(WV_ID wid)
{
#if 0
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::cleanWaveformClipRegion: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("cleanWaveformClipRegion(QAbstractMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::cleanWaveformClipRegion: indexOfMethod failed");
        return;
    }
    
    QMetaMethod cleanWaveformClipRegion = d->compositor->metaObject()->method(at);
    cleanWaveformClipRegion.invoke(d->compositor, Qt::DirectConnection, Q_ARG(QAbstractMrOfs::WV_ID, wid));   //<<<
#else
    Q_UNUSED(wid);
    qWarning("QMrOfs::cleanWaveformClipRegion NOT supproted!!!");   
#endif
}

/*!
    wave thread
*/  
void QMrOfs::cleanAllWaveformClipRegion()
{
#if 0
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::cleanAllWaveformClipRegion: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("cleanAllWaveformClipRegion()");
    if (at == -1) {
        qWarning("QMrOfs::cleanAllWaveformClipRegion: indexOfMethod failed");
        return;
    }
    
    QMetaMethod cleanAllWaveformClipRegion = d->compositor->metaObject()->method(at);
    cleanAllWaveformClipRegion.invoke(d->compositor, Qt::DirectConnection);       //<<<
#else 
    qWarning("QMrOfs::cleanAllWaveformClipRegion NOT supproted!!!");    
#endif
}

/*!
    ui thread/wave thread
*/
int QMrOfs::setCompositorThreadPriority(int newpri)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::setCompositorThreadPriority: compositor object is NULL");
        return -1;
    }

    int at = d->compositor->metaObject()->indexOfMethod("setCompositorThreadPriority(int)");
    if (at == -1) {
        qWarning("QMrOfs::setCompositorThreadPriority: indexOfMethod failed");
        return -1;
    }

    int oldpri = 0;
    QMetaMethod setCompositorThreadPriority = d->compositor->metaObject()->method(at);
    setCompositorThreadPriority.invoke(d->compositor, Qt::DirectConnection, Q_RETURN_ARG(int, oldpri), Q_ARG(int, newpri));

    return oldpri;
}

/*!
    ui thread -> compositor thread
*/
QAbstractMrOfs::WV_ID QMrOfs::openIView(const QWidget &container, const QRect& vpt)
{ 
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::openIView: compositor object is NULL");
        return -1;
    }

    int at = d->compositor->metaObject()->indexOfMethod("openIView(QWidget,QRect)");
    if (at == -1) {
        qWarning("QMrOfs::openIView: indexOfMethod failed");
        return -1;
    }

    WV_ID ivid = -1;
    QMetaMethod openIView = d->compositor->metaObject()->method(at);
    openIView.invoke(d->compositor, d->connType, Q_RETURN_ARG(QAbstractMrOfs::WV_ID, ivid), 
                                Q_ARG(QWidget, container), Q_ARG(QRect, vpt));

    return ivid;
}

/*!
    ui thread -> compositor thread
*/
QAbstractMrOfs::WV_ID QMrOfs::createIViewform(WV_ID vid, const HDMI_CONFIG hcfg, Format fmt)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::createIViewform: compositor object is NULL");
        return -1;
    }
    
    int at = d->compositor->metaObject()->indexOfMethod("createIViewform(QAbstractMrOfs::WV_ID,HDMI_CONFIG,QAbstractMrOfs::Format)");
    if (at == -1) {
        qWarning("QMrOfs::createIViewform: indexOfMethod failed");
        return -1;
    }

    WV_ID iwfid = -1;
    QMetaMethod createIViewform = d->compositor->metaObject()->method(at);
    createIViewform.invoke(d->compositor, d->connType, Q_RETURN_ARG(QAbstractMrOfs::WV_ID, iwfid), 
        Q_ARG(QAbstractMrOfs::WV_ID, vid), Q_ARG(HDMI_CONFIG, hcfg), Q_ARG(QAbstractMrOfs::Format, fmt));

    return iwfid;
}

/*!
    iview thread
    \ref addWaveformClipRegion, beginUpdateWaves, endUpdateWaves
*/
int QMrOfs::flipIView(WV_ID vid, WV_ID wid, const QRegion &rgn, const unsigned char *bits)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::flipIView: compositor object is NULL");
        return -1;
    }

    //step0, lock iview resources
    int at = d->compositor->metaObject()->indexOfMethod("lockIViewResources()");
    if (at == -1) {
        qWarning("QMrOfs::flipIView: indexOfMethod failed(lockIViewResources)");    
        return -1;
    }
    
    QMetaMethod lockIViewResources = d->compositor->metaObject()->method(at);
    // d->connType == Qt::BlockingQueuedConnection
    lockIViewResources.invoke(d->compositor, Qt::DirectConnection);      //<<<

    //step1, submit view clip regions (normally the whole viewport of iview)
    addWaveformClipRegion(wid, rgn);
    
    //step2, submit iview bits
    at = d->compositor->metaObject()->indexOfMethod(
        "submitIViewData(QAbstractMrOfs::WV_ID,QAbstractMrOfs::WV_ID,const unsigned char*)");
    if (at == -1) {
        qWarning("QMrOfs::flipIView: indexOfMethod failed(submitIViewData)");
        return -1;
    }
    QMetaMethod submitIViewData = d->compositor->metaObject()->method(at);
    submitIViewData.invoke(d->compositor, Qt::DirectConnection,            //<<<
        Q_ARG(QAbstractMrOfs::WV_ID, vid), Q_ARG(QAbstractMrOfs::WV_ID, wid), 
        Q_ARG(const unsigned char*, bits));
    
    //step3, unlock iview resources
    at = d->compositor->metaObject()->indexOfMethod("unlockIViewResources()");
    if (at == -1) {
        qWarning("QMrOfs::flipIView: indexOfMethod failed(unlockIViewResources");
        return -1;
    }
    QMetaMethod unlockIViewResources = d->compositor->metaObject()->method(at);
    unlockIViewResources.invoke(d->compositor, Qt::DirectConnection);   //<<<
    
    //step4, flush view(to composite)
    at = d->compositor->metaObject()->indexOfMethod("flushIView(QAbstractMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::flipIView: indexOfMethod failed(flushIView)");
        return -1;
    }
    QMetaMethod flushIView = d->compositor->metaObject()->method(at);
    flushIView.invoke(d->compositor, d->connType, Q_ARG(QAbstractMrOfs::WV_ID, wid));
    return 0;
}

/*!
    ui thread -> compositor thread
*/
void QMrOfs::closeIView(WV_ID vid)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::closeIView: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("closeIView(QAbstractMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::closeIView: indexOfMethod failed");
        return;        
    }
    
    QMetaMethod closeIView = d->compositor->metaObject()->method(at);
    closeIView.invoke(d->compositor, d->connType, Q_ARG(QAbstractMrOfs::WV_ID, vid));
}

bool QMrOfs::enableBSFlushing(bool enable)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::enableBSFlushing: compositor object is NULL");
        return false;
    }

    int at = d->compositor->metaObject()->indexOfMethod("enableBSFlushing(bool)");
    if (at == -1) {
        qWarning("QMrOfs::enableBSFlushing: indexOfMethod failed");
        return false;        
    }

    bool ret = false;
    QMetaMethod closeIView = d->compositor->metaObject()->method(at);
    closeIView.invoke(d->compositor, d->connType, Q_ARG(bool, enable), Q_RETURN_ARG(bool, ret));
    return ret;
}

bool QMrOfs::enableWVFlushing(bool enable)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::enableWVFlushing: compositor object is NULL");
        return false;
    }

    int at = d->compositor->metaObject()->indexOfMethod("enableWVFlushing(bool)");
    if (at == -1) {
        qWarning("QMrOfs::enableWVFlushing: indexOfMethod failed");
        return false;        
    }

    bool ret = false;
    QMetaMethod closeIView = d->compositor->metaObject()->method(at);
    closeIView.invoke(d->compositor, d->connType, Q_ARG(bool, enable), Q_RETURN_ARG(bool, ret));
    return ret;
}

bool QMrOfs::enableIVFlushing(bool enable)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::enableIVFlushing: compositor object is NULL");
        return false;
    }

    int at = d->compositor->metaObject()->indexOfMethod("enableIVFlushing(bool)");
    if (at == -1) {
        qWarning("QMrOfs::enableIVFlushing: indexOfMethod failed");
        return false;        
    }

    bool ret = false;
    QMetaMethod closeIView = d->compositor->metaObject()->method(at);
    closeIView.invoke(d->compositor, d->connType, Q_ARG(bool, enable), Q_RETURN_ARG(bool, ret));
    return ret;

}

bool QMrOfs::enableCRFlushing(bool enable)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::enableCRFlushing: compositor object is NULL");
        return false;
    }

    int at = d->compositor->metaObject()->indexOfMethod("enableCRFlushing(bool)");
    if (at == -1) {
        qWarning("QMrOfs::enableCRFlushing: indexOfMethod failed");
        return false;        
    }

    bool ret = false;
    QMetaMethod closeIView = d->compositor->metaObject()->method(at);
    closeIView.invoke(d->compositor, d->connType, Q_ARG(bool, enable), Q_RETURN_ARG(bool, ret));
    return ret;
}

#ifdef PAINT_WAVEFORM_IN_QT
void QMrOfs::PaintWaveformLines(
    QImage* image,
    XCOORDINATE xStart,
    const YCOORDINATE* y,
    const YCOORDINATE* yMax,
    const YCOORDINATE* yMin,
    short yFillBaseLine,
    uint pointsCount,
    const QColor& color,
    QAbstractMrOfs::LINEMODE mode)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::paintWaveformLines: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("paintWaveformLines(QImage*,ushort,const uint*,const uint*,const uint*,short,uint,QColor,QAbstractMrOfs::LINEMODE)");
    if (at == -1) {
        qWarning("QMrOfs::paintWaveformLines: indexOfMethod failed");
        return;
    }

    QMetaMethod paintWaveformLines = d->compositor->metaObject()->method(at);
    paintWaveformLines.invoke(d->compositor, Qt::DirectConnection, Q_ARG(QImage*, image), 
        Q_ARG(ushort, xStart),
        Q_ARG(const uint*, y),
        Q_ARG(const uint*, yMax),
        Q_ARG(const uint*, yMin),
        Q_ARG(short, yFillBaseLine),
        Q_ARG(unsigned int, pointsCount),
        Q_ARG(QColor, color),
        Q_ARG(QAbstractMrOfs::LINEMODE, mode));
}

void QMrOfs::EraseRect(QImage* image, const QRect& rect)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::eraseRect: compositor object is NULL");
        return;
    }
    
    int at = d->compositor->metaObject()->indexOfMethod("eraseRect(QImage*,QRect)");
    if (at == -1) {
        qWarning("QMrOfs::eraseRect: indexOfMethod failed");
        return;
    }
    
    QMetaMethod eraseRect = d->compositor->metaObject()->method(at);
    eraseRect.invoke(d->compositor, Qt::DirectConnection, Q_ARG(QImage*, image), Q_ARG(QRect, rect));
}

void QMrOfs::DrawVerticalLine(QImage* image, 
    XCOORDINATE x, 
    YCOORDINATE y1, 
    YCOORDINATE y2, 
    const QColor& color)
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::drawVerticalLine: compositor object is NULL");
        return;
    }
    int at = d->compositor->metaObject()->indexOfMethod("drawVerticalLine(QImage*,ushort,uint,uint,QColor)");
    if (at == -1) {
        qWarning("QMrOfs::drawVerticalLine: indexOfMethod failed");
        return;
    }
    
    QMetaMethod drawVerticalLine = d->compositor->metaObject()->method(at);
    drawVerticalLine.invoke(d->compositor, Qt::DirectConnection, Q_ARG(QImage*, image), 
        Q_ARG(ushort, x),
        Q_ARG(uint, y1),
        Q_ARG(uint, y2),
        Q_ARG(QColor, color));
}
#endif

#ifdef QT_MRDP
bool QMrOfs::startMRDPService()
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::startMRDPService: compositor object is NULL");
        return false;
    }

    int at = d->compositor->metaObject()->indexOfMethod("startMRDPService()");
    if (at == -1) {
        qWarning("QMrOfs::startMRDPService: indexOfMethod failed");    
        return false;
    }
    
    bool ret = false;
    QMetaMethod startMRDPService = d->compositor->metaObject()->method(at);
    startMRDPService.invoke(d->compositor, d->connType, Q_RETURN_ARG(bool, ret));

    return ret;
}

void QMrOfs::stopMRDPService()
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::stopMRDPService: compositor object is NULL");
        return;
    }

    int at = d->compositor->metaObject()->indexOfMethod("stopMRDPService()");
    if (at == -1) {
        qWarning("QMrOfs::stopMRDPService: indexOfMethod failed");    
        return;
    }

    QMetaMethod stopMRDPService = d->compositor->metaObject()->method(at);
    stopMRDPService.invoke(d->compositor, d->connType);
}

bool QMrOfs::MRDPServiceRunning() 
{
    Q_D(QMrOfs);
    if (!d->compositor) {
        qWarning("QMrOfs::MRDPServiceRunning: compositor object is NULL");
        return false;
    }

    int at = d->compositor->metaObject()->indexOfMethod("MRDPServiceRunning()");
    if (at == -1) {
        qWarning("QMrOfs::MRDPServiceRunning: indexOfMethod failed");    
        return false;
    }

    bool ret = false;
    QMetaMethod MRDPServiceRunning = d->compositor->metaObject()->method(at);
    MRDPServiceRunning.invoke(d->compositor, d->connType, Q_RETURN_ARG(bool, ret));

    return ret;
}

#else
bool QMrOfs::startMRDPService() {}
void QMrOfs::stopMRDPService() {}
bool QMrOfs::MRDPServiceRunning() {return false;}
#endif  // QT_MRDP

QMrOfsPrivate::QMrOfsPrivate() {}
QMrOfsPrivate::~QMrOfsPrivate() {}

QT_END_NAMESPACE
    
