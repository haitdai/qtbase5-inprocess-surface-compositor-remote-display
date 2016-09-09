#include <QtWidgets>
#include "dialog.h"

Dialog *tlw;

#define  UPDATE_DATA 1
//#define WAVE_STYLE_UPDATING   1         //QT updateEvent 方式更新波形，不定义表示直接更新, 用于对比性能提升
#define WAVE_STYLE_DIRECT     1         //direct 方式更新波形
const int blocking_ui_time = 500;       //ms  5000,  不能设为0否则100% CPU, timer触发频率
const int wave_update_interval = 40;   //ms 40, 25      更新频率对cpu开销影响较大
const int wave_update_width = 20;        //控制每次波形更新宽度,  更新范围对cpu开销影响较小
//wave width and height must be the same as wave_widget's
const int wave_width = 1000;              //每道波形宽度 221  500
const int scrollarea_width = 1100;
const int wave_widget_width= scrollarea_width;        //波形区域宽度, 控制滚动  500
const int wave_height = 300;              //每道波形高度 99   1000
const int scrollarea_height= 700;     //波形区域高度,控制滚动  1000
const int wave_widget_height = scrollarea_height*2;
const int wave_v_sep = 10;               //波形垂直方向间隔
const int wave_h_sep = 10;               //波形水平方向间隔

const HDMI_CONFIG s_hdmi_cfg = HDMI_CONFIG_1050x1680;    //try HDMI_CONFIG_1280x720, HDMI_CONFIG_1024x768, HDMI_CONFIG_1680x1050, HDMI_CONFIG_1280x800, HDMI_CONFIG_800x600, HDMI_CONFIG_1050x1680
int iview_width = 1280;
int iview_widget_width = 1280;
int iview_height = 720;
int iview_widget_height = 720;
const int iview_v_sep = 60;
const int iview_v_sep_more = 10;
const int iview_h_sep = 10;

const int wave_horiz_num = 1;   // 8
const int wave_vert_num = 2;    // 8      

RealtimeDisplay::RealtimeDisplay(QWidget* parent) : QWidget(parent) {};
RealtimeDisplay::~RealtimeDisplay() {}

typedef short XCOORDINATE;
typedef uint YCOORDINATE;
typedef enum {
    LINEMODE_DRAW,
    LINEMODE_FILL
} LINEMODE;

static void paintWaveformLines(
    QImage* image,
    XCOORDINATE xStart,
    const YCOORDINATE* y,
    const YCOORDINATE* yMax,
    const YCOORDINATE* yMin,
    short yFillBaseLine,
    unsigned int pointsCount,
    const QColor& color,
    LINEMODE mode);
static void drawWaveformLines(QImage* image,
    XCOORDINATE xStart,
    const YCOORDINATE* y,
    const YCOORDINATE* yMax,
    const YCOORDINATE* yMin,
    unsigned int pointsCount,
    const QColor& color);
static void fillWaveformLines(QImage* image,                     
    XCOORDINATE xStart,                     
    const YCOORDINATE* y,                     
    const YCOORDINATE* yMin,                    
    short yFillBaseLine,                    
    unsigned int pointsCount,                    
    const QColor& color);
static void eraseRect(QImage* image, const QRect& rect);
static void drawVerticalLine(QImage* image, 
    XCOORDINATE x, 
    YCOORDINATE y1, 
    YCOORDINATE y2, 
    const QColor& color);
/*! 
    [wave thread]
*/
static void paintWaveformLines(
    QImage* image,
    XCOORDINATE xStart,
    const YCOORDINATE* y,
    const YCOORDINATE* yMax,
    const YCOORDINATE* yMin,
    short yFillBaseLine,
    unsigned int pointsCount,
    const QColor& color,
    LINEMODE mode)
{
    if (mode == LINEMODE_DRAW) {        
        drawWaveformLines(image,
            xStart,
            y,
            yMax,
            yMin,
            pointsCount,
            color);    
    } else if (mode == LINEMODE_FILL) {
        fillWaveformLines(image,
        xStart,
        y,
        yMin,
        yFillBaseLine,
        pointsCount,
        color);    
    } else {    
        // do nothing
    }
}

// restore left shift outside
static inline short rightShift8Bits(int y) {
    return (y >> 8);
}

static void drawWaveformLines(QImage* image,
    XCOORDINATE xStart,
    const YCOORDINATE* y,
    const YCOORDINATE* yMax,
    const YCOORDINATE* yMin,
    unsigned int pointsCount,
    const QColor& color) 
{ 
    if (image != 0) {
        uchar *dataPtr = const_cast<uchar*>( const_cast<const QImage*>(image)->bits() );        
        int w = image->width();        
        int h = image->height();
        for(int index = 0; index < pointsCount; index++) {
            if (xStart + index >= 0 && xStart + index < w) {
                if (rightShift8Bits(y[index]) >= 0 && rightShift8Bits(y[index]) < h && 
                    rightShift8Bits(yMax[index]) >= 0 && rightShift8Bits(yMax[index]) < h) {
                    int r = color.red() << 8;
                    int g = color.green() << 8;
                    int b = color.blue() << 8;
                    int diff = rightShift8Bits(yMax[index]) - rightShift8Bits(y[index]) + 1;
                    int rd = r / diff;                    
                    int gd = g / diff;                    
                    int bd = b / diff;                    
                    unsigned char * base = dataPtr + rightShift8Bits(y[index]) * w * 4 + (xStart + index) * 4;                                
                    for( int yy = rightShift8Bits(y[index]); yy < rightShift8Bits(yMax[index]) + 1; yy++) {
                        *(base + 0) = (unsigned char)(b >> 8);
                        *(base + 1) = (unsigned char)(g >> 8);
                        *(base + 2) = (unsigned char)(r >> 8);
                        *(base + 3) = 0xFF;
                        base += w * 4;
                        r -= rd;
                        g -= gd;
                        b -= bd;
                    }                
                }                
                if (rightShift8Bits(y[index]) >= 0 && rightShift8Bits(y[index]) < h && 
                    rightShift8Bits(yMin[index]) >= 0 && rightShift8Bits(yMin[index]) < h ) {                    
                    int r = color.red() << 8;                    
                    int g = color.green() << 8;                    
                    int b = color.blue() << 8;
                    int diff = rightShift8Bits(y[index]) - rightShift8Bits(yMin[index]) + 1;
                    int rd = r / diff;                    
                    int gd = g / diff;                    
                    int bd = b / diff;                    
                    unsigned char * base = dataPtr + (rightShift8Bits(y[index]) -1 ) * w * 4 + (xStart + index) * 4;                                
                    for (int yy = rightShift8Bits(y[index]) - 1; yy > rightShift8Bits(yMin[index]) - 1; yy--) {
                        *(base + 0) = (unsigned char)(b >> 8);
                        *(base + 1) = (unsigned char)(g >> 8);
                        *(base + 2) = (unsigned char)(r >> 8);
                        *(base + 3) = 0xFF;
                        base -= w * 4;
                        r -= rd;
                        g -= gd;
                        b -= bd; 
                    }
                }
            }
        }
    }
}
    
static void fillWaveformLines(QImage* image,                     
    XCOORDINATE xStart,                     
    const YCOORDINATE* y,                     
    const YCOORDINATE* yMin,                    
    short yFillBaseLine,                    
    unsigned int pointsCount,                    
    const QColor& color) {    
    if (image != 0) {
        uchar *dataPtr = const_cast<uchar*>(const_cast<const QImage*>(image)->bits());        
        int w = image->width();        
        int h = image->height();        
        for (int index = 0; index < pointsCount; index++) {            
            if (xStart + index >= 0 && xStart + index < w) {                
                if (rightShift8Bits(y[index]) >= 0 && rightShift8Bits(y[index]) < h && 
                    rightShift8Bits(yMin[index]) >= 0 && rightShift8Bits(yMin[index]) < h) {
                    int r = color.red() << 8;                    
                    int g = color.green() << 8;                    
                    int b = color.blue() << 8;
                    int diff = rightShift8Bits(y[index]) - rightShift8Bits(yMin[index]) + 1;                    
                    int rd = r / diff;                    
                    int gd = g / diff;                    
                    int bd = b / diff;                    
                    unsigned char * base = dataPtr + rightShift8Bits(y[index])  * w * 4 + (xStart + index) * 4;                                
                    for (int yy = rightShift8Bits(y[index]); yy > rightShift8Bits(yMin[index]) - 1; yy--) {                         
                        *(base + 0) = (unsigned char)(b >> 8);                         
                        *(base + 1) = (unsigned char)(g >> 8);                         
                        *(base + 2) = (unsigned char)(r >> 8);                         
                        *(base + 3) = 0xFF;                        
                        base -= w * 4;                        
                        r -= rd;                        
                        g -= gd;                        
                        b -= bd;                    
                    }                
                }
                if (rightShift8Bits(y[index]) >= 0 && rightShift8Bits(y[index]) < h && 
                    yFillBaseLine >= 0 && yFillBaseLine < h ) {                      
                    unsigned char * base = dataPtr + ( rightShift8Bits(y[index]) + 1)  * w * 4 + (xStart + index) * 4;                                
                    for( int yy = rightShift8Bits(y[index]) + 1; yy < yFillBaseLine; yy++) {                         
                        *(base + 0) = color.blue();                         
                        *(base + 1) = color.green();                         
                        *(base + 2) = color.red();                         
                        *(base + 3) = 0xFF;                        
                        base += w * 4;                    
                    }                
                }            
            }        
        }    
    }
}

