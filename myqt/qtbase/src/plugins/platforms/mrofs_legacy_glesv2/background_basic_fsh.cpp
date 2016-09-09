// This file was created by Filewrap 1.1
// Little endian mode
// DO NOT EDIT

#include "../PVRTMemoryFileSystem.h"

// using 32 bit to guarantee alignment.
#ifndef A32BIT
 #define A32BIT static const unsigned int
#endif

// ******** Start: background_basic.fsh ********

// File data
static const char _background_basic_fsh[] = 
	"uniform mediump vec4                   bkColor;\r\n"
	"mediump float                          rbTemp;\r\n"
	"void main(void)\r\n"
	"{\r\n"
	"\tgl_FragColor =  bkColor;\r\n"
	"}\r\n"
	"\r\n";

// Register background_basic.fsh in memory file system at application startup time
static CPVRTMemoryFileSystem RegisterFile_background_basic_fsh("background_basic.fsh", _background_basic_fsh, 149);

// ******** End: background_basic.fsh ********

