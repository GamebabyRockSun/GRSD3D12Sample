//===================================================================================
//项目名称：GRS 公共平台 V2.0
//文件描述：Windows头文件再封装头文件，将需要的简单Windows定义也整合到一起
//模块名称：GRSLib V2.0
//
//组织（公司、集团）名称：One Man Group
//作者：Gamebaby Rock Sun
//日期：2010-5-20 14:27:19
//修订日期：2017-11-25
//===================================================================================
#pragma once
//设置简体中文为默认代码页
#pragma setlocale("chinese-simplified")

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN             // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <process.h> //for _beginthread, _endthread 
#include <locale.h>

//Winsock & Net API
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windns.h>
#include <iphlpapi.h>
#include <MSWSOCK.h> 	

//添加WTL支持 方便使用COM
#include <wrl.h>
using namespace Microsoft;
using namespace Microsoft::WRL;
#include <shellapi.h>

//#include <winerror.h> //for downlist code
//#ifndef __midl
//FORCEINLINE _Translates_Win32_to_HRESULT_(x) HRESULT HRESULT_FROM_WIN32(unsigned long x) { return (HRESULT)(x) <= 0 ? (HRESULT)(x) : (HRESULT)(((x) & 0x0000FFFF) | (FACILITY_WIN32 << 16) | 0x80000000); }
//#else
//#define HRESULT_FROM_WIN32(x) __HRESULT_FROM_WIN32(x)
//#endif

#pragma comment( lib, "Ws2_32.lib" )
#pragma comment( lib, "Iphlpapi.lib")
#pragma comment( lib, "Mswsock.lib" )

//COM Support
#include <comutil.h> 
#include <comip.h>	//for _com_ptr_t
#include <comdef.h> //for _com_error

#ifdef _DEBUG
#pragma comment (lib,"comsuppwd.lib")
#else
#pragma comment (lib,"comsuppw.lib")
#endif


//使用系统安全字符串函数支持
#include <strsafe.h>
//使用ATL的字符集转换支持
#include <atlconv.h>
//使用ATL集合类
#include <atlcoll.h>

using namespace ATL;



//XML功能支持
#include <msxml6.h>
#pragma comment (lib,"msxml6.lib")

//另外一种方法，直接导入DLL中的类型信息，需要时自行还原下列注释
//#import "msxml6.dll"
//using namespace MSXML2;

//IXMLDOMParseError*      pXMLErr = nullptr;
//IXMLDOMDocument*		pXMLDoc = nullptr;
//
//hr = pXMLDoc->loadXML(pXMLString, &bSuccess);
//
//GRS_SAFEFREE(pXMLData);
//
//if (VARIANT_TRUE != bSuccess || FAILED(hr))
//{
//	pXMLDoc->get_parseError(&pXMLErr);
//	ReportDocErr(argv[1], pXMLErr);
//}

//VOID ReportDocErr(LPCTSTR pszXMLFileName, IXMLDOMParseError* pIDocErr)
//{
//	GRS_USEPRINTF();
//	long lLine = 0;
//	long lCol = 0;
//	LONG lErrCode = 0;
//	BSTR bstrReason = nullptr;
//
//	pIDocErr->get_line(&lLine);
//	pIDocErr->get_linepos(&lCol);
//	pIDocErr->get_reason(&bstrReason);
//
//	GRS_PRINTF(_T("XML文档\"%s\"中(行:%ld,列:%ld)处出现错误(0x%08x),原因:%s\n")
//		, pszXMLFileName, lLine, lCol, lErrCode, bstrReason);
//
//	SysFreeString(bstrReason);
//
//}

//===================================================================================
//Debug 支持相关宏定义

//用于输出编译信息时，将其他类型的信息变成字符串型的
#ifndef GRS_STRING2
#define GRS_STRING2(x) #x
#endif // !GRS_STRING2

#ifndef GRS_STRING
#define GRS_STRING(x) GRS_STRING2(x)
#endif // !GRS_STRING

#ifndef GRS_WSTRING2
#define GRS_WSTRING2(x) L #x
#endif // !GRS_WSTRING2

#ifndef GRS_WSTRING
#define GRS_WSTRING(x) GRS_WSTRING2(x)
#endif // !GRS_WSTRING

#ifndef GRS_WIDEN2
#define GRS_WIDEN2(x) L ## x
#endif // !GRS_WIDEN2

