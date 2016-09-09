TARGET = qnativewifibearer

PLUGIN_TYPE = bearer
PLUGIN_CLASS_NAME = QNativeWifiEnginePlugin
load(qt_plugin)

QT = core-private network-private

HEADERS += qnativewifiengine.h \
           platformdefs.h \
           ../qnetworksession_impl.h \
           ../qbearerengine_impl.h

SOURCES += main.cpp \
           qnativewifiengine.cpp \
           ../qnetworksession_impl.cpp

OTHER_FILES += nativewifi.json

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