#ifndef __MEM_EX_H__
#define __MEM_EX_H__

#if __cplusplus
extern "C" {
#endif

#ifdef MEM_USE_STATIC_LIB
  #define DLL_API
#elif defined(WIN32)
  #ifdef VC_MEMEX_EXPORTS
    #define DLL_API __declspec(dllexport)
  #else
    #define DLL_API __declspec(dllimport)
  #endif
#else
  #define DLL_API
#endif

DLL_API unsigned int GetMemoryStartAddr(void); //������ڴ���ʼ��ַ
DLL_API unsigned int GetMemoryTotalSize(void); //��������ڴ��С
DLL_API unsigned int GetNowRemainMemSize(void); //��ǰʣ������ڴ��С
DLL_API unsigned int GetMinRemainMemSize(void); //������������, ʣ������ڴ��С����Сֵ
DLL_API unsigned int GetTotalMemAllocTimes(void); //������������, �����ڴ����
DLL_API unsigned int GetTotalMemFreeTimes(void); //������������, �ͷ��ڴ����
DLL_API unsigned int GetNowAllocMemBlockCount(void); //��ǰ�Ѿ�������ڴ�����
DLL_API unsigned int GetMaxAllocMemBlockCount(void); //�����Ѿ�������ڴ�����
DLL_API unsigned int GetNowAllocFindHoleNodeCount(void); //�������һ���ڴ���ҵĴ���
DLL_API unsigned int GetMaxAllocFindHoleNodeCount(void); //������������, ����һ���ڴ���ҵĴ��������ֵ

// ϵͳ��ʼ��ʱ,����������Ҫ������ڴ�
DLL_API void MemInitWhenFirstCall();

// �ڴ�����ͷ�(Ӧ�ò�Ҫֱ�ӵ���������������Ӧ�õ���MMOS����� new/delete MM_RealMalloc/MM_RealFree )
DLL_API void* RealMallocEx(unsigned long dwSize);
DLL_API void RealFreeEx(void *p);


#if __cplusplus
}
#endif

#endif
