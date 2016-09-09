#include "qmrofsdefs.h"

QT_BEGIN_NAMESPACE

/*************************************************************************/
/********************************* utils ***********************************/
/*************************************************************************/

/*!
    \note xvinfo id print BUG
*/
static const struct FormatMap{
    QMrOfs::Format        main_fmt;
    QImage::Format          qt_fmt;
    int                     xrender_fmt;
    int                     xv_fmt;
    int                     depth;
} s_fm[] = { 
    /*mrofs format*/                       /*qt format*/                          /*xcb_pict_standard_t*/      /*xcb_xv_image_format_info_t.id(fourcc)*/    /*depth*/
    {QMrOfs::Format_ARGB32_Premultiplied, QImage::Format_ARGB32_Premultiplied,  XCB_PICT_STANDARD_ARGB_32, 0,                                      32}, //waveform/backingstore
    {QMrOfs::Format_xBGR32,               QImage::Format_RGBX8888,              0,                         0,                                      32}, //ivew@RGB_LSBFirst
    {QMrOfs::Format_UYVY,                 QImage::Format_Invalid,               0,                         FOURCC_UYVY,                            16}, //ivewform@YUV
};

#define TABLESIZE(tab) sizeof(tab)/sizeof(tab[0])

int depthFromMainFormat(QMrOfs::Format mfmt)
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
bool validMainFormat(QMrOfs::Format mfmt)
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
QImage::Format qtFormatFromMainFormat(QMrOfs::Format mfmt)
{
    for(unsigned int i = 0; i < TABLESIZE(s_fm); i ++) {
        if (mfmt == s_fm[i].main_fmt)
            return s_fm[i].qt_fmt;
    }
    return QImage::Format_Invalid;
}

/*!
    fourcc
*/
uint32_t xvFormatFromMainFormat(QMrOfs::Format mfmt)
{
    for(unsigned int i = 0; i < TABLESIZE(s_fm); i ++) {
        if (mfmt == s_fm[i].main_fmt)
            return s_fm[i].xv_fmt;
    }
    return 0;
}

/*!
    get standard xcb pict format from depth
*/
xcb_pict_standard_t stdPictFormatForDepth(int depth)
{
    switch(depth) {
        //same as QImage::ARGB32_premultiplied, all of them are byte-order-free format
        case 32: return XCB_PICT_STANDARD_ARGB_32;
        case 24: return XCB_PICT_STANDARD_RGB_24;
        default:
            qWarning("Unsupported screen depth: %d", depth);
            return (xcb_pict_standard_t)(-1);
    }
}

/*!
    other std: XCB_PICT_STANDARD_RGB_24, XCB_PICT_STANDARD_A_8, ...
    \note  用于创建pixmap (使用standard xrender pict format argb)    
*/
xcb_render_pictformat_t pictFormatFromStd(xcb_connection_t *conn, xcb_pict_standard_t std_fmt)
{
    xcb_generic_error_t *error = 0;
    xcb_render_query_pict_formats_cookie_t formatsCookie = xcb_render_query_pict_formats(conn);
    xcb_render_query_pict_formats_reply_t *formatsReply = xcb_render_query_pict_formats_reply(conn, formatsCookie, &error);
    if (!formatsReply || error) {
        qFatal("pictFormatFromStd: query_pict_formats failed");
        free(formatsReply);
        free(error);
    }
        
    xcb_render_pictforminfo_t *fmt_inf = xcb_render_util_find_standard_format(formatsReply, std_fmt);
    if (!fmt_inf) {
        qFatal("pictFormatFromStd: Failed to find format %d", std_fmt);
        free(formatsReply);
    } else { 
        xcb_render_directformat_t *direct = &fmt_inf->direct;
        qDebug("pictFormatFromStd-xcb_render_picformatinfo_t: id(%d), type(0x%x), depth(%d),  \
            red_shift(%d), red_mask(0x%x), green_shift(%d), green_mask(0x%x), blue_shift(%d), blue_mask(0x%x), alpha_shift(%d), alpha_mask(0x%x), \
            colormap(0x%x)",
            fmt_inf->id, fmt_inf->type, fmt_inf->depth, 
            direct->red_shift, direct->red_mask, direct->green_shift, direct->green_mask, direct->blue_shift, direct->blue_mask, direct->alpha_shift, direct->alpha_mask,
            fmt_inf->colormap);
    }

    return fmt_inf->id;
}

