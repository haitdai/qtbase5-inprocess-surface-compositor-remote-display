#ifndef QMROFS_H
#define QMROFS_H

#include <QtCore/QObject>
#include <QtGui/qrgb.h>
#include <QtGui/qregion.h>
#include <QtWidgets/qwidget.h>
#include <QtWidgets/qabstractmrofs.h>

QT_BEGIN_NAMESPACE

#define MROFS_VERSION  "1.0.0" 

class QMrOfsPrivate;
/*!
    design:         sharing interface, independent implementation(since different hardware platforms): 
        UBHE: 		armv7+sgx530/OGLESv2+EGL 
        IGDIPPER:	x86+hd4k/XRender+XVideo
        MERAK:		armv7+sgx530/PVR2D
    facade:               QMrOfs is based on QAbstractMrOfs, but most new features is in itself.
    interface:            QMrOfsPlatformCompositor.
    deployment:        	QMrOfs is in QtWidgets package; 
    		QMrOfs is in QtWidgets package;
    		legacy QMrOfs is in QtGui package.
*/
class  Q_WIDGETS_EXPORT QMrOfs : public QObject, public QAbstractMrOfs  {
    Q_OBJECT
    Q_DECLARE_PRIVATE(QMrOfs)
    
    Q_PROPERTY(QRgb colorKey READ getColorKey)
    Q_PROPERTY(const char* version READ getVersion)
    
public:

    /*!
        \param tlw  指定QMrOfs的根widget, QMrOfs 在tlw.windowHandle内合成，不管之外的部分
        \note
        QMrOfs 的实例创建在win  上，负责该win 内的合成；双屏时，期望每个screen上都有1个win。
        如果一个screen上创建了多个可见的win，那么只要app愿意，它可以为每个win创建一个QMrOfs实例，每个QMrOfs实例负责该win内的合成。
        实现时，增加对一个screen上只允许1个win的检查，限制每个screen上只能有1个win(设计约束)。
        把QMrOfs实例绑定到win上而不绑定到screen的原因是:解耦合screen 和win。
        \note
        实际被合成的最多只有2+N 个虚屏:
       1x  字符屏(该win)，预期只有1个。
       Nx  波形屏(多个wave view 共用1个物理的波形屏)。
       1x  iView 屏(由MIPI 模块提供内容，理论上不消耗cpu)。
        \ref QScreenPrivate::platformCompositor
        \note
        不支持诸如QMrOfs::setWindow 的接口。
        一旦指定win创建QMrOfs，必须保证win在QMrOfs存在期间始终存在。
        \note 
        应用调用 QWidget::windowHandle 可获得QWidget 的TLW
        \note
        保留Nx 个波形屏的原因是:
        1,  不仅合成，画主屏波形时也要裁剪popup，否则将会在popup区域内出现主屏的波形；
        2,  应用不想在画主屏波形的时候，自己裁剪popup；
        3,  使用N 个波形屏的合成次数和使用1个波形屏一样，只是在显存上增加了一些空间占用。
    */
    
    explicit QMrOfs(QWidget *tlw, QObject *parent = 0);
    ~QMrOfs();
    
public Q_SLOTS:
    WV_ID setWaveviewLayer(WV_ID vid, int layer);                            //override, deprecated
    
Q_SIGNALS:
    void wavesFlushed(bool bOK);                                                          //override
    
public:
    /*!
        \brief
        1 个waveview 包含1个或多个waveform，每个waveform有自己的独立存储
        \note 这里只在函数原型上保留了对v1 的兼容，语义上有区别:
        v1:  在screen上的view
        v2:  NOT IMPLEMENTED
    */
    WV_ID openWaveview();                                                                   // override
    
    /*!
        \brief
        关闭waveview 时，属于它的waveform全部自动销毁
        如果1个waveform属于多个waveview，由于其中1个waveview的关闭导致waveform销毁前，必须先清除其它waveview对该waveform的引用
    */
    void closeWaveview(WV_ID vid);                                                      // override
    
    /*!
        \brief 重新设置waveview 位置
        \note widget(container) 坐标系
    */
    void setWaveViewport(WV_ID vid, const QRect& vpt);                       // override
    
    /*!
        v1: 创建波形纹理
        v2:  NOT IMPLEMENTED        
    */
    WV_ID createWaveform(WV_ID vid);                                                // override, deprecated
    
