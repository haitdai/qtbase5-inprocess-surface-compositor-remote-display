#ifndef QMROFSWAVEFORM_H
#define QMROFSWAVEFORM_H

#include <QtCore/qlist.h>
#include <QtCore/qstack.h>
#include <QtCore/qmap.h>
#include "private/qwidget_p.h"
#include <QtWidgets/qmrofs.h>
#include "qmrofsdefs.h"

QT_BEGIN_NAMESPACE 

class QXcbShmImage;
class QXcbWindow;
class QMrOfsWaveview;
class QMrOfsWaveController;
class QMrOfsCompositor;
#include <QtPlatformSupport/private/qplatformcompositor_p.h> 

/*!
    \brief waveform in texture range
    \note 1 waveform 1 texture
*/
class QMrOfsWaveform : public QXcbObject
{
public:
    QMrOfsWaveform(QMrOfsWaveview *wv, QMrOfs::Format fmt, const QRect& rc);
    ~QMrOfsWaveform();
    bool allocateData(QMrOfs::Format fmt, const QSize& size_in_pixel);
    void deallocateData();
    QSize resize(QSize size_in_pixel);    
    void flushData();                                                 //xcb_image -> xcb_image's pixmap
    void flushData(const unsigned char *bits);            //bits -> xcb_image -> xcb_image's pixmap
    QAbstractMrOfs::WV_ID wid();
    QWaveData*  data();                                              //shared between wave thread and compositing thread
    QPoint move(const QPoint& pos);
    void markDirty(const QRegion &rgn);
    void clearDirty();
    QRect geometry();
    QSize size();
    QPlatformImage * platformImage();
    void updateEffectiveClip(const QRegion &bs_clip, const QRegion &old_cursor_rgn, QRegion &result_rgn);   //ref. effective_clip
    xcb_xfixes_region_t effectiveDirtyRegion();           //ref. effective_clip, xcb version
    QPoint originInTLW();
    
private:    
    QMrOfs::Format     format0;           //waveform/backingstore
    int                  wid0;              //wid
    QRect                geometry0;         //pos + size of wf in pixel, WAVEVIEW COORDINATE, geometry0.size ��image size ����һ�£����Գ���wv.viewport �ķ�Χ
    QRegion              dirty;             //region needs to be flushed in next compositing, WAVEFORM COORDINATE
    QRegion              effective_clip;    //effective_clip = wf.dirty UNION BS.dirty SUBTRACT popups_above.region INTERSECT wf.geometry, result in TLW COORDINATE
    QRegion              effective_dirty;   //effective_clip in WF COORDINATE
    xcb_xfixes_region_t  xcb_effective_dirty;  //xcb version of dirty, WAVEFORM COORDINATE
    QMrOfsWaveview    *view;                 //wv
    QPlatformImage        *xcb_image;            //shm image + pixmap
};

/*!
    \brief viewport in screen range
*/
class QMrOfsWaveview : public QXcbObject
{
public:
    QMrOfsWaveview(QMrOfsWaveController *wc, const QWidget *container, bool isIView = false);
    ~QMrOfsWaveview();
    void setViewport(const QRect& rc);
    QRect viewport();
    QMrOfs::WV_ID vid();
    QMrOfsWaveform* fromWid(QMrOfs::WV_ID wid);
    bool exist(QMrOfsWaveform* wf);
    void deleteAllWaveforms();
    void flushAllWaveforms();
    const QWidget* containerWidget();
    QMrOfsWaveController* controller();
    void addWaveform(QMrOfsWaveform *wf);
    void removeWaveform(QMrOfsWaveform *wf);
    void cleanAllWaveformClipRegion();
    void updateAllWaveformEffectiveClip(const QRegion &bs_clip, const QRegion &old_cursor_rgn, QRegion &result_rgn);
    const QList<QMrOfsWaveform*>& waveforms();   //for wave controller iteration
    bool isIView();
    
private:
    QRect                                      vpt0;           //pos + size of wv in pixel, CONTAINER WIDGET COORDINATE, ���ܳ���container_widget �ķ�Χ
    QList<QMrOfsWaveform*>                   wfs;            //wfs
    int                                        vid0;           //vid
    const QWidget                             *container_widget; //container widget
    QMrOfsWaveController                    *controller0;
    bool                                       m_isIView;      //isIView? (not iView means waveView)
};