/*
    iView@RGB 时, format 为BGR LSBFirst(little endian)
    \ref xcb_render_util_find_standard_format

    \note 
    xrender 支持PICT_x8b8g8r8和PICT_b8g8r8,  EMGD 支持PICT_a8b8g8r8 和PICT_x8b8g8r8(ref gen7_get_card_format in SNA driver)
    ISP 没有填充ALPHA 通道，因此使用PICT_x8b8g8r8 最合适
    \ref PictureCreateDefaultFormats
*/
xcb_render_pictformat_t pictFormatFromxBGR(xcb_connection_t *conn)
{
    xcb_generic_error_t *error = 0;
    xcb_render_query_pict_formats_cookie_t formatsCookie = xcb_render_query_pict_formats(conn);
    xcb_render_query_pict_formats_reply_t *formatsReply = xcb_render_query_pict_formats_reply(conn, formatsCookie, &error);
    if (!formatsReply || error) {
        qFatal("pictFormatFromxBGR: query_pict_formats failed");
        free(formatsReply);
        free(error);
    }
        
    static const struct {
    xcb_render_pictforminfo_t templ;
    unsigned long         mask;
    } abgrFormats = {        /* PICT_a8b8g8r8: r-b swap of PICT_a8r8g8b8 */
        {
            0,              /* id */
            XCB_RENDER_PICT_TYPE_DIRECT,    /* type */
            32,             /* depth - ref. PictureCreateDefaultFormats */
            { 0 },          /* pad */
            {               /* direct */
                0,          /* direct.red */
                0xff,       /* direct.red_mask */
                8,          /* direct.green */
                0xff,       /* direct.green_mask */
                16,         /* direct.blue */
                0xff,       /* direct.blue_mask */
                0,         /* direct.alpha */
                0,          /* direct.alpha_mask - ref. PictureCreateDefaultFormats */
            },
            0,              /* colormap */
        },
        XCB_PICT_FORMAT_TYPE | 
        XCB_PICT_FORMAT_DEPTH |
        XCB_PICT_FORMAT_RED |
        XCB_PICT_FORMAT_RED_MASK |
        XCB_PICT_FORMAT_GREEN |
        XCB_PICT_FORMAT_GREEN_MASK |
        XCB_PICT_FORMAT_BLUE |
        XCB_PICT_FORMAT_BLUE_MASK |
//        XCB_PICT_FORMAT_ALPHA |           //if alphamask == 0, the offset of alpha is omitted
        XCB_PICT_FORMAT_ALPHA_MASK,
    };

    xcb_render_pictforminfo_t *fmt_inf = xcb_render_util_find_format (formatsReply, abgrFormats.mask, &abgrFormats.templ, 0);
    if (!fmt_inf) {
        qFatal("pictFormatFromxBGR: Failed to find format");
        free(formatsReply);
    } else { 
        xcb_render_directformat_t *direct = &fmt_inf->direct;
        qDebug("pictFormatFromxBGR-xcb_render_picformatinfo_t: id(%d), type(0x%x), depth(%d),  \
            red_shift(%d), red_mask(0x%x), green_shift(%d), green_mask(0x%x), blue_shift(%d), blue_mask(0x%x), alpha_shift(%d), alpha_mask(0x%x), \
            colormap(0x%x)",
            fmt_inf->id, fmt_inf->type, fmt_inf->depth, 
            direct->red_shift, direct->red_mask, direct->green_shift, direct->green_mask, direct->blue_shift, direct->blue_mask, direct->alpha_shift, direct->alpha_mask,
            fmt_inf->colormap);
    }

    return fmt_inf->id;
}

