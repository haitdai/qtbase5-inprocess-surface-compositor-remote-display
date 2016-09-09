#ifndef QMROFSBACKINGSTORE_P_H
#define QMROFSBACKINGSTORE_P_H
#include "qmrofsdefs.h"
#include <qpa/qplatformbackingstore.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <cmem.h>
#include <qmutex.h>

QT_BEGIN_NAMESPACE

class QMrOfsCompositor;
//class QMrOfsWindow;
class QWindow;

class QMrOfsBackingStore : public QPlatformBackingStore
{
public:
    QMrOfsBackingStore(QWindow *window);
    ~QMrOfsBackingStore();
    /*!
        \note ui render needs NOT to lock this device
        \sa QMrOfsCompositor::doComposite
    */
    virtual QPaintDevice *paintDevice();
    virtual void flush(QWindow *window, const QRegion &region, const QPoint &offset);
    virtual void resize(const QSize &size, const QRegion &region);
    void beginPaint(const QRegion &);
    void endPaint();
    const QImage image();

    GLuint texID();                                                   /*< NOT locking to avoid compositor waiting ui */
    int setVertices(const QRect &rect);                     /*< NOT locking to avoid compositor waiting ui */
    void unSetVertices();                                    /*< NOT locking to avoid compositor waiting ui */
    int   setTexCoords(const QRect &rect);          /*< NOT locking to avoid compositor waiting ui */        
    void unSetTexCoords();                              /*< NOT locking to avoid compositor waiting ui */

    //DHT 20141124, fixed + scrolled 
#ifdef MROFS_USE_GETSCROLLRECT
    int setVerticesFixed(const QRect &rect);
    int setVerticesScrolled(const QRect &rect);
    int setTexCoordsFixed(const QRect &rect);
    int setTexCoordsScrolled(const QRect &rect);    
#endif   //MROFS_USE_GETSCROLLRECT

    void updateTexture();
    void lock();                                                     
    void unlock();
protected:
//    friend class QMrOfsWindow;
public:
    QImage                                                 mImage;                  //backing store content, in cmem, for composition
    QImage                                                 mImageUI;              //backing store content, in system memory, for ui drawing
    QMutex                                                   m_bsLock;               //uImageUI's procetor: ui thread writing; compositor thread reading, non-recursive for default    
private:    
    GLfloat                                                    vertexArray[4*3];      // 4 vertex for a rectangle,  3 GLfloat each vertex, accessed by compositor thread ONLY
    GLfloat                                                    texCoordArray[4*2];  // 4 vertex for a texture, 2 GLfloat each vertex(2D texture), accessed by compositor thread ONLY
//DHT20141124, GetScrollRect
#ifdef MROFS_USE_GETSCROLLRECT
    GLfloat                                                    vertexArrayFixed[4*3];
    GLfloat                                                    vertexArrayScrolled[4*3];
    GLfloat                                                    texCoordArrayFixed[4*2];
    GLfloat                                                    texCoordArrayScrolled[4*2];
#endif //MROFS_USE_GETSCROLLRECT    
//END DHT
    GLuint                                                     texid;                       // use it to docomposite       
    NATIVE_PIXMAP_STRUCT                         nativePixmap;
    QSize                                                      reqSize;                       // backingstore size used by Qt
    QSize                                                      realSize;                      // divided by 4 in width
    QMrOfsCompositor                                 *mCompositor;    
    QRegion                                                m_dirty;    
#ifdef MROFS_PROFILING
    struct timeval                                         m_startTime;
    struct timeval                                         m_endTime;
    unsigned long                                          m_diffTime;
#endif
    friend class QMrOfsCompositor;
};

QT_END_NAMESPACE

#endif // QFBBACKINGSTORE_P_H

