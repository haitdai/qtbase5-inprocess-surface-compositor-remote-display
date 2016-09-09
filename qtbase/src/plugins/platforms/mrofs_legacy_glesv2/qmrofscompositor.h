#ifndef QMROFSCOMPOSITOR_H
#define  QMROFSCOMPOSITOR_H
#include "qmrofsdefs.h"
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#ifdef MROFS_TEXTURE_STREAM2
#include <bc_cat.h>
#endif
#include "OGLES2Tools.h"
#include "qmrofscursor.h"
#include <QtGui/qmrofs.h>
#include <qpa/qplatformscreen.h>                                          //QPlatformCompositor
#include <qlist.h>
#include <QtCore/qnamespace.h>
#include <qthread.h>
#include <qtimer.h>
#include <QtCore/qmutex.h>

#ifdef MROFS_USE_GETSCROLLRECT
#include <QtGui/ScreenScrollController.h>
#endif

class QMrOfsScreen;
class QMrOfsWindow;
class QMrOfsBackingStore;
class QMrOfsCursor;
class QMrOfsWaveform;
class QMrOfsWaveview;
class QMrOfsWaveController;

QT_BEGIN_NAMESPACE
    
#define MROFS_BKCOLOR_VALUE            0xFFFFFFFF                //all whilte in argb/QRgb, according to cmem initialization
//#define MROFS_COLORKEY_VALUE          0xFFFF80C0                //pure red in argb, filled by app
#define MROFS_COLORKEY_VALUE          0xFF000008                //almost pure black in argb, filled by app

struct ShaderDescription
{
    const char  *pszVertShaderSrcFile;
    const char  *pszVertShaderBinFile;
    const char * pszVertShaderMemContent;
    const char  *pszFragShaderSrcFile;
    const char  *pszFragShaderBinFile;
    const char * pszFragShaderMemContent;
    const unsigned int ui32NumAttributes;
    const char  **pszAttributes;
};

struct Shader
{
    GLuint uiId;
    GLuint uiVertexShaderId;
    GLuint uiFragmentShaderId;
    GLuint uiOrientationMatrixLoc;
    GLuint uiVertexLoc;
    GLuint uiTexCoordLoc;        
    GLuint uiBkColorLoc;
};

struct BackingStoreShader : Shader
{
    GLuint uiColorKeyLoc;
};

typedef Shader WaveformShader;

class QMrOfsCompositor;
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
    QTimer *m_timer;    
};


/*!
    \brief implememtation of QPlatformCompositor
    \note public slots method are running in compositor thread(m_th)
*/
class QMrOfsCompositor : public QPlatformCompositor
{
    Q_OBJECT
 
public:
    /*!
        \note a screen - a compositor - a process (qt instance)
    */
    QMrOfsCompositor();    
    ~QMrOfsCompositor();
public Q_SLOTS:
    void bar() {};
    
public:
    //derived from QPlatformCompositor, for QPlatformCompositor/QMrOfs/app calling {{{
    //ui thread -> compositor thread, translated and invoked in QMrOfs
    Q_INVOKABLE QMrOfs::WV_ID openWaveview();
    //ui thread -> compositor thread
    Q_INVOKABLE void closeWaveview(QMrOfs::WV_ID vid);
    //ui thread -> compositor thread    
    Q_INVOKABLE QMrOfs::WV_ID setWaveviewLayer(QMrOfs::WV_ID vid, int layer);
    //ui thread -> compositor thread    
    Q_INVOKABLE void setWaveViewport(QMrOfs::WV_ID vid, const QRect& vpt);
    //ui thread -> compositor thread    
    Q_INVOKABLE QMrOfs::WV_ID createWaveform(QMrOfs::WV_ID vid);
    //ui thread -> compositor thread    
    Q_INVOKABLE void destroyWaveform(QMrOfs::WV_ID wid);
    //ui thread -> compositor thread    
    Q_INVOKABLE QWaveData* setWaveformSize(QMrOfs::WV_ID wid, const QSize& rc);
    //ui thread -> compositor thread    
    Q_INVOKABLE void bindWaveform(QMrOfs::WV_ID wid, QMrOfs::WV_ID vid);
    //app thread -> compositor thread    
    Q_INVOKABLE QWaveData* waveformRenderBuffer(QMrOfs::WV_ID wid);
    //ui thread -> compositor thread        
    Q_INVOKABLE QPoint setWaveformPos(QMrOfs::WV_ID wid, const QPoint& pos);
    //app thread -> compositor thread        
    Q_INVOKABLE void flushWaves(const QRegion& rgn);
    //app/ui thread -> compositor thread        
    Q_INVOKABLE QRgb getColorKey() const;
    //app/ui thread -> compositor thread
    Q_INVOKABLE bool enableComposition(bool enable);

