TEMPLATE = subdirs

android:!android-no-sdk: SUBDIRS += android

SUBDIRS += minimal

!win32|contains(QT_CONFIG, freetype):SUBDIRS += offscreen

contains(QT_CONFIG, xcb) {
    SUBDIRS += xcb
}

mac {
    ios: SUBDIRS += ios
    else: SUBDIRS += cocoa
}

win32: SUBDIRS += windows

qnx {
    SUBDIRS += qnx
}

contains(QT_CONFIG, eglfs) {
    SUBDIRS += eglfs
    SUBDIRS += minimalegl
}

contains(QT_CONFIG, directfb) {
    SUBDIRS += directfb
}

contains(QT_CONFIG, kms) {
    SUBDIRS += kms
}

contains(QT_CONFIG, mrofs_legacy_glesv2) {
	SUBDIRS += mrofs_legacy_glesv2
}

contains(QT_CONFIG, mrofs_legacy_pvr2d) {
	SUBDIRS += mrofs_legacy_pvr2d
}

contains(QT_CONFIG, mrofs_pvr2d) {
	SUBDIRS += mrofs_pvr2d
}

contains(QT_CONFIG, linuxfb): SUBDIRS += linuxfb

win32 {
    # for MMOS new delete override
    contains(CONFIG, debug){
        DEFINES += MEM_DEBUG
    }
    
    INCLUDEPATH += .\
            $$PWD/../../../../next_mem/Include \
            $$PWD/../../../../next_newdelete
    HEADERS  += \
        ../../../../next_newdelete/NewOperatorEx.h 
    SOURCES += \
        ../../../../next_newdelete/NewOperatorEx.cpp
    LIBS += -L$$PWD/../../../../next_mem/Libs
    LIBS += -lVC_MemEx
}

