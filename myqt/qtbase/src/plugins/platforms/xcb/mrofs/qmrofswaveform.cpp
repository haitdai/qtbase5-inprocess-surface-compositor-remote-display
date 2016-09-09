
#include "QtGui/qimage.h"
#include "qmrofswaveform.h"
#include "qmrofscompositor.h"
#include "qxcbscreen.h"        //QXcbScreen
#include "qxcbimage.h"        //QXcbShmImage
#include "qxcbwindow.h"       //QXcbWindow

QT_BEGIN_NAMESPACE   

/**************************************************************************************/
/******************************* QMrOfsWaveform *************************************/
/**************************************************************************************/

/*!
    -1 标识invalid
    ref. xvinfo, xdpyinfo -ext RENDER, xcb_render_query_pict_formats_reply(for xrender), xcb_xv_query_image_attributes(for xv)
    \note
    xv image format id 是固定的常数
    xcb_render_pictforminfo_t.id是随机产生的，不能作为常数使用(2次运行的值可能不同), 但xcb_pict_standard_t 是固定的常数可以用
*/

QMrOfsWaveform::QMrOfsWaveform(QMrOfsWaveview* wv, QMrOfs::Format fmt, const QRect& rc) 
    : format0(fmt), wid0(-1), geometry0(rc), dirty(QRegion()), effective_clip(QRegion()), effective_dirty(QRegion()), view(wv), xcb_image(NULL)
{
    static QAtomicInt widGenerator(0);
    this->wid0 = widGenerator.fetchAndAddRelaxed(1);
    
    qWarning("%s, wid(%d)", __FUNCTION__, this->wid0);
    
    QSize size_in_pixel = QSize(rc.width(), rc.height());
    allocateData(fmt, size_in_pixel);

    //initialize waveform xcb clip region
    xcb_effective_dirty = xcb_generate_id(xcb_connection());
    xcb_xfixes_create_region(xcb_connection(), xcb_effective_dirty, 0, NULL);

    //picture is OK in QXcbShmImage
}

QMrOfsWaveform::~QMrOfsWaveform()
{
    geometry0 = QRect();
    wid0 = -1;
    dirty = QRegion();
    effective_clip = QRegion();
    effective_dirty = QRegion();
    view = NULL;
    
    deallocateData();

    xcb_xfixes_destroy_region(xcb_connection(), xcb_effective_dirty);
}

bool QMrOfsWaveform::allocateData(QMrOfs::Format fmt, const QSize& size_in_pixel)
{
    QWindow *win = view->containerWidget()->window()->windowHandle();                         //widget -> window
    QXcbScreen *scrn = static_cast<QXcbScreen *>(win->screen()->handle());                      //window -> xcb_window
    setConnection(scrn->connection());

    this->xcb_image = new QXcbShmImage(scrn, size_in_pixel, depthFromMainFormat(fmt), fmt); 

    if(!this->xcb_image) {
        return false;
    }
    
    return true;
}

void QMrOfsWaveform::deallocateData()
{
    delete this->xcb_image;
    this->xcb_image = NULL;
}

/*!
    \return old size of wf
    \note wf's data has changed, but wf does NOT.
    \note untouch geometry0.x/y
*/
QSize QMrOfsWaveform::resize(QSize size_in_pixel)
{
    QSize old_sz = this->size();

    //size not changed
    if(old_sz == size_in_pixel)
        return old_sz;

    //reallocate data
    this->deallocateData();                               //xcb_image is deleted
    this->allocateData(this->format0, size_in_pixel);     //re-allocate, buf_num not change

    //update wf geometry, untouch x/y
    geometry0.setSize(size_in_pixel);
    
    return old_sz;
} 

/*!
    flush dirty region(image -> pixmap)
    \note  wf coordinate
    \note WAVEFORM USE ONLY
    1, QXcbShmImage.QImage 必须有alpha channel，且为xrender 支持的format(最好为ARGB32_Premultiplied)
    2, xcb_shm_put_image 必须把alpha 值随RGB原样拷贝到xcb_pixmap，不能中途丢弃
    3, xrender 能识别这个alpha 值
*/
void QMrOfsWaveform::flushData()
{  
    foreach (const QRect &rc, this->dirty.rects()) {
        if(rc.isEmpty())
            continue;
        //put image into pixmap, dirty parts
        (reinterpret_cast<QXcbShmImage*>(this->xcb_image))->put(
            (reinterpret_cast<QXcbShmImage*>(this->xcb_image))->pixmap(), rc.topLeft(), rc);
    }
}

