
An simple in-process compositor and remote display server under the Qt5.0 API, hightlights including:

0, as we know, the early version of Qt5(5.0, 5.1, 5.3)'s support for interoperability of qwidget and other native drawing APIs(ogles2, etc)
 is very buggy, things changed until the importion of QOpenGLWidget in 5.4. here is a work-around solution for early version of Qt5.

1, a inprocess compositor
since Qt's qwidget updating mechanism at high frequency may exhuaste the CPU in some low-end or power consumption-critical products such as mobile 
devices, I have implemented a simple in-process compositor, the compositor can take 1 single user interface surface, and multiple real-time 
updating surfaces like videos, camara preview, waveform, etc, into 1 single video buffer which can be scanned out by LCDC directly. the user 
interface surface is drawn by Qt as usual, and the other real-time updating surfaces can be drawn by QRasterPaintEngine, or Pixman, or freetype2, or Cairo, or OGLES2(with eglImage extension or Pixel Buffer feature), or even in-house raster operations, Porter-Duff operations is helpful.
to eliminate the overhead of IPC, a inprocess style is prefered than outprocess which is normal in x-window enviroment.

2, cross-platform
underlying hardware/platform could be xcb/xorg, or egl/gles2, or some private graphics API like powerVR 2D Bliting API in the early days.
other choice is also feasible, such as drm or wayland.

3, a simple remote-display server
in which case a qt-based client can run on windows PC or Android tablet to view the linux-based display server(NOT xserver) remotely. There's a inprocess compositor run in the remote server. 