#ifndef MRDP_P_H
#define MRDP_P_H

#include "mrdp.pb.h"            // protobuf headers, all class lies in namespace ::mrdp
#include <QtNetwork/qtcpserver.h>
#include <QtNetwork/qtcpsocket.h>
#include <QtGui/qregion.h>
#include <QtCore/qsharedpointer.h>
#include <QtWidgets/qplatformcompositor.h>
#include <QtCore/qatomic.h>
#include <QtCore/qthread.h>
#include <string>
#include "minilzo.h"

#define MRDP_PORT       2016
#define MRDP_MAX_SEND_BUFFER_SIZE   (2 * 1024 * 1024)
#define MRDP_MAX_RECV_BUFFER_SIZE  (2 * 1024 * 1024)
class MRDPConnMgr;
class MRDPTransceiver;
class MessageProcessorBase;

// package := header + payload
// header := length + crc16
// payload := protobuf_message(Message)
// protobuf_message := Message{msg_id + [req | resp | evt]}
// req := {client_id + [nego | close | ma | ka]}
// resp := {client_id + error_no + [nego | close]}
// evt := {client_id + [nego | su | sr | ss]}

typedef struct ExtraHeader_ {
    uint length;    // header + payload
    uint crc16;     // header - crc16
} ExtraHeader;
#define EXTRA_HEADER_SIZE   8               // no use sizeof
#define EXTRA_HEADER_ENDIAN Q_LITTLE_ENDIAN // always little endian

/*!
    session state for specified client
*/
enum SessionState {
    SS_NEGOTIATE_0 = 0x1,     // negotiation event posted, but negotiation request not received
    SS_NEGOTIATE_1 = 0x2,     // negotiation request received, but response not been posted yet
    SS_ESTABLISHED = 0x4,     // negotiation response already been posted
    SS_CLOSING = 0x8,         // close request received from client, but response not been posted yet
    SS_CLOSED = 0x10,          // close request received from client, response been posted, [OR] server post shutdown directly to client(no more acknowledgement from client is needed)
    SS_ERROR = 0x20,           // error state, at now suspend
};

enum ParseState {
    PS_EXPECT_HEADER  = 0x1,
    PS_EXPECT_PAYLOAD = 0x2,
    PS_ERROR          = 0x4,
};

/*!
    Message Parser: parse the message, then deliver them to Message Processor
*/
class MessageParser {
public: 
    MessageParser() ;
    virtual ~MessageParser();
    void enlist(MessageProcessorBase *processor);
    void exec(const uchar *buf, size_t size, MRDPTransceiver& peer);
    ParseState getState();
    void setState(ParseState ps);
    
private:
    ParseState                                          ps_;  // parsing state        
    ::mrdp::Message                                     rootMsg;
    QMap< ::mrdp::MSG_ID, MessageProcessorBase*>        processors;
};

/*!
    Message Processor: process the messages
*/
class MessageProcessorBase {
public:
    MessageProcessorBase() {};
    virtual ~MessageProcessorBase() {};
    virtual void process(const ::mrdp::Message& msg, MRDPTransceiver& peer) = 0;
    virtual ::mrdp::MSG_ID getID() = 0;
};

template < ::mrdp::MSG_ID id>
class MessageProcessor : public MessageProcessorBase {
public:
    
    MessageProcessor(MessageParser* parser) {
        id_ = id;
        parser->enlist(this);
    };
    
    ~MessageProcessor() {};
    void process(const ::mrdp::Message& msg, MRDPTransceiver& peer);
    
    ::mrdp::MSG_ID getID(){
        return id_;
    };

private:
    ::mrdp::MSG_ID             id_;
};

// lzo work buffer
#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]
    
