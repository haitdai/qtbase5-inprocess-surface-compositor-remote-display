DEFINES += QT_MRDP

#in includer's path
include($$QT_SOURCE_TREE/src/3rdparty/minilzo.pri)

#inputmgr(note: independent from mrofs_inputmgr configuration option)
INCLUDEPATH += $$QT_SOURCE_TREE/../inputmgr/include
contains(QT_CONFIG, xcb) {
	LIBS += -L$$QT_SOURCE_TREE/../inputmgr/lib/x86linux  
	LIBS += -linputmgr	
} else:contains(QT_CONFIG, mrofs_pvr2d) {
	LIBS += -L$$QT_SOURCE_TREE/../inputmgr/lib/armlinux  
	LIBS += -linputmgr	
}

INCLUDEPATH += $$PWD
HEADERS += \
    $$PWD/mrdp.pb.h  \
    $$PWD/mrdp_p.h    
SOURCES += \
    $$PWD/mrdp.pb.cc  \
    $$PWD/mrdp.cpp
        