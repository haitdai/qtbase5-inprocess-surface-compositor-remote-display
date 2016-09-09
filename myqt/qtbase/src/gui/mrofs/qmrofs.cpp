#include <QtCore/qglobal.h>
#include <QtCore/qmetaobject.h>
#include <QtGui/qscreen.h>
#include <qimage.h>
#include <qpa/qplatformscreen.h>
#include <QtGui/qmrofs.h>
#include "qmrofs_p.h"

QT_BEGIN_NAMESPACE

#ifdef QT_MROFS_LEGACY

QMrOfs::QMrOfs(QObject *parent) : QObject(*new QMrOfsPrivate /*<object private*/, parent)
{
    Q_D(QMrOfs);
    //use primary screen
    QScreen *primaryScrn = QGuiApplication::screens().at(0);
    d->compositor = primaryScrn->compositor();
    d->connType = Qt::BlockingQueuedConnection;
}

QMrOfs::~QMrOfs() {}

QMrOfs::WV_ID QMrOfs::openWaveview()
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::openWaveview: compositor object is NULL");
        return -1;
    }

    WV_ID vid = -1;
    int at = compositor->metaObject()->indexOfMethod("openWaveview()");
    if (at == -1) {
        qWarning("QMrOfs::openWaveview: indexOfMethod failed");
        return -1;
    }
    
    QMetaMethod openWaveview = compositor->metaObject()->method(at);
    openWaveview.invoke(compositor, d->connType, Q_RETURN_ARG(QMrOfs::WV_ID, vid));

    return vid;
}

void QMrOfs::closeWaveview(WV_ID vid)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::closeWaveview: compositor object is NULL");
        return;
    }

    int at = compositor->metaObject()->indexOfMethod("closeWaveview(QMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::closeWaveview: indexOfMethod failed");        
        return;
    }

    QMetaMethod closeWaveview = compositor->metaObject()->method(at);
    closeWaveview.invoke(compositor, d->connType, Q_ARG(QMrOfs::WV_ID, vid));
}

QMrOfs::WV_ID QMrOfs::setWaveviewLayer(WV_ID vid, int layer)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::setWaveviewLayer: compositor object is NULL");
        return -1;
    }

    QMrOfs::WV_ID old_layer = -1;
    int at = compositor->metaObject()->indexOfMethod("setWaveviewLayer(QMrOfs::WV_ID,int)");
    if (at == -1) {
        qWarning("QMrOfs::setWaveviewLayer: indexOfMethod failed");              
        return old_layer;
    }

    QMetaMethod setWaveviewLayer = compositor->metaObject()->method(at);
    setWaveviewLayer.invoke(compositor, d->connType, Q_RETURN_ARG(QMrOfs::WV_ID, old_layer),
                                          Q_ARG(QMrOfs::WV_ID, vid),
                                          Q_ARG(int, layer));
    
    return old_layer;
}

void QMrOfs::setWaveViewport(WV_ID vid, const QRect& vpt)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::setWaveViewport: compositor object is NULL");
        return;
    }

    int at = compositor->metaObject()->indexOfMethod("setWaveViewport(QMrOfs::WV_ID,QRect)");
    if (at == -1) {
        qWarning("QMrOfs::setWaveViewport: failed");
        return;
    }

    QRect rc = vpt;
    QMetaMethod setWaveViewport = compositor->metaObject()->method(at);
    setWaveViewport.invoke(compositor, d->connType, Q_ARG(int, vid),
                                        Q_ARG(QRect, rc));
}

QMrOfs::WV_ID QMrOfs::createWaveform(WV_ID vid)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::createWaveform: compositor object is NULL");
        return -1;
    }
    
    WV_ID ret = -1;	
    int at = compositor->metaObject()->indexOfMethod("createWaveform(QMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::createWaveform: indexOfMethod failed");           
        return ret;
    }

    QMetaMethod createWaveform = compositor->metaObject()->method(at);
    createWaveform.invoke(compositor, d->connType, Q_RETURN_ARG(QMrOfs::WV_ID, ret),        
                                        Q_ARG(QMrOfs::WV_ID, vid));
                                  
    return ret;                                  
}

