#include "mrdp_p.h"
#include <QtCore/qvector.h>
#include <QtCore/qrect.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qendian.h>
#include <QtGui/qpainter.h>
#ifndef Q_OS_WIN        // there's no inputmgr under windows platform
#include "InputMgr.h"
#else
int vInputConnect() {return -1;}
int vInputSendButtonEvent(int fd, int button, int press) 
{
    Q_UNUSED(fd);
    Q_UNUSED(button);
    Q_UNUSED(press);    
    return -1;
}
int vInputSendKeyEvent(int fd, int key, int down) 
{
    Q_UNUSED(fd);
    Q_UNUSED(key);
    Q_UNUSED(down);  
    return -1;
}
int vInputSendMotionEvent(int fd, int x, int y)
{
    Q_UNUSED(fd);
    Q_UNUSED(x);
    Q_UNUSED(y); 
    return -1;
}
int vInputClose(int fd)
{
    Q_UNUSED(fd);
    return -1;
}
#endif
#include <exception>
#ifdef Q_PROCESSOR_ARM
#include "cmem.h"
#else
int CMEM_cacheInv(void *ptr, size_t size)
{
    Q_UNUSED(ptr);
    Q_UNUSED(size);
    return -1;
}
#endif
#include <QtCore/qfile.h>
#include <errno.h>

////////////////////////////// DEBUG ////////////////////////////////////////
//#define MRDP_DEBUG_DUMP_PIXELS 1
//#define MRDP_DEBUG_TRACE_PIXELS 1
//#define MRDP_DEBUG_TRACE_REGIONS 1
//#define MRDP_DEBUG_TRACE_MSG 1
//#define MRDP_DEBUG_TRACE_EXTRA_HEADER 1
//#define MRDP_DEBUG_TRACE_STATE  1

/////////////////////////////////////////////// minilzo /////////////////////////////////////////////////
#define IN_LEN(in_len)      (in_len)
#define OUT_LEN(in_len)     (in_len + in_len / 16 + 64 + 3)

/////////////////////////////////////////////// message handler /////////////////////////////////////////
MessageParser::MessageParser()
{

}

MessageParser::~MessageParser()
{

}

void MessageParser::enlist(MessageProcessorBase *processor)
{
    processors[processor->getID()] = processor;
}

/*!
    parse the messages into rootMsg, then deliver them to specified processors
*/
void MessageParser::exec(const uchar* buf, size_t size, MRDPTransceiver& peer)
{
    // parse message
    rootMsg.ParseFromArray(buf, size);
#ifdef MRDP_DEBUG_TRACE_MSG
    qDebug("%s: [recv] msg id(%d)", __FUNCTION__, rootMsg.id());
#endif  
    // find the concrete handler to process
    QMap< ::mrdp::MSG_ID, MessageProcessorBase*>::const_iterator it = processors.find(rootMsg.id());
    if(it != processors.end()) {
        it.value()->process(rootMsg, peer);
    } else {
        qWarning("no handler found for msg(%d)", rootMsg.id());
    }
}

ParseState MessageParser::getState()
{
    return ps_;
}

void MessageParser::setState(ParseState ps)
{
    ps_ = ps;
}

template<>
void MessageProcessor< ::mrdp::REQUEST_NEGOTIATION>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if (!msg.has_req()) {
        qWarning("no request found: %d", msg.id());
        return;
    }
    const ::mrdp::Request &req = msg.req();
    // check client id
    if(peer.clientID() != req.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), peer.clientID(), req.clientid());
        return;
    }

    if(!req.has_negoreq()) {
        qWarning("no neogreq found: %d", msg.id());
        return;
    }

    const ::mrdp::NegotiateReq &negoReq = req.negoreq();

    peer.handleNegotiateReq(negoReq);
}

template<>
void MessageProcessor< ::mrdp::REQUEST_CLOSE>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_req()) {
        qWarning("no request payload found :%d", msg.id());
        return;
    }
    const ::mrdp::Request &req = msg.req();
    // check client id
    if(peer.clientID() != req.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), peer.clientID(), req.clientid());
        return;
    }

    if(!req.has_closereq()) {
        qWarning("no closereq found :%d", msg.id());
        return;
    }
    const ::mrdp::CloseReq &closeReq = req.closereq();

    peer.handleCloseReq(closeReq);
}

template<>
void MessageProcessor< ::mrdp::REQUEST_MOTIONACTION>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_req()) {
        qWarning("no request payload found :%d", msg.id());
        return;
    }
    const ::mrdp::Request &req = msg.req();
    // check client id
    if(peer.clientID() != req.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), peer.clientID(), req.clientid());
        return;
    }

    if(!req.has_mareq()) {
        qWarning("no mareq found :%d", msg.id());
        return;
    }
    const ::mrdp::MotionActionReq &maReq = req.mareq();

    peer.handleMotionActionReq(maReq);
}

template<>
void MessageProcessor< ::mrdp::REQUEST_BUTTONACTION>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_req()) {
        qWarning("no request payload found :%d", msg.id());
        return;
    }
    const ::mrdp::Request &req = msg.req();
    // check client id
    if(peer.clientID() != req.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), peer.clientID(), req.clientid());
        return;
    }

    if(!req.has_bareq()) {
        qWarning("no bareq found :%d", msg.id());
        return;
    }
    const ::mrdp::ButtonActionReq &maReq = req.bareq();

    peer.handleButtonActionReq(maReq);
}

template<>
void MessageProcessor< ::mrdp::REQUEST_KEYACTION>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_req()) {
        qWarning("no request payload found :%d", msg.id());
        return;
    }
    const ::mrdp::Request &req = msg.req();
    // check client id
    if(peer.clientID() != req.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), peer.clientID(), req.clientid());
        return;
    }

    if(!req.has_kareq()) {
        qWarning("no kareq found :%d", msg.id());
        return;
    }
    const ::mrdp::KeyActionReq &kaReq = req.kareq();

    peer.handleKeyActionReq(kaReq);
}

template<>
void MessageProcessor< ::mrdp::REQUEST_SCREENUPDATE>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_req()) {
        qWarning("no request payload found :%d", msg.id());
        return;
    }
    const ::mrdp::Request &req = msg.req();
    // check client id
    if(peer.clientID() != req.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), peer.clientID(), req.clientid());
        return;
    }

    if(!req.has_sureq()) {
        qWarning("no sureq found :%d", msg.id());
        return;
    }
    const ::mrdp::ScreenUpdateReq &suReq = req.sureq();

    peer.handleScreenUpdateReq(suReq);
}

template<>
void MessageProcessor< ::mrdp::REQUEST_SCREENUPDATEREPORTMODE>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_req()) {
        qWarning("no request payload found :%d", msg.id());
        return;
    }
    const ::mrdp::Request &req = msg.req();
    // check client id
    if(peer.clientID() != req.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), peer.clientID(), req.clientid());
        return;
    }

    if(!req.has_sureq()) {
        qWarning("no sureq found :%d", msg.id());
        return;
    }
    const ::mrdp::ScreenUpdateReportModeReq &surmReq = req.surmreq();

    peer.handleScreenUpdateReportModeReq(surmReq);
}

template<>
void MessageProcessor< ::mrdp::RESPONSE_NEGOTIATION>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_resp()) {
        qWarning("no response payload found :%d", msg.id());
        return;
    }
    const ::mrdp::Response &resp = msg.resp();
    // check client id
    if(peer.clientID() != resp.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), resp.clientid(), peer.clientID());
        return;
    }
    if(!resp.has_negoresp()) {
        qWarning("no negoresp found :%d", msg.id());
        return;
    }
    const ::mrdp::NegotiateResp &negoResp = resp.negoresp();

    peer.handleNegotiateResp(negoResp);
}

