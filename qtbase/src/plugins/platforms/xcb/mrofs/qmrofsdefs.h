#ifndef QMROFS_DEFS_H
#define QMROFS_DEFS_H

#include "qxcbstd.h"
#include <QtWidgets/qmrofs.h>
QT_BEGIN_NAMESPACE

//独立的合成器线程
#define QMROFS_THREAD_COMPOSITOR

//profiling FPS
//#define MROFS_PROFILING  1

/*************************************************************************/
/********************************* utils ***********************************/
/*************************************************************************/
xcb_pict_standard_t stdPictFormatForDepth(int depth);
xcb_render_pictformat_t pictFormatFromStd(xcb_connection_t *conn, xcb_pict_standard_t std_fmt);             //for ui/wave rendering(output by qt)
xcb_render_pictformat_t pictFormatFromxBGR(xcb_connection_t *conn);                                       //for iview rendering(output by ISP@RGBA_LSBFirst@Alpha==0)
xcb_render_pictformat_t pictFormatFromVisualId(xcb_connection_t *conn, const xcb_visualid_t visualId);      //for screen display
xcb_xfixes_region_t qtRegion2XcbRegion(xcb_connection_t *conn, const QRegion &qt_rgn, xcb_xfixes_region_t xcb_rgn);

const unsigned int FOURCC_UYVY = 0x59565955;          //LSB of "UYVY"
int depthFromMainFormat(QMrOfs::Format mfmt);
bool validMainFormat(QMrOfs::Format mfmt);
QImage::Format qtFormatFromMainFormat(QMrOfs::Format mfmt);
uint32_t xvFormatFromMainFormat(QMrOfs::Format mfmt);                                                 //for iview rendering(output by ISP@FOURCC_UYVY)
bool checkExtensions(xcb_connection_t *conn);
xcb_screen_t *screenOfDisplay(xcb_connection_t *conn, int screen_num);
unsigned long tv_diff(struct timeval *tv1, struct timeval *t);

QT_END_NAMESPACE

#endif //QMROFS_DEFS_H
