#include <QtWidgets>

#include "mrdpviewer.h"

/*!
	width : height == 1.6 : 1
*/
static const int REMOTE_SCRREN_WIDTH_DEFAULT  = 512;
static const int REMOTE_SCRREN_HEIGHT_DEFAULT = 320;
/*!
	control bar
*/
static const int CONTROLBAR_MINIMUM_HEIGHT = 32;
static const int CONTROLBAR_MINIMUM_WIDTH = 600;

ControlBar::ControlBar(QWidget *parent) : anim(this, "pos"), QWidget(parent),
    connected(false), ipaddr(tr("77.77.1.101")), hz(-1)
{
	setMinimumSize(CONTROLBAR_MINIMUM_WIDTH, CONTROLBAR_MINIMUM_HEIGHT);
    initUI();
}

void ControlBar::initUI()
{
	layout = new QHBoxLayout(this);

	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

    ipaddrLabel = new QLabel(tr("MK-IP: "), this);
	layout->addWidget(ipaddrLabel);
    ipaddrEdit = new QLineEdit("77.77.1.101", this);
	ipaddrEdit->setInputMask("000.000.000.000;_");
	layout->addWidget(ipaddrEdit);
    connButton = new QToolButton(this);
    connButton->setText(tr("connect"));
	layout->addWidget(connButton);

	displayModeFSCheckBox = new QCheckBox(tr("full screen"), this);
	layout->addWidget(displayModeFSCheckBox);

	fitModeWindowCheckBox = new QCheckBox(tr("fit to window"), this);
	layout->addWidget(fitModeWindowCheckBox);
	fitModeWindowCheckBox->setChecked(true);

    refreshModeCheckBox = new QCheckBox(tr("immediate refresh"), this);       // unchecked means periodic refresh
    refreshModeCheckBox->setChecked(true);
	refreshModeCheckBox->setEnabled(false);
	layout->addWidget(refreshModeCheckBox);
    hzSlider = new QSlider(Qt::Horizontal, this);
    hzSlider->setTickPosition(QSlider::TicksBelow);
    hzSlider->setMinimum(5);        // min: 5hz
    hzSlider->setMaximum(25);       // max: 25hz
	hzSlider->setEnabled(false);
	layout->addWidget(hzSlider);
    hzLabel = new QLabel(this);
	layout->addWidget(hzLabel);

	// stretch factor:  2:6:2:3:3:2:2
	layout->setStretch(0, 2);
	layout->setStretch(1, 6);
	layout->setStretch(2, 2);
	layout->setStretch(3, 3);
	layout->setStretch(4, 3);
	layout->setStretch(5, 2);
	layout->setStretch(6, 2);

    setLayout(layout);

	// semi-transparent bkg
	setAutoFillBackground(true);
	QPalette pal;
	pal.setColor(QPalette::Background, QColor(0x0,0x0,0x0,0x88));
	setPalette(pal);

    anim.setDuration(500);      // ms
    
    in = false;

    // no scale(scrolling), normal window
    immediateMode_ = true;
    displayMode_ = DM_NORMAL;
    fitMode_ = FM_WINDOW;

    connect(connButton, SIGNAL(clicked()), this, SLOT(setConnectionState()));
    connect(refreshModeCheckBox, SIGNAL(stateChanged(int)), this, SLOT(setRefreshMode(int)));    
    connect(hzSlider, SIGNAL(valueChanged(int)), this, SLOT(setHz(int)));    
	connect(displayModeFSCheckBox, SIGNAL(stateChanged(int)), this, SLOT(setDisplayMode(int)));
	connect(fitModeWindowCheckBox, SIGNAL(stateChanged(int)), this, SLOT(setFitMode(int)));
}

ControlBar::~ControlBar()
{

}

void ControlBar::setConnectionState()
{
	ipaddr = ipaddrEdit->text();
    if(connected) {
        emit disconnectRequested();
    } else {
        emit connectRequested(ipaddr, immediateMode_, hz);
    }

    // connected state and view display will be updated in notification, ref. updateConnectionState
}

void ControlBar::setRefreshMode(int checkSt)
{
    immediateMode_ = (Qt::Checked == checkSt);
    if(immediateMode_) {
        hzSlider->setEnabled(false);
    } else {
        hzSlider->setEnabled(true);
    }

    emit refreshModeChanged(immediateMode_, hz);
}

void ControlBar::setHz(int hz)
{
    this->hz = hz;
    emit hzChanged(hz);     // notify parent
}

void ControlBar::setDisplayMode(int checked)
{
	displayMode_ = checked ? DM_FULLSCREEN : DM_NORMAL;
    emit displayModeChanged(displayMode_);
}

