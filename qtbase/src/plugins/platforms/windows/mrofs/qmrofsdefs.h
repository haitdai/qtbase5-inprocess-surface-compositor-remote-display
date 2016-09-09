#ifndef QMROFS_DEFS_H
#define QMROFS_DEFS_H

#include "qtwindowsglobal.h"
#include <QtWidgets/qabstractmrofs.h>    // QAbstractMrOfs
#include <QtCore/qglobal.h>
#include <QDebug>

QT_BEGIN_NAMESPACE

#define QMROFS_THREAD_COMPOSITOR 1

// sync bounding rect instead of clip rects, for reducing API calling
//#define QMROFS_SYNC_BOUNDING_RECT 1

/*************************************************************************/
/********************************* utils ***********************************/
/*************************************************************************/

const unsigned int FOURCC_UYVY = 0x59565955;          //LSB of "UYVY"

#define QMROFS_FORMAT_DEFAULT QAbstractMrOfs::Format_ARGB32_Premultiplied

int depthFromMainFormat(QAbstractMrOfs::Format mfmt);
bool validMainFormat(QAbstractMrOfs::Format mfmt);
QImage::Format qtFormatFromMainFormat(QAbstractMrOfs::Format mfmt);
QAbstractMrOfs::Format mainFormatFromQtFormat(QImage::Format qfmt);
unsigned long tv_diff(struct timeval *tv1, struct timeval *tv2);

QT_END_NAMESPACE

#endif
