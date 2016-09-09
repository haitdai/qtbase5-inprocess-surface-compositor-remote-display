TARGET = qinputmgrplugin

PLUGIN_TYPE = generic
PLUGIN_CLASS_NAME = QInputmgrPlugin
load(qt_plugin)

QT += core-private platformsupport-private gui-private

SOURCES = main.cpp

OTHER_FILES += \
    inputmgr.json

INCLUDEPATH += $$PWD/../../../../../inputmgr/include