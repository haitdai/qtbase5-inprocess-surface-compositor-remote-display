// This file was created by Filewrap 1.1
// Little endian mode
// DO NOT EDIT

#include "../PVRTMemoryFileSystem.h"

// using 32 bit to guarantee alignment.
#ifndef A32BIT
 #define A32BIT static const unsigned int
#endif

// ******** Start: waveform.fsh ********

// File data
static const char _waveform_fsh[] = 
	"#ifdef GL_IMG_texture_stream2\r\n"
	"#extension GL_IMG_texture_stream2 : enable\r\n"
	"uniform samplerStreamIMG sTexture;\r\n"
	"#elif defined(GL_OES_EGL_image_external)\r\n"
	"#extension GL_OES_EGL_image_external : require\r\n"
	"uniform samplerExternalOES             sTexture;\r\n"
	"#endif\r\n"
	"\r\n"
	"varying mediump vec2                   TexCoord;\r\n"
	"void main(void)\r\n"
	"{\r\n"
	"#ifdef GL_IMG_texture_stream2\r\n"
	"\tgl_FragColor = textureStreamIMG(sTexture, TexCoord);\r\n"
	"#elif defined(GL_OES_EGL_image_external)\r\n"
	"\tgl_FragColor = texture2D(sTexture, TexCoord);\r\n"
	"#endif\r\n"
	"}\r\n";

// Register waveform.fsh in memory file system at application startup time
static CPVRTMemoryFileSystem RegisterFile_waveform_fsh("waveform.fsh", _waveform_fsh, 518);

// ******** End: waveform.fsh ********