template<>
void MessageProcessor< ::mrdp::RESPONSE_CLOSE>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_resp()) {
        qWarning("no response payload found :%d", msg.id());
        return;
    }
    const ::mrdp::Response &resp = msg.resp();
    // check client id
    if(peer.clientID() != resp.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), resp.clientid(), peer.clientID());
        return;
    }

    if(!resp.has_closeresp()) {
        qWarning("no closeresp found :%d", msg.id());
        return;
    }
    const ::mrdp::CloseResp &closeResp = resp.closeresp();

    peer.handleCloseResp(closeResp);
}

template<>
void MessageProcessor< ::mrdp::EVENT_NEGOTIATION>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_evt()) {
        qWarning("no event payload found :%d", msg.id());
        return;
    }
    const ::mrdp::Event &evt = msg.evt();
    // negotiation not finished, no checking clientid
    if(!evt.has_clientid()) {
        qWarning("no clientid found :%d", msg.id());
        return;
    }
    if(!evt.has_negoevt()) {
        qWarning("no negoevt found :%d", msg.id());
        return;
    }
    const ::mrdp::NegotiateEvt &negoEvt = evt.negoevt();

    peer.handleNegotiateEvt(negoEvt, evt.clientid());
}

template<>
void MessageProcessor< ::mrdp::EVENT_SCREENUPDATED>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_evt()) {
        qWarning("no event payload found :%d", msg.id());
        return;     
    }
    const ::mrdp::Event &evt = msg.evt();
    // check client id
    if(peer.clientID() != evt.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), evt.clientid(), peer.clientID());
        return;
    }   

    if(!evt.has_suevt()) {
        qWarning("no suevt found :%d", msg.id());
        return;
    }
    const ::mrdp::ScreenUpdatedEvt &suEvt = evt.suevt();

    peer.handleScreenUpdatedEvt(suEvt);
}

template<>
void MessageProcessor< ::mrdp::EVENT_SCREENRESIZED>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_evt()) {
        qWarning("no event payload found :%d", msg.id());
        return;     
    }
    const ::mrdp::Event &evt = msg.evt();
    // check client id
    if(peer.clientID() != evt.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), evt.clientid(), peer.clientID());
        return;
    }   

    if(!evt.has_srevt()) {
        qWarning("no suevt found :%d", msg.id());
        return;
    }
    const ::mrdp::ScreenResizedEvt &svEvt = evt.srevt();

    peer.handleScreenResizedEvt(svEvt);
}

template<>
void MessageProcessor< ::mrdp::EVENT_SERVERSHUTDOWN>::process(const ::mrdp::Message &msg, MRDPTransceiver& peer)
{
    if(!msg.has_evt()) {
        qWarning("no event payload found :%d", msg.id());
        return;     
    }
    const ::mrdp::Event &evt = msg.evt();
    // check client id
    if(peer.clientID() != evt.clientid()) {
        qWarning("clientid mismatch :%d, clientid(server:%d, client:%d)", msg.id(), evt.clientid(), peer.clientID());
        return;
    }   

    if(!evt.has_ssevt()) {
        qWarning("no suevt found :%d", msg.id());
        return;
    }
    const ::mrdp::ServerShutdownEvt &ssEvt = evt.ssevt();

    peer.handleServerShutdownEvt(ssEvt);
}

////////////////////////////////////////////// mrdp transceiver /////////////////////////////////////////
MRDPTransceiver::MRDPTransceiver(QTcpSocket *ioDev) : sock(ioDev), 
    packedBufOffset(0), packedBufCapacity(0), compressedBufOffset(0), compressedBufCapacity(0),
    payload(NULL), payloadSize(0), allocatedPayloadBytes(0), readInStart(0), readInExpect(0),
    unpackedOffset(0), decompressedBufOffset(0), decompressedBufCapacity(0)
{
    reqParser.setState(PS_EXPECT_HEADER);

	// register sock's job
	connect(sock, SIGNAL(readyRead()), this, SLOT(parseMessage()));	
	connect(this, SIGNAL(parseError()), this, SLOT(handleParseError()));						// override
	connect(this, SIGNAL(nextMessage()), this, SLOT(parseMessage()), Qt::QueuedConnection);		// watch out!!!
	connect(sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleSockError(QAbstractSocket::SocketError)));	// override
}

MRDPTransceiver::~MRDPTransceiver()
{
    cleanMsgPool();

	// register sock's job
	disconnect(sock, SIGNAL(readyRead()), this, SLOT(parseMessage()));	
	disconnect(this, SIGNAL(parseError()), this, SLOT(handleParseError()));
	disconnect(this, SIGNAL(nextMessage()), this, SLOT(parseMessage()));
	disconnect(sock, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleSockError(QAbstractSocket::SocketError)));
}

QTcpSocket* MRDPTransceiver::socket()
{
    return sock;
}

void MRDPTransceiver::parseMessage()
{
    int bytesAvailable = (int)sock->bytesAvailable();
    ParseState ps = reqParser.getState();

    switch(ps) {
    case PS_EXPECT_HEADER: {      // parse extra header
        if(bytesAvailable < EXTRA_HEADER_SIZE) {        // whole header in one read
            // do nothing until next readyRead(data can be buffered by QAbstractSocket)
            return;
        }
        // read now
        uchar buf[EXTRA_HEADER_SIZE];            
        readInExpect = EXTRA_HEADER_SIZE;
        readInStart = 0;
        Q_ASSERT(bytesAvailable >= EXTRA_HEADER_SIZE);
        while(readInExpect > 0) {
            int readBytes = sock->read((char*)(buf) + readInStart, readInExpect);   // never blocked
            if(-1 == readBytes) {
                qWarning("broken socket ??? %s", __FUNCTION__);
                reqParser.setState(PS_ERROR);
                return;
            }
            readInExpect -= readBytes;
            readInStart += readBytes;
        }
        ExtraHeader *header = reinterpret_cast<ExtraHeader*>(buf);
        size_t checkSize = size_t(reinterpret_cast<uchar*>(&header->crc16) - reinterpret_cast<uchar*>(header));
        Q_ASSERT(checkSize == 4);   // sizeof(uint)
        uint crc16 = qChecksum(reinterpret_cast<char*>(header), checkSize);
        if(crc16 != header->crc16) {
            qWarning("extra header checksum error(0x%x:0x%x), length maybe wrong(%d)", crc16, header->crc16, qFromLittleEndian(header->length));
            reqParser.setState(PS_ERROR);
            return;         // re-send ???
        }
        header->length = qFromLittleEndian(header->length);
        payloadSize = header->length - EXTRA_HEADER_SIZE;
        memcpy(&extraHeader, header, EXTRA_HEADER_SIZE);
        reqParser.setState(PS_EXPECT_PAYLOAD);
#ifdef MRDP_DEBUG_TRACE_EXTRA_HEADER
        qDebug("%s: extra header length(%d), crc(0x%x)", __FUNCTION__, header->length, header->crc16);
#endif       
        bytesAvailable = (int)sock->bytesAvailable();
        // socket received more data than header parsing needed
        if(bytesAvailable > 0) {    
            emit nextMessage();  // continue parse payload
        }
        break;
        }
    case PS_EXPECT_PAYLOAD: {    // read a payload, then parse via MessageParser 
        if(bytesAvailable < payloadSize) {        // whole payload in one read, thanks to QAbstractSocket::setReadBufferSize
            // do nothing until next readyRead(data can be buffered by QAbstractSocket)
            return;
        }
        // WATCHOUT !!! no shrink for msg buffer ...
        if(!payload) {                                  // first allocation
            payload = new uchar[payloadSize];
            allocatedPayloadBytes = payloadSize;
        }
        if(payloadSize > allocatedPayloadBytes) {       // resize
            delete [] payload;
            payload = new uchar[payloadSize];
            allocatedPayloadBytes = payloadSize;
        }
        // rewind for new payload
        readInStart = 0;        
        readInExpect = payloadSize;
        Q_ASSERT(bytesAvailable >= payloadSize);
        while(readInExpect > 0) {
            int readBytes = sock->read((char*)(payload) + readInStart, readInExpect);       // never blocked
            if(-1 == readBytes) {
                qWarning("broken socket ??? %s", __FUNCTION__);
                reqParser.setState(PS_ERROR);
                return;
            }
            readInStart += readBytes;
            readInExpect -= readBytes;
        }
        reqParser.exec(payload, payloadSize, *this);      
        reqParser.setState(PS_EXPECT_HEADER);

        // bytesAvailable maybe change after QAbstractSocket::read, stale value may cause hang(readRead never emitted)
        bytesAvailable = (int)sock->bytesAvailable();
        // if there's something left, continue to parse, note: use queued signal instead of direct calling to avoid infinite recursion
        if(bytesAvailable > 0) {
            emit nextMessage();
        }
        break;
        }
    case PS_ERROR:
    default:
        qWarning("parse in error state while %s", __FUNCTION__);
        // TODO: can we find another proper header in coming data to recover ???
        emit parseError();
        break;
    }
}