void QMrOfs::destroyWaveform(WV_ID wid)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::destroyWaveform: compositor object is NULL");
        return;
    }
    
    int at = compositor->metaObject()->indexOfMethod("destroyWaveform(QMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::destroyWaveform: indexOfMethod failed");    
        return;
    }

    QMetaMethod destroyWaveform = compositor->metaObject()->method(at);
    destroyWaveform.invoke(compositor, d->connType, Q_ARG(QMrOfs::WV_ID, wid));
}

QWaveData* QMrOfs::setWaveformSize(WV_ID wid, const QSize& sz)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::setWaveformSize: compositor object is NULL");
        return NULL;
    }

    QWaveData *ret = NULL;
    int at = compositor->metaObject()->indexOfMethod("setWaveformSize(QMrOfs::WV_ID,QSize)");
    if (at == -1) {
        qWarning("QMrOfs::setWaveformSize: indexOfMethod failed");    
        return ret;
    }

    QMetaMethod setWaveformGeometry = compositor->metaObject()->method(at);
    setWaveformGeometry.invoke(compositor, d->connType, Q_RETURN_ARG(QWaveData*, ret),        
                                                Q_ARG(QMrOfs::WV_ID, wid),
                                                Q_ARG(QSize, sz));

    return ret;
}

void QMrOfs::bindWaveform(WV_ID wid, WV_ID vid)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::bindWaveform: compositor object is NULL");
        return;
    }
    
    int at = compositor->metaObject()->indexOfMethod("bindWaveform(QMrOfs::WV_ID,QMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::bindWaveform: indexOfMethod failed");    
        return;
    }


    QMetaMethod bindWaveform = compositor->metaObject()->method(at);
    bindWaveform.invoke(compositor, d->connType, Q_ARG(QMrOfs::WV_ID, wid),
                                    Q_ARG(QMrOfs::WV_ID, vid)); 
}

QWaveData* QMrOfs::waveformRenderBuffer(WV_ID wid)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::waveformRenderBuffer: compositor object is NULL");
        return NULL;
    }

    QWaveData *ret = NULL;	
    int at = compositor->metaObject()->indexOfMethod("waveformRederContext(QMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::waveformRederContext: indexOfMethod failed");    
        return ret;
    }

    QMetaMethod waveformRederContext = compositor->metaObject()->method(at);
    waveformRederContext.invoke(compositor, d->connType, Q_RETURN_ARG(QWaveData*, ret),        
    						        Q_ARG(QMrOfs::WV_ID, wid));      

    return ret;	
}

//thread-safe
//exec in app's wave thread
void QMrOfs::lockWaveformRenderBuffer(WV_ID wid)
{
    Q_UNUSED(wid);
    //do nothing

    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::lockWaveformRenderBuffer: compositor object is NULL");
        return;
    }   
    int at = compositor->metaObject()->indexOfMethod("lockWaveformRenderBuffer(QMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::lockWaveformRenderBuffer: indexOfMethod failed");    
        return;
    }
    
    QMetaMethod lockWaveformRenderBuffer = compositor->metaObject()->method(at);
    lockWaveformRenderBuffer.invoke(compositor, d->connType,
                                        Q_ARG(QMrOfs::WV_ID, wid));          
}

//thread-safe
//exec in app's wave thread
void QMrOfs::unlockWaveformRenderBuffer(WV_ID wid)
{
    Q_UNUSED(wid);
    //do nothing

    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::unlockWaveformRenderBuffer: compositor object is NULL");
        return;
    }   
    int at = compositor->metaObject()->indexOfMethod("unlockWaveformRenderBuffer(QMrOfs::WV_ID)");
    if (at == -1) {
        qWarning("QMrOfs::unlockWaveformRenderBuffer: indexOfMethod failed");    
        return;
    }
    
    QMetaMethod unlockWaveformRenderBuffer = compositor->metaObject()->method(at);
    unlockWaveformRenderBuffer.invoke(compositor, d->connType,
                                        Q_ARG(QMrOfs::WV_ID, wid));           
}
    
QPoint QMrOfs::setWaveformPos(WV_ID wid, const QPoint& pos)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::setWaveformPos: compositor object is NULL");
        return QPoint();
    }

    QPoint old = QPoint();
    int at = compositor->metaObject()->indexOfMethod("setWaveformPos(QMrOfs::WV_ID,QPoint)");
    if (at == -1) {
        qWarning("QMrOfs::setWaveformPos: indexOfMethod failed");    
        return old;
    }
    
    QMetaMethod setWaveformPos = compositor->metaObject()->method(at);
    setWaveformPos.invoke(compositor, d->connType, Q_RETURN_ARG(QPoint, old),        
                                        Q_ARG(QMrOfs::WV_ID, wid),
    				            Q_ARG(QPoint, pos));      

    return old;
}

