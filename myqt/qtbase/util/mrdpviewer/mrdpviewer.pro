TEMPLATE = app

# for mrofs interface, QWidget, ...
QT += widgets 

# for mrdp_p.h (platformsupport private headers)
QT += platformsupport-private

# for include instructions in mrdp_p.h
INCLUDEPATH +=  $$PWD/../../src/platformsupport/mrdp
INCLUDEPATH +=  $$PWD/../../src/3rdparty/minilzo

LIBPATH         += $$PWD/../../../protobuf/lib

TARGET=mrdpviewer

HEADERS       = mrdpviewer.h   \
	         $$PWD/../../src/platformsupport/mrdp/mrdp.pb.h
	         
SOURCES       = main.cpp  \
                         mrdpviewer.cpp  \
                         $$PWD/../../src/platformsupport/mrdp/mrdp.pb.cc

win32 {
    # for MMOS new delete override
    contains(CONFIG, debug){
        DEFINES += MEM_DEBUG
    }
      
    INCLUDEPATH += .\
        $$PWD/../../../next_mem/Include \
        $$PWD/../../../next_newdelete
    HEADERS  += \
        ../../../next_newdelete/NewOperatorEx.h 
    SOURCES += \
        ../../../next_newdelete/NewOperatorEx.cpp
        
    LIBS += -L$$PWD/../../../next_mem/Libs    
    LIBS += -lVC_MemEx
}