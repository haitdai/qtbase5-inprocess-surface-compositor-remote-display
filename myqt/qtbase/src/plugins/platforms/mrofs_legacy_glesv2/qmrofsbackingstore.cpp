#include "qmrofsbackingstore.h"
#include "qmrofswindow.h"
#include "qmrofsscreen.h"
#include "qmrofscompositor.h"
#include <qpa/qplatformwindow.h>
#include <QtGui/qscreen.h>
#include <QThread>
#include <QtGui/QPainter>

QT_BEGIN_NAMESPACE

QMrOfsBackingStore::QMrOfsBackingStore(QWindow *window)  : QPlatformBackingStore(window), texid(-1), m_bsLock(QMutex::Recursive)
{
    //由于不能保证在backingstore 构造时，不能确定platformWindow(QWindow.create/QWidget.createWinIds)一定存在；
    //但在QMrOfsWindow.setVisible 时可以确保platformWindow 一定存在...
    if (window->handle())
        (static_cast<QMrOfsWindow *>(window->handle()))->setBackingStore(this);                                 //SET TO window
    else
        (static_cast<QMrOfsCompositor *>(window->screen()->compositor()))->addBackingStore(this);    //ADD TO window until it's VISIBLE
    qInvalidateNativePixmap(&nativePixmap);
    mCompositor = static_cast<QMrOfsCompositor *>(window->screen()->compositor());        
}

/*!
    \note run after compositor thread finished
    \note
    对于 mImageUI, 在 QMrOfsBackingStore's dtor 中自动释放(QImageData::own_data == true)
    对于 mImage, 手工释放(QImageData::own_data == false)
*/
QMrOfsBackingStore::~QMrOfsBackingStore()
{
#ifdef MROFS_EGLIMAGE_EXTERNAL
    qDestroyEGLImageTexture(texid);
#elif defined(MROFS_TEXTURE_STREAM2)
    qDestroyIMGStreamTexture(texid);
#endif
    texid = -1;
    qDestroyNativePixmap(&nativePixmap);
}

/*\brief
    BS = BackingStore; WF = WaveForm.
    BS is read when compositing in compositor thread, BS is written when ui draws it in ui thread(between BS.beginPaint and BS.endPaint).
    WF is read when compositing in compositor thread; WF is wrtten when waveobj draws it in WAVE thread(between QPainter.begin and QPainter.end).
    there are 2 trigger point of compositing(reading BS in compositor thread): ui flush in ui thread and wave flush in wave thread.
    there are 2 swap ways of compositing: front(single fb) and flip(double fb); front is asyncronized and flip is synchronized(eglSwapBuffers waits for fb dma done).
    basically, there are 2 ways to syncroniize compositor thread and ui thread:
    1, copy backingstore when ui flushes then trigger compositing; do nothing in beginpaint/endpaint; the WHOLE backingstore needs to be copied because of WF is almost fullscreen.
        NOTE under am3358's 300MHz CPU + 100MHz L3 + 90MHz DDR3 configuration, Memcopy's ferformance is 72.9MB/s approximitely(120FPS @320*480*4BPP).
    2, in ui thread: lock in beginpaint, unlock in endpaint; in compositor thread: lock before compositing and unlock after it.
        trigger compositing in flush
*/
void QMrOfsBackingStore::lock()
{
    //qWarning("QMrOfsBackingStore::lock");
    m_bsLock.lock();
}

void QMrOfsBackingStore::unlock()
{
    //qWarning("QMrOfsBackingStore::unlock");
    m_bsLock.unlock();
}

void QMrOfsBackingStore::beginPaint(const QRegion &rgn)
{
#ifdef MROFS_PROFILING
    gettimeofday(&m_startTime, NULL);    
    qWarning("MROFS_PROFILING: beginPaint: %ld:%ld", m_startTime.tv_sec, m_startTime.tv_usec);
#endif

#ifdef MROFS_TEARING_ELIMINATION_DELAY
    m_bsLock.lock();
#endif

    m_dirty |= rgn;
}

