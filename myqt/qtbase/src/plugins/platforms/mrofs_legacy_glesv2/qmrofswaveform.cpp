//TODO: CHECK MEMORY LEAK, USE SOME SMART POINTER STUFF
#include "qmrofswaveform.h"
#include "qmrofscompositor.h"
#include <QtGui/qscreen.h>
#include <QtWidgets/QApplication>

QT_BEGIN_NAMESPACE   
#ifdef MROFS_EGLIMAGE_EXTERNAL    
QMrOfsWaveform::QMrOfsWaveform() : wid0(-1), geometry(-1,-1, 0,0), texid(-1)
#elif defined(MROFS_TEXTURE_STREAM2)
QMrOfsWaveform::QMrOfsWaveform(QMrOfsCompositor *cmpr) : wid0(-1), geometry(-1,-1, 0,0), texid(-1), compositor(cmpr)
#endif
{
    static QAtomicInt widGenerator(0);
    this->wid0 = widGenerator.fetchAndAddRelaxed(1);
     qInvalidateNativePixmap(&nativePixmap);            //reset to invalidate, ready for resize
}

QMrOfsWaveform::~QMrOfsWaveform()
{
	
}

void QMrOfsWaveform::resize(const QSize& sz)
{
    this->geometry.setX(0);
    this->geometry.setY(0);
    this->geometry.setSize(sz);
}

/*!
    \brief texture size in dots
    \return true if really allocation
*/
bool QMrOfsWaveform::allocateData(EGLDisplay eglDisplay, QImage::Format format, const QSize& size)
{
    qWarning("QMrOfsWaveform::allocateData,old size(%d,%d), new size(%d,%d)", data0.size().width(), data0.size().height(), size.width(), size.height());
    if(size == data0.size())
        return false;
    
    if(qIsNativePixmapValid(&nativePixmap)) {
#ifdef MROFS_EGLIMAGE_EXTERNAL        
        qDestroyEGLImageTexture(texid);
#elif defined(MROFS_TEXTURE_STREAM2)
#   ifdef MROFS_TEXTURE_STREAM2_DEBUG
        qWarning("QMrOfsWaveform::allocateData: texid(%d)", texid);
#   endif
        qDestroyIMGStreamTexture(texid);
#endif
        qDestroyNativePixmap(&nativePixmap);
        texid = -1;
    }

    qCreateNativePixmap(format, size.width(), size.height(), &nativePixmap, this->reqSize, this->realSize);
#ifdef MROFS_EGLIMAGE_EXTERNAL
    qCreateEGLImageTexture(eglDisplay, &nativePixmap, &texid);
#elif defined(MROFS_TEXTURE_STREAM2)
    this->compositor->createIMGStreamTextureforWv(eglDisplay, &nativePixmap, &texid);
#endif

    qWarning("QMrOfsWaveform::allocateData, allocated size(%d,%d)", nativePixmap.lWidth, nativePixmap.lHeight);
    this->data0 = QWaveData(reinterpret_cast<uchar*>(nativePixmap.vAddress), nativePixmap.lWidth, nativePixmap.lHeight, nativePixmap.lStride, format);
    return true;
}

void QMrOfsWaveform::deallocateData()
{
    if(qIsNativePixmapValid(&nativePixmap)) {
#ifdef MROFS_EGLIMAGE_EXTERNAL        
        qDestroyEGLImageTexture(texid);
#elif defined(MROFS_TEXTURE_STREAM2)
        qDestroyIMGStreamTexture(texid);
#endif
        texid = -1;
        qDestroyNativePixmap(&nativePixmap);
    }
}

