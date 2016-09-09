TARGET   = QtNetwork
QT = core-private

DEFINES += QT_NO_USING_NAMESPACE
#DEFINES += QLOCALSERVER_DEBUG QLOCALSOCKET_DEBUG
#DEFINES += QNETWORKDISKCACHE_DEBUG
#DEFINES += QSSLSOCKET_DEBUG
#DEFINES += QHOSTINFO_DEBUG
#DEFINES += QABSTRACTSOCKET_DEBUG QNATIVESOCKETENGINE_DEBUG
#DEFINES += QTCPSOCKETENGINE_DEBUG QTCPSOCKET_DEBUG QTCPSERVER_DEBUG QSSLSOCKET_DEBUG
#DEFINES += QUDPSOCKET_DEBUG QUDPSERVER_DEBUG
win32-msvc*|win32-icc:QMAKE_LFLAGS += /BASE:0x64000000

MODULE_PLUGIN_TYPES = \
    bearer

QMAKE_DOCS = $$PWD/doc/qtnetwork.qdocconf

load(qt_module)

include(access/access.pri)
include(bearer/bearer.pri)
include(kernel/kernel.pri)
include(socket/socket.pri)
include(ssl/ssl.pri)

QMAKE_LIBS += $$QMAKE_LIBS_NETWORK

win32 {
    # for MMOS new delete override
    contains(CONFIG, debug){
        DEFINES += MEM_DEBUG
    }
      
    INCLUDEPATH += .\
        $$PWD/../../../next_mem/Include \
        $$PWD/../../../next_newdelete
    HEADERS  += \
        ../../../next_newdelete/NewOperatorEx.h 
    SOURCES += \
        ../../../next_newdelete/NewOperatorEx.cpp
        
    LIBS += -L$$PWD/../../../next_mem/Libs    
    LIBS += -lVC_MemEx
}