class MRDPTransceiver : public QObject {
    Q_OBJECT

public:
    MRDPTransceiver(QTcpSocket *ioDev);
    virtual ~MRDPTransceiver();

public:
    QTcpSocket *socket();
    virtual uint clientID() = 0;
    virtual uint screenDepth() = 0;
    
public slots:
    virtual void parseMessage();
    virtual void handleSockError(QAbstractSocket::SocketError err);
    virtual void handleParseError();
    
protected:
    inline void buildExtraHeader(ExtraHeader &hdr, size_t payloadSize);
    inline ::mrdp::Request* buildReqHeader(::mrdp::Message& msg, uint clientID);
    inline ::mrdp::Event* buildEvtHeader(::mrdp::Message& msg, uint clientID);
    inline ::mrdp::Response* buildRespHeader(::mrdp::Message& msg, uint clientID, ::mrdp::ErrorNo error = ::mrdp::ERR_SUCCESS);
    void sendExtraHeader(const ExtraHeader &hdr);
    uint pack(uint dstPos, const QRect &dirty, const QImage &srcImg);
    void sendMsgBody(::mrdp::Message *msg);
    uint unpack(uint srcPos, const QRect &dirty, QImage &dstImg);
    virtual QImage::Format remoteQFormat() { return QImage::Format_ARGB32_Premultiplied; };
    ::mrdp::ErrorNo compress();
    ::mrdp::ErrorNo decompress(const std::string& src);
    ::mrdp::Message *getMsgToSendFromPool(::mrdp::MSG_ID id);
    void cleanMsgPool();
    
public:      // parsing in client stub
    virtual void handleNegotiateReq(const ::mrdp::NegotiateReq &req) { Q_UNUSED(req); };
    virtual void handleCloseReq(const ::mrdp::CloseReq &req) { Q_UNUSED(req); };
    virtual void handleMotionActionReq(const ::mrdp::MotionActionReq &req) { Q_UNUSED(req); };
    virtual void handleButtonActionReq(const ::mrdp::ButtonActionReq &req) { Q_UNUSED(req); };
    virtual void handleKeyActionReq(const ::mrdp::KeyActionReq &req) { Q_UNUSED(req); };
    virtual void handleScreenUpdateReq(const ::mrdp::ScreenUpdateReq &req) { Q_UNUSED(req); };
    virtual void handleScreenUpdateReportModeReq(const ::mrdp::ScreenUpdateReportModeReq &req) { Q_UNUSED(req); };

public:         // parsing in server proxy
    virtual void handleNegotiateResp(const ::mrdp::NegotiateResp &resp) { Q_UNUSED(resp); };
    virtual void handleCloseResp(const ::mrdp::CloseResp &resp) { Q_UNUSED(resp); };
    virtual void handleNegotiateEvt(const ::mrdp::NegotiateEvt &evt, uint clientid) { Q_UNUSED(evt); Q_UNUSED(clientid) };
    virtual void handleScreenUpdatedEvt(const ::mrdp::ScreenUpdatedEvt &evt) { Q_UNUSED(evt); };
    virtual void handleScreenResizedEvt(const ::mrdp::ScreenResizedEvt &evt) { Q_UNUSED(evt); };
    virtual void handleServerShutdownEvt(const ::mrdp::ServerShutdownEvt &evt) { Q_UNUSED(evt); };

signals:
    void parseError();
    void nextMessage();             // continue to proceed next (incoming) message
    
protected:
    QTcpSocket                 *sock;                                 // io device    
    MessageParser               reqParser;                         // the parser

    // sending: screenImage(in MRDPConnMgr) -pack-> packedBuf -compress-> compressedBuf -> socket
    // sender {{{
    uint                        packedBufOffset;                        // [server use ONLY]
    size_t                      packedBufCapacity;                      // [server use ONLY]
    QScopedArrayPointer<uchar>  packedBuf;  // lzo input buffer     [server use ONLY]
    uint                        compressedBufOffset;                    // [server use ONLY]
    size_t                      compressedBufCapacity;                  // [server use ONLY]
    QScopedArrayPointer<uchar>  compressedBuf;                          // lzo output buffer    [server use ONLY]
    std::string                 msgSendStr;         // member var is much efficient than stack var

    // 1, avoid lots of new-delete ops
    // 2, naked-pointer is much faster than QSharedPointer/QShadedDataPointer
    QMap< ::mrdp::MSG_ID, ::mrdp::Message *> msgPool;                    // [client and server use]
    HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);                     // [client and server use]    
    // }}}

    // receving: socket -> payload -decompress-> decompressedBuf -unpack-> screenImage(in MRDPServerProxy)
    // receiver {{{
    // extra header
    ExtraHeader                extraHeader;                            // [client and server use]
    // payload buffer
    uchar                     *payload;                               // [client and server use]
    size_t                     payloadSize;
    size_t                     allocatedPayloadBytes;
    // raw message receving control
    int                        readInStart;                            // [client and server use]
    int                        readInExpect;                           // [client and server use]
    uint                       unpackedOffset;                         // unpack from decompressed buf [server use ONLY]
    uint                       decompressedBufOffset;                  // [client use ONLY]
    size_t                     decompressedBufCapacity;                // [client use ONLY]
    QScopedArrayPointer<uchar> decompressedBuf;    // [client use ONLY]
    // }}}
};

