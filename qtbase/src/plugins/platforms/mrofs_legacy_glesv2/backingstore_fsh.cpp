// This file was created by Filewrap 1.1
// Little endian mode
// DO NOT EDIT

#include "../PVRTMemoryFileSystem.h"

// using 32 bit to guarantee alignment.
#ifndef A32BIT
 #define A32BIT static const unsigned int
#endif

// ******** Start: backingstore.fsh ********

// File data
static const char _backingstore_fsh[] = 
	"#ifdef GL_IMG_texture_stream2\r\n"
	"#extension GL_IMG_texture_stream2 : enable\r\n"
	"uniform samplerStreamIMG sTexture;\r\n"
	"#elif defined(GL_OES_EGL_image_external)\r\n"
	"#extension GL_OES_EGL_image_external : require\r\n"
	"uniform samplerExternalOES             sTexture;\r\n"
	"#endif\r\n"
	"\r\n"
	"varying mediump vec2                   TexCoord;\r\n"
	"uniform mediump vec4                   bkColor;\r\n"
	"uniform highp vec4                     colorKey;\r\n"
	"const mediump float factor = 0.001;\r\n"
	"void main(void)\r\n"
	"{\r\n"
	"#ifdef GL_IMG_texture_stream2\r\n"
	"\tmediump vec4 c = textureStreamIMG(sTexture, TexCoord);\r\n"
	"#elif defined(GL_OES_EGL_image_external)\r\n"
	"\tmediump vec4 c = texture2D(sTexture, TexCoord);\r\n"
	"#endif\r\n"
	"\t//if (all(lessThan(c, ckUpper)) && all(greaterThan(c, ckLower)))\r\n"
	"\tmediump vec4 delta = abs(c - colorKey);\r\n"
	"\t/*\r\n"
	"\tDHT20140924, no vsync, for Dubhe Frontend,\r\n"
	"\tmaximized FPS is 60+ with colorkey,\r\n"
	"\tmaximized FPS is 70+ without colorkey,\r\n"
	"\tNOTE:\r\n"
	"\t1, setting c.a to zero is faster than calling discard\r\n"
	"\t2, compare rgb independently is faster than calling all+lessThan/greaterThan\r\n"
	"\t*/\r\n"
	"\tif(delta.r < factor && delta.g < factor && delta.b < factor)\r\n"
	"\t\tc.a = 0.0;\r\n"
	"\tgl_FragColor = c;\r\n"
	"}\r\n"
	"\r\n";

// Register backingstore.fsh in memory file system at application startup time
static CPVRTMemoryFileSystem RegisterFile_backingstore_fsh("backingstore.fsh", _backingstore_fsh, 1143);

// ******** End: backingstore.fsh ********