/*!
    \todo
    if xBGR is not supported by render in xserver, use ARGB just the same as wf, and then do r/b swap here
    \todo
    should we re-set alpha to 0xFF ???
    \note IVIEWFORM USE ONLY    
*/
void QMrOfsWaveform::flushData(const unsigned char *bits)
{
    //QWaveData.data() as DST, bits as SRC
    unsigned char *dst = const_cast<unsigned char*>(this->data()->constBits());
    
    //bits -> QWaveData.data, if app has updated QWaveData.data bits by waveformRenderBuffer directly, memcpy is NOT necessary here
    if(bits) {
        //32 pixel alignment in width, ref. section7.2 in <ISP driver user guide>
        unsigned int remainder = this->size().width() % 32 == 0 ? 0 : 1;
        unsigned int sz_in_byte = (this->size().width() / 32 + remainder) * 32 * this->size().height() * 4;        
        memcpy(dst, bits, sz_in_byte);
    }
    
    //QWaveData.data -> xcb pixmap
    foreach (const QRect &rc, this->dirty.rects()) {
        if(rc.isEmpty())
            continue;
        //put image into pixmap, dirty parts
         (reinterpret_cast<QXcbShmImage*>(this->xcb_image))->put(
             (reinterpret_cast<QXcbShmImage*>(this->xcb_image))->pixmap(), rc.topLeft(), rc);
    }   
}

QAbstractMrOfs::WV_ID QMrOfsWaveform::wid()
{
    return this->wid0;
}

/*!
    QImage for waveform
    \return QWaveData APP which can draws into
*/
QWaveData*  QMrOfsWaveform::data()
{
    return this->xcb_image->image();
}

/*!
    \note untouch size
*/
QPoint QMrOfsWaveform::move(const QPoint& pos)
{
    QPoint old = QPoint(geometry0.left(), geometry0.top());
    geometry0.moveTo(pos.x(), pos.y());
    return old;
}

/*!
    union rgn into dirty
    \note rgn is in wf coordinate; rgn is clamped in wf geometry range
*/
void QMrOfsWaveform::markDirty(const QRegion &rgn)
{
    //translate geometry from wv to wf coordinate, then clamp rgn to it
    QPoint offset = QPoint(geometry0.x(), geometry0.y());    
    QRegion real_rgn = rgn;
    real_rgn &= geometry0.translated(-offset.x(), -offset.y());

    //add to dirty(wf coordinate)
    this->dirty += real_rgn;
}

/*!
    APP wants multiple adds with 1-shot clean
*/
void QMrOfsWaveform::clearDirty()
{
    this->dirty = QRegion();
}

/*!
    \retrun waveview coordinate
*/
QRect QMrOfsWaveform::geometry()
{
    return this->geometry0;
}

QSize QMrOfsWaveform::size()
{
    return this->xcb_image->image()->size();
}

/*!
    if compositor gets QXcbShmImage then gets everything of it
    ref. QXcbBackingStore.picture()
*/
QPlatformImage *QMrOfsWaveform::platformImage()
{
    return this->xcb_image;
}

