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
        \param tlw  ָ��QMrOfs�ĸ�widget, QMrOfs ��tlw.windowHandle�ںϳɣ�����֮��Ĳ���
        \note
        QMrOfs ��ʵ��������win  �ϣ������win �ڵĺϳɣ�˫��ʱ������ÿ��screen�϶���1��win��
        ���һ��screen�ϴ����˶���ɼ���win����ôֻҪappԸ�⣬������Ϊÿ��win����һ��QMrOfsʵ����ÿ��QMrOfsʵ�������win�ڵĺϳɡ�
        ʵ��ʱ�����Ӷ�һ��screen��ֻ����1��win�ļ�飬����ÿ��screen��ֻ����1��win(���Լ��)��
        ��QMrOfsʵ���󶨵�win�϶����󶨵�screen��ԭ����:�����screen ��win��
        \note
        ʵ�ʱ��ϳɵ����ֻ��2+N ������:
       1x  �ַ���(��win)��Ԥ��ֻ��1����
       Nx  ������(���wave view ����1������Ĳ�����)��
       1x  iView ��(��MIPI ģ���ṩ���ݣ������ϲ�����cpu)��
        \ref QScreenPrivate::platformCompositor
        \note
        ��֧������QMrOfs::setWindow �Ľӿڡ�
        һ��ָ��win����QMrOfs�����뱣֤win��QMrOfs�����ڼ�ʼ�մ��ڡ�
        \note 
        Ӧ�õ��� QWidget::windowHandle �ɻ��QWidget ��TLW
        \note
        ����Nx ����������ԭ����:
        1,  �����ϳɣ�����������ʱҲҪ�ü�popup�����򽫻���popup�����ڳ��������Ĳ��Σ�
        2,  Ӧ�ò����ڻ��������ε�ʱ���Լ��ü�popup��
        3,  ʹ��N ���������ĺϳɴ�����ʹ��1��������һ����ֻ�����Դ���������һЩ�ռ�ռ�á�
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
        1 ��waveview ����1������waveform��ÿ��waveform���Լ��Ķ����洢
        \note ����ֻ�ں���ԭ���ϱ����˶�v1 �ļ��ݣ�������������:
        v1:  ��screen�ϵ�view
        v2:  NOT IMPLEMENTED
    */
    WV_ID openWaveview();                                                                   // override
    
    /*!
        \brief
        �ر�waveview ʱ����������waveformȫ���Զ�����
        ���1��waveform���ڶ��waveview����������1��waveview�Ĺرյ���waveform����ǰ���������������waveview�Ը�waveform������
    */
    void closeWaveview(WV_ID vid);                                                      // override
    
    /*!
        \brief ��������waveview λ��
        \note widget(container) ����ϵ
    */
    void setWaveViewport(WV_ID vid, const QRect& vpt);                       // override
    
    /*!
        v1: ������������
        v2:  NOT IMPLEMENTED        
    */
    WV_ID createWaveform(WV_ID vid);                                                // override, deprecated
    
    /*!
        \brief ֻ��waveform�ߴ磬���ı��waweview�İ󶨹�ϵ
        \note
        resize ֮ǰ��APPӦ��֤
        1, compositor ֮ǰ�Ĺ��������꣬��û���µ�compositing ����
        2, wave �߳������е�painter ���ͷ�(painter �Ỻ��QWaveData ��ָ��)
        �Ա���dangling pointer to QWaveData
        \ref QXcbBackingStore.resize, QMrOfs::enableComposition
    */
    QWaveData* setWaveformSize(WV_ID wid, const QSize& sz);           // override
    /*!
        \brief
        һ����waveview����waveform�����ڣ������ֶ�destroy
    */
    void destroyWaveform(WV_ID wid);                                                // override
    
    /*!
       ��waveform ��waveview 
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
        unlock, �������˸�ɲ�����
        v1: NOT IMPLEMENTED
        v2: NOT IMPLEMENTED        
    */
    void unlockWaveformRenderBuffer(WV_ID wid);                       // override
    
    /*!
        \note waveview ����ϵ
    */
    QPoint setWaveformPos(WV_ID wid, const QPoint& pos);                // override

    /*!
        \note
        v1: �ύ���θ��²��ȴ��ϳɽ���
        v2: NOT IMPLEMENTED(use endUpdateWaves instead)
    */
    void flushWaves(const QRegion& rgn = QRegion());                          // override
    
    /*!
        v1: ���colorkey ������䲨�οؼ�
        v2: NOT IMPLEMENTED        
    */
    QRgb getColorKey() const;                                                              // override
    bool enableComposition(bool enable);                                         // override
    const char* getVersion();

    /*********************************************************************************/
    /*********************************V2 ADDITIONAL***********************************/
    /*********************************************************************************/
    /*!
        \brief ��1��wave view
        \param container: wave view ������ widget, ����z ��λ��container ֮�ϵ�popup region����waveview ���ʱ���޳�
        \param vpt:          wave view ��λ�úʹ�С�����screen    (TODO: �Ƿ���Բ�Ҫ? ֱ����container ����Ϣ)
        \return opened wave view ID
        \note container(widget) ����ϵ
        \note �㷨:
        ��view �������Ϊview_region��һ��Ϊview ��container widget �ķ�Χ
        ����container ���ڵ����ڽ�������popup����Ϊpopup
        �ҳ�z����popup ֮�ϵ�����popups���������������<union>����Ϊ upper_popups_region
        �ϳɱ�view ����Ч����Ϊ:   comp_region = view_region <subtract> upper_popups_region
        \note Ч���ϵ�ͬ��openWaveview(container) + setWaveviewport(vid, vpt)
    */
    virtual WV_ID openWaveview(const QWidget& container, const QRect& vpt);

    /*!
        \brief
        �����������洢�������������Ҫ�޸ģ�����setWaveformSize
        \return WV_ID
        \note ����WV_ID ����QWaveData*������waveformRenderBuffer ���QWaveData*
        \note waveview ����ϵ
        \ref  setWaveformSize
        \note 
        Ч���ϵ�ͬ��createWaveform(vid) + setWaveformSize(wid, sz)
        format �̶�ΪARGB32_Premultiplied
    */
    virtual WV_ID createWaveform(WV_ID vid, const QRect& rc);

    /*!
        \brief
        v2: NOT IMPLEMENTED
    */
    virtual void unbindWaveform(WV_ID wid, WV_ID vid);
    
    /*!
        \brief ����
        \param self: �����Ϊpopup ������widget
        \note 
        �����popup��Ȼ�Ƿ�native ��(qt so called ALLIEN WIDGET)������TLW(NATIVE WINDOW)
        ����Popup ��¼����z ���ȵ��õ�z ������
        ���ڸ����ڣ�������������popup ���Ϊpopup��ʹ��λ��z������         >>> VERY IMPORTANT <<<
        ���popup ����waveview, �����ȵ���popup �ٵ���openwaveview������waveview ��container widgetû��popup ancestor
        ���self �Ѵ��ڣ�������popup�б�Ҳ���ı�z��ֻ���¼�����Ч����
    */
    virtual void openPopup(const QWidget& self); 
    
    /*!
        \brief �ش�
        \note 
        ���selfΪ��z�����϶˵�popup����self ��֮�ϵ�����popup ȫ���ر�
        ���self �����ڣ��򲻸ı�popup�б�ֻ���¼�����Ч����
    */
    virtual void closePopup(const QWidget& self);
    
    /*!
        \brief ����waveform ��clip region������ֻ��clip region ��Χ�����(ʵ�ʺϳɵķ�Χ�� waveform dirty region ��backingstore dirty region �ϲ����region)��
        \param wid: waveform ID
        \param rgn: clip region
        \note ���Ӵ˽ӿڵ�Ŀ���ǽ�һ������cpu/gpu������
        \note ���flushWavesʱûָ��region( ��region)����ʹ��addWaveformClipRegion���õ�region��
        \note waveform ����ϵ
    */
    virtual void addWaveformClipRegion(WV_ID wid, const QRegion &rgn);
    virtual void addWaveformClipRegion(WV_ID wid, const QRect &rc);
    
    /*!
        \breif ���waveform ��clip region
        \param  wid: waveform ID
        \note
        ���clip region ֮��waveform �������κ������
        waveform ����ϵ��
        endUpdateWaves �Զ�������в��ε�clip regions��
    */
    virtual void cleanWaveformClipRegion(WV_ID wid);

    /*!
        \brief �������waveforms ��clip region
        \note flushWaves ���Զ��������waveforms ��clip region
        endUpdateWaves �Զ�������в��ε�clip regions��        
        */
    virtual void cleanAllWaveformClipRegion();

    /*!
        ���ò����߳����ȼ�(����ΪSCHED_FIFO)
        \return �ϵ����ȼ�
    */
    virtual int setCompositorThreadPriority(int newpri);

    /*!
        lock wave resources
    */
    virtual void beginUpdateWaves();
    
    /*!
        unlock wave resources, then flushwaves, and then clean up all waveforms dirty regions.
        ����Ѵ�iview, ���β���Ҫ��flush��(ֻ�����)
    */
    virtual void endUpdateWaves();
      
    
    /*!
        \brief ��iview
        \note ���ú��ʵ�z ��iview ���Ա�popup �ڵ�;
        ���ú��ʵĳߴ��λ�ã�iview ������ȫ��Ч��;
        widget(container)����ϵ;
        container widget ����Ϊ��native window
    */     
    virtual WV_ID openIView(const QWidget &container, const QRect& vpt);

    /*!
        ����iviewform, mmap ��ʽ
        ref. createWaveform
        \todo  USERPTR/DMABUF
        */
    virtual WV_ID createIViewform(WV_ID vid, const HDMI_CONFIG hcfg = HDMI_CONFIG_1280x720, Format fmt = Format_xBGR32);
    
    /*!
        ��bits flip ��iview ����ʾ����(copy flip), �ٴ����ϳ�
        bits: mmap-ed v4l2 buffer bits in xBGR32 format, ���app ͨ��waveformRenderBuffer ֱ�Ӹ���QWaveData.data, ��bits == NULL
    */
    virtual int flipIView(WV_ID vid, WV_ID wid, const QRegion &rgn, const unsigned char* bits = NULL);

    /*!
        �ر�iview
        \note iview �ڵ�����viewform ��pixel buffer ȫ���ͷ�
    */
    virtual void closeIView(WV_ID vid);

    /*!
        �ϳɴ�������
      */
    virtual bool enableBSFlushing(bool enable);
    virtual bool enableWVFlushing(bool enable);
    virtual bool enableIVFlushing(bool enable);
    virtual bool enableCRFlushing(bool enable);   
#ifdef PAINT_WAVEFORM_IN_QT
    virtual void PaintWaveformLines(
        QImage* image,                      
        XCOORDINATE xStart,              //����X�������                     
        const YCOORDINATE* y,              //y���꼯��                     
        const YCOORDINATE* yMax,           //y�������ֵ����     (��ʵ����*256)                
        const YCOORDINATE* yMin,           //y������Сֵ����     (��ʵ����*256)                                
        short yFillBaseLine,        //���ʱ�Ļ�������, LINEMODE_FILLʱ��Ч                     
        unsigned int pointsCount,   //��ĸ���                     
        const QColor& color,        //��ɫ                     
        LINEMODE mode);             //��ͼ��ʽ    

    virtual void EraseRect(QImage* image, const QRect& rect);   //��������    
    virtual void DrawVerticalLine(QImage* image, XCOORDINATE x, YCOORDINATE y1, YCOORDINATE y2, const QColor& color = 0);   //�����ߣ�Pace��        
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
