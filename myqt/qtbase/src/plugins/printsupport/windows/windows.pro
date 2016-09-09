plaTARGET = windowsprintersupport
MODULE = windowsprintersupport
PLUGIN_TYPE = printsupport
PLUGIN_CLASS_NAME = QWindowsPrinterSupportPlugin
load(qt_plugin)

QT *= core-private
QT *= gui-private
QT *= printsupport-private

INCLUDEPATH *= $$QT_SOURCE_TREE/src/printsupport/kernel

SOURCES += \
    main.cpp \
    qwindowsprintersupport.cpp

HEADERS += \
    qwindowsprintersupport.h

OTHER_FILES += windows.json

LIBS += -lwinspool -lcomdlg32

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