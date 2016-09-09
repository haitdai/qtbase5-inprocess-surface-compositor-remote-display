#include "iview.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/i2c.h> 
#ifdef VIDEO_MAX_PLANES				// otherwise we have no v4l2-subdev
#include <linux/v4l2-subdev.h>
#endif
#include <asm/types.h>

static HDMI_CONFIG s_hdmi_cfg = HDMI_CONFIG_1280x720;

/////////////////////////////////////// LOG /////////////////////////////////////////////
#define V4L2_DEBUG  0x00
#define V4L2_INFO   0x01
#define V4L2_ERROR  0x02
#define V4L2_MIN    V4L2_DEBUG
#define V4L2_MAX    V4L2_ERROR

#define GET_TIME(tv1, tv2, fp, str) do {\
    gettimeofday(&tv2, NULL);\
    fprintf(fp, "--> [%ld]: %s", 1000000 * (tv2.tv_sec - tv1.tv_sec) + (tv2.tv_usec - tv1.tv_usec), str);\
    tv1 = tv2;\
} while (0)

void debug_for_v4l2(int level, const char *format, ...);

#define log_debug(format, ...) debug_for_v4l2(V4L2_DEBUG, format, ##__VA_ARGS__)
#define log_info(format, ...) debug_for_v4l2(V4L2_INFO, format, ##__VA_ARGS__)
#define log_err(format, ...) debug_for_v4l2(V4L2_ERROR, format, ##__VA_ARGS__)

static const char *tips[3] = {" DEBUG- ", " INFO - ", " ERROR- "};

static int is_valid_level(int level)
{
    if(level < V4L2_MIN || level > V4L2_MAX)
        return -1;
    else
        return 0;
}

void debug_for_v4l2(int level, const char *format, ...)
{
    va_list p;

    if(-1 == is_valid_level(level)){
        fprintf(stdout, "The param @level is invalid.\n");
        return;
    }

    fprintf(stdout, "%s", tips[level]);
    va_start(p, format);
    vfprintf(stdout, format, p);
    if (V4L2_ERROR == level)
        vfprintf(stdout, format, p);
    va_end(p);

    if(V4L2_ERROR == level){
        fprintf(stdout, ":%d-%s.\n", errno, strerror(errno));
    }
}

////////////////////////////////// CONVERSION ///////////////////////////////
void convert_0bgr2xrgb(unsigned char *dst, unsigned char *src, int width, int height, int depth);
void convert_0bgr2xbgr(unsigned char *dst, unsigned char *src, int width, int height, int depth);

/////////////////////////////////////// HDMI ////////////////////////////////////////////
#define DEV_PATH                    "/dev/i2c-3"
#define I2C_ADDR                      0x0F
#define I2C_RDWR                      0x0707
#define IMG_IN_FORMAT            0x800B     // for RGB888
//#define   IMG_IN_FORMAT       0x200F      // for YUV422P

#define IMG_OUT_FORMAT            V4L2_PIX_FMT_RGB24        // for RGB
//#define   IMG_OUT_FORMAT      V4L2_PIX_FMT_UYVY
//#define   IMG_OUT_FORMAT      V4L2_PIX_FMT_VYUY
//#define   IMG_OUT_FORMAT      V4L2_PIX_FMT_YUYV

typedef struct {
    int in_width;
    int in_height;
    int in_format;
    int out_width;
    int out_height;
    int out_format;
    int mem_width;          //32 byte alignment
    int mem_height;
} resolution;
static resolution* resolution_from_hdmi_config(HDMI_CONFIG hcfg);

#define OPS_BITS_8       0x01       /* should be same as MISENSOR_8BIT */
#define OPS_BITS_16     0x02        /* should be same as MISENSOR_16BIT */
#define OPS_BITS_32     0x04        /* should be same as MISENSOR_32BIT */
typedef struct {
    unsigned short  ops_bits;
    unsigned short  reg;
    unsigned int    data;
    unsigned int    time;                      /* the time(us) should be wait after write */
}H2C_IPC_INIT_CODE;
static H2C_IPC_INIT_CODE* h2c_ipc_init_code_from_hdmi_config(HDMI_CONFIG hcfg);
static const char * h2c_ipc_init_code_str(HDMI_CONFIG hcfg);
static int hdmi_2_mipi_init(char *arg, int len);

    
H2C_IPC_INIT_CODE hdmi_init_cfg_1280x720[] = {  //1280*720
    {OPS_BITS_16, 0x0002, 0x0F00, 10},
    {OPS_BITS_16, 0x0002, 0x0000, 0},
    /*// FIFO Delay Setting*/
    {OPS_BITS_16, 0x0006, 0x0154, 0},
    /*// PLL Setting*/
    {OPS_BITS_16, 0x0020, 0x3057, 0},
    {OPS_BITS_16, 0x0022, 0x0203, 10},
    /*Waitx1us(10);*/
    {OPS_BITS_16, 0x0022, 0x0213, 0},
    /*// Interrupt Control*/
    {OPS_BITS_16, 0x0014, 0x0000, 0},
    {OPS_BITS_16, 0x0016, 0x05FF, 0},
    /*// CSI Lane Enable*/
    {OPS_BITS_32, 0x0140, 0x00000000, 0},
    {OPS_BITS_32, 0x0144, 0x00000000, 0},
    {OPS_BITS_32, 0x0148, 0x00000000, 0},
    {OPS_BITS_32, 0x014C, 0x00000000, 0},
    {OPS_BITS_32, 0x0150, 0x00000000, 0},
    /*// CSI Transition Timing*/
    {OPS_BITS_32, 0x0210, 0x00000FA0, 0},
    {OPS_BITS_32, 0x0214, 0x00000004, 0},
    {OPS_BITS_32, 0x0218, 0x00001503, 0},
    {OPS_BITS_32, 0x021C, 0x00000001, 0},
    {OPS_BITS_32, 0x0220, 0x00000103, 0},
    {OPS_BITS_32, 0x0224, 0x00003A98, 0},
    {OPS_BITS_32, 0x0228, 0x00000008, 0},
    {OPS_BITS_32, 0x022C, 0x00000002, 0},
    {OPS_BITS_32, 0x0230, 0x00000005, 0},
    {OPS_BITS_32, 0x0234, 0x0000001F, 0},
    {OPS_BITS_32, 0x0238, 0x00000001, 0},
    {OPS_BITS_32, 0x0204, 0x00000001, 0},
    {OPS_BITS_32, 0x0518, 0x00000001, 0},
    {OPS_BITS_32, 0x0500, 0xA3008086, 0},
    /*// HDMI Interrupt Mask*/
    {OPS_BITS_8, 0x8502, 0x01, 0},
    {OPS_BITS_8, 0x8512, 0xFE, 0},
    {OPS_BITS_8, 0x8514, 0xFF, 0},
    {OPS_BITS_8, 0x8515, 0xFF, 0},
    {OPS_BITS_8, 0x8516, 0xFF, 0},
    /*// HDMI Audio REFCLK*/
    {OPS_BITS_8, 0x8531, 0x01, 0},
    {OPS_BITS_8, 0x8540, 0x8C, 0},
    {OPS_BITS_8, 0x8541, 0x0A, 0},
    {OPS_BITS_8, 0x8630, 0xB0, 0},
    {OPS_BITS_8, 0x8631, 0x1E, 0},
    {OPS_BITS_8, 0x8632, 0x04, 0},
    {OPS_BITS_8, 0x8670, 0x01, 0},
    /*// NCO_48F0A*/
    /*// NCO_48F0B*/
    /*// NCO_48F0C*/
    /*// NCO_48F0D*/
    /*// NCO_44F0A*/
    /*// NCO_44F0B*/
    /*// NCO_44F0C*/
    /*// NCO_44F0D*/
    /*// HDMI PHY*/
    {OPS_BITS_8, 0x8532, 0x80, 0},
    {OPS_BITS_8, 0x8536, 0x40, 0},
    {OPS_BITS_8, 0x853F, 0x0A, 0},
    /*// HDMI SYSTEM*/
    {OPS_BITS_8, 0x8543, 0x32, 0},
    {OPS_BITS_8, 0x8544, 0x10, 0},
    {OPS_BITS_8, 0x8545, 0x31, 0},
    {OPS_BITS_8, 0x8546, 0x2D, 0},
    /*// EDID*/
    {OPS_BITS_8, 0x85C7, 0x01, 0},
    {OPS_BITS_8, 0x85CA, 0x00, 0},
    {OPS_BITS_8, 0x85CB, 0x01, 0},
    /*// EDID Data*/
    {OPS_BITS_8, 0x8C00, 0x00, 0},
    {OPS_BITS_8, 0x8C01, 0xFF, 0},
    {OPS_BITS_8, 0x8C02, 0xFF, 0},
    {OPS_BITS_8, 0x8C03, 0xFF, 0},
    {OPS_BITS_8, 0x8C04, 0xFF, 0},
    {OPS_BITS_8, 0x8C05, 0xFF, 0},
    {OPS_BITS_8, 0x8C06, 0xFF, 0},
    {OPS_BITS_8, 0x8C07, 0x00, 0},
    {OPS_BITS_8, 0x8C08, 0x52, 0},
    {OPS_BITS_8, 0x8C09, 0x62, 0},
    {OPS_BITS_8, 0x8C0A, 0x2A, 0},
    {OPS_BITS_8, 0x8C0B, 0x2C, 0},
    {OPS_BITS_8, 0x8C0C, 0x00, 0},
    {OPS_BITS_8, 0x8C0D, 0x00, 0},
    {OPS_BITS_8, 0x8C0E, 0x00, 0},
    {OPS_BITS_8, 0x8C0F, 0x00, 0},
    {OPS_BITS_8, 0x8C10, 0x2C, 0},
    {OPS_BITS_8, 0x8C11, 0x18, 0},
    {OPS_BITS_8, 0x8C12, 0x01, 0},
    {OPS_BITS_8, 0x8C13, 0x03, 0},
    {OPS_BITS_8, 0x8C14, 0x80, 0},
    {OPS_BITS_8, 0x8C15, 0x00, 0},
    {OPS_BITS_8, 0x8C16, 0x00, 0},
    {OPS_BITS_8, 0x8C17, 0x78, 0},
    {OPS_BITS_8, 0x8C18, 0x0A, 0},
    {OPS_BITS_8, 0x8C19, 0xDA, 0},
    {OPS_BITS_8, 0x8C1A, 0xFF, 0},
    {OPS_BITS_8, 0x8C1B, 0xA3, 0},
    {OPS_BITS_8, 0x8C1C, 0x58, 0},
    {OPS_BITS_8, 0x8C1D, 0x4A, 0},
    {OPS_BITS_8, 0x8C1E, 0xA2, 0},
    {OPS_BITS_8, 0x8C1F, 0x29, 0},
    {OPS_BITS_8, 0x8C20, 0x17, 0},
    {OPS_BITS_8, 0x8C21, 0x49, 0},
    {OPS_BITS_8, 0x8C22, 0x4B, 0},
    {OPS_BITS_8, 0x8C23, 0x00, 0},
    {OPS_BITS_8, 0x8C24, 0x00, 0},
    {OPS_BITS_8, 0x8C25, 0x00, 0},
    {OPS_BITS_8, 0x8C26, 0x01, 0},
    {OPS_BITS_8, 0x8C27, 0x01, 0},
    {OPS_BITS_8, 0x8C28, 0x01, 0},
    {OPS_BITS_8, 0x8C29, 0x01, 0},
    {OPS_BITS_8, 0x8C2A, 0x01, 0},
    {OPS_BITS_8, 0x8C2B, 0x01, 0},
    {OPS_BITS_8, 0x8C2C, 0x01, 0},
    {OPS_BITS_8, 0x8C2D, 0x01, 0},
    {OPS_BITS_8, 0x8C2E, 0x01, 0},
    {OPS_BITS_8, 0x8C2F, 0x01, 0},
    {OPS_BITS_8, 0x8C30, 0x01, 0},
    {OPS_BITS_8, 0x8C31, 0x01, 0},
    {OPS_BITS_8, 0x8C32, 0x01, 0},
    {OPS_BITS_8, 0x8C33, 0x01, 0},
    {OPS_BITS_8, 0x8C34, 0x01, 0},
    {OPS_BITS_8, 0x8C35, 0x01, 0},
    {OPS_BITS_8, 0x8C36, 0x01, 0},
    {OPS_BITS_8, 0x8C37, 0x1D, 0},
    {OPS_BITS_8, 0x8C38, 0x00, 0},
    {OPS_BITS_8, 0x8C39, 0x72, 0},
    {OPS_BITS_8, 0x8C3A, 0x51, 0},
    {OPS_BITS_8, 0x8C3B, 0xD0, 0},
    {OPS_BITS_8, 0x8C3C, 0x1E, 0},
    {OPS_BITS_8, 0x8C3D, 0x20, 0},
    {OPS_BITS_8, 0x8C3E, 0x6E, 0},
    {OPS_BITS_8, 0x8C3F, 0x28, 0},
    {OPS_BITS_8, 0x8C40, 0x55, 0},
    {OPS_BITS_8, 0x8C41, 0x00, 0},
    {OPS_BITS_8, 0x8C42, 0xD9, 0},
    {OPS_BITS_8, 0x8C43, 0x28, 0},
    {OPS_BITS_8, 0x8C44, 0x11, 0},
    {OPS_BITS_8, 0x8C45, 0x00, 0},
    {OPS_BITS_8, 0x8C46, 0x00, 0},
    {OPS_BITS_8, 0x8C47, 0x1E, 0},
    {OPS_BITS_8, 0x8C48, 0x8C, 0},
    {OPS_BITS_8, 0x8C49, 0x0A, 0},
    {OPS_BITS_8, 0x8C4A, 0xD0, 0},
    {OPS_BITS_8, 0x8C4B, 0x8A, 0},
    {OPS_BITS_8, 0x8C4C, 0x20, 0},
    {OPS_BITS_8, 0x8C4D, 0xE0, 0},
    {OPS_BITS_8, 0x8C4E, 0x2D, 0},
    {OPS_BITS_8, 0x8C4F, 0x10, 0},
    {OPS_BITS_8, 0x8C50, 0x10, 0},
    {OPS_BITS_8, 0x8C51, 0x3E, 0},
    {OPS_BITS_8, 0x8C52, 0x96, 0},
    {OPS_BITS_8, 0x8C53, 0x00, 0},
    {OPS_BITS_8, 0x8C54, 0x13, 0},
    {OPS_BITS_8, 0x8C55, 0x8E, 0},
    {OPS_BITS_8, 0x8C56, 0x21, 0},
    {OPS_BITS_8, 0x8C57, 0x00, 0},
    {OPS_BITS_8, 0x8C58, 0x00, 0},
    {OPS_BITS_8, 0x8C59, 0x1E, 0},
    {OPS_BITS_8, 0x8C5A, 0x00, 0},
    {OPS_BITS_8, 0x8C5B, 0x00, 0},
    {OPS_BITS_8, 0x8C5C, 0x00, 0},
    {OPS_BITS_8, 0x8C5D, 0xFC, 0},
    {OPS_BITS_8, 0x8C5E, 0x00, 0},
    {OPS_BITS_8, 0x8C5F, 0x4D, 0},
    {OPS_BITS_8, 0x8C60, 0x69, 0},
    {OPS_BITS_8, 0x8C61, 0x6E, 0},
    {OPS_BITS_8, 0x8C62, 0x64, 0},
    {OPS_BITS_8, 0x8C63, 0x72, 0},
    {OPS_BITS_8, 0x8C64, 0x61, 0},
    {OPS_BITS_8, 0x8C65, 0x79, 0},
    {OPS_BITS_8, 0x8C66, 0x20, 0},
    {OPS_BITS_8, 0x8C67, 0x69, 0},
    {OPS_BITS_8, 0x8C68, 0x50, 0},
    {OPS_BITS_8, 0x8C69, 0x43, 0},
    {OPS_BITS_8, 0x8C6A, 0x32, 0},
    {OPS_BITS_8, 0x8C6B, 0x32, 0},
    {OPS_BITS_8, 0x8C6C, 0x00, 0},
    {OPS_BITS_8, 0x8C6D, 0x00, 0},
    {OPS_BITS_8, 0x8C6E, 0x00, 0},
    {OPS_BITS_8, 0x8C6F, 0xFD, 0},
    {OPS_BITS_8, 0x8C70, 0x00, 0},
    {OPS_BITS_8, 0x8C71, 0x17, 0},
    {OPS_BITS_8, 0x8C72, 0x3D, 0},
    {OPS_BITS_8, 0x8C73, 0x0F, 0},
    {OPS_BITS_8, 0x8C74, 0x8C, 0},
    {OPS_BITS_8, 0x8C75, 0x17, 0},
    {OPS_BITS_8, 0x8C76, 0x00, 0},
    {OPS_BITS_8, 0x8C77, 0x0A, 0},
    {OPS_BITS_8, 0x8C78, 0x20, 0},
    {OPS_BITS_8, 0x8C79, 0x20, 0},
    {OPS_BITS_8, 0x8C7A, 0x20, 0},
    {OPS_BITS_8, 0x8C7B, 0x20, 0},
    {OPS_BITS_8, 0x8C7C, 0x20, 0},
    {OPS_BITS_8, 0x8C7D, 0x20, 0},
    {OPS_BITS_8, 0x8C7E, 0x01, 0},
    {OPS_BITS_8, 0x8C7F, 0xF5, 0},
    {OPS_BITS_8, 0x8C80, 0x02, 0},
    {OPS_BITS_8, 0x8C81, 0x03, 0},
    {OPS_BITS_8, 0x8C82, 0x17, 0},
    {OPS_BITS_8, 0x8C83, 0x74, 0},
    {OPS_BITS_8, 0x8C84, 0x47, 0},
    {OPS_BITS_8, 0x8C85, 0x84, 0},
    {OPS_BITS_8, 0x8C86, 0x02, 0},
    {OPS_BITS_8, 0x8C87, 0x01, 0},
    {OPS_BITS_8, 0x8C88, 0x02, 0},
    {OPS_BITS_8, 0x8C89, 0x02, 0},
    {OPS_BITS_8, 0x8C8A, 0x02, 0},
    {OPS_BITS_8, 0x8C8B, 0x02, 0},
    {OPS_BITS_8, 0x8C8C, 0x23, 0},
    {OPS_BITS_8, 0x8C8D, 0x09, 0},
    {OPS_BITS_8, 0x8C8E, 0x07, 0},
    {OPS_BITS_8, 0x8C8F, 0x01, 0},
    {OPS_BITS_8, 0x8C90, 0x66, 0},
    {OPS_BITS_8, 0x8C91, 0x03, 0},
    {OPS_BITS_8, 0x8C92, 0x0C, 0},
    {OPS_BITS_8, 0x8C93, 0x00, 0},
    {OPS_BITS_8, 0x8C94, 0x30, 0},
    {OPS_BITS_8, 0x8C95, 0x00, 0},
    {OPS_BITS_8, 0x8C96, 0x80, 0},
    {OPS_BITS_8, 0x8C97, 0x8C, 0},
    {OPS_BITS_8, 0x8C98, 0x0A, 0},
    {OPS_BITS_8, 0x8C99, 0xD0, 0},
    {OPS_BITS_8, 0x8C9A, 0xD8, 0},
    {OPS_BITS_8, 0x8C9B, 0x09, 0},
    {OPS_BITS_8, 0x8C9C, 0x80, 0},
    {OPS_BITS_8, 0x8C9D, 0xA0, 0},
    {OPS_BITS_8, 0x8C9E, 0x20, 0},
    {OPS_BITS_8, 0x8C9F, 0xE0, 0},
    {OPS_BITS_8, 0x8CA0, 0x2D, 0},
    {OPS_BITS_8, 0x8CA1, 0x10, 0},
    {OPS_BITS_8, 0x8CA2, 0x10, 0},
    {OPS_BITS_8, 0x8CA3, 0x60, 0},
    {OPS_BITS_8, 0x8CA4, 0xA2, 0},
    {OPS_BITS_8, 0x8CA5, 0x00, 0},
    {OPS_BITS_8, 0x8CA6, 0xC4, 0},
    {OPS_BITS_8, 0x8CA7, 0x8E, 0},
    {OPS_BITS_8, 0x8CA8, 0x21, 0},
    {OPS_BITS_8, 0x8CA9, 0x00, 0},
    {OPS_BITS_8, 0x8CAA, 0x00, 0},
    {OPS_BITS_8, 0x8CAB, 0x1E, 0},
    {OPS_BITS_8, 0x8CAC, 0x8C, 0},
    {OPS_BITS_8, 0x8CAD, 0x0A, 0},
    {OPS_BITS_8, 0x8CAE, 0xD0, 0},
    {OPS_BITS_8, 0x8CAF, 0x8A, 0},
    {OPS_BITS_8, 0x8CB0, 0x20, 0},
    {OPS_BITS_8, 0x8CB1, 0xE0, 0},
    {OPS_BITS_8, 0x8CB2, 0x2D, 0},
    {OPS_BITS_8, 0x8CB3, 0x10, 0},
    {OPS_BITS_8, 0x8CB4, 0x10, 0},
    {OPS_BITS_8, 0x8CB5, 0x3E, 0},
    {OPS_BITS_8, 0x8CB6, 0x96, 0},
    {OPS_BITS_8, 0x8CB7, 0x00, 0},
    {OPS_BITS_8, 0x8CB8, 0x13, 0},
    {OPS_BITS_8, 0x8CB9, 0x8E, 0},
    {OPS_BITS_8, 0x8CBA, 0x21, 0},
    {OPS_BITS_8, 0x8CBB, 0x00, 0},
    {OPS_BITS_8, 0x8CBC, 0x00, 0},
    {OPS_BITS_8, 0x8CBD, 0x1E, 0},
    {OPS_BITS_8, 0x8CBE, 0x8C, 0},
    {OPS_BITS_8, 0x8CBF, 0x0A, 0},
    {OPS_BITS_8, 0x8CC0, 0xD0, 0},
    {OPS_BITS_8, 0x8CC1, 0x8A, 0},
    {OPS_BITS_8, 0x8CC2, 0x20, 0},
    {OPS_BITS_8, 0x8CC3, 0xE0, 0},
    {OPS_BITS_8, 0x8CC4, 0x2D, 0},
    {OPS_BITS_8, 0x8CC5, 0x10, 0},
    {OPS_BITS_8, 0x8CC6, 0x10, 0},
    {OPS_BITS_8, 0x8CC7, 0x3E, 0},
    {OPS_BITS_8, 0x8CC8, 0x96, 0},
    {OPS_BITS_8, 0x8CC9, 0x00, 0},
    {OPS_BITS_8, 0x8CCA, 0x13, 0},
    {OPS_BITS_8, 0x8CCB, 0x8E, 0},
    {OPS_BITS_8, 0x8CCC, 0x21, 0},
    {OPS_BITS_8, 0x8CCD, 0x00, 0},
    {OPS_BITS_8, 0x8CCE, 0x00, 0},
    {OPS_BITS_8, 0x8CCF, 0x1E, 0},
    {OPS_BITS_8, 0x8CD0, 0x8C, 0},
    {OPS_BITS_8, 0x8CD1, 0x0A, 0},
    {OPS_BITS_8, 0x8CD2, 0xD0, 0},
    {OPS_BITS_8, 0x8CD3, 0x8A, 0},
    {OPS_BITS_8, 0x8CD4, 0x20, 0},
    {OPS_BITS_8, 0x8CD5, 0xE0, 0},
    {OPS_BITS_8, 0x8CD6, 0x2D, 0},
    {OPS_BITS_8, 0x8CD7, 0x10, 0},
    {OPS_BITS_8, 0x8CD8, 0x10, 0},
    {OPS_BITS_8, 0x8CD9, 0x3E, 0},
    {OPS_BITS_8, 0x8CDA, 0x96, 0},
    {OPS_BITS_8, 0x8CDB, 0x00, 0},
    {OPS_BITS_8, 0x8CDC, 0x13, 0},
    {OPS_BITS_8, 0x8CDD, 0x8E, 0},
    {OPS_BITS_8, 0x8CDE, 0x21, 0},
    {OPS_BITS_8, 0x8CDF, 0x00, 0},
    {OPS_BITS_8, 0x8CE0, 0x00, 0},
    {OPS_BITS_8, 0x8CE1, 0x1E, 0},
    {OPS_BITS_8, 0x8CE2, 0x00, 0},
    {OPS_BITS_8, 0x8CE3, 0x00, 0},
    {OPS_BITS_8, 0x8CE4, 0x00, 0},
    {OPS_BITS_8, 0x8CE5, 0x00, 0},
    {OPS_BITS_8, 0x8CE6, 0x00, 0},
    {OPS_BITS_8, 0x8CE7, 0x00, 0},
    {OPS_BITS_8, 0x8CE8, 0x00, 0},
    {OPS_BITS_8, 0x8CE9, 0x00, 0},
    {OPS_BITS_8, 0x8CEA, 0x00, 0},
    {OPS_BITS_8, 0x8CEB, 0x00, 0},
    {OPS_BITS_8, 0x8CEC, 0x00, 0},
    {OPS_BITS_8, 0x8CED, 0x00, 0},
    {OPS_BITS_8, 0x8CEE, 0x00, 0},
    {OPS_BITS_8, 0x8CEF, 0x00, 0},
    {OPS_BITS_8, 0x8CF0, 0x00, 0},
    {OPS_BITS_8, 0x8CF1, 0x00, 0},
    {OPS_BITS_8, 0x8CF2, 0x00, 0},
    {OPS_BITS_8, 0x8CF3, 0x00, 0},
    {OPS_BITS_8, 0x8CF4, 0x00, 0},
    {OPS_BITS_8, 0x8CF5, 0x00, 0},
    {OPS_BITS_8, 0x8CF6, 0x00, 0},
    {OPS_BITS_8, 0x8CF7, 0x00, 0},
    {OPS_BITS_8, 0x8CF8, 0x00, 0},
    {OPS_BITS_8, 0x8CF9, 0x00, 0},
    {OPS_BITS_8, 0x8CFA, 0x00, 0},
    {OPS_BITS_8, 0x8CFB, 0x00, 0},
    {OPS_BITS_8, 0x8CFC, 0x00, 0},
    {OPS_BITS_8, 0x8CFD, 0x00, 0},
    {OPS_BITS_8, 0x8CFE, 0x00, 0},
    {OPS_BITS_8, 0x8CFF, 0x27, 0},
    /*// HDCP Setting*/
    {OPS_BITS_8, 0x85D1, 0x01, 0},
    {OPS_BITS_8, 0x8560, 0x24, 0},
    {OPS_BITS_8, 0x8563, 0x11, 0},
    {OPS_BITS_8, 0x8564, 0x0F, 0},
    /*// Video Setting*/
    {OPS_BITS_8, 0x8573, 0x80, 0},
    /*// HDMI Audio Setting*/
    {OPS_BITS_8, 0x8600, 0x00, 0},
    {OPS_BITS_8, 0x8602, 0xF3, 0},
    {OPS_BITS_8, 0x8603, 0x02, 0},
    {OPS_BITS_8, 0x8604, 0x0C, 0},
    {OPS_BITS_8, 0x8606, 0x05, 0},
    {OPS_BITS_8, 0x8607, 0x00, 0},
    {OPS_BITS_8, 0x8620, 0x22, 0},
    {OPS_BITS_8, 0x8640, 0x01, 0},
    {OPS_BITS_8, 0x8641, 0x65, 0},
    {OPS_BITS_8, 0x8642, 0x07, 0},
    {OPS_BITS_8, 0x8652, 0x02, 0},
    {OPS_BITS_8, 0x8665, 0x10, 0},
    /*// Info Frame Extraction*/
    {OPS_BITS_8, 0x8709, 0xFF, 0},
    {OPS_BITS_8, 0x870B, 0x2C, 0},
    {OPS_BITS_8, 0x870C, 0x53, 0},
    {OPS_BITS_8, 0x870D, 0x01, 0},
    {OPS_BITS_8, 0x870E, 0x30, 0},
    {OPS_BITS_8, 0x9007, 0x10, 0},
    {OPS_BITS_8, 0x854A, 0x01, 0},
    {OPS_BITS_16, 0x0004, 0x0C17, 0}            // 0x0CD7 for yuv422p 0x0C17 for rgb24
};