/*! 
    [wave thread]
*/
static void eraseRect(QImage* image, const QRect& rect)
{
    if ( image != 0) {        
        uchar *dataPtr = const_cast<uchar*>( const_cast<const QImage*>(image)->bits());        
        int w = image->width();        
        int h = image->height();        
        for (int y = rect.top(); y < rect.bottom(); y++) {
            if ( y >= 0 && y < h )            {                
                if ( rect.left() >= 0 && rect.right() < w )                {                    
                    uchar *d = dataPtr + (y * w * 4) + rect.left() * 4;                     
                    memset( d, 0, rect.width() * 4);                
                }
            }
        }
    }
}

/*! 
    [wave thread]
*/
static void drawVerticalLine(QImage* image, 
    XCOORDINATE x, 
    YCOORDINATE y1, 
    YCOORDINATE y2, 
    const QColor& color)
{    
    if ( image != 0)    {        
        uchar *dataPtr = const_cast<uchar*>( const_cast<const QImage*>(image)->bits() );        
        int w = image->width();        
        int h = image->height();        
        if (y1 > y2) {            
            uint temp = y1;            
            y1 = y2;            
            y2 = temp;        
        }        
        if ( x >= 0 && x < w ) {            
            y1= 0.1, y2 = 0.2;
            int y = ( y1 >> 8 ); 
            if ( y >= 0 && y < h ) {                
                if ( (y1 & 0xFF ) != 0 )                {                    
                    uchar t = 256 - (y1 & 0xFF);                    
                    uchar *    d = dataPtr + ( y * w * 4 ) + x * 4;                     
                    *d       = (color.red() * t) >> 8;                    
                    *(d + 1 )= (color.green() * t) >> 8;                    
                    *(d + 2 )= (color.blue() * t) >> 8;                    
                    *(d + 3 )= 0xFF;                    
                    y++;                
                } else {                
                
                }            
            }            
            for( ; y < (y2 >> 8); y++) {
                if ( y >= 0 && y < h ) {                    
                    uchar *    d = dataPtr + (y * w * 4 ) + x * 4;                     
                    *d       = color.red();                    
                    *(d + 1 )= color.green();                    
                    *(d + 2 )= color.blue();                    
                    *(d + 3 )= 0xFF;                
                }            
            }            
            if ( (y2 & 0xFF ) != 0) {                
                y++;
                if (y >= 0 && y < h) {                    
                    uchar t = (y2 & 0xFF);                    
                    uchar *    d = dataPtr + ( y * w * 4 ) + x * 4;                     
                    *d       = (color.red() * t) >> 8;                    
                    *(d + 1 )= (color.green() * t) >> 8;                    
                    *(d + 2 )= (color.blue() * t) >> 8;                    
                    *(d + 3 )= 0xFF;                
                }
            }
        }
    }
}




//frontend uses this
void RealtimeDisplay::moveEvent(QMoveEvent *ev)
{
   qDebug("moveEvent: pos(%d,%d), old pos(%d,%d)", ev->pos().x(), ev->pos().y(), ev->oldPos().x(), ev->oldPos().y()); 
   QPoint tl = mapToGlobal(QPoint(0,0));
   qDebug("moveEvent: mapToGlobal(%d,%d)", tl.x(), tl.y());
//   tlw->mrofs->setWaveformPos(tlw->wid0, tl);
}

WaveWidget::WaveWidget(QWidget* parent) : QWidget(parent) {}
WaveWidget::~WaveWidget() {}

void WaveWidget::updateWaves(const QRect& rc)
{
	update(rc);
}

//用 colorkey 绘制波形占位控件（这里画前景，画背景也可）
void WaveWidget::paintEvent(QPaintEvent *ev)
{
#ifdef WAVE_STYLE_UPDATING
	int h = 0;
	foreach(WaveformInfo* wf, tlw->wr->wfs) {
		QPainter painter(this);
		painter.drawImage(wf->wi, h * wave_height, *wf->data, 0, 0, wave_update_width, wave_height);
		h++;
	}
#else

//正常的control 绘制
  
	qWarning("WaveWidget::paintEvent");
    static int k;
//    return;
    QPainter p(this);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);          // old
    QVector<QRect> rects = ev->region().rects();  
    for(int i = 0; i < rects.size(); i++) {
        const QRect &rc = rects[i];
        qDebug("rc: %d, %d, %d, %d", rc.top(), rc.left(), rc.width(), rc.height());    
    }

    QRect rc = rect();
    if(k % 2) {
        p.setBrush(QBrush(QColor(0xFFFF0000)));                   // Qt::red
        p.fillRect(rc, QBrush(QColor(0xFFFF00FF)));                               //Qt::blue: argb
        p.drawEllipse(rc);
        p.drawText(500, 500, "11111111");
//            p.drawImage(QPointF(rc.left(), rc.top()), bufferfly);
    } else {
        p.setBrush(QBrush(QColor(0xFF00FF00)));                   //Qt::green
           p.fillRect(rc, QBrush(QColor(0, 0, 0xFF, 0xFF)));   // r, g, b, a
           p.drawEllipse(rc);
        p.drawText(rc, "222222222");
//            p.drawImage(QPointF(rc.left(), rc.top()), bufferfly);
    }
         
    k++;
#endif
}

#if 0
#include <sys/time.h>
static inline unsigned long tv_diff(struct timeval *tv1, struct timeval *tv2)
{
    return (tv2->tv_sec - tv1->tv_sec) * 1000 +  (tv2->tv_usec - tv1->tv_usec) / 1000;
}
#endif

char dst[1024][1024];
char src[1024][1024];
#if 0
static int frames;
struct timeval timespan;
#endif

//no event loop
void WaveThread::run()
{
    wt = new QTimer(); 
    wt->setInterval(wave_update_interval);   //half time will be blocked
    connect(wt, SIGNAL(timeout()), wr, SLOT(renderSlot()));
    sleep(5);
    wt->start();
    exec();
    return;

   while(!bExit)
   {
#if 0
static  struct timeval startTime, endTime;
    unsigned long diffTime;
    gettimeofday(&startTime, NULL);    
//      msleep(wave_update_interval);
        memcpy(dst, src, 1024*1024);
        continue;
#endif

#ifdef WAVE_STYLE_UPDATING
       tlw->wavewidget->update();
#else
//       tlw->wavedata0Lock.lock();
//     WaveRender::render(tlw->wavedata0);
 //      tlw->wavedata0Lock.unlock();
      WaveRender::render2(tlw->wavedata0);      

    //flush waves
//    tlw->mrofs->flushWaves(); 
#endif

#if 0
		gettimeofday(&endTime, NULL);
		diffTime = tv_diff(&startTime, &endTime);

		frames ++;
		diffTime = tv_diff(&timespan, &endTime);
		if(diffTime > 5000) {
			qWarning("compositing FPS: %ld, in seconds(%ld), frames(%ld)", (frames*1000)/diffTime, diffTime/1000, frames);
			timespan = endTime;
			frames = 0;
		}
#endif
   }	// while
}

void WaveRender::suspend()
{
    qWarning("suspend called");
    running = false;    
}

void WaveRender::resume()
{
    running = true;
}

void WaveRender::renderSlot()
{
#ifdef WAVE_STYLE_UPDATING
	foreach(WaveformInfo* wf, wfs) {
		renderX(*wf);    
	}
#else
    if (!running)
        return;

    if(wfs.size() < 1) 
        return;

    tlw->mrofs->beginUpdateWaves();

    foreach(WaveformInfo* wf, wfs) {
        renderX(*wf);    
    }

    tlw->mrofs->endUpdateWaves();

#endif
}

