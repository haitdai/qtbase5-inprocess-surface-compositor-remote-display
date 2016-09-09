TARGET = qbbbearer

PLUGIN_TYPE = bearer
PLUGIN_CLASS_NAME = QBBEnginePlugin
load(qt_plugin)

QT = core-private network-private

# Uncomment this to enable debugging output for the plugin
#DEFINES += QBBENGINE_DEBUG

HEADERS += qbbengine.h \
           ../qnetworksession_impl.h \
           ../qbearerengine_impl.h

SOURCES += qbbengine.cpp \
           ../qnetworksession_impl.cpp \
           main.cpp

OTHER_FILES += blackberry.json

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