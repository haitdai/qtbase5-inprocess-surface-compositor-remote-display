#ifndef DIALOG_H
#define DIALOG_H
#include <QtGui>
#include <QDialog>
#include <QApplication>
#include <QtWidgets>      //mrofsv4
#include <QThread>
#include <QObject>
#include <QMutex>
#include <QScrollArea>
#include <QSocketNotifier>  //iview data read
#include <QtWidgets/QAbstractMrOfs>    // QAbstractMrOfs

//#define IVIEW_TEST	1
#define INCH_12
//#define INCH_15
//#define INCH_17

#ifndef INCH_12
#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#else
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 800
#endif

class Dialog;

struct WaveformInfo {
    QAbstractMrOfs::WV_ID     vid;
    QAbstractMrOfs::WV_ID     wid;
    Qt::GlobalColor   color; 
    Qt::GlobalColor   color2; 
    int               wi;
    int               k;
    QWaveData*        data;
};

class WaveRender : public QObject
{
    Q_OBJECT
signals:
	void updateWaves(const QRect& rc);
protected slots:
    void renderSlot();
    void suspend();
    void resume();
public:
    WaveRender(bool run, QObject *parent = 0) : QObject(parent), running(run) {}
	static void render_0( QPaintDevice* dev, QAbstractMrOfs::WV_ID wid);
	void renderX(WaveformInfo& wf);
	static void render2( QPaintDevice* dev);
	static  void render3(int loopCount);
    QList<WaveformInfo*> wfs;
    bool running;
};


class WaveThread : public QThread
{
    Q_OBJECT
    void run();
public:
    WaveThread(WaveRender *wr, QObject *parent = 0) : QThread(parent) ,wr(wr) {}
    bool bExit;
    QTimer      *wt;
    WaveRender  *wr;
};

#ifdef IVIEW_TEST
struct IViewformInfo {
    QAbstractMrOfs::WV_ID     vid;
    QAbstractMrOfs::WV_ID     wid;
    QRect             rect; //iviewform coordinate
    QWaveData*        data;
};

class IViewRender : public QObject
{
    Q_OBJECT

protected slots:
    void renderSlot();

public :
    IViewRender(bool run_immediately, QObject *parent = 0);
    ~IViewRender();
    void suspend();
    void resume();
    QList<IViewformInfo*> wfs;    
    static const int     BUF_NUM;    

private:
    int                  m_fd;
    QSocketNotifier     *m_notify;
    isp_buf_info        *m_isp_buf;    //isb_buf_info array
    int                  m_isp_buf_num;
    QAbstractMrOfs::WV_ID      m_vid;
    QAbstractMrOfs::WV_ID      m_wid;
    QRect                m_ivRect;      //ivewform coordinate
    bool                 m_running;
};

class IViewThread : public QThread
{
    Q_OBJECT
    void run();
public:
    IViewThread(IViewRender *wr, QObject *parent = 0) : QThread(parent) ,wr(wr) {
//        qRegisterMetaType<isp_buf_info>("isp_buf_info");
    }

    bool         bExit;
    IViewRender  *wr;    
};
#endif	// IVIEW_TEST

#if 1
class RealtimeDisplay : public QWidget
{
public:
    RealtimeDisplay(QWidget* parent);
    ~RealtimeDisplay();
    void moveEvent(QMoveEvent *ev);
    void paintEvent(QPaintEvent *ev);    
};

class RealtimeView : public QWidget
{
public:
    RealtimeView(QWidget* wgt) : QWidget(wgt){};
    void paintEvent(QPaintEvent *ev);    
};

class WaveWidget : public QWidget
{
Q_OBJECT
public slots:
	void updateWaves(const QRect& rc);
public:
    WaveWidget(QWidget* parent);
    ~WaveWidget(); 
    void paintEvent(QPaintEvent *ev);
};
#else
typedef QWidget RealtimeDisplay;
typedef QWidget RealtimeView;
#endif

class ScrollAreaWidget : public QScrollArea
{
public:
    ScrollAreaWidget(QWidget *parent=0);
    void scrollContentsBy(int xpos, int ypos);
    void paintEvent(QPaintEvent *ev);    
};
class QPopup : public QWidget
{
public:
    QPopup(QWidget *parent);
    ~QPopup();
};

//! [Dialog header]
class Dialog : public QWidget   //QDialog
{
    Q_OBJECT

public:
    Dialog(bool hasWaves, bool hasIView, int offset, QWidget *parent);
    void reactToSIP();
    void paintEvent(QPaintEvent *ev);
private:
    QRect desktopGeometry;

    void testQRectPar(QRect rc);
public slots:
    void desktopResized(int screen);
    void handleScreenOrientationChanged(Qt::ScreenOrientation orient);
    void switchClicked();
    void closeClicked();
    void toggleMRDPClicked();
    void blockUI();
    void hScrollValueChanged(int val);
    void vScrollValueChanged(int val);
    void hScrollRangeChanged(int min, int max);
    void vScrollRangeChanged(int min, int max);
signals:
    void waveDataChanged();
    void suspendRender();
    void resumeRender();
private:
    QPushButton      *button;       //bottom side
    QPushButton      *buttonClose;
#ifdef IVIEW_TEST
    QPushButton      *buttonIV;
    QPushButton      *buttonIVClose;
#endif
    QPushButton      *buttonToggleMRDP;
    
    //dialog -|-> view->scrollArea->display->gridlayout->waveform(tile)
    //        |-> statusbar
    //        |-> menubar
    RealtimeView *view;
#if 1
    ScrollAreaWidget  *scrollArea;
#else
    QScrollArea  *scrollArea;
#endif
    RealtimeDisplay  *display;
    QGridLayout  *gridLayout;
    WaveWidget       *wavewidget;  //1 view 包含 1 wavewidget 包含 N Waveform(wavedata)
    QMutex        wavedata0Lock;
    QWaveData*    wavedata0;    //2道波形
    QWaveData*    wavedata1;
    QLabel       *statusbar;    //top side
    QAbstractMrOfs       *mrofs;
    QAbstractMrOfs::WV_ID  wid0;        //2道波形
    QAbstractMrOfs::WV_ID  wid1;
    QAbstractMrOfs::WV_ID  vid;        //waveview id OR ivew id
    QTimer      *timer;         //trigger wave updating

    QImage       wavefile0;
    QImage       wavefile1;
    QImage       bufferfly;
    
    QScrollBar  *hsbar;
    int         hpos;
    int         hmin;
    int         hmax;
    QScrollBar  *vsbar;
    int         vpos;
    int         vmin;
    int         vmax;

    WaveThread  *wth;
    WaveRender  *wr;
#ifdef IVIEW_TEST
    IViewThread  *ivth;
    IViewRender *ivwr;
#endif
    QMutex      orientingLock;
    bool        orienting;
    bool        hasWaves;
    bool        hasIView;
friend class WaveWidget;
friend class WaveRender;
friend class WaveThread;
friend class RealtimeDisplay;
#ifdef IVIEW_TEST
friend class IViewRender;
friend class IViewThread;
#endif
friend class ScrollAreaWidget;
};
//! [Dialog header]

#endif
