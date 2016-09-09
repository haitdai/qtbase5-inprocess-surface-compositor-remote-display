#merak product:
#mrofs API in qtwidgets based on pvr2d

TARGET = qmrofs_pvr2d

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QMrOfsIntegrationPlugin
load(qt_plugin)

QT += core-private gui-private widgets-private platformsupport-private

SOURCES = main.cpp qmrofsintegration.cpp qmrofsscreen.cpp
SOURCES += \ 
	qmrofswindow.cpp  \
	qmrofsbackingstore.cpp  \
	qmrofscompositor.cpp  \
	qmrofswaveform.cpp  \
	qmrofscursor.cpp   \
	qmrofscursorimage.cpp  \
	qmrofsimage.cpp  \
	qmrofsdefs.cpp  
	
HEADERS = qmrofsintegration.h qmrofsscreen.h
HEADERS += \
	qmrofswindow.h  \
	qmrofsbackingstore.h  \
	qmrofscompositor.h \
	qmrofswaveform.h  \
	qmrofscursor.h   \
	qmrofscursorimage.h   \
	qmrofsimage.h  \
	qmrofsdefs.h 
	
CONFIG += qpa/genericunixfontdatabase

DEFINES += QT_MROFS

LIBS += -lpvr2d

contains(QT_CONFIG, mrdp) {
    include($$QT_SOURCE_TREE/src/3rdparty/minilzo.pri)
	DEFINES += QT_MRDP
}

OTHER_FILES += qmrofs_pvr2d.json

