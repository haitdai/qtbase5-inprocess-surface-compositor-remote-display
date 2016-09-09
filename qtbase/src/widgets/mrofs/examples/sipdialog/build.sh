#!/bin/sh
source ~/mkprofile
#~/arm-2009q1-rootfs/rootfs_mr/mk_fs_my/qt-5.2-am335x-mrofs-pvr2d-release-static/bin/qmake
~/arm-2009q1-rootfs/rootfs_mr/mk_fs_my/qt-5.2-am335x-mrofs-pvr2d/bin/qmake
make  -j8
arm-none-linux-gnueabi-objcopy -S sipdialog sipdialog_stripped
cp sipdialog_stripped ~/arm-2009q1-rootfs/rootfs_mr/mk_fs_my/.