/*!
    the ONLY difference between xBGR and BGR is the depth
    \note  emgd driver treat PICT_r8g8b8 as BGR@LSBFirst which is same as ISP does
*/
xcb_render_pictformat_t pictFormatFromBGR(xcb_connection_t *conn)
{
    xcb_generic_error_t *error = 0;
    xcb_render_query_pict_formats_cookie_t formatsCookie = xcb_render_query_pict_formats(conn);
    xcb_render_query_pict_formats_reply_t *formatsReply = xcb_render_query_pict_formats_reply(conn, formatsCookie, &error);
    if (!formatsReply || error) {
        qFatal("pictFormatFromBGR: query_pict_formats failed");
        free(formatsReply);
        free(error);
    }
        
    static const struct {
    xcb_render_pictforminfo_t templ;
    unsigned long         mask;
    } bgrFormats = {        /* PICT_r8g8b8 */
        {
            0,              /* id */
            XCB_RENDER_PICT_TYPE_DIRECT,    /* type */
            24,             /* depth - ref. PictureCreateDefaultFormats */
            { 0 },          /* pad */
            {               /* direct */
                0,          /* direct.red */
                0xff,       /* direct.red_mask */
                8,          /* direct.green */
                0xff,       /* direct.green_mask */
                16,         /* direct.blue */
                0xff,       /* direct.blue_mask */
                0,         /* direct.alpha */
                0,          /* direct.alpha_mask - ref. PictureCreateDefaultFormats */
            },
            0,              /* colormap */
        },
        XCB_PICT_FORMAT_TYPE | 
        XCB_PICT_FORMAT_DEPTH |
        XCB_PICT_FORMAT_RED |
        XCB_PICT_FORMAT_RED_MASK |
        XCB_PICT_FORMAT_GREEN |
        XCB_PICT_FORMAT_GREEN_MASK |
        XCB_PICT_FORMAT_BLUE |
        XCB_PICT_FORMAT_BLUE_MASK |
//        XCB_PICT_FORMAT_ALPHA |               //when alphmask == 0, the offset of alpha is omitted
        XCB_PICT_FORMAT_ALPHA_MASK,
    };

    xcb_render_pictforminfo_t *fmt_inf = xcb_render_util_find_format(formatsReply, bgrFormats.mask, &bgrFormats.templ, 0);
    if (!fmt_inf) {
        qFatal("pictFormatFromBGR: Failed to find format");
        free(formatsReply);
    } else { 
        xcb_render_directformat_t *direct = &fmt_inf->direct;
        qDebug("pictFormatFromBGR-xcb_render_picformatinfo_t: id(%d), type(0x%x), depth(%d),  \
            red_shift(%d), red_mask(0x%x), green_shift(%d), green_mask(0x%x), blue_shift(%d), blue_mask(0x%x), alpha_shift(%d), alpha_mask(0x%x), \
            colormap(0x%x)",
            fmt_inf->id, fmt_inf->type, fmt_inf->depth, 
            direct->red_shift, direct->red_mask, direct->green_shift, direct->green_mask, direct->blue_shift, direct->blue_mask, direct->alpha_shift, direct->alpha_mask,
            fmt_inf->colormap);
    }

    return fmt_inf->id;
}

/*
    iView@YUV时, 直接输出不合成(因为xv 限制,无法输出到非window 的drawable上)
*/