void WaveRender::render3(int loopCount)
{
#if 1
#   if 1
    static float f0 = 99.0, f1 = 9.0, f2 = 9.0;
    for (int i = 0; i < 999999; i ++)
        f1 = f1 * f2;
#   else
    static double d0 = 99.0, d1 = 9.0, d2 = 9.0;
    for (int i = 0; i < 999999; i ++)
        d1 = d1 * d2;
#   endif
#else
    static int i0 = 99, i1 = 9, i2 = 9;
    for (int i = 0; i < loopCount; i ++)
        i1 = i0 * i1;
#endif
}

//#define USE_RECTF
#define USE_LINEARGRADIENT
#include <QLinearGradient>
void WaveRender::render2(QPaintDevice* dev)
{
////    qDebug("WaveRender::render");

    //画第一道波形
#ifdef UPDATE_DATA
    QPainter p(dev);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    static int i = 0;
    //模拟波形更新：切换2张qimage, 1 次更新 wave_update_width * wave_height 范围, 超出
#if 1
    static int wi2;
#ifdef USE_RECTF
    float xf = wi2 * 1.0 + 0.1;
    float widthf = wave_update_width * 1.0 + 0.1;
    float heightf = wave_height * 1.0 + 0.1;
    QRectF urcf(xf, 0, widthf, heightf);
#else
    QRect urc(wi2, 0, wave_update_width, wave_height);
#endif

    static bool bDo = true;
    if (bDo) {   //i<50
        QThread::msleep(5000);
        qWarning("render2 ...........");
#ifdef USE_LINEARGRADIENT
        QLinearGradient lg(QPointF(0.5, 0.0), QPointF(0.5, 1.0));
        lg.setCoordinateMode(QGradient::ObjectBoundingMode);
        lg.setColorAt(0,    QColor(98, 98, 98)); 
//        lg.setColorAt(0.25, QColor(158, 158, 150)); 
//        lg.setColorAt(0.5,  QColor(116, 116, 100)); 
//        lg.setColorAt(0.75, QColor(66, 66, 60)); 
        lg.setColorAt(1,    QColor(34, 34, 34)); 
#       ifdef USE_RECTF
        urcf.setHeight(urcf.height()/10);
        urcf.setWidth(urcf.width()/10);
        p.fillRect(urcf, QBrush(lg));
#       else
        urc.setHeight(urc.height()/20);
        urc.setWidth(urc.width()/10);
        p.fillRect(urc, QBrush(lg));
#       endif
//        bDo = false;
#else
#       ifdef USE_RECTF
        p.fillRect(urcf, QBrush(Qt::black));
#       else
        p.fillRect(urc, QBrush(Qt::black));
#       endif
#endif
    } else {
        /*
#ifdef USE_LINEARGRADIENT   
        QLinearGradient lg(QPointF(0.5, 0.0), QPointF(0.5, 1.0));
        lg.setColorAt(0, QColor(128, 128, 120)); 
        lg.setColorAt(0.5, QColor(96, 96, 90)); 
        lg.setColorAt(1, QColor(48, 48, 0)); 
#       ifdef USE_RECTF
        p.fillRect(urcf, QBrush(lg));
#       else
        p.fillRect(urc, QBrush(lg));
#       endif
#else
#       ifdef USE_RECTF
        p.fillRect(urcf, QBrush(Qt::black));
#       else
        p.fillRect(urc, QBrush(Qt::black));
#       endif
#endif
*/
    }

    wi2 += wave_update_width;
    if(wi2 > wave_width - wave_update_width)   //re-wind
        wi2 = 0;
#else
    QImage *wd = static_cast<QImage*>(dev);
    uchar *bits = wd->bits();
    int count = wd->size().width()*wd->size().height();
//    qDebug("bits: %p, pixel count: %d", bits, count);
    if (i == 0) { 
        for (int i = 0; i < count/10; i ++)
        {
            int j = i * 4;
            bits[j] = 0xFF;
            bits[j+1] = 0;
            bits[j+2] = 0;
            bits[j+3] = 0xFF;
        }
    } else {
        for (int i = 0; i < count/10; i ++)
        {
            int j = i * 4;
            bits[j] = 0xFF;
            bits[j+1] = 0;
            bits[j+2] = 0xFF;
            bits[j+3] = 0;
        }
    }
#endif
    i = (i + 1) % 100;
#endif  //UPDATE_DATA

    //todo 画第二道波形


    //qDebug("Dialog::updateWave, flushWaves");
}
 
void WaveRender::render_0(QPaintDevice* dev, QAbstractMrOfs::WV_ID wid)
{
#ifdef UPDATE_DATA
    QPainter p(dev);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
    static int k = 0;

#if 1
    static int wi = 0;
    QRect urc(wi, 0, wave_update_width, wave_height);
    QRect erc = QRect(
                (urc.left()+urc.right())/2 - urc.width()/4,
                (urc.top()+urc.bottom())/2 - urc.height()/4,
                urc.width()/2,
                urc.height()/2);

    //设置到合成器, waveform 坐标系
    tlw->mrofs->addWaveformClipRegion(wid, urc);

    //背景，透明, aliasing
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
    QPainter::CompositionMode old = p.compositionMode();
    p.setCompositionMode(QPainter::CompositionMode_Source);    //默认mode 为SourceOver, 改为Source便于填充透明色
    p.fillRect(urc, QBrush(Qt::transparent));
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    //前景，不透明, anti aliasing
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
    if (k < 1) {
        p.setBrush(QBrush(Qt::red));
        p.drawEllipse(erc);
//        k = 1;
    } else {
//        p.drawImage(erc, tlw->wavefile1, erc);
        erc.adjust(0, erc.height()/10, 0, erc.height()/10); 
        p.setBrush(QBrush(Qt::green));
        p.drawEllipse(erc);
//        k = 0;
    }
    wi += wave_update_width;
    if(wi > wave_width - wave_update_width) {   //re-wind
        wi = 0;
        k = (0==k) ? 1 : 0;
    }
#else
    QImage *wd = static_cast<QImage*>(dev);
    uchar *bits = wd->bits();
    int count = wd->size().width()*wd->size().height();
//    qDebug("bits: %p, pixel count: %d", bits, count);
    if (k == 0) { 
        for (int i = 0; i < count/3; i ++)
        {
            int j = i * 4;
            bits[j] = 0xFF;
            bits[j+1] = 0;
            bits[j+2] = 0;
            bits[j+3] = 0xFF;
        }
        k = 1;
    } else {
        for (int i = 0; i < count/3; i ++)
        {
            int j = i * 4;
            bits[j] = 0xFF;
            bits[j+1] = 0;
            bits[j+2] = 0xFF;
            bits[j+3] = 0;
        }
        k = 0;
    }
#endif
#endif  //UPDATE_DATA

//    QThread::msleep(200);
    //todo 画第二道波形


    //qDebug("Dialog::updateWave, flushWaves");
}

/*!
    O2 - by_qt:                 3.0 (includes compositing)
    O2 - NOT by_qt:          1.0 (includes compositing)
*/
//#define DRAW_WAVEFORM_BY_QT 1
void WaveRender::renderX(WaveformInfo& wf)
{
    QRect urc(wf.wi, 0, wave_update_width, wave_height);	// 
    QRect erc = QRect(
                (urc.left()+urc.right())/2 - urc.width()/4,
                (urc.top()+urc.bottom())/2 - urc.height()/4,
                urc.width()/2,
                urc.height()/2);

#ifdef WAVE_STYLE_UPDATING
	emit updateWaves(urc);
#else
    //设置到合成器, waveform 坐标系
	tlw->mrofs->addWaveformClipRegion(wf.wid, urc);
#endif


#ifdef DRAW_WAVEFORM_BY_QT
    QPainter p(wf.data);
    //背景，透明, aliasing
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
    QPainter::CompositionMode old = p.compositionMode();
    p.setCompositionMode(QPainter::CompositionMode_Source);    //默认mode 为SourceOver, 改为Source便于填充透明色
    p.fillRect(urc, QBrush(Qt::transparent));

    //前景，不透明, anti aliasing
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    if (wf.k < 1) {
        p.setBrush(QBrush(wf.color));
        p.drawEllipse(erc);
    } else {
        erc.adjust(0, erc.height()/10, 0, erc.height()/10); 
        p.setBrush(QBrush(wf.color2));
        p.drawEllipse(erc);
    }    
#else
    eraseRect(wf.data, urc);

    uint ymax[] = {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169};
    uint y[] = {50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159};
    uint ymin[] = {40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129};
    for(int i = 0; i < 20; i ++) {
        y[i] = y[i] << 8;
        ymax[i] = ymax[i] << 8;
        ymin[i] = ymin[i] << 8;
    }

    paintWaveformLines(
        wf.data,
        urc.x(),
        y,
        ymax,
        ymin,
        wave_height / 2,
        20,     // paintWaveformLines
        wf.k < 1 ? QColor(0xFF, 0, 0, 0xFF) : QColor(0, 0, 0xFF, 0xFF),       // r g b a
        LINEMODE_DRAW);

    drawVerticalLine(wf.data, 
        urc.x(),
        0, 
        200 << 8, 
        wf.k < 1 ? QColor(0xFF, 0, 0, 0xFF) : QColor(0, 0, 0xFF, 0xFF));       // r g b a
#endif

    wf.wi += wave_update_width;
    if(wf.wi > wave_width - wave_update_width) {   //re-wind
        wf.wi = 0;
        wf.k = (0==wf.k) ? 1 : 0;
    }
}