H2C_IPC_INIT_CODE hdmi_init_cfg_1024x768[] = {  //1024*768
    /*// Software Reset*/       
    {OPS_BITS_16, 0x0002, 0x0F00, 10},      
    {OPS_BITS_16, 0x0002, 0x0000, 0},
    /*// FIFO Delay Setting*/       
    {OPS_BITS_16, 0x0006, 0x0154, 0},
    /*// PLL Setting*/      
    {OPS_BITS_16, 0x0020, 0x3057, 0},       
    {OPS_BITS_16, 0x0022, 0x0203, 10},
    /*Waitx1us(10);*/       
    {OPS_BITS_16, 0x0022, 0x0213, 0},
    /*// Interrupt Control*/        
    {OPS_BITS_16, 0x0014, 0x0000, 0},       
    {OPS_BITS_16, 0x0016, 0x05FF, 0},
    /*// CSI Lane Enable*/      
    {OPS_BITS_32, 0x0140, 0x00000000, 0},       
    {OPS_BITS_32, 0x0144, 0x00000000, 0},       
    {OPS_BITS_32, 0x0148, 0x00000000, 0},       
    {OPS_BITS_32, 0x014C, 0x00000000, 0},       
    {OPS_BITS_32, 0x0150, 0x00000000, 0},
    /*// CSI Transition Timing*/        
    {OPS_BITS_32, 0x0210, 0x00000FA0, 0},       
    {OPS_BITS_32, 0x0214, 0x00000004, 0},       
    {OPS_BITS_32, 0x0218, 0x00001503, 0},       
    {OPS_BITS_32, 0x021C, 0x00000001, 0},       
    {OPS_BITS_32, 0x0220, 0x00000103, 0},       
    {OPS_BITS_32, 0x0224, 0x00003A98, 0},       
    {OPS_BITS_32, 0x0228, 0x00000008, 0},       
    {OPS_BITS_32, 0x022C, 0x00000002, 0},       
    {OPS_BITS_32, 0x0230, 0x00000005, 0},       
    {OPS_BITS_32, 0x0234, 0x0000001F, 0},       
    {OPS_BITS_32, 0x0238, 0x00000001, 0},       
    {OPS_BITS_32, 0x0204, 0x00000001, 0},       
    {OPS_BITS_32, 0x0518, 0x00000001, 0},       
    {OPS_BITS_32, 0x0500, 0xA3008086, 0},
    /*// HDMI Interrupt Mask*/      
    {OPS_BITS_8, 0x8502, 0x01, 0},      
    {OPS_BITS_8, 0x8512, 0xFE, 0},      
    {OPS_BITS_8, 0x8514, 0xFF, 0},      
    {OPS_BITS_8, 0x8515, 0xFF, 0},      
    {OPS_BITS_8, 0x8516, 0xFF, 0},
    /*// HDMI Audio REFCLK*/        
    {OPS_BITS_8, 0x8531, 0x01, 0},      
    {OPS_BITS_8, 0x8540, 0x8C, 0},      
    {OPS_BITS_8, 0x8541, 0x0A, 0},      
    {OPS_BITS_8, 0x8630, 0xB0, 0},      
    {OPS_BITS_8, 0x8631, 0x1E, 0},      
    {OPS_BITS_8, 0x8632, 0x04, 0},      
    {OPS_BITS_8, 0x8670, 0x01, 0},
    /*// NCO_48F0A*/
    /*// NCO_48F0B*/
    /*// NCO_48F0C*/
    /*// NCO_48F0D*/
    /*// NCO_44F0A*/
    /*// NCO_44F0B*/
    /*// NCO_44F0C*/
    /*// NCO_44F0D*/
    /*// HDMI PHY*/     
    {OPS_BITS_8, 0x8532, 0x80, 0},      
    {OPS_BITS_8, 0x8536, 0x40, 0},      
    {OPS_BITS_8, 0x853F, 0x0A, 0},
    /*// HDMI SYSTEM*/      
    {OPS_BITS_8, 0x8543, 0x32, 0},      
    {OPS_BITS_8, 0x8544, 0x10, 0},      
    {OPS_BITS_8, 0x8545, 0x31, 0},      
    {OPS_BITS_8, 0x8546, 0x2D, 0},
    /*// EDID*/     
    {OPS_BITS_8, 0x85C7, 0x01, 0},      
    {OPS_BITS_8, 0x85CA, 0x00, 0},      
    {OPS_BITS_8, 0x85CB, 0x01, 0},
    /*// EDID Data*/        
    {OPS_BITS_8, 0x8C00, 0x00, 0},      
    {OPS_BITS_8, 0x8C01, 0xFF, 0},      
    {OPS_BITS_8, 0x8C02, 0xFF, 0},      
    {OPS_BITS_8, 0x8C03, 0xFF, 0},      
    {OPS_BITS_8, 0x8C04, 0xFF, 0},      
    {OPS_BITS_8, 0x8C05, 0xFF, 0},      
    {OPS_BITS_8, 0x8C06, 0xFF, 0},      
    {OPS_BITS_8, 0x8C07, 0x00, 0},      
    {OPS_BITS_8, 0x8C08, 0x52, 0},      
    {OPS_BITS_8, 0x8C09, 0x62, 0},      
    {OPS_BITS_8, 0x8C0A, 0x2A, 0},      
    {OPS_BITS_8, 0x8C0B, 0x2C, 0},      
    {OPS_BITS_8, 0x8C0C, 0x00, 0},      
    {OPS_BITS_8, 0x8C0D, 0x00, 0},      
    {OPS_BITS_8, 0x8C0E, 0x00, 0},      
    {OPS_BITS_8, 0x8C0F, 0x00, 0},      
    {OPS_BITS_8, 0x8C10, 0x2C, 0},      
    {OPS_BITS_8, 0x8C11, 0x18, 0},      
    {OPS_BITS_8, 0x8C12, 0x01, 0},      
    {OPS_BITS_8, 0x8C13, 0x03, 0},      
    {OPS_BITS_8, 0x8C14, 0x80, 0},      
    {OPS_BITS_8, 0x8C15, 0x00, 0},      
    {OPS_BITS_8, 0x8C16, 0x00, 0},      
    {OPS_BITS_8, 0x8C17, 0x78, 0},      
    {OPS_BITS_8, 0x8C18, 0x0A, 0},      
    {OPS_BITS_8, 0x8C19, 0xDA, 0},      
    {OPS_BITS_8, 0x8C1A, 0xFF, 0},      
    {OPS_BITS_8, 0x8C1B, 0xA3, 0},      
    {OPS_BITS_8, 0x8C1C, 0x58, 0},      
    {OPS_BITS_8, 0x8C1D, 0x4A, 0},      
    {OPS_BITS_8, 0x8C1E, 0xA2, 0},      
    {OPS_BITS_8, 0x8C1F, 0x29, 0},      
    {OPS_BITS_8, 0x8C20, 0x17, 0},      
    {OPS_BITS_8, 0x8C21, 0x49, 0},      
    {OPS_BITS_8, 0x8C22, 0x4B, 0},      
    {OPS_BITS_8, 0x8C23, 0x00, 0},      
    {OPS_BITS_8, 0x8C24, 0x00, 0},      
    {OPS_BITS_8, 0x8C25, 0x00, 0},      
    {OPS_BITS_8, 0x8C26, 0x01, 0},      
    {OPS_BITS_8, 0x8C27, 0x01, 0},      
    {OPS_BITS_8, 0x8C28, 0x01, 0},      
    {OPS_BITS_8, 0x8C29, 0x01, 0},      
    {OPS_BITS_8, 0x8C2A, 0x01, 0},      
    {OPS_BITS_8, 0x8C2B, 0x01, 0},      
    {OPS_BITS_8, 0x8C2C, 0x01, 0},      
    {OPS_BITS_8, 0x8C2D, 0x01, 0},      
    {OPS_BITS_8, 0x8C2E, 0x01, 0},      
    {OPS_BITS_8, 0x8C2F, 0x01, 0},      
    {OPS_BITS_8, 0x8C30, 0x01, 0},      
    {OPS_BITS_8, 0x8C31, 0x01, 0},      
    {OPS_BITS_8, 0x8C32, 0x01, 0},      
    {OPS_BITS_8, 0x8C33, 0x01, 0},      
    {OPS_BITS_8, 0x8C34, 0x01, 0},      
    {OPS_BITS_8, 0x8C35, 0x01, 0},      
    {OPS_BITS_8, 0x8C36, 0x64, 0},      
    {OPS_BITS_8, 0x8C37, 0x19, 0},      
    {OPS_BITS_8, 0x8C38, 0x00, 0},      
    {OPS_BITS_8, 0x8C39, 0x40, 0},      
    {OPS_BITS_8, 0x8C3A, 0x41, 0},      
    {OPS_BITS_8, 0x8C3B, 0x00, 0},      
    {OPS_BITS_8, 0x8C3C, 0x26, 0},      
    {OPS_BITS_8, 0x8C3D, 0x30, 0},      
    {OPS_BITS_8, 0x8C3E, 0x18, 0},      
    {OPS_BITS_8, 0x8C3F, 0x88, 0},      
    {OPS_BITS_8, 0x8C40, 0x36, 0},      
    {OPS_BITS_8, 0x8C41, 0x00, 0},      
    {OPS_BITS_8, 0x8C42, 0xD9, 0},      
    {OPS_BITS_8, 0x8C43, 0x28, 0},      
    {OPS_BITS_8, 0x8C44, 0x11, 0},      
    {OPS_BITS_8, 0x8C45, 0x00, 0},      
    {OPS_BITS_8, 0x8C46, 0x00, 0},      
    {OPS_BITS_8, 0x8C47, 0x1E, 0},      
    {OPS_BITS_8, 0x8C48, 0x8C, 0},      
    {OPS_BITS_8, 0x8C49, 0x0A, 0},      
    {OPS_BITS_8, 0x8C4A, 0xD0, 0},      
    {OPS_BITS_8, 0x8C4B, 0x8A, 0},      
    {OPS_BITS_8, 0x8C4C, 0x20, 0},      
    {OPS_BITS_8, 0x8C4D, 0xE0, 0},      
    {OPS_BITS_8, 0x8C4E, 0x2D, 0},      
    {OPS_BITS_8, 0x8C4F, 0x10, 0},      
    {OPS_BITS_8, 0x8C50, 0x10, 0},      
    {OPS_BITS_8, 0x8C51, 0x3E, 0},      
    {OPS_BITS_8, 0x8C52, 0x96, 0},      
    {OPS_BITS_8, 0x8C53, 0x00, 0},      
    {OPS_BITS_8, 0x8C54, 0x13, 0},      
    {OPS_BITS_8, 0x8C55, 0x8E, 0},      
    {OPS_BITS_8, 0x8C56, 0x21, 0},      
    {OPS_BITS_8, 0x8C57, 0x00, 0},      
    {OPS_BITS_8, 0x8C58, 0x00, 0},      
    {OPS_BITS_8, 0x8C59, 0x1E, 0},      
    {OPS_BITS_8, 0x8C5A, 0x00, 0},      
    {OPS_BITS_8, 0x8C5B, 0x00, 0},      
    {OPS_BITS_8, 0x8C5C, 0x00, 0},      
    {OPS_BITS_8, 0x8C5D, 0xFC, 0},      
    {OPS_BITS_8, 0x8C5E, 0x00, 0},      
    {OPS_BITS_8, 0x8C5F, 0x4D, 0},      
    {OPS_BITS_8, 0x8C60, 0x69, 0},      
    {OPS_BITS_8, 0x8C61, 0x6E, 0},      
    {OPS_BITS_8, 0x8C62, 0x64, 0},      
    {OPS_BITS_8, 0x8C63, 0x72, 0},      
    {OPS_BITS_8, 0x8C64, 0x61, 0},      
    {OPS_BITS_8, 0x8C65, 0x79, 0},      
    {OPS_BITS_8, 0x8C66, 0x20, 0},      
    {OPS_BITS_8, 0x8C67, 0x69, 0},      
    {OPS_BITS_8, 0x8C68, 0x50, 0},      
    {OPS_BITS_8, 0x8C69, 0x43, 0},      
    {OPS_BITS_8, 0x8C6A, 0x32, 0},      
    {OPS_BITS_8, 0x8C6B, 0x32, 0},      
    {OPS_BITS_8, 0x8C6C, 0x00, 0},      
    {OPS_BITS_8, 0x8C6D, 0x00, 0},      
    {OPS_BITS_8, 0x8C6E, 0x00, 0},      
    {OPS_BITS_8, 0x8C6F, 0xFD, 0},      
    {OPS_BITS_8, 0x8C70, 0x00, 0},      
    {OPS_BITS_8, 0x8C71, 0x17, 0},      
    {OPS_BITS_8, 0x8C72, 0x3D, 0},      
    {OPS_BITS_8, 0x8C73, 0x0F, 0},      
    {OPS_BITS_8, 0x8C74, 0x8C, 0},      
    {OPS_BITS_8, 0x8C75, 0x17, 0},      
    {OPS_BITS_8, 0x8C76, 0x00, 0},      
    {OPS_BITS_8, 0x8C77, 0x0A, 0},      
    {OPS_BITS_8, 0x8C78, 0x20, 0},      
    {OPS_BITS_8, 0x8C79, 0x20, 0},      
    {OPS_BITS_8, 0x8C7A, 0x20, 0},      
    {OPS_BITS_8, 0x8C7B, 0x20, 0},      
    {OPS_BITS_8, 0x8C7C, 0x20, 0},      
    {OPS_BITS_8, 0x8C7D, 0x20, 0},      
    {OPS_BITS_8, 0x8C7E, 0x01, 0},      
    {OPS_BITS_8, 0x8C7F, 0xA5, 0},      
    {OPS_BITS_8, 0x8C80, 0x02, 0},      
    {OPS_BITS_8, 0x8C81, 0x03, 0},      
    {OPS_BITS_8, 0x8C82, 0x17, 0},      
    {OPS_BITS_8, 0x8C83, 0x74, 0},      
    {OPS_BITS_8, 0x8C84, 0x47, 0},      
    {OPS_BITS_8, 0x8C85, 0x84, 0},      
    {OPS_BITS_8, 0x8C86, 0x02, 0},      
    {OPS_BITS_8, 0x8C87, 0x01, 0},      
    {OPS_BITS_8, 0x8C88, 0x02, 0},      
    {OPS_BITS_8, 0x8C89, 0x02, 0},      
    {OPS_BITS_8, 0x8C8A, 0x02, 0},      
    {OPS_BITS_8, 0x8C8B, 0x02, 0},      
    {OPS_BITS_8, 0x8C8C, 0x23, 0},      
    {OPS_BITS_8, 0x8C8D, 0x09, 0},      
    {OPS_BITS_8, 0x8C8E, 0x07, 0},      
    {OPS_BITS_8, 0x8C8F, 0x01, 0},      
    {OPS_BITS_8, 0x8C90, 0x66, 0},      
    {OPS_BITS_8, 0x8C91, 0x03, 0},      
    {OPS_BITS_8, 0x8C92, 0x0C, 0},      
    {OPS_BITS_8, 0x8C93, 0x00, 0},      
    {OPS_BITS_8, 0x8C94, 0x30, 0},      
    {OPS_BITS_8, 0x8C95, 0x00, 0},      
    {OPS_BITS_8, 0x8C96, 0x80, 0},      
    {OPS_BITS_8, 0x8C97, 0x8C, 0},      
    {OPS_BITS_8, 0x8C98, 0x0A, 0},      
    {OPS_BITS_8, 0x8C99, 0xD0, 0},      
    {OPS_BITS_8, 0x8C9A, 0xD8, 0},      
    {OPS_BITS_8, 0x8C9B, 0x09, 0},      
    {OPS_BITS_8, 0x8C9C, 0x80, 0},      
    {OPS_BITS_8, 0x8C9D, 0xA0, 0},      
    {OPS_BITS_8, 0x8C9E, 0x20, 0},      
    {OPS_BITS_8, 0x8C9F, 0xE0, 0},      
    {OPS_BITS_8, 0x8CA0, 0x2D, 0},      
    {OPS_BITS_8, 0x8CA1, 0x10, 0},      
    {OPS_BITS_8, 0x8CA2, 0x10, 0},      
    {OPS_BITS_8, 0x8CA3, 0x60, 0},      
    {OPS_BITS_8, 0x8CA4, 0xA2, 0},      
    {OPS_BITS_8, 0x8CA5, 0x00, 0},      
    {OPS_BITS_8, 0x8CA6, 0xC4, 0},      
    {OPS_BITS_8, 0x8CA7, 0x8E, 0},      
    {OPS_BITS_8, 0x8CA8, 0x21, 0},      
    {OPS_BITS_8, 0x8CA9, 0x00, 0},      
    {OPS_BITS_8, 0x8CAA, 0x00, 0},      
    {OPS_BITS_8, 0x8CAB, 0x1E, 0},      
    {OPS_BITS_8, 0x8CAC, 0x8C, 0},      
    {OPS_BITS_8, 0x8CAD, 0x0A, 0},      
    {OPS_BITS_8, 0x8CAE, 0xD0, 0},      
    {OPS_BITS_8, 0x8CAF, 0x8A, 0},      
    {OPS_BITS_8, 0x8CB0, 0x20, 0},      
    {OPS_BITS_8, 0x8CB1, 0xE0, 0},      
    {OPS_BITS_8, 0x8CB2, 0x2D, 0},      
    {OPS_BITS_8, 0x8CB3, 0x10, 0},      
    {OPS_BITS_8, 0x8CB4, 0x10, 0},      
    {OPS_BITS_8, 0x8CB5, 0x3E, 0},      
    {OPS_BITS_8, 0x8CB6, 0x96, 0},      
    {OPS_BITS_8, 0x8CB7, 0x00, 0},      
    {OPS_BITS_8, 0x8CB8, 0x13, 0},      
    {OPS_BITS_8, 0x8CB9, 0x8E, 0},      
    {OPS_BITS_8, 0x8CBA, 0x21, 0},      
    {OPS_BITS_8, 0x8CBB, 0x00, 0},      
    {OPS_BITS_8, 0x8CBC, 0x00, 0},      
    {OPS_BITS_8, 0x8CBD, 0x1E, 0},      
    {OPS_BITS_8, 0x8CBE, 0x8C, 0},      
    {OPS_BITS_8, 0x8CBF, 0x0A, 0},      
    {OPS_BITS_8, 0x8CC0, 0xD0, 0},      
    {OPS_BITS_8, 0x8CC1, 0x8A, 0},      
    {OPS_BITS_8, 0x8CC2, 0x20, 0},      
    {OPS_BITS_8, 0x8CC3, 0xE0, 0},      
    {OPS_BITS_8, 0x8CC4, 0x2D, 0},      
    {OPS_BITS_8, 0x8CC5, 0x10, 0},      
    {OPS_BITS_8, 0x8CC6, 0x10, 0},      
    {OPS_BITS_8, 0x8CC7, 0x3E, 0},      
    {OPS_BITS_8, 0x8CC8, 0x96, 0},      
    {OPS_BITS_8, 0x8CC9, 0x00, 0},      
    {OPS_BITS_8, 0x8CCA, 0x13, 0},      
    {OPS_BITS_8, 0x8CCB, 0x8E, 0},      
    {OPS_BITS_8, 0x8CCC, 0x21, 0},      
    {OPS_BITS_8, 0x8CCD, 0x00, 0},      
    {OPS_BITS_8, 0x8CCE, 0x00, 0},      
    {OPS_BITS_8, 0x8CCF, 0x1E, 0},      
    {OPS_BITS_8, 0x8CD0, 0x8C, 0},      
    {OPS_BITS_8, 0x8CD1, 0x0A, 0},      
    {OPS_BITS_8, 0x8CD2, 0xD0, 0},      
    {OPS_BITS_8, 0x8CD3, 0x8A, 0},      
    {OPS_BITS_8, 0x8CD4, 0x20, 0},      
    {OPS_BITS_8, 0x8CD5, 0xE0, 0},      
    {OPS_BITS_8, 0x8CD6, 0x2D, 0},      
    {OPS_BITS_8, 0x8CD7, 0x10, 0},      
    {OPS_BITS_8, 0x8CD8, 0x10, 0},      
    {OPS_BITS_8, 0x8CD9, 0x3E, 0},      
    {OPS_BITS_8, 0x8CDA, 0x96, 0},      
    {OPS_BITS_8, 0x8CDB, 0x00, 0},      
    {OPS_BITS_8, 0x8CDC, 0x13, 0},      
    {OPS_BITS_8, 0x8CDD, 0x8E, 0},      
    {OPS_BITS_8, 0x8CDE, 0x21, 0},      
    {OPS_BITS_8, 0x8CDF, 0x00, 0},      
    {OPS_BITS_8, 0x8CE0, 0x00, 0},      
    {OPS_BITS_8, 0x8CE1, 0x1E, 0},      
    {OPS_BITS_8, 0x8CE2, 0x00, 0},      
    {OPS_BITS_8, 0x8CE3, 0x00, 0},      
    {OPS_BITS_8, 0x8CE4, 0x00, 0},      
    {OPS_BITS_8, 0x8CE5, 0x00, 0},      
    {OPS_BITS_8, 0x8CE6, 0x00, 0},      
    {OPS_BITS_8, 0x8CE7, 0x00, 0},      
    {OPS_BITS_8, 0x8CE8, 0x00, 0},      
    {OPS_BITS_8, 0x8CE9, 0x00, 0},      
    {OPS_BITS_8, 0x8CEA, 0x00, 0},      
    {OPS_BITS_8, 0x8CEB, 0x00, 0},      
    {OPS_BITS_8, 0x8CEC, 0x00, 0},      
    {OPS_BITS_8, 0x8CED, 0x00, 0},      
    {OPS_BITS_8, 0x8CEE, 0x00, 0},      
    {OPS_BITS_8, 0x8CEF, 0x00, 0},      
    {OPS_BITS_8, 0x8CF0, 0x00, 0},      
    {OPS_BITS_8, 0x8CF1, 0x00, 0},      
    {OPS_BITS_8, 0x8CF2, 0x00, 0},      
    {OPS_BITS_8, 0x8CF3, 0x00, 0},      
    {OPS_BITS_8, 0x8CF4, 0x00, 0},      
    {OPS_BITS_8, 0x8CF5, 0x00, 0},      
    {OPS_BITS_8, 0x8CF6, 0x00, 0},      
    {OPS_BITS_8, 0x8CF7, 0x00, 0},      
    {OPS_BITS_8, 0x8CF8, 0x00, 0},      
    {OPS_BITS_8, 0x8CF9, 0x00, 0},      
    {OPS_BITS_8, 0x8CFA, 0x00, 0},      
    {OPS_BITS_8, 0x8CFB, 0x00, 0},      
    {OPS_BITS_8, 0x8CFC, 0x00, 0},      
    {OPS_BITS_8, 0x8CFD, 0x00, 0},      
    {OPS_BITS_8, 0x8CFE, 0x00, 0},      
    {OPS_BITS_8, 0x8CFF, 0x27, 0},
    /*// HDCP Setting*/     
    {OPS_BITS_8, 0x85D1, 0x01, 0},      
    {OPS_BITS_8, 0x8560, 0x24, 0},      
    {OPS_BITS_8, 0x8563, 0x11, 0},      
    {OPS_BITS_8, 0x8564, 0x0F, 0},
    /*// Video Setting*/        
    {OPS_BITS_8, 0x8573, 0x80, 0},   /* 0x81 for YUV422, 0x80 for RGB24 */
    /*// HDMI Audio Setting*/       
    {OPS_BITS_8, 0x8600, 0x00, 0},      
    {OPS_BITS_8, 0x8602, 0xF3, 0},      
    {OPS_BITS_8, 0x8603, 0x02, 0},      
    {OPS_BITS_8, 0x8604, 0x0C, 0},      
    {OPS_BITS_8, 0x8606, 0x05, 0},      
    {OPS_BITS_8, 0x8607, 0x00, 0},      
    {OPS_BITS_8, 0x8620, 0x22, 0},      
    {OPS_BITS_8, 0x8640, 0x01, 0},      
    {OPS_BITS_8, 0x8641, 0x65, 0},      
    {OPS_BITS_8, 0x8642, 0x07, 0},      
    {OPS_BITS_8, 0x8652, 0x02, 0},      
    {OPS_BITS_8, 0x8665, 0x10, 0},
    /*// Info Frame Extraction*/        
    {OPS_BITS_8, 0x8709, 0xFF, 0},      
    {OPS_BITS_8, 0x870B, 0x2C, 0},      
    {OPS_BITS_8, 0x870C, 0x53, 0},      
    {OPS_BITS_8, 0x870D, 0x01, 0},      
    {OPS_BITS_8, 0x870E, 0x30, 0},      
    {OPS_BITS_8, 0x9007, 0x10, 0},      
    {OPS_BITS_8, 0x854A, 0x01, 0},      
    {OPS_BITS_16, 0x0004, 0x0C17, 0}   /* 0x0CD7 for YUV422, 0x0C17 for RGB24 */
};    

