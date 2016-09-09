TEMPLATE = subdirs

contains(QT_CONFIG, accessibility) {
     SUBDIRS += widgets 
}

win32 {
    # for MMOS new delete override
    contains(CONFIG, debug){
        DEFINES += MEM_DEBUG
    }
      
    INCLUDEPATH += .\
        $$PWD/../../../../next_mem/Include \
        $$PWD/../../../../next_newdelete
    HEADERS  += \
        ../../../../next_newdelete/NewOperatorEx.h 
    SOURCES += \
        ../../../../next_newdelete/NewOperatorEx.cpp
        
    LIBS += -L$$PWD/../../../../next_mem/Libs    
    LIBS += -lVC_MemEx
}