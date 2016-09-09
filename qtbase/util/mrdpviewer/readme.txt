*basically, mrdpviewer can run on all platforms that qt supported.

*howto to gen vcproj
<QTDIR>\bin\qmake -spec win32-msvc2010 -tp vc <QT_SRC_DIR>\qtbase\util\mrdpviewer\mrdpviewer.pro
where QTDIR is your customized qt installed directory; QT_SRC_DIR is your qt source directory.
qt for vs plugin is unnecessary.