H2C_IPC_INIT_CODE hdmi_init_cfg_1680x1050[] = { //1680*1050
    {OPS_BITS_16, 0x0002, 0x0F00, 10},
    {OPS_BITS_16, 0x0002, 0x0000, 0},
    /*// FIFO Delay Setting*/
    {OPS_BITS_16, 0x0006, 0x0154, 0},
    /*// PLL Setting*/
    {OPS_BITS_16, 0x0020, 0x306B, 0},
    {OPS_BITS_16, 0x0022, 0x0203, 10},
    /*Waitx1us(10);*/
    {OPS_BITS_16, 0x0022, 0x0213, 0},
    /*// Interrupt Control*/
    {OPS_BITS_16, 0x0014, 0x0000, 0},
    {OPS_BITS_16, 0x0016, 0x05FF, 0},
    /*// CSI Lane Enable*/
    {OPS_BITS_32, 0x0140, 0x00000000, 0},
    {OPS_BITS_32, 0x0144, 0x00000000, 0},
    {OPS_BITS_32, 0x0148, 0x00000000, 0},
    {OPS_BITS_32, 0x014C, 0x00000000, 0},
    {OPS_BITS_32, 0x0150, 0x00000000, 0},
    /*// CSI Transition Timing*/
    {OPS_BITS_32, 0x0210, 0x00001388, 0},
    {OPS_BITS_32, 0x0214, 0x00000004, 0},
    {OPS_BITS_32, 0x0218, 0x00001A03, 0},
    {OPS_BITS_32, 0x021C, 0x00000001, 0},
    {OPS_BITS_32, 0x0220, 0x00000404, 0},
    {OPS_BITS_32, 0x0224, 0x00004E20, 0},
    {OPS_BITS_32, 0x0228, 0x00000008, 0},
    {OPS_BITS_32, 0x022C, 0x00000003, 0},
    {OPS_BITS_32, 0x0230, 0x00000005, 0},
    {OPS_BITS_32, 0x0234, 0x0000001F, 0},
    {OPS_BITS_32, 0x0238, 0x00000001, 0},
    {OPS_BITS_32, 0x0204, 0x00000001, 0},
    {OPS_BITS_32, 0x0518, 0x00000001, 0},
    {OPS_BITS_32, 0x0500, 0xA3008086, 0},
    /*// HDMI Interrupt Mask*/
    {OPS_BITS_8, 0x8502, 0x01, 0},
    {OPS_BITS_8, 0x8512, 0xFE, 0},
    {OPS_BITS_8, 0x8514, 0xFF, 0},
    {OPS_BITS_8, 0x8515, 0xFF, 0},
    {OPS_BITS_8, 0x8516, 0xFF, 0},
    /*// HDMI Audio REFCLK*/
    {OPS_BITS_8, 0x8531, 0x01, 0},
    {OPS_BITS_8, 0x8540, 0x8C, 0},
    {OPS_BITS_8, 0x8541, 0x0A, 0},
    {OPS_BITS_8, 0x8630, 0xB0, 0},
    {OPS_BITS_8, 0x8631, 0x1E, 0},
    {OPS_BITS_8, 0x8632, 0x04, 0},
    {OPS_BITS_8, 0x8670, 0x01, 0},
    /*// NCO_48F0A*/
    /*// NCO_48F0B*/
    /*// NCO_48F0C*/
    /*// NCO_48F0D*/
    /*// NCO_44F0A*/
    /*// NCO_44F0B*/
    /*// NCO_44F0C*/
    /*// NCO_44F0D*/
    /*// HDMI PHY*/
    {OPS_BITS_8, 0x8532, 0x80, 0},
    {OPS_BITS_8, 0x8536, 0x40, 0},
    {OPS_BITS_8, 0x853F, 0x0A, 0},
    /*// HDMI SYSTEM*/
    {OPS_BITS_8, 0x8543, 0x32, 0},
    {OPS_BITS_8, 0x8544, 0x10, 0},
    {OPS_BITS_8, 0x8545, 0x31, 0},
    {OPS_BITS_8, 0x8546, 0x2D, 0},
    /*// EDID*/
    {OPS_BITS_8, 0x85C7, 0x01, 0},
    {OPS_BITS_8, 0x85CA, 0x00, 0},
    {OPS_BITS_8, 0x85CB, 0x01, 0},
    /*// EDID Data*/
    {OPS_BITS_8, 0x8C00, 0x00, 0},
    {OPS_BITS_8, 0x8C01, 0xFF, 0},
    {OPS_BITS_8, 0x8C02, 0xFF, 0},
    {OPS_BITS_8, 0x8C03, 0xFF, 0},
    {OPS_BITS_8, 0x8C04, 0xFF, 0},
    {OPS_BITS_8, 0x8C05, 0xFF, 0},
    {OPS_BITS_8, 0x8C06, 0xFF, 0},
    {OPS_BITS_8, 0x8C07, 0x00, 0},
    {OPS_BITS_8, 0x8C08, 0x52, 0},
    {OPS_BITS_8, 0x8C09, 0x62, 0},
    {OPS_BITS_8, 0x8C0A, 0x2A, 0},
    {OPS_BITS_8, 0x8C0B, 0x2C, 0},
    {OPS_BITS_8, 0x8C0C, 0x00, 0},
    {OPS_BITS_8, 0x8C0D, 0x00, 0},
    {OPS_BITS_8, 0x8C0E, 0x00, 0},
    {OPS_BITS_8, 0x8C0F, 0x00, 0},
    {OPS_BITS_8, 0x8C10, 0x2C, 0},
    {OPS_BITS_8, 0x8C11, 0x18, 0},
    {OPS_BITS_8, 0x8C12, 0x01, 0},
    {OPS_BITS_8, 0x8C13, 0x03, 0},
    {OPS_BITS_8, 0x8C14, 0x80, 0},
    {OPS_BITS_8, 0x8C15, 0x00, 0},
    {OPS_BITS_8, 0x8C16, 0x00, 0},
    {OPS_BITS_8, 0x8C17, 0x78, 0},
    {OPS_BITS_8, 0x8C18, 0x0A, 0},
    {OPS_BITS_8, 0x8C19, 0xDA, 0},
    {OPS_BITS_8, 0x8C1A, 0xFF, 0},
    {OPS_BITS_8, 0x8C1B, 0xA3, 0},
    {OPS_BITS_8, 0x8C1C, 0x58, 0},
    {OPS_BITS_8, 0x8C1D, 0x4A, 0},
    {OPS_BITS_8, 0x8C1E, 0xA2, 0},
    {OPS_BITS_8, 0x8C1F, 0x29, 0},
    {OPS_BITS_8, 0x8C20, 0x17, 0},
    {OPS_BITS_8, 0x8C21, 0x49, 0},
    {OPS_BITS_8, 0x8C22, 0x4B, 0},
    {OPS_BITS_8, 0x8C23, 0x00, 0},
    {OPS_BITS_8, 0x8C24, 0x00, 0},
    {OPS_BITS_8, 0x8C25, 0x00, 0},
    {OPS_BITS_8, 0x8C26, 0x01, 0},
    {OPS_BITS_8, 0x8C27, 0x01, 0},
    {OPS_BITS_8, 0x8C28, 0x01, 0},
    {OPS_BITS_8, 0x8C29, 0x01, 0},
    {OPS_BITS_8, 0x8C2A, 0x01, 0},
    {OPS_BITS_8, 0x8C2B, 0x01, 0},
    {OPS_BITS_8, 0x8C2C, 0x01, 0},
    {OPS_BITS_8, 0x8C2D, 0x01, 0},
    {OPS_BITS_8, 0x8C2E, 0x01, 0},
    {OPS_BITS_8, 0x8C2F, 0x01, 0},
    {OPS_BITS_8, 0x8C30, 0x01, 0},
    {OPS_BITS_8, 0x8C31, 0x01, 0},
    {OPS_BITS_8, 0x8C32, 0x01, 0},
    {OPS_BITS_8, 0x8C33, 0x01, 0},
    {OPS_BITS_8, 0x8C34, 0x01, 0},
    {OPS_BITS_8, 0x8C35, 0x01, 0},
    {OPS_BITS_8, 0x8C36, 0xC8, 0},
    {OPS_BITS_8, 0x8C37, 0x32, 0},
    {OPS_BITS_8, 0x8C38, 0x90, 0},
    {OPS_BITS_8, 0x8C39, 0x2C, 0},
    {OPS_BITS_8, 0x8C3A, 0x61, 0},
    {OPS_BITS_8, 0x8C3B, 0x1A, 0},
    {OPS_BITS_8, 0x8C3C, 0x26, 0},
    {OPS_BITS_8, 0x8C3D, 0x40, 0},
    {OPS_BITS_8, 0x8C3E, 0x50, 0},
    {OPS_BITS_8, 0x8C3F, 0x50, 0},
    {OPS_BITS_8, 0x8C40, 0x66, 0},
    {OPS_BITS_8, 0x8C41, 0x00, 0},
    {OPS_BITS_8, 0x8C42, 0xD9, 0},
    {OPS_BITS_8, 0x8C43, 0x28, 0},
    {OPS_BITS_8, 0x8C44, 0x11, 0},
    {OPS_BITS_8, 0x8C45, 0x00, 0},
    {OPS_BITS_8, 0x8C46, 0x00, 0},
    {OPS_BITS_8, 0x8C47, 0x1E, 0},
    {OPS_BITS_8, 0x8C48, 0x8C, 0},
    {OPS_BITS_8, 0x8C49, 0x0A, 0},
    {OPS_BITS_8, 0x8C4A, 0xD0, 0},
    {OPS_BITS_8, 0x8C4B, 0x8A, 0},
    {OPS_BITS_8, 0x8C4C, 0x20, 0},
    {OPS_BITS_8, 0x8C4D, 0xE0, 0},
    {OPS_BITS_8, 0x8C4E, 0x2D, 0},
    {OPS_BITS_8, 0x8C4F, 0x10, 0},
    {OPS_BITS_8, 0x8C50, 0x10, 0},
    {OPS_BITS_8, 0x8C51, 0x3E, 0},
    {OPS_BITS_8, 0x8C52, 0x96, 0},
    {OPS_BITS_8, 0x8C53, 0x00, 0},
    {OPS_BITS_8, 0x8C54, 0x13, 0},
    {OPS_BITS_8, 0x8C55, 0x8E, 0},
    {OPS_BITS_8, 0x8C56, 0x21, 0},
    {OPS_BITS_8, 0x8C57, 0x00, 0},
    {OPS_BITS_8, 0x8C58, 0x00, 0},
    {OPS_BITS_8, 0x8C59, 0x1E, 0},
    {OPS_BITS_8, 0x8C5A, 0x00, 0},
    {OPS_BITS_8, 0x8C5B, 0x00, 0},
    {OPS_BITS_8, 0x8C5C, 0x00, 0},
    {OPS_BITS_8, 0x8C5D, 0xFC, 0},
    {OPS_BITS_8, 0x8C5E, 0x00, 0},
    {OPS_BITS_8, 0x8C5F, 0x4D, 0},
    {OPS_BITS_8, 0x8C60, 0x69, 0},
    {OPS_BITS_8, 0x8C61, 0x6E, 0},
    {OPS_BITS_8, 0x8C62, 0x64, 0},
    {OPS_BITS_8, 0x8C63, 0x72, 0},
    {OPS_BITS_8, 0x8C64, 0x61, 0},
    {OPS_BITS_8, 0x8C65, 0x79, 0},
    {OPS_BITS_8, 0x8C66, 0x20, 0},
    {OPS_BITS_8, 0x8C67, 0x69, 0},
    {OPS_BITS_8, 0x8C68, 0x50, 0},
    {OPS_BITS_8, 0x8C69, 0x43, 0},
    {OPS_BITS_8, 0x8C6A, 0x32, 0},
    {OPS_BITS_8, 0x8C6B, 0x32, 0},
    {OPS_BITS_8, 0x8C6C, 0x00, 0},
    {OPS_BITS_8, 0x8C6D, 0x00, 0},
    {OPS_BITS_8, 0x8C6E, 0x00, 0},
    {OPS_BITS_8, 0x8C6F, 0xFD, 0},
    {OPS_BITS_8, 0x8C70, 0x00, 0},
    {OPS_BITS_8, 0x8C71, 0x17, 0},
    {OPS_BITS_8, 0x8C72, 0x3D, 0},
    {OPS_BITS_8, 0x8C73, 0x0F, 0},
    {OPS_BITS_8, 0x8C74, 0x8C, 0},
    {OPS_BITS_8, 0x8C75, 0x17, 0},
    {OPS_BITS_8, 0x8C76, 0x00, 0},
    {OPS_BITS_8, 0x8C77, 0x0A, 0},
    {OPS_BITS_8, 0x8C78, 0x20, 0},
    {OPS_BITS_8, 0x8C79, 0x20, 0},
    {OPS_BITS_8, 0x8C7A, 0x20, 0},
    {OPS_BITS_8, 0x8C7B, 0x20, 0},
    {OPS_BITS_8, 0x8C7C, 0x20, 0},
    {OPS_BITS_8, 0x8C7D, 0x20, 0},
    {OPS_BITS_8, 0x8C7E, 0x01, 0},
    {OPS_BITS_8, 0x8C7F, 0x32, 0},
    {OPS_BITS_8, 0x8C80, 0x02, 0},
    {OPS_BITS_8, 0x8C81, 0x03, 0},
    {OPS_BITS_8, 0x8C82, 0x17, 0},
    {OPS_BITS_8, 0x8C83, 0x74, 0},
    {OPS_BITS_8, 0x8C84, 0x47, 0},
    {OPS_BITS_8, 0x8C85, 0x84, 0},
    {OPS_BITS_8, 0x8C86, 0x02, 0},
    {OPS_BITS_8, 0x8C87, 0x01, 0},
    {OPS_BITS_8, 0x8C88, 0x02, 0},
    {OPS_BITS_8, 0x8C89, 0x02, 0},
    {OPS_BITS_8, 0x8C8A, 0x02, 0},
    {OPS_BITS_8, 0x8C8B, 0x02, 0},
    {OPS_BITS_8, 0x8C8C, 0x23, 0},
    {OPS_BITS_8, 0x8C8D, 0x09, 0},
    {OPS_BITS_8, 0x8C8E, 0x07, 0},
    {OPS_BITS_8, 0x8C8F, 0x01, 0},
    {OPS_BITS_8, 0x8C90, 0x66, 0},
    {OPS_BITS_8, 0x8C91, 0x03, 0},
    {OPS_BITS_8, 0x8C92, 0x0C, 0},
    {OPS_BITS_8, 0x8C93, 0x00, 0},
    {OPS_BITS_8, 0x8C94, 0x30, 0},
    {OPS_BITS_8, 0x8C95, 0x00, 0},
    {OPS_BITS_8, 0x8C96, 0x80, 0},
    {OPS_BITS_8, 0x8C97, 0x8C, 0},
    {OPS_BITS_8, 0x8C98, 0x0A, 0},
    {OPS_BITS_8, 0x8C99, 0xD0, 0},
    {OPS_BITS_8, 0x8C9A, 0xD8, 0},
    {OPS_BITS_8, 0x8C9B, 0x09, 0},
    {OPS_BITS_8, 0x8C9C, 0x80, 0},
    {OPS_BITS_8, 0x8C9D, 0xA0, 0},
    {OPS_BITS_8, 0x8C9E, 0x20, 0},
    {OPS_BITS_8, 0x8C9F, 0xE0, 0},
    {OPS_BITS_8, 0x8CA0, 0x2D, 0},
    {OPS_BITS_8, 0x8CA1, 0x10, 0},
    {OPS_BITS_8, 0x8CA2, 0x10, 0},
    {OPS_BITS_8, 0x8CA3, 0x60, 0},
    {OPS_BITS_8, 0x8CA4, 0xA2, 0},
    {OPS_BITS_8, 0x8CA5, 0x00, 0},
    {OPS_BITS_8, 0x8CA6, 0xC4, 0},
    {OPS_BITS_8, 0x8CA7, 0x8E, 0},
    {OPS_BITS_8, 0x8CA8, 0x21, 0},
    {OPS_BITS_8, 0x8CA9, 0x00, 0},
    {OPS_BITS_8, 0x8CAA, 0x00, 0},
    {OPS_BITS_8, 0x8CAB, 0x1E, 0},
    {OPS_BITS_8, 0x8CAC, 0x8C, 0},
    {OPS_BITS_8, 0x8CAD, 0x0A, 0},
    {OPS_BITS_8, 0x8CAE, 0xD0, 0},
    {OPS_BITS_8, 0x8CAF, 0x8A, 0},
    {OPS_BITS_8, 0x8CB0, 0x20, 0},
    {OPS_BITS_8, 0x8CB1, 0xE0, 0},
    {OPS_BITS_8, 0x8CB2, 0x2D, 0},
    {OPS_BITS_8, 0x8CB3, 0x10, 0},
    {OPS_BITS_8, 0x8CB4, 0x10, 0},
    {OPS_BITS_8, 0x8CB5, 0x3E, 0},
    {OPS_BITS_8, 0x8CB6, 0x96, 0},
    {OPS_BITS_8, 0x8CB7, 0x00, 0},
    {OPS_BITS_8, 0x8CB8, 0x13, 0},
    {OPS_BITS_8, 0x8CB9, 0x8E, 0},
    {OPS_BITS_8, 0x8CBA, 0x21, 0},
    {OPS_BITS_8, 0x8CBB, 0x00, 0},
    {OPS_BITS_8, 0x8CBC, 0x00, 0},
    {OPS_BITS_8, 0x8CBD, 0x1E, 0},
    {OPS_BITS_8, 0x8CBE, 0x8C, 0},
    {OPS_BITS_8, 0x8CBF, 0x0A, 0},
    {OPS_BITS_8, 0x8CC0, 0xD0, 0},
    {OPS_BITS_8, 0x8CC1, 0x8A, 0},
    {OPS_BITS_8, 0x8CC2, 0x20, 0},
    {OPS_BITS_8, 0x8CC3, 0xE0, 0},
    {OPS_BITS_8, 0x8CC4, 0x2D, 0},
    {OPS_BITS_8, 0x8CC5, 0x10, 0},
    {OPS_BITS_8, 0x8CC6, 0x10, 0},
    {OPS_BITS_8, 0x8CC7, 0x3E, 0},
    {OPS_BITS_8, 0x8CC8, 0x96, 0},
    {OPS_BITS_8, 0x8CC9, 0x00, 0},
    {OPS_BITS_8, 0x8CCA, 0x13, 0},
    {OPS_BITS_8, 0x8CCB, 0x8E, 0},
    {OPS_BITS_8, 0x8CCC, 0x21, 0},
    {OPS_BITS_8, 0x8CCD, 0x00, 0},
    {OPS_BITS_8, 0x8CCE, 0x00, 0},
    {OPS_BITS_8, 0x8CCF, 0x1E, 0},
    {OPS_BITS_8, 0x8CD0, 0x8C, 0},
    {OPS_BITS_8, 0x8CD1, 0x0A, 0},
    {OPS_BITS_8, 0x8CD2, 0xD0, 0},
    {OPS_BITS_8, 0x8CD3, 0x8A, 0},
    {OPS_BITS_8, 0x8CD4, 0x20, 0},
    {OPS_BITS_8, 0x8CD5, 0xE0, 0},
    {OPS_BITS_8, 0x8CD6, 0x2D, 0},
    {OPS_BITS_8, 0x8CD7, 0x10, 0},
    {OPS_BITS_8, 0x8CD8, 0x10, 0},
    {OPS_BITS_8, 0x8CD9, 0x3E, 0},
    {OPS_BITS_8, 0x8CDA, 0x96, 0},
    {OPS_BITS_8, 0x8CDB, 0x00, 0},
    {OPS_BITS_8, 0x8CDC, 0x13, 0},
    {OPS_BITS_8, 0x8CDD, 0x8E, 0},
    {OPS_BITS_8, 0x8CDE, 0x21, 0},
    {OPS_BITS_8, 0x8CDF, 0x00, 0},
    {OPS_BITS_8, 0x8CE0, 0x00, 0},
    {OPS_BITS_8, 0x8CE1, 0x1E, 0},
    {OPS_BITS_8, 0x8CE2, 0x00, 0},
    {OPS_BITS_8, 0x8CE3, 0x00, 0},
    {OPS_BITS_8, 0x8CE4, 0x00, 0},
    {OPS_BITS_8, 0x8CE5, 0x00, 0},
    {OPS_BITS_8, 0x8CE6, 0x00, 0},
    {OPS_BITS_8, 0x8CE7, 0x00, 0},
    {OPS_BITS_8, 0x8CE8, 0x00, 0},
    {OPS_BITS_8, 0x8CE9, 0x00, 0},
    {OPS_BITS_8, 0x8CEA, 0x00, 0},
    {OPS_BITS_8, 0x8CEB, 0x00, 0},
    {OPS_BITS_8, 0x8CEC, 0x00, 0},
    {OPS_BITS_8, 0x8CED, 0x00, 0},
    {OPS_BITS_8, 0x8CEE, 0x00, 0},
    {OPS_BITS_8, 0x8CEF, 0x00, 0},
    {OPS_BITS_8, 0x8CF0, 0x00, 0},
    {OPS_BITS_8, 0x8CF1, 0x00, 0},
    {OPS_BITS_8, 0x8CF2, 0x00, 0},
    {OPS_BITS_8, 0x8CF3, 0x00, 0},
    {OPS_BITS_8, 0x8CF4, 0x00, 0},
    {OPS_BITS_8, 0x8CF5, 0x00, 0},
    {OPS_BITS_8, 0x8CF6, 0x00, 0},
    {OPS_BITS_8, 0x8CF7, 0x00, 0},
    {OPS_BITS_8, 0x8CF8, 0x00, 0},
    {OPS_BITS_8, 0x8CF9, 0x00, 0},
    {OPS_BITS_8, 0x8CFA, 0x00, 0},
    {OPS_BITS_8, 0x8CFB, 0x00, 0},
    {OPS_BITS_8, 0x8CFC, 0x00, 0},
    {OPS_BITS_8, 0x8CFD, 0x00, 0},
    {OPS_BITS_8, 0x8CFE, 0x00, 0},
    {OPS_BITS_8, 0x8CFF, 0x27, 0},
    /*// HDCP Setting*/
    {OPS_BITS_8, 0x85D1, 0x01, 0},
    {OPS_BITS_8, 0x8560, 0x24, 0},
    {OPS_BITS_8, 0x8563, 0x11, 0},
    {OPS_BITS_8, 0x8564, 0x0F, 0},
    /*// Video Setting*/
    {OPS_BITS_8, 0x8573, 0x80, 0},			/* 0x81 for yuv422, 0x80 for rgb */
    /*// HDMI Audio Setting*/
    {OPS_BITS_8, 0x8600, 0x00, 0},
    {OPS_BITS_8, 0x8602, 0xF3, 0},
    {OPS_BITS_8, 0x8603, 0x02, 0},
    {OPS_BITS_8, 0x8604, 0x0C, 0},
    {OPS_BITS_8, 0x8606, 0x05, 0},
    {OPS_BITS_8, 0x8607, 0x00, 0},
    {OPS_BITS_8, 0x8620, 0x22, 0},
    {OPS_BITS_8, 0x8640, 0x01, 0},
    {OPS_BITS_8, 0x8641, 0x65, 0},
    {OPS_BITS_8, 0x8642, 0x07, 0},
    {OPS_BITS_8, 0x8652, 0x02, 0},
    {OPS_BITS_8, 0x8665, 0x10, 0},
    /*// Info Frame Extraction*/
    {OPS_BITS_8, 0x8709, 0xFF, 0},
    {OPS_BITS_8, 0x870B, 0x2C, 0},
    {OPS_BITS_8, 0x870C, 0x53, 0},
    {OPS_BITS_8, 0x870D, 0x01, 0},
    {OPS_BITS_8, 0x870E, 0x30, 0},
    {OPS_BITS_8, 0x9007, 0x10, 0},
    {OPS_BITS_8, 0x854A, 0x01, 0},
    {OPS_BITS_16, 0x0004, 0x0C17, 0}			/* 0x0CD7 for YUV422, 0x0C17 for RGB24 */
};

