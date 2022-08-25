#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>

#define GRS_ALLOC(sz)		::HeapAlloc(GetProcessHeap(),0,sz)
#define GRS_CALLOC(sz)		::HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sz)
#define GRS_REALLOC(p,sz)	::HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,p,sz)

#define GRS_FREE(p)		     ::HeapFree(GetProcessHeap(),0,p)
#define GRS_SAFE_FREE(p)     if(nullptr != (p)){::HeapFree(GetProcessHeap(),0,(p));(p)=nullptr;}

#define GRS_MSIZE(p)		 ::HeapSize(GetProcessHeap(),0,p)
#define GRS_MVALID(p)        ::HeapValidate(GetProcessHeap(),0,p)

#ifndef GRS_OPEN_HEAP_LFH
//下面这个宏用于打开堆的LFH特性,以提高性能
#define GRS_OPEN_HEAP_LFH(h) \
    ULONG  ulLFH = 2;\
    HeapSetInformation((h),HeapCompatibilityInformation,&ulLFH ,sizeof(ULONG) ) ;
#endif // !GRS_OPEN_HEAP_LFH