/*!
    flush happened after endPaint returned, ref. QWidgetBackingStore::endPaint
*/
void QMrOfsBackingStore::endPaint()
{
#ifdef MROFS_PROFILING
    gettimeofday(&m_endTime, NULL);    
    m_diffTime = tv_diff(&m_startTime, &m_endTime);
    qWarning("MROFS_PROFILING: endPaint: %ld:%ld, diff: %ld", m_endTime.tv_sec, m_endTime.tv_usec, m_diffTime);
#endif

#ifdef MROFS_TEARING_ELIMINATION_DELAY
    m_bsLock.unlock();
#endif
}

/* \brief
    blitting mImageUI into mImage in dirty range
*/
void QMrOfsBackingStore::updateTexture()
{
#ifdef MROFS_TEARING_ELIMINATION_COPY

#  ifdef MROFS_PROFILING
    struct timeval startTime, endTime;
    unsigned long diffTime;
    gettimeofday(&startTime, NULL);    
    qWarning("MROFS_PROFILING: updateTexture begin: %ld:%ld", startTime.tv_sec, startTime.tv_usec);    
#  endif

#ifdef MROFS_UPDATING_BS_IN_UI_THREAD
    m_bsLock.lock();
#endif

    QPainter blitter(&this->mImage);

#  ifdef WHOLE_BS_BLITTING       //whole image blitting
    blitter.drawImage(mImage.rect(), mImageUI, mImageUI.rect());
    return;
# else
    if (!m_dirty.isNull()) {
        QRegion fixed;
        QRect imageRect = mImage.rect();

        foreach (const QRect &rect, m_dirty.rects()) {
            // intersect with image rect to be sure
            QRect r = imageRect & rect;
            fixed |= r;
#  ifdef MROFS_PROFILING            
    qWarning("MROFS_PROFILING, dirty rect(%d,%d,%d,%d)", r.x(), r.y(), r.width(), r.height());
#  endif
        }

        foreach (const QRect &rect, fixed.rects()) {
            blitter.drawImage(rect, mImageUI, rect);
        }
        
        m_dirty = QRegion();        
    } 
#endif   

#ifdef MROFS_UPDATING_BS_IN_UI_THREAD
    m_bsLock.unlock();   
#endif

#  ifdef MROFS_PROFILING
    gettimeofday(&endTime, NULL);    
    m_diffTime = tv_diff(&startTime, &endTime);
    qWarning("MROFS_PROFILING: updateTexture end: %ld:%ld, diff: %ld", endTime.tv_sec, endTime.tv_usec, diffTime);
#  endif

#endif    //MROFS_TEARING_ELIMINATION_COPY
}

/* \brief
    region: dirtyOnScreen 
*/
void QMrOfsBackingStore::flush(QWindow *window, const QRegion &region, const QPoint &offset)
{
    Q_UNUSED(window);
    Q_UNUSED(offset);
    
    //backingstore is redrawed ok in QWidget.paintEvent ...

    //copy ui image into composition image
    //sequential locking, safe
    //lock1
#ifdef MROFS_UPDATING_BS_IN_UI_THREAD    
    updateTexture();
#endif    
    //unlock1
    
    //flush backingstore in compositor thread
    //lock2
    mCompositor->flushBackingStore(this, window, region);
    //unlock2
#if 0
    (static_cast<QMrOfsWindow *>(window->handle()))->repaint(region);
#endif
}

void QMrOfsBackingStore::resize(const QSize &size, const QRegion &staticContents)
{
    Q_UNUSED(staticContents);
    
    if((size == mImage.size())
#ifdef MROFS_TEARING_ELIMINATION_COPY        
        && (size == mImageUI.size())
#endif        
        )
        return;
    
    //modify backingstore in compositor thread
    mCompositor->resizeBackingStore(this, size);
    
    //composition backingstore - make sure to run non-const-data version of QImage::QImage, otherwise QImage will be copied by Qt
    mImage = QImage(reinterpret_cast<uchar*>(nativePixmap.vAddress), nativePixmap.lWidth, nativePixmap.lHeight, nativePixmap.lStride, window()->screen()->handle()->format());
    
#ifdef MROFS_TEARING_ELIMINATION_COPY
    //ui backingstore
    mImageUI = QImage(QSize(nativePixmap.lWidth, nativePixmap.lHeight), window()->screen()->handle()->format());
#endif
}