H2C_IPC_INIT_CODE hdmi_init_cfg_1280x800[] = {  //1280*800
    /*// Software Rese*/
    {OPS_BITS_16, 0x0002, 0x0F00, 10},
    {OPS_BITS_16, 0x0002, 0x0000, 0},
    /*// FIFO Delay Settin*/
    {OPS_BITS_16, 0x0006, 0x0154, 0},
    /*// PLL Settin*/
    {OPS_BITS_16, 0x0020, 0x3057, 0},
    {OPS_BITS_16, 0x0022, 0x0203, 10},
    /*Waitx1us(10)*/
    {OPS_BITS_16, 0x0022, 0x0213, 0},
    /*// Interrupt Contro*/
    {OPS_BITS_16, 0x0014, 0x0000, 0},
    {OPS_BITS_16, 0x0016, 0x05FF, 0},
    /*// CSI Lane Enabl*/
    {OPS_BITS_32, 0x0140, 0x00000000, 0},
    {OPS_BITS_32, 0x0144, 0x00000000, 0},
    {OPS_BITS_32, 0x0148, 0x00000000, 0},
    {OPS_BITS_32, 0x014C, 0x00000000, 0},
    {OPS_BITS_32, 0x0150, 0x00000000, 0},
    /*// CSI Transition Timin*/
    {OPS_BITS_32, 0x0210, 0x00000FA0, 0},
    {OPS_BITS_32, 0x0214, 0x00000004, 0},
    {OPS_BITS_32, 0x0218, 0x00001503, 0},
    {OPS_BITS_32, 0x021C, 0x00000001, 0},
    {OPS_BITS_32, 0x0220, 0x00000103, 0},
    {OPS_BITS_32, 0x0224, 0x00003A98, 0},
    {OPS_BITS_32, 0x0228, 0x00000008, 0},
    {OPS_BITS_32, 0x022C, 0x00000002, 0},
    {OPS_BITS_32, 0x0230, 0x00000005, 0},
    {OPS_BITS_32, 0x0234, 0x0000001F, 0},
    {OPS_BITS_32, 0x0238, 0x00000001, 0},
    {OPS_BITS_32, 0x0204, 0x00000001, 0},
    {OPS_BITS_32, 0x0518, 0x00000001, 0},
    {OPS_BITS_32, 0x0500, 0xA3008086, 0},
    /*// HDMI Interrupt Mas*/
    {OPS_BITS_8, 0x8502, 0x01, 0},
    {OPS_BITS_8, 0x8512, 0xFE, 0},
    {OPS_BITS_8, 0x8514, 0xFF, 0},
    {OPS_BITS_8, 0x8515, 0xFF, 0},
    {OPS_BITS_8, 0x8516, 0xFF, 0},
    /*// HDMI Audio REFCL*/
    {OPS_BITS_8, 0x8531, 0x01, 0},
    {OPS_BITS_8, 0x8540, 0x8C, 0},
    {OPS_BITS_8, 0x8541, 0x0A, 0},
    {OPS_BITS_8, 0x8630, 0xB0, 0},
    {OPS_BITS_8, 0x8631, 0x1E, 0},
    {OPS_BITS_8, 0x8632, 0x04, 0},
    {OPS_BITS_8, 0x8670, 0x01, 0},
    /*// NCO_48F0*/
    /*// NCO_48F0*/
    /*// NCO_48F0*/
    /*// NCO_48F0*/
    /*// NCO_44F0*/
    /*// NCO_44F0*/
    /*// NCO_44F0*/
    /*// NCO_44F0*/
    /*// HDMI PH*/
    {OPS_BITS_8, 0x8532, 0x80, 0},
    {OPS_BITS_8, 0x8536, 0x40, 0},
    {OPS_BITS_8, 0x853F, 0x0A, 0},
    /*// HDMI SYSTE*/
    {OPS_BITS_8, 0x8543, 0x32, 0},
    {OPS_BITS_8, 0x8544, 0x10, 0},
    {OPS_BITS_8, 0x8545, 0x31, 0},
    {OPS_BITS_8, 0x8546, 0x2D, 0},
    /*// EDI*/
    {OPS_BITS_8, 0x85C7, 0x01, 0},
    {OPS_BITS_8, 0x85CA, 0x00, 0},
    {OPS_BITS_8, 0x85CB, 0x01, 0},
    /*// EDID Dat*/
    {OPS_BITS_8, 0x8C00, 0x00, 0},
    {OPS_BITS_8, 0x8C01, 0xFF, 0},
    {OPS_BITS_8, 0x8C02, 0xFF, 0},
    {OPS_BITS_8, 0x8C03, 0xFF, 0},
    {OPS_BITS_8, 0x8C04, 0xFF, 0},
    {OPS_BITS_8, 0x8C05, 0xFF, 0},
    {OPS_BITS_8, 0x8C06, 0xFF, 0},
    {OPS_BITS_8, 0x8C07, 0x00, 0},
    {OPS_BITS_8, 0x8C08, 0x52, 0},
    {OPS_BITS_8, 0x8C09, 0x62, 0},
    {OPS_BITS_8, 0x8C0A, 0x2A, 0},
    {OPS_BITS_8, 0x8C0B, 0x2C, 0},
    {OPS_BITS_8, 0x8C0C, 0x00, 0},
    {OPS_BITS_8, 0x8C0D, 0x00, 0},
    {OPS_BITS_8, 0x8C0E, 0x00, 0},
    {OPS_BITS_8, 0x8C0F, 0x00, 0},
    {OPS_BITS_8, 0x8C10, 0x2C, 0},
    {OPS_BITS_8, 0x8C11, 0x18, 0},
    {OPS_BITS_8, 0x8C12, 0x01, 0},
    {OPS_BITS_8, 0x8C13, 0x03, 0},
    {OPS_BITS_8, 0x8C14, 0x80, 0},
    {OPS_BITS_8, 0x8C15, 0x00, 0},
    {OPS_BITS_8, 0x8C16, 0x00, 0},
    {OPS_BITS_8, 0x8C17, 0x78, 0},
    {OPS_BITS_8, 0x8C18, 0x0A, 0},
    {OPS_BITS_8, 0x8C19, 0xDA, 0},
    {OPS_BITS_8, 0x8C1A, 0xFF, 0},
    {OPS_BITS_8, 0x8C1B, 0xA3, 0},
    {OPS_BITS_8, 0x8C1C, 0x58, 0},
    {OPS_BITS_8, 0x8C1D, 0x4A, 0},
    {OPS_BITS_8, 0x8C1E, 0xA2, 0},
    {OPS_BITS_8, 0x8C1F, 0x29, 0},
    {OPS_BITS_8, 0x8C20, 0x17, 0},
    {OPS_BITS_8, 0x8C21, 0x49, 0},
    {OPS_BITS_8, 0x8C22, 0x4B, 0},
    {OPS_BITS_8, 0x8C23, 0x00, 0},
    {OPS_BITS_8, 0x8C24, 0x00, 0},
    {OPS_BITS_8, 0x8C25, 0x00, 0},
    {OPS_BITS_8, 0x8C26, 0x01, 0},
    {OPS_BITS_8, 0x8C27, 0x01, 0},
    {OPS_BITS_8, 0x8C28, 0x01, 0},
    {OPS_BITS_8, 0x8C29, 0x01, 0},
    {OPS_BITS_8, 0x8C2A, 0x01, 0},
    {OPS_BITS_8, 0x8C2B, 0x01, 0},
    {OPS_BITS_8, 0x8C2C, 0x01, 0},
    {OPS_BITS_8, 0x8C2D, 0x01, 0},
    {OPS_BITS_8, 0x8C2E, 0x01, 0},
    {OPS_BITS_8, 0x8C2F, 0x01, 0},
    {OPS_BITS_8, 0x8C30, 0x01, 0},
    {OPS_BITS_8, 0x8C31, 0x01, 0},
    {OPS_BITS_8, 0x8C32, 0x01, 0},
    {OPS_BITS_8, 0x8C33, 0x01, 0},
    {OPS_BITS_8, 0x8C34, 0x01, 0},
    {OPS_BITS_8, 0x8C35, 0x01, 0},
    {OPS_BITS_8, 0x8C36, 0x40, 0},
    {OPS_BITS_8, 0x8C37, 0x1F, 0},
    {OPS_BITS_8, 0x8C38, 0x00, 0},
    {OPS_BITS_8, 0x8C39, 0x72, 0},
    {OPS_BITS_8, 0x8C3A, 0x51, 0},
    {OPS_BITS_8, 0x8C3B, 0x20, 0},
    {OPS_BITS_8, 0x8C3C, 0x12, 0},
    {OPS_BITS_8, 0x8C3D, 0x30, 0},
    {OPS_BITS_8, 0x8C3E, 0x6E, 0},
    {OPS_BITS_8, 0x8C3F, 0x28, 0},
    {OPS_BITS_8, 0x8C40, 0x24, 0},
    {OPS_BITS_8, 0x8C41, 0x00, 0},
    {OPS_BITS_8, 0x8C42, 0xD9, 0},
    {OPS_BITS_8, 0x8C43, 0x28, 0},
    {OPS_BITS_8, 0x8C44, 0x11, 0},
    {OPS_BITS_8, 0x8C45, 0x00, 0},
    {OPS_BITS_8, 0x8C46, 0x00, 0},
    {OPS_BITS_8, 0x8C47, 0x1E, 0},
    {OPS_BITS_8, 0x8C48, 0x8C, 0},
    {OPS_BITS_8, 0x8C49, 0x0A, 0},
    {OPS_BITS_8, 0x8C4A, 0xD0, 0},
    {OPS_BITS_8, 0x8C4B, 0x8A, 0},
    {OPS_BITS_8, 0x8C4C, 0x20, 0},
    {OPS_BITS_8, 0x8C4D, 0xE0, 0},
    {OPS_BITS_8, 0x8C4E, 0x2D, 0},
    {OPS_BITS_8, 0x8C4F, 0x10, 0},
    {OPS_BITS_8, 0x8C50, 0x10, 0},
    {OPS_BITS_8, 0x8C51, 0x3E, 0},
    {OPS_BITS_8, 0x8C52, 0x96, 0},
    {OPS_BITS_8, 0x8C53, 0x00, 0},
    {OPS_BITS_8, 0x8C54, 0x13, 0},
    {OPS_BITS_8, 0x8C55, 0x8E, 0},
    {OPS_BITS_8, 0x8C56, 0x21, 0},
    {OPS_BITS_8, 0x8C57, 0x00, 0},
    {OPS_BITS_8, 0x8C58, 0x00, 0},
    {OPS_BITS_8, 0x8C59, 0x1E, 0},
    {OPS_BITS_8, 0x8C5A, 0x00, 0},
    {OPS_BITS_8, 0x8C5B, 0x00, 0},
    {OPS_BITS_8, 0x8C5C, 0x00, 0},
    {OPS_BITS_8, 0x8C5D, 0xFC, 0},
    {OPS_BITS_8, 0x8C5E, 0x00, 0},
    {OPS_BITS_8, 0x8C5F, 0x4D, 0},
    {OPS_BITS_8, 0x8C60, 0x69, 0},
    {OPS_BITS_8, 0x8C61, 0x6E, 0},
    {OPS_BITS_8, 0x8C62, 0x64, 0},
    {OPS_BITS_8, 0x8C63, 0x72, 0},
    {OPS_BITS_8, 0x8C64, 0x61, 0},
    {OPS_BITS_8, 0x8C65, 0x79, 0},
    {OPS_BITS_8, 0x8C66, 0x20, 0},
    {OPS_BITS_8, 0x8C67, 0x69, 0},
    {OPS_BITS_8, 0x8C68, 0x50, 0},
    {OPS_BITS_8, 0x8C69, 0x43, 0},
    {OPS_BITS_8, 0x8C6A, 0x32, 0},
    {OPS_BITS_8, 0x8C6B, 0x32, 0},
    {OPS_BITS_8, 0x8C6C, 0x00, 0},
    {OPS_BITS_8, 0x8C6D, 0x00, 0},
    {OPS_BITS_8, 0x8C6E, 0x00, 0},
    {OPS_BITS_8, 0x8C6F, 0xFD, 0},
    {OPS_BITS_8, 0x8C70, 0x00, 0},
    {OPS_BITS_8, 0x8C71, 0x17, 0},
    {OPS_BITS_8, 0x8C72, 0x3D, 0},
    {OPS_BITS_8, 0x8C73, 0x0F, 0},
    {OPS_BITS_8, 0x8C74, 0x8C, 0},
    {OPS_BITS_8, 0x8C75, 0x17, 0},
    {OPS_BITS_8, 0x8C76, 0x00, 0},
    {OPS_BITS_8, 0x8C77, 0x0A, 0},
    {OPS_BITS_8, 0x8C78, 0x20, 0},
    {OPS_BITS_8, 0x8C79, 0x20, 0},
    {OPS_BITS_8, 0x8C7A, 0x20, 0},
    {OPS_BITS_8, 0x8C7B, 0x20, 0},
    {OPS_BITS_8, 0x8C7C, 0x20, 0},
    {OPS_BITS_8, 0x8C7D, 0x20, 0},
    {OPS_BITS_8, 0x8C7E, 0x01, 0},
    {OPS_BITS_8, 0x8C7F, 0x91, 0},
    {OPS_BITS_8, 0x8C80, 0x02, 0},
    {OPS_BITS_8, 0x8C81, 0x03, 0},
    {OPS_BITS_8, 0x8C82, 0x17, 0},
    {OPS_BITS_8, 0x8C83, 0x74, 0},
    {OPS_BITS_8, 0x8C84, 0x47, 0},
    {OPS_BITS_8, 0x8C85, 0x84, 0},
    {OPS_BITS_8, 0x8C86, 0x02, 0},
    {OPS_BITS_8, 0x8C87, 0x01, 0},
    {OPS_BITS_8, 0x8C88, 0x02, 0},
    {OPS_BITS_8, 0x8C89, 0x02, 0},
    {OPS_BITS_8, 0x8C8A, 0x02, 0},
    {OPS_BITS_8, 0x8C8B, 0x02, 0},
    {OPS_BITS_8, 0x8C8C, 0x23, 0},
    {OPS_BITS_8, 0x8C8D, 0x09, 0},
    {OPS_BITS_8, 0x8C8E, 0x07, 0},
    {OPS_BITS_8, 0x8C8F, 0x01, 0},
    {OPS_BITS_8, 0x8C90, 0x66, 0},
    {OPS_BITS_8, 0x8C91, 0x03, 0},
    {OPS_BITS_8, 0x8C92, 0x0C, 0},
    {OPS_BITS_8, 0x8C93, 0x00, 0},
    {OPS_BITS_8, 0x8C94, 0x30, 0},
    {OPS_BITS_8, 0x8C95, 0x00, 0},
    {OPS_BITS_8, 0x8C96, 0x80, 0},
    {OPS_BITS_8, 0x8C97, 0x8C, 0},
    {OPS_BITS_8, 0x8C98, 0x0A, 0},
    {OPS_BITS_8, 0x8C99, 0xD0, 0},
    {OPS_BITS_8, 0x8C9A, 0xD8, 0},
    {OPS_BITS_8, 0x8C9B, 0x09, 0},
    {OPS_BITS_8, 0x8C9C, 0x80, 0},
    {OPS_BITS_8, 0x8C9D, 0xA0, 0},
    {OPS_BITS_8, 0x8C9E, 0x20, 0},
    {OPS_BITS_8, 0x8C9F, 0xE0, 0},
    {OPS_BITS_8, 0x8CA0, 0x2D, 0},
    {OPS_BITS_8, 0x8CA1, 0x10, 0},
    {OPS_BITS_8, 0x8CA2, 0x10, 0},
    {OPS_BITS_8, 0x8CA3, 0x60, 0},
    {OPS_BITS_8, 0x8CA4, 0xA2, 0},
    {OPS_BITS_8, 0x8CA5, 0x00, 0},
    {OPS_BITS_8, 0x8CA6, 0xC4, 0},
    {OPS_BITS_8, 0x8CA7, 0x8E, 0},
    {OPS_BITS_8, 0x8CA8, 0x21, 0},
    {OPS_BITS_8, 0x8CA9, 0x00, 0},
    {OPS_BITS_8, 0x8CAA, 0x00, 0},
    {OPS_BITS_8, 0x8CAB, 0x1E, 0},
    {OPS_BITS_8, 0x8CAC, 0x8C, 0},
    {OPS_BITS_8, 0x8CAD, 0x0A, 0},
    {OPS_BITS_8, 0x8CAE, 0xD0, 0},
    {OPS_BITS_8, 0x8CAF, 0x8A, 0},
    {OPS_BITS_8, 0x8CB0, 0x20, 0},
    {OPS_BITS_8, 0x8CB1, 0xE0, 0},
    {OPS_BITS_8, 0x8CB2, 0x2D, 0},
    {OPS_BITS_8, 0x8CB3, 0x10, 0},
    {OPS_BITS_8, 0x8CB4, 0x10, 0},
    {OPS_BITS_8, 0x8CB5, 0x3E, 0},
    {OPS_BITS_8, 0x8CB6, 0x96, 0},
    {OPS_BITS_8, 0x8CB7, 0x00, 0},
    {OPS_BITS_8, 0x8CB8, 0x13, 0},
    {OPS_BITS_8, 0x8CB9, 0x8E, 0},
    {OPS_BITS_8, 0x8CBA, 0x21, 0},
    {OPS_BITS_8, 0x8CBB, 0x00, 0},
    {OPS_BITS_8, 0x8CBC, 0x00, 0},
    {OPS_BITS_8, 0x8CBD, 0x1E, 0},
    {OPS_BITS_8, 0x8CBE, 0x8C, 0},
    {OPS_BITS_8, 0x8CBF, 0x0A, 0},
    {OPS_BITS_8, 0x8CC0, 0xD0, 0},
    {OPS_BITS_8, 0x8CC1, 0x8A, 0},
    {OPS_BITS_8, 0x8CC2, 0x20, 0},
    {OPS_BITS_8, 0x8CC3, 0xE0, 0},
    {OPS_BITS_8, 0x8CC4, 0x2D, 0},
    {OPS_BITS_8, 0x8CC5, 0x10, 0},
    {OPS_BITS_8, 0x8CC6, 0x10, 0},
    {OPS_BITS_8, 0x8CC7, 0x3E, 0},
    {OPS_BITS_8, 0x8CC8, 0x96, 0},
    {OPS_BITS_8, 0x8CC9, 0x00, 0},
    {OPS_BITS_8, 0x8CCA, 0x13, 0},
    {OPS_BITS_8, 0x8CCB, 0x8E, 0},
    {OPS_BITS_8, 0x8CCC, 0x21, 0},
    {OPS_BITS_8, 0x8CCD, 0x00, 0},
    {OPS_BITS_8, 0x8CCE, 0x00, 0},
    {OPS_BITS_8, 0x8CCF, 0x1E, 0},
    {OPS_BITS_8, 0x8CD0, 0x8C, 0},
    {OPS_BITS_8, 0x8CD1, 0x0A, 0},
    {OPS_BITS_8, 0x8CD2, 0xD0, 0},
    {OPS_BITS_8, 0x8CD3, 0x8A, 0},
    {OPS_BITS_8, 0x8CD4, 0x20, 0},
    {OPS_BITS_8, 0x8CD5, 0xE0, 0},
    {OPS_BITS_8, 0x8CD6, 0x2D, 0},
    {OPS_BITS_8, 0x8CD7, 0x10, 0},
    {OPS_BITS_8, 0x8CD8, 0x10, 0},
    {OPS_BITS_8, 0x8CD9, 0x3E, 0},
    {OPS_BITS_8, 0x8CDA, 0x96, 0},
    {OPS_BITS_8, 0x8CDB, 0x00, 0},
    {OPS_BITS_8, 0x8CDC, 0x13, 0},
    {OPS_BITS_8, 0x8CDD, 0x8E, 0},
    {OPS_BITS_8, 0x8CDE, 0x21, 0},
    {OPS_BITS_8, 0x8CDF, 0x00, 0},
    {OPS_BITS_8, 0x8CE0, 0x00, 0},
    {OPS_BITS_8, 0x8CE1, 0x1E, 0},
    {OPS_BITS_8, 0x8CE2, 0x00, 0},
    {OPS_BITS_8, 0x8CE3, 0x00, 0},
    {OPS_BITS_8, 0x8CE4, 0x00, 0},
    {OPS_BITS_8, 0x8CE5, 0x00, 0},
    {OPS_BITS_8, 0x8CE6, 0x00, 0},
    {OPS_BITS_8, 0x8CE7, 0x00, 0},
    {OPS_BITS_8, 0x8CE8, 0x00, 0},
    {OPS_BITS_8, 0x8CE9, 0x00, 0},
    {OPS_BITS_8, 0x8CEA, 0x00, 0},
    {OPS_BITS_8, 0x8CEB, 0x00, 0},
    {OPS_BITS_8, 0x8CEC, 0x00, 0},
    {OPS_BITS_8, 0x8CED, 0x00, 0},
    {OPS_BITS_8, 0x8CEE, 0x00, 0},
    {OPS_BITS_8, 0x8CEF, 0x00, 0},
    {OPS_BITS_8, 0x8CF0, 0x00, 0},
    {OPS_BITS_8, 0x8CF1, 0x00, 0},
    {OPS_BITS_8, 0x8CF2, 0x00, 0},
    {OPS_BITS_8, 0x8CF3, 0x00, 0},
    {OPS_BITS_8, 0x8CF4, 0x00, 0},
    {OPS_BITS_8, 0x8CF5, 0x00, 0},
    {OPS_BITS_8, 0x8CF6, 0x00, 0},
    {OPS_BITS_8, 0x8CF7, 0x00, 0},
    {OPS_BITS_8, 0x8CF8, 0x00, 0},
    {OPS_BITS_8, 0x8CF9, 0x00, 0},
    {OPS_BITS_8, 0x8CFA, 0x00, 0},
    {OPS_BITS_8, 0x8CFB, 0x00, 0},
    {OPS_BITS_8, 0x8CFC, 0x00, 0},
    {OPS_BITS_8, 0x8CFD, 0x00, 0},
    {OPS_BITS_8, 0x8CFE, 0x00, 0},
    {OPS_BITS_8, 0x8CFF, 0x27, 0},
    /*// HDCP Settin*/
    {OPS_BITS_8, 0x85D1, 0x01, 0},
    {OPS_BITS_8, 0x8560, 0x24, 0},
    {OPS_BITS_8, 0x8563, 0x11, 0},
    {OPS_BITS_8, 0x8564, 0x0F, 0},
    /*// Video Settin*/
    {OPS_BITS_8, 0x8573, 0x80, 0},
    /*// HDMI Audio Settin*/
    {OPS_BITS_8, 0x8600, 0x00, 0},
    {OPS_BITS_8, 0x8602, 0xF3, 0},
    {OPS_BITS_8, 0x8603, 0x02, 0},
    {OPS_BITS_8, 0x8604, 0x0C, 0},
    {OPS_BITS_8, 0x8606, 0x05, 0},
    {OPS_BITS_8, 0x8607, 0x00, 0},
    {OPS_BITS_8, 0x8620, 0x22, 0},
    {OPS_BITS_8, 0x8640, 0x01, 0},
    {OPS_BITS_8, 0x8641, 0x65, 0},
    {OPS_BITS_8, 0x8642, 0x07, 0},
    {OPS_BITS_8, 0x8652, 0x02, 0},
    {OPS_BITS_8, 0x8665, 0x10, 0},
    /*// Info Frame Extractio*/
    {OPS_BITS_8, 0x8709, 0xFF, 0},
    {OPS_BITS_8, 0x870B, 0x2C, 0},
    {OPS_BITS_8, 0x870C, 0x53, 0},
    {OPS_BITS_8, 0x870D, 0x01, 0},
    {OPS_BITS_8, 0x870E, 0x30, 0},
    {OPS_BITS_8, 0x9007, 0x10, 0},
    {OPS_BITS_8, 0x854A, 0x01, 0},
    {OPS_BITS_16, 0x0004, 0x0C17, 0}
};
        
