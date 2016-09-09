#dubhe product:
#legacy mrofs API in qtgui based on glesv2

TARGET = qmrofs_legacy_glesv2

PLUGIN_TYPE = platforms
PLUGIN_CLASS_NAME = QMrOfsIntegrationPlugin

include($$PWD/tools/tools.pri)
include($$PWD/tools/OGLES2/ogles2.pri)

load(qt_plugin)

QT += core-private gui-private platformsupport-private

##NOTE:  ?_vsh.cpp,  ?_fsh.cpp

SOURCES += main.cpp   \
	qmrofsintegration.cpp   \
	qmrofsscreen.cpp  \
    qmrofsbackingstore.cpp  \
	qmrofswindow.cpp \
	qmrofswaveform.cpp \	
	qmrofscompositor.cpp \
	qmrofscursor.cpp  
	
HEADERS += qmrofsintegration.h  \
		qmrofsscreen.h \
		qmrofsbackingstore.h \
		qmrofswindow.h \
		qmrofswaveform.h \
		qmrofscompositor.h \
		qmrofsdefs.h \
		qmrofscursor.h
		
CONFIG += qpa/genericunixfontdatabase

OTHER_FILES += mrofs_legacy_glesv2.json

##INCs for cmem
INCLUDEPATH += $$QMAKE_INCDIR_CMEM
#LIBs for cmem
LIBS += -L$$QMAKE_LIBDIR_CMEM $$QMAKE_LIBS_CMEM
## DIRTY PATCH!!!
LIBS += -lQt5Gui
##INCs for bufferclass_ti
INCLUDEPATH += $$QMAKE_INCDIR_BC

DEFINES += QT_MROFS_LEGACY

##DEFINEs for PVRT
DEFINES += BUILD_OGLES2

##INCs for PVRT
INCLUDEPATH += $$PWD/tools/OGLES2
INCLUDEPATH += $$PWD/tools
INCLUDEPATH += $$PWD
###libogles2tools.a compiled as source directly
#LIBS += $$PWD/tools/lib/libogles2tools.a

##INCs for EGL/OGLES2
INCLUDEPATH += $$QMAKE_INCDIR_OPENGL_ES2
!isEmpty(QMAKE_INCDIR_EGL): INCLUDEPATH += $$QMAKE_INCDIR_EGL
!isEmpty(QMAKE_LIBS_EGL): LIBS += $$QMAKE_LIBS_EGL

##LIBs for EGL/OGLES2
for(p, QMAKE_LIBDIR_OPENGL_ES2) {
    exists($$p):LIBS += -L$$p
}
for(p, QMAKE_LIBDIR_EGL) {
    exists($$p):LIBS += -L$$p
}
LIBS += $$QMAKE_LIBS_OPENGL_ES2

### generating shaders, $$PWD means current dir in source tree, not build path.
#FILEWRAP 	= $$PWD/utils/Filewrap/Linux_x86_32/Filewrap
#compositor_basic_vsh.target = compositor_basic_vsh.cpp 
#compositor_basic_vsh.commands = $$FILEWRAP -s -o $@ $$PWD/compositor_basic.vsh
#compositor_basic_vsh.depends = compositor_basic_fsh
#compositor_basic_fsh.target = compositor_basic_fsh.cpp
#compositor_basic_fsh.commands = $$FILEWRAP -s -o $@ $$PWD/compositor_basic.fsh
#compositor_basic_fsh.depends =
#QMAKE_EXTRA_TARGETS += compositor_basic_vsh compositor_basic_fsh
###PRE_TARGETDEPS += compositor_basic_vsh compositor_basic_fsh


