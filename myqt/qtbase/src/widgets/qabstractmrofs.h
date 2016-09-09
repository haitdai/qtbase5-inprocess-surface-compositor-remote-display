#ifndef QABSTRACTMROFS_H
#define QABSTRACTMROFS_H
#include <QtGui/qimage.h>

typedef enum {
	HDMI_CONFIG_1280x720,       //1280*720, RGB24
	HDMI_CONFIG_1024x768,       //1024*768, RGB24
	HDMI_CONFIG_1680x1050,      //1680*1050, RGB24
	HDMI_CONFIG_1280x800,       //1280x800, RGB24	
	HDMI_CONFIG_800x600,        //800X600, RGB24
	HDMI_CONFIG_1050x1680      // 1050x1680, RGB24
} HDMI_CONFIG;

class QImage;
typedef QImage QWaveData;

//#define PAINT_WAVEFORM_IN_QT    1

class Q_WIDGETS_EXPORT QAbstractMrOfs {
public:
    typedef int WV_ID;
    static QAbstractMrOfs* createInstance(QWidget *tlw, QObject *parent = 0);
    virtual WV_ID openWaveview() = 0;
    virtual void closeWaveview(WV_ID vid) = 0;
    virtual void setWaveViewport(WV_ID vid, const QRect& vpt) = 0;
    virtual WV_ID createWaveform(WV_ID vid) = 0;
    virtual QWaveData* setWaveformSize(WV_ID wid, const QSize& sz) = 0;
    virtual void destroyWaveform(WV_ID wid) = 0;
    virtual void bindWaveform(WV_ID wid, WV_ID vid) = 0;
    virtual QWaveData* waveformRenderBuffer(WV_ID wid) = 0;
    virtual void lockWaveformRenderBuffer(WV_ID wid) = 0;
    virtual void unlockWaveformRenderBuffer(WV_ID wid) = 0;
    virtual QPoint setWaveformPos(WV_ID wid, const QPoint& pos) = 0;
    virtual void flushWaves(const QRegion& rgn = QRegion()) = 0;
    virtual QRgb getColorKey() const = 0;
    virtual bool enableComposition(bool enable) = 0;
    virtual const char* getVersion() = 0;

    virtual WV_ID openWaveview(const QWidget& container, const QRect& vpt) = 0;
    virtual WV_ID createWaveform(WV_ID vid, const QRect& rc) = 0;
    virtual void unbindWaveform(WV_ID wid, WV_ID vid) = 0;
    virtual void openPopup(const QWidget& self) = 0; 
    virtual void closePopup(const QWidget& self) = 0;
    virtual void addWaveformClipRegion(WV_ID wid, const QRegion &rgn) = 0;
    virtual void addWaveformClipRegion(WV_ID wid, const QRect &rc) = 0;
    virtual void cleanWaveformClipRegion(WV_ID wid) = 0;
    virtual void cleanAllWaveformClipRegion() = 0;
    virtual int setCompositorThreadPriority(int newpri) = 0;
    virtual void beginUpdateWaves() = 0;
    virtual void endUpdateWaves() = 0;
    
    enum Format {
        //QT@ARGB, qt default format, fastest
        Format_ARGB32_Premultiplied     = 0,
        // SLOW since multiplied realtime
        Format_ARGB32                             = 0x1,
        // ARGB without ALPHA
        Format_RGB32                               = 0x2,
        //IVIEW@RGBx_LSBFirst [inte GPU supported without necessory to swap byte order],  CPU do NOT care RGB components in each pixel        
        Format_xBGR32                             = 0x3,          
        //QT@RGBA8888, which is ARGB in Little Endian, and ABGR in Big Endian [powerVR GPU supported without necessory to swap byte order]
        // SLOW since qt format conversion
        Format_RGBA32                             = 0x4,          
        //QT@RGBA8888_Premultiplied, SLOW since qt format conversion
        Format_RGBA32_Premultiplied     = 0x5,          
        Format_RGB_MASK                        = 0xFFFF,
        //xv, 16bpp packed, faster, NOT USED
        Format_YUY2                                 = 0x10000,      
        //xv, 16bpp packed, faster, non-render: IVIEW@YUV, NOT USED
        Format_UYVY                                 = 0x20000,      
        Format_YUV_MASK                        = 0xFFFF0000,
    };    
    
    virtual WV_ID openIView(const QWidget &container, const QRect& vpt) = 0;
    virtual WV_ID createIViewform(WV_ID vid, const HDMI_CONFIG hcfg = HDMI_CONFIG_1280x720, Format fmt = Format_xBGR32) = 0;
    virtual int flipIView(WV_ID vid, WV_ID wid, const QRegion &rgn, const unsigned char* bits = NULL) = 0;
    virtual void closeIView(WV_ID vid) = 0;
    virtual bool enableBSFlushing(bool enable) = 0;
    virtual bool enableWVFlushing(bool enable) = 0;
    virtual bool enableIVFlushing(bool enable) = 0;
    virtual bool enableCRFlushing(bool enable) = 0;  
    
#ifdef PAINT_WAVEFORM_IN_QT
    /*!
        wave form related data type
    */
    typedef short XCOORDINATE;
    typedef int     YCOORDINATE;
    typedef enum {
        LINEMODE_DRAW,
        LINEMODE_FILL
    } LINEMODE;
    virtual void PaintWaveformLines(
        QImage* image,                      
        XCOORDINATE xStart, 
        const YCOORDINATE* y, 
        const YCOORDINATE* yMax,
        const YCOORDINATE* yMin,
        short yFillBaseLine,
        unsigned int pointsCount,  
        const QColor& color,    
        LINEMODE mode) = 0;  
    virtual void EraseRect(QImage* image, const QRect& rect) = 0;   
    virtual void DrawVerticalLine(QImage* image, XCOORDINATE x, YCOORDINATE y1, YCOORDINATE y2, const QColor& color) = 0; 
#endif    

    // mrdp control {{{
    virtual bool startMRDPService() = 0;
    virtual void stopMRDPService() = 0;
    virtual bool MRDPServiceRunning() = 0;
    // }}}
    
protected:
    static QAbstractMrOfs *self;
};

#endif    // QABSTRACTMROFS_H