/*!
    \note mrofs compositor does NOT need raster data of the backingstore than fb compositor, neither QMrOfsBackingStore::image()
    \note BUT QPainter need it by QBackingStore::paintDevice()
*/
const QImage QMrOfsBackingStore::image()
{
    return mImage;
}

QPaintDevice *QMrOfsBackingStore::paintDevice() 
{ 
#ifdef MROFS_TEARING_ELIMINATION_COPY
    return &mImageUI; 
#else
    return &mImage;
#endif
}

GLuint QMrOfsBackingStore::texID() 
{ 
    return texid; 
}
    
/*!
    \note running in compositor thread
*/
int QMrOfsBackingStore::setVertices(const QRect &rect)
{
#if 1
    //map coordinates into [-1, 1] in x,y axis
    QMrOfsScreen* scrn = static_cast<QMrOfsScreen *>(window()->screen()->handle());
    GLfloat x0 = 2 * ((rect.left() + (rect.left() * 1.0) /scrn->geometry().width()) * 1.0 / scrn->geometry().width()) - 1.0;                 //left
    GLfloat x1 = 2 * ((rect.right() + (rect.right() * 1.0) /scrn->geometry().width())* 1.0 / scrn->geometry().width()) - 1.0;               //right
#if 0     
    GLfloat y0 = 1 - ((rect.top() + (rect.top() * 1.0) /scrn->geometry().height())* 1.0 / scrn->geometry().height()) * 2.0;                //top
    GLfloat y1 = 1 - ((rect.bottom() + (rect.bottom() * 1.0) /scrn->geometry().height())* 1.0 / scrn->geometry().height()) * 2.0;          //bottom
#else
    //GLESv2 SPEC 上说纹理的光栅化是半开半闭区间，因此默认会少输出一个像素(结束位置)
    //这里处理成: 下面的边向下靠，上面的边向上靠，盖住边界
    //注意纹理坐标也要对应处理，否则会拉伸(变模糊)
    GLfloat y0 = 1 - ((rect.top() + 0.0)* 1.0 / scrn->geometry().height()) * 2.0;                //top
    GLfloat y1 = 1 - ((rect.bottom() + 1.0)* 1.0 / scrn->geometry().height()) * 2.0;          //bottom
#endif
    int i = 0;
    //top-left, bottom-left, top-right, bottom-right
    vertexArray[i++] = x0;          //-1.0f;
    vertexArray[i++] = y0;          //1.0f;
    vertexArray[i++] = -1.0f;
    vertexArray[i++] = x0;          //-1.0f; 
    vertexArray[i++] = y1;          //-1.0f;    vertices, from 1 to -1(in ogles2 coordinate, from top to bottom)
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
#ifdef MROFS_TRACE_BS_COORDINATE        
    qWarning("[!QMrOfsBackingStore::setVertices]");    
    qWarning("\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", 
        vertexArray[0], vertexArray[1], vertexArray[3], vertexArray[4], vertexArray[6], vertexArray[7], vertexArray[9], vertexArray[10]);
#endif

    glEnableVertexAttribArray(mCompositor->vertexLoc());
    glVertexAttribPointer(mCompositor->vertexLoc(), 3, GL_FLOAT, GL_FALSE, 0, (const void*)vertexArray);
    return 0;
}

/*!
    \note running in compositor thread
*/
void QMrOfsBackingStore::unSetVertices()
{
    glDisableVertexAttribArray(mCompositor->vertexLoc());
}