/*!
    update effective clip region of this wf before compositing(ui/wave updateing)
    bs_clip in tlw COORDINATE
    effective_clip = wf_clip UNION bs_clip  INTERSECT wf_viewport SUBTRACT popups_above(wf)
*/
void QMrOfsWaveform::updateEffectiveClip(const QRegion &bs_clip, const QRegion &old_cursor_rgn, QRegion &result_rgn)
{
    const QWidget* container_widget = this->view->containerWidget();
    QMrOfsWaveController* controller = this->view->controller();
    QWidget *tlw = controller->topleastPopup();    
    if(NULL == tlw) {
        qFatal("QMrOfsWaveform::updateEffectiveClip, no topleastPopup exists, do you forget to call openPopup for the tlw???");
    }
    
    //map dirty/viewport to tlw coordinate and clamp dirty in viewport into effective_clip
    const QPoint cw_offset(container_widget->mapTo(tlw, QPoint()));  //container widget offset in tlw
    QRect vpt = this->view->viewport();                               //vpt in container widget coordinate
    const QPoint wf_offset = QPoint(cw_offset.x() + this->geometry0.x() + vpt.x(), cw_offset.y() + this->geometry0.y() + vpt.y());   //wf offset in tlw
    vpt.translate(cw_offset);                                         //vpt in tlw coordinate    
    this->effective_clip = this->dirty.translated(wf_offset);         //effective_clip in tlw coordinate
    this->effective_clip += bs_clip;                                  //UNION bs_clip
    this->effective_clip += old_cursor_rgn;                           //UNION old_cursor_rgn
    this->effective_clip &= vpt;                                      //INTERSECT vpt
    
    //no popup to sit in, something wrong
    QWidget *popup = controller->findPopup(container_widget);
    if(NULL == popup) {
        qWarning("QMrOfsWaveform::updateEffectiveClip: waveform(waveview) has NOT belong to any popups!!!");
        return;
    }
    
    //update effective clip via popups above popup
    controller->clipByPopupsAbove(this->effective_clip, popup);

    //set effective_clip(tlw coordinate) bck to dirty(waveform coordinate)
    this->effective_dirty = this->effective_clip.translated(QPoint() - wf_offset);
    
    // INTERSECT with wf range
    this->effective_dirty &= QRect(0, 0, geometry0.width(), geometry0.height());

    // the same with effective_clip
    this->effective_clip = this->effective_dirty.translated(wf_offset);
        
    //qt retion to xcb region(xcb_effective_dirty)
    qtRegion2XcbRegion(xcb_connection(), this->effective_dirty, xcb_effective_dirty);

    //result it, carrying on for compositeRegion
    result_rgn += this->effective_clip;
}

/*!
    xcb version of effective_clip, already sync with it in updateEffectiveClip
    \ref. QXcbBackingStore.effectiveClip()
*/
xcb_xfixes_region_t QMrOfsWaveform::effectiveDirtyRegion()
{
    return this->xcb_effective_dirty;
}

/*!
    this waveform's topleft in tlw coordinate, used as compositor's destination origin
*/
QPoint QMrOfsWaveform::originInTLW()
{
    const QWidget* container_widget = this->view->containerWidget();
    QMrOfsWaveController* controller = this->view->controller();
    QWidget *tlw = controller->topleastPopup();    
    if(NULL == tlw) {
        qFatal("QMrOfsWaveform::originInTLW, no topleastPopup exists, do you forget to call openPopup for the tlw???");
    }
    
    //map dirty/viewport to tlw coordinate and clamp dirty in viewport into effective_clip
    const QPoint cw_offset(container_widget->mapTo(tlw, QPoint()));  //container widget offset in tlw
    QRect vpt = this->view->viewport();                                             //vpt in container widget coordinate
    QPoint wf_offset = QPoint(cw_offset.x() + this->geometry0.x() + vpt.x(), cw_offset.y() + this->geometry0.y() + vpt.y());   //wf offset in tlw

    return wf_offset;
}

/**************************************************************************************/
/******************************* QMrOfsWaveview *************************************/
/**************************************************************************************/

QMrOfsWaveview::QMrOfsWaveview(QMrOfsWaveController *wc, const QWidget *container, bool isIView) : 
vpt0(QRect()), wfs(QList<QMrOfsWaveform*>()), vid0(-1), container_widget(container), controller0(wc), m_isIView(isIView)
{
    static QAtomicInt vidGenerator(0);
    this->vid0 = vidGenerator.fetchAndAddRelaxed(1);
    
    QWindow *win = container_widget->window()->windowHandle();                                                //widget -> window
    QXcbScreen *scrn = static_cast<QXcbScreen *>(win->screen()->handle());           //window -> xcb_window
    setConnection(scrn->connection());
}

QMrOfsWaveview::~QMrOfsWaveview()
{
    deleteAllWaveforms();
}

/*!
    container widget coordinate
    \note vpt is clamped in container_widget range
    \note if container_widget has benn resized, it's wv viewport should be reset(setWaveviewport) manually  <<<
*/
void QMrOfsWaveview::setViewport(const QRect& rc)
{
    QRect tmp_vpt = rc;
    tmp_vpt &= container_widget->rect();
    this->vpt0 = tmp_vpt;
}

/*!
    container widget coordinate
*/
QRect QMrOfsWaveview::viewport()
{
    //note the difference of rect() and geometry()
    return this->vpt0;
}

QAbstractMrOfs::WV_ID QMrOfsWaveview::vid()
{
    return this->vid0;
}

