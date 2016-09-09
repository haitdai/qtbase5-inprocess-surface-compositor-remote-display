#include "qmrofscompositor.h"
#include "qmrofswaveform.h"
#include "qmrofswindow.h"
#include "qmrofsbackingstore.h"
#include "qmrofsscreen.h"
#include "qmrofscursor.h"
#include <qpa/qwindowsysteminterface.h>
#if !defined(QT_NO_EVDEV) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK))
#include <QtPlatformSupport/private/qdevicediscovery_p.h>
#endif
#include <QtCore/qglobal.h>
#include <QtCore/qmetaobject.h>


#include <pthread.h>                  //posix real time thread
#include <sched.h>

QT_BEGIN_NAMESPACE

QMrOfsCompositorThread::QMrOfsCompositorThread(QObject* parent, QMrOfsCompositor *cmptr) : QThread(parent), compositor(cmptr), m_timer(NULL)
{

}

QMrOfsCompositorThread::~QMrOfsCompositorThread()
{

}
    
void QMrOfsCompositorThread::run()
{
    /* 
    QTimer's hightest freuency seems to be 30Hz
    if self refresh, uncomment this:
    */
#if 0
    QTimer timer;
    timer.setTimerType(Qt::PreciseTimer);
    timer.start(10);             //much faster than wave timer
    m_timer = &timer;
    connect(&timer, SIGNAL(timeout()), compositor, SLOT(doComposite3()), Qt::AutoConnection);   //should direct
#endif

#if 0
    this->setPriority(TimeCriticalPriority);
    this->setPriority(LowestPriority);
#endif
	
#if 0
    //SCHED POLICY
    struct sched_param sp;
    int target_priority = sched_get_priority_max(SCHED_FIFO) - 1;
    sp.sched_priority = target_priority;
    pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
#endif

    (void) exec();
}


/*!
    \note in compositor thread
*/
QMrOfs::WV_ID QMrOfsCompositor::openWaveview()
{
    //create a view and add to controller
    QMrOfsWaveview * wv = m_wc->openWaveview();
    if (wv == NULL)
        return -1;
    
    //return vid to app
    return wv->vid();

    //QWindowSystemInterface::handleNativeEvent(platformWindow->window(), d->m_eventType, &msg, &filterResult);		
}

/*!
    \note if app want a whole clean,  explicitly call destroy waveform
*/
void QMrOfsCompositor::closeWaveview(QMrOfs::WV_ID vid)
{
    m_wc->closeWaveview(vid);
}

QMrOfs::WV_ID QMrOfsCompositor::setWaveviewLayer(QMrOfs::WV_ID vid, int layer)
{
    return m_wc->setWaveviewLayer(vid, layer);
}

void QMrOfsCompositor::setWaveViewport(QMrOfs::WV_ID vid, const QRect& rc)
{
    QMrOfsWaveview *mv = m_wc->waveview(vid);
    mv->setViewport(rc);
    
    //do NOT trigger composition
}

QMrOfs::WV_ID QMrOfsCompositor::createWaveform(QMrOfs::WV_ID vid)
{
    QMrOfsWaveview *wv = m_wc->waveview(vid);
    if(!wv)
        return -1;
#ifdef MROFS_EGLIMAGE_EXTERNAL
    QMrOfsWaveform *wf = m_wc->addWaveform();
#elif defined(MROFS_TEXTURE_STREAM2)
    //create a waveform and add to controller, 1 fd 1 buffer(index is 0) currently
    QMrOfsWaveform *wf = m_wc->addWaveform(this);
#endif
    //bind to view of vid
    wv->bindWaveform(wf);
    //return wid to app
    return wf->wid();
}

void QMrOfsCompositor::destroyWaveform(QMrOfs::WV_ID wid)
{
    //do nothing, waveforms will be deleted automatically when wave view closed
    Q_UNUSED(wid);
}

QWaveData* QMrOfsCompositor::setWaveformSize(QMrOfs::WV_ID wid, const QSize& sz)
{
    qWarning("QMrOfsCompositor::setWaveformSize, size(%d,%d)", sz.width(), sz.height());
    QMrOfsWaveform *wf = m_wc->waveform(wid);
    if (wf == NULL)
        return NULL;
    wf->resize(sz);
    bool reallyAllocated = wf->allocateData(m_display, m_texFmt, sz);    

#ifdef MROFS_TEXTURE_STREAM2
//dirty patch: since waveform id is smaller than backingstore's, we should UNMAP backingstore's bc_dev memory also
//原因:
//TEXTURE STREAM2 扩展要求在首次map (发生在glbindtexstream中)较小 id 的纹理时，较大id的纹理全部被unmap了，否则目标纹理(较小id)将
//永远无法map，在QMrOfsCompositor中，较小id的纹理是波形纹理，因此在重建(导致umap)波形纹理后，立即重建bs纹理，
//保证在下一次合成时，界面纹理(较大id)始终处于unmap状态。
//限制:
//由于增加了1次纹理重建，因此texture stream2 比 eglimage 在切换横竖屏/界面重新布局时稍慢。
//注意:
//1, setWaveformSize 必须在界面线程调用
//2, 界面等待setWaveformSize 返回(保证界面QPainter中没有stale BackingStore)
//3, setWaveformSize 返回后，界面紧邻的下一次刷新必须为全屏刷新，保证new BackingStore 中的新内容全部更新到屏幕
# if 0
    if(reallyAllocated) {        
        int layer = m_winStack.size() - 1;
        qWarning("setWaveformSize, layer: %d", layer);
        for (int layerIndex = layer; layerIndex != -1; layerIndex--) {
            if (!m_winStack[layerIndex]->window()->isVisible())
                continue;
            QMrOfsBackingStore *bs = m_winStack[layerIndex]->backingStore();
            if (bs) {
                //QImage tmpImage(bs->mImage.size(), bs->window()->screen()->handle()->format());
                //tmpImage = bs->mImage.copy();                
                qWarning("setWaveformSize: automatically resizeBackingStore2");                
                resizeBackingStore2(bs, bs->reqSize);
                bs->mImage = QImage(reinterpret_cast<uchar*>(bs->nativePixmap.vAddress), 
                    bs->nativePixmap.lWidth, 
                    bs->nativePixmap.lHeight, 
                    bs->nativePixmap.lStride, 
                    bs->window()->screen()->handle()->format());
                //bs->mImage = tmpImage.copy();
            }
        }
    }
# else
    qWarning("setWaveformSize: NO automatically resizeBackingStore2");
    Q_UNUSED(reallyAllocated);
# endif
#else
    Q_UNUSED(reallyAllocated);
#endif    

    //波形调好后才打开合成:
    //上层必须保证setWaveformSize 在resizeBackingStore 之后，否则合成永远关闭[避免pvr map 错乱]。
    m_enabled_internal = true;
//    qWarning("setWaveformSize: m_enabled_internal true");

    qWarning("setWaveformSize: size(%d, %d)", sz.width(), sz.height());
    
    return &wf->data();
}

void QMrOfsCompositor::bindWaveform(QMrOfs::WV_ID wid, QMrOfs::WV_ID vid)
{
    //unbind first, from all possible views
    QMrOfsWaveform *wf = m_wc->waveform(wid);
    m_wc->unbindWaveform(wf);

    //re-bind to vid
    QMrOfsWaveview *wv = m_wc->waveview(vid);    
    wv->bindWaveform(wf);
}

QWaveData* QMrOfsCompositor::waveformRenderBuffer(QMrOfs::WV_ID wid)
{
    QMrOfsWaveform *wf = m_wc->waveform(wid);
    if (wf == NULL)
        return NULL;
    return &wf->data();
}

QPoint QMrOfsCompositor::setWaveformPos(QMrOfs::WV_ID wid, const QPoint& pos)
{
    QMrOfsWaveform *wf = m_wc->waveform(wid);
    QMrOfsWaveview * wv = m_wc->waveformInView(wid);
    if(!wv) {
        qFatal("illegal or unbinded wave form");
        return QPoint();
    }
    //qWarning("setWaveformPos: pos(%d, %d)", pos.x(), pos.y());
    //pos 是滚动位置(up-, down+, left-, right+)
    //tmp.setX(0 - pos.x() + wv->vpt().x());
    //tmp.setY(0 - pos.y() + wv->vpt().y());
    //update waveform position[waveview in waveform coordinate system]
    return wf->move(pos);
}

/*!
    \note
    rgn is the NEW dirty region between 2 wave flushes.
    rgn is in wave image's coordinate system.
*/
void QMrOfsCompositor::flushWaves(const QRegion& rgn)
{
    //MROFS_USE_GETSCROLLRECT 时，应该在合成前，set waveviewport 后 flush wave data
#ifndef MROFS_USE_GETSCROLLRECT
    //write wave data back to cmem main memory which can be read by GPU
    m_wc->flushWaves(rgn, m_scrnRect);
#endif

    //re-compositing
    doComposite2();
}

QRgb QMrOfsCompositor::getColorKey()  const
{
    return QRgb(MROFS_COLORKEY_VALUE);
}

bool QMrOfsCompositor::enableComposition(bool enable)
{
    bool old = m_enabled;
    m_enabled = enable;
    return old;
}

void QMrOfsCompositor::lockWaveformRenderBuffer(QMrOfs::WV_ID id)
{
    wc_lock.lock();
}

void QMrOfsCompositor::unlockWaveformRenderBuffer(QMrOfs::WV_ID id)
{
    wc_lock.unlock();
}

QRect QMrOfsCompositor::initialize(QMrOfsScreen *scrn)
{
    int at = this->metaObject()->indexOfMethod("initialize2(QMrOfsScreen*)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::initialize: indexOfMethod failed");
        return QRect();
    }
    QRect rc;
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_RETURN_ARG(QRect, rc), Q_ARG(QMrOfsScreen *, scrn));
    return rc;
}

QRect QMrOfsCompositor::initialize2(QMrOfsScreen *scrn)
{
    CPVRTString errStr;
    initializeWaveController();
    initializeEGL(scrn);
    loadShaders(&errStr);
    initializeGLES2();
    initializeCursor(scrn);
    m_primScrn = scrn;
    initializeCMem();
#ifdef MROFS_TEXTURE_STREAM2    
    initializeBcDev();
#endif
    return m_scrnRect;
}

void QMrOfsCompositor::initializeWaveController()
{   
    //m_wc has NOT thread affinity since NOT derived from QObject
    if(m_wc)
        return;
    m_wc = new QMrOfsWaveController();
    if(NULL == m_wc)
        qFatal("wave controller initialization failed");
}

void QMrOfsCompositor::initializeCMem()
{
    int i = CMEM_init();
    if (0 != i)
        qFatal("CMEM_init failed");
}

#ifdef MROFS_TEXTURE_STREAM2
void QMrOfsCompositor::initializeBcDev()
{
    // initialization result: each fd has 1 buffer which size is minimal aligned value defined in bc_cat
    for(int i = 0; i < MAX_BCCAT_DEVICES; i ++) {
        initializeBcDevX(BC_PIX_FMT_ARGB8888, 8, 8, 1, i);
    }
    this->m_glTexBindStreamIMG = (PFNGLTEXBINDSTREAMIMGPROC)eglGetProcAddress("glTexBindStreamIMG");  
    if (NULL == this->m_glTexBindStreamIMG) {
        qFatal("initializeBcDev: m_glTexBindStreamIMG extension no exist");
    } 
}

