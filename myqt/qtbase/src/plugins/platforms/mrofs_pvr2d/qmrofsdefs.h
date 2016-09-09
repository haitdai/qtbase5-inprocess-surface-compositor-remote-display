#ifndef QMROFS_DEFS_H
#define QMROFS_DEFS_H
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <linux/kd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <limits.h>
#include <signal.h>
#include <linux/fb.h>
#include "pvr2d.h"
#include <QtWidgets/qabstractmrofs.h>    // QAbstractMrOfs
#include <QtCore/qglobal.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

#define QMROFS_THREAD_COMPOSITOR 1


// BLT STYLE {{{
/* 
DEBUG_CPU_BLT can use GPU_SURFACE or CPU_SURFACE 
*/
//#define DEBUG_CPU_BLT   1
#define DEBUG_GPU_BLT    1

//#define BATCH_BLT   1
#ifndef DEBUG_GPU_BLT
#udef BATCH_BLT
#endif
// }}} BLT STYLE

/* 
    DEBUG_GPU_BLT can ONLY use GPU_SURFACE 
*/
/* fb surface is always GPU SURFACE */
//#define CPU_SURFACE 1
#define GPU_SURFACE 1
#if defined(DEBUG_GPU_BLT) || !defined(DEBUG_CPU_BLT)
#undef CPU_SURFACE
#define GPU_SURFACE 1
#endif

/*!
    sometimes(we do NOT know when exactly), wired alignment issue by malloc + wrap!!!
*/
//#define QMROFS_MEM_ALLOCATED_PVR 1
//#define QMROFS_MEM_ALLOCATED_MALLOC    1
//CMEM's NONCACHE is much slower than PVR2D's 
//CMEM's CACHE is a little better than PVR2D's which has no CACHE mode supporting
#define QMROFS_MEM_ALLOCATED_CMEM   1
//#define DEBUG_BACKINGSTORE  1

/*************************************************************************/
/********************************* utils ***********************************/
/*************************************************************************/

const unsigned int FOURCC_UYVY = 0x59565955;          //LSB of "UYVY"

#define QMROFS_FORMAT_DEFAULT QAbstractMrOfs::Format_ARGB32_Premultiplied

int depthFromMainFormat(QAbstractMrOfs::Format mfmt);
bool validMainFormat(QAbstractMrOfs::Format mfmt);
QImage::Format qtFormatFromMainFormat(QAbstractMrOfs::Format mfmt);
PVR2DFORMAT pvr2DFormatFromMainFormat(QAbstractMrOfs::Format mfmt);
QImage::Format qtFormatFromPVR2DFormat(PVR2DFORMAT pvr2dfmt);
QAbstractMrOfs::Format mainFormatFromQtFormat(QImage::Format qfmt);
unsigned long tv_diff(struct timeval *tv1, struct timeval *tv2);

QT_END_NAMESPACE

#endif