/*!
    client in server (running on mk)
*/
class MRDPClientStub : public MRDPTransceiver {
Q_OBJECT
    
public:
    MRDPClientStub(MRDPConnMgr *mgr, uint clientID, QTcpSocket *ioDev);
    virtual ~MRDPClientStub();

public slots:
    void handleSockError(QAbstractSocket::SocketError err);    
    void handleParseError();
    
public:
    uint clientID();
    uint screenDepth();
    
public:
    void handleNegotiateReq(const ::mrdp::NegotiateReq &req);           // client negotite req
    void handleCloseReq(const ::mrdp::CloseReq &req);                   // client close req
    void handleMotionActionReq(const ::mrdp::MotionActionReq &req);     // client mouse move action req
    void handleButtonActionReq(const ::mrdp::ButtonActionReq &req);     // client mouse button/key button action req
    void handleKeyActionReq(const ::mrdp::KeyActionReq &req);           // client key action req
    void handleScreenUpdateReq(const ::mrdp::ScreenUpdateReq &req);     // client screen update req
    void handleScreenUpdateReportModeReq(const ::mrdp::ScreenUpdateReportModeReq &req);  // client screen update report mode [configuration] req

protected:
    void postNegotiateEvt();
    void postNegotiateResp();           // client negotiate resp
    void postScreenUpdatedEvt(QPlatformCompositor &compositor, bool activeMode, bool fullScreen = false);
public: 
    void postScreenResizedEvt();
    void postScreenUpdatedEvtChecked(QPlatformCompositor &compositor, const QRegion &sdr);
protected:  
    void postServerShutdownEvt();
    void postCloseResp();               // client close resp

protected:
    QImage::Format remoteQFormat();    
    
private:    
    uint                                        id;              // client id
    SessionState                                ss;              // session state
    ::mrdp::Switch                              compress_;
    uint                                        reqVersion;
    ::mrdp::ScreenUpdateReportMode              surm;            // report screen update actively
    QRegion                                     screenDirtyAccum;    // accumulated dirty region, passive ONLY

    MRDPConnMgr                                *connMgr;

    // message processors - share the message parser in base class
    MessageProcessor< ::mrdp::REQUEST_NEGOTIATION>             negoReqHandler;
    MessageProcessor< ::mrdp::REQUEST_CLOSE>                   closeReqHandler;
    MessageProcessor< ::mrdp::REQUEST_MOTIONACTION>            maReqHandler;
    MessageProcessor< ::mrdp::REQUEST_BUTTONACTION>            baReqHandler;
    MessageProcessor< ::mrdp::REQUEST_KEYACTION>               kaReqHandler;
    MessageProcessor< ::mrdp::REQUEST_SCREENUPDATE>            suReqHandler;
    MessageProcessor< ::mrdp::REQUEST_SCREENUPDATEREPORTMODE>  surmReqHandler;
};

/*!
    server in client (running on pc)
*/
class MRDPServerProxy : public MRDPTransceiver {
    Q_OBJECT
    
public:
    MRDPServerProxy(QTcpSocket *ioDev);
    ~MRDPServerProxy();

public:
    uint clientID();
    uint screenDepth();

public:
    void handleNegotiateResp(const ::mrdp::NegotiateResp &resp);
    void handleCloseResp(const ::mrdp::CloseResp &resp);
    void handleNegotiateEvt(const ::mrdp::NegotiateEvt &evt, uint clientid);
    void handleScreenUpdatedEvt(const ::mrdp::ScreenUpdatedEvt &evt);
    void handleScreenResizedEvt(const ::mrdp::ScreenResizedEvt &evt);
    void handleServerShutdownEvt(const ::mrdp::ServerShutdownEvt &evt);

public:
    void postNegotiateReq();
    void postCloseReq();            // client close actively
    void postScreenUpdateReq(::mrdp::ScreenUpdateMode sum);     // client query actively
    void postMotionActionReq(int x, int y);
    void postButtonActionReq(uint button, ::mrdp::ButtonActionPress state);
    void postKeyActionReq(uint key, ::mrdp::KeyActionDown state);
    void postScreenUpdateReportModeReq(::mrdp::ScreenUpdateReportMode);

public:
    void setScreenImage(const QImage &img);

protected:
    QImage::Format remoteQFormat();
    
signals:        // notify ui
    void negotiationFinished(QImage::Format screenFormat, const QRect &screenRect);
    void screenResized(QSize sz);
    void screenUpdated(const QRegion &dirty);       
    void closeConfirmed();  // server confirm close 
    void serverShutdown();

private:
    QRegion                                     screenDirty;
    QImage::Format                          remoteQFormat_;       // server reported format, ref. NegotiateResp
    QImage                                      screenImage_;        // shared with ui
    uint                                        clientID_;