    Q_INVOKABLE void lockWaveformRenderBuffer(QMrOfs::WV_ID id);
    Q_INVOKABLE void unlockWaveformRenderBuffer(QMrOfs::WV_ID id);
    // }}}
    
    //for QMrOfsScreen calling {{{
    QRect initialize(QMrOfsScreen *scrn);
    Q_INVOKABLE QRect initialize2(QMrOfsScreen *scrn);    
    void setScreenGeometry(const QRect& rc);
    Q_INVOKABLE void setScreenGeometry2(const QRect& rc);    
    void setScreenOrientation(Qt::ScreenOrientation ornt);
    Q_INVOKABLE void setScreenOrientation2(Qt::ScreenOrientation ornt);    
    QWindow *topLevelAt(const QPoint & p) const;
    Q_INVOKABLE QWindow *topLevelAt2(const QPoint & p) const;    
    QPlatformCursor *cursor();
    Q_INVOKABLE QPlatformCursor *cursor2();    
    /*!
        \note: NO LOCKING during composition, which means wave data or backingstore maybe changed in the process of compositing,
        but always 
    */
    void doComposite();     
    Q_INVOKABLE void doComposite2();         
    
    public Q_SLOTS:
        void doComposite3();
    // }}}

    //for QMrOfsBackingStore calling {{{
    void flushBackingStore(QMrOfsBackingStore *bs, QWindow *win, const QRegion& region = QRegion());
    Q_INVOKABLE void flushBackingStore2(QMrOfsBackingStore *bs, QWindow *win, const QRegion& region);    
    void resizeBackingStore(QMrOfsBackingStore *bs, const QSize &size);
    Q_INVOKABLE void resizeBackingStore2(QMrOfsBackingStore *bs, const QSize &size);    
    GLuint vertexLoc();
    GLuint texCoordLoc();
    /*
    void MakeCurrent();
    */
    //}}}
    
    //for QMrOfsWindow calling {{{
    Q_INVOKABLE void addWindow(QMrOfsWindow *window);    
    Q_INVOKABLE QWindow* addWindow2(QMrOfsWindow *window);
    void removeWindow(QMrOfsWindow *window);    
    Q_INVOKABLE QWindow* removeWindow2(QMrOfsWindow *window);      
    void raise(QMrOfsWindow *window);    
    Q_INVOKABLE QWindow* raise2(QMrOfsWindow *window);
    void lower(QMrOfsWindow *window);
    Q_INVOKABLE QWindow* lower2(QMrOfsWindow *window);    
private:
    void topWindowChanged(QWindow *);
    QWindow *topWindow() const;
public:        
    void addBackingStore(QMrOfsBackingStore *bs);
    Q_INVOKABLE void addBackingStore2(QMrOfsBackingStore *bs);    
    void setDirty(const QRect &rect);
    Q_INVOKABLE void setDirty2(const QRect &rect = QRect());    
    // }}}