QMrOfsWaveform* QMrOfsWaveview::fromWid(QAbstractMrOfs::WV_ID wid)
{
    foreach(QMrOfsWaveform *wf, wfs) {
        if(wf->wid() == wid)
            return wf;
    }

    return NULL;
}

bool QMrOfsWaveview::exist(QMrOfsWaveform* wf)
{
    //wf's wid is in this waveview: by wid OR pointer
    if(wf && (NULL != fromWid(wf->wid()) || -1 != wfs.indexOf(wf)))
        return true;

    return false;
}

void QMrOfsWaveview::deleteAllWaveforms()
{
    foreach(QMrOfsWaveform *wf, wfs) {
        delete wf;
        wfs.removeOne(wf);
    }
}

/*!
    wf ONLY
*/
void QMrOfsWaveview::flushAllWaveforms()
{
    if(isIView())
        return;
        
    foreach(QMrOfsWaveform *wf, wfs) {
        wf->flushData();
    }
}

const QWidget* QMrOfsWaveview::containerWidget()
{
    return this->container_widget;
}

QMrOfsWaveController* QMrOfsWaveview::controller()
{
    return this->controller0;
}

void QMrOfsWaveview::addWaveform(QMrOfsWaveform *wf)
{
    if(exist(wf)) {            //avoid redundent
        return;
    }

    wfs.append(wf);
}

void QMrOfsWaveview::removeWaveform(QMrOfsWaveform *wf)
{
    wfs.removeOne(wf);
}

void QMrOfsWaveview::cleanAllWaveformClipRegion()
{
    foreach(QMrOfsWaveform *wf, wfs) {
        wf->clearDirty();
    }    
}

void QMrOfsWaveview::updateAllWaveformEffectiveClip(const QRegion &bs_clip, const QRegion &old_cursor_rgn, QRegion &result_rgn)
{
    foreach(QMrOfsWaveform *wf, wfs) {
        wf->updateEffectiveClip(bs_clip, old_cursor_rgn, result_rgn);
    }
}

const QList<QMrOfsWaveform*>& QMrOfsWaveview::waveforms()
{
    return this->wfs;
}

bool QMrOfsWaveview::isIView()
{
    return m_isIView;
}

/**************************************************************************************/
/***************************** QMrOfsWaveController ***********************************/
/**************************************************************************************/

QMrOfsWaveController::QMrOfsWaveController(QPlatformWindow *plat_win) 
    : wv_widget_map(QMap<QMrOfsWaveview*, QWidget*>()), popups_z(QList<QWidget*>())
    , xcb_win0(plat_win), xcb_composite_region(XCB_NONE), qt_composite_region(QRegion())
{
    QWindow *win = plat_win->window();        //platform window : xcb window
    QXcbScreen *scrn = static_cast<QXcbScreen *>(win->screen()->handle());
    setConnection(scrn->connection());

    //prepare an empty region
    xcb_composite_region = xcb_generate_id(xcb_connection());
    xcb_xfixes_create_region  (xcb_connection(), xcb_composite_region, 0, NULL);
}

QMrOfsWaveController::~QMrOfsWaveController()
{
    QMrOfsWaveview *wv = NULL;
    
    //delete until empty
    while (!wv_widget_map.empty()) {
        wv = wv_widget_map.firstKey();
        closeWaveview(wv);
    }

    xcb_xfixes_destroy_region (xcb_connection(), xcb_composite_region);
    qt_composite_region = QRegion();
}

/*!
    rc is in QWidget coordinate (container )
*/
QMrOfsWaveview* QMrOfsWaveController::openWaveview(const QWidget *container, const QRect &rc, bool isIView)
{
    if(NULL == container) {
        qFatal("QMrOfsWaveview::openWaveview, container is NULL");
    }
    
    QMrOfsWaveview *wv = new QMrOfsWaveview(this, container, isIView);
    wv_widget_map.insert(wv, const_cast<QWidget*>(container));
    wv->setViewport(rc);
    return wv;
}

/*!
    when waveview closing, all waveform and wavedata attached to it will be deleted automatically
*/
void QMrOfsWaveController::closeWaveview(QMrOfsWaveview *wv)
{  
    if(NULL == wv)     //nothing to close
        return; 

    //delete view, and all waveforms and wave data will be deleted in wv's dtor
    delete wv;
    
    //remove from wvs
    wv_widget_map.remove(wv);
}