void ControlBar::setDisplayMode(DISPLAY_MODE dm) {
	if(DM_FULLSCREEN == dm) {
		displayModeFSCheckBox->setChecked(true);
	} else {
		displayModeFSCheckBox->setChecked(false);
	}
}

void ControlBar::setFitMode(int checked)
{
	fitMode_ = checked ? FM_WINDOW : FM_CONTENT;
    emit fitModeChanged(fitMode_);
}

void ControlBar::setFitMode(FIT_MODE dm) {
	if(FM_WINDOW == dm) {
		fitModeWindowCheckBox->setChecked(true);
	} else {
		fitModeWindowCheckBox->setChecked(false);
	}
}

bool ControlBar::updateConnectionState(bool cnted)
{
    bool oldst = this->connected;
    if(oldst == cnted) {
        return oldst;
    }

    this->connected = cnted;
    cnted ? connButton->setText(tr("disconnect")) : connButton->setText(tr("connect"));;
    return oldst;
}

void ControlBar::animIn()
{
    if(isVisible())
        return;
	setVisible(true);
	return;

#if 0
    QPoint curPos = pos();
    QWidgetList wl = QApplication::topLevelWidgets();
    QPoint endPos = QPoint(curPos.x(), wl.at(0)->geometry().height());
    anim.setStartValue(QPoint(curPos.x(), curPos.y()));
    anim.setEndValue(QPoint(endPos.x(), endPos.y()));
    anim.start();
#endif
}

void ControlBar::animOut()
{
    if(!isVisible()) 
        return;
	setVisible(false);
	return;

#if 0
    QPoint curPos = pos();
    QPoint endPos = QPoint(curPos.x(), geometry().height());
    anim.setStartValue(QPoint(curPos.x(), curPos.y()));
    anim.setEndValue(QPoint(endPos.x(), endPos.y()));
    anim.start();
#endif
}

DISPLAY_MODE ControlBar::displayMode()
{
    return displayMode_;
}

FIT_MODE ControlBar::fitMode()
{
    return fitMode_;
}

FIT_MODE ControlBar::updateFitMode(FIT_MODE newMode)
{
    FIT_MODE oldMode = fitMode_;

    if(oldMode == newMode)
        return oldMode;

    // update ui and internal data(by signal currentIndexChanged)
	fitModeWindowCheckBox->setChecked(FM_WINDOW == newMode);

    return oldMode;
}

MyScrollArea::MyScrollArea(MRDPServerProxy* sp, QWidget * parent) : QScrollArea(parent), serverProxy(sp) {
	setMouseTracking(true);
	setCursor(Qt::BlankCursor);
	setViewportMargins(0, 0, 0, 0);
}

MyScrollArea::~MyScrollArea() 
{

}

QPoint MyScrollArea::ptLocalWindow2RemoteScreen(const QPoint &pt)
{
	MRDPViewer *parent = static_cast<MRDPViewer*>(parentWidget());
	int x = pt.x() * parent->remoteScreenSize().width() * 1.0 / width();
	int y = pt.y() * parent->remoteScreenSize().height() * 1.0 / height();
	QPoint ptInRS(x, y);
	return ptInRS;
}

#ifdef CAPTURE_INPUT_EVENT_BY_EVENTFILTER
bool MyScrollArea::eventFilter(QObject * watched, QEvent * event)
{
	if(watched != widget()) {
		return QScrollArea::eventFilter(watched, event);
	}
	switch(event->type()) {
	case QEvent::MouseButtonPress:
		mousePressEventR(static_cast<QMouseEvent*>(event));
		break;
	case QEvent::MouseButtonRelease:
		mouseReleaseEventR(static_cast<QMouseEvent*>(event));
		break;
	case QEvent::MouseMove:
		mouseMoveEventR(static_cast<QMouseEvent*>(event));
		break;
	case QEvent::KeyPress:
		keyPressEventR(static_cast<QKeyEvent*>(event));
		break;
	case QEvent::KeyRelease:
		keyReleaseEventR(static_cast<QKeyEvent*>(event));
		break;
	default:
		break;
	}
	// pass the event on to the parent class
	return QScrollArea::eventFilter(watched, event);
}

void MyScrollArea::mouseDoubleClickEventR(QMouseEvent * event)
{

}

void MyScrollArea::mouseMoveEventR(QMouseEvent * event)
{
	QPoint pt(event->x(), event->y());
	qDebug(">>>>>mouse move event(%d, %d)", pt.x(), pt.y());
	if(serverProxy) {
		serverProxy->postMotionActionReq(event->x(), event->y());
	}
}