/*!
    \note visualId usually comes from screen info
    \ref xcb_get_setup
    \note 用于创建xcb win( visual id 为screen root visual)
*/
xcb_render_pictformat_t pictFormatFromVisualId(xcb_connection_t *conn, const xcb_visualid_t visualId)
{
    xcb_render_pictformat_t fmt;
    
    //query all supported formats and check error
    xcb_generic_error_t *error = 0;
    xcb_render_query_pict_formats_cookie_t formatsCookie = xcb_render_query_pict_formats(conn);
    xcb_render_query_pict_formats_reply_t *formatsReply = xcb_render_query_pict_formats_reply(conn, formatsCookie, &error);
    if (!formatsReply || error) {
        qWarning("QXcbWindow::createPicture: query_pict_formats failed");
        free(formatsReply);
        free(error);
        return XCB_NONE;
    }
    
    //pick a visual/format from all supported formats by visualId
    xcb_render_pictvisual_t *visual = xcb_render_util_find_visual_format(formatsReply, visualId);
    if (!visual) {
        qFatal("pictVisualFromId: Failed to find visual for visualid(%d)", visualId);
        free(formatsReply);
    } else { 
        fmt = visual->format;
        qDebug("pictVisualFromId-xcb_render_picformatinfo_t: id(%d)", fmt);
    }

    return fmt;
}

/*!
    transform qt region to xcb region
*/
xcb_xfixes_region_t qtRegion2XcbRegion(xcb_connection_t *conn, const QRegion &qt_rgn, xcb_xfixes_region_t xcb_rgn)
{
    QVector<QRect> qt_rects = qt_rgn.rects(); 
    QVector<xcb_rectangle_t> xcb_rects(qt_rects.size());
    xcb_rectangle_t *xcb_rect = xcb_rects.data();
    for (int i = 0; i < xcb_rects.size(); i++) {                                                                    //qt rect to xcb rect
        xcb_rect[i].x = static_cast<short>(qt_rects[i].x());
        xcb_rect[i].y = static_cast<short>(qt_rects[i].y());
        xcb_rect[i].width = static_cast<short>(qt_rects[i].width());
        xcb_rect[i].height = static_cast<short>(qt_rects[i].height());
    }

    xcb_xfixes_set_region(conn, xcb_rgn, xcb_rects.size(), xcb_rect);

    return xcb_rgn;
}

/**/
static bool checkExtension(xcb_connection_t *conn, xcb_extension_t *ext)
{
    const xcb_query_extension_reply_t *ext_reply = NULL;
    bool ext_present = false;
    
    ext_reply = xcb_get_extension_data(conn, ext);
    ext_present = ext_reply && ext_reply->present;
    if(!ext_present) {
        qFatal("checkExtension error: name(%s), global_id(%d)", ext->name, ext->global_id);
        return false;
    } else {
        qWarning("checkExtension: name(%s), global_id(%d), major_opcode(%d), first_event(%d), first_error(%d)", 
            ext->name, ext->global_id, ext_reply->major_opcode, ext_reply->first_event, ext_reply->first_error);
        return true;
    }
}

/*!
    check extensions required by MrOfs
    \ref ExtensionToggleList in miinitext.c of xserver source
*/
bool checkExtensions(xcb_connection_t *conn)
{  
    if(!checkExtension(conn, &xcb_xfixes_id))
        return false;

    if(!checkExtension(conn, &xcb_render_id))
        return false;    

    if(!checkExtension(conn, &xcb_shm_id))
        return false;    
    
    if(!checkExtension(conn, &xcb_xv_id))
        return false;

    return true;
}

xcb_screen_t *screenOfDisplay(xcb_connection_t *conn, int screen_num)
{
    xcb_screen_iterator_t it;
    it = xcb_setup_roots_iterator(xcb_get_setup(conn));
    for(; it.rem; --screen_num, xcb_screen_next(&it)) {
        if(!screen_num)
            return it.data;
    }

    return NULL;
}

unsigned long tv_diff(struct timeval *tv1, struct timeval *t)
{
    return (t->tv_sec - tv1->tv_sec) * 1000 +  (t->tv_usec - tv1->tv_usec) / 1000;
}

QT_END_NAMESPACE


