contains(QT_CONFIG, mrofs) {
    INCLUDEPATH += $$PWD
    HEADERS += \
        $$PWD/qplatformcompositor_p.h
    SOURCES += \
        $$PWD/qplatformcompositor.cpp
}