    /*!
        \brief 只改waveform尺寸，不改变和waweview的绑定关系
        \note
        resize 之前，APP应保证
        1, compositor 之前的工作已做完，且没有新的compositing 请求
        2, wave 线程中所有的painter 已释放(painter 会缓存QWaveData 的指针)
        以避免dangling pointer to QWaveData
        \ref QXcbBackingStore.resize, QMrOfs::enableComposition
    */
    QWaveData* setWaveformSize(WV_ID wid, const QSize& sz);           // override
    /*!
        \brief
        一般由waveview管理waveform生命期，无需手动destroy
    */
    void destroyWaveform(WV_ID wid);                                                // override
    
    /*!
       绑定waveform 和waveview 
       \note
        v1:  NOT IMPLEMENTED       
        v2:  NOT IMPLEMENTED       
    */
    void bindWaveform(WV_ID wid, WV_ID vid);                                    // override
    
    /*!
        \brief wid to QWaveData*
    */
    QWaveData* waveformRenderBuffer(WV_ID wid);                        // override
    
    /*!
        \note 
        v1: NOT IMPLEMENTED
        v2: NOT IMPLEMENTED        
    */
    void lockWaveformRenderBuffer(WV_ID wid);                            // override
    
    /*!
        unlock, 如果不闪烁可不调用
        v1: NOT IMPLEMENTED
        v2: NOT IMPLEMENTED        
    */
    void unlockWaveformRenderBuffer(WV_ID wid);                       // override
    
    /*!
        \note waveview 坐标系
    */
    QPoint setWaveformPos(WV_ID wid, const QPoint& pos);                // override

    /*!
        \note
        v1: 提交波形更新并等待合成结束
        v2: NOT IMPLEMENTED(use endUpdateWaves instead)
    */
    void flushWaves(const QRegion& rgn = QRegion());                          // override
    
    /*!
        v1: 获得colorkey 用于填充波形控件
        v2: NOT IMPLEMENTED        
    */
    QRgb getColorKey() const;                                                              // override
    bool enableComposition(bool enable);                                         // override
    const char* getVersion();

    /*********************************************************************************/
    /*********************************V2 ADDITIONAL***********************************/
    /*********************************************************************************/
    /*!
        \brief 打开1个wave view
        \param container: wave view 的容器 widget, 所有z 序位于container 之上的popup region，在waveview 输出时被剔除
        \param vpt:          wave view 的位置和大小，相对screen    (TODO: 是否可以不要? 直接用container 的信息)
        \return opened wave view ID
        \note container(widget) 坐标系
        \note 算法:
        本view 的区域记为view_region，一般为view 的container widget 的范围
        查找container 所在的最邻近的祖先popup，记为popup
        找出z序在popup 之上的所有popups，并计算其其余的<union>，记为 upper_popups_region
        合成本view 的有效区域为:   comp_region = view_region <subtract> upper_popups_region
        \note 效果上等同于openWaveview(container) + setWaveviewport(vid, vpt)
    */
    virtual WV_ID openWaveview(const QWidget& container, const QRect& vpt);

    /*!
        \brief
        创建波形屏存储，如果创建后需要修改，调用setWaveformSize
        \return WV_ID
        \note 返回WV_ID 而非QWaveData*，调用waveformRenderBuffer 获得QWaveData*
        \note waveview 坐标系
        \ref  setWaveformSize
        \note 
        效果上等同于createWaveform(vid) + setWaveformSize(wid, sz)
        format 固定为ARGB32_Premultiplied
    */
    virtual WV_ID createWaveform(WV_ID vid, const QRect& rc);

    /*!
        \brief
        v2: NOT IMPLEMENTED
    */
    virtual void unbindWaveform(WV_ID wid, WV_ID vid);
    
    /*!
        \brief 开窗
        \param self: 标记作为popup 开窗的widget
        \note 
        这里的popup仍然是非native 的(qt so called ALLIEN WIDGET)，而非TLW(NATIVE WINDOW)
        对于Popup 记录的是z 序，先调用的z 序在下
        对于根窗口，必须先于其它popup 标记为popup，使其位于z序最下         >>> VERY IMPORTANT <<<
        如果popup 内有waveview, 必须先调用popup 再调用openwaveview，否则waveview 的container widget没有popup ancestor
        如果self 已存在，则不新增popup列表，也不改变z序，只重新计算有效区域。
    */
    virtual void openPopup(const QWidget& self); 
    
    /*!
        \brief 关窗
        \note 
        如果self为非z序最上端的popup，则self 及之上的所有popup 全部关闭
        如果self 不存在，则不改变popup列表，只重新计算有效区域。
    */
    virtual void closePopup(const QWidget& self);
    
