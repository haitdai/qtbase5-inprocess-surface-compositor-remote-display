#!/bin/bash
#############################################
# mrofs_legacy interface on mrofs_glesv2 QPA                                #
#############################################
#prepare dest dir
#mkdir -p ~/qt5.1/dist
#do configuration
#kms  not OK  under  mesa-8.0.4,  TODO:  try in mesa 9.1
#-nomake examples  -nomake  tests \
#
#$prefix/$sysroot
#-sysroot  /media/DATA/project/dubhe/335x/ti-sdk-5.6/linux-devkit/arm-arago-linux-gnueabi   
#-pkg-config  
#-hostprefix  /media/DATA/project/dubhe/335x/ti-sdk-5.6/linux-devkit
#-device linux-am335x-g++  -device-option CROSS_COMPILE=arm-arago-linux-gnueabi-  
#if does NOT work after clean,  append -fully-process to configure
#
#-debug
#-release
#-profile
#rm /media/DATA/project/dubhe/335x/ti-sdk-5.6/targetNFS/opt/qt-5.2-arm
#QtQuickControls NOT allowed this:
#-no-accessibility
if [ -z $QT_SRC_PATH ]; then
QT_SRC_PATH=/media/DATA/project/bigdipper/svn_soft_nmef/02.Code/Qt/qt-everywhere-opensource-src-5.2.1
fi
if [ -z $QT_SHIPPING_PATH ]; then
QT_SHIPPING_PATH=/home/dht/arm-2009q1-rootfs/rootfs_mr/dubhe_fs_my
fi
if [ "$QT_SHIPPING_TYPE" == "rel" ] || [ "$QT_SHIPPING_TYPE" == "release" ]; then
QT_SHIPPING_TYPE=release
elif [ "$QT_SHIPPING_TYPE" == "dbg" ] || [ "$QT_SHIPPING_TYPE" == "debug" ]; then
QT_SHIPPING_TYPE=debug
else
	echo "Wrong shipping type(QT_SHIPPING_TYPE: dbg|rel)!!!!!!!!!!!!!!!!!!!!!"
	exit -1
fi
if [ "$QT_STATIC_SHARED" == "static" ]; then
QT_STATIC_SHARED=static
else
QT_STATIC_SHARED=shared
fi
QT_SHIPPING_NAME=qt-5.2-am335x-mrofs-legacy-glesv2
QT_LINK_NAME=$QT_SHIPPING_NAME
QT_SHIPPING_NAME=$QT_SHIPPING_NAME-$QT_SHIPPING_TYPE-$QT_STATIC_SHARED
QT_SHIPPING_PREFIX=$QT_SHIPPING_PATH/$QT_SHIPPING_NAME
QT_LINK_PREFIX=$QT_SHIPPING_PATH/$QT_LINK_NAME
rm $QT_LINK_PREFIX
rm -rf $QT_SHIPPING_PREFIX
$QT_SRC_PATH/configure  \
-prefix    $QT_SHIPPING_PREFIX  \
-platform  linux-g++  -xplatform  linux-am335x-g++  \
-opensource \
-confirm-license  \
-largefile  \
-no-c++11  \
-shared   \
-make libs  \
-$QT_SHIPPING_TYPE  \
-$QT_STATIC_SHARED  \
-no-dbus  \
-separate-debug-info   \
-no-accessibility  \
-no-sql-db2 -no-sql-ibase -no-sql-mysql -no-sql-oci -no-sql-odbc -no-sql-psql -no-sql-sqlite -no-sql-sqlite2 -no-sql-tds  \
-no-javascript-jit  \
-no-qml-debug \
-no-sse2 -no-sse3 -no-ssse3 -no-sse4.1 -no-sse4.2 -no-avx -no-avx2 -no-mips_dsp -no-mips_dspr2    \
-qt-zlib \
-qt-libpng  \
-no-libjpeg  \
-no-openssl   \
-no-harfbuzz  \
-qt-pcre  \
-no-xcb  \
-no-xkbcommon  \
-nomake  examples  \
-nomake  tests  \
-no-nis  \
-no-cups  \
-no-iconv  \
-no-directfb  \
-no-kms  \
-no-glib  \
-no-icu  \
-no-eglfs   \
-no-printfile \
-mrofs_legacy  \
-mrofs_legacy_glesv2   \
-qpa mrofs_legacy_glesv2  \
-no-xcb-xlib  \
-no-opengl  \
-no-xinput2  \
-no-xinput  \
-no-egl \
-qt-freetype  \
-no-sm  \
-no-linuxfb  \
-verbose  $ADDITIONAL_OPTS
##make 
make -j16 module-qtbase
##install
make module-qtbase-install_subtargets
build_dir=$PWD
cd $QT_SHIPPING_PATH 
ln -s  $QT_SHIPPING_NAME  $QT_LINK_NAME
##delete .svn directories
find . -iname "*.svn" -exec rm -rf {} \;
cd $build_dir
