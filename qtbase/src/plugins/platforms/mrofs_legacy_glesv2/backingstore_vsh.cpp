// This file was created by Filewrap 1.1
// Little endian mode
// DO NOT EDIT

#include "../PVRTMemoryFileSystem.h"

// using 32 bit to guarantee alignment.
#ifndef A32BIT
 #define A32BIT static const unsigned int
#endif

// ******** Start: backingstore.vsh ********

// File data
static const char _backingstore_vsh[] = 
	"attribute highp   vec4  inVertex;\r\n"
	"uniform mediump mat4    orientationMatrix;\r\n"
	"attribute mediump vec2  inTexCoord;\r\n"
	"varying mediump vec2    TexCoord;\r\n"
	"void main()\r\n"
	"{\r\n"
	"\tgl_Position = orientationMatrix * inVertex;\r\n"
	"\tTexCoord = inTexCoord;\r\n"
	"}\r\n";

// Register backingstore.vsh in memory file system at application startup time
static CPVRTMemoryFileSystem RegisterFile_backingstore_vsh("backingstore.vsh", _backingstore_vsh, 241);

// ******** End: backingstore.vsh ********

