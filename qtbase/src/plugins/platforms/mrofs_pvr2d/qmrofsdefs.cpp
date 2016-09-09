#include "qmrofsdefs.h"

QT_BEGIN_NAMESPACE

/*************************************************************************/
/********************************* utils ***********************************/
/*************************************************************************/

/*!
    \note xvinfo id print BUG
*/
static const struct FormatMap{
    QAbstractMrOfs::Format        main_fmt;
    QImage::Format                qt_fmt;
    uint                          pvr2d_fmt;
    uint                          depth;
} s_fm[] = { 
    /*QAbstractMrOfs format*/                       /*qt format*/                          /*pvr2d_fmt*/         /*depth*/
    {QAbstractMrOfs::Format_ARGB32_Premultiplied,       QImage::Format_ARGB32_Premultiplied,    PVR2D_ARGB8888,         32}, // waveform/backingstore@baytrail
    {QAbstractMrOfs::Format_ARGB32,                           QImage::Format_ARGB32,                        PVR2D_ARGB8888,         32},
    {QAbstractMrOfs::Format_RGB32,                             QImage::Format_RGB32,                          PVR2D_ARGB8888,         32},  // ???
    {QAbstractMrOfs::Format_xBGR32,                           QImage::Format_RGBX8888,                     PVR2D_RGB888,             24}, // ivew@RGB_LSBFirst@baytrail
    {QAbstractMrOfs::Format_RGBA32,                           QImage::Format_RGBA8888,                     PVR2D_ARGB8888,         32},  // in case using fb with RGBA format OR. using OGLES2 texture
    {QAbstractMrOfs::Format_RGBA32_Premultiplied,       QImage::Format_RGBA8888_Premultiplied, PVR2D_ARGB8888,         32},
    {QAbstractMrOfs::Format_UYVY,                               QImage::Format_Invalid,                           PVR2D_NO_OF_FORMATS, 0},  // ivewform@YUV@baytrail
};

#define TABLESIZE(tab) sizeof(tab)/sizeof(tab[0])

int depthFromMainFormat(QAbstractMrOfs::Format mfmt)
{
    for(unsigned int i = 0; i < TABLESIZE(s_fm); i ++) {
        if (mfmt == s_fm[i].main_fmt)
            return s_fm[i].depth;
    }
    return 0;
}

/*!
    check
*/
bool validMainFormat(QAbstractMrOfs::Format mfmt)
{
    for(unsigned int i = 0; i < TABLESIZE(s_fm); i ++) {
        if (mfmt == s_fm[i].main_fmt)
            return true;
    }
    return false;
}

/*!
    QImage::Format
*/
QImage::Format qtFormatFromMainFormat(QAbstractMrOfs::Format mfmt)
{
    for(unsigned int i = 0; i < TABLESIZE(s_fm); i ++) {
        if (mfmt == s_fm[i].main_fmt)
            return s_fm[i].qt_fmt;
    }
    return QImage::Format_Invalid;
}

/*!
    PVR2D_NO_OF_FORMATS: invalid format
*/
PVR2DFORMAT pvr2DFormatFromMainFormat(QAbstractMrOfs::Format mfmt)
{
    for(unsigned int i = 0; i < TABLESIZE(s_fm); i ++) {
        if (mfmt == s_fm[i].main_fmt)
                return s_fm[i].pvr2d_fmt;
    }

    return PVR2D_NO_OF_FORMATS;
}

/*!
    QImage::Format
*/
QImage::Format qtFormatFromPVR2DFormat(PVR2DFORMAT pvr2dfmt) {
    for(unsigned int i = 0; i < TABLESIZE(s_fm); i ++) {
        if (pvr2dfmt == s_fm[i].pvr2d_fmt)
            return s_fm[i].qt_fmt;
    }

    return QImage::Format_Invalid;
}

QAbstractMrOfs::Format mainFormatFromQtFormat(QImage::Format qfmt)
{
    for(unsigned int i = 0; i < TABLESIZE(s_fm); i ++) {
        if (qfmt == s_fm[i].qt_fmt)
            return s_fm[i].main_fmt;
    }
    return QAbstractMrOfs::Format_ARGB32_Premultiplied;
}

unsigned long tv_diff(struct timeval *tv1, struct timeval *tv2)
{
    return (tv2->tv_sec - tv1->tv_sec) * 1000 +  (tv2->tv_usec - tv1->tv_usec) / 1000;
}

QT_END_NAMESPACE


