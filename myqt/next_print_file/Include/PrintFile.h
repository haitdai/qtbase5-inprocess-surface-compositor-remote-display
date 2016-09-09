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
	MIStream& operator >> (char *psz); //pszָ��Ļ�����Ҫ�㹻��!
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


	//��ʼ��
	virtual BOOL Init() = 0;

	//����Job ID�ַ���
	virtual VOID SetJobID(const CHAR* pJobID) = 0;

	//���Job ID�ַ���
	virtual UINT16 GetJobID(CHAR *pCmdMem, DWORD dwLen) = 0;

	//������ҳ��
	virtual VOID SetTotalPage(UINT16 nTotalPage) = 0;

	//�����ҳ��
	virtual UINT16 GetTotalPage() = 0;

	//���õ�ǰҳ��
	virtual VOID SetCurrentPage(UINT16 nCurrentPage) = 0;

	//��õ�ǰҳ��
	virtual UINT16 GetCurrentPage() = 0;

	//�����ļ��Ƿ���ת90�ȴ�ӡ
	virtual VOID SetRotate90(BOOL bRotate90) = 0;

	//����ļ���ӡ��תֵ
	virtual UINT16 GetRotate90() = 0;

	//����������
	virtual VOID SetShowAreaWidth(UINT16 nShowAreaWidth) = 0;

	//���������
	virtual UINT16 GetShowAreaWidth() = 0;

	//��������߶�
	virtual VOID SetShowAreaHeight(UINT16 nShowAreaHeight) = 0;

	//�������߶�
	virtual UINT16 GetShowAreaHeight() = 0;

	//����ֽ�Ŵ�ӡ�ٶ�
	virtual VOID SetShowSpeed(FLOAT nShowSpeed) = 0;

	//���ֽ�Ŵ�ӡ�ٶ�
	virtual UINT16 GetShowSpeed() = 0;

	//���ô�ӡ�ļ�����
	virtual VOID SetFileType(UCHAR cFileType) = 0;

	//��ô�ӡ�ļ�����
	virtual UCHAR GetFileType() = 0;

	//���ÿ����߶ȵ�dpi��ֵ
	virtual VOID SetDpiWDivisionH( UCHAR value ) = 0;

	//��ȡ�����߶ȵ�dpi��ֵ
	virtual UCHAR GetDpiWDivisionH() = 0;

	//�������ص�
	virtual VOID DrawPixel(UINT16 nX, UINT16 nY, DWORD dwColor) = 0;

	//����ֱ��
	virtual VOID DrawLine(UINT16 nX1, UINT16 nY1, UINT16 nX2, UINT16 nY2, UCHAR cLineType, DWORD dwColor) = 0;
	
	//����ͼƬ
	virtual VOID DrawPicture(DWORD dwPictureID, UINT16 nX, UINT16 nY, UINT16 nPictureWidth, UINT16 nPictureHeight, const CHAR *pImageData24) = 0;
	
	//�����ַ�
	virtual VOID DrawChar(DWORD dwCharCode, UINT16 nX, UINT16 nY, UINT16 nFontWidth, UINT16 nFontHeight, const CHAR *pFontData, const CHAR *pFontInfo, UCHAR ucFontColor) = 0;
	
	//��������ڴ泤��
	virtual DWORD GetCmdFileLength() = 0;

	//д��һ�������ڴ�
	virtual DWORD WriteToCmdFile(UCHAR *pCmdMem, DWORD dwLen) = 0;		

	virtual VOID WriteToCmdFile(MOStream & s) = 0;

	//��һ�������ڴ��
 	virtual BOOL ReadFromCmdFile(const UCHAR *pCmdMem, DWORD dwFileLength) = 0;

	//��ȡ�Ű��ļ��Ŀ�Ⱥ͸߶�
	virtual BOOL GetWidthHeight( UINT16 & width, UINT16 & height ) = 0;

	//��ȡ��դ�����ڴ泤��
	virtual DWORD GetRasterLength( UCHAR perPixelBits ) = 0;

	//д��դ����
	virtual BOOL WriteToRaster(UCHAR * bmpBuf, DWORD bufLength, UCHAR perPixelBits) = 0;

	//��ȡ��դ����ÿ�еĳ���
	virtual DWORD GetRasterBytesPerLine(UCHAR perPixelBits) = 0;

	//���ö���
	virtual VOID Reset() = 0;
};

//�����ӿ���ָ��
MPrintFileInterface* PrintInterfaceCreateObject();

//�ͷŽӿ���ָ��
VOID PrintInterfaceReleaseObject(MPrintFileInterface * pMPrintFileInterface);

#endif	//__PRINT_INTERFACE_H__
