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

DLL_API unsigned int GetMemoryStartAddr(void); //管理的内存起始地址
DLL_API unsigned int GetMemoryTotalSize(void); //管理的总内存大小
DLL_API unsigned int GetNowRemainMemSize(void); //当前剩余空闲内存大小
DLL_API unsigned int GetMinRemainMemSize(void); //从启动到现在, 剩余空闲内存大小的最小值
DLL_API unsigned int GetTotalMemAllocTimes(void); //从启动到现在, 分配内存次数
DLL_API unsigned int GetTotalMemFreeTimes(void); //从启动到现在, 释放内存次数
DLL_API unsigned int GetNowAllocMemBlockCount(void); //当前已经分配的内存块个数
DLL_API unsigned int GetMaxAllocMemBlockCount(void); //最大的已经分配的内存块个数
DLL_API unsigned int GetNowAllocFindHoleNodeCount(void); //最近分配一次内存查找的次数
DLL_API unsigned int GetMaxAllocFindHoleNodeCount(void); //从启动到现在, 分配一次内存查找的次数的最大值

// 系统初始化时,分配所有需要管理的内存
DLL_API void MemInitWhenFirstCall();

// 内存分配释放(应用不要直接调用以下这两个，应该调用MMOS里面的 new/delete MM_RealMalloc/MM_RealFree )
DLL_API void* RealMallocEx(unsigned long dwSize);
DLL_API void RealFreeEx(void *p);


#if __cplusplus
}
#endif

#endif
