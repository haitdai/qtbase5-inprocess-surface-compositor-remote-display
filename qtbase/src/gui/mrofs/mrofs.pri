contains(QT_CONFIG, mrofs_legacy) {
DEFINES += QT_MROFS_LEGACY
HEADERS += \
	mrofs/qmrofs.h \
	mrofs/qmrofs_p.h   \
	mrofs/ScreenScrollController.h

SOURCES += \ 
	mrofs/qmrofs.cpp  \ 
	mrofs/ScreenScrollController.cpp
}
