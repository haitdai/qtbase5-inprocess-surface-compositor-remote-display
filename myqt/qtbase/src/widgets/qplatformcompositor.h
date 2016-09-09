#ifndef QPLATFORMCOMPOSITOR_H
#define QPLATFORMCOMPOSITOR_H

#include <QtCore/qglobal.h>
#include <QtWidgets/qabstractmrofs.h>
#include <qpa/qplatformbackingstore.h>            // QPlatformCompositor
#include <qpa/qplatformscreen.h>                  // deriving must include declaration of base class, qpa directory is 

QT_BEGIN_NAMESPACE

class Q_WIDGETS_EXPORT QPlatformCompositor: public QObject {
    Q_OBJECT 

/***************** base functions*************/    
public:      
    virtual QAbstractMrOfs::WV_ID openWaveview() = 0;    
    virtual void closeWaveview(QAbstractMrOfs::WV_ID vid) = 0;    
    virtual QAbstractMrOfs::WV_ID setWaveviewLayer(QAbstractMrOfs::WV_ID vid, int layer) = 0;    
    virtual void setWaveViewport(QAbstractMrOfs::WV_ID vid, const QRect& vpt) = 0;    
    virtual QAbstractMrOfs::WV_ID createWaveform(QAbstractMrOfs::WV_ID vid) = 0;                             
    //create waveform object, do NOT allocate waveform bits    
    virtual void destroyWaveform(QAbstractMrOfs::WV_ID wid) = 0;    
    virtual QWaveData* setWaveformSize(QAbstractMrOfs::WV_ID wid, const QSize& rc) = 0;     
    //allocate waveform bits by rc    
    virtual void bindWaveform(QAbstractMrOfs::WV_ID wid, QAbstractMrOfs::WV_ID vid) = 0;    
    virtual QWaveData* waveformRenderBuffer(QAbstractMrOfs::WV_ID wid) = 0;    
    virtual QPoint setWaveformPos(QAbstractMrOfs::WV_ID wid, const QPoint& pos) = 0;    
    virtual void flushWaves(const QRegion& rgn) = 0;    
    virtual QRgb getColorKey() const = 0;    
    virtual bool enableComposition(bool enable) = 0;
    
/***************** v2 additional functions*************/
public:
    //compositor thread
    virtual int setCompositorThreadPriority(int newpri) = 0;

    //popups
    virtual void openPopup(const QWidget& self) = 0;
    virtual void closePopup(const QWidget& self) = 0;

    //waves
    virtual QAbstractMrOfs::WV_ID openWaveview(const QWidget& container, const QRect& vpt) = 0;
    virtual QAbstractMrOfs::WV_ID createWaveform(QAbstractMrOfs::WV_ID vid, const QRect& rc) = 0;
    virtual void unbindWaveform(QAbstractMrOfs::WV_ID wid, QAbstractMrOfs::WV_ID vid) = 0;
    virtual void addWaveformClipRegion(QAbstractMrOfs::WV_ID wid, const QRegion &rgn) = 0;
    virtual void addWaveformClipRegion(QAbstractMrOfs::WV_ID wid, const QRect &rgn) = 0;    
    virtual void cleanWaveformClipRegion(QAbstractMrOfs::WV_ID wid) = 0;
    virtual void cleanAllWaveformClipRegion() = 0;
    virtual void lockWaveResources() = 0;
    virtual void unlockWaveResources() = 0;        
    virtual void submitWaveData() = 0;          //all wave in all layers data will be submitted
    virtual void flushWaves() = 0;                //flush to screen (compositing)

    //backingstore
    virtual void lockBackingStoreResources() = 0;
    virtual void unlockBackingStoreResources() = 0;

    //iview
    virtual QAbstractMrOfs::WV_ID openIView(const QWidget &container, const QRect& vpt) = 0;
    virtual void closeIView(QAbstractMrOfs::WV_ID vid) = 0;        
    virtual QAbstractMrOfs::WV_ID createIViewform(QAbstractMrOfs::WV_ID vid, const HDMI_CONFIG hcfg,
            QAbstractMrOfs::Format format = QAbstractMrOfs::Format_xBGR32) = 0;
    virtual void submitIViewData(QAbstractMrOfs::WV_ID vid, QAbstractMrOfs::WV_ID wid, const unsigned char *bits) = 0; 
    virtual void flushIView(QAbstractMrOfs::WV_ID wid) = 0;   //flush to screen(compositing)
    virtual void lockIViewResources() = 0;
    virtual void unlockIViewResources() = 0;

    //flushBackingStore, flushCursor 不在此声明，因为它们只在qt内部调用
    
    //flushing control
    virtual bool enableBSFlushing(bool enable) = 0;          //ui 更新触发合成
    virtual bool enableWVFlushing(bool enable) = 0;       //wave 更新触发合成
    virtual bool enableIVFlushing(bool enable) = 0;         //iview 更新触发合成
    virtual bool enableCRFlushing(bool enable) = 0;      //cursor 更新(位置)触发合成


/***************** v4 additional functions*************/
#ifdef PAINT_WAVEFORM_IN_QT
public:    
    virtual void paintWaveformLines(QImage* image,        
                                                    QAbstractMrOfs::XCOORDINATE xStart,        
                                                    const QAbstractMrOfs::YCOORDINATE* y,        
                                                    const QAbstractMrOfs::YCOORDINATE* yMax,        
                                                    const QAbstractMrOfs::YCOORDINATE* yMin,        
                                                    short yFillBaseLine,        
                                                    unsigned int pointsCount,        
                                                    const QColor& color,        
                                                    QAbstractMrOfs::LINEMODE mode) = 0; 
    
    virtual void eraseRect(QImage* image, const QRect& rect) = 0;        
    virtual void drawVerticalLine(QImage* image,             
                                                  QAbstractMrOfs::XCOORDINATE x,             
                                                  QAbstractMrOfs::YCOORDINATE y1,             
                                                  QAbstractMrOfs::YCOORDINATE y2,             
                                                  const QColor& color) = 0;
#endif   

public:
    // mouse cursor
    virtual void changeCursor(QCursor * widgetCursor, QWindow *window) = 0;
    virtual void pointerEvent(const QMouseEvent &e) = 0;

#ifdef QT_MRDP
public:
    // mrdp service 
    virtual bool startMRDPService() = 0;        // mrofs call
    virtual void stopMRDPService() = 0;         // mrofs call
    virtual bool MRDPServiceRunning() = 0;
    virtual const QRegion& screenDirtyRegion() = 0;   // mrdp call
    virtual const QImage& screenImage() = 0;    // mrdp call
    virtual void lockScreenResources() = 0;
    virtual void unlockScreenResources() = 0;    
#endif //QT_MRDP    
};

QT_END_NAMESPACE

#endif      //QPLATFORMCOMPOSITOR_H