/*!
    \brief  compositing assistant
*/
class QMrOfsWaveController : public QXcbObject
{
    Q_DISABLE_COPY(QMrOfsWaveController)
        
public:
    QMrOfsWaveController(QPlatformWindow *win);
    ~QMrOfsWaveController();    
    QMrOfsWaveview* openWaveview(const QWidget *container, const QRect &rc, bool isIView = false);
    void closeWaveview(QMrOfsWaveview *wv);
    void setWaveviewport(QMrOfsWaveview *wv, const QRect &rc);
    QMrOfsWaveview* fromVid(QMrOfs::WV_ID vid);                       //waveview ptr
    QMrOfsWaveform* fromWid(QMrOfs::WV_ID wid);                      //waveform ptr
    QMrOfsWaveview* viewOfWaveform(QMrOfsWaveform *wf);         //waveformInView
    //allocate form storage then bind to vid, waveform and iviewform all AS waveform:
    QMrOfsWaveform* addWaveform(QMrOfsWaveview *wv, const QRect &rc, QMrOfs::Format fmt = QMrOfs::Format_ARGB32_Premultiplied);   
    void removeWaveform(QMrOfsWaveform *wf);                          // �������waveview �Ը�waveform ������, ���ͷ�waveform �ڴ�
    QRect moveWaveform(QMrOfsWaveform *wf, const QRect &rc); //pos + size
    
    /*!
        ��֧��waveform ��flyweight, bind/unbind ��ʵ��
    */
    void bindWaveform(QMrOfsWaveform *wf, QMrOfsWaveview *wv, bool force_unbind = false);
    void unbindWaveform(QMrOfsWaveform *wf, QMrOfsWaveview *wv = NULL);               //wf == NULL means unbind for all views
    
    void flushWaves();
    void flushIView(QMrOfsWaveview *iv, QMrOfsWaveform *ivf, const unsigned char *bits);
    
    void openPopup(const QWidget* popup);
    void closePopup(const QWidget* popup);
    QWidget* findPopup(const QWidget* container_widget);
    QWidget* topmostPopup();
    QWidget* topleastPopup();
    
    void addWaveformClipRegion(QMrOfsWaveform *wf, const QRegion &rgn);
    void clipByPopupsAbove(QRegion& rgn, const QWidget *popup);        //rgn is input/output
    bool updateEffectiveClips(const QRegion &bs_clip, const QRegion &old_cursor_rgn);       //update effective_clip of all_wf and comopsite region
    void cleanEffectiveClips();    
    const QRegion& compositeRegion(xcb_xfixes_region_t* ret2);    //composite_region = effective_clip(all_wf) UNION bs_clip, xcb version    
    void accept(QMrOfsCompositor *compositor);                //compositor AS visitor
    
    //FIXME: beginPaint(rgn)/endPaint of waveform; in beginPaint, sync with xserver if rgn is intersected with existed dirty area ???
    
private:
    //waveview ��container widget ��map
    QMap<QMrOfsWaveview*, QWidget*>   wv_widget_map;          //only VISIBL widget(wv container) counts, ref closePopup
    QList<QWidget*>                     popups_z;               //backest Ϊtopmost
    QPlatformWindow                  *xcb_win0;               //tlw
    xcb_xfixes_region_t                 xcb_composite_region;   //ref. compositeRegion, TLW coordinate(the same as BACKINGSTORE coordinate)
    QRegion                                 qt_composite_region;
};

QT_END_NAMESPACE
    
#endif   //QMROFSWAVEFORM_H