void MRDPTransceiver::handleSockError(QAbstractSocket::SocketError err) 
{
    qWarning("%s: sock error(%d)", __FUNCTION__, err);
}

void MRDPTransceiver::handleParseError()
{
    qWarning("%s: parse error", __FUNCTION__);
}

inline void MRDPTransceiver::buildExtraHeader(ExtraHeader &hdr, size_t payloadSize) 
{
    size_t headerSize = EXTRA_HEADER_SIZE;
    size_t headerSize2 = size_t(reinterpret_cast<uchar*>(&hdr.crc16) - reinterpret_cast<uchar*>(&hdr));
    hdr.length = qToLittleEndian(headerSize + payloadSize);
    hdr.crc16 = qChecksum(reinterpret_cast<char*>(&hdr), headerSize2);

#ifdef MRDP_DEBUG_TRACE_EXTRA_HEADER
    qDebug("buildExtraHeader: checksum(0x%x), length(%d)", hdr.crc16, hdr.length);
#endif
}

inline ::mrdp::Request* MRDPTransceiver::buildReqHeader(::mrdp::Message& msg, uint clientid)
{
    ::mrdp::Request *req = msg.mutable_req();
    req->set_clientid(clientid);
    return req;
}

inline ::mrdp::Event* MRDPTransceiver::buildEvtHeader(::mrdp::Message& msg, uint clientid)
{
    ::mrdp::Event *evt = msg.mutable_evt();
    evt->set_clientid(clientid);
    return evt;
}

inline mrdp::Response* MRDPTransceiver::buildRespHeader(::mrdp::Message& msg, uint clientid, ::mrdp::ErrorNo error)
{
    ::mrdp::Response *resp = msg.mutable_resp();
    resp->set_clientid(clientid);
    resp->set_error(error);
    return resp;
}

// send ExtraHeader and payload seperately
// always little-endian
void MRDPTransceiver::sendExtraHeader(const ExtraHeader &hdr)
{
    qint64 bytesToWrite = sizeof(hdr);
    Q_ASSERT(bytesToWrite == EXTRA_HEADER_SIZE);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    const uchar *buf = reinterpret_cast<const uchar *>(&hdr);        
#else
    const uchar buf[EXTRA_HEADER_SIZE];
    qToLittleEndian(hdr.length, &buf[0]);
    // crc is endianless
#endif  
    while(bytesToWrite > 0) {
        qint64 bytesWritten = sock->write((char*)(buf), bytesToWrite);
        if(-1 == bytesWritten) {     // error happened
            qWarning("%s: write sock failed, len(%d)", __FUNCTION__, bytesToWrite);
            handleSockError(sock->error());   // error will be handled by serverproxy or clientstub independently
            return;
        }
        sock->flush();
        bytesToWrite -= bytesWritten;
    }
}

// srcImg -> packedBuf
// return: new dstPos
uint MRDPTransceiver::pack(uint dstPos, const QRect &dirty, const QImage &srcImg)
{
    if(packedBuf.isNull() || dstPos >= packedBufCapacity) {
        qWarning("packedBuf is not ready!!!");
        return (uint)(-1);
    }

    QRect effectiveDirty = dirty.intersected(srcImg.rect());
    uchar *dstBits = packedBuf.data() + dstPos;
    uint bytesPerPixel = screenDepth() / 8;
    uint dstScanLine = effectiveDirty.width() * bytesPerPixel;
    uint srcPos = (srcImg.width() * effectiveDirty.top() + effectiveDirty.left()) * bytesPerPixel;
    const uchar *srcBits = srcImg.constBits() + srcPos;
    uint srcScanLine = srcImg.width() * bytesPerPixel;
    for(int i = 0; i < effectiveDirty.height(); i++) {
        CMEM_cacheInv(const_cast<uchar*>(srcBits), dstScanLine);
        memcpy(dstBits, srcBits, dstScanLine);
        srcBits += srcScanLine;
        dstBits += dstScanLine;     // packed
    }

    uint newDstPos = dstPos + effectiveDirty.width() * effectiveDirty.height() * bytesPerPixel;
    if(newDstPos > packedBufCapacity) {
        qWarning("packed buffer overflow");
        return (uint)(-1);
    }

    return newDstPos;
}

/*!
    CAUTION: 
    1, Message::SerializeToFileDescriptor can not work normaly, since
    QAbstractSocket is always non-blocking socket, ref. QNativeSocketEngine::initialize
    but protobuf can not handling non-blocking socket properly, ref. FileOutputStream::CopyingFileOutputStream::Write    
    2, socket/WSASocket can not be accessed by read/write under MS windows
*/
void MRDPTransceiver::sendMsgBody(::mrdp::Message *msg)
{
    msg->SerializeToString(&msgSendStr);
    qint64 bytesToWrite = msgSendStr.size();
    while(bytesToWrite > 0) {
        qint64 bytesWritten = sock->write(msgSendStr.data(), bytesToWrite);
        if(-1 == bytesWritten) {     // error happened
            qWarning("%s: write msg(%d) to sock failed", __FUNCTION__, msg->id());
            handleSockError(sock->error());   // error will be handled by serverproxy or clientstub independently
            return;
        }
        sock->flush();
        bytesToWrite -= bytesWritten;
    }
}

// decompressedBuf -> dstImg
uint MRDPTransceiver::unpack(uint srcPos, const QRect &dirty, QImage &dstImg)
{
    uint dirtyPixelsInByte = dirty.width() * dirty.height() * screenDepth() / 8;

    // blit to dst img at dirty position by dirty length, maybe format transform
    // NOTE: since dstImg's bits is shared by mutiple objects(pixmap, image, ...), bits will be detached in QPainter's ctor, which means
    // mk screen's updating will not be shown in PC's monitor, so, we do it manually instead of QPainter::drawImage...

    uchar *dstBits = const_cast<uchar*>(dstImg.constBits()) + (dirty.top()  * dstImg.width()  + dirty.left()) * screenDepth() / 8;
    const uchar*srcBits = decompressedBuf.data() + srcPos;
    int srcScanlineInBytes = dirty.width() * screenDepth() / 8;
    int dstScanlineInBytes = dstImg.width() * screenDepth() / 8;
    for(int i = 0; i < dirty.height(); i++) {                  // copy line by line
        memcpy(dstBits, srcBits, srcScanlineInBytes);
        srcBits += srcScanlineInBytes;
        dstBits += dstScanlineInBytes;
    }

#if defined(MRDP_DEBUG_TRACE_PIXELS)
    qDebug("dstImg(%p)", dstImg.constBits());
#endif
    return dirtyPixelsInByte;    // new offset
}

