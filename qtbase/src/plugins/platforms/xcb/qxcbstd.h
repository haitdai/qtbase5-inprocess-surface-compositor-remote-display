#ifndef QXCBSTD_H
#define QXCBSTD_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/shm.h>//shm ops
#include <sys/time.h>
#include <sys/ipc.h>
#include <linux/kd.h>

#ifndef QT_NO_DEBUG_OUTPUT
#define  QT_NO_DEBUG_OUTPUT
#endif

#include <qdebug.h>
#include <qimage.h>

#include <pthread.h>                  //posix real-time thread
#include <sched.h>

#ifndef XCB_USE_RENDER
#error "xcb-render NOT found which is imperative for QMrOfsV2!"
#endif

#include <xcb/xcb.h>
#include <xcb/render.h>            //render ops
#ifndef template                     //render util BUG
#define template template_param
#endif
extern "C" {
#include <xcb/xcb_renderutil.h>
}
#undef template
#include <xcb/xfixes.h>             //xcb region ops
#include <xcb/shm.h>
#include <xcb/sync.h>
#include <xcb/xcb_image.h>
#include <xcb/xv.h>
#include <xcb/xcbext.h>

#include "qxcbobject.h"              //xcb plugin common headers

#include <QtGui/qregion.h>        //qt region ops

#endif //QXCBSTD_H