/*
    \note fdIdx starts from 0
*/
int QMrOfsCompositor::initializeBcDevX(unsigned int bcFmt, int width, int height, int numBufs, int fdIdx)
{
    char bcdev_name[] = "/dev/bccatX";
    bc_buf_params_t buf_param;
    BCIO_package ioctl_var;
        
    buf_param.width  = width;
    buf_param.height = height;
    buf_param.count  = numBufs;
    buf_param.fourcc = bcFmt;
    buf_param.type   = BC_MEMORY_USERPTR;
    
    bcdev_name[strlen(bcdev_name)-1] = '0' + fdIdx;
    errno = 0;
    if ((this->m_bcFd[fdIdx] = open(bcdev_name, O_RDWR|O_NDELAY)) == -1) {
        qFatal("initializeBcDevX: open %s failed[0x%x]", bcdev_name, errno);
    }
    
    //request buffers without allocating
    errno = 0;
    if (ioctl(this->m_bcFd[fdIdx], BCIOREQ_BUFFERS, &buf_param) != 0) {
        qFatal("initializeBcDevX: BCIOREQ_BUFFERS failed[0x%x]", errno);
    }
    
    //confirm request is OK
    errno = 0;
    if (ioctl(this->m_bcFd[fdIdx], BCIOGET_BUFFERCOUNT, &ioctl_var) != 0) {
        qFatal("initializeBcDevX: BCIOGET_BUFFERCOUNT failed[0x%x]", errno);
    }
    if (ioctl_var.output == 0) {
        qFatal("initializeBcDevX: no texture buffer available");
    }

    //phyaddr set later
	
    qWarning("initializeBcDevX: opened: %s, 0x%x, %d", bcdev_name, this->m_bcFd[fdIdx], fdIdx);
    return fdIdx;
}
#endif

void QMrOfsCompositor::initializeEGL(QMrOfsScreen *scrn)
{
    EGLint major, minor;

    m_display = eglGetDisplay(m_nativeDisp);
    CHK_EGL(eglGetDisplay);
    
    eglInitialize(m_display, &major, &minor);
    CHK_EGL(eglInitialize);
    
    eglBindAPI(EGL_OPENGL_ES_API);
    CHK_EGL(eglBindAPI);
    
    EGLint configAttribs[32], configNum;
    int i = 0;
    //buffer size(same as depth)
    configAttribs[i++] = EGL_BUFFER_SIZE;
    configAttribs[i++] = scrn->fbDepth();    /*!< according to fbdev, maybe EGL_DONT_CARE is better ?  */
    //depth/z buffer
    //configAttribs[i++] = EGL_DEPTH_SIZE;
    //configAttribs[i++] = 24;  /*! < pvr defaults */
    //stencil buffer
    //configAttribs[i++] = EGL_STENCIL_SIZE;
    //configAttribs[i++] = 8;    /*! < pvr defaults */
    //surface type
    configAttribs[i++] = EGL_SURFACE_TYPE;
#ifdef MROFS_EGLIMAGE_EXTERNAL
    configAttribs[i++] = EGL_WINDOW_BIT | EGL_PIXMAP_BIT;
#elif defined(MROFS_TEXTURE_STREAM2)
/*
    egl1.4 spec: EGL_SWAP_BEHAVIOR_PRESERVED_BIT must be set in eglconfig first if we want to set EGL_BUFFER_PRESERVED in egl attrib, 
    but sgx530 seems NOT support EGL_SWAP_BEHAVIOR_PRESERVED_BIT
*/
    configAttribs[i++] = EGL_WINDOW_BIT;            /*< texture stream does NOT need EGL_PIXMAP_BIT so we can do FLIP */
#endif
    //renderable type
    configAttribs[i++] = EGL_RENDERABLE_TYPE;
    configAttribs[i++] = EGL_OPENGL_ES2_BIT;
    //FSAA mode - ref. glSampleCoverage
    //configAttribs[i++] = EGL_SAMPLE_BUFFERS;
    //configAttribs[i++] = 1;    /*!< pvr defaults */
    ////configAttribs[i++] = EGL_SAMPLES;
    ////configAttribs[i++] = 4; /*! pvr defaults */
    //terminator
    configAttribs[i] = EGL_NONE;

    eglChooseConfig(m_display, configAttribs, &m_cfg, 1, &configNum);
    CHK_EGL(eglChooseConfig);
    if  (configNum < 1) {
        qFatal("no matching egl configuration\n");
        exit(-1);
    }
    
    m_surface = eglCreateWindowSurface(m_display, m_cfg, (void*) NULL, NULL);
    CHK_EGL(eglCreateWindowSurface);

    //if we are in flip mode
#ifdef MROFS_TEXTURE_STREAM2
    //preserv front buffer is slower than not, but, MAKE SURE back and front buffers are consistent(we DO)
    //there seems no difference in PVR's implementation
    //eglSurfaceAttrib(m_display, m_surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED);   //EGL_BUFFER_PRESERVED EGL_BUFFER_DESTROYED
#endif

    //context setting
    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };    
    m_ctx = eglCreateContext(m_display, m_cfg, EGL_NO_CONTEXT, contextAttribs);
    CHK_EGL(eglCreateContext);

    eglMakeCurrent(m_display, m_surface, m_surface, m_ctx);     /*< now compositor thread's current context is m_ctx */
    CHK_EGL(eglMakeCurrent);

    //texture format, NOT egl format
    m_texFmt = scrn->format();
    
    //geometry
    GLint width, height;
    eglQuerySurface(m_display, m_surface, EGL_WIDTH, &width);
    eglQuerySurface(m_display, m_surface, EGL_HEIGHT, &height);
    m_scrnRect = QRect(0, 0, width, height); 
    
    //color buffer background, actually rgba in [0, 1]
    glClearColor(m_bk.x, m_bk.y, m_bk.z, m_bk.w);
    
    //full screen gl
    glViewport(0, 0, m_scrnRect.width(), m_scrnRect.height());

    //swap interval has no effects on sgx530
#ifdef MROFS_EGLIMAGE_EXTERNAL
    eglSwapInterval(m_display, 1);         //front mode, useless
#elif defined(MROFS_TEXTURE_STREAM2)
    eglSwapInterval(m_display, 1);         //flip mode, non-zero to force vsync to work, ref OMAPLFBSwapHandler for detail, DHT 20140920
#endif
    CHK_EGL(eglSwapInterval);    
}

//filewrap output by hand
#if  0
static const char *_backingstore_vsh = "";
static const char *_backingstore_fsh = "";
#else
//background for backingstore
#ifndef  __background_basic_sh_cpp
#define  __background_basic_sh_cpp
#include "background_basic_vsh.cpp" 
#include "background_basic_fsh.cpp"
#endif // __background_basic_sh_cpp
//backingstore 
#ifndef  __backingstore_sh_cpp
#define  __backingstore_sh_cpp
#include "backingstore_vsh.cpp" 
#include "backingstore_fsh.cpp"
#endif // __backingstore_sh_cpp
//waveform
#ifndef  __waveform_sh_cpp
#define  __waveform_sh_cpp
#include "waveform_vsh.cpp" 
#include "waveform_fsh.cpp"
#endif // __waveform_sh_cpp
#endif

//background for backingstore
const char *gBackgroundShaderAttribs[] = {"inVertex" /*<VERTEX_ARRAY*/};  //
const ShaderDescription gBackgroundDesc = { "background_basic.vsh",
                                                                        "background_basic.vsc",
                                                                        _background_basic_vsh,
                                                                        "background_basic.fsh",
                                                                        "background_basic.fsc",                                                               
                                                                        _background_basic_fsh,        
                                                                        sizeof(gBackgroundShaderAttribs)/sizeof(char*),
                                                                        (const char**)gBackgroundShaderAttribs                                                                  
                                                            
};
//backingstore
const char *gBackingStoreShaderAttribs[]          = { "inVertex" /*<VERTEX_ARRAY*/, "inTexCoord" /*< TEXCOORD_ARRAY*/};
const ShaderDescription gBackingStoreShaderDesc = { "backingstore.vsh",                 
                                                                        "backingstore.vsc",         
                                                                        _backingstore_vsh,             //in backingstore_vsh.cpp
                                                                        "backingstore.fsh",                 
                                                                        "backingstore.fsc", 
                                                                        _backingstore_fsh,             //in backingstore_fsh.cpp
                                                                        sizeof(gBackingStoreShaderAttribs)/sizeof(char*),  //  2
                                                                        (const char**)gBackingStoreShaderAttribs 
}; 
//waveform
const char * gWaveformShaderAttribs[]          = { "inVertex" /*<VERTEX_ARRAY*/, "inTexCoord" /*< TEXCOORD_ARRAY*/};
const ShaderDescription gWaveformShaderDesc = { "waveform.vsh",                 
                                                                        "waveform.vsc",         
                                                                        _waveform_vsh,             //in backingstore_vsh.cpp
                                                                        "waveform.fsh",                 
                                                                        "waveform.fsc", 
                                                                        _waveform_fsh,             //in backingstore_fsh.cpp
                                                                        sizeof(gWaveformShaderAttribs)/sizeof(char*),  //  2
                                                                        (const char**)gWaveformShaderAttribs 
};

