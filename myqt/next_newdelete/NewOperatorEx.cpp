#include "NewOperatorEx.h"
#include "MemEx.h"
#include <stdlib.h>

#undef USING_MALLOC
#define MEM_DEBUG

#ifdef WIN32
	#define MEM_ERROR()  \
			do  \
			{  \
				_asm INT 3	\
			} while ( 0 )

#elif defined(LINUX)
	#define MEM_ERROR()  \
		while(1) \
		{ \
			raise(SIGSTOP);\
		}
#else
    #define MEM_ERROR()
#endif
    
void * operator new(size_t size)
{
    MemInitWhenFirstCall();
	void *p;

	if(0 == size)
	{
		//MM_ASSERT(FALSE);
		return 0;
	}

#ifdef USING_MALLOC
	DWORD dwLevel;	

	dwLevel = MM_Lock();
	p = malloc(size);
	MM_Unlock(dwLevel);
#else

#ifdef MEM_DEBUG
	p = RealMallocEx(size + 4);
	if(NULL == p)
	{//增加错误处理
		return 0;
	}

	char *tmp = (char*)p;
	*tmp = 'N';
	*(tmp + 1) = 'O';
	*(tmp + 2) = 'R';
	*(tmp + 3) = 'M';
	tmp = tmp + 4;
	p = tmp;
#else
	p = malloc(size);
#endif  

#endif

	return p;
}

void operator delete(void *p)
{
    if(0 == p)
	{//增加错误处理
		//MM_ASSERT(FALSE);
		return;
	}

#ifdef USING_MALLOC
	DWORD dwLevel;	

	dwLevel = MM_Lock();
	free(p);
	MM_Unlock(dwLevel);	

#else

#ifdef MEM_DEBUG
	char *tmp = (char*)p;
	if ( ( 'N' != *(tmp - 4))
		|| ('O' != *(tmp - 3))
		|| ('R' != *(tmp - 2))
		|| ('M' != *(tmp - 1)) )
	{
		//MM_ASSERT(!"Attemp to free new[] allocated memory!");
		MEM_ERROR();
		return;
	}
    //倒过来
	*(tmp - 1) = 'N';
	*(tmp - 2) = 'O';
	*(tmp - 3) = 'R';
	*(tmp - 4) = 'M';
	
	tmp = tmp - 4;
	p = tmp;
	RealFreeEx(p);
#else
	free(p);
#endif    

#endif

}

void * operator new[](size_t count)
{
    MemInitWhenFirstCall();
    void *p = 0;	

	if(0 == count)
	{
		//MM_ASSERT(FALSE);
		return 0;
	}


#ifdef USING_MALLOC

	DWORD dwLevel;	

	dwLevel = MM_Lock();
	p = malloc(count);
	MM_Unlock(dwLevel);

#else

#ifdef MEM_DEBUG
	p = RealMallocEx(count + 4);
	if(0 == p)
	{//增加错误处理
		//MM_ASSERT(FALSE);
		return 0;
	}

	char *tmp = (char*)p;
	*tmp = 'A';
	*(tmp + 1) = 'R';
	*(tmp + 2) = 'A';
	*(tmp + 3) = 'Y';
	tmp = tmp + 4;
	p = tmp;
#else
	p = malloc(count);
#endif      

#endif
	return p;
}

void operator delete[](void *p)
{
	if(0 == p)
	{//增加错误处理
		//MM_ASSERT(FALSE);
		return;
	}

#ifdef USING_MALLOC

	DWORD dwLevel;	

	dwLevel = MM_Lock();
	free(p);
	MM_Unlock(dwLevel);	

#else

#ifdef MEM_DEBUG
	char *tmp = (char*)p;
	if ( ( 'A' != *(tmp - 4))
		|| ('R' != *(tmp - 3))
		|| ('A' != *(tmp - 2))
		|| ('Y' != *(tmp - 1)) )
	{
		//MM_ASSERT(!"Attemp to Free new allocated Memory!");
		MEM_ERROR();
		return;
	}
	//倒过来
	*(tmp - 1) = 'A';
	*(tmp - 2) = 'R';
	*(tmp - 3) = 'A';
	*(tmp - 4) = 'R';
	
	tmp = tmp - 4;
	p = tmp;
	RealFreeEx(p);
#else
	free(p);
#endif    

#endif
}
