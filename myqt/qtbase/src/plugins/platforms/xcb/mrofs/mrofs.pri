#bigdipper product:
#mrofs API based on xcb-xrender
#mipi-isp API based on v4l2(isp device) and h2c device

HEADERS += \
	mrofs/qmrofscompositor.h \
	mrofs/qmrofswaveform.h  \
	mrofs/qmrofscursor_p.h   \
	mrofs/qmrofscursorimage.h   \
	mrofs/qmrofsdefs.h 
	
SOURCES += \ 
	mrofs/qmrofscompositor.cpp  \
	mrofs/qmrofswaveform.cpp  \
	mrofs/qmrofscursor.cpp   \
	mrofs/qmrofscursorimage.cpp  \
	mrofs/qmrofsdefs.cpp 