#ifndef GRS_WIDEN
#define GRS_WIDEN(x) GRS_WIDEN2(x)
#endif // !GRS_WIDEN

#ifndef __WFILE__
#define __WFILE__ GRS_WIDEN(__FILE__)
#endif // !GRS_WIDEN

#ifndef GRS_DBGOUT_BUF_LEN
#define GRS_DBGOUT_BUF_LEN 2048
#endif

//断言定义（从GRS_Def.h文件移至这里）
#ifdef _DEBUG
#define GRS_ASSERT(s) if(!(s)) { ::DebugBreak(); }
#else
#define GRS_ASSERT(s) 
#endif

__inline void __cdecl GRSDebugOutputA(LPCSTR pszFormat, ...)
{//UNICODE版的待时间的Trace
	va_list va;
	va_start(va, pszFormat);

	CHAR pBuffer[GRS_DBGOUT_BUF_LEN] = {};

	if (S_OK != ::StringCchVPrintfA(pBuffer, sizeof(pBuffer) / sizeof(pBuffer[0]), pszFormat, va))
	{
		va_end(va);
		return;
	}
	va_end(va);
	OutputDebugStringA(pBuffer);
}

__inline void __cdecl GRSDebugOutputW(LPCWSTR pszFormat, ...)
{//UNICODE版的待时间的Trace
	va_list va;
	va_start(va, pszFormat);

	WCHAR pBuffer[GRS_DBGOUT_BUF_LEN] = {};

	if (S_OK != ::StringCchVPrintfW(pBuffer, sizeof(pBuffer) / sizeof(pBuffer[0]), pszFormat, va))
	{
		va_end(va);
		return;
	}
	va_end(va);
	OutputDebugStringW(pBuffer);
}

#ifdef UNICODE
#define GRSDebugOutput GRSDebugOutputW
#else 
#define GRSDebugOutput GRSDebugOutputA
#endif


//普通的调试输出支持
#if defined(DEBUG) | defined(_DEBUG)
#define GRS_TRACE(...)		GRSDebugOutput(__VA_ARGS__)
#define GRS_TRACEA(...)		GRSDebugOutputA(__VA_ARGS__)
#define GRS_TRACEW(...)		GRSDebugOutputW(__VA_ARGS__)
#define GRS_TRACE_LINE()	GRSDebugOutputA("%s(%d):",__FILE__, __LINE__)
#else
#define GRS_TRACE(...)
#define GRS_TRACEA(...)
#define GRS_TRACEW(...)
#define GRS_TRACE_LINE()
#endif

#define GRS_TRACE_ON1

#if defined(GRS_DBGOUT_ON1)
#define GRS_TRACE1(...)		GRSDebugOutput(__VA_ARGS__)
#define GRS_TRACEA1(...)	GRSDebugOutputA(__VA_ARGS__)
#define GRS_TRACEW1(...)	GRSDebugOutputW(__VA_ARGS__)
#define GRS_TRACE_LINE1()	GRSDebugOutputA("%s(%d):",__FILE__, __LINE__)
#else
#define GRS_TRACE1(...)
#define GRS_TRACEA1(...)
#define GRS_TRACEW1(...)
#define GRS_TRACE_LINE1()
#endif

//输出异常CGRSException
#if defined(DEBUG) | defined(_DEBUG)
#define GRS_TRACE_EXPW(Fun,e) \
			GRSDebugOutputW(L"%s(%d):%s函数捕获异常(0x%08X):%s\n"\
			,__WFILE__\
			,__LINE__\
			,L#Fun\
			,(e).GetErrCode()\
			,(e).GetReason())
#else
#define GRS_TRACE_EXPW(Fun,e)
#endif
//===================================================================================

//===================================================================================
//警告宏，用于检测运行时设置参数是否正确
#if defined(DEBUG) | defined(_DEBUG)
#define GRS_WARNING_IF(isTrue, ... )\
	{ \
		static bool s_TriggeredWarning = false; \
		if ((bool)(isTrue) && !s_TriggeredWarning)\
		{ \
			s_TriggeredWarning = true; \
			GRSDebugOutputW(L"%s(%d)中出现判定警告(条件\'%s\'为真)：%s\n"\
			,__WFILE__\
			,__LINE__\
			,GRS_WSTRING(isTrue)\
			,__VA_ARGS__);\
		} \
	}

