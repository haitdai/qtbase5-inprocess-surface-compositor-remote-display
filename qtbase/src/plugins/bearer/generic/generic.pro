TARGET = qgenericbearer

PLUGIN_TYPE = bearer
PLUGIN_CLASS_NAME = QGenericEnginePlugin
load(qt_plugin)

QT = core-private network-private

HEADERS += qgenericengine.h \
           ../qnetworksession_impl.h \
           ../qbearerengine_impl.h \
           ../platformdefs_win.h
SOURCES += qgenericengine.cpp \
           ../qnetworksession_impl.cpp \
           main.cpp

OTHER_FILES += generic.json

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