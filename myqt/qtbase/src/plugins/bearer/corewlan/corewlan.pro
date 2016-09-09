TARGET = qcorewlanbearer

PLUGIN_TYPE = bearer
PLUGIN_CLASS_NAME = QCoreWlanEnginePlugin
load(qt_plugin)

QT = core-private network-private
LIBS += -framework Foundation -framework SystemConfiguration

contains(QT_CONFIG, corewlan) {
    LIBS += -framework CoreWLAN -framework Security
}

HEADERS += qcorewlanengine.h \
           ../qnetworksession_impl.h \
           ../qbearerengine_impl.h

SOURCES += main.cpp \
           ../qnetworksession_impl.cpp

OBJECTIVE_SOURCES += qcorewlanengine.mm

OTHER_FILES += corewlan.json

win32 {
    # for MMOS new delete override
    contains(CONFIG, debug){
        DEFINES += MEM_DEBUG
    }
    
    # for MMOS new delete override
    contains(CONFIG, debug){
        DEFINES += MEM_DEBUG
    }
    
    INCLUDEPATH += .\
            $$PWD/../../../../../next_mem/Include \
            $$PWD/../../../../../next_newdelete
    HEADERS  += \
        ../../../../../next_newdelete/NewOperatorEx.h 
    SOURCES += \
        ../../../../../next_newdelete/NewOperatorEx.cpp
        
    LIBS += -L$$PWD/../../../../../next_mem/Libs
    LIBS += -lVC_MemEx
}