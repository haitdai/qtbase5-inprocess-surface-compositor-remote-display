TARGET = qxcb

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QXcbIntegrationPlugin
load(qt_plugin)

QT += core-private gui-private platformsupport-private 

SOURCES = \
        qxcbclipboard.cpp \
        qxcbconnection.cpp \
        qxcbintegration.cpp \
        qxcbkeyboard.cpp \
        qxcbmime.cpp \
        qxcbdrag.cpp \
        qxcbscreen.cpp \
        qxcbwindow.cpp \
        qxcbbackingstore.cpp \
        qxcbwmsupport.cpp \
        main.cpp \
        qxcbnativeinterface.cpp \
        qxcbcursor.cpp \
        qxcbimage.cpp \
        qxcbxsettings.cpp \
        qxcbsystemtraytracker.cpp

HEADERS = \
        qxcbclipboard.h \
        qxcbconnection.h \
        qxcbintegration.h \
        qxcbkeyboard.h \
        qxcbdrag.h \
        qxcbmime.h \
        qxcbobject.h \
        qxcbscreen.h \
        qxcbwindow.h \
        qxcbbackingstore.h \
        qxcbwmsupport.h \
        qxcbnativeinterface.h \
        qxcbcursor.h \
        qxcbimage.h \
        qxcbxsettings.h \
        qxcbsystemtraytracker.h  \
        qxcbstd.h

LIBS += -ldl

# needed by GLX, Xcursor ...
contains(QT_CONFIG, xcb-xlib) {
    DEFINES += XCB_USE_XLIB
    LIBS += -lX11 -lX11-xcb

    *-maemo* {
        contains(QT_CONFIG, xinput2) {
            # XInput2 support for Harmattan.
            DEFINES += XCB_USE_XINPUT2_MAEMO
            SOURCES += qxcbconnection_maemo.cpp
            LIBS += -lXi
        }
        DEFINES += XCB_USE_MAEMO_WINDOW_PROPERTIES
    } else {
    #DHT we must define xinput2 if we want xinput2 to be in QT_CONFIG
        contains(QT_CONFIG, xinput2) {
            DEFINES += XCB_USE_XINPUT2
            SOURCES += qxcbconnection_xi2.cpp
            LIBS += -lXi
        }
    }
}

#DHT if we defined xcb we got xcb-render automatically
# to support custom cursors with depth > 1
contains(QT_CONFIG, xcb-render) {
    DEFINES += XCB_USE_RENDER
    LIBS += -lxcb-render -lxcb-render-util -lXrender
}

contains(QT_CONFIG, xcb-xv) {
    DEFINES += XCB_USE_XV
    LIBS += -lxcb-xv -lXv
}

#DHT we must define sm manually if we want xcb-sm to be in QT_CONFIG
# build with session management support
contains(QT_CONFIG, xcb-sm) {
    DEFINES += XCB_USE_SM
    LIBS += -lSM -lICE
    SOURCES += qxcbsessionmanager.cpp
    HEADERS += qxcbsessionmanager.h
}

contains(QT_CONFIG, opengl) {
# DHT we must define xcb-xlib manually if we want xcb-xlib to be in QT_CONFIG
    contains(QT_CONFIG, xcb-xlib):!contains(QT_CONFIG, opengles2) {
        DEFINES += XCB_USE_GLX
        HEADERS += qglxintegration.h
        SOURCES += qglxintegration.cpp
        LIBS += $$QMAKE_LIBS_DYNLOAD
        contains(QT_CONFIG, xcb-glx) {
            DEFINES += XCB_HAS_XCB_GLX
            LIBS += -lxcb-glx
        }
    } else:contains(QT_CONFIG, egl) {
    ##DHT,  egl+ogles2 under XCB, mesa support EGL under X11, so qt can take advantage of it
        DEFINES += XCB_USE_EGL
        CONFIG += egl
        HEADERS += qxcbeglsurface.h

        # EGL on MeeGo 1.2 Harmattan needs this macro to map EGLNativeDisplayType
        # and other types to the correct X11 types
        DEFINES += SUPPORT_X11
    }
}

DEFINES += $$QMAKE_DEFINES_XCB
LIBS += $$QMAKE_LIBS_XCB
QMAKE_CXXFLAGS += $$QMAKE_CFLAGS_XCB

CONFIG += qpa/genericunixfontdatabase

contains(QT_CONFIG, dbus) {
DEFINES += XCB_USE_IBUS
QT += dbus
LIBS += -ldbus-1
}

OTHER_FILES += xcb.json README

contains(QT_CONFIG, xcb-qt) {
    DEFINES += XCB_USE_RENDER
    XCB_DIR = ../../../3rdparty/xcb
    INCLUDEPATH += $$XCB_DIR/include $$XCB_DIR/sysinclude
    LIBS += -lxcb -L$$OUT_PWD/xcb-static -lxcb-static
} else {
    LIBS += -lxcb -lxcb-image -lxcb-icccm -lxcb-sync -lxcb-xfixes -lxcb-shm -lxcb-randr
    !contains(DEFINES, QT_NO_SHAPE):LIBS += -lxcb-shape
    contains(DEFINES, QT_NO_XKB) {
        LIBS += -lxcb-keysyms
    } else {
        LIBS += -lxcb-xkb
    }
}

# libxkbcommon
contains(QT_CONFIG, xkbcommon-qt): {
    include(../../../3rdparty/xkbcommon.pri)
} else {
    LIBS += $$QMAKE_LIBS_XKBCOMMON
    QMAKE_CXXFLAGS += $$QMAKE_CFLAGS_XKBCOMMON
    equals(QMAKE_VERSION_XKBCOMMON, "0.2.0") {
        DEFINES += XKBCOMMON_0_2_0
        INCLUDEPATH += ../../../3rdparty/xkbcommon/xkbcommon/
    }
}

contains(QT_CONFIG, mrofs) {
    include(mrofs/mrofs.pri)	
    DEFINES += QT_MROFS	
    QT += widgets-private
    contains(QT_CONFIG, mrofs_inputmgr) {
        DEFINES+=QT_MROFS_INPUTMGR
    }
}