ScrollAreaWidget::ScrollAreaWidget(QWidget* parent) : QScrollArea(parent) {}
void ScrollAreaWidget::scrollContentsBy(int xpos, int ypos)
{
//    tlw->wavewidget->setUpdatesEnabled(false);
    QScrollArea::scrollContentsBy(xpos, ypos);   //默认行为：重画 scroll viewport
}

void ScrollAreaWidget::paintEvent(QPaintEvent *ev)
{
    qWarning("ScrollAreaWidget::paintEvent");
    QVector<QRect> rects = ev->region().rects();    
    for(int i = 0; i < rects.size(); i ++) { 
        const QRect& rc = rects[i];
        qDebug("rc: %d, %d, %d, %d", rc.top(), rc.left(), rc.width(), rc.height());
    }       
}

void RealtimeDisplay::paintEvent(QPaintEvent *ev)
{
    qWarning("RealtimeDisplay::paintEvent");
    QVector<QRect> rects = ev->region().rects();
    for(int i = 0; i < rects.size(); i ++) { 
        const QRect& rc = rects[i];
        qDebug("rc: %d, %d, %d, %d", rc.top(), rc.left(), rc.width(), rc.height());
    }    
}
void RealtimeView::paintEvent(QPaintEvent *ev)
{
//    qWarning("RealtimeView::paintEvent");
    QVector<QRect> rects = ev->region().rects();    
    for(int i = 0; i < rects.size(); i ++) { 
        const QRect& rc = rects[i];
//        qDebug("rc: %d, %d, %d, %d", rc.top(), rc.left(), rc.width(), rc.height());
    }       
}

//定时更新波形
void Dialog::blockUI()
{
#if 1
    //qWarning("dialog update");
    //update(QRect(0, 0, 1280, 256));		// QRect(512, 512, 256, 256)
    wavewidget->update();
#endif

#if 0
    //qWarning("wavewidget update");
    //this->wavewidget->update();     // QRect(50, 50, 100, 100)
#endif

#if 0    
    qWarning("realtimedisplay update");
    this->display->update();
#endif    
}

void Dialog::paintEvent(QPaintEvent *ev)
{
    static int k;

    QPainter p(this);
//	qWarning("Dialog::paintEvent");
    //背景，透明, aliasing
//    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, false);
//    QPainter::CompositionMode old = p.compositionMode();
//    p.setCompositionMode(QPainter::CompositionMode_Source);    //默认mode 为SourceOver, 改为Source便于填充透明色
//    p.fillRect(this->geometry(), QBrush(Qt::green));

//    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);          // old
    QVector<QRect> rects = ev->region().rects();
    for(int i = 0; i < rects.size(); i ++) { 
        const QRect& rc = rects[i];
//    qDebug("rc: %d, %d, %d, %d", rc.top(), rc.left(), rc.width(), rc.height());
        if(k % 2) {
            p.setBrush(QBrush(QColor(0x00FF0000)));                   // Qt::red
            p.fillRect(rc, QBrush(QColor(0xFF0000FF)));                               //Qt::blue
            p.drawEllipse(rc);
            p.drawText(500, 500, "11111111");
//            p.drawImage(QPointF(rc.left(), rc.top()), bufferfly);
        } else {
            p.setBrush(QBrush(QColor(0xFF00FF00)));                   //Qt::green
               p.fillRect(rc, QBrush(QColor(0, 0, 0xFF, 0x55)));   // r, g, b, a
               p.drawEllipse(rc);
            p.drawText(rc, "222222222");
//            p.drawImage(QPointF(rc.left(), rc.top()), bufferfly);
        }
    }

#if 0
    QRgb  c;
    if(i%2) {
        c = QRgb(0xFFFF0000);       // ARGB
    } else {
        c = QRgb(0xFF0000FF);
    }
    //fill background with colorkey
    p.fillRect(0, 0, width(), height(), QColor(c));


    p.drawText(QPointF(iterator.x.toReal() + x,
                        y.toReal()), visualTab);
#endif

    k++;
}

void Dialog::testQRectPar(QRect rc)
{
    qDebug("rc: %d, %d, %d, %d", rc.top(), rc.left(), rc.width(), rc.height());
    qDebug("this: %p", this);
    this->desktopGeometry = rc;
}

void Dialog::hScrollValueChanged(int val)
{
    hpos = 0 - (val - 0);
    QPoint wvpos(hpos, vpos); 
    qDebug("wvpos: %d, %d, val: %d", hpos, vpos, vsbar->value());
    //fixme
//    mrofs->setWaveformPos(this->wid0, wvpos);
}

void Dialog::vScrollValueChanged(int val)
{
    vpos = 0 - (val - 60);   //screen cordinate
    QPoint wvpos(hpos, vpos);
    qDebug("wvpos: %d, %d, val: %d, %d", hpos, vpos, val, vsbar->value());
    //fixme
//   mrofs->setWaveformPos(this->wid0, wvpos);
}

void Dialog::hScrollRangeChanged(int min, int max)
{
    hmin = min;
    hmax = max;
    hpos = vpos = 0;
}

void Dialog::vScrollRangeChanged(int min, int max)
{
    vmin = min;
    vmax = max;
    qDebug("vmin %d, vmax %d", vmin, vmax);
}


static void loadImage(const QString &fileName, QImage *image)
{    
    bool loadOK = image->load(fileName);
    if(!loadOK) {
        qDebug("\n\n\n >>>>> loadImage failed :%s <<<<< \n\n\n", qPrintable(fileName));
    }
    QSize resultSize = image->size();
    QImage fixedImage(resultSize, QImage::Format_ARGB32_Premultiplied);    
    QPainter painter(&fixedImage);    
    painter.setCompositionMode(QPainter::CompositionMode_Source);    
    painter.fillRect(fixedImage.rect(), Qt::transparent);    
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);    
    painter.drawImage(QPoint(0,0), *image);    
    painter.end();    
    *image = fixedImage;    
}