/*!
    
*/
void QMrOfsWaveController::setWaveviewport(QMrOfsWaveview *wv, const QRect &rc)
{
    if(NULL == wv)
        return;
    
    wv->setViewport(rc);
}

/*!
    vid to ptr
*/    
QMrOfsWaveview* QMrOfsWaveController::fromVid(QAbstractMrOfs::WV_ID vid)
{
    QMrOfsWaveview *wv = NULL;
    
    //vid to ptr, untouch the map
    QMap<QMrOfsWaveview*, QWidget*>::const_iterator i = wv_widget_map.constBegin();    
    while (i != wv_widget_map.constEnd()) {
        if (i.key()->vid() == vid) {
            wv = i.key();
            break;
        }
        ++i;
    }

    return wv;
}

/*!
    wid to ptr
*/    
QMrOfsWaveform* QMrOfsWaveController::fromWid(QAbstractMrOfs::WV_ID wid)
{
    QMrOfsWaveview *wv = NULL;
    QMrOfsWaveform    *wf = NULL;
    
    //vid to ptr, untouch the map
    QMap<QMrOfsWaveview*, QWidget*>::const_iterator i = wv_widget_map.constBegin();
    while (i != wv_widget_map.constEnd()) {
        wv = i.key();
        if(NULL != (wf = wv->fromWid(wid)))      //found it!!!
            return wf;
        ++i;
    }
    
    return NULL;
}

/*!
    find the view which waveform wid sit in
*/    
QMrOfsWaveview* QMrOfsWaveController::viewOfWaveform(QMrOfsWaveform *wf)
{
    QMrOfsWaveview *wv = NULL;
    
    //vid to ptr, untouch the map
    QMap<QMrOfsWaveview*, QWidget*>::const_iterator i = wv_widget_map.constBegin();
    while (i != wv_widget_map.constEnd()) {
        wv = i.key();
        if(wv->exist(wf))
            return wv;
        ++i;
    }

    return NULL;
}

/*!
    new a waveform and data(size_in_byte), then add it to view vid
    \note waveform 不支持在不同的waveview 之间共享!!!
    waveform and iviewform are all AS waveform
    \param fmt: UYVY/ARGB32_Premultiplied
*/
QMrOfsWaveform* QMrOfsWaveController::addWaveform(QMrOfsWaveview *wv, const QRect &rc, QMrOfs::Format fmt)
{
    qWarning("%s, wv(%p), rc(%d,%d,%d,%d), fmt(0x%x)", 
		__FUNCTION__, wv, rc.x(), rc.y(), rc.width(), rc.height(), fmt);
    
    //illegal fmt
    if(!validMainFormat(fmt)) {
        qWarning("QMrOfsWaveController::addWaveform: unsupported format: %d", fmt);
        return NULL;
    }

    //allocate data in ctor of wf
    QMrOfsWaveform *wf = new QMrOfsWaveform(wv, fmt, rc);
    if(!wf) {
        qWarning("%s: waveform ctor failed(%p, %x, (%d,%d,%d,%d))", 
            __FUNCTION__, wv, fmt, rc.x(), rc.y(), rc.width(), rc.height());
        return NULL;
    }
    wv->addWaveform(wf);

    //this wf's clip region keeps empty, no region caculating needed here
    
    return wf;
}

/*!
    delete waveform wf, and unbind it from all views
    wf is global unique
*/
void QMrOfsWaveController::removeWaveform(QMrOfsWaveform *wf)
{
    QMrOfsWaveview *wv = viewOfWaveform(wf);
    if(NULL == wv) {
        qWarning("QMrOfsWaveController::removeWaveform:  can NOT find waveview of waveform %p", wf);
        return;
    }
    wv->removeWaveform(wf);
    //deallocate data in dtor of wf    
    delete wf;
}

/*!
    move waveform to new position in waveview    
    \note size maybe changed also
*/
QRect QMrOfsWaveController::moveWaveform(QMrOfsWaveform *wf, const QRect &rc)
{
    if(NULL == wf) {
        qWarning("QMrOfsWaveController::moveWaveform, wf is NULL");
        return QRect();
    }
    
    QRect old_geo = wf->geometry();

    //size changed, 注意: wf resize 后data 指针变了
    QSize new_sz = QSize(rc.width(), rc.height());
    if(!(old_geo.width() == new_sz.width() && old_geo.height() == new_sz.height())) {
        wf->resize(new_sz);
    }
    
    QPoint new_pos = QPoint(rc.left(), rc.top());
    if(!(old_geo.x() == rc.left() && old_geo.y() == rc.top())) {
        wf->move(new_pos); 
    }
    
    return old_geo;
}