    // message processors - share the message parser in base class
    MessageProcessor< ::mrdp::RESPONSE_NEGOTIATION>     negoRespHandler;
    MessageProcessor< ::mrdp::RESPONSE_CLOSE>           closeRespHandler;
    MessageProcessor< ::mrdp::EVENT_NEGOTIATION>        negoEvtHandler;
    MessageProcessor< ::mrdp::EVENT_SCREENUPDATED>      suEvtHandler;
    MessageProcessor< ::mrdp::EVENT_SCREENRESIZED>      srEvtHandler;
    MessageProcessor< ::mrdp::EVENT_SERVERSHUTDOWN>     ssEvtHandler;
};

/*!
    server side only !!!
*/
class MRDPConnMgr : QObject {
    Q_OBJECT    
        
public:
    static MRDPConnMgr& instance(); 
    bool startService(QPlatformCompositor *compstor);        // start listen
signals:
    void startService__(QPlatformCompositor *compstor, bool *ret);
private slots:
    void doStartService(QPlatformCompositor *compstor, bool *ret);
public:    
    void stopService();         // stop listen, clean up all existed connections and client stubs
signals:
    void stopService__();
private slots:
    void doStopService();
public:
    bool serviceRunning();
public:    
    // a client closed: watch out the non-invasive smart pointer limitation
    void cleanupConnection(MRDPClientStub *client); 
    void setScreenGeometry(const QRect &newGeometry);   // such as changing screen orientation, called by compositor
    QRect screenGeometry();                             // called by client stubs
    void setScreenDepth(int depth);
    int screenDepth();                                  // called by client stubs
    QPlatformCompositor &compositor();            // for client stub usage
    int input();                 // for client stub usage           

public:    
    void tryNotifyScreenUpdated(const QRegion& sdr);      // compositor thread
private slots:
    void doNotifyScreenUpdated(QRegion sdr);      // mrdp thread (transfered)

signals:
    void screenUpdated(QRegion sdr);

public slots:
    void acceptConnection();            // new client arrived

protected:
    void initCompositor(QPlatformCompositor *compstor);
    void deInitCompositor();
    void initInput();
    void deInitInput();

private:
    MRDPConnMgr(const QRect &scrnGeom = QRect(), int depth = 32);
    virtual ~MRDPConnMgr();
    ::mrdp::ErrorNo initminilzo();

private:
    int                         screenDepth_;
    QRect                       screenGeometry_;     // offset + size
    QPlatformCompositor         *compositor_;        // compositor
    int                         vinputFD;            // inputmgr vinput fd(virtual input device)
    QTcpServer                  listener;
    QList< QSharedPointer<MRDPClientStub> >   clientStubs;            
    QAtomicInt                         clientIDGenerator;  // global client id generator
    static MRDPConnMgr                       *self;
    static QThread                           mrdpThread;         // 1 thread for all connections, thread pool is unnecessary

    bool                       serviceRunning_;
};

class MRDPUtil {
public: 
    static QImage::Format MRDPFormat2QFormat(::mrdp::Format mrdpFormat);    
    static QRect MRDPRect2QRect(const ::mrdp::Rect &mrdpRc);
    static inline bool checkSS(SessionState ssCurrent, uint ssExpected, const char* location);
    static inline void changeSS(SessionState &ssCurrent, SessionState ssExpected, const char* location);
};

#endif  // MRDP_P_H

