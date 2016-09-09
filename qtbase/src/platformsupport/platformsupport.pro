TARGET     = QtPlatformSupport
QT         = core-private gui-private network

CONFIG += static internal_module
mac:LIBS_PRIVATE += -lz

DEFINES += QT_NO_CAST_FROM_ASCII
PRECOMPILED_HEADER = ../corelib/global/qt_pch.h

include(cfsocketnotifier/cfsocketnotifier.pri)
include(cglconvenience/cglconvenience.pri)
include(eglconvenience/eglconvenience.pri)
include(eventdispatchers/eventdispatchers.pri)
include(fbconvenience/fbconvenience.pri)
include(fontdatabases/fontdatabases.pri)
include(glxconvenience/glxconvenience.pri)
include(input/input.pri)
include(devicediscovery/devicediscovery.pri)
include(services/services.pri)
include(themes/themes.pri)
include(linuxaccessibility/linuxaccessibility.pri)
include(compositor/compositor.pri)
contains(QT_CONFIG, mrdp) {
    include(mrdp/mrdp.pri)
}	

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

load(qt_module)