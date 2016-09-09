TEMPLATE = subdirs

*-maemo*: SUBDIRS += meego

contains(QT_CONFIG, evdev) {
    SUBDIRS += evdevmouse evdevtouch evdevkeyboard evdevtablet
    #DHT, tablet is NOT necessary
    SUBDIRS -= evdevtablet
    #ENDDHT
}

contains(QT_CONFIG, mrofs_inputmgr) {
    SUBDIRS += inputmgr
}

contains(QT_CONFIG, tslib) {
    SUBDIRS += tslib
}