/*!
    \brief 
    load shaders from cpp file which created by filewrap at compileing time.
    vertex/texure coordinates arrays are bound too.
*/
bool QMrOfsCompositor::loadShader(CPVRTString* pErrorStr, const ShaderDescription &descr, Shader &shader)
{
#if 0  /*< use source in memory instead of memory filesystem to speed up */
    if (PVRTShaderLoadFromFile(descr.pszVertShaderBinFile, descr.pszVertShaderSrcFile, GL_VERTEX_SHADER, GL_SGX_BINARY_IMG, &shader.uiVertexShaderId, pErrorStr) != PVR_SUCCESS) {
#else        
    if (PVRTShaderLoadSourceFromMemory(descr.pszVertShaderMemContent, GL_VERTEX_SHADER, &shader.uiVertexShaderId, pErrorStr) != PVR_SUCCESS) {
#endif        
        *pErrorStr = descr.pszVertShaderSrcFile + CPVRTString(":\n") + *pErrorStr;
        return false;
    }
#if 0    
    if (PVRTShaderLoadFromFile(descr.pszFragShaderBinFile, descr.pszFragShaderSrcFile, GL_FRAGMENT_SHADER, GL_SGX_BINARY_IMG, &shader.uiFragmentShaderId, pErrorStr) != PVR_SUCCESS) {
#else        
    if (PVRTShaderLoadSourceFromMemory(descr.pszFragShaderMemContent, GL_FRAGMENT_SHADER, &shader.uiFragmentShaderId, pErrorStr) != PVR_SUCCESS) {
#endif        
        *pErrorStr = descr.pszFragShaderSrcFile + CPVRTString(":\n") + *pErrorStr;
        return false;
    }

    //hint glBindAttribLocation
    if (PVRTCreateProgram(&shader.uiId, shader.uiVertexShaderId, shader.uiFragmentShaderId, descr.pszAttributes, descr.ui32NumAttributes, pErrorStr))
    {
        *pErrorStr = descr.pszFragShaderSrcFile + CPVRTString(":\n") + *pErrorStr;
        return false;
    }

    //additional to glBindAttribLocation
    shader.uiVertexLoc     = VERTEX_ARRAY;
    shader.uiTexCoordLoc = TEXCOORD_ARRAY;
    
    return true;
}

/*!
    \brief shaders must be pre-loaded in PVRTMemoryFileSystem in QMrOfsScreen's ctor
*/
void QMrOfsCompositor::loadShaders(CPVRTString* pErrorStr)
{
    //background shader without textures
    if (!loadShader(pErrorStr, gBackgroundDesc, m_bkShdr)) {
        qFatal("Load Shader FAILED: %s", pErrorStr->c_str());
    }
    m_bkShdr.uiOrientationMatrixLoc  = glGetUniformLocation(m_bkShdr.uiId, "orientationMatrix");
    m_bkShdr.uiBkColorLoc               = glGetUniformLocation(m_bkShdr.uiId, "bkColor");
    glUniformMatrix4fv(m_bkShdr.uiOrientationMatrixLoc, 1, GL_FALSE, m_orntMtrx.ptr());
    glUniform4fv(m_bkShdr.uiBkColorLoc, 1, m_bk.ptr());

    //backingstore shader with textures && colorkey
    if (!loadShader(pErrorStr, gBackingStoreShaderDesc, m_bsShdr)) {
        qFatal("Load Shader FAILED: %s", pErrorStr->c_str());
    }
    m_bsShdr.uiOrientationMatrixLoc  = glGetUniformLocation(m_bsShdr.uiId, "orientationMatrix");
    m_bsShdr.uiBkColorLoc               = glGetUniformLocation(m_bsShdr.uiId, "bkColor");                   /*< not used */    
    m_bsShdr.uiColorKeyLoc             = glGetUniformLocation(m_bsShdr.uiId, "colorKey");
    glUniformMatrix4fv(m_bsShdr.uiOrientationMatrixLoc, 1, GL_FALSE, m_orntMtrx.ptr());
    glUniform4fv(m_bsShdr.uiBkColorLoc, 1, m_bk.ptr());
    glUniform4fv(m_bsShdr.uiColorKeyLoc, 1, m_ck.ptr());
    glUniform1i(glGetUniformLocation(m_bsShdr.uiId, "sTexture"), 0);                                                /*<texture/sampler 0 in program backinstore(GL_TEXTURE0), see glActiveTexture */

    //waveform shader with textures && colorkey
    if (!loadShader(pErrorStr, gWaveformShaderDesc, m_wfShdr)) {
        qFatal("Load Shader FAILED: %s", pErrorStr->c_str());
    }
    m_wfShdr.uiOrientationMatrixLoc  = glGetUniformLocation(m_wfShdr.uiId, "orientationMatrix");
    m_wfShdr.uiBkColorLoc               = glGetUniformLocation(m_wfShdr.uiId, "bkColor");                    /*< not used */   
    glUniformMatrix4fv(m_wfShdr.uiOrientationMatrixLoc, 1, GL_FALSE, m_orntMtrx.ptr());                   /*< waveform rotating with screen orientation just like backingstore */
    glUniform4fv(m_wfShdr.uiBkColorLoc, 1, m_bk.ptr());                                                                     /*< same with backingstore */
    glUniform1i(glGetUniformLocation(m_wfShdr.uiId, "sTexture"), 1);                                                /*<texture/sampler 0 in program waveform(GL_TEXTURE1) see glActiveTexture */
}

void QMrOfsCompositor::initializeGLES2()
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);    
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);  
    //GL_DITHER is useful if we want smooth gradient effects  under 16bit fb (backingstore is 32bit), [default setting is Enabled]
    //it seems to use "minimal average error" method
    //glDisable(GL_DITHER);
}

GLuint QMrOfsCompositor::vertexLoc()
{
    return VERTEX_ARRAY;
}

GLuint QMrOfsCompositor::texCoordLoc()
{
    return TEXCOORD_ARRAY;
}

void QMrOfsCompositor::initializeCursor(QMrOfsScreen *scrn)
{
    //handle cursor stuff
    QByteArray hideCursorVal = qgetenv("QT_QPA_FB_HIDECURSOR");
#if !defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK)
    bool hideCursor = false;
#else
    bool hideCursor = true; // default to true to prevent the cursor showing up with the subclass on Android
#endif
    if (hideCursorVal.isEmpty()) {
#if !defined(QT_NO_EVDEV) && (!defined(Q_OS_ANDROID) || defined(Q_OS_ANDROID_NO_SDK))
        QScopedPointer<QDeviceDiscovery> dis(QDeviceDiscovery::create(QDeviceDiscovery::Device_Mouse));
        hideCursor = dis->scanConnectedDevices().isEmpty();
#endif
    } else {
        hideCursor = hideCursorVal.toInt() != 0;
    }
    if (!hideCursor)
        m_csr = new QMrOfsCursor(scrn, this);
}

/*!
    \note in UI thread, create by QMrOfsIntegration
*/
QMrOfsCompositor::QMrOfsCompositor() : m_th(0), m_wc(NULL),  wc_lock(QMutex::Recursive), m_display(0), m_surface(0), m_ctx(0), m_cfg(0), 
    m_nativeDisp(EGL_DEFAULT_DISPLAY), m_orntMtrx(PVRTMat4::Identity()), 
    m_winStack(QList<QMrOfsWindow *>()), m_backingStores(QList<QMrOfsBackingStore*>()), 
#ifdef MROFS_PROFILING
    m_msecs(0), m_frames(0),
#endif
    m_csr(NULL), m_enabled(false), m_enabled_internal(false), connType(Qt::BlockingQueuedConnection)
{
    m_ck = PVRTVec4(((MROFS_COLORKEY_VALUE>>16) & 0xFF) *1.0 / 255, ((MROFS_COLORKEY_VALUE>>8) & 0xFF) *1.0 / 255, ((MROFS_COLORKEY_VALUE) & 0xFF) *1.0 / 255,  1.0);  /*< rgba */
    m_bk = PVRTVec4(((MROFS_BKCOLOR_VALUE>>16) & 0xFF) *1.0 / 255, ((MROFS_BKCOLOR_VALUE>>8) & 0xFF) *1.0 / 255, ((MROFS_BKCOLOR_VALUE) & 0xFF) *1.0 / 255,  1.0);  /*< rgba */

#ifdef MROFS_TEXTURE_STREAM2
    m_bcFd[0] = m_bcFd[1] = -1;
    m_bcIDWv = m_bcIDBs = -1; //m_bcIDAm = -1;
#endif

    //move to compositor thread
    m_th = new QMrOfsCompositorThread(NULL, this);
    this->moveToThread(m_th);
    
    //run the compositor thread
    m_th->start();

#ifdef MROFS_PROFILING
    gettimeofday(&m_time, NULL);
#endif    
}

/*!
    \note FIXME!!!
    if we run this in compositor thread, event should NOT be proceeded after it
    if we run this in other thread after compositer thread ending, some functions such as CMem_exit maybe not worked propertly
*/
QMrOfsCompositor::~QMrOfsCompositor()
{
    releaseResources();
}

void QMrOfsCompositor::releaseShader(Shader &shader)
{
    glDeleteProgram(shader.uiId);
    glDeleteShader(shader.uiVertexShaderId);
    glDeleteShader(shader.uiFragmentShaderId);
}

void QMrOfsCompositor::deInitCMem()
{
    int i = CMEM_exit();    
    if (0 != i)
        qFatal("CMEM_exit failed");
}

#ifdef MROFS_TEXTURE_STREAM2
void QMrOfsCompositor::deInitBcDev()
{
    for (int i = 0; i < MAX_BCCAT_DEVICES; i ++) {
        close(this->m_bcFd[i]);
        this->m_bcFd[i] = -1;
    }
}
#endif

void QMrOfsCompositor::releaseResources()
{
    releaseShader(m_wfShdr);
    releaseShader(m_bsShdr);
    releaseShader(m_bkShdr);
#ifdef MROFS_TEXTURE_STREAM2    
    deInitBcDev();      
#endif
    deInitCMem();
    delete m_wc;
    delete m_csr;
}

/*! 
    \brief for screen ONLY
*/

int QMrOfsCompositor::setVertices(const QRect &rect) 
{
    int i = 0;
    //map coordinates into [-1, 1] in x,y axis
    const GLfloat x0 = 2 * ((rect.left() + (rect.left() * 1.0) /m_scrnRect.width()) * 1.0 / m_scrnRect.width()) - 1.0;                 //left
    const GLfloat x1 = 2 * ((rect.right() + (rect.right() * 1.0) /m_scrnRect.width())* 1.0 / m_scrnRect.width()) - 1.0;               //right
    const GLfloat y0 = 1 - ((rect.top() + (rect.top() * 1.0) /m_scrnRect.height())* 1.0 / m_scrnRect.height()) * 2.0;                //top
    const GLfloat y1 = 1 - ((rect.bottom() + (rect.bottom() * 1.0) /m_scrnRect.height())* 1.0 / m_scrnRect.height()) * 2.0;          //bottom
    //top-left, bottom-left, top-right, bottom-right
    m_vertexArray[i++] = x0;          //-1.0f;
    m_vertexArray[i++] = y0;          //1.0f;
    m_vertexArray[i++] = -1.0f;
    m_vertexArray[i++] = x0;          //-1.0f; 
    m_vertexArray[i++] = y1;          //-1.0f;
    m_vertexArray[i++] = -1.0f;
    m_vertexArray[i++] = x1;          //1.0f;
    m_vertexArray[i++] = y0;          //1.0f; 
    m_vertexArray[i++] = -1.0f;
    m_vertexArray[i++] = x1;          //1.0f; 
    m_vertexArray[i++] = y1;          //-1.0f;    
    m_vertexArray[i++] = -1.0f;  

    //trace it
#ifdef MROFS_TRACE_COORDINATE    
    qDebug("[!QMrOfsBackingStore::setVertices]\n");    
    qDebug("\t%f\t%f\t%f\t%f\t%f\t%f\t%f\t%f", 
        m_vertexArray[0], m_vertexArray[1], m_vertexArray[3], m_vertexArray[4], m_vertexArray[6], m_vertexArray[7], m_vertexArray[9], m_vertexArray[10]);
#endif    
        
    glEnableVertexAttribArray(vertexLoc());
    glVertexAttribPointer(vertexLoc(), 3, GL_FLOAT, GL_FALSE, 0, (const void*)m_vertexArray);
    return 0;
}

/*! 
    \brief for screen ONLY
*/
int QMrOfsCompositor::unSetVertices()
{
    glDisableVertexAttribArray(vertexLoc());
    return 0;
}


/*TODO:  remove it if not necessary */
/*! draw background of the backingstore for lowest layer : opt */
void QMrOfsCompositor::drawBackgroundSolidColor(const QRect &rect,  unsigned int color)
{
    //drawbackground seems not necessary
#ifdef MROFS_SINGLE_STEP_SWAP
    glUseProgram(m_bkShdr.uiId);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_bk = PVRTVec4(((color>>16)&0xFF)*1.0/255, ((color>>8)&0xFF)*1.0/255, (color&0xFF)*1.0/255, 1.0);    /*<rgba*/
    glUniform4fv(m_bkShdr.uiBkColorLoc, 1, m_bk.ptr());
    glUniformMatrix4fv(m_bkShdr.uiOrientationMatrixLoc, 1, GL_FALSE, m_orntMtrx.ptr());
    setVertices(rect);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    unSetVertices(); 
    eglSwapBuffers(this->m_display, this->m_surface);
#else
    Q_UNUSED(rect);
    Q_UNUSED(color);
#endif    
}

