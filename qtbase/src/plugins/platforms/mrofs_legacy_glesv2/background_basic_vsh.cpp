// This file was created by Filewrap 1.1
// Little endian mode
// DO NOT EDIT

#include "../PVRTMemoryFileSystem.h"

// using 32 bit to guarantee alignment.
#ifndef A32BIT
 #define A32BIT static const unsigned int
#endif

// ******** Start: background_basic.vsh ********

// File data
static const char _background_basic_vsh[] = 
	"attribute highp   vec4  inVertex;\r\n"
	"uniform mediump mat4    orientationMatrix;\r\n"
	"void main()\r\n"
	"{\r\n"
	"\tgl_Position = orientationMatrix * inVertex;\r\n"
	"}\r\n";

// Register background_basic.vsh in memory file system at application startup time
static CPVRTMemoryFileSystem RegisterFile_background_basic_vsh("background_basic.vsh", _background_basic_vsh, 144);

// ******** End: background_basic.vsh ********