/*!
    ref. QMrOfsCompositor::flushWaves
    \note
    texture(0, 0) in top-left corner means nativepixmap's [0, 0]
*/
void QMrOfsWaveform::flushData(QMrOfsWaveview* view, const QRegion& rgn, const QRect& rcScreen)
{
    if(!qIsNativePixmapValid(&nativePixmap)) {
        qWarning("QMrOfsWaveform::flushData: flusing waveform before allocating!");
        return;
    }
    
    //应用没给波形脏区域，则在屏幕范围内flush cache
    if(rgn.isEmpty()) {                            //flush screen range if (dirty) rgn is empty

        QRect vptRectInScreen = view->vpt().intersected(rcScreen);
        QRect vptRectInWaveform = this->geometry.intersected(vptRectInScreen);
        if(vptRectInWaveform.isEmpty())       //nothing to flush
            return;
        
        //visible vpt rect in waveform: waveform coordinate system
        vptRectInWaveform.translate(0 - this->geometry.x(), 0 - this->geometry.y());      
        
        //flush the intersection region of screen area and rgn(wave dirty region)
        //range: [start, end)
        int start_row = vptRectInWaveform.y();
        int end_row = vptRectInWaveform.y() + vptRectInWaveform.height();
        int start_col = vptRectInWaveform.x();
        int end_col = vptRectInWaveform.x() + vptRectInWaveform.width();
        int bytesPP = nativePixmap.lStride / nativePixmap.lWidth;
#ifdef MROFS_FLUSH_CACHE_BY_BLOCK
        //DHT20141125, no needs to modify for GetScrollRect
        void *vAddr = reinterpret_cast<void*>(nativePixmap.vAddress + start_row * nativePixmap.lStride/* + start_col * bytesPP*/);   //avoid nativePixmap's overrun
        size_t szInByte = nativePixmap.lStride * vptRectInWaveform.height();
        /*
    qWarning("flushData: nativePixmap(%p), width(%d), height(%d), vAddr(%p), size_x(%x), size_d(%d)", 
        nativePixmap.vAddress, nativePixmap.lWidth, nativePixmap.lHeight, vAddr, szInByte, szInByte);
        */
        qWritebackNativePixmap(vAddr, szInByte, reinterpret_cast<void*>(nativePixmap.vAddress), nativePixmap.lSizeInBytes); 
#elif defined(MROFS_FLUSH_CACHE_BY_ROW)                          //not contigous memory range, have to handle by row
        void * vAddr;                 
        size_t szInByte = bytesPP * vptRectInWaveform.width();
        for(int i = start_row; i < end_row; i ++) {
            vAddr = reinterpret_cast<void*>(nativePixmap.vAddress + start_row * nativePixmap.lStride + start_col * bytesPP);
            qWritebackNativePixmap(vAddr, szInByte, reinterpret_cast<void*>(nativePixmap.vAddress), nativePixmap.lSizeInBytes);  
        }        
#else // whole cache flushing
        qWritebackNativePixmap(reinterpret_cast<void*>(nativePixmap.vAddress), nativePixmap.lSizeInBytes, reinterpret_cast<void*>(nativePixmap.vAddress), nativePixmap.lSizeInBytes); 
#endif
    }else {                                            //flush real dirty range
        //FIXME: 应用给了波形脏区域...
        qWritebackNativePixmap(reinterpret_cast<void*>(nativePixmap.vAddress), nativePixmap.lSizeInBytes, reinterpret_cast<void*>(nativePixmap.vAddress), nativePixmap.lSizeInBytes);  
    }
}

QMrOfs::WV_ID QMrOfsWaveform::wid()
{
    return static_cast<QMrOfs::WV_ID>(this->wid0);
}

QWaveData& QMrOfsWaveform::data()
{
    return this->data0;
}

/*!
    \note pos is wave viewport's position in waveform coordinate system(up-down+)
*/
QPoint QMrOfsWaveform::move(const QPoint& pos)
{
    QPoint old = QPoint(this->geometry.x(), this->geometry.y());
    if (pos.x()== old.x() && pos.y() == old.y())
        return old;
    
    //size not touched
    this->geometry.moveTo(pos);
    
    return old;
}