void MyScrollArea::mousePressEventR(QMouseEvent * event)
{
	qDebug(">>>>>mouse press event");
	if(serverProxy) {
		serverProxy->postButtonActionReq(event->button(), ::mrdp::BA_PRESSED);
	}
}

void MyScrollArea::mouseReleaseEventR(QMouseEvent * event)
{
	qDebug("mouse release event");
	if(serverProxy) {
		serverProxy->postButtonActionReq(event->button(), ::mrdp::BA_RELEASED);
	}
}

void MyScrollArea::keyPressEventR(QKeyEvent * event)
{
	qDebug(">>>>>key press event");
	if(serverProxy) {
		serverProxy->postKeyActionReq(event->key(), ::mrdp::KA_DOWN);
	}
}

void MyScrollArea::keyReleaseEventR(QKeyEvent * event)
{
	qDebug(">>>>>key release event");
	if(serverProxy) {
		serverProxy->postKeyActionReq(event->key(), ::mrdp::KA_UP);
	}
}
#else
void MyScrollArea::mouseDoubleClickEvent(QMouseEvent * event)
{
	QScrollArea::mouseDoubleClickEvent(event);
}

void MyScrollArea::mouseMoveEvent(QMouseEvent * event)
{
	QPoint pt(event->x(), event->y());
	qDebug(">>>>>mouse move event(%d, %d)", pt.x(), pt.y());
	pt = ptLocalWindow2RemoteScreen(pt);
	if(serverProxy) {
		serverProxy->postMotionActionReq(pt.x(), pt.y());
	}

	QScrollArea::mouseMoveEvent(event);
}

void MyScrollArea::mousePressEvent(QMouseEvent * event)
{
	qDebug(">>>>>mouse press event");
	if(serverProxy) {
		serverProxy->postButtonActionReq(event->button(), ::mrdp::BA_PRESSED);
	}
	QScrollArea::mousePressEvent(event);
}

void MyScrollArea::mouseReleaseEvent(QMouseEvent * event)
{
	qDebug(">>>>>mouse release event");
	if(serverProxy) {
		serverProxy->postButtonActionReq(event->button(), ::mrdp::BA_RELEASED);
	}
	QScrollArea::mouseReleaseEvent(event);
}

void MyScrollArea::keyPressEvent(QKeyEvent * event)
{
	qDebug(">>>>>key press event");
	if(serverProxy) {
		serverProxy->postKeyActionReq(event->key(), ::mrdp::KA_DOWN);
	}
	QScrollArea::keyPressEvent(event);
}

void MyScrollArea::keyReleaseEvent(QKeyEvent * event)
{
	qDebug(">>>>>key release event");
	if(serverProxy) {
		serverProxy->postKeyActionReq(event->key(), ::mrdp::KA_UP);
	}
	QScrollArea::keyReleaseEvent(event);
}
#endif	// CAPTURE_INPUT_EVENT_BY_EVENTFILTER

MRDPViewer::MRDPViewer(QWidget *parent) : QWidget(parent), 
	serverProxy(NULL), mrofsLocal(NULL)
{
    // mrofs(cms2)
    //mrofsLocal = QAbstractMrOfs::createInstance(this);
    //mrofsLocal->enableComposition(true);

	setContentsMargins(0, 0, 0, 0);

    // ui layout
    layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

    // control bar
    controlBar = new ControlBar(this);
    controlBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(controlBar);

	remoteScreenLabel = new QLabel(this);
	remoteScreenLabel->setBackgroundRole(QPalette::Base);
	remoteScreenLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	remoteScreenLabel->setScaledContents(true);
	remoteScreenLabel->setMouseTracking(true);

    scrollArea = new MyScrollArea(NULL, this);
    scrollArea->setBackgroundRole(QPalette::Dark);
    scrollArea->setWidget(remoteScreenLabel);
	layout->addWidget(scrollArea);
	setLayout(layout);

    setWindowTitle(tr("MRDP Viewer - press F12 to hide control bar"));
	activateWindow();

	// initialize size
    //resize(REMOTE_SCRREN_WIDTH_DEFAULT + 2, REMOTE_SCRREN_HEIGHT_DEFAULT + controlBar->height() + 2);
	resize(REMOTE_SCRREN_WIDTH_DEFAULT, REMOTE_SCRREN_HEIGHT_DEFAULT);

    // ui event handlers
    connect(controlBar, SIGNAL(connectRequested(const QString &, bool, int)), 
            this, SLOT(connectToServer(const QString &, bool, int)));
    connect(controlBar, SIGNAL(disconnectRequested()), this, SLOT(disconnectFromServer()));
    connect(controlBar, SIGNAL(refreshModeChanged(bool,int)), this, SLOT(switchRefreshMode(bool,int)));
    connect(controlBar, SIGNAL(hzChanged(int)), this, SLOT(adjustHz(int)));
    connect(controlBar, SIGNAL(displayModeChanged(DISPLAY_MODE)), this, SLOT(toggleDisplayMode(DISPLAY_MODE)));
    connect(controlBar, SIGNAL(fitModeChanged(FIT_MODE)), this, SLOT(toggleFitMode(FIT_MODE)));

    // timer - worked under non-immediate mode
    connect(&refreshTimer, SIGNAL(timeout()), this, SLOT(queryScreenUpdate()));

	sock.setReadBufferSize(MRDP_MAX_RECV_BUFFER_SIZE);
}