#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>
//! [Dialog constructor part1]
Dialog::Dialog(bool hasWaves, bool hasIView, int offset, QWidget *parent) : 
    QWidget(parent), wavedata0Lock(QMutex::Recursive), orientingLock(QMutex::Recursive),
    hasWaves(hasWaves), hasIView(hasIView), wavewidget(NULL)
{
    if(!parent) {   //TLW

    tlw = this;
    
	QPalette pal;
#if 1
	pal.setBrush(QPalette::Background, QColor(0xFF, 0xFF, 0xFF, 0xFF));		// if opaque, more paintEvent
	this->setPalette(pal);
	this->setAutoFillBackground(true);
#endif

    loadImage("/merak/butterfly.png",  &bufferfly);
#if 0
    QFontDatabase database;    
    fontTree->setColumnCount(1);    
    fontTree->setHeaderLabels(QStringList() << tr("Font"));    
    foreach (QString family, database.families()) {        
        const QStringList styles = database.styles(family);        
        if (styles.isEmpty())            
            continue;        
        QTreeWidgetItem *familyItem = new QTreeWidgetItem(fontTree);        
        familyItem->setText(0, family);        
        familyItem->setCheckState(0, Qt::Unchecked);        
        familyItem->setFlags(familyItem->flags() | Qt::ItemIsTristate);        
        foreach (QString style, styles) {            
            QTreeWidgetItem *styleItem = new QTreeWidgetItem(familyItem);            
            styleItem->setText(0, style);            
            styleItem->setCheckState(0, Qt::Unchecked);            
            styleItem->setData(0, Qt::UserRole, QVariant(database.weight(family, style)));            
            styleItem->setData(0, Qt::UserRole + 1, QVariant(database.italic(family, style)));        
        }    
    }
#endif

#if 0
    desktopGeometry = QApplication::desktop()->availableGeometry(0);
    setWindowTitle(tr("SIP Dialog Example"));
#endif

    //loading wave datas: 0 and 1
    bool ok = false;
    ok = wavefile0.load("./wave0.png");
    qDebug("!!!!load wave0.png ok (%d)", ok);
    ok = wavefile1.load("./wave1.png");
    qDebug("!!!!load wave1.png ok (%d)", ok);

    qDebug("Dialog::Dialog, offset(%d)", offset);
    this->setGeometry(offset, offset, SCREEN_WIDTH, SCREEN_HEIGHT);          //horizontal

    //界面布局 start
    view = new RealtimeView(this);
#if 0
	pal.setBrush(QPalette::Background, QColor(0xFF, 0, 0xFF, 0xFF));
	view->setPalette(pal);
	view->setAutoFillBackground(true);    
#endif

    button = new QPushButton(view);       //button's parent is view
    button->setText("rotate/openpopup");
    button->setGeometry(0, 0, 200, 60);

    buttonClose = new QPushButton(view);       //button's parent is view
    buttonClose->setText("closepopup");
    buttonClose->setGeometry(200, 0, 200, 60);

    buttonToggleMRDP = new QPushButton(view);
    buttonToggleMRDP->setText("stop mrdp");
    buttonToggleMRDP->setGeometry(400, 0, 200, 60);
#if 1
    display = new RealtimeDisplay(this);   //display's parent is dialog
    display->setGeometry(0, 60, wave_widget_width, wave_widget_height); 
#if 1
        pal.setBrush(QPalette::Background, QColor(0xFF, 0, 0xFF, 0xFF));
        display->setPalette(pal);
        display->setAutoFillBackground(true);       

    //display->setAttribute(Qt::WA_OpaquePaintEvent, true);    // �������ǿ��opaque
#endif	

    scrollArea = new ScrollAreaWidget(view);
#if 1
	pal.setBrush(QPalette::Background, QColor(0xFF, 0, 0xFF, 0xFF));    //0x55
	scrollArea->setPalette(pal);
	scrollArea->setAutoFillBackground(true);        
#endif	    
    //给scrollArea/scrollArea->viewport() 设置 grabGesture 没用:
    this->grabGesture(Qt::TapGesture);
    this->grabGesture(Qt::PanGesture);
        scrollArea->setWidget(display);    //display's parent is scrollArea
        scrollArea->setGeometry(0, 60, scrollarea_width, scrollarea_height);
            gridLayout = new QGridLayout(display); //ref. Qt::AAImmediateWidgetCreation
            display->setLayout(gridLayout);    //gridlayout's parent is display
            wavewidget = new WaveWidget(display);
#if 1
	pal.setBrush(QPalette::Background, QColor(0xFF, 0, 0xFF, 0xFF));    //0x55
	wavewidget->setPalette(pal);
	wavewidget->setAutoFillBackground(true);        
#endif	
    gridLayout->addWidget(wavewidget);   //wavewidget's parent is gridlayout

#if 0    
    statusbar = new QLabel(view);      //statusbar's parent is view
    statusbar->setText("statusbar");
    statusbar->setGeometry(0, 800, 1050, 50);
#endif

    //scrollbar
    QScrollBar *hsbar, *vsbar;
    hsbar = scrollArea->horizontalScrollBar();
    vsbar = scrollArea->verticalScrollBar();
    connect(hsbar, SIGNAL(valueChanged(int)), this, SLOT(hScrollValueChanged(int)));
    connect(vsbar, SIGNAL(valueChanged(int)), this, SLOT(vScrollValueChanged(int)));
    connect(hsbar, SIGNAL(rangeChanged(int, int)), this, SLOT(hScrollRangeChanged(int, int)));
    connect(vsbar, SIGNAL(rangeChanged(int, int)), this, SLOT(vScrollRangeChanged(int, int)));
    this->hsbar = hsbar;
    this->vsbar = vsbar;
    //界面布局 end   
#endif

    //波形更新定时器 start
    timer = new QTimer(this); 
    timer->setInterval(blocking_ui_time * 2);   //half time will be blocked
    connect(timer, SIGNAL(timeout()), this, SLOT(blockUI()));
    timer->start();
    //波形更新定时器 end



    //meta object/method infomation 确认
#if 0
    int mc = mrofs->metaObject()->methodCount();
    const QMetaObject *mobj = mrofs->metaObject();
    QMetaMethod mmthod;
    QByteArray mthodinfo;
    for(int i = 0; i < mc; i ++){
        mmthod = mobj->method(i);
        mthodinfo = mmthod.methodSignature(); 
        qDebug("meta info ----");
        qDebug("%s", mthodinfo.constData());
    }
#endif

    //波形viewport/data
#if 1
    //mrofs, popup 初始化
    qWarning(">>>>app dialog consturctor");
//    QAbstractMrOfs *mrofs = new QMrOfsV4(this);
    QAbstractMrOfs *mrofs = QAbstractMrOfs::createInstance(this);
    if(!mrofs->MRDPServiceRunning()) {
        mrofs->startMRDPService();
    } else {
        qWarning("MRDPService already running");
    }
    buttonToggleMRDP->setText("stop MRDP");
    this->mrofs = mrofs;
    mrofs->openPopup(*this);                               //标记tlw为首个 popup
    qWarning("QMrOfsV4 constructed");

    //wave view
    QWaveData *wavedata;
    QAbstractMrOfs::WV_ID wid;
    QRect vpt(0, 60, wave_widget_width, wave_widget_height);                          //viewport, tlw坐标系
    QAbstractMrOfs::WV_ID vid = mrofs->openWaveview(*this, vpt);
    this->vid = vid;
    qWarning("openWaveview OK");

    //波形
    QRect rc(0, 0, wave_width, wave_height);             //waveform size, wv 坐标系 
    QPoint pos = QPoint(0, 10);                 //重设 waveform pos, wv 坐标系
    WaveformInfo *wf = NULL;
    this->wr = new WaveRender(true);
	connect(this->wr, SIGNAL(updateWaves(QRect)), this->wavewidget, SLOT(updateWaves(QRect)));		//Qt::AutoConnection
	// 8x8 waveforms in 1 single waveview
	for(int i = 0; i < wave_horiz_num; i++) {
		for(int j = 0; j < wave_vert_num; j++) {
#if 1
		int m = i, n = j;
		if (i > wave_widget_width / wave_width) {
			m = i % (wave_widget_width / wave_width);
		}
		if (j > wave_widget_height / wave_height) {
			n = j % (wave_widget_height / wave_height);
		}
		rc = QRect(0, 0, wave_width, wave_height);             //waveform size, wv 坐标系 
		wid = mrofs->createWaveform(vid, rc);               
		qWarning("createWaveform 1 OK");
		//bits 指针
		wavedata = mrofs->waveformRenderBuffer(wid);
		pos = QPoint(0+wave_width*m, 10+wave_height*n);                 //重设 waveform pos, wv 坐标系
		mrofs->setWaveformPos(wid, pos);
		qWarning("setWaveformPos 1 OK");
		//
		wf = new WaveformInfo();
		wf->wid = wid;
		wf->vid = vid;
		wf->color = Qt::green;
		wf->color2 = Qt::red;
		wf->wi = 0;                         //initial x pos
		wf->k = 0;                          //initial k
		wf->data = wavedata;
		wr->wfs.push_back(wf);
#endif
		}
	}

#if 0
    rc = QRect(0, 0, wave_width, wave_height);             //waveform size, wv 坐标系 
    wid = mrofs->createWaveform(vid, rc);               
    qWarning("createWaveform 1 OK");
    //bits 指针
    wavedata = mrofs->waveformRenderBuffer(wid);
    pos = QPoint(0, 10);                 //重设 waveform pos, wv 坐标系
    mrofs->setWaveformPos(wid, pos);
    qWarning("setWaveformPos 1 OK");
    //
    wf = new WaveformInfo();
    wf->wid = wid;
    wf->vid = vid;
    wf->color = Qt::green;
    wf->color2 = Qt::red;
    wf->wi = 0;                         //initial x pos
    wf->k = 0;                          //initial k
    wf->data = wavedata;
    wr->wfs.push_back(wf);
#endif

#if 0
    rc = QRect(0, 0 + wave_height, wave_width, wave_height); //紧邻第一道波形下方 
    wid = mrofs->createWaveform(vid, rc);               
    qWarning("createWaveform 2 OK");
    //bits 指针
    wavedata = mrofs->waveformRenderBuffer(wid);
    pos = QPoint(0, 10 + wave_height);       //+10 pixels on vertical orientation
    mrofs->setWaveformPos(wid, pos);
    qWarning("setWaveformPos 2 OK");
    wf = new WaveformInfo();
    wf->wid = wid;
    wf->vid = vid;
    wf->color = Qt::red;
    wf->color2 = Qt::green;
    wf->wi = 0;                         //initial x pos
    wf->k = 0;                          //initial k
    wf->data = wavedata;
    wr->wfs.push_back(wf);
#endif

#if 0
    rc = QRect(0, 0 + wave_height*2, wave_width, wave_height); //紧邻第一道波形下方 
    wid = mrofs->createWaveform(vid, rc);               
    qWarning("createWaveform 3 OK");
    //bits 指针
    wavedata = mrofs->waveformRenderBuffer(wid);
    pos = QPoint(0, 10 + wave_height*2);       //+10 pixels on vertical orientation
    mrofs->setWaveformPos(wid, pos);
    qWarning("setWaveformPos 3 OK");
    wf = new WaveformInfo();
    wf->wid = wid;
    wf->vid = vid;
    wf->color = Qt::blue;
    wf->color2 = Qt::yellow;
    wf->wi = 0;                         //initial x pos
    wf->k = 0;                          //initial k
    wf->data = wavedata;
    wr->wfs.push_back(wf);
#endif

    // test exceed screen range ...
#if 0
    rc = QRect(0, 0 + wave_height*3, wave_width, wave_height); //紧邻第一道波形下方 
    wid = mrofs->createWaveform(vid, rc);               
    qWarning("createWaveform 4 OK");
    //bits 指针
    wavedata = mrofs->waveformRenderBuffer(wid);
    pos = QPoint(0, 10 + wave_height*3);       //+10 pixels on vertical orientation
    mrofs->setWaveformPos(wid, pos);
    qWarning("setWaveformPos 4 OK");
    wf = new WaveformInfo();
    wf->wid = wid;
    wf->vid = vid;
    wf->color = Qt::yellow;
    wf->color2 = Qt::blue;
    wf->wi = 0;                         //initial x pos
    wf->k = 0;                          //initial k
    wf->data = wavedata;
    wr->wfs.push_back(wf);
#endif

    //wave thread
    this->wth = new WaveThread(wr);
    this->wr->moveToThread(this->wth);
    //QThread::sleep(2);
    this->wth->start();    
    qWarning("wavethread started");  

    //ready to go
    mrofs->enableComposition(true);
    qWarning("enableComposition");

#if 1 
    mrofs->enableBSFlushing(true);
    mrofs->enableCRFlushing(true);     //cursor flushing
    mrofs->enableWVFlushing(true); 
    mrofs->enableIVFlushing(false);     //60FPS cause mouse cursor very lagged
#endif

#endif
#if 0
    groupBox = new QGroupBox(scrollArea);
    groupBox->setTitle(tr("SIP Dialog Example"));
    QGridLayout *gridLayout = new QGridLayout(groupBox);
    groupBox->setLayout(gridLayout);
//! [Dialog constructor part1]

//! [Dialog constructor part2]
    QLineEdit* lineEdit = new QLineEdit(groupBox);
    lineEdit->setText(tr("Open and close the SIP"));
    lineEdit->setMinimumWidth(220);

    QLabel* label = new QLabel(groupBox);
    label->setText(tr("This dialog resizes if the SIP is opened")); 
    label->setMinimumWidth(220);

    QPushButton* button = new QPushButton(groupBox);
    button->setText(tr("Close Dialog"));
    button->setMinimumWidth(220);
//! [Dialog constructor part2]

//! [Dialog constructor part3]
    if (desktopGeometry.height() < 400)
        gridLayout->setVerticalSpacing(80);
    else
        gridLayout->setVerticalSpacing(150);

    gridLayout->addWidget(label);
    gridLayout->addWidget(lineEdit);
    gridLayout->addWidget(button);
//! [Dialog constructor part3]

//! [Dialog constructor part4]
    scrollArea->setWidget(groupBox);
    QHBoxLayout* layout = new QHBoxLayout();
    layout->addWidget(scrollArea);
    setLayout(layout);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//! [Dialog constructor part4]
#endif

//! [Dialog constructor part5]
#if 0
    connect(button, SIGNAL(clicked()),
        qApp, SLOT(closeAllWindows()));
#endif
    
/*  DHT desktop 
    connect(QApplication::desktop(), SIGNAL(workAreaResized(int)),
        this, SLOT(desktopResized(int)));
*/
//DHT
#if 0
    QApplication *app =  static_cast<QApplication*>(QApplication::instance());
    QScreen *scrn = app->primaryScreen();
    scrn->setOrientationUpdateMask(Qt::PortraitOrientation | Qt::InvertedPortraitOrientation | Qt::LandscapeOrientation | Qt::InvertedLandscapeOrientation);
    connect(scrn, SIGNAL(orientationChanged(Qt::ScreenOrientation)), this, SLOT(handleScreenOrientationChanged(Qt::ScreenOrientation)));
#endif
//END    
    } else {        
        if(hasWaves && !hasIView) {
        this->setGeometry(offset, offset, 1050/2, 1680/2);   //相对parent
        QPalette pal;
        pal.setBrush(QPalette::Background, QColor(0x0, 0x0, 0x0, 0xFF));
        this->setPalette(pal);
        this->setAutoFillBackground(true);

        button = new QPushButton(this);    
        button->setText("rotate/openpopup");
        button->setGeometry(0, 0, 100, 60);

        buttonClose = new QPushButton(this);
        buttonClose->setText("closepopup");
        buttonClose->setGeometry(100, 0, 100, 60);

        tlw->mrofs->openPopup(*this);
        qWarning("openPopup in popup OK");
        
            connect(this, SIGNAL(suspendRender()), tlw->wr, SLOT(suspend()), Qt::BlockingQueuedConnection);
            connect(this, SIGNAL(resumeRender()), tlw->wr, SLOT(resume()), Qt::BlockingQueuedConnection);


            emit suspendRender();

            //wave view
            QWaveData *wavedata;
            QAbstractMrOfs::WV_ID wid;
            QRect vpt(0, 60, 1050/2, 1600/2);
            this->vid = tlw->mrofs->openWaveview(*this, vpt);
            qWarning("openWaveview in popup OK");

            //wave form
            QRect rc(0, 0, wave_width, wave_height);
            QPoint pos = QPoint(0, 10);            
            WaveformInfo *wf = NULL;

#if 1
            rc = QRect(0, 0, wave_width, wave_height);
            wid = tlw->mrofs->createWaveform(this->vid, rc); //注意vid
            qWarning("createWaveform in popup OK");
            //bits 指针
            wavedata = tlw->mrofs->waveformRenderBuffer(wid);
            pos = QPoint(0, 10);       //+10 pixels on vertical orientation
            tlw->mrofs->setWaveformPos(wid, pos);
            qWarning("setWaveformPos popup OK");
            wf = new WaveformInfo();
            wf->wid = wid;
            wf->vid = vid;
            wf->color = Qt::cyan;
            wf->color = Qt::magenta;
            wf->wi = 0;                         //initial x pos
            wf->k = 0;                          //initial k
            wf->data = wavedata;
            tlw->wr->wfs.push_back(wf);
#endif

#if 1
            rc = QRect(0, 0 + wave_height, wave_width, wave_height);
            wid = tlw->mrofs->createWaveform(this->vid, rc); //注意vid
            qWarning("createWaveform in popup OK");
            //bits 指针
            wavedata = tlw->mrofs->waveformRenderBuffer(wid);
            pos = QPoint(0, 10 + wave_height);       //+10 pixels on vertical orientation
            tlw->mrofs->setWaveformPos(wid, pos);
            qWarning("setWaveformPos popup OK");
            wf = new WaveformInfo();
            wf->wid = wid;
            wf->vid = vid;
            wf->color = Qt::cyan;
            wf->color = Qt::magenta;
            wf->wi = 0;                         //initial x pos
            wf->k = 0;                          //initial k
            wf->data = wavedata;
            tlw->wr->wfs.push_back(wf);
#endif
            emit resumeRender();
        }//hasWaves

#ifdef IVIEW_TEST
        if(hasIView && !hasWaves) {

          //init width, height by hdmi config
          switch(s_hdmi_cfg) {
          case HDMI_CONFIG_1280x720:
          default:
            iview_width = iview_widget_width = 1280;
            iview_height = iview_widget_height = 720;
          break;
          case HDMI_CONFIG_1024x768:
            iview_width = iview_widget_width = 1024;
            iview_height = iview_widget_height = 768;          
          break;
          case HDMI_CONFIG_1680x1050:
            iview_width = iview_widget_width = 1680;
            iview_height = iview_widget_height = 1050;          
          break;
          case HDMI_CONFIG_1280x800:
            iview_width = iview_widget_width = 1280;
            iview_height = iview_widget_height = 800;  
          break;
          case HDMI_CONFIG_800x600:
            iview_width = iview_widget_width = 800;
            iview_height = iview_widget_height = 600;
          break;          
          case HDMI_CONFIG_1050x1680:
            iview_width = iview_widget_width = 1050;
            iview_height = iview_widget_height = 1680;
          break;
          }
            
            this->setGeometry(offset, offset, iview_widget_width, iview_widget_height + iview_v_sep + iview_v_sep_more);   //720P
            QPalette pal;
            pal.setBrush(QPalette::Background, QColor(0x0, 0x0, 0x0, 0xFF));    //opaque black
            this->setPalette(pal);
            this->setAutoFillBackground(true);

            button = new QPushButton(this);    
            button->setText("rotate/openpopup");
            button->setGeometry(0, 0, 100, iview_v_sep);

            buttonClose = new QPushButton(this);
            buttonClose->setText("closepopup");
            buttonClose->setGeometry(100, 0, 100, iview_v_sep);

            tlw->mrofs->openPopup(*this);
            qWarning("openPopup in popup OK");


            //iview
            QWaveData *wavedata;
            QAbstractMrOfs::WV_ID wid;
            QRect vpt(0, iview_v_sep, iview_width, iview_height + iview_v_sep_more);
            if (s_hdmi_cfg == HDMI_CONFIG_1680x1050) {
                vpt.setY(0);
                vpt.setHeight(iview_height);
            }
            this->vid = tlw->mrofs->openIView(*this, vpt);
            if(this->vid != -1) {
                qWarning("openIView in popup OK");
            } else {
                qWarning("openIView in popup FAILED");
            }
            
            //iviewform
            wid = tlw->mrofs->createIViewform(this->vid, s_hdmi_cfg);       //format by default == QMrOfsV2::Format_xBGR32
            if(wid != -1) {
                qWarning("createIViewform in popup OK");
            } else {
                qWarning("createIVewform in popup FAILED");
            }
            
            //iview render
            this->ivwr = new IViewRender(true);
            
            wavedata = tlw->mrofs->waveformRenderBuffer(wid);       //iview data as QImage, RGBX8888
            QPoint pos  = QPoint(0, iview_v_sep_more);       //+iview_v_sep pixels on vertical orientation
            if(s_hdmi_cfg == HDMI_CONFIG_1680x1050) {
                pos.setY(0);
            }
            tlw->mrofs->setWaveformPos(wid, pos);
            qWarning("setWaveformPos popup OK");

            IViewformInfo *iwif = new IViewformInfo();
            iwif->vid = this->vid;            
            iwif->wid = wid;
            iwif->rect = QRect(0, 0, iview_width, iview_height);
            iwif->data = wavedata;
            this->ivwr->wfs.push_back(iwif);

			//iview thread
            this->ivth = new IViewThread(this->ivwr);
            tlw->ivwr = this->ivwr;
            tlw->ivth = this->ivth;
            tlw->ivwr->moveToThread(tlw->ivth);
            this->ivth->start(); 

//            emit resumeRender();
//            connect(this, SIGNAL(suspendRender()), tlw->ivwr, SLOT(suspend()), Qt::BlockingQueuedConnection);
//            connect(this, SIGNAL(resumeRender()), tlw->ivwr, SLOT(resume()), Qt::BlockingQueuedConnection);

        }
#endif	// IVIEW_TEST

        if(!hasIView && !hasWaves) {
            this->setGeometry(offset, offset, 1280/2, 720/2);   //720P
            QPalette pal;
            pal.setBrush(QPalette::Background, QColor(0x0, 0x0, 0x0, 0xFF));    //opaque black
            this->setPalette(pal);
            this->setAutoFillBackground(true);

            button = new QPushButton(this);    
            button->setText("rotate/openpopup");
            button->setGeometry(0, 0, 100, 60);

            buttonClose = new QPushButton(this);
            buttonClose->setText("closepopup");
            buttonClose->setGeometry(100, 0, 100, 60);

            tlw->mrofs->openPopup(*this);
            qWarning("openPopup in popup OK");
        }
    }//!has parent

    connect(button, SIGNAL(clicked()), this, SLOT(switchClicked()));
    connect(buttonClose, SIGNAL(clicked()), this, SLOT(closeClicked())); 
    connect(buttonToggleMRDP, SIGNAL(clicked()), this, SLOT(toggleMRDPClicked()));
}

