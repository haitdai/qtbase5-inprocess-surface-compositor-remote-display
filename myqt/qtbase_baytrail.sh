#!/bin/bash
#############################################
# mrofs interface on xcb QPA                                                            #
#############################################
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
#rm /media/DATA/project/dubhe/335x/ti-sdk-5.6/targetNFS/opt/qt-5.2-am335x
#QtQuickControls NOT allowed this:
#-no-accessibility
#-qt-xcb make qt only depends system libxcb, other util libs such as image/renderutil/keysyms/... are all self-contained by qt
#-qt-xkbcommon because  yocoto/poky sysroot NOT contains libxkbcommon
#iconv is always used since contained in libc
#dubhe use qt-freetype and self-contained "font config"
#bigdipper use system freetype and system fontconfig
#-egl: in bigdipper, we use EGL+OGLES2 under X instead of GLX+OGL; in dubhe(no X), we use EGL+OGLES2 under fbdev
#-skip qtwebkit 
#-prefix is replaced by -extprefix when sysroot is used
#is not used since under windows we have NO sdk
if [ -z $QT_SRC_PATH ]; then
QT_SRC_PATH=/media/DATA/project/bigdipper/svn_soft_nmef/02.Code/Qt/qt-everywhere-opensource-src-5.2.1
fi
if [ -z $QT_SHIPPING_PATH ]; then
QT_SHIPPING_PATH=/home/dht/arm-2009q1-rootfs/rootfs_mr/bigdipper_fs_my
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
QT_SHIPPING_NAME=qt-5.2-bigdipper-mrofs-xcb
QT_LINK_NAME=$QT_SHIPPING_NAME
QT_SHIPPING_NAME=$QT_SHIPPING_NAME-$QT_SHIPPING_TYPE-$QT_STATIC_SHARED
QT_SHIPPING_PREFIX=$QT_SHIPPING_PATH/$QT_SHIPPING_NAME
QT_LINK_PREFIX=$QT_SHIPPING_PATH/$QT_LINK_NAME
rm $QT_LINK_PREFIX
rm -rf $QT_SHIPPING_PREFIX
$QT_SRC_PATH/configure  \
-sysroot  /opt/poky/1.4.3/sysroots/core2-poky-linux   \
-gcc-sysroot \
-extprefix    $QT_SHIPPING_PREFIX  \
-platform  linux-g++  -xplatform  linux-baytrail-g++  \
-opensource \
-confirm-license  \
-no-c++11  \
-shared   \
-make libs  \
-$QT_SHIPPING_TYPE  \
-$QT_STATIC_SHARED  \
-no-dbus  \
-separate-debug-info \
-no-accessibility  \
-no-sql-db2 -no-sql-ibase -no-sql-mysql -no-sql-oci -no-sql-odbc -no-sql-psql -no-sql-sqlite -no-sql-sqlite2 -no-sql-tds  \
-no-javascript-jit  \
-no-qml-debug \
-no-neon -no-avx -no-avx2 -no-mips_dsp -no-mips_dspr2    \
-qt-zlib \
-qt-libpng  \
-qt-libjpeg  \
-no-openssl   \
-no-harfbuzz  \
-qt-pcre  \
-system-xkbcommon  \
-nomake  examples  \
-nomake  tests  \
-no-nis  \
-no-cups  \
-iconv  \
-no-directfb  \
-no-kms  \
-no-glib  \
-no-icu  \
-no-mtdev \
-no-eglfs   \
-mrofs \
-mrofs_inputmgr  \
-mipi_isp  \
-system-xcb  \
-xcb-xlib  \
-qpa  xcb  \
-xinput2  \
-no-opengl \
-no-egl  \
-system-freetype  \
-fontconfig  \
-printfile \
-no-linuxfb  \
-verbose  $ADDITIONAL_OPTS
##make 
make -j8 module-qtbase
##install
make module-qtbase-install_subtargets
build_dir=$PWD
cd $QT_SHIPPING_PATH 
ln -s  $QT_SHIPPING_NAME  $QT_LINK_NAME
##delete .svn directories
find . -iname "*.svn" -exec rm -rf {} \;
cd $build_dir



