TARGET     = QtPrintSupport
QT = core-private gui-private widgets-private

DEFINES   += QT_NO_USING_NAMESPACE

QMAKE_DOCS = $$PWD/doc/qtprintsupport.qdocconf

load(qt_module)

QMAKE_LIBS += $$QMAKE_LIBS_PRINTSUPPORT

include(kernel/kernel.pri)
include(widgets/widgets.pri)
include(dialogs/dialogs.pri)


win32 {
    # for MMOS new delete override
    contains(CONFIG, debug){
        DEFINES += MEM_DEBUG
    }
    
    INCLUDEPATH += . \
            $$PWD/../../../next_mem/Include \
            $$PWD/../../../next_newdelete
    HEADERS  += \
        ../../../next_newdelete/NewOperatorEx.h 
    SOURCES += \
        ../../../next_newdelete/NewOperatorEx.cpp
    LIBS += -L$$PWD/../../../next_mem/Libs
    LIBS += -lVC_MemEx
}



# for BD print
contains(QT_CONFIG, printfile) {
	INCLUDEPATH += .  \
    	    $$PWD/../../../next_print_file/Include
	LIBS += -L$$PWD/../../../next_print_file/Libs
	LIBS += -L$$PWD/../../lib
	LIBS += -lPrintFile
	DEFINES += QT_PRINTFILE	
}