void Dialog::handleScreenOrientationChanged(Qt::ScreenOrientation orient) 
{
#if 0
    QMrOfs::WV_ID wid = this->wid0;
    wavedata0Lock.lock();
    switch(orient) {
    case Qt::PortraitOrientation:
    {
        qDebug(">>> [orientationChanged] Qt::PortraitOrientation");
        QSize sz(wave_width, wave_height);  //same with realtimedisplay,  notice max texture size limit
        this->wavedata0 = this->mrofs->setWaveformSize(this->wid0, sz);
        QPoint pos = QPoint(0, 60);                 //位置, 屏幕坐标系
        this->mrofs->setWaveformPos(wid, pos);
    }
    break;
    case Qt::InvertedPortraitOrientation:
    {
        qDebug(">>> [orientationChanged] Qt::IvertedPortraitOrientation");
        QSize sz(wave_width, wave_height);  //same with realtimedisplay,  notice max texture size limit
        this->wavedata0 = this->mrofs->setWaveformSize(this->wid0, sz);
        QPoint pos = QPoint(0, 60);                 //位置, 屏幕坐标系
        this->mrofs->setWaveformPos(wid, pos);
    }
    break;
    case Qt::LandscapeOrientation:
    {
        qDebug(">>> [orientationChanged] Qt::LandscapeOrientation");
        QSize sz(wave_height, wave_width);  //same with realtimedisplay,  notice max texture size limit
        this->wavedata0 = this->mrofs->setWaveformSize(this->wid0, sz);
        QPoint pos = QPoint(0, 60);                 //位置, 屏幕坐标系
        this->mrofs->setWaveformPos(wid, pos);
    }
    break;
    case Qt::InvertedLandscapeOrientation:
    {
        qDebug(">>> [orientationChanged] Qt::InvertedLandscapeOrientation");
        QSize sz(wave_height, wave_width);  //same with realtimedisplay,  notice max texture size limit
        this->wavedata0 = this->mrofs->setWaveformSize(this->wid0, sz);
        QPoint pos = QPoint(0, 60);                 //位置, 屏幕坐标系
        this->mrofs->setWaveformPos(wid, pos);
    }
    break;
    default:
    break;
    }
    wavedata0Lock.unlock();

    //全屏刷新，因为backingstore 隐含切换了(texture streaming...)
    qDebug("handleScreenOrientationChanged, update");
    update();
#endif
}

