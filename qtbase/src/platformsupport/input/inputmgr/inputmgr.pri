# only xcb(bigdipper) and mrofs_pvr2d(merak) needs inputmgr
contains(QT_CONFIG, xcb) {
	LIBS += -L$$PWD/../../../../../inputmgr/lib/x86linux  
	DEFINES += INPUT_MGR_X86
} else:contains(QT_CONFIG, mrofs_pvr2d) {
	LIBS += -L$$PWD/../../../../../inputmgr/lib/armlinux  
	DEFINES += INPUT_MGR_ARM
}
LIBS += -linputmgr

INCLUDEPATH += $$PWD/../../../../../inputmgr/include

HEADERS += \
    $$PWD/qinputmgrmanager_p.h \
	$$PWD/qinputmgrmouseevent_p.h \
	$$PWD/qinputmgrkeyboardevent_p.h \
	$$PWD/qinputmgrkeyboard_defaultmap_p.h

SOURCES += \
    $$PWD/qinputmgrmanager.cpp \
	$$PWD/qinputmgrkeyboardevent.cpp \
	$$PWD/qinputmgrmouseevent.cpp
	
contains(QT_CONFIG, libudev) {
    LIBS_PRIVATE += $$QMAKE_LIBS_LIBUDEV
}