H2C_IPC_INIT_CODE hdmi_init_cfg_800x600[] = {       //800x600
    {OPS_BITS_16, 0x0002, 0x0F00, 10},
    {OPS_BITS_16, 0x0002, 0x0000, 0},
    /*// FIFO Delay Setting*/
    {OPS_BITS_16, 0x0006, 0x0168, 0},
    /*// PLL Setting*/
    {OPS_BITS_16, 0x0020, 0x3057, 0},
    {OPS_BITS_16, 0x0022, 0x0203, 10},
    /*Waitx1us(10);*/
    {OPS_BITS_16, 0x0022, 0x0213, 0},
    /*// Interrupt Control*/
    {OPS_BITS_16, 0x0014, 0x0000, 0},
    {OPS_BITS_16, 0x0016, 0x05FF, 0},
    /*// CSI Lane Enable*/
    {OPS_BITS_32, 0x0140, 0x00000000, 0},
    {OPS_BITS_32, 0x0144, 0x00000000, 0},
    {OPS_BITS_32, 0x0148, 0x00000000, 0},
    {OPS_BITS_32, 0x014C, 0x00000000, 0},
    {OPS_BITS_32, 0x0150, 0x00000000, 0},
    /*// CSI Transition Timing*/
    {OPS_BITS_32, 0x0210, 0x00000FA0, 0},
    {OPS_BITS_32, 0x0214, 0x00000004, 0},
    {OPS_BITS_32, 0x0218, 0x00001503, 0},
    {OPS_BITS_32, 0x021C, 0x00000001, 0},
    {OPS_BITS_32, 0x0220, 0x00000103, 0},
    {OPS_BITS_32, 0x0224, 0x00003A98, 0},
    {OPS_BITS_32, 0x0228, 0x00000008, 0},
    {OPS_BITS_32, 0x022C, 0x00000002, 0},
    {OPS_BITS_32, 0x0230, 0x00000005, 0},
    {OPS_BITS_32, 0x0234, 0x0000001F, 0},
    {OPS_BITS_32, 0x0238, 0x00000001, 0},
    {OPS_BITS_32, 0x0204, 0x00000001, 0},
    {OPS_BITS_32, 0x0518, 0x00000001, 0},
    {OPS_BITS_32, 0x0500, 0xA3008086, 0},
    /*// HDMI Interrupt Mask*/
    {OPS_BITS_8, 0x8502, 0x01, 0},
    {OPS_BITS_8, 0x8512, 0xFE, 0},
    {OPS_BITS_8, 0x8514, 0xFF, 0},
    {OPS_BITS_8, 0x8515, 0xFF, 0},
    {OPS_BITS_8, 0x8516, 0xFF, 0},
    /*// HDMI Audio REFCLK*/
    {OPS_BITS_8, 0x8531, 0x01, 0},
    {OPS_BITS_8, 0x8540, 0x8C, 0},
    {OPS_BITS_8, 0x8541, 0x0A, 0},
    {OPS_BITS_8, 0x8630, 0xB0, 0},
    {OPS_BITS_8, 0x8631, 0x1E, 0},
    {OPS_BITS_8, 0x8632, 0x04, 0},
    {OPS_BITS_8, 0x8670, 0x01, 0},
    /*// NCO_48F0A*/
    /*// NCO_48F0B*/
    /*// NCO_48F0C*/
    /*// NCO_48F0D*/
    /*// NCO_44F0A*/
    /*// NCO_44F0B*/
    /*// NCO_44F0C*/
    /*// NCO_44F0D*/
    /*// HDMI PHY*/
    {OPS_BITS_8, 0x8532, 0x80, 0},
    {OPS_BITS_8, 0x8536, 0x40, 0},
    {OPS_BITS_8, 0x853F, 0x0A, 0},
    /*// HDMI SYSTEM*/
    {OPS_BITS_8, 0x8543, 0x32, 0},
    {OPS_BITS_8, 0x8544, 0x10, 0},
    {OPS_BITS_8, 0x8545, 0x31, 0},
    {OPS_BITS_8, 0x8546, 0x2D, 0},
    /*// EDID*/
    {OPS_BITS_8, 0x85C7, 0x01, 0},
    {OPS_BITS_8, 0x85CA, 0x00, 0},
    {OPS_BITS_8, 0x85CB, 0x01, 0},
    /*// EDID Data*/
    {OPS_BITS_8, 0x8C00, 0x00, 0},
    {OPS_BITS_8, 0x8C01, 0xFF, 0},
    {OPS_BITS_8, 0x8C02, 0xFF, 0},
    {OPS_BITS_8, 0x8C03, 0xFF, 0},
    {OPS_BITS_8, 0x8C04, 0xFF, 0},
    {OPS_BITS_8, 0x8C05, 0xFF, 0},
    {OPS_BITS_8, 0x8C06, 0xFF, 0},
    {OPS_BITS_8, 0x8C07, 0x00, 0},
    {OPS_BITS_8, 0x8C08, 0x52, 0},
    {OPS_BITS_8, 0x8C09, 0x62, 0},
    {OPS_BITS_8, 0x8C0A, 0x2A, 0},
    {OPS_BITS_8, 0x8C0B, 0x2C, 0},
    {OPS_BITS_8, 0x8C0C, 0x00, 0},
    {OPS_BITS_8, 0x8C0D, 0x00, 0},
    {OPS_BITS_8, 0x8C0E, 0x00, 0},
    {OPS_BITS_8, 0x8C0F, 0x00, 0},
    {OPS_BITS_8, 0x8C10, 0x2C, 0},
    {OPS_BITS_8, 0x8C11, 0x18, 0},
    {OPS_BITS_8, 0x8C12, 0x01, 0},
    {OPS_BITS_8, 0x8C13, 0x03, 0},
    {OPS_BITS_8, 0x8C14, 0x80, 0},
    {OPS_BITS_8, 0x8C15, 0x00, 0},
    {OPS_BITS_8, 0x8C16, 0x00, 0},
    {OPS_BITS_8, 0x8C17, 0x78, 0},
    {OPS_BITS_8, 0x8C18, 0x0A, 0},
    {OPS_BITS_8, 0x8C19, 0xDA, 0},
    {OPS_BITS_8, 0x8C1A, 0xFF, 0},
    {OPS_BITS_8, 0x8C1B, 0xA3, 0},
    {OPS_BITS_8, 0x8C1C, 0x58, 0},
    {OPS_BITS_8, 0x8C1D, 0x4A, 0},
    {OPS_BITS_8, 0x8C1E, 0xA2, 0},
    {OPS_BITS_8, 0x8C1F, 0x29, 0},
    {OPS_BITS_8, 0x8C20, 0x17, 0},
    {OPS_BITS_8, 0x8C21, 0x49, 0},
    {OPS_BITS_8, 0x8C22, 0x4B, 0},
    {OPS_BITS_8, 0x8C23, 0x00, 0},
    {OPS_BITS_8, 0x8C24, 0x00, 0},
    {OPS_BITS_8, 0x8C25, 0x00, 0},
    {OPS_BITS_8, 0x8C26, 0x01, 0},
    {OPS_BITS_8, 0x8C27, 0x01, 0},
    {OPS_BITS_8, 0x8C28, 0x01, 0},
    {OPS_BITS_8, 0x8C29, 0x01, 0},
    {OPS_BITS_8, 0x8C2A, 0x01, 0},
    {OPS_BITS_8, 0x8C2B, 0x01, 0},
    {OPS_BITS_8, 0x8C2C, 0x01, 0},
    {OPS_BITS_8, 0x8C2D, 0x01, 0},
    {OPS_BITS_8, 0x8C2E, 0x01, 0},
    {OPS_BITS_8, 0x8C2F, 0x01, 0},
    {OPS_BITS_8, 0x8C30, 0x01, 0},
    {OPS_BITS_8, 0x8C31, 0x01, 0},
    {OPS_BITS_8, 0x8C32, 0x01, 0},
    {OPS_BITS_8, 0x8C33, 0x01, 0},
    {OPS_BITS_8, 0x8C34, 0x01, 0},
    {OPS_BITS_8, 0x8C35, 0x01, 0},
    {OPS_BITS_8, 0x8C36, 0xA0, 0},
    {OPS_BITS_8, 0x8C37, 0x0F, 0},
    {OPS_BITS_8, 0x8C38, 0x20, 0},
    {OPS_BITS_8, 0x8C39, 0x00, 0},
    {OPS_BITS_8, 0x8C3A, 0x31, 0},
    {OPS_BITS_8, 0x8C3B, 0x58, 0},
    {OPS_BITS_8, 0x8C3C, 0x1C, 0},
    {OPS_BITS_8, 0x8C3D, 0x20, 0},
    {OPS_BITS_8, 0x8C3E, 0x28, 0},
    {OPS_BITS_8, 0x8C3F, 0x80, 0},
    {OPS_BITS_8, 0x8C40, 0x14, 0},
    {OPS_BITS_8, 0x8C41, 0x00, 0},
    {OPS_BITS_8, 0x8C42, 0xD9, 0},
    {OPS_BITS_8, 0x8C43, 0x28, 0},
    {OPS_BITS_8, 0x8C44, 0x11, 0},
    {OPS_BITS_8, 0x8C45, 0x00, 0},
    {OPS_BITS_8, 0x8C46, 0x00, 0},
    {OPS_BITS_8, 0x8C47, 0x1E, 0},
    {OPS_BITS_8, 0x8C48, 0x8C, 0},
    {OPS_BITS_8, 0x8C49, 0x0A, 0},
    {OPS_BITS_8, 0x8C4A, 0xD0, 0},
    {OPS_BITS_8, 0x8C4B, 0x8A, 0},
    {OPS_BITS_8, 0x8C4C, 0x20, 0},
    {OPS_BITS_8, 0x8C4D, 0xE0, 0},
    {OPS_BITS_8, 0x8C4E, 0x2D, 0},
    {OPS_BITS_8, 0x8C4F, 0x10, 0},
    {OPS_BITS_8, 0x8C50, 0x10, 0},
    {OPS_BITS_8, 0x8C51, 0x3E, 0},
    {OPS_BITS_8, 0x8C52, 0x96, 0},
    {OPS_BITS_8, 0x8C53, 0x00, 0},
    {OPS_BITS_8, 0x8C54, 0x13, 0},
    {OPS_BITS_8, 0x8C55, 0x8E, 0},
    {OPS_BITS_8, 0x8C56, 0x21, 0},
    {OPS_BITS_8, 0x8C57, 0x00, 0},
    {OPS_BITS_8, 0x8C58, 0x00, 0},
    {OPS_BITS_8, 0x8C59, 0x1E, 0},
    {OPS_BITS_8, 0x8C5A, 0x00, 0},
    {OPS_BITS_8, 0x8C5B, 0x00, 0},
    {OPS_BITS_8, 0x8C5C, 0x00, 0},
    {OPS_BITS_8, 0x8C5D, 0xFC, 0},
    {OPS_BITS_8, 0x8C5E, 0x00, 0},
    {OPS_BITS_8, 0x8C5F, 0x4D, 0},
    {OPS_BITS_8, 0x8C60, 0x69, 0},
    {OPS_BITS_8, 0x8C61, 0x6E, 0},
    {OPS_BITS_8, 0x8C62, 0x64, 0},
    {OPS_BITS_8, 0x8C63, 0x72, 0},
    {OPS_BITS_8, 0x8C64, 0x61, 0},
    {OPS_BITS_8, 0x8C65, 0x79, 0},
    {OPS_BITS_8, 0x8C66, 0x20, 0},
    {OPS_BITS_8, 0x8C67, 0x69, 0},
    {OPS_BITS_8, 0x8C68, 0x50, 0},
    {OPS_BITS_8, 0x8C69, 0x43, 0},
    {OPS_BITS_8, 0x8C6A, 0x32, 0},
    {OPS_BITS_8, 0x8C6B, 0x32, 0},
    {OPS_BITS_8, 0x8C6C, 0x00, 0},
    {OPS_BITS_8, 0x8C6D, 0x00, 0},
    {OPS_BITS_8, 0x8C6E, 0x00, 0},
    {OPS_BITS_8, 0x8C6F, 0xFD, 0},
    {OPS_BITS_8, 0x8C70, 0x00, 0},
    {OPS_BITS_8, 0x8C71, 0x17, 0},
    {OPS_BITS_8, 0x8C72, 0x3D, 0},
    {OPS_BITS_8, 0x8C73, 0x0F, 0},
    {OPS_BITS_8, 0x8C74, 0x8C, 0},
    {OPS_BITS_8, 0x8C75, 0x17, 0},
    {OPS_BITS_8, 0x8C76, 0x00, 0},
    {OPS_BITS_8, 0x8C77, 0x0A, 0},
    {OPS_BITS_8, 0x8C78, 0x20, 0},
    {OPS_BITS_8, 0x8C79, 0x20, 0},
    {OPS_BITS_8, 0x8C7A, 0x20, 0},
    {OPS_BITS_8, 0x8C7B, 0x20, 0},
    {OPS_BITS_8, 0x8C7C, 0x20, 0},
    {OPS_BITS_8, 0x8C7D, 0x20, 0},
    {OPS_BITS_8, 0x8C7E, 0x01, 0},
    {OPS_BITS_8, 0x8C7F, 0x7F, 0},
    {OPS_BITS_8, 0x8C80, 0x02, 0},
    {OPS_BITS_8, 0x8C81, 0x03, 0},
    {OPS_BITS_8, 0x8C82, 0x17, 0},
    {OPS_BITS_8, 0x8C83, 0x74, 0},
    {OPS_BITS_8, 0x8C84, 0x47, 0},
    {OPS_BITS_8, 0x8C85, 0x84, 0},
    {OPS_BITS_8, 0x8C86, 0x02, 0},
    {OPS_BITS_8, 0x8C87, 0x01, 0},
    {OPS_BITS_8, 0x8C88, 0x02, 0},
    {OPS_BITS_8, 0x8C89, 0x02, 0},
    {OPS_BITS_8, 0x8C8A, 0x02, 0},
    {OPS_BITS_8, 0x8C8B, 0x02, 0},
    {OPS_BITS_8, 0x8C8C, 0x23, 0},
    {OPS_BITS_8, 0x8C8D, 0x09, 0},
    {OPS_BITS_8, 0x8C8E, 0x07, 0},
    {OPS_BITS_8, 0x8C8F, 0x01, 0},
    {OPS_BITS_8, 0x8C90, 0x66, 0},
    {OPS_BITS_8, 0x8C91, 0x03, 0},
    {OPS_BITS_8, 0x8C92, 0x0C, 0},
    {OPS_BITS_8, 0x8C93, 0x00, 0},
    {OPS_BITS_8, 0x8C94, 0x30, 0},
    {OPS_BITS_8, 0x8C95, 0x00, 0},
    {OPS_BITS_8, 0x8C96, 0x80, 0},
    {OPS_BITS_8, 0x8C97, 0x8C, 0},
    {OPS_BITS_8, 0x8C98, 0x0A, 0},
    {OPS_BITS_8, 0x8C99, 0xD0, 0},
    {OPS_BITS_8, 0x8C9A, 0xD8, 0},
    {OPS_BITS_8, 0x8C9B, 0x09, 0},
    {OPS_BITS_8, 0x8C9C, 0x80, 0},
    {OPS_BITS_8, 0x8C9D, 0xA0, 0},
    {OPS_BITS_8, 0x8C9E, 0x20, 0},
    {OPS_BITS_8, 0x8C9F, 0xE0, 0},
    {OPS_BITS_8, 0x8CA0, 0x2D, 0},
    {OPS_BITS_8, 0x8CA1, 0x10, 0},
    {OPS_BITS_8, 0x8CA2, 0x10, 0},
    {OPS_BITS_8, 0x8CA3, 0x60, 0},
    {OPS_BITS_8, 0x8CA4, 0xA2, 0},
    {OPS_BITS_8, 0x8CA5, 0x00, 0},
    {OPS_BITS_8, 0x8CA6, 0xC4, 0},
    {OPS_BITS_8, 0x8CA7, 0x8E, 0},
    {OPS_BITS_8, 0x8CA8, 0x21, 0},
    {OPS_BITS_8, 0x8CA9, 0x00, 0},
    {OPS_BITS_8, 0x8CAA, 0x00, 0},
    {OPS_BITS_8, 0x8CAB, 0x1E, 0},
    {OPS_BITS_8, 0x8CAC, 0x8C, 0},
    {OPS_BITS_8, 0x8CAD, 0x0A, 0},
    {OPS_BITS_8, 0x8CAE, 0xD0, 0},
    {OPS_BITS_8, 0x8CAF, 0x8A, 0},
    {OPS_BITS_8, 0x8CB0, 0x20, 0},
    {OPS_BITS_8, 0x8CB1, 0xE0, 0},
    {OPS_BITS_8, 0x8CB2, 0x2D, 0},
    {OPS_BITS_8, 0x8CB3, 0x10, 0},
    {OPS_BITS_8, 0x8CB4, 0x10, 0},
    {OPS_BITS_8, 0x8CB5, 0x3E, 0},
    {OPS_BITS_8, 0x8CB6, 0x96, 0},
    {OPS_BITS_8, 0x8CB7, 0x00, 0},
    {OPS_BITS_8, 0x8CB8, 0x13, 0},
    {OPS_BITS_8, 0x8CB9, 0x8E, 0},
    {OPS_BITS_8, 0x8CBA, 0x21, 0},
    {OPS_BITS_8, 0x8CBB, 0x00, 0},
    {OPS_BITS_8, 0x8CBC, 0x00, 0},
    {OPS_BITS_8, 0x8CBD, 0x1E, 0},
    {OPS_BITS_8, 0x8CBE, 0x8C, 0},
    {OPS_BITS_8, 0x8CBF, 0x0A, 0},
    {OPS_BITS_8, 0x8CC0, 0xD0, 0},
    {OPS_BITS_8, 0x8CC1, 0x8A, 0},
    {OPS_BITS_8, 0x8CC2, 0x20, 0},
    {OPS_BITS_8, 0x8CC3, 0xE0, 0},
    {OPS_BITS_8, 0x8CC4, 0x2D, 0},
    {OPS_BITS_8, 0x8CC5, 0x10, 0},
    {OPS_BITS_8, 0x8CC6, 0x10, 0},
    {OPS_BITS_8, 0x8CC7, 0x3E, 0},
    {OPS_BITS_8, 0x8CC8, 0x96, 0},
    {OPS_BITS_8, 0x8CC9, 0x00, 0},
    {OPS_BITS_8, 0x8CCA, 0x13, 0},
    {OPS_BITS_8, 0x8CCB, 0x8E, 0},
    {OPS_BITS_8, 0x8CCC, 0x21, 0},
    {OPS_BITS_8, 0x8CCD, 0x00, 0},
    {OPS_BITS_8, 0x8CCE, 0x00, 0},
    {OPS_BITS_8, 0x8CCF, 0x1E, 0},
    {OPS_BITS_8, 0x8CD0, 0x8C, 0},
    {OPS_BITS_8, 0x8CD1, 0x0A, 0},
    {OPS_BITS_8, 0x8CD2, 0xD0, 0},
    {OPS_BITS_8, 0x8CD3, 0x8A, 0},
    {OPS_BITS_8, 0x8CD4, 0x20, 0},
    {OPS_BITS_8, 0x8CD5, 0xE0, 0},
    {OPS_BITS_8, 0x8CD6, 0x2D, 0},
    {OPS_BITS_8, 0x8CD7, 0x10, 0},
    {OPS_BITS_8, 0x8CD8, 0x10, 0},
    {OPS_BITS_8, 0x8CD9, 0x3E, 0},
    {OPS_BITS_8, 0x8CDA, 0x96, 0},
    {OPS_BITS_8, 0x8CDB, 0x00, 0},
    {OPS_BITS_8, 0x8CDC, 0x13, 0},
    {OPS_BITS_8, 0x8CDD, 0x8E, 0},
    {OPS_BITS_8, 0x8CDE, 0x21, 0},
    {OPS_BITS_8, 0x8CDF, 0x00, 0},
    {OPS_BITS_8, 0x8CE0, 0x00, 0},
    {OPS_BITS_8, 0x8CE1, 0x1E, 0},
    {OPS_BITS_8, 0x8CE2, 0x00, 0},
    {OPS_BITS_8, 0x8CE3, 0x00, 0},
    {OPS_BITS_8, 0x8CE4, 0x00, 0},
    {OPS_BITS_8, 0x8CE5, 0x00, 0},
    {OPS_BITS_8, 0x8CE6, 0x00, 0},
    {OPS_BITS_8, 0x8CE7, 0x00, 0},
    {OPS_BITS_8, 0x8CE8, 0x00, 0},
    {OPS_BITS_8, 0x8CE9, 0x00, 0},
    {OPS_BITS_8, 0x8CEA, 0x00, 0},
    {OPS_BITS_8, 0x8CEB, 0x00, 0},
    {OPS_BITS_8, 0x8CEC, 0x00, 0},
    {OPS_BITS_8, 0x8CED, 0x00, 0},
    {OPS_BITS_8, 0x8CEE, 0x00, 0},
    {OPS_BITS_8, 0x8CEF, 0x00, 0},
    {OPS_BITS_8, 0x8CF0, 0x00, 0},
    {OPS_BITS_8, 0x8CF1, 0x00, 0},
    {OPS_BITS_8, 0x8CF2, 0x00, 0},
    {OPS_BITS_8, 0x8CF3, 0x00, 0},
    {OPS_BITS_8, 0x8CF4, 0x00, 0},
    {OPS_BITS_8, 0x8CF5, 0x00, 0},
    {OPS_BITS_8, 0x8CF6, 0x00, 0},
    {OPS_BITS_8, 0x8CF7, 0x00, 0},
    {OPS_BITS_8, 0x8CF8, 0x00, 0},
    {OPS_BITS_8, 0x8CF9, 0x00, 0},
    {OPS_BITS_8, 0x8CFA, 0x00, 0},
    {OPS_BITS_8, 0x8CFB, 0x00, 0},
    {OPS_BITS_8, 0x8CFC, 0x00, 0},
    {OPS_BITS_8, 0x8CFD, 0x00, 0},
    {OPS_BITS_8, 0x8CFE, 0x00, 0},
    {OPS_BITS_8, 0x8CFF, 0x27, 0},
    /*// HDCP Setting*/
    {OPS_BITS_8, 0x85D1, 0x01, 0},
    {OPS_BITS_8, 0x8560, 0x24, 0},
    {OPS_BITS_8, 0x8563, 0x11, 0},
    {OPS_BITS_8, 0x8564, 0x0F, 0},
    /*// Video Setting*/
    {OPS_BITS_8, 0x8573, 0x80, 0},              // 0x81 for yuv422p 0x80 for rgb24
    /*// HDMI Audio Setting*/
    {OPS_BITS_8, 0x8600, 0x00, 0},
    {OPS_BITS_8, 0x8602, 0xF3, 0},
    {OPS_BITS_8, 0x8603, 0x02, 0},
    {OPS_BITS_8, 0x8604, 0x0C, 0},
    {OPS_BITS_8, 0x8606, 0x05, 0},
    {OPS_BITS_8, 0x8607, 0x00, 0},
    {OPS_BITS_8, 0x8620, 0x22, 0},
    {OPS_BITS_8, 0x8640, 0x01, 0},
    {OPS_BITS_8, 0x8641, 0x65, 0},
    {OPS_BITS_8, 0x8642, 0x07, 0},
    {OPS_BITS_8, 0x8652, 0x02, 0},
    {OPS_BITS_8, 0x8665, 0x10, 0},
    /*// Info Frame Extraction*/
    {OPS_BITS_8, 0x8709, 0xFF, 0},
    {OPS_BITS_8, 0x870B, 0x2C, 0},
    {OPS_BITS_8, 0x870C, 0x53, 0},
    {OPS_BITS_8, 0x870D, 0x01, 0},
    {OPS_BITS_8, 0x870E, 0x30, 0},
    {OPS_BITS_8, 0x9007, 0x10, 0},
    {OPS_BITS_8, 0x854A, 0x01, 0},
    {OPS_BITS_16, 0x0004, 0x0C17, 0}			// 0x0CD7 for yuv422p 0x0C17 for rgb24
};