MRDPViewer::~MRDPViewer()
{

}

QSize MRDPViewer::remoteScreenSize()
{
	return remoteScreenImage.size();
}

void MRDPViewer::keyPressEvent(QKeyEvent * event)
{
	if(Qt::Key_F12 == event->key()) {
		if(controlBar->isVisible()) {
			controlBar->animOut();
			setWindowTitle(tr("MRDP Viewer - press F12 to show control bar"));
		} else  {
			controlBar->animIn();
			setWindowTitle(tr("MRDP Viewer - press F12 to hide control bar"));
		}
	}

	if(Qt::Key_F11 == event->key()) {
		if(DM_FULLSCREEN == controlBar->displayMode()) {
			controlBar->setDisplayMode(DM_NORMAL);
		} else {
			controlBar->setDisplayMode(DM_FULLSCREEN);
		}
	}

	if(Qt::Key_F10 == event->key()) {
		if(FM_WINDOW == controlBar->fitMode()) {		// toggle
			controlBar->setFitMode(FM_CONTENT);
		} else {
			controlBar->setFitMode(FM_WINDOW);
		}
	}
}

void MRDPViewer::connectToServer(const QString &ipaddr, bool immediate, int hz)
{
    sock.connectToHost(ipaddr, MRDP_PORT, QIODevice::ReadWrite, QAbstractSocket::IPv4Protocol);      // read-write, any ip protocol

	connect(&sock, SIGNAL(connected()), this, SLOT(serverConnected()));
	connect(&sock, SIGNAL(disconnected()), this, SLOT(serverDisconnected()));
    connect(&sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleSockError(QAbstractSocket::SocketError)));    
}

void MRDPViewer::serverConnected()
{
    serverProxy = new MRDPServerProxy(&sock);
	qDebug("serverConnected, serverProxy(%p)", serverProxy);
	disconnect(serverProxy, SIGNAL(parseError()), this, SLOT(handleParseError()));
	connect(serverProxy, SIGNAL(parseError()), this, SLOT(handleParseError()));
	connect(serverProxy, SIGNAL(negotiationFinished(QImage::Format, const QRect&)), this, SLOT(verifyNegotiation(QImage::Format, const QRect&)));
	connect(serverProxy, SIGNAL(screenResized(const QSize&)), this, SLOT(resizeScreenImage(QSize)));
	connect(serverProxy, SIGNAL(screenUpdated(const QRegion&)), this, SLOT(updateRemoteScreenArea(const QRegion&)), Qt::QueuedConnection);
	connect(serverProxy, SIGNAL(closeConfirmed()), this, SLOT(reset()));
	connect(serverProxy, SIGNAL(serverShutdown()), this, SLOT(reset()));    

	controlBar->updateConnectionState(true);

	scrollArea->setServerProxy(serverProxy);
}

// disconnect explicitly, or error occurred
void MRDPViewer::disconnectFromServer()
{
    sock.disconnectFromHost();
}