/*!
    \brief draw backingstore layer
    \note the number of backingstore is decided by the number of tlw(window/dialog/popup...), which maybe greater than 1, draw them from bottom to up
    \note color key is the KEY
*/
#include <private/qimage_p.h>
void QMrOfsCompositor::drawBackingStoreLayer()
{
    QPoint screenOffset = m_scrnRect.topLeft();
#ifdef MROFS_WHOLE_BACKINGSTORE_COMPOSITING    
    int layer = m_winStack.size() - 1;
    for (int layerIndex = layer; layerIndex != -1; layerIndex--) {
        if (!m_winStack[layerIndex]->window()->isVisible())
            continue;
        QRect windowRect = m_winStack[layerIndex]->geometry().translated(-screenOffset);       /*< in local screen coordinate*/
        QRect textureRect = windowRect.translated(-windowRect.left(), -windowRect.top());        /*< in window/bs coordinate*/        
        QMrOfsBackingStore *bs = m_winStack[layerIndex]->backingStore();
        if (bs) {
            qDebug("[QMrOfsCompositor::doComposite] bs image d %p, data %p, rect(%d,%d,%d,%d)", 
                bs->mImage.data_ptr(), bs->mImage.data_ptr()->data, windowRect.x(), windowRect.y(), windowRect.width(), windowRect.height());
            drawBackingStore(windowRect, bs, textureRect/*full texture drawing*/);
        }
    }
#else
    qFatal("NOT implemented since partial updating will NOT work correctly under compositing mode other than fb blitting mode.");
#endif
}

/*! \brief draw 1 backingstore's content to gl scanout which is fixed portrait
    \note sgx530 support max 8 texture units(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS), so we can use them all parallelly :  glActiveTexture(GL_TEXTUREx)
*/
/*!!!!DHT20141027, 在一次 flushBackingStore 中直接合成整个backingstore 可能和Qt 的语义不一致 !!!，其原有的语义是合成脏区域
改进:                                                                                                                                                                                                                                                                             
除屏幕bs 外，增加一个系统内存bs，ui 画在该bs 上。
在一次 flushBackingStore 中，合成器将bs 中的dirty region 拷贝到屏幕bs。
最后把屏幕bs 和波形合成。
*/
void QMrOfsCompositor::drawBackingStore(const QRect & target, QMrOfsBackingStore *bs, const QRect & source)
{
    //prolog
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);    
    
    //colorkey already OK
    //orientation matrix already OK

    glUseProgram(m_bsShdr.uiId);
    glUniformMatrix4fv(m_bsShdr.uiOrientationMatrixLoc, 1, GL_FALSE, m_orntMtrx.ptr());
    glUniform4fv(m_bsShdr.uiBkColorLoc, 1, m_bk.ptr());
    glUniform4fv(m_bsShdr.uiColorKeyLoc, 1, m_ck.ptr());
    glUniform1i(glGetUniformLocation(m_bsShdr.uiId, "sTexture"), 0);    
    glActiveTexture(GL_TEXTURE0); //unit0

#ifdef MROFS_EGLIMAGE_EXTERNAL
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, bs->texID());
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);   
#elif defined(MROFS_TEXTURE_STREAM2)
    if(bs->texID() == (GLuint)(-1) || this->m_bcIDBs == -1) {
        qWarning("!!!!!!!!!!!!!!!!!!!drawBackingStore, BACKINGSTORE DID NOT INITIALIZED, skip drawing");
        goto CLEAN_RET;
    }
//    qWarning("drawBackingStore, bcid: %d, texid: %d", this->m_bcIDBs, bs->texID());    

    /* NOW texture's backend is stream IMG */
    glBindTexture(GL_TEXTURE_STREAM_IMG, bs->texID());

    /* NOW stream IMG's backend is (this->m_bcIDBs, 0)
    trigger PVRSRVMapDeviceClassMemoryKM, ref. sgx kernel driver source for detail
    */
    this->m_glTexBindStreamIMG(this->m_bcIDBs, 0);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif    //!defined(MROFS_TEXTURE_STREAM2)
    
//DHT 20141124, GetScrollRect
#ifdef MROFS_USE_GETSCROLLRECT
    Q_UNUSED(target);
    Q_UNUSED(source);
#   ifdef MROFS_USE_SCRECTS
    //DHT 20141216, ScRects
    for(int i = 0; i < this->mainRects.m_count; i++) {
        bs->setVertices(this->mainRects.m_target[i]);
        bs->setTexCoords(this->mainRects.m_source[i]);
        
        //flushBackingStore already done
        
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
#   else
    //Fixed Area
    bs->setVerticesFixed(this->mainRects.m_target[0]);
    bs->setTexCoordsFixed(this->mainRects.m_source[0]);
    
    //flushBackingStore already done
    
    //qWarning("fixedRect: %d, %d, %d, %d", this->mainRects.m_target[0].x(), this->mainRects.m_target[0].y(), this->mainRects.m_target[0].width(), this->mainRects.m_target[0].height());        
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);     
    
    //Scrolled Area
    bs->setVerticesScrolled(this->mainRects.m_target[1]);                 //offset in screen
    //qWarning("scrolledVertexRect: %d, %d, %d, %d", this->mainRects.m_target[1].x(), this->mainRects.m_target[1].y(), this->mainRects.m_target[1].width(), this->mainRects.m_target[1].height());
    bs->setTexCoordsScrolled(this->mainRects.m_source[1]);         //offset in texture
    //qWarning("scrolledRect: %d, %d, %d, %d", scrolledRect.x(), scrolledRect.y(), scrolledRect.width(), scrolledRect.height());    

    //TODO: flushBackingStore!!!
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);        
#   endif    //MROFS_USE_SCRECTS
#else    //!defined(MROFS_USE_GETSCROLLRECT) 
    bs->setVertices(target);
    bs->setTexCoords(source);
    //backingstore has already flushed in QPlatformBackingStore::flush (Screen Range)
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);       
#endif  //MROFS_USE_GETSCROLLRECT

#ifdef MROFS_SINGLE_STEP_SWAP
    eglSwapBuffers(this->m_display, this->m_surface);    
#endif

//epilog
#ifdef MROFS_EGLIMAGE_EXTERNAL    
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
#elif defined(MROFS_TEXTURE_STREAM2)
CLEAN_RET:
    glBindTexture(GL_TEXTURE_STREAM_IMG, 0);
#endif     //MROFS_EGLIMAGE_EXTERNAL
    bs->unSetTexCoords();
    bs->unSetVertices();
}

//HINT: blend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) to line/points anti antialiasing
//HINT: blend(GL_SRC_ALPHA_SATURATE, GO_ONE) to polygon antialiasing


/*!
    \brief draw the waveform layer
    \wavefor posotion is relative to local screen(primary)'s top-left
*/
void QMrOfsCompositor::drawWaveformLayer()
{
    //draw all waveforms, bottom-up
    //TODO:  waveform fragment shader support overlapped waveview ...
    foreach(QMrOfsWaveview *wv, m_wc->wvs) {
        foreach(QMrOfsWaveform *wf, wv->wfs) {
            drawWaveform(wf, wv);
        }
    }
}

void QMrOfsCompositor::drawWaveform(QMrOfsWaveform *wf, QMrOfsWaveview *wv)
{
    //prolog
    glDisable(GL_BLEND);
    glUseProgram(m_wfShdr.uiId);
    glUniformMatrix4fv(m_wfShdr.uiOrientationMatrixLoc, 1, GL_FALSE, m_orntMtrx.ptr());
    glUniform4fv(m_wfShdr.uiBkColorLoc, 1, m_bk.ptr());
    glUniform1i(glGetUniformLocation(m_wfShdr.uiId, "sTexture"), 1);
    //orientation matrix already OK 
    //let's go 
    glActiveTexture(GL_TEXTURE1); //unit1

#ifdef MROFS_EGLIMAGE_EXTERNAL    
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, wf->texID());
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#elif defined(MROFS_TEXTURE_STREAM2)
    /*GL Libs seems did NOT check waveform texture's validation, so we did it before:
    */
    if(wf->texID() == (GLuint)(-1) || this->m_bcIDWv ==  -1) {
        qWarning("!!!!!!!!!!!!!!!!!!!drawWaveform, WAVEFORM DID NOT INITIALIZED, skip drawing");
        goto CLEAN_RET;
    }
//    qWarning("drawWaveform, bcid: %d, texid: 0x%x", this->m_bcIDWv, wf->texID());    
    glBindTexture(GL_TEXTURE_STREAM_IMG, wf->texID());
    /*
    trigger PVRSRVMapDeviceClassMemoryKM, ref. sgx kernel driver source for detail
    NOTE: if m_bcIDWv is in it's first-time mapping(such as REMAPPING), all bcdevs which bcid are greater than m_bcIDWv MUST keep UMAPPED before glTexBindStreamIMG(m_bcIDWv, ...) being called:
    */
    this->m_glTexBindStreamIMG(this->m_bcIDWv, 0);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_STREAM_IMG, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#endif    //MROFS_EGLIMAGE_EXTERNAL
    
#ifdef MROFS_USE_GETSCROLLRECT
#ifdef MROFS_USE_SCRECTS
    for(int i =0; i < this->waveRects.m_count; i ++) {
        setWaveViewport(this->waveRects.m_viewId, this->waveRects.m_target[i]);
        setWaveformPos(this->waveRects.m_waveId, 
            QPoint(this->waveRects.m_target[i].x() - this->waveRects.m_source[i].x(), 
                    this->waveRects.m_target[i].y() - this->waveRects.m_source[i].y()));
        
        //MROFS_USE_GETSCROLLRECT 时，在合成前，应该set waveviewport之后 flush wave data
        m_wc->flushWaves(QRegion(), m_scrnRect);

        wf->setVertices(wv, this->m_scrnRect, vertexLoc());
        wf->setTexCoords(wv, this->m_scrnRect, texCoordLoc());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);             
    }
#else   //!defined(MROFS_USE_SCRECTS)
    setWaveViewport(this->waveRects.m_viewId, this->waveRects.m_target[0]);
    setWaveformPos(this->waveRects.m_waveId, 
        QPoint(this->waveRects.m_target[0].x() - this->waveRects.m_source[0].x(), 
                this->waveRects.m_target[0].y() - this->waveRects.m_source[0].y()));
    
    //flushWaves!!!
    m_wc->flushWaves(QRegion(), m_scrnRect);
    
    wf->setVertices(wv, this->m_scrnRect, vertexLoc());
    wf->setTexCoords(wv, this->m_scrnRect, texCoordLoc());
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);     
#endif  //MROFS_USE_SCRECTS
#else    //!defined(MROFS_USE_GETSCROLLRECT)
    wf->setVertices(wv, this->m_scrnRect, vertexLoc());
    wf->setTexCoords(wv, this->m_scrnRect, texCoordLoc());
    //waveform has already flushed in QMrOfs::flushWaves
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); 
#endif   //MROFS_USE_GETSCROLLRECT

   
#ifdef MROFS_SINGLE_STEP_SWAP
    eglSwapBuffers(this->m_display, this->m_surface);    
