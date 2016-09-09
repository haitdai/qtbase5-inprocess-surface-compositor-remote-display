#ifndef __PRINT_FILE_INTERFACE_H__
#define __PRINT_FILE_INTERFACE_H__

#include <string>


class MStream
{
public:
	enum StreamType
	{
		OSTREAM,
		ISTREAM
	};

	enum EndianType
	{
		UNKNOWN_ENDIAN,
		BIGENDIAN,
		LITENDIAN 
	};

public:
	MStream();
	MStream(UINT32 size);
	virtual ~MStream();

public:
	virtual BOOL Kind(StreamType k) = 0;
	const UCHAR *Base();
	const UCHAR *Ptr();
	UINT32 Length();
	VOID Reset();
protected:
	UCHAR *m_basePtr;
	UCHAR *m_DataPtr;
	UINT32 m_nSize;
	BOOL  m_bNeedRelease;
	EndianType m_endianType;
};

class MOStream : public MStream
{
public:
	MOStream(UINT32 nSize = 0);
	BOOL Kind(StreamType k)
	{
		return OSTREAM  == k;
	}

	BOOL GoodBitFormat()
	{
		return m_nGoodBitFormat ? TRUE : FALSE;
	}

	MOStream& operator << (bool b);
	MOStream& operator << (char ch);
	MOStream& operator << (UCHAR uch);
	MOStream& operator << (const char* psz);
	MOStream& operator << (const std::string& str);
	MOStream& operator << (short s);
	MOStream& operator << (USHORT us);
	MOStream& operator << (int i);
	MOStream& operator << (UINT ui);
	MOStream& operator << (long l);
	MOStream& operator << (ULONG ul);
	MOStream& operator << (float f);
	MOStream& operator << (double d);

	BOOL Write(const UCHAR *pData, UINT32 nSize);
	
protected:
	BOOL Adjust(UINT32 nSize, UCHAR *&pBuf);
	int m_nGoodBitFormat;
};

class MIStream:public MStream
{
public:
	MIStream(const UCHAR *pData, UINT32 nSize, BOOL bRel = FALSE);
	BOOL Kind(StreamType k)
	{
		return ISTREAM == k;
	}

	MIStream& operator >> (bool& b);
	MIStream& operator >> (char& ch);
	MIStream& operator >> (UCHAR& uch);
	MIStream& operator >> (char *psz); //psz指向的缓冲区要足够大!
	MIStream& operator >> (std::string & str);
	MIStream& operator >> (short& s);
	MIStream& operator >> (USHORT& us);
	MIStream& operator >> (int& i);
	MIStream& operator >> (UINT& ui);
	MIStream& operator >> (long& l);
	MIStream& operator >> (ULONG& ul);
	MIStream& operator >> (float& f);
	MIStream& operator >> (double& d);

	BOOL Read(UCHAR *&pData, UINT32& nSize);
	const UCHAR* Read(UINT32& nSize);
protected:
	BOOL Adjust(UINT32 nSize, UCHAR *&pBuf);
};


struct MPrintFileInterface
{
	enum
	{
		FILE_TYPE_PRINT = 0,
		FILE_TYPE_RECORD,
	};

	struct CmdFileData
	{
		//UCHAR * m_dataPtr;
		//DWORD m_dataLength;
		MOStream  s;
		//CmdFileData(DWORD length);
		//~CmdFileData();
		UINT16 Release();
	};


	//初始化
	virtual BOOL Init() = 0;

	//设置Job ID字符串
	virtual VOID SetJobID(const CHAR* pJobID) = 0;

	//获得Job ID字符串
	virtual UINT16 GetJobID(CHAR *pCmdMem, DWORD dwLen) = 0;

	//设置总页数
	virtual VOID SetTotalPage(UINT16 nTotalPage) = 0;

	//获得总页数
	virtual UINT16 GetTotalPage() = 0;

	//设置当前页数
	virtual VOID SetCurrentPage(UINT16 nCurrentPage) = 0;

	//获得当前页数
	virtual UINT16 GetCurrentPage() = 0;

	//设置文件是否旋转90度打印
	virtual VOID SetRotate90(BOOL bRotate90) = 0;

	//获得文件打印旋转值
	virtual UINT16 GetRotate90() = 0;

	//设置区域宽度
	virtual VOID SetShowAreaWidth(UINT16 nShowAreaWidth) = 0;

	//获得区域宽度
	virtual UINT16 GetShowAreaWidth() = 0;

	//设置区域高度
	virtual VOID SetShowAreaHeight(UINT16 nShowAreaHeight) = 0;

	//获得区域高度
	virtual UINT16 GetShowAreaHeight() = 0;

	//设置纸张打印速度
	virtual VOID SetShowSpeed(FLOAT nShowSpeed) = 0;

	//获得纸张打印速度
	virtual UINT16 GetShowSpeed() = 0;

	//设置打印文件类型
	virtual VOID SetFileType(UCHAR cFileType) = 0;

	//获得打印文件类型
	virtual UCHAR GetFileType() = 0;

	//设置宽度与高度的dpi比值
	virtual VOID SetDpiWDivisionH( UCHAR value ) = 0;

	//获取宽度与高度的dpi比值
	virtual UCHAR GetDpiWDivisionH() = 0;

	//绘制象素点
	virtual VOID DrawPixel(UINT16 nX, UINT16 nY, DWORD dwColor) = 0;

	//绘制直线
	virtual VOID DrawLine(UINT16 nX1, UINT16 nY1, UINT16 nX2, UINT16 nY2, UCHAR cLineType, DWORD dwColor) = 0;
	
	//绘制图片
	virtual VOID DrawPicture(DWORD dwPictureID, UINT16 nX, UINT16 nY, UINT16 nPictureWidth, UINT16 nPictureHeight, const CHAR *pImageData24) = 0;
	
	//绘制字符
	virtual VOID DrawChar(DWORD dwCharCode, UINT16 nX, UINT16 nY, UINT16 nFontWidth, UINT16 nFontHeight, const CHAR *pFontData, const CHAR *pFontInfo, UCHAR ucFontColor) = 0;
	
	//获得连续内存长度
	virtual DWORD GetCmdFileLength() = 0;

	//写到一块连续内存
	virtual DWORD WriteToCmdFile(UCHAR *pCmdMem, DWORD dwLen) = 0;		

	virtual VOID WriteToCmdFile(MOStream & s) = 0;

	//从一块连续内存读
 	virtual BOOL ReadFromCmdFile(const UCHAR *pCmdMem, DWORD dwFileLength) = 0;

	//获取排版文件的宽度和高度
	virtual BOOL GetWidthHeight( UINT16 & width, UINT16 & height ) = 0;

	//获取光栅数据内存长度
	virtual DWORD GetRasterLength( UCHAR perPixelBits ) = 0;

	//写光栅数据
	virtual BOOL WriteToRaster(UCHAR * bmpBuf, DWORD bufLength, UCHAR perPixelBits) = 0;

	//获取光栅数据每行的长度
	virtual DWORD GetRasterBytesPerLine(UCHAR perPixelBits) = 0;

	//重置对象
	virtual VOID Reset() = 0;
};

//创建接口类指针
MPrintFileInterface* PrintInterfaceCreateObject();

//释放接口类指针
VOID PrintInterfaceReleaseObject(MPrintFileInterface * pMPrintFileInterface);

#endif	//__PRINT_INTERFACE_H__
