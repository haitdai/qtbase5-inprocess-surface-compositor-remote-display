// This file was created by Filewrap 1.1
// Little endian mode
// DO NOT EDIT

#include "../PVRTMemoryFileSystem.h"

// using 32 bit to guarantee alignment.
#ifndef A32BIT
 #define A32BIT static const unsigned int
#endif

// ******** Start: waveform.vsh ********

// File data
static const char _waveform_vsh[] = 
	"attribute highp   vec4  inVertex;\r\n"
	"uniform mediump mat4    orientationMatrix;\r\n"
	"attribute mediump vec2  inTexCoord;\r\n"
	"varying mediump vec2    TexCoord;\r\n"
	"void main()\r\n"
	"{\r\n"
	"\tgl_Position = orientationMatrix * inVertex;\r\n"
	"\tTexCoord = inTexCoord;\r\n"
	"}\r\n";

// Register waveform.vsh in memory file system at application startup time
static CPVRTMemoryFileSystem RegisterFile_waveform_vsh("waveform.vsh", _waveform_vsh, 241);

// ******** End: waveform.vsh ********