#endif

    //epilog
#ifdef MROFS_EGLIMAGE_EXTERNAL    
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);
#elif defined(MROFS_TEXTURE_STREAM2)
CLEAN_RET:
    glBindTexture(GL_TEXTURE_STREAM_IMG, 0);
#endif        
    wf->unSetTexCoords(texCoordLoc());
    wf->unSetVertices(vertexLoc());
}

/*!
    \brief draw animation layer
*/
void QMrOfsCompositor::drawAnimationLayer()
{
    //TODO
}

void QMrOfsCompositor::drawCursor()
{
    //TODO m_csr
}

void QMrOfsCompositor::doComposite()
{
    int at = this->metaObject()->indexOfMethod("doComposite2()");
    if (at == -1) {
        qFatal("QMrOfsCompositor::doComposite: indexOfMethod failed");
        return;
    }
    
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, Qt::QueuedConnection);
}

/*!
    \brief composite all layers
    \return touched region
*/
void QMrOfsCompositor::doComposite2()
{
#if defined(MROFS_PROFILING)
    struct timeval startTime, endTime;
    unsigned long diffTime;
    gettimeofday(&startTime, NULL);    
    qWarning("MROFS_PROFILING: doComposite2 before: %ld:%ld", startTime.tv_sec, startTime.tv_usec);
#endif

    // 在切换横竖屏过程中(波形size调整 + 界面size调整)，上层必须保证合成开关是关闭的!!!      [双缓冲]
    //需要上层保证的原因是，波形和界面是分开设置大小的，合成器无法准确知道2个都ok的时机，但上层知道。
    if(!m_enabled)// || !m_enabled_internal)
        return;

    //lock bs
#ifdef MROFS_UPDATING_BS_IN_UI_THREAD
    if(0 < m_winStack.size()) {          //currently ONLY 1 bs is supported
        QMrOfsBackingStore *bs = m_winStack[0]->backingStore();    
        bs->lock();
    }
#endif

//lockWaveformRenderBuffer(0);

#ifdef  MROFS_USE_GETSCROLLRECT
    ScreenScrollController::Instance().GetScrollRects(this->mainRects, this->waveRects);
#if 0
    qWarning("doComposite2: bs rect count(%d), wave rect count(%d)", this->mainRects.m_count, this->waveRects.m_count);
    for(int i = 0; i < this->mainRects.m_count; i++) {
        qWarning("doComposite2: bs source rect (%d,%d,%d,%d), bs dest rect(%d,%d,%d,%d)", 
            this->mainRects.m_source[i].x(), this->mainRects.m_source[i].y(), this->mainRects.m_source[i].width(), this->mainRects.m_source[i].height(),
            this->mainRects.m_target[i].x(), this->mainRects.m_target[i].y(), this->mainRects.m_target[i].width(), this->mainRects.m_target[i].height());
    }
    for(int i = 0; i < this->waveRects.m_count; i++) {
        qWarning("doComposite2: wv source rect (%d,%d,%d,%d), wv dest rect(%d,%d,%d,%d)", 
            this->waveRects.m_source[i].x(), this->waveRects.m_source[i].y(), this->waveRects.m_source[i].width(), this->waveRects.m_source[i].height(), 
            this->waveRects.m_target[i].x(), this->waveRects.m_target[i].y(), this->waveRects.m_target[i].width(), this->waveRects.m_target[i].height());
    }  
#endif    
#endif

    drawWaveformLayer();
    drawBackingStoreLayer();
    drawAnimationLayer();

    //FACT: glFlush's NOT waiting GPU done its job which is not agree with as GLESv2 spec
    //glFinish() eglWaitGL() - useless; 
    //FACT1: in SGX530, waiting VSYNC means driver waiting for LCD DMA complementation interruption. and takes effect ONLY under FLIP MODE and swapIntervale is non-zeros
    //FACT2: driver waiting for LCD DMA complementaion DOES NOT MEAN eglSwapBuffers doing also, actually eglSwapBuffer never knows what VSYNC is, neither care it.
    //FACT3: eglSwapBuffers always returns much more earlier than LCD's job done.
    eglSwapBuffers(this->m_display, this->m_surface);   
    
    // 开启双缓冲后，仍偶尔有闪烁的原因:
    // 即使在双缓冲情形下，eglSwapBuffer  也不会在 lcdc dma 完成中断上等待，而是提前返回。
    //GLESv2 库实际上是以3缓冲形式绘制的，[实际上，即使打开双缓冲，实测的FPS也会突破60]
    //当front buffer 刷屏时，2 个back buffer 交替被eglSwapBuffer之前的绘制改写；这2个back buffer 之间的切换，和lcdc dma完成与否完全无关。
    //这里和普通OGLESv2程序的的一个不同之处在于，纹理内容会被改写，因此back buffer 0 正在被 defered 的绘制命令(上一次的)读取时，
    //它又被当前这一次的合成前在波形或ui线程中改写了。
    // 这里没办法知道上一次的绘制的精确完成时间，只能估计一个延时[绘制back buffer的时间]。保证上一次defered 绘制完成后，再启动本次对纹理的修改。
    // 波形和界面线程并行触发合成时，两次合成之间的时间间隔是随机的，延时20ms强制使得这个间隔 >= 20ms。
    //让单独的1个线程触发合成，也能解决。(实测demo有效，608M无效，报警更新方式待查)
    // 实测单层大约 40FPS不闪，双层大约 25FPS 不闪。
    QMrOfsCompositorThread::msleep(20);

	//unlockWaveformRenderBuffer(0);
    //unlock bs
#ifdef MROFS_UPDATING_BS_IN_UI_THREAD
    if(0 < m_winStack.size()) {
        QMrOfsBackingStore *bs = m_winStack[0]->backingStore();    
        bs->unlock();
    }
#endif

#if defined(MROFS_PROFILING)
    gettimeofday(&endTime, NULL);
    diffTime = tv_diff(&startTime, &endTime);
    qWarning("MROFS_PROFILING: doComposite2 after: %ld:%ld, diff: %ld", endTime.tv_sec, endTime.tv_usec, diffTime);

    //FPS
    static bool fpsDebug = qgetenv("QT_DEBUG_COMP_FPS").toInt();    
    if(fpsDebug) {
        m_frames ++;
        diffTime = tv_diff(&m_time, &endTime);
        if(diffTime > 5000) {
            qWarning("compositing FPS: %ld, in seconds(%ld), frames(%ld)", (m_frames*1000)/diffTime, diffTime/1000, m_frames);
            m_time = endTime;
            m_frames = 0;
        }
    }
#endif
}

/* slots version
*/
void QMrOfsCompositor::doComposite3()
{
    doComposite2();
}

/*
    \note WHAT A COMPLICATE PROBLEM!!! 
    ui, waveform and compositor are all in different threads, plus LCDC's dma reading, it's say, there are total 4 asyncronous executing,
    each of them can be the reason of screen tearing during animaiton/scrolling and any other situation where ui and waveform are updating simultaneously.
*/
void QMrOfsCompositor::flushBackingStore(QMrOfsBackingStore *bs, QWindow* win, const QRegion& region)
{
#if defined(MROFS_PROFILING)
    struct timeval startTime, endTime;
    unsigned long diffTime;
    gettimeofday(&startTime, NULL);    
    qWarning("MROFS_PROFILING: flushBackingStore before: %ld:%ld", startTime.tv_sec, startTime.tv_usec);
#endif

    int at = this->metaObject()->indexOfMethod("flushBackingStore2(QMrOfsBackingStore*,QWindow*,QRegion)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::flushBackingStore: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    
    //ui blocked here  this->connType, Qt::QueuedConnection, Qt::BlockingQueuedConnection, Qt::AutoConnection
    method.invoke(this, this->connType,
                            Q_ARG(QMrOfsBackingStore *, bs),
                            Q_ARG(QWindow*, win),
                            Q_ARG(QRegion, region));
    
#if defined(MROFS_PROFILING)
    gettimeofday(&endTime, NULL);    
    diffTime = tv_diff(&startTime, &endTime);
    qWarning("MROFS_PROFILING: flushBackingStore after: %ld:%ld, diff: %ld", endTime.tv_sec, endTime.tv_usec, diffTime);
#endif
    //double buffer FB is on the way ...
}

void QMrOfsCompositor::flushBackingStore2(QMrOfsBackingStore *bs, QWindow *win, const QRegion& region)
{
#if defined(MROFS_PROFILING)
    struct timeval startTime, endTime;
    unsigned long diffTime;
    gettimeofday(&startTime, NULL);    
    qWarning("MROFS_PROFILING: flushBackingStore2 before: %ld:%ld", startTime.tv_sec, startTime.tv_usec);
#endif

    if(!bs || !qIsNativePixmapValid(&bs->nativePixmap)) {
        qWarning("QMrOfsCompositor::flushBackingStore2: flusing backingstore before allocating nativePixmap! bs(0x%p)", bs);        
        return;
    }

#ifdef MROFS_UPDATING_BS_IN_COMP_THREAD
    bs->updateTexture();
#endif

#ifdef MROFS_FLUSH_CACHE_BY_BLOCK
    if(!region.isEmpty()) {                                   //flush dirty region's boundingRect
#ifdef MROFS_USE_GETSCROLLRECT
        //DHT 20141216 MROFS_USE_GETSCROLLRECT 时，应该允许flush 屏幕范围之外的部分，否则这部分永远没有机会被flushed
        QRect dirtyBoundingRect = region.boundingRect();
#else
        //DHT 20141125, no needs to modify for GetScrollRect
        //DHT 20141208, 旋转时region 可能超出bs  大小(如 480 * 480)，将它限制在屏幕 之内(此时屏幕方向和最新的bs保持一致)
        QRect dirtyBoundingRect = region.boundingRect();
        //转换到屏幕坐标系
        QRect tmpRectInScreen(win->geometry().left() + dirtyBoundingRect.left(),
                  win->geometry().top() + dirtyBoundingRect.top(),
                  dirtyBoundingRect.width(),
                  dirtyBoundingRect.height());

        //裁剪到屏幕范围    
        QRect dirtyRectInScreen = m_scrnRect.intersected(tmpRectInScreen);

        //转换回bs 坐标系
        dirtyBoundingRect = dirtyRectInScreen.translated(-win->geometry().left(), -win->geometry().top());
#endif //!defined(MROFS_USE_GETSCROLLRECT)
        
        //计算bs 中需要flush 的内存范围
        void * vAddr = reinterpret_cast<void*>(bs->nativePixmap.vAddress + bs->nativePixmap.lStride * dirtyBoundingRect.y()/* + bytesPP * dirtyBoundingRect.x()*/); //avoid nativePixmap's overrun
        size_t szInByte = bs->nativePixmap.lStride * dirtyBoundingRect.height();

        /*
        qWarning("flushbackingstore2: by block, nativePixmap(%p), width(%d), height(%d), vAddr(%p), size_x(%x), size_d(%d)", 
        bs->nativePixmap.vAddress, bs->nativePixmap.lWidth, bs->nativePixmap.lHeight, vAddr, szInByte, szInByte);
        qWarning("flushbackingstore2: by block, dirtyBoundingRect: %d, %d, %d, %d", 
            dirtyBoundingRect.x(), dirtyBoundingRect.y(), dirtyBoundingRect.width(), dirtyBoundingRect.height());
        */
        
        //flush cache
        qWritebackNativePixmap(vAddr, szInByte, reinterpret_cast<void*>(bs->nativePixmap.vAddress), bs->nativePixmap.lSizeInBytes);
    }
#elif defined(MROFS_FLUSH_CACHE_BY_ROW)    //flush each row in each rect in region(dirty)
        //TODO: 限制region 防止越界
        foreach (const QRect &rect, region.rects()) {
            if(rect.isEmpty()) 
                continue;
            //flush the intersection region of screen area and rgn(wave dirty region)
            //range: [start, end)
            int start_row = rect.y();
            int end_row = rect.y() + rect.height();
            int start_col = rect.x();
            int end_col = rect.x() + rect.width();
            int bytesPP = bs->nativePixmap.lStride / bs->nativePixmap.lWidth;                     
            void * vAddr;                 
            size_t szInByte = bytesPP * rect.width();
            for(int i = start_row; i < end_row; i ++) {    //by row 的方式，每行的起止地址不连续，因此要逐行处理
                vAddr = reinterpret_cast<void*>(bs->nativePixmap.vAddress + start_row * bs->nativePixmap.lStride + start_col * bytesPP);
                qWritebackNativePixmap(vAddr, szInByte, reinterpret_cast<void*>(bs->nativePixmap.vAddress), bs->nativePixmap.lSizeInBytes); 
            }
        }
#else
    qWritebackNativePixmap(reinterpret_cast<void*>(bs->nativePixmap.vAddress), bs->nativePixmap.lSizeInBytes, reinterpret_cast<void*>(bs->nativePixmap.vAddress), bs->nativePixmap.lSizeInBytes);
#endif //!defined(MROFS_FLUSH_CACHE_BY_BLOCK) && !defined(MROFS_FLUSH_CACHE_BY_ROW)

    setDirty2();                //trigger composition, not care about size since full-screen composition

#if defined(MROFS_PROFILING)
    gettimeofday(&endTime, NULL);    
    diffTime = tv_diff(&startTime, &endTime);
    qWarning("MROFS_PROFILING: flushBackingStore2 after: %ld:%ld, diff: %ld", endTime.tv_sec, endTime.tv_usec, diffTime);
#endif 
}

void QMrOfsCompositor::resizeBackingStore(QMrOfsBackingStore *bs, const QSize &size)
{
#ifdef MROFS_TEXTURE_STREAM2_DEBUG
    qWarning("QMrOfsCompositor::resizeBackingStore in ui thread, size(%d,%d)", size.width(), size.height());
#endif
    int at = this->metaObject()->indexOfMethod("resizeBackingStore2(QMrOfsBackingStore*,QSize)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::resizeBackingStore: indexOfMethod failed");
        return;
    }

    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_ARG(QMrOfsBackingStore*, bs), Q_ARG(const QSize&, size));
}