// packedBuf -> compressedBuf
::mrdp::ErrorNo MRDPTransceiver::compress()
{
    if(compressedBufCapacity <= 0) {
        qWarning("compressedBuf is not ready: %s", __FUNCTION__);
        return ::mrdp::ERR_FAILURE_GENERIC;
    }

    int r;
    uchar *outBuf = compressedBuf.data(); 

    //compress from 'in' to 'out' with LZO1X-1
    r = lzo1x_1_compress(packedBuf.data(), packedBufOffset, outBuf, (lzo_uint*)(&compressedBufOffset), wrkmem);

    if (r != LZO_E_OK) {
        qWarning("lzo internal error - compression failed: %d !!!", r);
        return ::mrdp::ERR_LZO;
    }

    if (compressedBufOffset >= packedBufOffset)
    {
        qWarning("This block contains incompressible data");
        return ::mrdp::ERR_LZO;
    }

    if (compressedBufOffset >= compressedBufCapacity) {
        qWarning("compress output buffer overflow: len(%d), capacity(%d)",
            compressedBufOffset, compressedBufCapacity);
        return ::mrdp::ERR_LZO;
    }

    return ::mrdp::ERR_SUCCESS;
}

// src(compressed) -> decompressedBuf
::mrdp::ErrorNo MRDPTransceiver::decompress(const std::string& src)
{
    if(decompressedBuf.isNull()) {
        qWarning("decompressedBuf is not ready: %s", __FUNCTION__);     
        return ::mrdp::ERR_FAILURE_GENERIC;
    }

    int r;
    uchar *outBuf = decompressedBuf.data();

    //decompress from 'in' to 'out' with LZO1X-1
    r = lzo1x_decompress(reinterpret_cast<const uchar*>(src.data()), src.size(), 
        outBuf, reinterpret_cast<lzo_uint*>(&decompressedBufOffset), NULL);

    if (r != LZO_E_OK) {
        qWarning("lzo internal error - decompression failed: %d !!!", r);
        return ::mrdp::ERR_LZO;
    }

    if (decompressedBufOffset <= src.size())
    {
        qWarning("This block contains in-decompressible data");
        return ::mrdp::ERR_LZO;
    }

    if (decompressedBufOffset > decompressedBufCapacity) {
        qWarning("decompress output buffer overflow: len(%d), capacity(%d), capacity_src_str(%d)",
            decompressedBufOffset, decompressedBufCapacity, src.capacity());
        return ::mrdp::ERR_LZO;
    }

    return ::mrdp::ERR_SUCCESS; 
}

::mrdp::Message *MRDPTransceiver::getMsgToSendFromPool(::mrdp::MSG_ID id)
{
    ::mrdp::Message *ret = NULL;
    QMap< ::mrdp::MSG_ID, ::mrdp::Message*>::const_iterator i = msgPool.find(id);
    while(i != msgPool.end() && i.key() == id) {
        ret = i.value();
        break;          // first match is ok
        ++i;
    }

    if(!ret) {
        ret = new ::mrdp::Message();
        ret->set_id(id);
        msgPool.insert(id, ret);
    }
#ifdef MRDP_DEBUG_TRACE_MSG
    qDebug("%s: [send] msg id(%d)", __FUNCTION__, id);
#endif
    return ret;
}

void MRDPTransceiver::cleanMsgPool()
{
    foreach(::mrdp::Message* msg, msgPool) {
        delete msg;
        msg = NULL;
    }

    msgPool.clear();
}

////////////////////////////////////////////// mrdp client stub ////////////////////////////////////////

MRDPClientStub::MRDPClientStub(MRDPConnMgr *mgr, uint clientid, QTcpSocket *ioDev) : MRDPTransceiver(ioDev),
    id(clientid), ss(SS_CLOSED), surm(::mrdp::SURM_ACTIVE), connMgr(mgr),
    negoReqHandler(&reqParser),
    closeReqHandler(&reqParser),
    maReqHandler(&reqParser),
    baReqHandler(&reqParser),
    kaReqHandler(&reqParser),
    suReqHandler(&reqParser),
    surmReqHandler(&reqParser)
{
#ifdef MRDP_DEBUG_TRACE_STATE
    qDebug("MRDPClientStub(%p) ctor: ss(%d)", this, ss);
#endif    
    Q_ASSERT(mgr);
    uint packedBufMaxLen = (mgr->screenDepth() / 8) * mgr->screenGeometry().width() * mgr->screenGeometry().height();
    packedBufCapacity = IN_LEN(packedBufMaxLen);
    // build pack buffer
    uchar *packedBits = new uchar[packedBufCapacity];
    packedBuf.reset(packedBits);
    memset(packedBits, 0, packedBufCapacity);

    // build compress buffer
    compressedBufCapacity = OUT_LEN(packedBufMaxLen);
    uchar *compressedBits = new uchar[compressedBufCapacity];
    compressedBuf.reset(compressedBits);
    memset(compressedBits, 0, compressedBufCapacity);

    postNegotiateEvt();                 // server advise to clients immediately on connection established
}

MRDPClientStub::~MRDPClientStub() 
{
#ifdef MRDP_DEBUG_TRACE_STATE
    qDebug("MRDPClientStub(%p) dtor: ss(%d)", this, ss);
#endif
    if(SS_CLOSING == ss || SS_ERROR == ss) {  // client ask to close
        ;
    } else {                // server shutdown actively, [stopService]
        postServerShutdownEvt();
    }
    MRDPUtil::changeSS(ss, SS_CLOSED, __FUNCTION__);
    sock->disconnectFromHost();
}

// expect negotiate req
void MRDPClientStub::postNegotiateEvt() 
{
    if(!MRDPUtil::checkSS(ss, SS_CLOSED, __FUNCTION__))
        return;

    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::EVENT_NEGOTIATION);

    // Event
    ::mrdp::Event *evt = buildEvtHeader(*msg, id);

    // NegotiateEvt
    ::mrdp::NegotiateEvt *negoEvt = evt->mutable_negoevt();
    // server prtocol version
    negoEvt->set_serverversion(::mrdp::PROTOVER_CURRENT);
    // compression switch
    negoEvt->set_compresspixelsuggestion(::mrdp::SW_ON);
    // surm
    negoEvt->set_surm(surm);

    // extra header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);

    MRDPUtil::changeSS(ss, SS_NEGOTIATE_0, __FUNCTION__);
}

void MRDPClientStub::postNegotiateResp()
{
    if(!MRDPUtil::checkSS(ss, SS_NEGOTIATE_1, __FUNCTION__))
        return;

    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::RESPONSE_NEGOTIATION);

    // Response - compress switch must be ON
    ::mrdp::Response *resp = buildRespHeader(*msg, id,
        ::mrdp::SW_ON == this->compress_ ? ::mrdp::ERR_SUCCESS : ::mrdp::ERR_INVALID_PARAMETER);

    // NegotiageResp
    ::mrdp::NegotiateResp *negoResp = resp->mutable_negoresp();
    negoResp->set_serverversion(static_cast< ::mrdp::ProtocolVersion>(
        qMin(reqVersion, static_cast<uint>(::mrdp::PROTOVER_CURRENT))));
    negoResp->set_format(::mrdp::FMT_ARGB32_PREMUL);

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    negoResp->set_pixelinbigendian(::mrdp::SW_OFF);
#else
    negoResp->set_pixelinbigendian(::mrdp::SW_ON);
#endif  
    
    ::mrdp::Rect *screenRect = negoResp->mutable_screenrect();
    QRect screenGeometry = connMgr->screenGeometry();
    screenRect->set_x(screenGeometry.left());
    screenRect->set_y(screenGeometry.top());
    screenRect->set_w(screenGeometry.width());
    screenRect->set_h(screenGeometry.height());

    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);

    if(resp->error() == ::mrdp::ERR_SUCCESS) {
        MRDPUtil::changeSS(ss, SS_ESTABLISHED, __FUNCTION__);
    } else {
        MRDPUtil::changeSS(ss, SS_ERROR, __FUNCTION__);
    }
}

