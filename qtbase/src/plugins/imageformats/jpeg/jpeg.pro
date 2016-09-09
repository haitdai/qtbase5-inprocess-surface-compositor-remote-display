TARGET  = qjpeg

PLUGIN_TYPE = imageformats
PLUGIN_CLASS_NAME = QJpegPlugin
load(qt_plugin)

QT += core-private

QTDIR_build:REQUIRES = "!contains(QT_CONFIG, no-jpeg)"

include(../../../gui/image/qjpeghandler.pri)
SOURCES += main.cpp
HEADERS += main.h
OTHER_FILES += jpeg.json

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