void QMrOfsCompositor::resizeBackingStore2(QMrOfsBackingStore *bs, const QSize &size)
{   
#ifdef MROFS_TEXTURE_STREAM2_DEBUG
    qWarning("QMrOfsCompositor::resizeBackingStore2: bs(%p), texid(%d), size(%d,%d), nativePixmap.vAddress(%p)", bs,
        bs->texid, size.width(), size.height(), reinterpret_cast<void *>(bs->nativePixmap.vAddress) );   
#endif

    if(reinterpret_cast<void *>(bs->nativePixmap.vAddress) != NULL) {
#ifdef MROFS_EGLIMAGE_EXTERNAL        
        qDestroyEGLImageTexture(bs->texid);
#elif defined (MROFS_TEXTURE_STREAM2)
        qDestroyIMGStreamTexture(bs->texid);
#endif
        qDestroyNativePixmap(&bs->nativePixmap);
    }
    bs->texid = -1;
    //create new one
    //allocate video mem
    qCreateNativePixmap(m_primScrn->format(), size.width(), size.height(), &bs->nativePixmap, bs->reqSize, bs->realSize);
    //for compositor,  note: create egl image and texture in ui thread and no context
#ifdef MROFS_EGLIMAGE_EXTERNAL
    qCreateEGLImageTexture(this->m_display, &bs->nativePixmap, &bs->texid);
#elif defined(MROFS_TEXTURE_STREAM2)
    this->createIMGStreamTextureforBs(this->m_display, &bs->nativePixmap, &bs->texid);
    m_enabled_internal = false;
    qWarning("resizeBackingStore2: m_enabled_internal false");
#endif
}

void QMrOfsCompositor::setScreenOrientation(Qt::ScreenOrientation ornt)
{
    int at = this->metaObject()->indexOfMethod("setScreenOrientation2(Qt::ScreenOrientation)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::setScreenOrientation2: indexOfMethod failed");
        return;
    }
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_ARG(Qt::ScreenOrientation, ornt));
}

void QMrOfsCompositor::setScreenOrientation2(Qt::ScreenOrientation ornt)
{
    //re-set screen geometry
    switch(ornt) {
        case Qt::PortraitOrientation:
            m_orntMtrx = PVRTMat4::Identity();
            break;
        case Qt::LandscapeOrientation:
            //rotate 90 degree anti-clockwised around Z axis
            m_orntMtrx = m_orntMtrx.RotationZ(-90.0f*PVRT_PIf/180.0f);
            break;
        case Qt::InvertedPortraitOrientation:
            //rotate 180 degree anti-clockwised around Z axis
            m_orntMtrx = m_orntMtrx.RotationZ(-180.0f*PVRT_PIf/180.0f);
            break;
        case Qt::InvertedLandscapeOrientation:
            //rotate 90 degree clockwised around Z axis
            m_orntMtrx = m_orntMtrx.RotationZ(90.0f*PVRT_PIf/180.0f);                   
            break;
        default:
            break;
    }
    glUniformMatrix4fv(m_bkShdr.uiOrientationMatrixLoc, 1, GL_FALSE, m_orntMtrx.ptr());
    glUniformMatrix4fv(m_bsShdr.uiOrientationMatrixLoc, 1, GL_FALSE, m_orntMtrx.ptr());    
    glUniformMatrix4fv(m_wfShdr.uiOrientationMatrixLoc, 1, GL_FALSE, m_orntMtrx.ptr());
}

void QMrOfsCompositor::setScreenGeometry(const QRect& rc)
{
    int at = this->metaObject()->indexOfMethod("setScreenGeometry2(QRect)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::setScreenGeometry: indexOfMethod failed");
        return;
    }
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_ARG(const QRect&, rc));
}

void QMrOfsCompositor::setScreenGeometry2(const QRect& rc)
{
    m_scrnRect = rc;
}

void QMrOfsCompositor::addWindow(QMrOfsWindow *window)
{
    int at = this->metaObject()->indexOfMethod("addWindow2(QMrOfsWindow*)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::addWindow: indexOfMethod failed");
        return;
    }
    QWindow* ret;
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_RETURN_ARG(QWindow*, ret), Q_ARG(QMrOfsWindow *, window));
    
    QWindowSystemInterface::handleWindowActivated(ret);
}

QWindow* QMrOfsCompositor::addWindow2(QMrOfsWindow *window)
{
    qDebug("[QMrOfsScreen::addWindow2] %p", window);
    if(m_winStack.size() >= 1) {
        //generating coredump
        qFatal("\n!!!ONLY 1 TLW IS SUPPORTED!!![existed(%p), new(%p)] \n", m_winStack[0], window);
    }
    m_winStack.prepend(window);
    if (!m_backingStores.isEmpty()) {
        for (int i = 0; i < m_backingStores.size(); ++i) {
            QMrOfsBackingStore *bs = m_backingStores.at(i);
            if (bs->window() == window->window()) {
                window->setBackingStore(bs); 
                m_backingStores.removeAt(i);
                break;
            }
        }
    }
    setDirty2(window->geometry());
    QWindow *w = topWindow();
    topWindowChanged(w);
    return w;
}

void QMrOfsCompositor::removeWindow(QMrOfsWindow *window)
{
    int at = this->metaObject()->indexOfMethod("removeWindow2(QMrOfsWindow*)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::removeWindow: indexOfMethod failed");
        return;
    }
    QWindow* ret;
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_RETURN_ARG(QWindow*, ret), Q_ARG(QMrOfsWindow *, window));
    
    QWindowSystemInterface::handleWindowActivated(ret);
}

QWindow* QMrOfsCompositor::removeWindow2(QMrOfsWindow *window)
{
    qDebug("[QMrOfsScreen::removeWindow] %p", window);
    m_winStack.removeOne(window);    
    setDirty2(window->geometry());
    QWindow *w = topWindow();
    topWindowChanged(w);
    return w;
}

void QMrOfsCompositor::raise(QMrOfsWindow *window)
{
    int at = this->metaObject()->indexOfMethod("raise2(QMrOfsWindow*)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::raise: indexOfMethod failed");
        return;
    }
    QWindow* ret;
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_RETURN_ARG(QWindow*, ret), Q_ARG(QMrOfsWindow *, window));
    
    QWindowSystemInterface::handleWindowActivated(ret);
}

QWindow* QMrOfsCompositor::raise2(QMrOfsWindow *window)
{
    qDebug("[QMrOfsScreen::raise] %p", window);
    int index = m_winStack.indexOf(window);
    if (index <= 0)
        return NULL;
    
    m_winStack.move(index, 0);
    setDirty2(window->geometry());
    QWindow *w = topWindow();
    topWindowChanged(w);
    return w;
}

void QMrOfsCompositor::lower(QMrOfsWindow *window)
{
    int at = this->metaObject()->indexOfMethod("lower2(QMrOfsWindow*)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::lower: indexOfMethod failed");
        return;
    }
    QWindow* ret;
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_RETURN_ARG(QWindow*, ret), Q_ARG(QMrOfsWindow *, window));
    
    QWindowSystemInterface::handleWindowActivated(ret);
}

/*
    lowest is in the bottom of stack
*/
QWindow* QMrOfsCompositor::lower2(QMrOfsWindow *window)
{
    qDebug("[QMrOfsScreen::lower] %p", window);
    int index = m_winStack.indexOf(window);
    if (index == -1 || index == (m_winStack.size() - 1))
        return NULL;
    m_winStack.move(index, m_winStack.size() - 1);
    setDirty2(window->geometry());
    QWindow *w = topWindow();
    topWindowChanged(w);
    return w;
}

QWindow *QMrOfsCompositor::topWindow() const
{
    QWindow *tlw = 0;
    foreach (QMrOfsWindow *w, m_winStack) {
        if (w->window()->type() == Qt::Window || w->window()->type() == Qt::Dialog)
            tlw = w->window();
    }
    qDebug("[QMrOfsScreen::topWindow] %p", tlw);    
    return tlw;
}

void QMrOfsCompositor::topWindowChanged(QWindow *) 
{
    //do nothing
}

QWindow *QMrOfsCompositor::topLevelAt(const QPoint & p) const
{
    int at = this->metaObject()->indexOfMethod("topLevelAt2(QPoint)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::topLevelAt: indexOfMethod failed");
        return NULL;
    }
    QWindow *ret;
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(const_cast<QMrOfsCompositor*>(this), this->connType, Q_RETURN_ARG(QWindow*, ret), Q_ARG(const QPoint&, p));
    return ret;
}

