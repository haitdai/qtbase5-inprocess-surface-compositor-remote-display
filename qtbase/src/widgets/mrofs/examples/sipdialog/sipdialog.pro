QT += widgets core network

HEADERS       = dialog.h 
		
SOURCES       = main.cpp \
                dialog.cpp 

QMAKE_CXXFLAGS += -g 
QMAKE_LFLAGS = -L/media/DATA/project/mk/qt-merak/lib -lsrv_um
#-pg
#QMAKE_LFLAGS += -O2 
#-pg

unix {
    # use plugin pro's TARGET string
    #QTPLUGIN += qevdevmouseplugin qmrofs_pvr2d
    QTPLUGIN += qinputmgrplugin qmrofs_pvr2d
}

# install
target.path = $$[QT_INSTALL_EXAMPLES]/widgets/dialogs/sipdialog
INSTALLS += target

wince50standard-x86-msvc2005: LIBS += libcmt.lib corelibc.lib ole32.lib oleaut32.lib uuid.lib commctrl.lib coredll.lib winsock.lib ws2.lib