H2C_IPC_INIT_CODE hdmi_init_cfg_1050x1680[] = {
    /*// Software Rese*/
    {OPS_BITS_16, 0x0002, 0x0F00, 10},
    {OPS_BITS_16, 0x0002, 0x0000, 0},
    /*// FIFO Delay Settin*/
    {OPS_BITS_16, 0x0006, 0x0154, 0},
    /*// PLL Settin*/
    {OPS_BITS_16, 0x0020, 0x306B, 0},
    {OPS_BITS_16, 0x0022, 0x0203, 10},
    /*Waitx1us(10)*/
    {OPS_BITS_16, 0x0022, 0x0213, 0},
    /*// Interrupt Contro*/
    {OPS_BITS_16, 0x0014, 0x0000, 0},
    {OPS_BITS_16, 0x0016, 0x05FF, 0},
    /*// CSI Lane Enabl*/
    {OPS_BITS_32, 0x0140, 0x00000000, 0},
    {OPS_BITS_32, 0x0144, 0x00000000, 0},
    {OPS_BITS_32, 0x0148, 0x00000000, 0},
    {OPS_BITS_32, 0x014C, 0x00000000, 0},
    {OPS_BITS_32, 0x0150, 0x00000000, 0},
    /*// CSI Transition Timin*/
    {OPS_BITS_32, 0x0210, 0x00001388, 0},
    {OPS_BITS_32, 0x0214, 0x00000004, 0},
    {OPS_BITS_32, 0x0218, 0x00001A03, 0},
    {OPS_BITS_32, 0x021C, 0x00000001, 0},
    {OPS_BITS_32, 0x0220, 0x00000404, 0},
    {OPS_BITS_32, 0x0224, 0x00004E20, 0},
    {OPS_BITS_32, 0x0228, 0x00000008, 0},
    {OPS_BITS_32, 0x022C, 0x00000003, 0},
    {OPS_BITS_32, 0x0230, 0x00000005, 0},
    {OPS_BITS_32, 0x0234, 0x0000001F, 0},
    {OPS_BITS_32, 0x0238, 0x00000001, 0},
    {OPS_BITS_32, 0x0204, 0x00000001, 0},
    {OPS_BITS_32, 0x0518, 0x00000001, 0},
    {OPS_BITS_32, 0x0500, 0xA3008086, 0},
    /*// HDMI Interrupt Mas*/
    {OPS_BITS_8, 0x8502, 0x01, 0},
    {OPS_BITS_8, 0x8512, 0xFE, 0},
    {OPS_BITS_8, 0x8514, 0xFF, 0},
    {OPS_BITS_8, 0x8515, 0xFF, 0},
    {OPS_BITS_8, 0x8516, 0xFF, 0},
    /*// HDMI Audio REFCL*/
    {OPS_BITS_8, 0x8531, 0x01, 0},
    {OPS_BITS_8, 0x8540, 0x8C, 0},
    {OPS_BITS_8, 0x8541, 0x0A, 0},
    {OPS_BITS_8, 0x8630, 0xB0, 0},
    {OPS_BITS_8, 0x8631, 0x1E, 0},
    {OPS_BITS_8, 0x8632, 0x04, 0},
    {OPS_BITS_8, 0x8670, 0x01, 0},
    /*// NCO_48F0*/
    /*// NCO_48F0*/
    /*// NCO_48F0*/
    /*// NCO_48F0*/
    /*// NCO_44F0*/
    /*// NCO_44F0*/
    /*// NCO_44F0*/
    /*// NCO_44F0*/
    /*// HDMI PH*/
    {OPS_BITS_8, 0x8532, 0x80, 0},
    {OPS_BITS_8, 0x8536, 0x40, 0},
    {OPS_BITS_8, 0x853F, 0x0A, 0},
    /*// HDMI SYSTE*/
    {OPS_BITS_8, 0x8543, 0x32, 0},
    {OPS_BITS_8, 0x8544, 0x10, 0},
    {OPS_BITS_8, 0x8545, 0x31, 0},
    {OPS_BITS_8, 0x8546, 0x2D, 0},
    /*// EDI*/
    {OPS_BITS_8, 0x85C7, 0x01, 0},
    {OPS_BITS_8, 0x85CA, 0x00, 0},
    {OPS_BITS_8, 0x85CB, 0x01, 0},
    /*// EDID Dat*/
    {OPS_BITS_8, 0x8C00, 0x00, 0},
    {OPS_BITS_8, 0x8C01, 0xFF, 0},
    {OPS_BITS_8, 0x8C02, 0xFF, 0},
    {OPS_BITS_8, 0x8C03, 0xFF, 0},
    {OPS_BITS_8, 0x8C04, 0xFF, 0},
    {OPS_BITS_8, 0x8C05, 0xFF, 0},
    {OPS_BITS_8, 0x8C06, 0xFF, 0},
    {OPS_BITS_8, 0x8C07, 0x00, 0},
    {OPS_BITS_8, 0x8C08, 0x52, 0},
    {OPS_BITS_8, 0x8C09, 0x62, 0},
    {OPS_BITS_8, 0x8C0A, 0x2A, 0},
    {OPS_BITS_8, 0x8C0B, 0x2C, 0},
    {OPS_BITS_8, 0x8C0C, 0x00, 0},
    {OPS_BITS_8, 0x8C0D, 0x00, 0},
    {OPS_BITS_8, 0x8C0E, 0x00, 0},
    {OPS_BITS_8, 0x8C0F, 0x00, 0},
    {OPS_BITS_8, 0x8C10, 0x2C, 0},
    {OPS_BITS_8, 0x8C11, 0x18, 0},
    {OPS_BITS_8, 0x8C12, 0x01, 0},
    {OPS_BITS_8, 0x8C13, 0x03, 0},
    {OPS_BITS_8, 0x8C14, 0x80, 0},
    {OPS_BITS_8, 0x8C15, 0x00, 0},
    {OPS_BITS_8, 0x8C16, 0x00, 0},
    {OPS_BITS_8, 0x8C17, 0x78, 0},
    {OPS_BITS_8, 0x8C18, 0x0A, 0},
    {OPS_BITS_8, 0x8C19, 0xDA, 0},
    {OPS_BITS_8, 0x8C1A, 0xFF, 0},
    {OPS_BITS_8, 0x8C1B, 0xA3, 0},
    {OPS_BITS_8, 0x8C1C, 0x58, 0},
    {OPS_BITS_8, 0x8C1D, 0x4A, 0},
    {OPS_BITS_8, 0x8C1E, 0xA2, 0},
    {OPS_BITS_8, 0x8C1F, 0x29, 0},
    {OPS_BITS_8, 0x8C20, 0x17, 0},
    {OPS_BITS_8, 0x8C21, 0x49, 0},
    {OPS_BITS_8, 0x8C22, 0x4B, 0},
    {OPS_BITS_8, 0x8C23, 0x00, 0},
    {OPS_BITS_8, 0x8C24, 0x00, 0},
    {OPS_BITS_8, 0x8C25, 0x00, 0},
    {OPS_BITS_8, 0x8C26, 0x01, 0},
    {OPS_BITS_8, 0x8C27, 0x01, 0},
    {OPS_BITS_8, 0x8C28, 0x01, 0},
    {OPS_BITS_8, 0x8C29, 0x01, 0},
    {OPS_BITS_8, 0x8C2A, 0x01, 0},
    {OPS_BITS_8, 0x8C2B, 0x01, 0},
    {OPS_BITS_8, 0x8C2C, 0x01, 0},
    {OPS_BITS_8, 0x8C2D, 0x01, 0},
    {OPS_BITS_8, 0x8C2E, 0x01, 0},
    {OPS_BITS_8, 0x8C2F, 0x01, 0},
    {OPS_BITS_8, 0x8C30, 0x01, 0},
    {OPS_BITS_8, 0x8C31, 0x01, 0},
    {OPS_BITS_8, 0x8C32, 0x01, 0},
    {OPS_BITS_8, 0x8C33, 0x01, 0},
    {OPS_BITS_8, 0x8C34, 0x01, 0},
    {OPS_BITS_8, 0x8C35, 0x01, 0},
    {OPS_BITS_8, 0x8C36, 0xC8, 0},
    {OPS_BITS_8, 0x8C37, 0x32, 0},
    {OPS_BITS_8, 0x8C38, 0x1A, 0},
    {OPS_BITS_8, 0x8C39, 0xC8, 0},
    {OPS_BITS_8, 0x8C3A, 0x40, 0},
    {OPS_BITS_8, 0x8C3B, 0x90, 0},
    {OPS_BITS_8, 0x8C3C, 0x26, 0},
    {OPS_BITS_8, 0x8C3D, 0x60, 0},
    {OPS_BITS_8, 0x8C3E, 0x28, 0},
    {OPS_BITS_8, 0x8C3F, 0x28, 0},
    {OPS_BITS_8, 0x8C40, 0x66, 0},
    {OPS_BITS_8, 0x8C41, 0x00, 0},
    {OPS_BITS_8, 0x8C42, 0xD9, 0},
    {OPS_BITS_8, 0x8C43, 0x28, 0},
    {OPS_BITS_8, 0x8C44, 0x11, 0},
    {OPS_BITS_8, 0x8C45, 0x00, 0},
    {OPS_BITS_8, 0x8C46, 0x00, 0},
    {OPS_BITS_8, 0x8C47, 0x1E, 0},
    {OPS_BITS_8, 0x8C48, 0x8C, 0},
    {OPS_BITS_8, 0x8C49, 0x0A, 0},
    {OPS_BITS_8, 0x8C4A, 0xD0, 0},
    {OPS_BITS_8, 0x8C4B, 0x8A, 0},
    {OPS_BITS_8, 0x8C4C, 0x20, 0},
    {OPS_BITS_8, 0x8C4D, 0xE0, 0},
    {OPS_BITS_8, 0x8C4E, 0x2D, 0},
    {OPS_BITS_8, 0x8C4F, 0x10, 0},
    {OPS_BITS_8, 0x8C50, 0x10, 0},
    {OPS_BITS_8, 0x8C51, 0x3E, 0},
    {OPS_BITS_8, 0x8C52, 0x96, 0},
    {OPS_BITS_8, 0x8C53, 0x00, 0},
    {OPS_BITS_8, 0x8C54, 0x13, 0},
    {OPS_BITS_8, 0x8C55, 0x8E, 0},
    {OPS_BITS_8, 0x8C56, 0x21, 0},
    {OPS_BITS_8, 0x8C57, 0x00, 0},
    {OPS_BITS_8, 0x8C58, 0x00, 0},
    {OPS_BITS_8, 0x8C59, 0x1E, 0},
    {OPS_BITS_8, 0x8C5A, 0x00, 0},
    {OPS_BITS_8, 0x8C5B, 0x00, 0},
    {OPS_BITS_8, 0x8C5C, 0x00, 0},
    {OPS_BITS_8, 0x8C5D, 0xFC, 0},
    {OPS_BITS_8, 0x8C5E, 0x00, 0},
    {OPS_BITS_8, 0x8C5F, 0x4D, 0},
    {OPS_BITS_8, 0x8C60, 0x69, 0},
    {OPS_BITS_8, 0x8C61, 0x6E, 0},
    {OPS_BITS_8, 0x8C62, 0x64, 0},
    {OPS_BITS_8, 0x8C63, 0x72, 0},
    {OPS_BITS_8, 0x8C64, 0x61, 0},
    {OPS_BITS_8, 0x8C65, 0x79, 0},
    {OPS_BITS_8, 0x8C66, 0x20, 0},
    {OPS_BITS_8, 0x8C67, 0x69, 0},
    {OPS_BITS_8, 0x8C68, 0x50, 0},
    {OPS_BITS_8, 0x8C69, 0x43, 0},
    {OPS_BITS_8, 0x8C6A, 0x32, 0},
    {OPS_BITS_8, 0x8C6B, 0x32, 0},
    {OPS_BITS_8, 0x8C6C, 0x00, 0},
    {OPS_BITS_8, 0x8C6D, 0x00, 0},
    {OPS_BITS_8, 0x8C6E, 0x00, 0},
    {OPS_BITS_8, 0x8C6F, 0xFD, 0},
    {OPS_BITS_8, 0x8C70, 0x00, 0},
    {OPS_BITS_8, 0x8C71, 0x17, 0},
    {OPS_BITS_8, 0x8C72, 0x3D, 0},
    {OPS_BITS_8, 0x8C73, 0x0F, 0},
    {OPS_BITS_8, 0x8C74, 0x8C, 0},
    {OPS_BITS_8, 0x8C75, 0x17, 0},
    {OPS_BITS_8, 0x8C76, 0x00, 0},
    {OPS_BITS_8, 0x8C77, 0x0A, 0},
    {OPS_BITS_8, 0x8C78, 0x20, 0},
    {OPS_BITS_8, 0x8C79, 0x20, 0},
    {OPS_BITS_8, 0x8C7A, 0x20, 0},
    {OPS_BITS_8, 0x8C7B, 0x20, 0},
    {OPS_BITS_8, 0x8C7C, 0x20, 0},
    {OPS_BITS_8, 0x8C7D, 0x20, 0},
    {OPS_BITS_8, 0x8C7E, 0x01, 0},
    {OPS_BITS_8, 0x8C7F, 0xE7, 0},
    {OPS_BITS_8, 0x8C80, 0x02, 0},
    {OPS_BITS_8, 0x8C81, 0x03, 0},
    {OPS_BITS_8, 0x8C82, 0x17, 0},
    {OPS_BITS_8, 0x8C83, 0x74, 0},
    {OPS_BITS_8, 0x8C84, 0x47, 0},
    {OPS_BITS_8, 0x8C85, 0x84, 0},
    {OPS_BITS_8, 0x8C86, 0x02, 0},
    {OPS_BITS_8, 0x8C87, 0x01, 0},
    {OPS_BITS_8, 0x8C88, 0x02, 0},
    {OPS_BITS_8, 0x8C89, 0x02, 0},
    {OPS_BITS_8, 0x8C8A, 0x02, 0},
    {OPS_BITS_8, 0x8C8B, 0x02, 0},
    {OPS_BITS_8, 0x8C8C, 0x23, 0},
    {OPS_BITS_8, 0x8C8D, 0x09, 0},
    {OPS_BITS_8, 0x8C8E, 0x07, 0},
    {OPS_BITS_8, 0x8C8F, 0x01, 0},
    {OPS_BITS_8, 0x8C90, 0x66, 0},
    {OPS_BITS_8, 0x8C91, 0x03, 0},
    {OPS_BITS_8, 0x8C92, 0x0C, 0},
    {OPS_BITS_8, 0x8C93, 0x00, 0},
    {OPS_BITS_8, 0x8C94, 0x30, 0},
    {OPS_BITS_8, 0x8C95, 0x00, 0},
    {OPS_BITS_8, 0x8C96, 0x80, 0},
    {OPS_BITS_8, 0x8C97, 0x8C, 0},
    {OPS_BITS_8, 0x8C98, 0x0A, 0},
    {OPS_BITS_8, 0x8C99, 0xD0, 0},
    {OPS_BITS_8, 0x8C9A, 0xD8, 0},
    {OPS_BITS_8, 0x8C9B, 0x09, 0},
    {OPS_BITS_8, 0x8C9C, 0x80, 0},
    {OPS_BITS_8, 0x8C9D, 0xA0, 0},
    {OPS_BITS_8, 0x8C9E, 0x20, 0},
    {OPS_BITS_8, 0x8C9F, 0xE0, 0},
    {OPS_BITS_8, 0x8CA0, 0x2D, 0},
    {OPS_BITS_8, 0x8CA1, 0x10, 0},
    {OPS_BITS_8, 0x8CA2, 0x10, 0},
    {OPS_BITS_8, 0x8CA3, 0x60, 0},
    {OPS_BITS_8, 0x8CA4, 0xA2, 0},
    {OPS_BITS_8, 0x8CA5, 0x00, 0},
    {OPS_BITS_8, 0x8CA6, 0xC4, 0},
    {OPS_BITS_8, 0x8CA7, 0x8E, 0},
    {OPS_BITS_8, 0x8CA8, 0x21, 0},
    {OPS_BITS_8, 0x8CA9, 0x00, 0},
    {OPS_BITS_8, 0x8CAA, 0x00, 0},
    {OPS_BITS_8, 0x8CAB, 0x1E, 0},
    {OPS_BITS_8, 0x8CAC, 0x8C, 0},
    {OPS_BITS_8, 0x8CAD, 0x0A, 0},
    {OPS_BITS_8, 0x8CAE, 0xD0, 0},
    {OPS_BITS_8, 0x8CAF, 0x8A, 0},
    {OPS_BITS_8, 0x8CB0, 0x20, 0},
    {OPS_BITS_8, 0x8CB1, 0xE0, 0},
    {OPS_BITS_8, 0x8CB2, 0x2D, 0},
    {OPS_BITS_8, 0x8CB3, 0x10, 0},
    {OPS_BITS_8, 0x8CB4, 0x10, 0},
    {OPS_BITS_8, 0x8CB5, 0x3E, 0},
    {OPS_BITS_8, 0x8CB6, 0x96, 0},
    {OPS_BITS_8, 0x8CB7, 0x00, 0},
    {OPS_BITS_8, 0x8CB8, 0x13, 0},
    {OPS_BITS_8, 0x8CB9, 0x8E, 0},
    {OPS_BITS_8, 0x8CBA, 0x21, 0},
    {OPS_BITS_8, 0x8CBB, 0x00, 0},
    {OPS_BITS_8, 0x8CBC, 0x00, 0},
    {OPS_BITS_8, 0x8CBD, 0x1E, 0},
    {OPS_BITS_8, 0x8CBE, 0x8C, 0},
    {OPS_BITS_8, 0x8CBF, 0x0A, 0},
    {OPS_BITS_8, 0x8CC0, 0xD0, 0},
    {OPS_BITS_8, 0x8CC1, 0x8A, 0},
    {OPS_BITS_8, 0x8CC2, 0x20, 0},
    {OPS_BITS_8, 0x8CC3, 0xE0, 0},
    {OPS_BITS_8, 0x8CC4, 0x2D, 0},
    {OPS_BITS_8, 0x8CC5, 0x10, 0},
    {OPS_BITS_8, 0x8CC6, 0x10, 0},
    {OPS_BITS_8, 0x8CC7, 0x3E, 0},
    {OPS_BITS_8, 0x8CC8, 0x96, 0},
    {OPS_BITS_8, 0x8CC9, 0x00, 0},
    {OPS_BITS_8, 0x8CCA, 0x13, 0},
    {OPS_BITS_8, 0x8CCB, 0x8E, 0},
    {OPS_BITS_8, 0x8CCC, 0x21, 0},
    {OPS_BITS_8, 0x8CCD, 0x00, 0},
    {OPS_BITS_8, 0x8CCE, 0x00, 0},
    {OPS_BITS_8, 0x8CCF, 0x1E, 0},
    {OPS_BITS_8, 0x8CD0, 0x8C, 0},
    {OPS_BITS_8, 0x8CD1, 0x0A, 0},
    {OPS_BITS_8, 0x8CD2, 0xD0, 0},
    {OPS_BITS_8, 0x8CD3, 0x8A, 0},
    {OPS_BITS_8, 0x8CD4, 0x20, 0},
    {OPS_BITS_8, 0x8CD5, 0xE0, 0},
    {OPS_BITS_8, 0x8CD6, 0x2D, 0},
    {OPS_BITS_8, 0x8CD7, 0x10, 0},
    {OPS_BITS_8, 0x8CD8, 0x10, 0},
    {OPS_BITS_8, 0x8CD9, 0x3E, 0},
    {OPS_BITS_8, 0x8CDA, 0x96, 0},
    {OPS_BITS_8, 0x8CDB, 0x00, 0},
    {OPS_BITS_8, 0x8CDC, 0x13, 0},
    {OPS_BITS_8, 0x8CDD, 0x8E, 0},
    {OPS_BITS_8, 0x8CDE, 0x21, 0},
    {OPS_BITS_8, 0x8CDF, 0x00, 0},
    {OPS_BITS_8, 0x8CE0, 0x00, 0},
    {OPS_BITS_8, 0x8CE1, 0x1E, 0},
    {OPS_BITS_8, 0x8CE2, 0x00, 0},
    {OPS_BITS_8, 0x8CE3, 0x00, 0},
    {OPS_BITS_8, 0x8CE4, 0x00, 0},
    {OPS_BITS_8, 0x8CE5, 0x00, 0},
    {OPS_BITS_8, 0x8CE6, 0x00, 0},
    {OPS_BITS_8, 0x8CE7, 0x00, 0},
    {OPS_BITS_8, 0x8CE8, 0x00, 0},
    {OPS_BITS_8, 0x8CE9, 0x00, 0},
    {OPS_BITS_8, 0x8CEA, 0x00, 0},
    {OPS_BITS_8, 0x8CEB, 0x00, 0},
    {OPS_BITS_8, 0x8CEC, 0x00, 0},
    {OPS_BITS_8, 0x8CED, 0x00, 0},
    {OPS_BITS_8, 0x8CEE, 0x00, 0},
    {OPS_BITS_8, 0x8CEF, 0x00, 0},
    {OPS_BITS_8, 0x8CF0, 0x00, 0},
    {OPS_BITS_8, 0x8CF1, 0x00, 0},
    {OPS_BITS_8, 0x8CF2, 0x00, 0},
    {OPS_BITS_8, 0x8CF3, 0x00, 0},
    {OPS_BITS_8, 0x8CF4, 0x00, 0},
    {OPS_BITS_8, 0x8CF5, 0x00, 0},
    {OPS_BITS_8, 0x8CF6, 0x00, 0},
    {OPS_BITS_8, 0x8CF7, 0x00, 0},
    {OPS_BITS_8, 0x8CF8, 0x00, 0},
    {OPS_BITS_8, 0x8CF9, 0x00, 0},
    {OPS_BITS_8, 0x8CFA, 0x00, 0},
    {OPS_BITS_8, 0x8CFB, 0x00, 0},
    {OPS_BITS_8, 0x8CFC, 0x00, 0},
    {OPS_BITS_8, 0x8CFD, 0x00, 0},
    {OPS_BITS_8, 0x8CFE, 0x00, 0},
    {OPS_BITS_8, 0x8CFF, 0x27, 0},
    /*// HDCP Settin*/
    {OPS_BITS_8, 0x85D1, 0x01, 0},
    {OPS_BITS_8, 0x8560, 0x24, 0},
    {OPS_BITS_8, 0x8563, 0x11, 0},
    {OPS_BITS_8, 0x8564, 0x0F, 0},
    /*// Video Settin*/
    {OPS_BITS_8, 0x8573, 0x80, 0},
    /*// HDMI Audio Settin*/
    {OPS_BITS_8, 0x8600, 0x00, 0},
    {OPS_BITS_8, 0x8602, 0xF3, 0},
    {OPS_BITS_8, 0x8603, 0x02, 0},
    {OPS_BITS_8, 0x8604, 0x0C, 0},
    {OPS_BITS_8, 0x8606, 0x05, 0},
    {OPS_BITS_8, 0x8607, 0x00, 0},
    {OPS_BITS_8, 0x8620, 0x22, 0},
    {OPS_BITS_8, 0x8640, 0x01, 0},
    {OPS_BITS_8, 0x8641, 0x65, 0},
    {OPS_BITS_8, 0x8642, 0x07, 0},
    {OPS_BITS_8, 0x8652, 0x02, 0},
    {OPS_BITS_8, 0x8665, 0x10, 0},
    /*// Info Frame Extractio*/
    {OPS_BITS_8, 0x8709, 0xFF, 0},
    {OPS_BITS_8, 0x870B, 0x2C, 0},
    {OPS_BITS_8, 0x870C, 0x53, 0},
    {OPS_BITS_8, 0x870D, 0x01, 0},
    {OPS_BITS_8, 0x870E, 0x30, 0},
    {OPS_BITS_8, 0x9007, 0x10, 0},
    {OPS_BITS_8, 0x854A, 0x01, 0},
    {OPS_BITS_16, 0x0004, 0x0C17, 0}
};

