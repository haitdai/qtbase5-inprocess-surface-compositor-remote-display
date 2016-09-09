TARGET     = QtXml
QT         = core-private

DEFINES   += QT_NO_USING_NAMESPACE
win32-msvc*|win32-icc:QMAKE_LFLAGS += /BASE:0x61000000

QMAKE_DOCS = $$PWD/doc/qtxml.qdocconf

load(qt_module)

HEADERS += qtxmlglobal.h

PRECOMPILED_HEADER = ../corelib/global/qt_pch.h

include(dom/dom.pri)
include(sax/sax.pri)


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