QWindow *QMrOfsCompositor::topLevelAt2(const QPoint & p) const
{
    QWindow *tlw = 0;
    foreach (QMrOfsWindow *w, m_winStack) {
        if (w->geometry().contains(p, false) && w->window()->isVisible())
            tlw = w->window();
    }
    qDebug("[QMrOfsScreen::topLevelAt] %p", tlw);
    return tlw;
}

void QMrOfsCompositor::addBackingStore(QMrOfsBackingStore *bs) 
{
    int at = this->metaObject()->indexOfMethod("addBackingStore2(QMrOfsBackingStore*)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::addBackingStore: indexOfMethod failed");
        return;
    }
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_ARG(QMrOfsBackingStore*, bs));
}

void QMrOfsCompositor::addBackingStore2(QMrOfsBackingStore *bs) 
{
    qDebug("addBackingStore2, bs: %p", bs);
    m_backingStores << bs;
}

void QMrOfsCompositor::setDirty(const QRect &rect) 
{
    int at = this->metaObject()->indexOfMethod("setDirty2(QRect)");
    if (at == -1) {
        qFatal("QMrOfsCompositor::setDirty: indexOfMethod failed");
        return;
    }
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_ARG(const QRect&, rect));
}

/*!
    \brief dirty the bounding rect of real dirty region.
    \note redraw range is larger than real dirty region since the former is the bounding rectangle of the latter.
    \note larger redraw region than real dirty region is nothing big deal with screen display(because whole bs contents is correct always) but a little more performance degeneration(in GPU). 
    \note since the difference of semantic between blitting and compositing, mRepaintRegion is useless in compositing scenario
*/
void QMrOfsCompositor::setDirty2(const QRect &rect)
{
    qDebug("[QMrOfsScreen::setDirty] rect(%d,%d,%d,%d)", rect.x(), rect.y(), rect.width(), rect.height());
    QRect intersection = rect.intersected(m_scrnRect);             /*< intersect in global coordinate */
    QPoint screenOffset = m_scrnRect.topLeft();                        /*< this screen in global coordinate */
    m_repaintRgn += intersection.translated(-screenOffset);         /*< translate to this screen coordinate(start from (0,0))*/
    doComposite2();                                                                    /*< compositing in compositor thread, whole range composited instead of repaint region */
}

QPlatformCursor *QMrOfsCompositor::cursor()
{
    int at = this->metaObject()->indexOfMethod("cursor2()");
    if (at == -1) {
        qFatal("QMrOfsCompositor::cursor: indexOfMethod failed");
        return NULL;
    }
    QPlatformCursor *ret;
    QMetaMethod method = this->metaObject()->method(at);
    method.invoke(this, this->connType, Q_RETURN_ARG(QPlatformCursor*, ret));
    return ret;
}

QPlatformCursor *QMrOfsCompositor::cursor2()
{
    return this->m_csr;
}

#ifdef MROFS_TEXTURE_STREAM2
int QMrOfsCompositor::bcDevID(int *pID)
{
    if(NULL == pID)
        return -1;
    
    if (-1 == *pID) {
//        *pID = qMax(qMax(this->m_bcIDWv, this->m_bcIDBs), this->m_bcIDAm) + 1;
        *pID = qMax(this->m_bcIDWv, this->m_bcIDBs) + 1;
    }
    return *pID;
}

int QMrOfsCompositor::bcDevIDBs()
{
    return (this->m_bcIDBs = 1);
}
int QMrOfsCompositor::bcDevIDWv()
{
    return (this->m_bcIDWv = 0);
//    return bcDevID(&this->m_bcIDWv);
}

int QMrOfsCompositor::bcDevIDAm()
{
    //return bcDevID(&this->m_bcIDAm);
    return -1;
}
    
int QMrOfsCompositor::bcDevFd(int id)
{
    if(id < 0 || id > MAX_BCCAT_DEVICES)
        return 0;
    
    return this->m_bcFd[id];
}

int QMrOfsCompositor::createIMGStreamTextureforWv(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID)
{
    int bcid = bcDevIDWv();
    int bcfd = bcDevFd(bcid);
    int ret = -1;

#ifdef MROFS_TEXTURE_STREAM2_DEBUG
    //let GPU finish the pending glTexBindStreamIMG
    qWarning("before createIMGStreamTextureforWv, into sleep");
#endif 

#ifdef MROFS_CREATEIMGSTREAMTEXTURE_REOPEN
    ret = qCreateIMGStreamTextureReopen(eglDisplay, pPixmap, pTexID, this->m_bcFd, this->m_bcIDWv, 0);
#elif defined(MROFS_CREATEIMGSTREAMTEXTURE_RECONFIGURE)
    ret = qCreateIMGStreamTextureReconfigure(eglDisplay, pPixmap, pTexID, bcfd, 0);
#endif

#ifdef MROFS_TEXTURE_STREAM2_DEBUG    
    qWarning("createIMGStreamTextureforWv: bcid(%d), bcfd(0x%x), texid(%d), vAddr(%p)", bcid, bcfd, *pTexID, pPixmap->vAddress);
#endif

    return ret;
}

int QMrOfsCompositor::createIMGStreamTextureforBs(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID)
{
    this->m_bcIDBs = bcDevIDBs();
    int bcfd = bcDevFd(this->m_bcIDBs);
    int ret = -1;

#ifdef MROFS_TEXTURE_STREAM2_DEBUG
    //let GPU finish the pending glTexBindStreamIMG
    qWarning("before createIMGStreamTextureforBs, %d", *pTexID);
#endif
    
#ifdef MROFS_CREATEIMGSTREAMTEXTURE_REOPEN
    ret = qCreateIMGStreamTextureReopen(eglDisplay, pPixmap, pTexID, this->m_bcFd, this->m_bcIDBs, 0);
#elif defined(MROFS_CREATEIMGSTREAMTEXTURE_RECONFIGURE)
    ret = qCreateIMGStreamTextureReconfigure(eglDisplay, pPixmap, pTexID, bcfd, 0);
#endif

#ifdef MROFS_TEXTURE_STREAM2_DEBUG    
    qWarning("createIMGStreamTextureforBs: bcid(%d), bcfd(0x%x), texid(%d), vAddr(%p)", this->m_bcIDBs, bcfd, *pTexID, pPixmap->vAddress); 
#endif
    return ret;
}

#endif

