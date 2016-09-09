TARGET = qoffscreen

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QOffscreenIntegrationPlugin
load(qt_plugin)

QT += core-private gui-private platformsupport-private

SOURCES =   main.cpp \
            qoffscreenintegration.cpp \
            qoffscreenwindow.cpp \
            qoffscreencommon.cpp

HEADERS =   qoffscreenintegration.h \
            qoffscreenwindow.h \
            qoffscreencommon.h

OTHER_FILES += offscreen.json

contains(QT_CONFIG, xlib):contains(QT_CONFIG, opengl):!contains(QT_CONFIG, opengles2) {
    SOURCES += qoffscreenintegration_x11.cpp
    HEADERS += qoffscreenintegration_x11.h
    system(echo "Using X11 offscreen integration with GLX")
} else {
    SOURCES += qoffscreenintegration_dummy.cpp
}


win32 {
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

