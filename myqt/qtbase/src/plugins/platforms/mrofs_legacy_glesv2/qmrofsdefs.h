#ifndef QMROFS_DEFS_H
#define QMROFS_DEFS_H
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/kd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <linux/fb.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <cmem.h>

/*!
    \brief qDebug controller, must defined before all qt headers file included in this file
    \note, all other files include this header file must put this file ahead of all qt header files also.
*/
#ifndef QT_NO_DEBUG_OUTPUT
#define  QT_NO_DEBUG_OUTPUT
#endif

#include <qdebug.h>
#include <qimage.h>

#define MROFS_USE_32BIT
#ifndef MROFS_USE_32BIT
#define MROFS_USE_16BIT 
#endif

/*!
    \brief  profiling  
*/
//#define MROFS_PROFILING
#ifdef MROFS_PROFILING
#include <sys/time.h>
extern unsigned long tv_diff(struct timeval *tv1, struct timeval *tv2);
#endif

/*!
    \brief use GL_OES_EGL_image_external extension which is provided by khronos(according to EGLv1.4 spec) and conflicts with GL_IMG_texture_stream2
    \note 
    good: standard EGLv1.4 spec
    bad:  EGL_PIXMAP_BIT must be set to EGL_SURFACE_TYPE which is conflicts with EGL double buffering rendering mode (FLIP), 
    while FLIP MODE is imperative for resolving screen tearing during animation(Qt's or ourselves both) and system performance
*/
//#define MROFS_EGLIMAGE_EXTERNAL

/*
    \brief use GL_IMG_texture_stream2 extension which is provided by PVR and conflicts with GL_OES_EGL_image_external extension
    \note
    good: EGL double buffering is OK
    bad:  wired 10 bccatX devices and not compatible with EGLv1.4 spec which means doing hard work again in BIGDIPPER 
    \note
    as for DUBHE, we have no choice but GL_IMG_texture_stream2
*/
#ifndef MROFS_EGLIMAGE_EXTERNAL
#define MROFS_TEXTURE_STREAM2
#endif

/* \brief  2 ways to eliminating screen tearing under double-buffer fb configuration
    MROFS_TEARING_ELIMINATION_COPY: copy backingstore's dirty region before composition, better effects, higher cpu(1~2%@600MHz@AM3358)
    MROFS_TEARING_ELIMINATION_DELAY: delay 10ms after composition, poor effects, lower cpu
*/
#define MROFS_TEARING_ELIMINATION_COPY
#ifdef MROFS_TEARING_ELIMINATION_COPY

//bs must be locked if is updated in ui thread, and NOTE: QMrOfs::flushbackingtore must wait until bs updating is done!!!
//#define MROFS_UPDATING_BS_IN_UI_THREAD

//lock is free if bs is updated in compositor thread
#define MROFS_UPDATING_BS_IN_COMP_THREAD

//for test:
//#  define WHOLE_BS_BLITTING    
#else
#define MROFS_TEARING_ELIMINATION_DELAY
#endif

/*
    \brief, alway compositing the whole backingstore's texture(via gpu); always updating the partial texture(via qrasterpaintengine)
    \note slightly higher gpu, lower coordinates precision lost
    \note always define it
*/
#define MROFS_WHOLE_BACKINGSTORE_COMPOSITING

/*
    \brief syncronized compositing
*/
//#define MROFS_SYNC_COMPOSITING

/*
    \brief swap buffer for every drawing unit
    \note debug use ONLY
*/
//#define MROFS_SINGLE_STEP_SWAP

/*
    \brief cooridnates debugging
*/
//#define MROFS_TRACE_WV_COORDINATE
//#define MROFS_TRACE_BS_COORDINATE

 //按块flush，会多flush 一点点(waveform 取整行，比vpt开窗2端多一些)，但系统调用次数较少，cpu开销最低
 //对于界面层，是脏区域和屏幕的交集
 //对于波形层，是波形和屏幕的交集(波形没有传脏区域)
#define MROFS_FLUSH_CACHE_BY_BLOCK         
//按行flush，会少一点点flush(只刷 waveviewport & screen & waveform 区域)，但系统调用次数较多，cpu开销较低
#ifndef MROFS_FLUSH_CACHE_BY_BLOCK
#define MROFS_FLUSH_CACHE_BY_ROW
#endif

/*!
    \brief  scrolling acceleration
*/
#define MROFS_USE_GETSCROLLRECT
#define MROFS_USE_SCRECTS
#ifdef MROFS_USE_SCRECTS
    #ifndef MROFS_USE_GETSCROLLRECT
        #define MROFS_USE_GETSCROLLRECT
    #endif
#endif

QT_BEGIN_NAMESPACE