#define HDMI_INIT_ARRAY_SIZE   (sizeof(hdmi_init_cfg_1280x720)/sizeof(H2C_IPC_INIT_CODE))

typedef struct {
    struct i2c_msg *msgs;
    int nmsgs;
} i2c_rdwr_ioctl_data;

/* for reset H2C command sequence */
H2C_IPC_INIT_CODE h2c_reset[] = {
        {OPS_BITS_16, 0x0002, 0x0F00, 1},
        {OPS_BITS_16, 0x0002, 0x0000, 1},
        {OPS_BITS_16, 0x0004, 0x0000, 1}
};
#define H2C_RESET_ARRAY_SIZE   (sizeof(h2c_reset)/sizeof(H2C_IPC_INIT_CODE))

static int read_reg_by_address(int fd, int addr, unsigned short reg, unsigned short bits);

static int reg_write(int fd, int addr, unsigned short bits, unsigned short reg, unsigned int val, unsigned int time)
{
    (void)time;
    
    i2c_rdwr_ioctl_data work_queue;
    struct i2c_msg msg;
    unsigned char data[6] = {0};
    int ret = -1;
    
    work_queue.nmsgs = 1;
    work_queue.msgs = &msg;

    msg.addr = addr;
    msg.flags = 0;
    msg.len = 2 + bits;
    msg.buf = data;

    data[0] = (reg >> 8) & 0xFF;        /* for addr want to write, high bits before */
    data[1] = reg & 0xFF;

    if(OPS_BITS_8 == bits){
        data[2] = (unsigned char)val;
    }else if(OPS_BITS_16 == bits){
#if 1
        unsigned short *wdata = (unsigned short *)&data[2];
        *wdata = ((unsigned short)val);
#else
        data[2] = (val >> 8) & 0xFF;
        data[3] = val & 0xFF;
#endif
    }else if(OPS_BITS_32 == bits){
#if 1
        unsigned int *wdata = (unsigned int*)&data[2];
        *wdata = ((unsigned int)val);
#else
        data[2] = (val & 0xFF000000) >> 24;
        data[3] = (val & 0x00FF0000) >> 16;
        data[4] = (val & 0x0000FF00) >> 8;
        data[5] = (val & 0x000000FF);
#endif
    }

    ret = ioctl(fd, I2C_RDWR, &work_queue);
    if(ret < 0){
        perror("ioctl error");
        return -1;
    }

    return 0;
}

static void hdmi_reset_h2c(int fd, int addr)
{
    unsigned short  ops_bits;
    unsigned short  reg;
    unsigned int    data;
    unsigned int    time;
    int ret = -1;
    unsigned int i = 0;

    for(i = 0; i < H2C_RESET_ARRAY_SIZE; i++){
        ops_bits = h2c_reset[i].ops_bits;
        reg = h2c_reset[i].reg;
        data = h2c_reset[i].data;
        time = h2c_reset[i].time;
//      fprintf(stdout, "Write %03d: %d-0x%04X-0x%08X-%d.\n", i+1, ops_bits, reg, data, time);
        ret = reg_write(fd, addr, ops_bits, reg, data, time);
        if(ret){
            break;
        }
        if(time)            /* for delay */
		    usleep(time);
//            sleep(time);
            
        usleep(10);
    }

    if(i == H2C_RESET_ARRAY_SIZE){
        fprintf(stdout, "Hdmi reset code write successfully(%d).\n", i);
    }else{
        fprintf(stdout, "Hdmi reset code write failed(%d).\n", i);
    }
}

static void hdmi_init_ops(int fd, int addr)
{
    unsigned short  ops_bits;
    unsigned short  reg;
    unsigned int    data;
    unsigned int    time;
    int ret = -1;
    unsigned int i = 0;
    char tBuf[20] = "unknown";
    H2C_IPC_INIT_CODE *hdmi_init = h2c_ipc_init_code_from_hdmi_config(s_hdmi_cfg);
    const char * RES_STRING = h2c_ipc_init_code_str(s_hdmi_cfg);
    
    if(!hdmi_init) {
        return;
    }
    
    for(i = 0; i < HDMI_INIT_ARRAY_SIZE; i++) {
        ops_bits = hdmi_init[i].ops_bits;
        reg = hdmi_init[i].reg;
        data = hdmi_init[i].data;
        time = hdmi_init[i].time;

        //fprintf(stdout, "Write %03d: %d-0x%04X-0x%08X-%d.\n", i+1, ops_bits, reg, data, time);
        ret = reg_write(fd, addr, ops_bits, reg, data, time);
        if(ret){
            break;
        }
        if(time)            /* for delay */
            usleep(time);
            
        usleep(10);
        
        if(0x0004 == reg && 0x00 == (data & 0xC0))      /* bit7-bit6:00- RGB888 */
            strncpy(tBuf, (char *)"RGB888", 20);
        else if(0x0004 == reg && 0xC0 == (data & 0xC0)) /* bit7-bit6:11- YUV422P */
            strncpy(tBuf, "YUV422P", 20);
    }

    if(i == HDMI_INIT_ARRAY_SIZE){
        fprintf(stdout, "Hdmi init code(%s[%s]) write successfully(%d).\n", RES_STRING, tBuf, i);
    }else{
        fprintf(stdout, "Hdmi init code(%s[%s]) write failed(%d).\n", RES_STRING, tBuf, i);
    }
}

static void hdmi_check(int fd, int addr)
{
    unsigned short rAddr[] = 
    {0x852E, 0x852F, 0x858A, 0x858B, 0x8580, 0x8581, 0x8582, 0x8583,
     0x858C, 0x858D, 0x8584, 0x8585, 0x8586, 0x8587, 0x8588, 0x8589,
     0x8526};
#define READ_REG_NUM    (sizeof(rAddr)/sizeof(rAddr[0]))

    unsigned int i = 0;

    for(i = 0; i < READ_REG_NUM; i++){
        read_reg_by_address(fd, addr, rAddr[i], 1);
    }

    fprintf(stdout, "hdmi check %d.\n", i);
}