    //{{{ assistant method called by QMrOfsWaveform/QMrOfsCompositor running in composition thread
#ifdef MROFS_TEXTURE_STREAM2
public:
    int createIMGStreamTextureforWv(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID);
private:    
    int createIMGStreamTextureforBs(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID);
    int bcDevID(int *pID);
    int bcDevIDWv();
    int bcDevIDBs();
    int bcDevIDAm();
    int bcDevFd(int id);    
#endif
    //}}}
    
private:
    void initializeWaveController();
    void initializeCMem();
#ifdef MROFS_TEXTURE_STREAM2    
    void initializeBcDev();
    int initializeBcDevX(unsigned int bcFmt, int width, int height, int numBufs, int fdIdx);
#endif    
    void initializeEGL(QMrOfsScreen *scrn);
    void initializeGLES2();
    void initializeCursor(QMrOfsScreen *scrn);    
    bool loadShader(CPVRTString* pErrorStr, const ShaderDescription &descr, Shader &shader);
    void loadShaders(CPVRTString* pErrorStr);
    void releaseShader(Shader &shader);
    void releaseResources();
    void deInitCMem();
#ifdef MROFS_TEXTURE_STREAM2    
    void deInitBcDev();
#endif
    int setVertices(const QRect &rect);
    int unSetVertices();
    void drawBackgroundSolidColor(const QRect &rect, unsigned int color = MROFS_BKCOLOR_VALUE);
    void drawBackingStoreLayer();
    void drawBackingStore(const QRect &target, QMrOfsBackingStore * bs, const QRect & source);
    void drawWaveformLayer();
    void drawWaveform(QMrOfsWaveform *wf, QMrOfsWaveview *wv);        
    void drawAnimationLayer();
    void drawCursor();
    
private:
    QMrOfsCompositorThread                  *m_th;                /*compositing thread */
    static QMrOfsCompositor                  *self;                   /*< singleton this */
    QMrOfsWaveController                      *m_wc;               /*<wave controller */
    QMutex                                             wc_lock; 
    
    EGLDisplay                                        m_display;        /*< EGL stuff */
    EGLSurface                                       m_surface;        /*< EGL stuff */
    EGLContext                                       m_ctx;               /*< EGL stuff */
    EGLConfig                                         m_cfg;               /*< EGL stuff */
    const EGLNativeDisplayType               m_nativeDisp;  /*< EGL stuff */
    PVRTMat4                                         m_orntMtrx;      /*< screen orientation matrix */
    PVRTVec4                                         m_ck;                 /*<color key in RGBA format */
    PVRTVec4                                         m_bk;                 /*< background color in RGBA format */
    //shades
    WaveformShader                 m_wfShdr;          /*< wave form shader */
    Shader                                m_bkShdr;          /*< background shader */
    BackingStoreShader             m_bsShdr;          /*< backingstore shader */

    QMrOfsScreen                  *m_primScrn;      /*< primary screen*/
    QRect                                  m_scrnRect;        /*< primary screen geometry */
    QImage::Format                  m_texFmt;           /*< primary screen texture format */
    GLfloat                                m_vertexArray[3*4];      /*< for screen background drawing*/    
    QList<QMrOfsWindow *>    m_winStack;       /*< window stack */
    QRegion                             m_repaintRgn;       /*< region in screen needs to update, all windows intersect it, useless in compositing scenario */
    
    //backingstores - useless
    QList <QMrOfsBackingStore*> m_backingStores;
    
#ifdef MROFS_TEXTURE_STREAM2    
#define MAX_BCCAT_DEVICES           2                                                  /* 3 */
    int                                                  m_bcFd[MAX_BCCAT_DEVICES];   /*> we do not know which layer use which id && fd */
    int                                                  m_bcIDWv;                                  /* [0, 2], fd index of wv, also bcdev id */
    int                                                  m_bcIDBs;                                   /* [0, 2], fd index of bs, also bcdev id */
//    int                                                  m_bcIDAm;                                  /* [0, 2], fd index of bs, also bcdev id */
    PFNGLTEXBINDSTREAMIMGPROC      m_glTexBindStreamIMG;
#endif

#ifdef MROFS_PROFILING
    struct timeval                                m_time;
    long                                 m_msecs;
    long                                 m_frames;
#endif

    // cursor
    QMrOfsCursor                       *m_csr;                    /*< not support cursor */

    bool                                       m_enabled;              /*< enabled composition */    
    bool                                       m_enabled_internal;
    Qt::ConnectionType                connType;              /*< BlockingQueuedConnection, caller always waiting */

#ifdef MROFS_USE_GETSCROLLRECT
    ScreenScrollController::ScRects  mainRects;
    ScreenScrollController::ScRects  waveRects;
#endif
};

QT_END_NAMESPACE

#endif
