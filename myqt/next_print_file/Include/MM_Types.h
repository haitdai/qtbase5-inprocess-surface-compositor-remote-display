#ifndef MM_TYPES_H
#define MM_TYPES_H

#ifndef BYTE
#define BYTE unsigned char
#endif

#ifndef LPBYTE
#define LPBYTE BYTE *
#endif

#ifndef WORD
#define WORD unsigned short
#endif

#ifndef LONG
#define LONG long
#endif

#ifndef ULONG
#define ULONG unsigned long
#endif

#ifndef PULONG
#define PULONG ULONG *
#endif

#ifndef DWORD
#define DWORD unsigned long
#endif

#ifndef UINT64
#define UINT64 unsigned long long
#endif

#ifndef INT64
#define INT64 long long
#endif

#ifndef SHORT
#define SHORT signed short
#endif

#ifndef USHORT
#define USHORT unsigned short
#endif

#ifndef PUSHORT
#define PUSHORT USHORT *
#endif

#ifndef BOOL
#define BOOL int
#endif

#ifndef HANDLE
#define HANDLE void*
#endif

#ifndef VOID
#define VOID void
#endif
	
#ifndef PVOID
#define PVOID void*
#endif

#ifndef UINT
#define UINT unsigned int
#endif

#ifndef INT8
#define INT8 char
#endif

#ifndef int8
#define int8 INT8
#endif

#ifndef INT16
#define INT16 short
#endif

#ifndef int16
#define int16 INT16
#endif

#ifndef INT32
#define INT32 int
#endif

#ifndef int32
#define int32 INT32
#endif

#ifndef UCHAR
#define UCHAR unsigned char
#endif

#ifndef CHAR
#define CHAR char
#endif

#ifndef WCHAR
#define WCHAR wchar_t
#endif

#ifndef PUCHAR
#define PUCHAR UCHAR *
#endif


#ifndef UINT8
#define UINT8 unsigned char
#endif

#ifndef uint8
#define uint8 UINT8
#endif


#ifndef UINT16
#define UINT16 unsigned short
#endif

#ifndef uint16
#define uint16 UINT16
#endif

#ifndef UINT32
#define UINT32 unsigned int
#endif

#ifndef FLOAT
#define FLOAT float
#endif

#ifndef DOUBLE
#define DOUBLE double
#endif

#ifndef INT
#define INT int
#endif

#ifndef schar
#define schar char
#endif

#ifndef uchar
#define uchar UCHAR
#endif

#ifndef sshort
#define sshort short int
#endif

#ifndef ushort
#define ushort unsigned short
#endif

#ifndef uint
#define  uint UINT
#endif

#ifndef sint
#define sint int
#endif

#ifndef slint
#define slint long
#endif

#ifndef slong
#define slong long int
#endif

#ifndef ulint
#define ulint unsigned long
#endif

#ifndef ulong
#define ulong unsigned long
#endif

#ifndef uint32 
#define uint32 unsigned int
#endif

#ifndef vuint32
#define vuint32 volatile unsigned long int
#endif

#ifndef vuint8
#define vuint8 volatile uint8
#endif

#ifndef vuint16
#define vuint16 volatile uint16
#endif


#ifndef VUINT8
#define  VUINT8 vuint8
#endif

#ifndef VUINT16
#define  VUINT16 vuint16
#endif

#ifndef VUINT32
#define  VUINT32 vuint32
#endif

#ifndef WPARAM
#define WPARAM unsigned int
#endif 

#ifndef LPARAM
#define LPARAM long
#endif

#ifndef MGCOLOR
#define MGCOLOR DWORD
#endif

#ifndef u_char
#define u_char unsigned char
#endif

#ifndef u_short
#define u_short unsigned short
#endif

#ifndef u_int
#define u_int unsigned int
#endif

#ifndef u_long
#define u_long unsigned long
#endif

#ifndef OBJID
#define OBJID UINT32
#endif

#ifndef MSGID
#define MSGID UINT32
#endif


#ifndef RECT
typedef struct
{
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
}TAG_RECT;
#define RECT  TAG_RECT
#endif

#ifndef TRUE
#define TRUE			1
#endif

#ifndef FALSE
#define FALSE	0
#endif

#ifndef NULL
#define NULL			0
#endif 

#ifndef MM_OK
#define	MM_OK			0
#endif

typedef UINT32 IpId;
typedef UINT16 PortId;
typedef FLOAT FLOAT32;
typedef DOUBLE FLOAT64;

    
#endif
