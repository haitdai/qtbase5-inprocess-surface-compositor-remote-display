#ifndef MRDPVIEWER_H
#define MRDPVIEWER_H

#include <QLineEdit>
#include <QButtonGroup>								// qt's combobox cause multiple backingstore will was not supported by mrofs, use radio buttons instead
#include <QRadioButton>
#include <QScrollArea>
#include <QDockWidget>
#include <QWidget>
#include <QMainWindow>
#include <QTimer>
#include <QtCore/qpropertyanimation.h>              // QPropertyAnimation
#include <QtWidgets/qabstractmrofs.h>
#include <QtPlatformSupport/private/mrdp_p.h>         // MRDPServerProxy

class QCheckBox;
class QLabel;
class QSlider;
class QToolButton;
class QGridLayout;
class QHBoxLayout;
class QVBoxLayout;

enum DISPLAY_MODE {
    DM_FULLSCREEN = 0,      // show remote screen in local full screen
    DM_NORMAL,              // show remote screen in local nromal window
};

// fit mode when display in normal mode
enum FIT_MODE {
    FM_WINDOW = 0,              // scale remote screen to fit local window
    FM_CONTENT,            // scroll remote screen in local window (pixel2pixel)
};

class ControlBar : public QWidget {
    Q_OBJECT

public:
    ControlBar(QWidget *parent = NULL);
    ~ControlBar();

public:                                                                         // notification from subject
    void animIn();
    void animOut();
    bool isIn();                                                              // get state
    DISPLAY_MODE displayMode();                            // get state
    FIT_MODE fitMode();                                               // get state
    FIT_MODE updateFitMode(FIT_MODE newMode);  // notification
    bool updateConnectionState(bool cnted);         // notification [controlbar as observer]
    
signals:                                                                          // internal 
    void connectRequested(const QString &ipaddr, bool immediate, int hz);
    void disconnectRequested();
    void refreshModeChanged(bool immediate, int hz);        // hz: -1 if immediate mode
    void hzChanged(int hz);
    void displayModeChanged(DISPLAY_MODE dm);
    void fitModeChanged(FIT_MODE fm);

protected slots:                                                            // internal 
    void setConnectionState();
    void setRefreshMode(int checkSt);
    void setHz(int hz);
    void setDisplayMode(int checked);
    void setFitMode(int checked);

public:
	void setDisplayMode(DISPLAY_MODE dm);
	void setFitMode(FIT_MODE fm);

private:
    void initUI();

private:
	QHBoxLayout			*layout;
    QLabel              *ipaddrLabel;
	QLineEdit           *ipaddrEdit;        // mrdp server ip(port is unconfigurable)
    QToolButton         *connButton;        // connect/disconnect mrdp server
	QCheckBox			*displayModeFSCheckBox;	// show full screen, otherwise normal

    QCheckBox           *refreshModeCheckBox;       // refresh mode selection: immediate/periodicly
    QSlider             *hzSlider;          // period set: Hz
    QLabel              *hzLabel;
	QCheckBox			*fitModeWindowCheckBox;		// fit to window, otherwise content(scroll bar if necessary)
    // bar animation
    QPropertyAnimation  anim;    
    bool                in;

    // data
    bool                connected;
    QString             ipaddr;            
    bool                immediateMode_;     // immediate or periodic
    int                 hz;                // periodic mode HZ    
    DISPLAY_MODE        displayMode_;
    FIT_MODE            fitMode_;
};

//#define CAPTURE_INPUT_EVENT_BY_EVENTFILTER 1
class MyScrollArea: public QScrollArea {
	Q_OBJECT
public:
	MyScrollArea(MRDPServerProxy* sp, QWidget * parent = 0);
	~MyScrollArea();
	void setServerProxy(MRDPServerProxy *sp) {serverProxy = sp;};

protected:
	QPoint ptLocalWindow2RemoteScreen(const QPoint &pt);
#ifdef CAPTURE_INPUT_EVENT_BY_EVENTFILTER
	bool eventFilter(QObject * watched, QEvent * event);
	void mouseDoubleClickEventR(QMouseEvent * e);
	void mouseMoveEventR(QMouseEvent * e);
	void mousePressEventR(QMouseEvent * e);
	void mouseReleaseEventR(QMouseEvent * e);
	void keyPressEventR(QKeyEvent * event);
	void keyReleaseEventR(QKeyEvent * event);
#else
	void mouseDoubleClickEvent(QMouseEvent * e);
	void mouseMoveEvent(QMouseEvent * e);
	void mousePressEvent(QMouseEvent * e);
	void mouseReleaseEvent(QMouseEvent * e);
	void keyPressEvent(QKeyEvent * event);
	void keyReleaseEvent(QKeyEvent * event);
#endif

private:
	MRDPServerProxy	*serverProxy;
};

class MRDPViewer : public QWidget
{
    Q_OBJECT

public:
    MRDPViewer(QWidget *parent = 0);
    virtual ~MRDPViewer();
	QSize remoteScreenSize();

protected:
	void keyPressEvent(QKeyEvent * event);

private slots:      // ui notification
    void connectToServer(const QString &ipaddr, bool immediate, int hz);
    void disconnectFromServer();
    void switchRefreshMode(bool immediate, int hz);
    void adjustHz(int hz);
	void handleSockError(QAbstractSocket::SocketError);
	void handleParseError();

protected slots:    // serverProxy notification
    void verifyNegotiation(QImage::Format screenFormat, const QRect &screenRect);
    void resizeScreenImage(QSize sz);
    void updateRemoteScreenArea(const QRegion& dirty);
    void reset();
    void toggleDisplayMode(DISPLAY_MODE dm);
    void toggleFitMode(FIT_MODE fm);
	void serverConnected();
	void serverDisconnected();

protected slots:    // timer notification
    void queryScreenUpdate();     // for dirty info

private:
    ControlBar          *controlBar;			 // topmost-floating control bar
	QVBoxLayout         *layout;
    QLabel              *remoteScreenLabel;      // display content
    MyScrollArea        *scrollArea;        
    QImage              remoteScreenImage;
    QPixmap             remoteScreenPixmap;
    QImage::Format      remoteScreenFormat;

    MRDPServerProxy     *serverProxy;
    QTimer               refreshTimer;
    uint                 refreshHz;
	QTcpSocket          sock;

    QAbstractMrOfs      *mrofsLocal;            // the same qt release of cms2
};

#endif