/*!
    vertices is set in gles2 coordinate system.
    DRAWING SEQUENCE: 
        left->right, top->bottom in screen coordinate system(Qt used).
        left -> right, bottom -> top in gles2 coordinate system.
*/
int QMrOfsWaveform::setVertices(QMrOfsWaveview *view, const QRect& rcScreen, GLuint vertexLoc)
{
    QRect vptRect = rcScreen.intersected(view->vpt());   //drawing only happened in waveviewport
    if(vptRect.isEmpty())
        return -1;
    
    int i = 0;
#if 1
    //map coordinates into [-1, 1] in x,y axis
    //NOTE: Y direction is opposite to screen coordinate system[Y : top -> bottom in screen coordinate system]
    //NOTE: float nomalization compensation
    GLfloat x0 = 2 * ((vptRect.left() + (vptRect.left() * 1.0) /rcScreen.width()) * 1.0 / rcScreen.width()) - 1.0;                 //left in screen, same in verties
    GLfloat x1 = 2 * ((vptRect.right() + (vptRect.right() * 1.0) /rcScreen.width())* 1.0 / rcScreen.width()) - 1.0;              //right in screen, same in verties
#if 0    
    GLfloat y0 = 1 - ((vptRect.top() + (vptRect.top() * 1.0) /rcScreen.height())* 1.0 / rcScreen.height()) * 2.0;                //top in screen, bottom in vertices
    GLfloat y1 = 1 - ((vptRect.bottom() + (vptRect.bottom() * 1.0) /rcScreen.height())* 1.0 / rcScreen.height()) * 2.0;     //bottom in screen, top in vertices
#else
    GLfloat y0 = 1 - ((vptRect.top() + 0.0)* 1.0 / rcScreen.height()) * 2.0;                //top in screen, bottom in vertices
    GLfloat y1 = 1 - ((vptRect.bottom() + 1.0)* 1.0 / rcScreen.height()) * 2.0;     //bottom in screen, top in vertices
#endif

#if 0
    //checking instead of clamping(since vptRect.isEmpty is out)
    const float error = 0.00001;
    if(x0 > 1.0 + error || x0 < 0 - 1.0 - error || y0 > 1.0 + error || y0 < 0 -1.0 - error ||
        x1 > 1.0 + error || x1 < 0 -1.0 - error || y1 > 1.0 + error || y1 < 0 -1.0 - error )
        qWarning("error waveform vertices coordinate: x0(%f), x1(%f), y0(%f), y1(%f)", x0, x1, y0, y1);
#endif

    //top-left, bottom-left, top-right, bottom-right
    vertexArray[i++] = x0;          //-1.0f;
    vertexArray[i++] = y0;          //1.0f;
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = x0;          //-1.0f; 
    vertexArray[i++] = y1;          //-1.0f;
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = x1;          //1.0f;
    vertexArray[i++] = y0;          //1.0f; 
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = x1;          //1.0f; 
    vertexArray[i++] = y1;          //-1.0f;    
    vertexArray[i++] = -1.0f;  
#else
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = 1.0f;
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = 1.0f;
    vertexArray[i++] = 1.0f;
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = 1.0f;
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = -1.0f;
#endif

    //trace it
#ifdef MROFS_TRACE_WV_COORDINATE        
    qWarning("[!QMrOfsWaveform::setVertices]");    
    qWarning("\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", 
        vertexArray[0], vertexArray[1], vertexArray[3], vertexArray[4], vertexArray[6], vertexArray[7], vertexArray[9], vertexArray[10]);
#endif

    glEnableVertexAttribArray(vertexLoc);
    glVertexAttribPointer(vertexLoc, 3, GL_FLOAT, GL_FALSE, 0, (const void*)vertexArray);
    return 0;
}

void QMrOfsWaveform::unSetVertices(GLuint vertexLoc)
{
    glDisableVertexAttribArray(vertexLoc);
}

