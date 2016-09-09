TARGET  = qico

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QICOPlugin
load(qt_plugin)

QTDIR_build:REQUIRES = "!contains(QT_CONFIG, no-ico)"

HEADERS += qicohandler.h main.h
SOURCES += main.cpp \
           qicohandler.cpp
OTHER_FILES += ico.json

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