QPopup::QPopup(QWidget *parent) : QWidget(parent)
{

}

QPopup::~QPopup() {}

void Dialog::closeClicked()
{
    if(this->isTopLevel()) {
        this->close();              // all resource automatically recycled when process end(TODO: check proper de-ininitalization)
        return;
    }

    // non top level, wave popup
    //FIXME:
    //如果提前关闭中间的popup, 应把上面的所有popup 都关闭，保持行为和 qmrofsv2 相同，ref closePopup
    if(hasWaves && !hasIView) {
        emit suspendRender();   //wait until realy suspended

        //dangling pointer !!!，第2层窗口中有2道波形，要删干净，否则 renderSlot 中会访问 closewaveview 中已delete的WaveData
        WaveformInfo *wf = tlw->wr->wfs.back();
        delete wf;
        tlw->wr->wfs.pop_back();
        wf = tlw->wr->wfs.back();
        delete wf;
        tlw->wr->wfs.pop_back();

        tlw->mrofs->closeWaveview(this->vid);   //close waveview
        tlw->mrofs->closePopup(*this);

        emit resumeRender();
    }

#ifdef IVIEW_TEST
    //non top level, iview popup
    if(hasIView && !hasWaves) {
        emit suspendRender();   //wait until realy suspended    
        IViewformInfo *ivwf = tlw->ivwr->wfs.back();
        delete ivwf;
        tlw->ivwr->wfs.pop_back();
        
        tlw->mrofs->closeWaveview(this->vid);    //close iview
        tlw->mrofs->closePopup(*this);
        emit resumeRender();        
    }
#endif	// IVIEW_TEST

    if(!hasIView && !hasWaves) {
        tlw->mrofs->closePopup(*this);    
    }
    
    this->close();
}