/*!
    \note  d->connType: wait until this composition completed
*/
void QMrOfs::flushWaves(const QRegion& rgn)
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::flushWaves: compositor object is NULL");
    	 return;
    }

    int at = compositor->metaObject()->indexOfMethod("flushWaves(QRegion)");
    if (at == -1) {
        qWarning("QMrOfs::flushWaves: indexOfMethod failed");    
        return;
    }

    QMetaMethod flushWaves = compositor->metaObject()->method(at);
    // d->connType == Qt::BlockingQueuedConnection
    flushWaves.invoke(compositor, Qt::QueuedConnection,
                                Q_ARG(QRegion, rgn));                   
    
    emit wavesFlushed(true);     /*< emitting in calling thread */
}

QRgb QMrOfs::getColorKey() const
{
    Q_D(const QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::getColorKey: compositor object is NULL");
        return QRgb(0xFFFFFFFF);
    }

    QRgb ret = QRgb(0xFFFFFFFF);    
    int at = compositor->metaObject()->indexOfMethod("getColorKey()");
    if (at == -1) {
        qWarning("QMrOfs::getColorKey: indexOfMethod failed");    
        return ret;
    }
    
    QMetaMethod getColorKey = compositor->metaObject()->method(at);
    getColorKey.invoke(compositor, d->connType, Q_RETURN_ARG(QRgb, ret));     
    return ret;
}

bool QMrOfs::enableComposition(bool enable) 
{
    Q_D(QMrOfs);
    QPlatformCompositor *compositor = d->compositor;
    if (!compositor) {
        qWarning("QMrOfs::enableComposition: compositor object is NULL");
        return false;
    }

    bool ret = false;    
    int at = compositor->metaObject()->indexOfMethod("enableComposition(bool)");
    if (at == -1) {
        qWarning("QMrOfs::enableComposition: indexOfMethod failed");    
        return ret;
    }
    
    QMetaMethod enableComposition = compositor->metaObject()->method(at);
    enableComposition.invoke(compositor, d->connType, Q_RETURN_ARG(bool, ret), Q_ARG(bool, enable));
    return ret;
}

QMrOfsPrivate::QMrOfsPrivate() : compositor(NULL) {}
QMrOfsPrivate::~QMrOfsPrivate() {}

#else

/*!
	dummy just keep building OK
*/
QMrOfs::QMrOfs(QObject *parent) : QObject(*new QMrOfsPrivate /*<object private*/, parent) {}
QMrOfs::~QMrOfs() {}
QMrOfs::WV_ID QMrOfs::openWaveview(){ return -1; }
void QMrOfs::closeWaveview(WV_ID vid){}
QMrOfs::WV_ID QMrOfs::setWaveviewLayer(WV_ID vid, int layer){ return -1; }
void QMrOfs::setWaveViewport(WV_ID vid, const QRect& vpt){}
QMrOfs::WV_ID QMrOfs::createWaveform(WV_ID vid){ return -1; }
QWaveData* QMrOfs::setWaveformSize(WV_ID wid, const QSize& sz){ return NULL; }
void QMrOfs::destroyWaveform(WV_ID wid){}
void QMrOfs::bindWaveform(WV_ID wid, WV_ID vid){}
QWaveData* QMrOfs::waveformRenderBuffer(WV_ID wid){ return NULL; }
QPoint QMrOfs::setWaveformPos(WV_ID wid, const QPoint& pos){ return QPoint(); }
void QMrOfs::flushWaves(const QRegion& rgn){}
QRgb QMrOfs::getColorKey() const { return QRgb(0xFFFFFFFF); }
bool QMrOfs::enableComposition(bool enable) { return enable; }
void QMrOfs::lockWaveformRenderBuffer(WV_ID wid) {}
void QMrOfs::unlockWaveformRenderBuffer(WV_ID wid) {}
QMrOfsPrivate::QMrOfsPrivate() {}
QMrOfsPrivate::~QMrOfsPrivate() {}
#endif

const char* QMrOfs::getVersion()
{
    return MROFS_VERSION;
}

#include "moc_qmrofs.cpp"

QT_END_NAMESPACE