#define GRS_WARNING_IF_NOT( isTrue, ... ) GRS_WARNING_IF(!(isTrue), __VA_ARGS__)

#else

#define GRS_WARNING_IF( isTrue, ... ) 
#define GRS_WARNING_IF_NOT( isTrue, ... )

#endif
//===================================================================================

//===================================================================================
//仿C 内存管理库函数 宏定义 直接调用相应WinAPI 提高效率
#ifndef GRS_ALLOC
#define GRS_ALLOC(sz)		HeapAlloc(GetProcessHeap(),0,sz)
#endif // !GRS_ALLOC

#ifndef GRS_CALLOC
#define GRS_CALLOC(sz)		HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sz)
#endif // !GRS_CALLOC

#ifndef GRS_REALLOC
#define GRS_REALLOC(p,sz)	HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,p,sz)
#endif // !GRS_REALLOC

#ifndef GRS_FREE
#define GRS_FREE(p)			HeapFree(GetProcessHeap(),0,p)
#endif // !GRS_FREE

#ifndef GRS_MSIZE
#define GRS_MSIZE(p)		HeapSize(GetProcessHeap(),0,p)
#endif // !GRS_MSIZE

#ifndef GRS_MVALID
#define GRS_MVALID(p)		HeapValidate(GetProcessHeap(),0,p)
#endif // !GRS_MVALID

#ifndef GRS_SAFEFREE
//下面这个表达式可能会引起副作用,p参数尽量使用指针变量,而不是表达式
#define GRS_SAFE_FREE(p) if( nullptr != (p) ){ HeapFree(GetProcessHeap(),0,(p));(p)=nullptr; }
#endif // !GRS_SAFEFREE

#ifndef GRS_SAFE_DELETE
//下面这个表达式可能会引起副作用,p参数尽量使用指针变量,而不是表达式
#define GRS_SAFE_DELETE(p) if( nullptr != (p) ){ delete p; (p) = nullptr; }
#endif // !GRS_SAFEFREE

#ifndef GRS_SAFE_DELETE_ARRAY
//下面这个表达式可能会引起副作用,p参数尽量使用指针变量,而不是表达式
#define GRS_SAFE_DELETE_ARRAY(p) if( nullptr != (p) ){ delete[] p; (p) = nullptr; }
#endif // !GRS_SAFEFREE


#ifndef GRS_OPEN_HEAP_LFH
//下面这个宏用于打开堆的LFH特性,以提高性能
#define GRS_OPEN_HEAP_LFH(h) \
    ULONG  ulLFH = 2;\
    HeapSetInformation((h),HeapCompatibilityInformation,&ulLFH ,sizeof(ULONG) ) ;
#endif // !GRS_OPEN_HEAP_LFH
//===================================================================================

//===================================================================================
//常用的宏定义 统一收集在这里
#ifndef GRS_NOVTABLE
#define GRS_NOVTABLE __declspec(novtable)
#endif // !GRS_NOVTABLE

#ifndef GRS_TLS
//TLS 存储支持
#define GRS_TLS				__declspec( thread )
#endif // !GRS_TLS

//内联定义
#ifndef GRS_INLINE
#define GRS_INLINE			__inline
#endif

#ifndef GRS_FORCEINLINE
#define GRS_FORCEINLINE		__forceinline
#endif

#ifndef GRS_NOVTABLE
#define GRS_NOVTABLE		__declspec(novtable)
#endif

#ifndef GRS_CDECL
#define GRS_CDECL			__cdecl
#endif

#ifndef GRS_STDCL 
#define GRS_STDCL			__stdcall
#endif

#ifndef GRS_FASTCL
#define GRS_FASTCL			__fastcall
#endif

#ifndef GRS_PASCAL
#define GRS_PASCAL			__pascal
#endif

//取静态数组元素的个数
#ifndef GRS_COUNTOF
#define GRS_COUNTOF(a)		sizeof(a)/sizeof(a[0])
#endif

//将除法结果向上取整，如3/2 = 2
#ifndef GRS_UPDIV
#define GRS_UPDIV(a,b) ((a) + (b) - 1)/(b)
#endif

//将a对齐到b的整数倍
#ifndef GRS_UPROUND
#define GRS_UPROUND(x,y) (((x)+((y)-1))&~((y)-1))
#endif

