QT -= QT_MROFS_LEGACY
contains(QT_CONFIG, mrofs) {
    DEFINES += QT_MROFS	
    HEADERS += \
	mrofs/qmrofs.h \
	mrofs/qmrofs_p.h 
	
    SOURCES += \ 
	mrofs/qmrofs.cpp 
	contains(QT_CONFIG, mipi_isp) {	
		DEFINES += QT_MIPI_ISP
		HEADERS += mrofs/iview.h
		SOURCES += mrofs/iview.cpp
	}
	
    contains(QT_CONFIG, mrdp) {
        DEFINES += QT_MRDP
    }
}