/*!
    DRAWING SEQUENCE: lower address -> upper address in texture's memory, into screen range indicated by QMrOfsWaveform::setVertices.
    DRAWING RANGE:       waveviewport intersects waveform
    texture is in texture coordinate system.
    texture coordinate system is the same as waveform coordinate system(up-,down+,left-,right+.
    for SGX530, texture's coordinate(0,0) means lowest address in it's memory(top-left).
*/
int QMrOfsWaveform::setTexCoords(QMrOfsWaveview *view, const QRect& rcScreen, GLuint texCoordLoc)
{
    QRect vptRectInScreen = view->vpt().intersected(rcScreen);                           //screen coordinate system
    QRect vptRectInWaveform = this->geometry.intersected(vptRectInScreen);        //screen coordinate system
    if(vptRectInWaveform.isEmpty())
        return -1;
    
    vptRectInWaveform.translate(0 - this->geometry.x(), 0 - this->geometry.y());      //waveform coordinate system
    
    //float nomalization compensation
    GLfloat x0 = vptRectInWaveform.x() *1.0 / this->geometry.width();                                              //left in screen, same in texture
    GLfloat x1 = (vptRectInWaveform.x() + vptRectInWaveform.width()) *1.0 / this->geometry.width();          //right in screen, same in texture
#if 0    
    GLfloat y0 = vptRectInWaveform.y() *1.0 / this->geometry.height();                                             //top in screen, same in texture
    GLfloat y1 = (vptRectInWaveform.y() + vptRectInWaveform.height()) *1.0 / this->geometry.height();       //bottom in screen, same in texture
#else
    GLfloat y0 = vptRectInWaveform.y() *1.0 / this->geometry.height();                                             //top in screen, same in texture
    GLfloat y1 = (vptRectInWaveform.y() + vptRectInWaveform.height() + 1.0) *1.0 / this->geometry.height();       //bottom in screen, same in texture
#endif

#if 0
    //checking instead of clamping(since vptRectInWaveform.isEmpty is out)
    const float error = 0.00001;
    if(x0 > 1.0 + error || x0 < 0 - error || y0 > 1.0 + error || y0 < 0 - error || x1 > 1.0 + error || x1 < 0 - error || y1 > 1.0 + error || y1 < 0 - error )
        qWarning("error waveform texture coordinate: x0(%f), x1(%f), y0(%f), y1(%f)", x0, x1, y0, y1);
#endif

    int i = 0;
#if 1
    texCoordArray[i++] = x0;
    texCoordArray[i++] = y0;
    texCoordArray[i++] = x0;
    texCoordArray[i++] = y1;
    texCoordArray[i++] = x1;
    texCoordArray[i++] = y0;
    texCoordArray[i++] = x1;
    texCoordArray[i++] = y1;
#else
    texCoordArray[i++] = 0.0f;
    texCoordArray[i++] = 0.0f;
    texCoordArray[i++] = 0.0f;
    texCoordArray[i++] = 1.0f;
    texCoordArray[i++] = 1.0f;
    texCoordArray[i++] = 0.0f;
    texCoordArray[i++] = 1.0f;
    texCoordArray[i++] = 1.0f;
#endif

    //trace it
#ifdef MROFS_TRACE_WV_COORDINATE        
    qWarning("[!QMrOfsWaveform::setTexCoords]");    
    qWarning("<\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", 
        texCoordArray[0], texCoordArray[1], texCoordArray[2], texCoordArray[3], texCoordArray[4], texCoordArray[5], texCoordArray[6], texCoordArray[7]);  
#endif

    glEnableVertexAttribArray(texCoordLoc);
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, (const void*)texCoordArray);

    //loc binded in QMrofsScreen
    return 0;
}

void QMrOfsWaveform::unSetTexCoords(GLuint texCoordLoc)
{
    glDisableVertexAttribArray(texCoordLoc);
}

QMrOfsWaveview::QMrOfsWaveview() : vpt0(-1, -1, 0, 0), wfs(QList<QMrOfsWaveform*>())
{
    static QAtomicInt vidGenerator(0);
    this->vid0 = vidGenerator.fetchAndAddRelaxed(1);
    this->layer = this->vid0;
}

QMrOfsWaveview::~QMrOfsWaveview()
{
    
}

void QMrOfsWaveview::setViewport(const QRect& rc)
{
    if (this->vpt0 == rc)
        return;
    this->vpt0 = rc;
    //qWarning("QMrOfsWaveview::setViewport: %d, %d, %d, %d", rc.x(), rc.y(), rc.width(), rc.height());
}

QMrOfs::WV_ID QMrOfsWaveview::vid()
{
    return static_cast<QMrOfs::WV_ID>(this->vid0);
}

QRect QMrOfsWaveview::vpt()
{
    return this->vpt0;
}

void QMrOfsWaveview::bindWaveform(QMrOfsWaveform *wf)
{
    int i = this->wfs.indexOf(wf);
    if(i == -1)
        this->wfs.append(wf);              /*< not exist */
    else
        this->wfs.replace(i, wf);           /*< already exist */
}

void QMrOfsWaveview::unbindWaveform(QMrOfsWaveform *wf)
{
    wfs.removeOne(wf); 
}

QMrOfs::WV_ID QMrOfsWaveview::bindedWaveform(QMrOfsWaveform *wf)
{
    int i = this->wfs.indexOf(wf);
    if(-1 == i)            //not found
        return -1;
    return wf->wid();
}
        
void QMrOfsWaveview::deleteAllWaveforms(QMrOfsWaveController *wc)
{
    foreach(QMrOfsWaveform *wf, wfs) {
        wc->deleteWaveform(wf);                     //delete waveforms from wc
        wfs.removeOne(wf);                              //remove from wfs
    }
}

void QMrOfsWaveview::flushAllWaveforms(const QRegion& rgn, const QRect& rcScreen)
{
    foreach(QMrOfsWaveform *wf, wfs) {
        wf->flushData(this, rgn, rcScreen);
    }
}