    /*!
        \brief 增加waveform 的clip region，波形只在clip region 范围内输出(实际合成的范围是 waveform dirty region 和backingstore dirty region 合并后的region)。
        \param wid: waveform ID
        \param rgn: clip region
        \note 增加此接口的目的是进一步降低cpu/gpu开销。
        \note 如果flushWaves时没指定region( 空region)，则使用addWaveformClipRegion设置的region。
        \note waveform 坐标系
    */
    virtual void addWaveformClipRegion(WV_ID wid, const QRegion &rgn);
    virtual void addWaveformClipRegion(WV_ID wid, const QRect &rc);
    
    /*!
        \breif 清空waveform 的clip region
        \param  wid: waveform ID
        \note
        清空clip region 之后，waveform 不会有任何输出。
        waveform 坐标系。
        endUpdateWaves 自动清除所有波形的clip regions。
    */
    virtual void cleanWaveformClipRegion(WV_ID wid);

    /*!
        \brief 清空所有waveforms 的clip region
        \note flushWaves 会自动清空所有waveforms 的clip region
        endUpdateWaves 自动清除所有波形的clip regions。        
        */
    virtual void cleanAllWaveformClipRegion();

    /*!
        设置波形线程优先级(策略为SCHED_FIFO)
        \return 老的优先级
    */
    virtual int setCompositorThreadPriority(int newpri);

    /*!
        lock wave resources
    */
    virtual void beginUpdateWaves();
    
    /*!
        unlock wave resources, then flushwaves, and then clean up all waveforms dirty regions.
        如果已打开iview, 则波形不需要再flush了(只需更新)
    */
    virtual void endUpdateWaves();
      
    
    /*!
        \brief 打开iview
        \note 设置合适的z 序，iview 可以被popup 遮挡;
        设置合适的尺寸和位置，iview 可以有全屏效果;
        widget(container)坐标系;
        container widget 必须为非native window
    */     
    virtual WV_ID openIView(const QWidget &container, const QRect& vpt);

    /*!
        创建iviewform, mmap 方式
        ref. createWaveform
        \todo  USERPTR/DMABUF
        */
    virtual WV_ID createIViewform(WV_ID vid, const HDMI_CONFIG hcfg = HDMI_CONFIG_1280x720, Format fmt = Format_xBGR32);
    
    /*!
        将bits flip 到iview 的显示表面(copy flip), 再触发合成
        bits: mmap-ed v4l2 buffer bits in xBGR32 format, 如果app 通过waveformRenderBuffer 直接更新QWaveData.data, 则bits == NULL
    */
    virtual int flipIView(WV_ID vid, WV_ID wid, const QRegion &rgn, const unsigned char* bits = NULL);

    /*!
        关闭iview
        \note iview 内的所有viewform 及pixel buffer 全部释放
    */
    virtual void closeIView(WV_ID vid);

    /*!
        合成触发开关
      */
    virtual bool enableBSFlushing(bool enable);
    virtual bool enableWVFlushing(bool enable);
    virtual bool enableIVFlushing(bool enable);
    virtual bool enableCRFlushing(bool enable);   
#ifdef PAINT_WAVEFORM_IN_QT
    virtual void PaintWaveformLines(
        QImage* image,                      
        XCOORDINATE xStart,              //填充的X起点坐标                     
        const YCOORDINATE* y,              //y坐标集合                     
        const YCOORDINATE* yMax,           //y坐标最大值集合     (真实坐标*256)                
        const YCOORDINATE* yMin,           //y坐标最小值集合     (真实坐标*256)                                
        short yFillBaseLine,        //填充时的基线坐标, LINEMODE_FILL时有效                     
        unsigned int pointsCount,   //点的个数                     
        const QColor& color,        //颜色                     
        LINEMODE mode);             //作图方式    

    virtual void EraseRect(QImage* image, const QRect& rect);   //擦除矩形    
    virtual void DrawVerticalLine(QImage* image, XCOORDINATE x, YCOORDINATE y1, YCOORDINATE y2, const QColor& color = 0);   //画竖线（Pace）        
#endif

    // mrdp control {{{
    virtual bool startMRDPService();
    virtual void stopMRDPService();
    virtual bool MRDPServiceRunning();
    // }}}

private:
    Q_DISABLE_COPY(QMrOfs);
};

QT_END_NAMESPACE

#endif //QMrOfs_H
