#ifndef QMROFSWAVEFORM_H
#define QMROFSWAVEFORM_H
#include "qmrofsdefs.h"
#include <QtGui/qmrofs.h>

#ifdef MROFS_USE_GETSCROLLRECT
#include <QtGui/ScreenScrollController.h>
#endif

QT_BEGIN_NAMESPACE 
class QMrOfsScreen;
class QMrOfsWaveview;
class QMrOfsWaveController;
class QMrOfsCompositor;
/*!
    \brief waveform in texture range
    \note 1 view 1 layer
*/    
class QMrOfsWaveform
{
private:
#ifdef MROFS_EGLIMAGE_EXTERNAL
    QMrOfsWaveform();
#elif defined(MROFS_TEXTURE_STREAM2)
    QMrOfsWaveform(QMrOfsCompositor *cmpr);
#endif
    ~QMrOfsWaveform();
    void resize(const QSize& sz);
    bool allocateData(EGLDisplay eglDisplay, QImage::Format format, const QSize& sz);
    void deallocateData();
    void flushData(QMrOfsWaveview* view, const QRegion& rgn, const QRect& rcScreen);
    QMrOfs::WV_ID wid();
    QWaveData&  data();
    QPoint move(const QPoint& pos);
    GLuint texID() { return texid; }
    int   setVertices(QMrOfsWaveview *view, const QRect& rcScreen, GLuint verteLoc);     
    void unSetVertices(GLuint vertexLoc);    
    int   setTexCoords(QMrOfsWaveview *view, const QRect& rcScreen, GLuint texCoordLoc); 
    void unSetTexCoords(GLuint texCoordLoc);
private:    
    QWaveData   data0; 
    int                 wid0;
    /*!
        (x,y): waveform's top-left in screen coordinate system(up-, down+, left-, right+), maybe negative value
        (width, height): waveform size
    */
    QRect            geometry;                        
    //texture controlling: 
    GLfloat                                                    vertexArray[4*3]; 
    GLfloat                                                    texCoordArray[4*2];
    GLuint                                                     texid;
    NATIVE_PIXMAP_STRUCT                         nativePixmap;
    //DHT
        NATIVE_PIXMAP_STRUCT                         nativePixmap2;
    //END
    QSize                                                      reqSize;
    QSize                                                      realSize;    
#ifdef MROFS_TEXTURE_STREAM2
    QMrOfsCompositor                                *compositor;
#endif
    friend class QMrOfsCompositor;
    friend class QMrOfsWaveview;
    friend class QMrOfsWaveController;
};

/*!
    \brief viewport in screen range
*/
class QMrOfsWaveview
{
private:
    QMrOfsWaveview();
    ~QMrOfsWaveview();
    void setViewport(const QRect& rc);
    QMrOfs::WV_ID vid();
    QRect vpt();
    void bindWaveform(QMrOfsWaveform* wf);
    void unbindWaveform(QMrOfsWaveform *wf);
    QMrOfs::WV_ID bindedWaveform(QMrOfsWaveform *wf);
    void deleteAllWaveforms(QMrOfsWaveController *wc);
    void flushAllWaveforms(const QRegion& rgn, const QRect& rcScreen);
    
private:
    QRect                                 vpt0;               //top/left/width/height, indicate the visible range or waveview in SCREEN COORDINATE SYSTEM (up-, down+, left-, right+)
    QList<QMrOfsWaveform*>   wfs;                 //forms binded to this view
    int                                      vid0;    
    int                                      layer;              //layer index
    friend class QMrOfsCompositor;                //for compositing
    friend class QMrOfsWaveController;
    friend class QMrOfsWaveform;
};

/*!
    \brief  compositing assistant
*/
class QMrOfsWaveController
{
private:
    QMrOfsWaveController();
    ~QMrOfsWaveController();    
    QMrOfsWaveview* openWaveview();
    void closeWaveview(QMrOfs::WV_ID vid);
    QMrOfsWaveview* waveview(QMrOfs::WV_ID vid);    
    QMrOfsWaveform* waveform(QMrOfs::WV_ID wid);
    QMrOfs::WV_ID setWaveviewLayer(QMrOfs::WV_ID vid, int layer);
    QMrOfsWaveview* waveformInView(QMrOfs::WV_ID wid);
#ifdef MROFS_EGLIMAGE_EXTERNAL    
    QMrOfsWaveform* addWaveform();
#elif defined(MROFS_TEXTURE_STREAM2)
    QMrOfsWaveform* addWaveform(QMrOfsCompositor *cmpr);
#endif
    void deleteWaveform(QMrOfsWaveform *wf);
    void unbindWaveform(QMrOfsWaveform *wf);  //for all views
    void flushWaves(const QRegion& rgn, const QRect& rcScreen);
    
private:
    //single index: sorted by layer, which means compositing can do faster but change layer slower
    QList<QMrOfsWaveview*>             wvs;             //all created wave views, sorted by layer
    QList<QMrOfsWaveform*>             wfs;              //all created wave forms, be waveform pool
    friend class QMrOfsCompositor; 
    friend class QMrOfsWaveview;
};

QT_END_NAMESPACE
    
#endif