static int read_reg_by_address(int fd, int addr, unsigned short reg, unsigned short bits)
{
#define SND_BUF_LEN     3
#define RCV_BUF_LEN     4
    unsigned char sndBuf[SND_BUF_LEN] = {0};
    unsigned char rcvBuf[RCV_BUF_LEN] = {0};

    i2c_rdwr_ioctl_data work_queue;
    struct i2c_msg msgs[2];
    int ret = -1;
    
    work_queue.nmsgs = 2;
    work_queue.msgs = msgs;

    sndBuf[0] = (reg >> 8) & 0xFF;
    sndBuf[1] = reg & 0xFF;
    (work_queue.msgs[0]).addr = addr;
    (work_queue.msgs[0]).flags = 0;
    (work_queue.msgs[0]).len = SND_BUF_LEN - 1;
    (work_queue.msgs[0]).buf = (unsigned char *)sndBuf;


    (work_queue.msgs[1]).addr = addr;
    (work_queue.msgs[1]).flags = 1;
    (work_queue.msgs[1]).len = bits;
    (work_queue.msgs[1]).buf = (unsigned char *)rcvBuf;

    ret = ioctl(fd, I2C_RDWR, &work_queue);
    if(ret < 0){
        perror("ioctl read error");
        return ret;
    }

    if(OPS_BITS_8 == bits)
        fprintf(stdout, "OPS_BITS_8, 0x%04X, 0x%02X.\n", reg, rcvBuf[0]);
    else if(OPS_BITS_16 == bits)
        fprintf(stdout, "OPS_BITS_16, 0x%04X, 0x%02X%02X.\n", reg, rcvBuf[1], rcvBuf[0]);
    else if(OPS_BITS_32 == bits)
        fprintf(stdout, "OPS_BITS_32, 0x%04X, 0x%02X%02X%02X%02X.\n", reg, 
            rcvBuf[3], rcvBuf[2], rcvBuf[1], rcvBuf[0]);
    return ret;
}

static void hdmi_get_h2c_ver(int fd, int addr)
{
    read_reg_by_address(fd, addr, 0x8540, OPS_BITS_8);
    read_reg_by_address(fd, addr, 0x8541, OPS_BITS_8);
    read_reg_by_address(fd, addr, 0x0004, OPS_BITS_16);
}

static void print_edid(int fd, int addr)
{
#define EDID_LEN    256
    unsigned short s_addr = 0x8C00;
    int i = 0;

    for(i = 0; i < EDID_LEN; i++){
        read_reg_by_address(fd, addr, s_addr + i, 1);
    }
}

static void print_all(int fd, int addr)
{
    unsigned int i = 0;
#if 0
    unsigned short  ops_bits;
    unsigned short  reg;
    H2C_IPC_INIT_CODE *hdmi_init = h2c_ipc_init_code_from_hdmi_config(s_hdmi_cfg);
    for(i = 0; i < HDMI_INIT_ARRAY_SIZE; i++){
        ops_bits = hdmi_init[i].ops_bits;
        reg = hdmi_init[i].reg;
        read_reg_by_address(fd, addr, reg, ops_bits);
    }
#else
    unsigned int start = 0x0000;
    unsigned int end = 0x9000;

    for(i = start; i <= end; i += 2){
        read_reg_by_address(fd, addr, i, OPS_BITS_16);
    }
#endif
    fprintf(stdout, "Read %d registers.\n", i);
}

static void show_usage()
{
    fprintf(stdout, " * Param error.\n");
    fprintf(stdout, "-------------------------------------------------------\n");
    fprintf(stdout, "Usage: cmd -r|-w|-R|-v|-a|-e\n");
    fprintf(stdout, "-r[read]\tread the check register.\n");
    fprintf(stdout, "-w[write]\twrite the config info to h2c.\n");
    fprintf(stdout, "-R[reset]\treset the h2c chipset.\n");
    fprintf(stdout, "-v[version]\tread and print some register.\n");
    fprintf(stdout, "-a[all]\t\tread and print all of the registers.\n");
    fprintf(stdout, "-e[edid]\tread and print EDID data.\n");
    fprintf(stdout, "-------------------------------------------------------\n");
}

H2C_IPC_INIT_CODE* h2c_ipc_init_code_from_hdmi_config(HDMI_CONFIG hcfg)
{
    switch(hcfg) {
    case HDMI_CONFIG_1280x720:
        return hdmi_init_cfg_1280x720;
    case HDMI_CONFIG_1024x768:
        return hdmi_init_cfg_1024x768;
    case HDMI_CONFIG_1680x1050:
        return hdmi_init_cfg_1680x1050;
    case HDMI_CONFIG_1280x800:
        return hdmi_init_cfg_1280x800;
    case HDMI_CONFIG_800x600:
        return hdmi_init_cfg_800x600;
    case HDMI_CONFIG_1050x1680:
        return hdmi_init_cfg_1050x1680;
    default:
        return NULL;
    break;
    }
}

const char* h2c_ipc_init_code_str(HDMI_CONFIG hcfg)
{
    switch(hcfg) {
    case HDMI_CONFIG_1280x720:
        return "1280x720";
    case HDMI_CONFIG_1024x768:
        return "1024x768";
    case HDMI_CONFIG_1680x1050:
        return "1680x1050";
    case HDMI_CONFIG_1280x800:
        return "1280x800";
    case HDMI_CONFIG_800x600:
        return "800x600";
    case HDMI_CONFIG_1050x1680:
        return "1050x1680";
    default:
        return NULL;
    break;
    }
}

int hdmi_2_mipi_init(char *arg, int len)
{
    int fd = -1;

    if(NULL == arg || len <= 0 || 0 == strcmp("-h", arg)){
        show_usage();
        return 0;
    }

    fd = open(DEV_PATH, O_RDWR);
    if(-1 == fd){
        perror("open error");
        return -1;
    }else{
        fprintf(stdout, "Open device %s success: %d.\n", DEV_PATH, fd);
    }

    fprintf(stdout, "Will %s i2c addressed: 0x%02X.\n", arg, I2C_ADDR);
    fprintf(stdout, "------------------------------------------------\n");
    if(0 == strcmp("-R", arg)){
        hdmi_reset_h2c(fd, I2C_ADDR);
    }else if(0 == strcmp("-w", arg)){
        hdmi_init_ops(fd, I2C_ADDR);
    }else if(0 == strcmp("-r", arg)){
        hdmi_check(fd, I2C_ADDR);
    }else if(0 == strcmp("-v", arg)){
        hdmi_get_h2c_ver(fd, I2C_ADDR);
    }else if(0 == strcmp("-e", arg)){
        print_edid(fd, I2C_ADDR);
    }else if(0 == strcmp("-a", arg)){
        print_all(fd, I2C_ADDR);
    }else{
        show_usage();   
    }

    close(fd);
    return 0;
}

//////////////////////////////////////ISP CONTROL //////////////////////////////////////
resolution resolution_array[] = {
    {
        in_width : 1280,
        in_height : 720,
        in_format : IMG_IN_FORMAT,
        out_width : 1280,
        out_height : 720,
        out_format : IMG_OUT_FORMAT,
        mem_width : 0,
        mem_height : 0,
    },
    {
        in_width : 1024,
        in_height : 768,
        in_format : IMG_IN_FORMAT,
        out_width : 1024,
        out_height : 768,
        out_format : IMG_OUT_FORMAT,
        mem_width : 0,
        mem_height : 0,
    },
    {
        in_width : 1680,                       //input does not need to be aligned
        in_height : 1050,
        in_format : IMG_IN_FORMAT,
        //ISP output buffer is auto aligned by ISP, but extra receving buffer must be kepet up with it
        //ref. section7.2 in <ISP driver user guide>
        //ref. QMrOfsV2Waveform::flushData
        out_width : 1680,
        out_height : 1050,
        out_format : IMG_OUT_FORMAT,
        mem_width : 0,
        mem_height : 0,
    },
    {
        in_width : 1280,
        in_height : 800,
        in_format : IMG_IN_FORMAT,
        out_width : 1280,
        out_height : 800,
        out_format : IMG_OUT_FORMAT,
        mem_width : 0,
        mem_height : 0,
    },    
    {
        in_width : 800,
        in_height : 600,
        in_format : IMG_IN_FORMAT,
        out_width : 800,
        out_height : 600,
        out_format : IMG_OUT_FORMAT,
        mem_width : 0,
        mem_height : 0,
    },      
    {
        in_width : 1050,                       //no use
        in_height : 1680,
        in_format : IMG_IN_FORMAT,
        out_width : 1050,
        out_height : 1680,
        out_format : IMG_OUT_FORMAT,
        mem_width : 0,
        mem_height : 0,
    }, 
};
#define RESOLUTION_NUM  (sizeof(resolution_array[])/sizeof(resolution_array))

resolution* resolution_from_hdmi_config(HDMI_CONFIG hcfg)
{
    switch(hcfg) {
    case HDMI_CONFIG_1280x720:
        return &resolution_array[0]; 
    case HDMI_CONFIG_1024x768:
        return &resolution_array[1]; 
    case HDMI_CONFIG_1680x1050:
        return &resolution_array[2];
    case HDMI_CONFIG_1280x800:
        return &resolution_array[3];  
    case HDMI_CONFIG_800x600:
        return &resolution_array[4];
    case HDMI_CONFIG_1050x1680:
        return &resolution_array[5];
    default:
        return NULL;
    }
}

/* get capability of isp device */
static int isp_get_cap(int fd)
{
    struct v4l2_capability cap;
    /* get device capabilities */
    int ret = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (-1 == ret) {
        log_err("VIDIOC_QUERYCAP error\n");
        return -1;
    }
    log_info("VIDIOC_QUERYCAP success Driver:%s, Card:%s, Bus:%s, Ver:0x%X Cap:0x%08X.\n",
            cap.driver, cap.card, cap.bus_info, cap.version, cap.capabilities);
            
    /* is video capture */
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        log_info("  DON'T support V4L2_CAP_VIDEO_CAPTURE.\n");
    } else {
        log_info("  Support V4L2_CAP_VIDEO_CAPTURE.\n");
    }

    /* is video overlay */
    if (!(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY)) {
        log_info("  DON'T support V4L2_CAP_VIDEO_OVERLAY.\n");
    } else {
        log_info("  Support V4L2_CAP_VIDEO_OVERLAY.\n");
    }
    /* is read/write */
    if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
        log_info("  DON'T support read/write operation.\n");
    } else {
        log_info("  Support read/write operation.\n");
    }

    /* is streaming operation */
    if (!(cap.capabilities & V4L2_CAP_STREAMING)){
        log_info("  DON'T support streaming operation.\n");
    } else {
        log_info("  Support streaming operation.\n");
    }

    return 0;
}

static int isp_get_index(int fd)
{
    int index = 0;
    struct v4l2_input input;

    /* get index of input */    
    if (-1 == ioctl(fd, VIDIOC_G_INPUT, &index)) {
        log_err("VIDIOC_G_INPUT error\n");
        return -1;
    }
    
    log_debug("VIDIOC_G_INPUT success index: %d.\n", index);

    /* get name of input */
    memset(&input, 0x00, sizeof(input));
    input.index = index;
    if (-1 == ioctl(fd, VIDIOC_ENUMINPUT, &input)) {
        log_err("VIDIOC_ENUMINPUT error\n");
        return -1;
    }
    
    log_debug("VIDIOC_ENUMINPUT success. Input name: [%s]\n", input.name);

    return 0;
}

#ifdef VIDEO_MAX_PLANES
static int isp_setup_subdev(int fd, resolution *res)
{
    int ret = 0;
    struct v4l2_subdev_format fmt;

    memset(&fmt, 0x00, sizeof(fmt));
    fmt.pad = 0;
    fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
    fmt.format.code = res->in_format;
    fmt.format.width = res->in_width;
    fmt.format.height = res->in_height;
    fmt.format.field = V4L2_FIELD_NONE;

    ret = ioctl(fd, VIDIOC_SUBDEV_S_FMT, &fmt);
    if (ret == -1) {
        log_err("Set subdev format error\n");
        return -1;
    }
    
    log_debug("Set subdev format success. size:%dx%d code:0x%04X.\n",
            fmt.format.width, fmt.format.height, fmt.format.code);

    ret = ioctl(fd, VIDIOC_SUBDEV_G_FMT, &fmt);
    if (ret == -1) {
        log_err("Get subdev format error\n");
        return -1;
    }
    
    log_debug("Get subdev format success. pad:%d which:%d code:0x%04X size:%dx%d f:%d.\n",
            fmt.pad, fmt.which, fmt.format.code, fmt.format.width, fmt.format.height,
            fmt.format.field);

    return ret;
}
#else
static int isp_setup_subdev(int fd, resolution *res)
{
	return -1;
}
#endif

static int isp_setup_stream(int fd, resolution *res)
{
    int ret = 0;
    struct v4l2_format fmt;

    memset(&fmt, 0x00, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;             // stream type
    fmt.fmt.pix.width = res->out_width;
    fmt.fmt.pix.height = res->out_height;
    fmt.fmt.pix.pixelformat = res->out_format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    fmt.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB; // TODO

    log_debug("Will set isp output format. size:%dx%d fmt:%.4s.\n", fmt.fmt.pix.width,
            fmt.fmt.pix.height, (char*)&fmt.fmt.pix.pixelformat);
            
    ret = ioctl(fd, VIDIOC_S_FMT, &fmt);
    if (ret == -1) {
        log_err("Set format error");
        return -1;
    }

    /* get stream format for confirm */
    memset(&fmt, 0x00, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;             // stream type

    ret = ioctl(fd, VIDIOC_G_FMT, &fmt);
    if (ret == -1) {
        log_err("Get format error\n");
        return -1;
    }
    res->mem_width = fmt.fmt.pix.width;
    res->mem_height = fmt.fmt.pix.height;
    log_debug("Post get isp output format. size:%dx%d fmt:%.4s f:%d bperline:%d imgsize:%d.\n",
        res->mem_width, res->mem_height, (char*)&fmt.fmt.pix.pixelformat, fmt.fmt.pix.field,
        fmt.fmt.pix.bytesperline, fmt.fmt.pix.sizeimage);

    return ret;
}

/////////////////////////////////// CONVERSION //////////////////////////////////////////////
/*!
    LSB-FIRST BGRA -> 
    little-endian ARGB
*/
void convert_0bgr2argb(unsigned char *dst, unsigned char *src, int width, int height, int depth)
{
    int i = 0, j = 0;

#if 0    
    static int round;
    unsigned char *check_byte = src + width * height / 2 * depth + width / 2 * depth;
    if (round > 99 && check_byte[3] == 0) {
        printf("convert_xbgr2argb: !!!!!!!! IT SEEMS THAT ISP RESET THE ALPHA !!!!!!!!!!!!!!, round(%d)", round);
    }
#endif

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            unsigned char *pix = src + width * i * depth + j * depth;
            unsigned char *dpix = dst + width * i * depth + j * depth;
            unsigned char p0, p2;
            p0 = pix[0];
//            p1 = pix[1];
            p2 = pix[2];
//            p3 = pix[3];
            dpix[0] = p2;           //B
            dpix[1] = pix[1];       //G
            dpix[2] = p0;           //R
            dpix[3] = 0xFF;         //A
#if 0            
            pix[3] = 0xFF;              //set the alpha to 0xFF to see if ISP resets it            
#endif
        }
    }
#if 0    
    round++;
#endif
}

/*!
    set the alpha channel
*/
void convert_0bgr2xbgr(unsigned char *dst, unsigned char *src, int width, int height, int depth)
{
    int i = 0, j = 0;
#if 0    
    static int round;
    unsigned char *check_byte = src + width * height / 2 * depth + width / 2 * depth;
    if (round > 99 && check_byte[3] == 0) {
        printf("convert_xbgr2argb: !!!!!!!! IT SEEMS THAT ISP RESET THE ALPHA !!!!!!!!!!!!!!, round(%d)", round);
    }
#else
    (void)src;
#endif    
    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
#if 0
            unsigned char *pix = src + width * i * depth + j * depth;
#endif            
            unsigned char *dpix = dst + width * i * depth + j * depth;
            dpix[3] = 0xFF;         //A
#if 0            
            pix[3] = 0xFF;              //set the alpha to 0xFF to see if ISP resets it            
#endif
        }
    }
#if 0    
    round++;
#endif
}


////////////////////////////////// interface implementation ////////////////////////////////
int isp_open(char *devname)
{
    int fd = -1;
    fd = open(devname, O_RDWR | O_NONBLOCK);
    if (-1 == fd) {
        log_err("Open %s error\n", devname);
    }
    return fd;
}

int isp_close(int fd)
{
    return close(fd);
}

int isp_setup(int fd, HDMI_CONFIG hcfg)
{
    int ret = -1;

    //TODO can remove these calls
    isp_get_cap(fd);
    isp_get_index(fd);

    resolution *res = resolution_from_hdmi_config(hcfg);

    /* set subdev format */
    ret = isp_setup_subdev(fd, res);
    if (ret != 0) {
        return ret;
    }

    /* set stream format for capturing, and get mmap resolution */
    ret = isp_setup_stream(fd, res);
    if (ret != 0) {
        return ret;
    }

    s_hdmi_cfg = hcfg;
    return ret;
}

isp_buf_info *isp_alloc_video_buffers(int fd, unsigned int request_buf_num)
{
    unsigned int idx = 0;
    struct v4l2_requestbuffers req;
    struct v4l2_buffer buf;
    isp_buf_info *head = NULL;
    isp_buf_info *pinfo = NULL;
    resolution *res = resolution_from_hdmi_config(s_hdmi_cfg);
    
    memset(&req, 0, sizeof(req));
    // see enum v4l2_buf_type definition of req.type in videodev2.h
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    req.count = request_buf_num;

    if (-1 == ioctl(fd, VIDIOC_REQBUFS, &req)) {
        if (errno == EINVAL)
            log_info("Video capture or mmap-stream isn't supported.\n");
        else 
            log_err("VIDIOC_REQBUFS error\n");
        return NULL;
    }

    if (req.count != request_buf_num){
        log_err("Not enough buffer for reqbufs\n");
        return NULL;
    }
    
    log_info("Reqbuf success. Count:%d Type:%d.\n", req.count, req.type);

    head = (isp_buf_info *)calloc(req.count, sizeof(isp_buf_info));
    if(NULL == head){
        log_err("Calloc for video buffer error\n");
        return NULL;
    }
    
    log_info("Calloc video buffer success.\n");

    for (idx = 0; idx < req.count; idx++) {
        pinfo = head + idx;
        memset(&buf, 0, sizeof(buf));
        buf.type = req.type;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = idx;

        if (-1 == ioctl(fd, VIDIOC_QUERYBUF, &buf)) {
            log_err("VIDIOC_QUERYBUF error\n");
            return NULL;
        }
        pinfo->length = buf.length;
        pinfo->depth = buf.length / res->mem_width / res->mem_height;
        pinfo->mem_width = res->mem_width;
        pinfo->mem_height = res->mem_height;
        log_info("VIDIOC_QUERYBUF success buf len:%d used:%d off:%d depth:%d.\n",
            buf.length, buf.bytesused, buf.m.offset, pinfo->depth);

        pinfo->length = buf.length;
        pinfo->index = idx;

        /* mmap memory for operation in user space */
        pinfo->start = (__u8*)mmap(NULL, buf.length, PROT_READ | PROT_WRITE, 
                MAP_SHARED, fd, buf.m.offset);
        if (MAP_FAILED == pinfo->start) {
            log_err("Mmap failed\n");
            return NULL;
        }
        
        log_info("  Mmap success.\n");
        memset(pinfo->start, 0x01, pinfo->length);

        // queue this buffer
        if (-1 == ioctl(fd, VIDIOC_QBUF, &buf)) {
            log_err("VIDIOC_QBUF error\n");
            return NULL;
        }
        
        log_debug("  VIDIOC_QBUF success.\n");
    }

    return head;
}

int isp_dealloc_video_buffers(isp_buf_info *head, unsigned int num)
{
    unsigned int i = 0;
    isp_buf_info *p;

    for(i = 0; i < num; i++){
        p = head + i;
        if (-1 == munmap(p->start, p->length))
            debug_for_v4l2(V4L2_ERROR, "Munmap error");
    }

    free(head);
    return 0;
}

int isp_video_on(int fd)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;   

    hdmi_2_mipi_init((char *)"-w", 3);
    sleep(2);

    if (-1 == ioctl(fd, VIDIOC_STREAMON, &type)){
        debug_for_v4l2(V4L2_ERROR, "VIDIOC_STREAMON error");
        return -1;
    }
    return 0;
}

int isp_video_off(int fd)
{
    int ret = -1;
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  

    ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (-1 == ret) {
        debug_for_v4l2(V4L2_ERROR, "VIDIO_STREAMOFF error");
        ret = -1;
    }

    sleep(1);
    hdmi_2_mipi_init((char *)"-R", 3);

    return ret;
}

int isp_hdmi_2_mipi_init(char *arg, int len)
{
    return hdmi_2_mipi_init(arg, len);
}

isp_buf_info *isp_dqueue_video_buffer(int fd, isp_buf_info *pbufs, unsigned int num, int *result)
{
    int ret;
    struct v4l2_buffer vbuf;
    isp_buf_info *value = NULL;

    /* init structure */
    memset(&vbuf, 0, sizeof(vbuf));
    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;            // want to get video capture
    vbuf.memory = V4L2_MEMORY_MMAP;

    ret = ioctl(fd, VIDIOC_DQBUF, &vbuf);
    if (ret == -1) {
        if (EAGAIN == errno) {
            *result = 1;
            return NULL;
        }
        log_err("VIDIOC_DQBUF error\n");
        *result = -1;
        return NULL;
    }

    if(vbuf.index > num) {
        log_err("VIDIOC_DQBUF error: buffer overrun, index(%d), num(%d)\n", vbuf.index, num);
        *result = -1;
        return NULL;
    }
    
    value = pbufs + vbuf.index;
    value->vbuf = vbuf;
    value->length = vbuf.bytesused;

    *result = 0;
    return value;
}

int isp_queue_video_buffer(int fd, isp_buf_info *pbuf)
{
    return ioctl(fd, VIDIOC_QBUF, &(pbuf->vbuf));
}