/*!
   不用flyweight ，也没有引用计数，bind 相关接口可以不实现
*/
void QMrOfsWaveController::bindWaveform(QMrOfsWaveform *wf, QMrOfsWaveview *wv, bool force_unbind)
{
    qWarning("QMrOfsWaveController::bindWaveform: unsupported operation!!!");
    Q_UNUSED(wf);
    Q_UNUSED(wv);
    Q_UNUSED(force_unbind);
}

/*!
    同bindWaveform
*/
void QMrOfsWaveController::unbindWaveform(QMrOfsWaveform *wf, QMrOfsWaveview *wv)
{
    qWarning("QMrOfsWaveController::unbindWaveform: unsupported operation!!!");
    Q_UNUSED(wf);
    Q_UNUSED(wv);
}

/*!
    dirty region already marked before
    \ref addWaveformClipRegion
*/
void QMrOfsWaveController::flushWaves()
{
    QMrOfsWaveview *wv = NULL;
    QMap<QMrOfsWaveview*, QWidget*>::const_iterator i = wv_widget_map.constBegin();
    while (i != wv_widget_map.constEnd()) {
        wv = i.key();
        if(!wv->isIView())              //only waveview flushed
            wv->flushAllWaveforms();
        ++i;
    }    
}

/*!
    handle xBGR v4l2 bits
*/
void QMrOfsWaveController::flushIView(QMrOfsWaveview *iv, QMrOfsWaveform *ivf, const unsigned char *bits)
{
    if(!iv || !ivf || !bits) {
        qWarning("QMrOfsWaveController::flushIView: invalid data");
        return;
    }

    ivf->flushData(bits);
}

/*!
    popup a "popup" and put it on the top of the z order(back in list)
    \note if APP have some waveview in this popup,  think this <<<
*/
void QMrOfsWaveController::openPopup(const QWidget* popup)
{
    if(!popup) {
        qWarning("QMrOfsWaveController::openPopup, popup is NULL");
        return;
    }
    
    if (-1 == popups_z.indexOf(const_cast<QWidget*>(popup))) {           //not exist
        //top-most sits in back-most
        popups_z.push_back(const_cast<QWidget*>(popup));
    }
}

/*!
    popup "popup", and keeps all other popups above it in z order
    \note if popup as QWidget has been hidden(hide/setVisible(false), call closePopup too <<<
*/
void QMrOfsWaveController::closePopup(const QWidget* popup)
{
    if(!popup) {
        qWarning("QMrOfsWaveController::closePopup, popup is NULL");
        return;
    }

    int pos = -1;
    if (-1 != (pos = popups_z.indexOf(const_cast<QWidget*>(popup)))) {           //exist
        popups_z.removeAt(pos);
#if 0  // delete all other popups above popup is not the expecting behaviro
        //pop all above popup, and then popup
        while(!popups_z.isEmpty() && (popups_z.back() != popup))
            popups_z.pop_back();
#endif        
    }
}

/*!
    \note 隐含条件: waveview 必然在某一个popup中，如果不属于临时弹出的，那么必属于tlw(也是popup)
*/
QWidget* QMrOfsWaveController::findPopup(const QWidget* container_widget)
{
    QWidget *popup = NULL;
    
    while (container_widget) {
        if(-1 != popups_z.indexOf(const_cast<QWidget*>(container_widget))) {  //container_widget is popup
            popup = const_cast<QWidget*>(container_widget);
            break;
        } else if(container_widget->isWindow()) {                                               //container_widget is TLW, TLW must be popup(in popups_z)
            qFatal("QMrOfsWaveController::findPopup, TLW(%p) is NOT in popups_z", container_widget);
        } else {                                                                                                     //container_widget not met, check it's parent
            container_widget = container_widget->parentWidget();
        }
    }

    return popup;
}

QWidget* QMrOfsWaveController::topmostPopup()
{
    return popups_z.back();
}

/*!
    topleast is tlw
*/
QWidget* QMrOfsWaveController::topleastPopup()
{
    return popups_z.front();
}

