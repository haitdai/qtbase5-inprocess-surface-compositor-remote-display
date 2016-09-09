TARGET = qnlabearer

PLUGIN_TYPE = bearer
PLUGIN_CLASS_NAME = QNlaEnginePlugin
load(qt_plugin)

QT = core core-private network network-private

!wince* {
    LIBS += -lws2_32
} else {
    LIBS += -lws2
}

HEADERS += qnlaengine.h \
           ../platformdefs_win.h \
           ../qnetworksession_impl.h \
           ../qbearerengine_impl.h

SOURCES += main.cpp \
           qnlaengine.cpp \
           ../qnetworksession_impl.cpp

OTHER_FILES += nla.json

win32 {
    # for MMOS new delete override
    contains(CONFIG, debug){
        DEFINES += MEM_DEBUG
    }
    
    # for MMOS new delete override
    contains(CONFIG, debug){
        DEFINES += MEM_DEBUG
    }
    
    INCLUDEPATH += .\
            $$PWD/../../../../../next_mem/Include \
            $$PWD/../../../../../next_newdelete
    HEADERS  += \
        ../../../../../next_newdelete/NewOperatorEx.h 
    SOURCES += \
        ../../../../../next_newdelete/NewOperatorEx.cpp
        
    LIBS += -L$$PWD/../../../../../next_mem/Libs
    LIBS += -lVC_MemEx
}