//{{{
#ifdef MROFS_EGLIMAGE_EXTERNAL
typedef void *EGLImageKHR;
typedef void* GLeglImageOES;
typedef void (*PFNGLEGLIMAGETARGETTEXTURE2DOES) (GLenum target, GLeglImageOES image);
typedef EGLImageKHR (*PEGLCREATEIMAGEKHR)(EGLDisplay eglDpy, 
			EGLContext eglContext, EGLenum target, EGLClientBuffer buffer, EGLint *pAttribList);
typedef EGLBoolean (*PEGLDESTROYIMAGE)(EGLDisplay eglDpy, EGLImageKHR eglImage);
#define EGL_NATIVE_PIXMAP_KHR					0x30B0	/* eglCreateImageKHR target */
#define NPS_INVALID      -1
#define NPS_RGB565       0
#define NPS_RGBA8888   2       /*seems to be BGRA*/
#elif defined(MROFS_TEXTURE_STREAM2)       //MROFS_TEXTURE_STREAM2
typedef void (GL_APIENTRYP PFNGLTEXBINDSTREAMIMGPROC) (GLint device, GLint deviceoffset);
typedef const GLubyte *(GL_APIENTRYP PFNGLGETTEXSTREAMDEVICENAMEIMGPROC) (GLenum target);
typedef void (GL_APIENTRYP PFNGLGETTEXSTREAMDEVICEATTRIBUTEIVIMGPROC) (GLenum target, GLenum pname, GLint *params);
#define BC_PIX_FMT_INVALID                -1
#define BC_PIX_FMT_ARGB8888             BC_FOURCC('A', 'R', 'G', 'B')        /*< ARGB8888*/
#define GL_TEXTURE_STREAM_IMG                                                 0x8C0D      //texture target
#define GL_TEXTURE_NUM_STREAM_DEVICES_IMG                        0x8C0E     
#define GL_TEXTURE_STREAM_DEVICE_WIDTH_IMG                      0x8C0F
#define GL_TEXTURE_STREAM_DEVICE_HEIGHT_IMG                     0x8EA0     
#define GL_TEXTURE_STREAM_DEVICE_FORMAT_IMG                    0x8EA1      
#define GL_TEXTURE_STREAM_DEVICE_NUM_BUFFERS_IMG          0x8EA2     
#define BCIORECONFIGURE_BUFFERS     BC_IOWR(5)
//debug texture stream2
#define MROFS_TEXTURE_STREAM2_DEBUG 1
#endif  //MROFS_EGLIMAGE_EXTERNAL
//}}}

typedef struct _NATIVE_PIXMAP_STRUCT
{
#ifdef MROFS_EGLIMAGE_EXTERNAL
    long ePixelFormat;          //for eglImage
#elif defined(MROFS_TEXTURE_STREAM2)
    int   fourcc;                      //for imgstream
#endif
    long eRotation;
    long lWidth;
    long lHeight;
    long lStride;
    long lSizeInBytes;
    long pAddress;
    long vAddress;
}NATIVE_PIXMAP_STRUCT;

void qCreateNativePixmap(QImage::Format format,
                                        unsigned int width,
                                        unsigned int height,
                                        NATIVE_PIXMAP_STRUCT *pPixmap,
                                        QSize &reqSize,
                                        QSize &realSize);                              //RGB565 or ARGB8888 only
void qInvalidateNativePixmap(NATIVE_PIXMAP_STRUCT *pPixmap);                            
bool qIsNativePixmapValid(NATIVE_PIXMAP_STRUCT *pPixmap);
void qDestroyNativePixmap(NATIVE_PIXMAP_STRUCT *pPixmap);                   
int qWritebackNativePixmap(void *ptr, size_t sz, void*blockAddr, size_t blockSz);

#ifdef MROFS_EGLIMAGE_EXTERNAL
int   qCreateEGLImageTexture(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID);    
void qDestroyEGLImageTexture(GLuint texid);        
#elif defined(MROFS_TEXTURE_STREAM2)
//#define MROFS_CREATEIMGSTREAMTEXTURE_REOPEN
#ifndef MROFS_CREATEIMGSTREAMTEXTURE_REOPEN 
#define MROFS_CREATEIMGSTREAMTEXTURE_RECONFIGURE
#endif
#ifdef MROFS_CREATEIMGSTREAMTEXTURE_REOPEN
int qCreateIMGStreamTextureReopen(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID, int *pBcFd, int bcID, int bcBfIdx);
#elif defined(MROFS_CREATEIMGSTREAMTEXTURE_RECONFIGURE)
int qCreateIMGStreamTextureReconfigure(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID, int bcFd, int bcBfIdx);
#endif
void qDestroyIMGStreamTexture(GLuint texid);
#endif

#define VERTEX_ARRAY               0
#define TEXCOORD_ARRAY          1

#define CHK_EGL(item)  \
    do{ \
        GLint err = eglGetError(); \
        if(EGL_SUCCESS != err) {   \
            qFatal(#item" FAILED: 0x%x(%d)", err, err);  \
            exit(-1);  \
        } \
    } while(0);

QT_END_NAMESPACE

#endif