/*!
    rgn: dirty region in screen coordinate
    img: screen image
    activeMode: see ::mrdp::ScreenUpdateReportMode
    compress stragy: 
    1, pack first(shorter locking period for compositor and contigous memory input for lzo)
    2, compress direct to protobuf object's internal buffer(std::string::data())
    TODO: queued these events to relieve compositor thread's blocking probability
*/
void MRDPClientStub::postScreenUpdatedEvt(QPlatformCompositor &compositor, bool activeMode, bool fullScreen) 
{
    // if client refused to recv, every byte sent by server will be filled into qt's internal buffer, causing memory exhausted...
    qint64 bytesBufferedByQt = sock->bytesToWrite();
    if(bytesBufferedByQt > MRDP_MAX_SEND_BUFFER_SIZE) {
        qWarning("%s: send buffer full(%d:%d), drop screen updated evt", 
        __FUNCTION__, bytesBufferedByQt, MRDP_MAX_SEND_BUFFER_SIZE);
        return;
    }
    
    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::EVENT_SCREENUPDATED);

    // Event
    ::mrdp::Event *evt = buildEvtHeader(*msg, id);

    // ScreenUpdatedEvt
    ::mrdp::ScreenUpdatedEvt *suEvt = evt->mutable_suevt();

    compositor.lockScreenResources();                     // mrdp snoop begin

    QRegion rgn;
    if(fullScreen) {
        rgn = connMgr->screenGeometry();
    } else {
        rgn = screenDirtyAccum;
    }
    if(rgn.isEmpty()) {
        qDebug("screen dirty region is NULL :%s", __FUNCTION__);
        compositor.unlockScreenResources();
        return;
    }
    const QImage &img = compositor.screenImage();
    if(img.isNull()) {
        qWarning("screen image is NULL :%s", __FUNCTION__);
        compositor.unlockScreenResources();
        return;
    }
    suEvt->clear_dirtyregion();
    suEvt->clear_dirtypixels();
    packedBufOffset = 0;
    QVector<QRect> rects = rgn.rects();
    for(int i = 0; i < rects.size(); i++) {
        const QRect &qrc = rects.at(i);
        // dirty region
        ::mrdp::Rect *mrdpRc = suEvt->add_dirtyregion();   // add first
        mrdpRc->set_x(qrc.x());                            // then assign values
        mrdpRc->set_y(qrc.y());
        mrdpRc->set_w(qrc.width());
        mrdpRc->set_h(qrc.height());   
#ifdef MRDP_DEBUG_TRACE_REGIONS
        qDebug("%s dirty rect(%d-%d): %d, %d, %d, %d", __FUNCTION__,
            i, rects.size(), qrc.x(), qrc.y(), qrc.width(), qrc.height());
#endif
        // dirty pixels
        packedBufOffset = pack(packedBufOffset, qrc, img); // pack it
        if((uint)(-1) == packedBufOffset) {
            compositor.unlockScreenResources();
            return;     // give up
        }
    }

    compositor.unlockScreenResources();     // mrdp snoop end
             
    screenDirtyAccum = QRegion();                           // clean screen dirty after pixels compression finished under active mode

    if(::mrdp::SW_ON == compress_) { 

//////////////////////////////////////////////
    {
#if defined(MRDP_DEBUG_TRACE_PIXELS) || defined(MRDP_DEBUG_DUMP_PIXELS)
        const uchar*dbg_daddr = packedBuf.data();
        size_t dbg_dsize = packedBufOffset;
        quint16 crc = 0;
        qDebug("%s before compress: crc(0x%x), daddr(%p), dsize(%d), start4(0x%x,0x%x,0x%x,0x%x), end4(0x%x,0x%x,0x%x,0x%x)", 
            __FUNCTION__, qChecksum(reinterpret_cast<const char*>(dbg_daddr), dbg_dsize), dbg_daddr, dbg_dsize, dbg_daddr[0], dbg_daddr[1], dbg_daddr[2], dbg_daddr[3],
            dbg_daddr[dbg_dsize-4], dbg_daddr[dbg_dsize-3], dbg_daddr[dbg_dsize-2], dbg_daddr[dbg_dsize-1]);
#endif

#if defined(MRDP_DEBUG_DUMP_PIXELS)
        QString ddf_name;
        ddf_name.sprintf("/tmp/ddf_%x", crc);
        QFile ddf(ddf_name);
        ddf.open(QIODevice::ReadWrite | QIODevice::Truncate);
        ddf.write(reinterpret_cast<const char*>(dbg_daddr), dbg_dsize);
        ddf.flush();
        ddf.close();
        QThread::msleep(1000);
#endif
    }
        // compress it
        if(::mrdp::ERR_SUCCESS != compress()) {
            return;     // give up
        }

        // we have to do 1 copy since std::string can not work with bare-naked pointer efficiently, for example, 
        // std::string::resize (auto) filling specified or null-terminated characters into additional space which is unexpected to(overriding compression output)
        // and protobuf always translates REPEATED BYTE to std::string 
        // for faster way, ref. RepeatedFiled.mutable_data
        if(!suEvt->has_dirtypixels()) {
            suEvt->set_dirtypixels(compressedBuf.data(), compressedBufOffset);   // 1-shot
        }

    {
#if defined(MRDP_DEBUG_TRACE_PIXELS) || defined(MRDP_DEBUG_DUMP_PIXELS)
        const char *dbg_caddr = reinterpret_cast<const char*>(compressedBuf.data());
        size_t dbg_csize = compressedBufOffset;
        qDebug("%s after compress: crc(0x%x), caddr(%p), csize(%d), start4(0x%x,0x%x,0x%x,0x%x), end4(0x%x,0x%x,0x%x,0x%x)", 
            __FUNCTION__, qChecksum(dbg_caddr, dbg_csize), dbg_caddr, dbg_csize, dbg_caddr[0], dbg_caddr[1], dbg_caddr[2], dbg_caddr[3],
            dbg_caddr[dbg_csize-4], dbg_caddr[dbg_csize-3], dbg_caddr[dbg_csize-2], dbg_caddr[dbg_csize-1]);
#endif
    }
//////////////////////////////////////////////

    } else {
        qWarning("screen update without compression is too much expensive");
    }   

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message evt is on the way
    sendMsgBody(msg);
}

//[mrdp thread]
void MRDPClientStub::postScreenUpdatedEvtChecked(QPlatformCompositor &compositor, const QRegion &sdr) 
{
    if(!MRDPUtil::checkSS(ss, SS_ESTABLISHED, __FUNCTION__))
        return;

    screenDirtyAccum += sdr;

    if(surm == ::mrdp::SURM_ACTIVE) {       // post screen updated evt immediately
        postScreenUpdatedEvt(compositor, true, false);
    }
}

void MRDPClientStub::postScreenResizedEvt() 
{
    if(!MRDPUtil::checkSS(ss, SS_ESTABLISHED, __FUNCTION__))
        return;

    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::EVENT_SCREENUPDATED);

    // Event
    ::mrdp::Event *evt = buildEvtHeader(*msg, id);

    // ScreenResizedEvt
    ::mrdp::ScreenResizedEvt *srEvt = evt->mutable_srevt();
    // new geometry
    ::mrdp::Rect *screenRect = srEvt->mutable_screenrect();
    QRect screenGeometry = connMgr->screenGeometry();
    screenRect->set_x(screenGeometry.left());
    screenRect->set_y(screenGeometry.top());
    screenRect->set_w(screenGeometry.width());
    screenRect->set_h(screenGeometry.height());    

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);
}