//将a增大对齐到4K的最小整数倍
#ifndef GRS_UPROUND4K
#define GRS_UPROUND4K(a) GRS_UPROUND(a,4096)
#endif

//不使用的参数
#ifndef GRS_UNUSE
#define GRS_UNUSE(a) a;
#endif

#ifndef GRS_SLEEP
//利用等待线程句柄自己精确延时一定的毫秒数,用于替代不精确的系统函数Sleep
#define GRS_SLEEP(dwMilliseconds) WaitForSingleObject(GetCurrentThread(),dwMilliseconds)
#endif // !GRS_SLEEP

#ifndef GRS_FOREVERLOOP
//《代码大全》中建议的死循环宏定义
#define GRS_FOREVERLOOP for(;;)        
#endif // !GRS_FOREVERLOOP

//复制句柄的宏定义
#ifndef GRS_DUPHANDLE
//复制一个句柄，继承标志关闭
#define GRS_DUPHANDLE(h,hDup)	DuplicateHandle(GetCurrentProcess(),(h),GetCurrentProcess(),&(hDup),0,FALSE,DUPLICATE_SAME_ACCESS)
#endif // !GRS_DUPHANDLE

#ifndef GRS_DUPHANDLEI
//复制一个句柄，继承标志打开
#define GRS_DUPHANDLEI(h,hDup)	DuplicateHandle(GetCurrentProcess(),(h),GetCurrentProcess(),&(hDup),0,TRUE,DUPLICATE_SAME_ACCESS)
#endif // !GRS_DUPHANDLEI

#ifndef GRS_SETHANDLE_NI
//将一个句柄设置为不能继承
#define GRS_SETHANDLE_NI(h)		SetHandleInformation(h,HANDLE_FLAG_INHERIT,0)
#endif // !GRS_SETHANDLE_NI

#ifndef GRS_SETHANDLE_NC
//将一个句柄设置为不能用CloseHandle关闭
#define GRS_SETHANDLE_NC(h)		SetHandleInformation(h,HANDLE_FLAG_PROTECT_FROM_CLOSE,HANDLE_FLAG_PROTECT_FROM_CLOSE)
#endif // !GRS_SETHANDLE_NC

#ifndef GRS_SETHANDLE_I
//将一个句柄设置为能继承
#define GRS_SETHANDLE_I(h)		SetHandleInformation(h,HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT)
#endif // !GRS_SETHANDLE_I

#ifndef GRS_SETHANDLE_C
//将一个句柄设置为能够用CloseHandle关闭
#define GRS_SETHANDLE_C(h)		SetHandleInformation(h,HANDLE_FLAG_PROTECT_FROM_CLOSE,0)
#endif // !GRS_SETHANDLE_C

#ifndef GRS_SAFE_CLOSEHANDLE
//安全关闭一个句柄
#define GRS_SAFE_CLOSEHANDLE(h) if(nullptr != (h)){CloseHandle(h);(h)=nullptr;}
#endif // !GRS_SAFE_CLOSEHANDLE

#ifndef GRS_SAFE_CLOSEFILE
//安全关闭一个文件句柄
#define GRS_SAFE_CLOSEFILE(h) if(INVALID_HANDLE_VALUE != (h)){CloseHandle(h);(h)=INVALID_HANDLE_VALUE;}
#endif // !GRS_SAFE_CLOSEFILE
//=======================================结束========================================

//===================================================================================
//注意DEBUG版的GRS_SAFE_RELEASE要求最后Release的时候计数为0，只是为了调试方便
//根据不同COM接口内部计数器实现的不一样，因此并不是所有的接口在释放时返回的计数都是0
//这个断言只有提醒之意，具体代码要具体分析，感觉使用不方便时，请注释那个GRS_ASSERT
#ifndef GRS_SAFE_RELEASE
#define GRS_SAFE_RELEASE(I) \
	if(nullptr != (I))\
	{\
		ULONG ulRefCnt = (I)->Release();\
		(I) = nullptr;\
	}
#endif // !GRS_SAFE_RELEASE

#ifndef GRS_SAFE_RELEASE_ASSERT
#define GRS_SAFE_RELEASE_ASSERT(I) \
	if(nullptr != (I))\
	{\
	ULONG ulRefCnt = (I)->Release();\
	GRS_ASSERT(0 == ulRefCnt);\
	(I) = nullptr;\
	}
#endif // !GRS_SAFE_RELEASE_ASSERT
//===================================================================================