/***********************************************************************
                    native pixmap and eglimage global functions, declared in qmrofsdefs.h 
***********************************************************************/
void qCreateNativePixmap(QImage::Format format,
                                        unsigned int width,
                                        unsigned int height,
                                        NATIVE_PIXMAP_STRUCT *pPixmap,
                                        QSize &reqSize, QSize &realSize)
{
    size_t bpp = 0;
    //head
#ifdef MROFS_EGLIMAGE_EXTERNAL    
    if(format == QImage::Format_RGB16) {  //QImage::Format
        (pPixmap)->ePixelFormat = NPS_RGB565;
        bpp = 2;
    }
    else if(format == QImage::Format_RGB32) {
        (pPixmap)->ePixelFormat = NPS_RGBA8888;
        bpp = 4;
    }
#elif defined(MROFS_TEXTURE_STREAM2)
    if(format == QImage::Format_RGB16) {  //QImage::Format
        (pPixmap)->fourcc = BC_PIX_FMT_RGB565;
        bpp = 2;
    }
    else if(format == QImage::Format_RGB32) {
        (pPixmap)->fourcc = BC_PIX_FMT_ARGB8888;
        bpp = 4;
    }
#endif
    else           
    {  
        qFatal("qCreateNativePixmap: sInvalid pixmap format type %d\n", format);
        exit(-1);
    }

    //DHT, check reqSize
#if 0	
    if(width > 2048) {
        qWarning("texture width exceeded %d", width);
    }
    if(height > 2000) {
        qWarning("texture height exceeded %d", height);
    }        
#endif

    (pPixmap)->eRotation = 0;
    reqSize = QSize(width, height);
    realSize = reqSize;
    //NOTE: rounding WIDTH to 8 pixels
    int remainder = reqSize.width() % 8;
    realSize.setWidth(realSize.width() / 8 * 8 + (remainder == 0 ? 0 : 8));    
     (pPixmap)->lWidth = realSize.width();
    (pPixmap)->lHeight = height;
    
    if(format == QImage::Format_RGB16)  {
        (pPixmap)->lStride = pPixmap->lWidth* 16/8;
        qWarning("RGB16!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        }
    else if(format == QImage::Format_RGB32) {
        (pPixmap)->lStride = pPixmap->lWidth* 32/8;
        qWarning("RGB32!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        }        
    
    (pPixmap)->lSizeInBytes = (pPixmap)->lHeight * (pPixmap)->lStride;
    
    //payload
    CMEM_AllocParams params = CMEM_DEFAULTPARAMS;
    params.flags = CMEM_CACHED;
    
    (pPixmap)->vAddress = (long) CMEM_alloc((pPixmap)->lSizeInBytes, &params);
    if(!(pPixmap)->vAddress)
    {
        qFatal("CMEM_alloc returned NULL\n");
        exit(-1);
    }
    memset((void*)((pPixmap)->vAddress), 0x3F, (pPixmap)->lSizeInBytes);      //all white
    
    //NOTE: validate 4K aliagnment
    (pPixmap)->pAddress = CMEM_getPhys((void*)(pPixmap)->vAddress);
    if((pPixmap)->pAddress & 0xFFF)
    {
        qFatal("PVR2DMemWrap may have issues with this non-aligned address!\n");
        exit(-1);
    }
    qWarning("[qCreateNativePixmap]: reqSize(%d,%d); realSize(%d,%d), vAddr(%p), pAddr(%p)", 
        reqSize.width(), reqSize.height(), realSize.width(), realSize.height(),(void*)( (pPixmap)->vAddress), (void*)((pPixmap)->pAddress));
}

void qInvalidateNativePixmap(NATIVE_PIXMAP_STRUCT *pPixmap)
{
    pPixmap->pAddress = pPixmap->vAddress = NULL;
#ifdef MROFS_EGLIMAGE_EXTERNAL
    pPixmap->ePixelFormat = NPS_INVALID;
#elif defined(MROFS_TEXTURE_STREAM2)
    pPixmap->fourcc = BC_PIX_FMT_INVALID;
#endif
    pPixmap->eRotation = -1;
    pPixmap->lHeight = pPixmap->lSizeInBytes = pPixmap->lStride = pPixmap->lWidth = 0;
}

bool qIsNativePixmapValid(NATIVE_PIXMAP_STRUCT *pPixmap)
{
    return (reinterpret_cast<void *>(pPixmap->vAddress) != NULL);
}

void qDestroyNativePixmap(NATIVE_PIXMAP_STRUCT *pPixmap)
{
    if(!pPixmap)
        return;
    
    CMEM_AllocParams params = CMEM_DEFAULTPARAMS;
   params.flags = CMEM_CACHED;
   
    if(pPixmap->vAddress)
        CMEM_free((void*)pPixmap->vAddress, &params);

    qInvalidateNativePixmap(pPixmap);
}

/*! \brief do writing back
    \note 2 threads will call this: compositor thread and ui thread, so, cmem MUST BE and IS thread-safe
    consume 5% cpu on 335x@600MHz when whole block wb
*/
int qWritebackNativePixmap(void *ptr, size_t sz, void*blockAddr, size_t blockSz)
{   
    if (ptr < blockAddr) {  //ptr overrun
        ptr = blockAddr;
    } else if ((char*)(ptr) >= (char*)(blockAddr) + blockSz) {
        return -1;
    } else {
        //do nothing
    }
    
    Q_ASSERT(ptr >= blockAddr && ptr < blockAddr + blockSz);
    
    if((char*)(ptr) + sz > (char*)(blockAddr) + blockSz) { //sz overrun
        sz = (size_t)((char*)(blockAddr) + blockSz - (char*)(ptr));
    }

    Q_ASSERT((char*)(ptr) + sz <= (char*)(blockAddr) + blockSz);
#if  1

    
    return CMEM_cacheWb(ptr, sz);
#else
//    return CMEM_cacheWb(ptr, sz/8);
 return 0;
#endif
}

#ifdef MROFS_EGLIMAGE_EXTERNAL
int qCreateEGLImageTexture(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID)
{
    GLeglImageOES eglImage;
    PFNGLEGLIMAGETARGETTEXTURE2DOES pfnEGLImageTargetTexture2DOES;    
    PEGLDESTROYIMAGE pfnEGLDestroyImage;
    PEGLCREATEIMAGEKHR pfnEglCreateImageKHR;
    int err;

    glGenTextures(1, pTexID);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, *pTexID);
    
    pfnEglCreateImageKHR = (PEGLCREATEIMAGEKHR)eglGetProcAddress("eglCreateImageKHR");
    if(pfnEglCreateImageKHR == NULL)
    {
        qFatal("eglCreateImageKHR not found!\n");
        return -1;
    }
    
    eglImage = pfnEglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, pPixmap, NULL);
    if((err = eglGetError()) != EGL_SUCCESS)
    {
        qFatal("Error after eglCreateImageKHR!, error = %x\n", err);
        return -1;
    }

    pfnEGLImageTargetTexture2DOES = 
        (PFNGLEGLIMAGETARGETTEXTURE2DOES)eglGetProcAddress("glEGLImageTargetTexture2DOES");
    if(pfnEGLImageTargetTexture2DOES == NULL)
    {
        qFatal("pFnEGLImageTargetTexture2DOES not found!\n");
        return -1;
    }
    
    pfnEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, eglImage);
    if((err = glGetError()) != 0)
    {
        qFatal("Error after glEGLImageTargetTexture2DOES!, error = %x\n", err);
        return -1;
    }

    pfnEGLDestroyImage = (PEGLDESTROYIMAGE)eglGetProcAddress("eglDestroyImageKHR");
    pfnEGLDestroyImage(eglDisplay, eglImage);
    if((err = eglGetError()) != EGL_SUCCESS)
    {
        qFatal("Error after pEGLDestroyImage!, error = %x\n", err);
        return -1; 
    }

    return 0;
}

void qDestroyEGLImageTexture(GLuint texid)
{
    if(texid != -1)
        glDeleteTextures(1, &texid);
}
#endif

#ifdef MROFS_TEXTURE_STREAM2
#ifdef MROFS_CREATEIMGSTREAMTEXTURE_REOPEN
int qCreateIMGStreamTextureReopen(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID, int *pBcFd, int bcID, int bcBfIdx)
{
    Q_UNUSED(eglDisplay);
    
    //close first
    if(pBcFd[bcID] != -1) {
        close(pBcFd[bcID]);
        pBcFd[bcID] = -1;
    }

    char bcdev_name[] = "/dev/bccatX";
    bc_buf_params_t buf_param;
    BCIO_package ioctl_var;
    buf_param.width  = pPixmap->lWidth;
    buf_param.height = pPixmap->lHeight;
    buf_param.count  = 1;
    buf_param.fourcc = pPixmap->fourcc;
    buf_param.type   = BC_MEMORY_USERPTR;

    bcdev_name[strlen(bcdev_name)-1] = '0' + bcID;
    
    qWarning("qCreateIMGStreamTextureReopen before, bcdev_name: %s, id: %d, fd: 0x%08x, pPixmap: 0x%08x, phyAddr: 0x%08x", bcdev_name, bcID, pBcFd[bcID], pPixmap, pPixmap->pAddress);
    
    errno = 0;
    if ((pBcFd[bcID] = open(bcdev_name, O_RDWR|O_NDELAY)) == -1) {
        qFatal("qCreateIMGStreamTextureReopen: open %s failed[0x%x]", bcdev_name, errno);
    }

    //request buffers without allocating
    errno = 0;
    if (ioctl(pBcFd[bcID], BCIOREQ_BUFFERS, &buf_param) != 0) {
        qFatal("qCreateIMGStreamTextureReopen: BCIOREQ_BUFFERS failed[0x%x]", errno);
    }
    
    //confirm request is OK
    errno = 0;
    if (ioctl(pBcFd[bcID], BCIOGET_BUFFERCOUNT, &ioctl_var) != 0) {
        qFatal("qCreateIMGStreamTextureReopen: BCIOGET_BUFFERCOUNT failed[0x%x]", errno);
        return -1;
    }
    if (ioctl_var.output == 0) {
        qFatal("qCreateIMGStreamTextureReopen: no texture buffer available");
        return -1;
    }
    
    qDebug("qCreateIMGStreamTextureReopen, bcname(%s), bcfd(0x%x), bcid(%d)", bcdev_name, pBcFd[bcID], bcID);
    
    //setting physical addr for this texture
    errno = 0;
    bc_buf_ptr_t buf_pa;
    buf_pa.pa = pPixmap->pAddress;
    buf_pa.size = pPixmap->lSizeInBytes;    //ignored by bc_cat which use fourcc + width + height in BCIOREQ_BUFFERS
    buf_pa.index = bcBfIdx; 
    if (ioctl(pBcFd[bcID], BCIOSET_BUFFERPHYADDR, &buf_pa) != 0) {
        qFatal("qCreateIMGStreamTextureReopen: BCIOSET_BUFFERADDR[%d]: failed (0x%lx) [0x%x]", buf_pa.index, buf_pa.pa, errno);
        return -1;
    }
    
    qWarning("qCreateIMGStreamTextureReopen after, bcdev_name: %s, id: %d, fd: 0x%08x", bcdev_name, bcID, pBcFd[bcID]);
#if 0
    errno = 0;
    int index = 0;
    ioctl_var.input = index;
    ioctl(bcFd, BCIOGET_BUFFERPHYADDR, &ioctl_var);
    qWarning("qCreateIMGStreamTexture2(%s), index(%d)'s phyaddr(0x%x)", bcName, index, ioctl_var.output);    
    errno = 0;
    ioctl_var.input = ioctl_var.output;
    ioctl(bcFd, BCIOGET_BUFFERIDX, &ioctl_var);
    qWarning("qCreateIMGStreamTexture2(%s), phyaddr(0x%x)'s index(%d)", bcName, ioctl_var.input, ioctl_var.output);
#endif


    //trigger PVRSRVUnmapDeviceClassMemoryKM, ref. sgx kernel driver source for detail
    //ONLY ONCE
    glGenTextures(1, pTexID);
    glBindTexture(GL_TEXTURE_STREAM_IMG, *pTexID); 
    //this->m_glTexBindStreamIMG(this->m_bcIDWv, 0);       
    
    return 0;
}
#elif defined(MROFS_CREATEIMGSTREAMTEXTURE_RECONFIGURE)
int qCreateIMGStreamTextureReconfigure(EGLDisplay eglDisplay, NATIVE_PIXMAP_STRUCT* pPixmap, GLuint *pTexID, int bcFd, int bcBfIdx)
{
    Q_UNUSED(eglDisplay);
    
    bc_buf_params_t buf_param;
    BCIO_package ioctl_var;
    buf_param.width  = pPixmap->lWidth;
    buf_param.height = pPixmap->lHeight;
    buf_param.count  = 1;
    buf_param.fourcc = pPixmap->fourcc;
    buf_param.type   = BC_MEMORY_USERPTR;

    //request buffers without allocating
    errno = 0;
    if (ioctl(bcFd, BCIORECONFIGURE_BUFFERS, &buf_param) != 0) {
        qFatal("qCreateIMGStreamTextureReconfigure: BCIORECONFIGURE_BUFFERS failed[0x%x]", errno);
        return -1;
    }
    
    //confirm request is OK
    errno = 0;
    if (ioctl(bcFd, BCIOGET_BUFFERCOUNT, &ioctl_var) != 0) {
        qFatal("qCreateIMGStreamTextureReconfigure: BCIOGET_BUFFERCOUNT failed[0x%x]", errno);
        return -1;
    }
    if (ioctl_var.output == 0) {
        qFatal("qCreateIMGStreamTextureReconfigure: no texture buffer available");
        return -1;
    }
    qDebug("qCreateIMGStreamTextureReconfigure,bdFd(0x%x), bufferCount(%d)", bcFd, ioctl_var.output);
    
    //setting physical addr for this texture
    errno = 0;
    bc_buf_ptr_t buf_pa;
    buf_pa.pa = pPixmap->pAddress;
    buf_pa.size = pPixmap->lSizeInBytes;
    // 1 dev 1 buffer which index always be 0    
    Q_ASSERT(0 == bcBfIdx);
    buf_pa.index = bcBfIdx; 
    if (ioctl(bcFd, BCIOSET_BUFFERPHYADDR, &buf_pa) != 0) {
        qFatal("qCreateIMGStreamTextureReconfigure: BCIOSET_BUFFERADDR[%d]: failed (0x%lx) [0x%x]", buf_pa.index, buf_pa.pa, errno);
        return -1;
    }

    //trigger PVRSRVUnmapDeviceClassMemoryKM, ref. sgx kernel driver source for detail
    //ONLY ONCE
    glGenTextures(1, pTexID);
    glBindTexture(GL_TEXTURE_STREAM_IMG, *pTexID); 
    //this->m_glTexBindStreamIMG(this->m_bcIDWv, 0);       
   
    return 0;
}
#endif

void qDestroyIMGStreamTexture(GLuint texid)
{
#ifdef MROFS_TEXTURE_STREAM2_DEBUG
    //let GPU finish the pending glTexBindStreamIMG
    qWarning("before qDestroyIMGStreamTexture, into sleep");
#endif

#ifdef MROFS_TEXTURE_STREAM2_DEBUG
    qWarning("glDeleteTextures trigger UNMAP, texid: %d", texid);
#endif
    /*DHT20140919, when IMG Stream, glDeleteTextures seems to do 2 things:
    1, mark this bc_dev(associated with texid) nees to be reopened, which make glTexBindStreamIMG to open this bc_dev again;
    2, do ummap this bc_dev's device memory;
    NOTE: glTexBindStreamIMG
    */
    if(texid != GLuint(-1))
        glDeleteTextures(1, &texid);    
}
#endif  //MROFS_TEXTURE_STREAM2

#ifdef MROFS_PROFILING
unsigned long tv_diff(struct timeval *tv1, struct timeval *tv2)
{
    return (tv2->tv_sec - tv1->tv_sec) * 1000 +  (tv2->tv_usec - tv1->tv_usec) / 1000;
}
#endif

QT_END_NAMESPACE


