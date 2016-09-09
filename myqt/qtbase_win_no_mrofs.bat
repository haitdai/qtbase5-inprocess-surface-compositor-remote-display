echo off

set HOST_PREFIX_FOR_INSTALL=%QT_SHIPPING_PATH_HOST%\%QT_SHIPPING_TYPE%
if "%HOST_PREFIX_FOR_INSTALL%"=="" (
    echo hostprefix is NOT set
    pause
    goto end
)
set PREFIX_FOR_INSTALL=%QT_SHIPPING_PATH%\%QT_SHIPPING_TYPE%
if "%PREFIX_FOR_INSTALL%"=="" (
    echo prefix is NOT set
    pause
    goto end
)

mkdir  %PREFIX_FOR_BUILD%\qtbase\bin
copy /V /Y %MY_QT_SRC_ROOT%\next_mem\bin\*.dll  %PREFIX_FOR_BUILD%\qtbase\bin\.

set "MY_QT_SRC_ROOT=%~dp0"
echo %MY_QT_SRC_ROOT%
call %MY_QT_SRC_ROOT%configure.bat  -hostprefix %HOST_PREFIX_FOR_INSTALL%  -prefix %PREFIX_FOR_INSTALL% -%QT_SHIPPING_TYPE% -force-debug-info -no-ltcg -mp -platform win32-msvc2010 -qt-pcre -no-icu -qt-libjpeg -qt-libpng -qt-freetype -no-harfbuzz -opengl desktop -no-openvg -no-sql-mysql -no-sql-odbc -no-sql-oci -no-sql-psql -no-sql-tds -no-sql-db2 -no-sql-sqlite -no-sql-sqlite2 -no-sql-ibase -vcproj -no-incredibuild-xge -no-native-gestures -no-accessibility -no-openssl -no-dbus -no-audio-backend -no-wmf-backend -no-qml-debug -no-slog2 -no-pps -no-system-proxies -no-syncqt -no-angle -nomake examples -nomake tests -opensource -confirm-license -no-directwrite -no-nis -no-cups -no-iconv -no-neon -no-fontconfig -mrdp 

echo !!!CONFIGURE DONE, NOW NMAKE!!!
cd %PREFIX_FOR_BUILD%
echo BUILDING QT!!!
nmake -f Makefile %QT_SHIPPING_TYPE%
echo INSTALLING QT!!!
nmake -f Makefile install

::merge host install into install - just for convenience
xcopy /E /V /Y  %HOST_PREFIX_FOR_INSTALL%\*  %PREFIX_FOR_INSTALL%\.

::merge extra binaries
xcopy /E /V /Y %PREFIX_FOR_BUILD%\qtbase\bin\VC_MemEx.dll  %PREFIX_FOR_INSTALL%\bin\.

:end