void MRDPClientStub::postCloseResp() 
{
    if(!MRDPUtil::checkSS(ss, SS_CLOSING, __FUNCTION__))
        return;

    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::RESPONSE_CLOSE);

    // Response
    ::mrdp::Response *resp = buildRespHeader(*msg, id);

    // CloseResp    
    // ::mrdp::CloseResp *closeResp = resp->mutable_closeresp();

    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);

    MRDPUtil::changeSS(ss, SS_CLOSED, __FUNCTION__);
}

void MRDPClientStub::postServerShutdownEvt() 
{
    if(!MRDPUtil::checkSS(ss, SS_NEGOTIATE_0 | SS_NEGOTIATE_1 | SS_ESTABLISHED | SS_CLOSING, __FUNCTION__))
        return;

    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::EVENT_SCREENUPDATED);

    // Event
    ::mrdp::Event *evt = buildEvtHeader(*msg, id);

    // ServerShutdownEvt    
    // ::mrdp::ServerShutdownEvt *srEvt = evt->mutable_ssevt();

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);

    MRDPUtil::changeSS(ss, SS_CLOSED, __FUNCTION__);
}

//[mrdp thread]
void MRDPClientStub::handleNegotiateReq(const ::mrdp::NegotiateReq &req) 
{
    if(!MRDPUtil::checkSS(ss, SS_NEGOTIATE_0, __FUNCTION__))
        return;

    this->reqVersion = req.clientversion();
    this->compress_ = req.compresspixels();
    this->surm = req.surm();

    MRDPUtil::changeSS(ss, SS_NEGOTIATE_1, __FUNCTION__);

    postNegotiateResp();

    // report accumulated dirty region and pixels immediately once session established
    if(MRDPUtil::checkSS(ss, SS_ESTABLISHED, __FUNCTION__)) { 
        // notify screen update (full screen)
        postScreenUpdatedEvt(connMgr->compositor(), true, true);
    }
}

// remote client ask to close 
void MRDPClientStub::handleCloseReq(const ::mrdp::CloseReq &req) 
{
    Q_UNUSED(req);

    MRDPUtil::changeSS(ss, SS_CLOSING, __FUNCTION__);

    postCloseResp();
    
    if(connMgr) {
        connMgr->cleanupConnection(this);  // destruct me !!!
    }
}

void MRDPClientStub::handleMotionActionReq(const ::mrdp::MotionActionReq &req) 
{
    // redirect to InputMgr
    vInputSendMotionEvent(connMgr->input(), req.x(), req.y());
}

void MRDPClientStub::handleButtonActionReq(const ::mrdp::ButtonActionReq &req)
{
    // redirect to InputMgr
    vInputSendButtonEvent(connMgr->input(), req.button(), req.pressed());
}

void MRDPClientStub::handleKeyActionReq(const ::mrdp::KeyActionReq &req) 
{
    // redirect to InputMgr
    vInputSendKeyEvent(connMgr->input(), req.key(), req.down());
}

//[mrdp thread]
void MRDPClientStub::handleScreenUpdateReq(const ::mrdp::ScreenUpdateReq &req) 
{
    // discard the requested rect, at present ...

    // report the real dirty region and pixels
    postScreenUpdatedEvt(connMgr->compositor(), false, false);
}

void MRDPClientStub::handleScreenUpdateReportModeReq(const ::mrdp::ScreenUpdateReportModeReq &req)
{
    surm = req.surm();
    Q_ASSERT(surm == ::mrdp::SURM_ACTIVE || surm == ::mrdp::SURM_PASSIVE);
}

QImage::Format MRDPClientStub::remoteQFormat()
{
    qWarning("remoteQFormat should NOT be called on server side!!!");
    return MRDPTransceiver::remoteQFormat();
}

uint MRDPClientStub::clientID()
{
    return id;
}

uint MRDPClientStub::screenDepth()
{
    return connMgr->screenDepth();
}

void MRDPClientStub::handleSockError(QAbstractSocket::SocketError err)
{
    MRDPTransceiver::handleSockError(err);
    MRDPUtil::changeSS(ss, SS_ERROR, __FUNCTION__);
    if(connMgr) {
        connMgr->cleanupConnection(this);  // destruct me !!!
    }
}

void MRDPClientStub::handleParseError()
{
    MRDPUtil::changeSS(ss, SS_ERROR, __FUNCTION__);
    if(connMgr) {
        connMgr->cleanupConnection(this);  // destruct me !!!
    }
}

////////////////////////////////////// mrdp server proxy ///////////////////////////////
MRDPServerProxy::MRDPServerProxy(QTcpSocket *ioDev) : MRDPTransceiver(ioDev),
    remoteQFormat_(QImage::Format_ARGB32_Premultiplied),
    negoRespHandler(&reqParser),
    closeRespHandler(&reqParser),
    negoEvtHandler(&reqParser),
    suEvtHandler(&reqParser),
    srEvtHandler(&reqParser),
    ssEvtHandler(&reqParser)
{
    sock = ioDev;
}

MRDPServerProxy::~MRDPServerProxy()
{

}

void MRDPServerProxy::handleNegotiateResp(const ::mrdp::NegotiateResp &resp)
{
    // resp.serverVersion();            // version check not used by now
    remoteQFormat_ = MRDPUtil::MRDPFormat2QFormat(resp.format());
    // resp.pixelInBigEndian();         // big endian not used by now
    QRect qRc = MRDPUtil::MRDPRect2QRect(resp.screenrect());
    emit negotiationFinished(remoteQFormat_, qRc);
}

void MRDPServerProxy::handleCloseResp(const ::mrdp::CloseResp &resp)
{
    emit closeConfirmed();
}

void MRDPServerProxy::handleNegotiateEvt(const ::mrdp::NegotiateEvt &evt, uint clientid)
{
    // check:
    //evt.serverVersion();
    //evt.compressPixelSuggestion();
    clientID_ = clientid;
    postNegotiateReq();
}

void MRDPServerProxy::handleScreenUpdatedEvt(const ::mrdp::ScreenUpdatedEvt &evt)
{
    if(evt.dirtyregion_size() <= 0 || !evt.has_dirtypixels()) 
        return;

    const ::std::string& compressedBuf = evt.dirtypixels();

////////////////////////////////////////////////////
    {
#if defined(MRDP_DEBUG_TRACE_PIXELS) || defined(MRDP_DEBUG_DUMP_PIXELS)
        const char*dbg_caddr = compressedBuf.data();
        size_t dbg_csize = compressedBuf.size();
        quint16 crc = 0;
        qDebug("%s(before decompress): crc(0x%x), caddr(%p), csize(%d), start4(0x%x,0x%x,0x%x,0x%x), end4(0x%x,0x%x,0x%x,0x%x)", 
            __FUNCTION__, crc = qChecksum(dbg_caddr, dbg_csize), dbg_caddr, dbg_csize, dbg_caddr[0], dbg_caddr[1], dbg_caddr[2], dbg_caddr[3],
            dbg_caddr[dbg_csize-4], dbg_caddr[dbg_csize-3], dbg_caddr[dbg_csize-2], dbg_caddr[dbg_csize-1]);
#endif

#if defined(MRDP_DEBUG_DUMP_PIXELS)
        QString cdf_name;
        cdf_name.sprintf("/tmp/cdf_%x", crc);
        QFile cdf(cdf_name);
        cdf.open(QIODevice::ReadWrite | QIODevice::Truncate);
        cdf.write(dbg_caddr, dbg_csize);
        cdf.flush();
        cdf.close();
        QThread::msleep(100);
#endif

    decompress(compressedBuf);   

#if defined(MRDP_DEBUG_TRACE_PIXELS) || defined(MRDP_DEBUG_DUMP_PIXELS)
        const uchar *dbg_daddr = decompressedBuf.data();
        size_t dbg_dsize = decompressedBufOffset;
        qDebug("%s(after decompress): crc(0x%x), daddr(%p), dsize(%d), start4(0x%x,0x%x,0x%x,0x%x), end4(0x%x,0x%x,0x%x,0x%x)", 
            __FUNCTION__, qChecksum(reinterpret_cast<const char*>(dbg_daddr), dbg_dsize), dbg_daddr, dbg_dsize, dbg_daddr[0], dbg_daddr[1], dbg_daddr[2], dbg_daddr[3],
            dbg_daddr[dbg_dsize-4], dbg_daddr[dbg_dsize-3], dbg_daddr[dbg_dsize-2], dbg_daddr[dbg_dsize-1]);
#endif
    }
////////////////////////////////////////////////////

    screenDirty = QRegion();
    unpackedOffset = 0;
    for(int i = 0; i < evt.dirtyregion_size(); i++) {
        const ::mrdp::Rect &rc = evt.dirtyregion(i);
        QRect qrc(rc.x(), rc.y(), rc.w(), rc.h());                      // dirty on screen
        screenDirty += qrc;
        unpackedOffset += unpack(unpackedOffset, qrc, screenImage_);      // unpack from decompressedBuf to screenImage, qrc by qrc
#ifdef MRDP_DEBUG_TRACE_REGIONS
        qDebug("%s dirty rect(%d-%d): %d, %d, %d, %d", __FUNCTION__,
            i, evt.dirtyregion_size(), qrc.x(), qrc.y(), qrc.width(), qrc.height());
#endif
    }

    // notification
    emit screenUpdated(screenDirty);
}