/*!
    set dirty region of wf
    \note rgn is in waveform coordinate
*/
void QMrOfsWaveController::addWaveformClipRegion(QMrOfsWaveform *wf, const QRegion &rgn)
{
    if(!wf) {
        qWarning("QMrOfsWaveController::addWaveformClipRegion, wf is NULL");
        return;
    }
    
    wf->markDirty(rgn);
}

/*!
    clip rgn by popups_z above popup, not including popup itself
    \param rgn[in/out]: waveform clip region in tlw coordinate
*/
void QMrOfsWaveController::clipByPopupsAbove(QRegion& rgn, const QWidget *popup)
{
    QWidget *tlw = topleastPopup();
    int start = popups_z.indexOf(const_cast<QWidget*>(popup));
    
    for (int i = start + 1; i < popups_z.size(); i ++) {
        //opaque children issue, ref QWidget.visibleRegion
        //watch out the difference between at(i) and operator[i]
#if 0
        //QRegion popup_rgn = popups_z.at(i)->visibleRegion() | popups_z.at(i)->childrenRegion();
        QRect tmp_rc = popups_z.at(i)->frameGeometry();
        QRegion popup_rgn = QRect(0, 0, tmp_rc.width(), tmp_rc.height());                
#else
        QWidgetPrivate * const d = popups_z.at(i)->d_func();
        QRegion popup_rgn  = d->clipRectNoGE();
#endif
        //map popup_rgn to tlw coordinate
        const QPoint offset = popups_z.at(i)->mapTo(tlw, QPoint());
        rgn -= popup_rgn.translated(offset);
    }
}

/*!
    updating effective_clip of all_wf(in wv) and composite region
    return: effective clips is not empty
*/
bool QMrOfsWaveController::updateEffectiveClips(const QRegion &bs_clip, const QRegion &old_cursor_rgn)
{
    //stage1: update all wfs effective_clip, union them into result_rgn
    qt_composite_region = QRegion();        
    QMap<QMrOfsWaveview*, QWidget*>::const_iterator i = wv_widget_map.constBegin();
    while (i != wv_widget_map.constEnd()) {                                           //all wvs
        QMrOfsWaveview *wv = i.key();
        wv->updateAllWaveformEffectiveClip(bs_clip, old_cursor_rgn, qt_composite_region);      //all wfs in this wv
        ++i;
    }
    
    //state2: union bs_clip into result_rgn
    qt_composite_region += bs_clip;

    //stage3: union old cursor range into result_rgn
    qt_composite_region += old_cursor_rgn;
    
    //stage4: translate result_rgn from qt region to xcb region
    qtRegion2XcbRegion(xcb_connection(), qt_composite_region, xcb_composite_region);

    return !qt_composite_region.isEmpty();
}

/*!
    clean effctive_clip of all_wf and composite region
*/
void QMrOfsWaveController::cleanEffectiveClips()
{
    //stage1: clean all wfs effective_clip_region
    //for each waveview
    QMap<QMrOfsWaveview*, QWidget*>::const_iterator i = wv_widget_map.constBegin();    
    while (i != wv_widget_map.constEnd()) {
        QMrOfsWaveview *wv = i.key();
        wv->cleanAllWaveformClipRegion();
        ++i;
    }

    //stage2: clean composite region
    xcb_xfixes_set_region(xcb_connection(), xcb_composite_region, 0, NULL);
    qt_composite_region = QRegion();    
}

/*!
    effective_clip(all_wf) UNION bs_clip
*/
const QRegion& QMrOfsWaveController::compositeRegion(xcb_xfixes_region_t* ret2)
{
    if(ret2) {
        *ret2 = xcb_composite_region;    
    }

    return qt_composite_region;
}

/*!
    accept compositor as visitor, then visit all waveforms
    \note  only VISIBLE widgets count
*/
void QMrOfsWaveController::accept(QMrOfsCompositor *compositor)
{
    QMap<QMrOfsWaveview*, QWidget*>::const_iterator i = wv_widget_map.constBegin();    
    while (i != wv_widget_map.constEnd()) {
        QMrOfsWaveview *wv = i.key();
        foreach(QMrOfsWaveform *wf, wv->waveforms()) {
            if(wv->isIView()) {
                compositor->visitIViewform(wf);
            } else {
                compositor->visitWaveform(wf);
            }
        }
        ++i;
    }
}

QT_END_NAMESPACE   
    