void MRDPViewer::serverDisconnected()
{
	disconnect(&sock, SIGNAL(connected()), this, SLOT(serverConnected()));
	disconnect(&sock, SIGNAL(disconnected()), this, SLOT(serverDisconnected()));
	disconnect(&sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleSockError(QAbstractSocket::SocketError)));   

	disconnect(serverProxy, SIGNAL(parseError()), this, SLOT(handleParseError()));
	disconnect(serverProxy, SIGNAL(negotiationFinished(QImage::Format, const QRect&)), this, SLOT(verifyNegotiation(QImage::Format, const QRect&)));
	disconnect(serverProxy, SIGNAL(screenResized(const QSize&)), this, SLOT(resizeScreenImage(QSize)));
	disconnect(serverProxy, SIGNAL(screenUpdated(const QRegion&)), this, SLOT(updateRemoteScreenArea(const QRegion&)));
	disconnect(serverProxy, SIGNAL(closeConfirmed()), this, SLOT(reset()));
	disconnect(serverProxy, SIGNAL(serverShutdown()), this, SLOT(reset()));   

    delete serverProxy;
	qDebug("serverDisconnected, serverProxy(%p)", serverProxy);
    serverProxy = NULL;
	scrollArea->setServerProxy(NULL);
    controlBar->updateConnectionState(false);

    // clear screen - QImage::scanline() will detach bits, do not use it!!!
    int bpl = remoteScreenImage.bytesPerLine();
    int height = remoteScreenImage.height();
    uchar *fillPos = const_cast<uchar*>(remoteScreenImage.constBits());
	int step_in_row = bpl / sizeof(uint);
	uint clearColor = 0xFF000000;			// argb
    for(int i = 0; i < height; i++) {
		for(int j = 0; j < step_in_row; j ++) {
			memcpy(fillPos, &clearColor, sizeof(uint));
			fillPos += sizeof(uint);
		}
    }

    updateRemoteScreenArea(rect());
}

// DEBUG ME!!!
void MRDPViewer::switchRefreshMode(bool immediate, int hz)
{
    if(immediate) {                 // server trigger refreshing(immediately)
        refreshTimer.stop();
        serverProxy->postScreenUpdateReportModeReq(::mrdp::SURM_ACTIVE);
    } else {                        // client trigger refreshing
        refreshTimer.start(1000 / hz);
        serverProxy->postScreenUpdateReportModeReq(::mrdp::SURM_PASSIVE);        
    }
}

void MRDPViewer::adjustHz(int hz)
{
    // adjust timer period
    refreshHz = hz;
}

void MRDPViewer::handleSockError(QAbstractSocket::SocketError err)
{
	if(serverProxy)
		serverProxy->handleSockError(err);
    disconnectFromServer();
    controlBar->updateConnectionState(false);
}

void MRDPViewer::handleParseError()
{
	serverProxy->handleParseError();
	disconnectFromServer();
	controlBar->updateConnectionState(false);
}

void MRDPViewer::verifyNegotiation(QImage::Format screenFormat, const QRect &screenRect)
{
    remoteScreenFormat = screenFormat;
    resizeScreenImage(QSize(screenRect.width(), screenRect.height()));

    // reset anyway, in case of re-connect
    serverProxy->setScreenImage(remoteScreenImage);     // set to serverProxy
}

// always pixel2pixel
void MRDPViewer::resizeScreenImage(QSize sz)
{
    if(sz == remoteScreenImage.size()) 
        return;

    remoteScreenImage = QImage(sz.width(), sz.height(), remoteScreenFormat);
    remoteScreenPixmap = QPixmap::fromImage(remoteScreenImage);     // TODO: check the flags
    remoteScreenLabel->setPixmap(remoteScreenPixmap);   // set to ui

	//if fit-to-window is set, scroll area will resize label to avoid scrollbar
	scrollArea->setWidgetResizable(FM_WINDOW == controlBar->fitMode());
    if (!(controlBar->fitMode() == FM_WINDOW)) {
        remoteScreenLabel->adjustSize();
    }
}

// pixmap is the only 1 buffer (which internal image is shared between serverProxy, me, and QLabel)
void MRDPViewer::updateRemoteScreenArea(const QRegion& dirty)
{
    // remoteScreenImage has been already updated in serverProxy
    
    // tell QLabel the shared pixmap was dirty
	remoteScreenLabel->markPixmapDirty();

    // notify ui which fullfill the real scaling
    remoteScreenLabel->update();            // dirty
}

void MRDPViewer::reset()
{
    // TODO
}

void MRDPViewer::toggleDisplayMode(DISPLAY_MODE dm)
{
    if(dm == DM_FULLSCREEN && !isFullScreen()) {        
        showFullScreen();
        // always fit content if full screen        
        controlBar->updateFitMode(FM_WINDOW);
    } else if (dm == DM_NORMAL && isFullScreen()) {
        showNormal();
    }
}

void MRDPViewer::toggleFitMode(FIT_MODE fm)
{
    bool fitToWindow = (FM_WINDOW == fm);
    //if fit-to-window is set, scroll area will resize label to avoid scrollbar
    scrollArea->setWidgetResizable(fitToWindow);
    if (!fitToWindow) {
		// resize label to fit remote screen image
		remoteScreenLabel->adjustSize();
    }
}

// 主动查询(增量)
void MRDPViewer::queryScreenUpdate()
{
    serverProxy->postScreenUpdateReq(mrdp::SUM_INCREMENTAL);
}