/*! 
    \brief 2D texture coordinates
    \note, coordinate system is different: 
    Qt: left=x+; down=y+; no z.
    ogles: left=x+; up=y+; inner=z+.
    in this case, reverse y direction during texture mapping
    \note running in compositor thread
*/
int QMrOfsBackingStore::setTexCoords(const QRect &rect)
{
    //since rounding width, use realSize to obtain correct texture coord in texture coordinate system, win.geometry not worked any more
    GLfloat x0 = (rect.left() + (rect.left() * 1.0 / this->realSize.width()))* 1.0  / this->realSize.width();               //left
    GLfloat x1 = (rect.right() + (rect.right() * 1.0 / this->realSize.width()))* 1.0 / this->realSize.width();              //right
#if 0   
    GLfloat y0 = (rect.top() + (rect.top() *1.0 / this->realSize.height()))* 1.0 / this->realSize.height();               //top
    GLfloat y1 = (rect.bottom() + (rect.bottom() * 1.0 / this->realSize.height()))* 1.0 / this->realSize.height();         //bottom
#else
    GLfloat y0 = (rect.top() + 0.0)* 1.0 / this->realSize.height();               //top
    GLfloat y1 = (rect.bottom() + 1.0)* 1.0 / this->realSize.height();         //bottom
#endif
    int i = 0;
    //bottom-left, top-left, bottom-right, top-right
#if 1
    texCoordArray[i++] = x0;
    texCoordArray[i++] = y0;   //y0 is top, ==0
    texCoordArray[i++] = x0;
    texCoordArray[i++] = y1;   //y1 is bottom, ==1,   texture: from 0 to 1 (in goles2 coordinate, from bottom to top)
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
#ifdef MROFS_TRACE_BS_COORDINATE        
    qWarning("[!QMrOfsBackingStore::setTexCoords]");    
    qWarning("<\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", 
        texCoordArray[0], texCoordArray[1], texCoordArray[2], texCoordArray[3], texCoordArray[4], texCoordArray[5], texCoordArray[6], texCoordArray[7]);  
#endif

    glEnableVertexAttribArray(mCompositor->texCoordLoc());
    glVertexAttribPointer(mCompositor->texCoordLoc(), 2, GL_FLOAT, GL_FALSE, 0, (const void*)texCoordArray);

    //loc binded in QMrofsScreen
    return 0;
}

/*!
    \note running in compositor thread
*/
void QMrOfsBackingStore::unSetTexCoords()
{
    glDisableVertexAttribArray(mCompositor->texCoordLoc());
}


//DHT20141124, GetScrollRect
#ifdef MROFS_USE_GETSCROLLRECT
int QMrOfsBackingStore::setVerticesFixed(const QRect &rect)
{
#if 1
    //map coordinates into [-1, 1] in x,y axis
    QMrOfsScreen* scrn = static_cast<QMrOfsScreen *>(window()->screen()->handle());
    GLfloat x0 = 2 * ((rect.left() + (rect.left() * 1.0) /scrn->geometry().width()) * 1.0 / scrn->geometry().width()) - 1.0;                 //left
    GLfloat x1 = 2 * ((rect.right() + (rect.right() * 1.0) /scrn->geometry().width())* 1.0 / scrn->geometry().width()) - 1.0;               //right
    GLfloat y0 = 1 - ((rect.top() + (rect.top() * 1.0) /scrn->geometry().height())* 1.0 / scrn->geometry().height()) * 2.0;                //top
    GLfloat y1 = 1 - ((rect.bottom() + (rect.bottom() * 1.0) /scrn->geometry().height())* 1.0 / scrn->geometry().height()) * 2.0 - 0.005;          //bottom

    int i = 0;
    //top-left, bottom-left, top-right, bottom-right
    vertexArrayFixed[i++] = x0;          //-1.0f;
    vertexArrayFixed[i++] = y0;          //1.0f;
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = x0;          //-1.0f; 
    vertexArrayFixed[i++] = y1;          //-1.0f;    vertices, from 1 to -1(in ogles2 coordinate, from top to bottom)
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = x1;          //1.0f;
    vertexArrayFixed[i++] = y0;          //1.0f; 
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = x1;          //1.0f; 
    vertexArrayFixed[i++] = y1;          //-1.0f;    
    vertexArrayFixed[i++] = -1.0f;  
#else
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = 1.0f;
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = 1.0f;
    vertexArrayFixed[i++] = 1.0f;
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = 1.0f;
    vertexArrayFixed[i++] = -1.0f;
    vertexArrayFixed[i++] = -1.0f;
#endif

    //trace it
#ifdef MROFS_TRACE_BS_COORDINATE        
    qWarning("[!QMrOfsBackingStore::setVertices]");    
    qWarning("\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", 
        vertexArrayFixed[0], vertexArrayFixed[1], vertexArrayFixed[3], vertexArrayFixed[4], vertexArrayFixed[6], vertexArrayFixed[7], vertexArrayFixed[9], vertexArrayFixed[10]);
#endif

    glEnableVertexAttribArray(mCompositor->vertexLoc());
    glVertexAttribPointer(mCompositor->vertexLoc(), 3, GL_FLOAT, GL_FALSE, 0, (const void*)vertexArrayFixed);
    return 0;
}

