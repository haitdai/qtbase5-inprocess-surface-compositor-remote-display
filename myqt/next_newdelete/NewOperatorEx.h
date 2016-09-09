#ifndef __NEWOPERATOR_EX_H__
#define __NEWOPERATOR_EX_H__

void *  operator new(size_t size);
void  operator delete(void *p);

void * operator new[](size_t count);
void  operator delete[](void *p);

#endif