void MRDPServerProxy::handleScreenResizedEvt(const ::mrdp::ScreenResizedEvt &evt)
{
    const ::mrdp::Rect &rc = evt.screenrect();
    QSize sz(rc.w(), rc.h());
    emit screenResized(sz);
}

void MRDPServerProxy::handleServerShutdownEvt(const ::mrdp::ServerShutdownEvt &evt)
{
    emit serverShutdown();
}

void MRDPServerProxy::postNegotiateReq()
{
    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::REQUEST_NEGOTIATION);

    // Request
    ::mrdp::Request *req = buildReqHeader(*msg, clientID_);

    // NegotiateReq
    ::mrdp::NegotiateReq *negoReq = req->mutable_negoreq();
    negoReq->set_clientversion(::mrdp::PROTOVER_CURRENT);
    negoReq->set_compresspixels(::mrdp::SW_ON);              // server prefer to compress pixels
    negoReq->set_surm(::mrdp::SURM_ACTIVE);                  // server prefer to report screen updated event actively

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);   
}

void MRDPServerProxy::postCloseReq()
{
    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::REQUEST_CLOSE);

    // Request
    ::mrdp::Request *req = buildReqHeader(*msg, clientID_);

    // no conent

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);
}

// called by ui, according to setting(immediate or periodic)
void MRDPServerProxy::postScreenUpdateReq(::mrdp::ScreenUpdateMode sum)
{
    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::REQUEST_SCREENUPDATE);

    // Request
    ::mrdp::Request *req = buildReqHeader(*msg, clientID_);

    // ScreenUpdateReq
    ::mrdp::ScreenUpdateReq *suReq = req->mutable_sureq();
    suReq->set_sum(sum);

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);
}

// caller must translate native mouse/key message into MRDP format
void MRDPServerProxy::postMotionActionReq(int x, int y)
{
    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::REQUEST_MOTIONACTION);

    // Request
    ::mrdp::Request *req = buildReqHeader(*msg, clientID_);

    // MotionActionReq
    ::mrdp::MotionActionReq *maReq = req->mutable_mareq();
    maReq->set_x(x);
    maReq->set_y(y);

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);
}

void MRDPServerProxy::postButtonActionReq(uint button, ::mrdp::ButtonActionPress state)
{
    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::REQUEST_BUTTONACTION);

    // Request
    ::mrdp::Request *req = buildReqHeader(*msg, clientID_);

    // ButtonActionReq
    ::mrdp::ButtonActionReq *baReq = req->mutable_bareq();
    baReq->set_button(button);
    baReq->set_pressed(state);

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);
}

void MRDPServerProxy::postKeyActionReq(uint key, ::mrdp::KeyActionDown state)
{
    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::REQUEST_KEYACTION);

    // Request
    ::mrdp::Request *req = buildReqHeader(*msg, clientID_);

    // ButtonActionReq
    ::mrdp::KeyActionReq *kaReq = req->mutable_kareq();
    kaReq->set_key(key);
    kaReq->set_down(state);

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);
}

void MRDPServerProxy::postScreenUpdateReportModeReq(::mrdp::ScreenUpdateReportMode surm)
{
    // Message
    ::mrdp::Message *msg = getMsgToSendFromPool(::mrdp::REQUEST_SCREENUPDATEREPORTMODE);

    // Request
    ::mrdp::Request *req = buildReqHeader(*msg, clientID_);

    // ScreenUpdateReq
    ::mrdp::ScreenUpdateReportModeReq *surmReq = req->mutable_surmreq();
    surmReq->set_surm(surm);

    // Extra Header
    ExtraHeader hdr;
    buildExtraHeader(hdr, msg->ByteSize());
    sendExtraHeader(hdr);

    // message is on the way
    sendMsgBody(msg);
}

QImage::Format MRDPServerProxy::remoteQFormat()
{
    return remoteQFormat_;
}

void MRDPServerProxy::setScreenImage(const QImage &img)
{
    screenDirty = QRegion();
    screenImage_ = img;

    uint decompressedBufMaxLen = img.byteCount();
    decompressedBufCapacity = IN_LEN(decompressedBufMaxLen);
    // build decompressed buffer
    uchar *decompressedBits = new uchar[decompressedBufCapacity];
    decompressedBuf.reset(decompressedBits);        // oldD will be cleaned up automatically
    memset(decompressedBits, 0, IN_LEN(decompressedBufCapacity));
}

uint MRDPServerProxy::clientID()
{
    return clientID_;
}

uint MRDPServerProxy::screenDepth()
{
    return screenImage_.depth();
}

////////////////////////////////////// mrdp session ////////////////////////////////////
MRDPConnMgr* MRDPConnMgr::self;
QThread MRDPConnMgr::mrdpThread;

MRDPConnMgr& MRDPConnMgr::instance()
{
    if(!self) {
        self = new MRDPConnMgr();
        self->moveToThread(&mrdpThread);
    }

    return *MRDPConnMgr::self;
}

/*!
    [calling thread]
*/    
bool MRDPConnMgr::startService(QPlatformCompositor *compstor)
{
    if(serviceRunning_) {
        qWarning("service already run: %s", __FUNCTION__);
        return true;
    }

    if(!mrdpThread.isRunning()) {
        mrdpThread.start();
    }

    bool ret = false;
    emit startService__(compstor, &ret);       // blocked here
    return ret;
}

/*!
    [mpdp thread]
*/
void MRDPConnMgr::doStartService(QPlatformCompositor *compstor, bool *ret)
{
    initCompositor(compstor);
    initInput();

    if(!listener.listen(QHostAddress::Any, MRDP_PORT)) {
        qWarning("listen failed(%d,%s): %s", 
            listener.serverError(), listener.errorString().toUtf8().data(), __FUNCTION__);
        *ret = false;
        return;
    }

    connect(&listener, SIGNAL(newConnection()), this, SLOT(acceptConnection()));
    // for active screen update, blocked until pixels compression finished
    // for passive screen update, blocked until region merging finished(pixel fetch needs lock again)
    connect(this, SIGNAL(screenUpdated(QRegion)), this, SLOT(doNotifyScreenUpdated(QRegion)), Qt::QueuedConnection);     
    serviceRunning_ = true;
    *ret = true;  

#ifdef MRDP_DEBUG_TRACE_STATE
    qDebug("MRDPConnMgr(%s) service started", __FUNCTION__);
#endif        
}