int QMrOfsBackingStore::setVerticesScrolled(const QRect &rect)
{
#if 1
    //map coordinates into [-1, 1] in x,y axis
    QMrOfsScreen* scrn = static_cast<QMrOfsScreen *>(window()->screen()->handle());
    GLfloat x0 = 2 * ((rect.left() + (rect.left() * 1.0) /scrn->geometry().width()) * 1.0 / scrn->geometry().width()) - 1.0;                 //left
    GLfloat x1 = 2 * ((rect.right() + (rect.right() * 1.0) /scrn->geometry().width())* 1.0 / scrn->geometry().width()) - 1.0;               //right
    GLfloat y0 = 1 - ((rect.top() + (rect.top() * 1.0) /scrn->geometry().height())* 1.0 / scrn->geometry().height()) * 2.0 ;                //top
    GLfloat y1 = 1 - ((rect.bottom() + (rect.bottom() * 1.0) /scrn->geometry().height())* 1.0 / scrn->geometry().height()) * 2.0;          //bottom

    int i = 0;
    //top-left, bottom-left, top-right, bottom-right
    vertexArrayScrolled[i++] = x0;          //-1.0f;
    vertexArrayScrolled[i++] = y0;          //1.0f;
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = x0;          //-1.0f; 
    vertexArrayScrolled[i++] = y1;          //-1.0f;    vertices, from 1 to -1(in ogles2 coordinate, from top to bottom)
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = x1;          //1.0f;
    vertexArrayScrolled[i++] = y0;          //1.0f; 
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = x1;          //1.0f; 
    vertexArrayScrolled[i++] = y1;          //-1.0f;    
    vertexArrayScrolled[i++] = -1.0f;  
#else
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = 1.0f;
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = 1.0f;
    vertexArrayScrolled[i++] = 1.0f;
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = 1.0f;
    vertexArrayScrolled[i++] = -1.0f;
    vertexArrayScrolled[i++] = -1.0f;
#endif

    //trace it
#ifdef MROFS_TRACE_BS_COORDINATE        
    qWarning("[!QMrOfsBackingStore::setVertices]");    
    qWarning("\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", 
        vertexArrayScrolled[0], vertexArrayScrolled[1], vertexArrayScrolled[3], vertexArrayScrolled[4], vertexArrayScrolled[6], vertexArrayScrolled[7], vertexArrayScrolled[9], vertexArrayScrolled[10]);
#endif

    glEnableVertexAttribArray(mCompositor->vertexLoc());
    glVertexAttribPointer(mCompositor->vertexLoc(), 3, GL_FLOAT, GL_FALSE, 0, (const void*)vertexArrayScrolled);
    return 0;
}