//open sequence: realtime_root_popup -> iview_popup -> wave_popup -> empty_popup
void Dialog::switchClicked()
{ 
#if 1
    static int i = 2;
    if(i == 0) {
        Dialog *popup= new Dialog(true, false, 200, tlw);      //has waves
        popup->show();
        i = 1;
    }else if (i == 1) {
        Dialog *popup= new Dialog(false, false, 300, tlw);     //no waves(empty popup)
        popup->show();
        i = 3;                  //open empty popup in another position
    } else if (i == 2) {
#ifdef IVIEW_TEST
        Dialog *popup = NULL;
        if(s_hdmi_cfg == HDMI_CONFIG_1680x1050) {
            popup= new Dialog(false, true, 0, tlw);      //has iview
        } else {
            popup= new Dialog(false, true, 100, tlw);      //has iview
        }
        popup->show();
#endif
        i = 0;
    } else if (i == 3) {
        Dialog *popup= new Dialog(false, false, 400, tlw);     //no waves(empty popup)
        popup->show();      
    }
#else
    QApplication *app =  static_cast<QApplication*>(QApplication::instance());
#if 0
    waveThread.bExit = true;
    QThread::msleep(1000);
    app->exit(0);
    return;
#endif
    QRect geo = this->geometry();
    qWarning("Dialog size: %d, %d, %d, %d", geo.x(), geo.y(), geo.width(), geo.height());
    static int k;
    k ++;
    if (k > 500)
        k = 0;
    this->setGeometry(50, 50, 500 + k, 500 + k);
    return;

    QScreen *scrn = app->primaryScreen();

    static int i = 1;
    switch( i ++ % 4) {
    case 0: 
    scrn->setOrientation(Qt::PortraitOrientation);
    qDebug(">>> [setOrientation] Qt::PortraitOrientation");
    break;
    case 1:
    scrn->setOrientation(Qt::InvertedPortraitOrientation);
    qDebug(">>> [setOrientation] Qt::InvertedPortraitOrientation");
    break;
    case 2:
    scrn->setOrientation(Qt::LandscapeOrientation);
    qDebug(">>> [setOrientation] Qt::LandscapeOrientation");
    break;
    case 3:
    scrn->setOrientation(Qt::InvertedLandscapeOrientation);
    qDebug(">>> [setOrientation] Qt::InvertedLandscapeOrientation");
    break;
    default:
    break;
    }
//    this->update();

#endif
};

void Dialog::toggleMRDPClicked()
{
    if(tlw->mrofs->MRDPServiceRunning()) {
       tlw->mrofs->stopMRDPService(); 
       buttonToggleMRDP->setText("start MRDP");
    } else {
       tlw->mrofs->startMRDPService();
       buttonToggleMRDP->setText("stop MRDP");        
    }
}

//! [Dialog constructor part5]

//! [desktopResized() function]
void Dialog::desktopResized(int screen)
{
    return;


    if (screen != 0)
        return;
    reactToSIP();
}
//! [desktopResized() function]

//! [reactToSIP() function]
void Dialog::reactToSIP()
{
    return;

    QRect availableGeometry = QApplication::desktop()->availableGeometry(0);
    if (windowState() | Qt::WindowMaximized)
        qDebug("!!!!!DESKTOP/SCREEN resized, Dialog show Maximized");
    else
        qDebug("!!!!!DESKTOP/SCREEN resized, Dialog show Normal");
    return;
#if 1
    qDebug("Dialog::reactToSIP, desktopGeo(%d,%d,%d,%d), availableGeo(%d,%d,%d,%d)", 
        desktopGeometry.x(), desktopGeometry.y(), desktopGeometry.width(), desktopGeometry.height(),
        availableGeometry.top(),availableGeometry.left(),availableGeometry.width(),availableGeometry.height());
#endif

    if (desktopGeometry != availableGeometry) {
        if (windowState() | Qt::WindowMaximized)
            setWindowState(windowState() & ~Qt::WindowMaximized);

        setGeometry(availableGeometry);
    }

    desktopGeometry = availableGeometry;
}

#ifdef IVIEW_TEST
////////////////////////////////IVIEW ////////////////////////////////////////////////

const int IViewRender::BUF_NUM = 4;

IViewRender::IViewRender(bool run_immediately, QObject *parent) :
    QObject(parent),
    m_fd(-1), m_notify(NULL), m_isp_buf(NULL), m_isp_buf_num(0), 
    m_running(run_immediately)
{    
    int ret = 0;
    isp_buf_info *pbufs;

    /* 1. open device */
    m_fd = isp_open((char *)DEF_DEVNAME);
    if (-1 == m_fd) {
        qWarning("Open %s error", DEF_DEVNAME);
        return;
    }

    /* 2. setup parameters into ISP driver */
    ret = isp_setup(m_fd, s_hdmi_cfg);        
    if (ret < 0) {
        isp_close(m_fd);
        return;
    }

    /* 3. initiat memory for ISP driver */
    pbufs = isp_alloc_video_buffers(m_fd, BUF_NUM);
    if (pbufs == NULL) {
        qWarning("setup memory failed\n");
        isp_close(m_fd);
        return;
    }
    this->m_isp_buf = pbufs;
    this->m_isp_buf_num = BUF_NUM;

    //QSocketNotifier do NOT support timeout, if we want it, start a 1-shot timer before created, then repeats to reset it everytime renderSlot returning(before)
    m_notify = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notify, SIGNAL(activated(int)), this, SLOT(renderSlot())); 
    isp_video_on(this->m_fd);
}

IViewRender::~IViewRender()
{
    isp_video_off(this->m_fd);
    isp_dealloc_video_buffers(this->m_isp_buf, BUF_NUM);
    isp_close(m_fd);
}

/*!
    ISP DATA READY
*/
void IViewRender::renderSlot()
{
    if (!m_running)
        return;

    if(wfs.size() != 1) {
        //qWarning("IViewRender::renderSlot: ONLY 1 iviewform supported");
        return;
    }

    //deque buffer from ISP
    int ret;
    isp_buf_info *pinfo;
    pinfo = isp_dqueue_video_buffer(this->m_fd, this->m_isp_buf, this->m_isp_buf_num, &ret);
    if (ret == 1)
        return;
    else if (ret == -1) {
        qWarning("VIDIOC_DQBUF error\n");
        return;
    }
    
//    GET_TIME(logfp, "3. before convert\n");
    // mem_width is pixel width, mem_height is pixel height, depth is BPP actually
//    convert_0bgr2xbgr(pinfo->start, pinfo->start, pinfo->mem_width, pinfo->mem_height, pinfo->depth);
//    GET_TIME(logfp, "4. after convert\n");

    IViewformInfo *ivif = this->wfs.back();
    QRegion rgn(ivif->rect);
//    qWarning("IViewRender::renderSlot: %p", &rgn);    
    tlw->mrofs->flipIView(ivif->vid, ivif->wid, rgn, pinfo->start);  //stack variable reference, must be SYNC!!!

    //queue buffer back to ISP
    ret = isp_queue_video_buffer(this->m_fd, pinfo);
    if (ret < 0) {
        qWarning("VIDIOC_QBUF error\n");
        return;
    }    

//statistcs ++
#if 0
    static int pkg_cnt;
    pkg_cnt ++;
    fprintf(stdout, " DEBUG- ***** Get %08d packages(%p), bytesuesed(%d), depth(%d), width(%d) *****", 
        pkg_cnt, pinfo->start, pinfo->vbuf.bytesused, pinfo->depth, pinfo->mem_width);
    fflush(stdout);     
    fprintf(stdout, "\r");      
    fflush(stdout);
#endif    
//statistics ++
}

void IViewRender::suspend()
{
    qWarning("suspend iview render");
    m_running = false;    
}

void IViewRender::resume()
{
    qWarning("resume iview render");
    m_running = true;
}

void IViewThread::run()
{
    exec();
}

#endif	//IVIEW_TEST

//! [reactToSIP() function]