QMrOfsWaveController::QMrOfsWaveController() : wvs(QList<QMrOfsWaveview*>()), wfs(QList<QMrOfsWaveform*>())
{
    
}

QMrOfsWaveController::~QMrOfsWaveController()
{
    
}

/*!
    \brief  view in tail position is higher in z order
*/
QMrOfsWaveview* QMrOfsWaveController::openWaveview()
{
    QMrOfsWaveview *view = new QMrOfsWaveview();
    wvs.append(view);
    return view;
}

/*!
    \brief by vid
*/
void QMrOfsWaveController::closeWaveview(QMrOfs::WV_ID vid)
{
    QMrOfsWaveview *to_close = NULL;
    foreach(QMrOfsWaveview * wv, wvs) {
        if(wv->vid() == vid) {
            to_close = wv;
            break;            //found vid's wv
        }
    }
    if(to_close == NULL)
        return;               //vid's wv not exist, nothing to delete

    //delete all waveforms belong to this view
    to_close->deleteAllWaveforms(this);
    //delete view
    delete to_close;
    //remove from wvs
    wvs.removeOne(to_close);
}

/*!
    \brief by vid which is not been indexed
*/
QMrOfsWaveview* QMrOfsWaveController::waveview(QMrOfs::WV_ID vid)
{
    QMrOfsWaveview *current_wv = NULL;
    foreach(QMrOfsWaveview *wv, wvs) {
        if(wv->vid() == vid) {
            current_wv = wv;
            break;
        }    
    }
    return current_wv;
}

QMrOfsWaveform* QMrOfsWaveController::waveform(QMrOfs::WV_ID wid)
{
    foreach(QMrOfsWaveform *wf, wfs) {
        if(wf->wid() == wid) {
            return wf;
        }    
    }

    return NULL;
}

/*!
    \breif change vid's "layer index" to layer
*/
QMrOfs::WV_ID QMrOfsWaveController::setWaveviewLayer(QMrOfs::WV_ID vid, int layer)
{
    //layer should be at least 0 and less than size()
    QMrOfsWaveview *current_wv = NULL;
    int old_layer = -1;
    foreach(QMrOfsWaveview * wv, wvs) {
        if(wv->vid() == vid) {
            current_wv = wv;
            break;            //found vid's wv
        }
    }
    if(current_wv == NULL)
        return -1;           //vid's wv not exist, nothing to change

    old_layer = wvs.indexOf(current_wv);
    if (old_layer != -1 && old_layer != layer) {
        wvs.move(old_layer, layer);   //do the change
    }
    
    return static_cast<QMrOfs::WV_ID>(old_layer);
}

QMrOfsWaveview* QMrOfsWaveController::waveformInView(QMrOfs::WV_ID wid)
{
    QMrOfsWaveform *wf = this->waveform(wid);
    if(NULL == wf)          //non-exists wave form
        return NULL;
    foreach(QMrOfsWaveview *wv, wvs) {
        if(-1 != wv->bindedWaveform(wf))
            return wv;
    }
    return NULL;             //wave form exists but not binded yet
}

#ifdef MROFS_EGLIMAGE_EXTERNAL    
QMrOfsWaveform* QMrOfsWaveController::addWaveform()
{
    QMrOfsWaveform *wf = new QMrOfsWaveform();                                         //without allocating 
    wfs.append(wf);
    return wf;
}
#elif defined(MROFS_TEXTURE_STREAM2)
QMrOfsWaveform* QMrOfsWaveController::addWaveform(QMrOfsCompositor *cmpr)
{
    QMrOfsWaveform *wf = new QMrOfsWaveform(cmpr);      //without allocating
    wfs.append(wf);
    return wf;
}
#endif

void QMrOfsWaveController::deleteWaveform(QMrOfsWaveform *wf)
{
    wf->deallocateData();      //clean eglImage and CMEM mem in waveform
    delete wf;                      //delete waveform
    wfs.removeOne(wf);
}

void QMrOfsWaveController::unbindWaveform(QMrOfsWaveform *wf)
{
    foreach(QMrOfsWaveview *wv, wvs) {
        wv->unbindWaveform(wf);
    }
}

void QMrOfsWaveController::flushWaves(const QRegion& rgn, const QRect& rcScreen)
{
    foreach(QMrOfsWaveview *wv, wvs) {
        wv->flushAllWaveforms(rgn, rcScreen);
    }
}

QT_END_NAMESPACE