int QMrOfsBackingStore::setTexCoordsFixed(const QRect &rect)
{
    //since rounding width, use realSize to obtain correct texture coord in texture coordinate system, win.geometry not worked any more
    GLfloat x0 = (rect.left() + (rect.left() * 1.0 / this->realSize.width()))* 1.0  / this->realSize.width();               //left
    GLfloat x1 = (rect.right() + (rect.right() * 1.0 / this->realSize.width()))* 1.0 / this->realSize.width();              //right
    GLfloat y0 = (rect.top() + (rect.top() *1.0 / this->realSize.height()))* 1.0 / this->realSize.height();               //top
    GLfloat y1 = (rect.bottom() + (rect.bottom() * 1.0 / this->realSize.height()))* 1.0 / this->realSize.height();         //bottom

    int i = 0;
    //bottom-left, top-left, bottom-right, top-right
#if 1
    texCoordArrayFixed[i++] = x0;
    texCoordArrayFixed[i++] = y0;   //y0 is top, ==0
    texCoordArrayFixed[i++] = x0;
    texCoordArrayFixed[i++] = y1;   //y1 is bottom, ==1,   texture: from 0 to 1 (in goles2 coordinate, from bottom to top)
    texCoordArrayFixed[i++] = x1;
    texCoordArrayFixed[i++] = y0; 
    texCoordArrayFixed[i++] = x1;
    texCoordArrayFixed[i++] = y1; 
#else
    texCoordArrayFixed[i++] = 0.0f;
    texCoordArrayFixed[i++] = 0.0f;
    texCoordArrayFixed[i++] = 0.0f;
    texCoordArrayFixed[i++] = 1.0f;
    texCoordArrayFixed[i++] = 1.0f;
    texCoordArrayFixed[i++] = 0.0f;
    texCoordArrayFixed[i++] = 1.0f;
    texCoordArrayFixed[i++] = 1.0f;
#endif

    //trace it
#ifdef MROFS_TRACE_BS_COORDINATE        
    qWarning("[!QMrOfsBackingStore::setTexCoords]");    
    qWarning("<\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", 
        texCoordArrayFixed[0], texCoordArrayFixed[1], texCoordArrayFixed[2], texCoordArrayFixed[3], texCoordArrayFixed[4], texCoordArrayFixed[5], texCoordArrayFixed[6], texCoordArrayFixed[7]);  
#endif

    glEnableVertexAttribArray(mCompositor->texCoordLoc());
    glVertexAttribPointer(mCompositor->texCoordLoc(), 2, GL_FLOAT, GL_FALSE, 0, (const void*)texCoordArrayFixed);

    //loc binded in QMrofsScreen
    return 0;
}

int QMrOfsBackingStore::setTexCoordsScrolled(const QRect &rect)
{
    //since rounding width, use realSize to obtain correct texture coord in texture coordinate system, win.geometry not worked any more
    GLfloat x0 = (rect.left() + (rect.left() * 1.0 / this->realSize.width()))* 1.0  / this->realSize.width();               //left
    GLfloat x1 = (rect.right() + (rect.right() * 1.0 / this->realSize.width()))* 1.0 / this->realSize.width();              //right
    GLfloat y0 = (rect.top() + (rect.top() *1.0 / this->realSize.height()))* 1.0 / this->realSize.height();               //top
    GLfloat y1 = (rect.bottom() + (rect.bottom() * 1.0 / this->realSize.height()))* 1.0 / this->realSize.height();         //bottom

    int i = 0;
    //bottom-left, top-left, bottom-right, top-right
#if 1
    texCoordArrayScrolled[i++] = x0;
    texCoordArrayScrolled[i++] = y0;   //y0 is top, ==0
    texCoordArrayScrolled[i++] = x0;
    texCoordArrayScrolled[i++] = y1;   //y1 is bottom, ==1,   texture: from 0 to 1 (in goles2 coordinate, from bottom to top)
    texCoordArrayScrolled[i++] = x1;
    texCoordArrayScrolled[i++] = y0; 
    texCoordArrayScrolled[i++] = x1;
    texCoordArrayScrolled[i++] = y1; 
#else
    texCoordArrayScrolled[i++] = 0.0f;
    texCoordArrayScrolled[i++] = 0.0f;
    texCoordArrayScrolled[i++] = 0.0f;
    texCoordArrayScrolled[i++] = 1.0f;
    texCoordArrayScrolled[i++] = 1.0f;
    texCoordArrayScrolled[i++] = 0.0f;
    texCoordArrayScrolled[i++] = 1.0f;
    texCoordArrayScrolled[i++] = 1.0f;
#endif

    //trace it
#ifdef MROFS_TRACE_BS_COORDINATE        
    qWarning("[!QMrOfsBackingStore::setTexCoords]");    
    qWarning("<\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", 
        texCoordArrayScrolled[0], texCoordArrayScrolled[1], texCoordArrayScrolled[2], texCoordArrayScrolled[3], texCoordArrayScrolled[4], texCoordArrayScrolled[5], texCoordArrayScrolled[6], texCoordArrayScrolled[7]);  
#endif

    glEnableVertexAttribArray(mCompositor->texCoordLoc());
    glVertexAttribPointer(mCompositor->texCoordLoc(), 2, GL_FLOAT, GL_FALSE, 0, (const void*)texCoordArrayScrolled);

    //loc binded in QMrofsScreen
    return 0;
}

#endif //MROFS_USE_GETSCROLLRECT

QT_END_NAMESPACE

