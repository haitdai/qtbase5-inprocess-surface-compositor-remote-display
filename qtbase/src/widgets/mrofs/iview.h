#ifndef _IVIEW_H_
#define _IVIEW_H_
#include "QtWidgets/qabstractmrofs.h"

///////////////////////////////////////// MIPI DATA INPUT in MMAP MODE ////////////////////////////////////
#include <linux/videodev2.h>

//////////////////////////////////HDMI///////////////////////////////
#define DEF_DEVNAME              "/dev/video0"

typedef struct   {
    unsigned char *start;
    int mem_width;
    int mem_height;
    int length;
    int depth;
    int index;
    struct v4l2_buffer vbuf;
} isp_buf_info;

Q_WIDGETS_EXPORT int isp_open(char *devname = DEF_DEVNAME);
Q_WIDGETS_EXPORT int isp_close(int fd);

Q_WIDGETS_EXPORT int isp_setup(int fd, HDMI_CONFIG hcfg = HDMI_CONFIG_1280x720);

Q_WIDGETS_EXPORT isp_buf_info *isp_alloc_video_buffers(int fd, unsigned int request_buf_num);  //allocate video buffers for ISP chip inside
Q_WIDGETS_EXPORT int isp_dealloc_video_buffers(isp_buf_info *head, unsigned int num);                //deallocate video buffer for ISP chip inside

Q_WIDGETS_EXPORT int isp_video_on(int fd);           //config h2c chip inside
Q_WIDGETS_EXPORT int isp_video_off(int fd);           //reset h2c chip inside

Q_WIDGETS_EXPORT int isp_hdmi_2_mipi_init(char *arg, int len);

Q_WIDGETS_EXPORT isp_buf_info *isp_dqueue_video_buffer(int fd, isp_buf_info *pbufs, unsigned int num, int *result);
Q_WIDGETS_EXPORT int isp_queue_video_buffer(int fd, isp_buf_info *pbuf);

#endif  // _IVIEW_H_
