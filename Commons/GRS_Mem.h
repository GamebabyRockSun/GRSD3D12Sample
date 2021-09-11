#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>

#define GRS_ALLOC(sz)		    ::HeapAlloc(GetProcessHeap(),0,sz)
#define GRS_CALLOC(sz)		    ::HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sz)
#define GRS_REALLOC(p,sz)	    ::HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,p,sz)

//下面这个表达式可能会引起副作用,p参数尽量使用指针变量,而不是表达式
#define GRS_SAFE_FREE(p)        if(nullptr != (p)){::HeapFree(GetProcessHeap(),0,(p));(p)=nullptr;}
#define GRS_FREE(p)		        GRS_SAFE_FREE(p)

#define GRS_MSIZE(p)		    ::HeapSize(GetProcessHeap(),0,p)
#define GRS_MVALID(p)		    ::HeapValidate(GetProcessHeap(),0,p)