/*!
    [calling thread]
*/
void MRDPConnMgr::stopService()
{
    emit stopService__();
    mrdpThread.exit(0);
    mrdpThread.wait();      // until mrdp thread finished
}

/*!
    [mrdp thread]
*/
void MRDPConnMgr::doStopService()
{
    disconnect(&listener, SIGNAL(newConnection()), this, SLOT(acceptConnection()));
    disconnect(this, SIGNAL(screenUpdated(QRegion)), this, SLOT(doNotifyScreenUpdated(QRegion)));

    listener.close();

    // CHECK: all clientStub will be destructed automatically
    clientStubs.clear();
    
    deInitInput();
    deInitCompositor(); 

    serviceRunning_ = false;
#ifdef MRDP_DEBUG_TRACE_STATE
    qDebug("MRDPConnMgr(%s) service stopped", __FUNCTION__);
#endif     
}

bool MRDPConnMgr::serviceRunning()
{
    return serviceRunning_;
}

void MRDPConnMgr::initCompositor(QPlatformCompositor *compstor)
{
    compositor_ = compstor;
}

void MRDPConnMgr::deInitCompositor()
{
    compositor_ = NULL;
}

/*!
    Frontend connect to InputMgr twice by AF_UNIX, one for normal input and the other for vinput, but with the same fd, 
    if we close it (in deInitInput), the normal input can not work neither, so we hold it permernently.
*/
void MRDPConnMgr::initInput()
{
    if(-1 == vinputFD) {
        vinputFD = vInputConnect();
    }
}

void MRDPConnMgr::deInitInput()
{
    // vInputClose(vinputFD);
}

QPlatformCompositor &MRDPConnMgr::compositor()
{
    return *compositor_;
}

int MRDPConnMgr::input() 
{
    return vinputFD;
}

/*!
    sdr will be copied into MRDPClient thread's event queue
*/
void MRDPConnMgr::tryNotifyScreenUpdated(const QRegion &sdr)
{
    emit screenUpdated(sdr);       // pull the trigger
}

//[mrdp thread]
void MRDPConnMgr::doNotifyScreenUpdated(QRegion sdr)
{
    foreach(QSharedPointer<MRDPClientStub> clientStub, clientStubs) {
        clientStub->postScreenUpdatedEvtChecked(*compositor_, sdr);
    }
}

/*!
    [compositor thread]
*/
MRDPConnMgr::MRDPConnMgr(const QRect &scrnGeom, int depth) : 
    screenDepth_(depth), screenGeometry_(scrnGeom), compositor_(NULL), 
    listener(this), vinputFD(-1), serviceRunning_(false)
{
    if(::mrdp::ERR_SUCCESS != initminilzo()) {
        qFatal("MRDPConnMgr init failed!");
    }
    connect(this, SIGNAL(startService__(QPlatformCompositor*, bool*)), 
        this, SLOT(doStartService(QPlatformCompositor*, bool*)));      // transfer thread
    connect(this, SIGNAL(stopService__()), 
        this, SLOT(doStopService()), Qt::BlockingQueuedConnection);
}

/*!
    [mrdp thread]
*/
MRDPConnMgr::~MRDPConnMgr() 
{
    delete self;
    self = NULL;
    ::google::protobuf::ShutdownProtobufLibrary();
}

// watchout: "const Client&" as parameter is wrong
void MRDPConnMgr::cleanupConnection(MRDPClientStub* client)
{
    QList< QSharedPointer<MRDPClientStub> >::const_iterator it = clientStubs.begin();
    while(it != clientStubs.end()) {
        const QSharedPointer<MRDPClientStub> &clientStub = *it;
        if(clientStub.data() == client) {               // once found, remove all matched
            clientStubs.removeAll(clientStub);
            return;
        }
        it++;
    }
}

void MRDPConnMgr::setScreenGeometry(const QRect &newGeometry)
{
    screenGeometry_ = newGeometry;

    // notify all client stubs
    foreach(QSharedPointer<MRDPClientStub> clientStub, clientStubs) {
        clientStub->postScreenResizedEvt();   
    }
}

QRect MRDPConnMgr::screenGeometry()
{
    return screenGeometry_;
}

void MRDPConnMgr:: setScreenDepth(int depth)
{
    screenDepth_ = depth;
}

int MRDPConnMgr::screenDepth()
{
    return screenDepth_;
}

void MRDPConnMgr::acceptConnection() 
{
    // conn has been created by QTcpServer::incomingConnection, now take it
    QTcpSocket *sock = listener.nextPendingConnection(); 
    if(!sock) {     // no connection present
        return;
    }
    sock->setReadBufferSize(MRDP_MAX_RECV_BUFFER_SIZE);
    
    // change to unbuffered mode, since protobuf will take charge
    // qintptr desc = sock->socketDescriptor();
    // sock->setSocketDescriptor(desc, sock->state(), QIODevice::Unbuffered);

    // append sock to clientStub, then add clientStub to clientStubs
    QSharedPointer<MRDPClientStub> clientStub(new MRDPClientStub(this, clientIDGenerator.fetchAndAddRelaxed(1), sock));
    clientStubs.append(clientStub);

    // after return, ref counter of client is 1, see QSharedPointer's dtor
}

::mrdp::ErrorNo MRDPConnMgr::initminilzo()
{
    if (lzo_init() != LZO_E_OK)
    {
        qWarning("lzo_init() failed !!!");
        return ::mrdp::ERR_LZO;
    }   
    return ::mrdp::ERR_SUCCESS;
}

static struct MRDP_QIMAGE_FORMAT_MAP {
    ::mrdp::Format     mrdpFormat;
    QImage::Format     qFormat;
} mrdp_qimage_fomats[] = {
    {::mrdp::FMT_ARGB32_PREMUL, QImage::Format_ARGB32_Premultiplied},
    {::mrdp::FMT_ARGB32,        QImage::Format_ARGB32},
    {::mrdp::FMT_RGB32,         QImage::Format_RGBX8888},
    {::mrdp::FMT_xBGR32,        QImage::Format_Invalid},
    {::mrdp::FMT_RGBA32,        QImage::Format_RGBA8888_Premultiplied},
    {::mrdp::FMT_BGRA32_PREMUL, QImage::Format_Invalid},
    {::mrdp::FMT_UYVY,          QImage::Format_Invalid},
};

QImage::Format MRDPUtil::MRDPFormat2QFormat(::mrdp::Format mrdpFormat)
{
    for(uint i = 0; i < sizeof(mrdp_qimage_fomats)/sizeof(mrdp_qimage_fomats[0]); i++) {
        if(mrdp_qimage_fomats[i].mrdpFormat == mrdpFormat)
            return mrdp_qimage_fomats[i].qFormat;
    }

    return QImage::Format_Invalid;
}

QRect MRDPUtil::MRDPRect2QRect(const ::mrdp::Rect &mrdpRc)
{
    QRect qRc(mrdpRc.x(), mrdpRc.y(), mrdpRc.w(), mrdpRc.h());
    return qRc;
}

inline bool MRDPUtil::checkSS(SessionState ssCurrent, uint ssExpected, const char* location)
{
    if(ssCurrent  != (ssCurrent & ssExpected)) {
        qWarning("ss wrong(current: %d, expected: %d): %s", ssCurrent, ssExpected, location);
        return false;
    }
    return true;
}

inline void MRDPUtil::changeSS(SessionState &ssCurrent, SessionState ssExpected, const char* location)
{
#ifdef MRDP_DEBUG_TRACE_STATE
    qDebug("changeSS(old(%d)->new(%d))", ssCurrent, ssExpected);
#endif
    ssCurrent = ssExpected;
}


// TODO:  both reading and writing can be async scheduled, but now only reading is 

