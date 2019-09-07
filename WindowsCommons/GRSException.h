//===================================================================================
//项目名称：GRS 公共平台 V2.0
//文件描述：异常类声明 以及异常模块接口定义文件
//模块名称：C/C++全部异常支持模块
//
//组织（公司、集团）名称：One Man Group
//作者：Gamebaby Rock Sun
//日期：2010-5-20 14:29:45
//修订日期：2011-11-21
//===================================================================================
#include "GRS_Win.h"
#include <eh.h>

//如果定义了GRS_EXPORT宏 ，那么使用GRS_DLL定义的类将变成导出类
#ifdef GRSEXCEPTION_EXPORTS
#define GRS_EXPORT
#endif
#include "GRS_DLL.h"

#pragma once

#define GRS_ERRBUF_MAXLEN	1024

//异常类，这个类将是整个类库中唯一的异常类
class GRS_DLL CGRSException
{
	enum EM_GRS_EXCEPTION
	{//异常的具体种类的枚举值，用于具体区分详细的异常信息
		ET_Empty = 0x0,
		ET_SE,				
		ET_LastError,
		ET_COM,
		ET_Customer
	};
protected:
	EM_GRS_EXCEPTION m_EType;		//异常类型
	UINT m_nSEHCode;
	DWORD m_dwLastError;

	EXCEPTION_POINTERS *m_EP;
	TCHAR m_lpErrMsg[GRS_ERRBUF_MAXLEN];
public:
	CGRSException& operator = ( CGRSException& ) = delete;
public:
	CGRSException()
		: m_nSEHCode(0)	, m_EP(nullptr),m_dwLastError(0),m_lpErrMsg(),m_EType(ET_Empty)
	{
	}

	//CGRSException(CGRSException& e)
	//{
	//}

	CGRSException( LPCTSTR pszSrcFile, INT iLine, HRESULT hr )
		: m_nSEHCode(0), m_EP(nullptr), m_dwLastError(0), m_lpErrMsg(), m_EType(ET_COM)
	{
		_com_error ce(hr);
		StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN, _T("%s(%d):发生COM错误0x%X，原因%s")
			, pszSrcFile
			, iLine
			, hr
			, ce.ErrorMessage());
	}

	CGRSException( LPCTSTR pszSrcFile, INT iLine )
		:m_nSEHCode(0), m_EP(nullptr), m_dwLastError(0), m_lpErrMsg(), m_EType(ET_LastError)
	{
		ToString( pszSrcFile, iLine, ::GetLastError() );
	}

	CGRSException( UINT nCode,EXCEPTION_POINTERS* ep )
		:m_nSEHCode(nCode),m_EP(ep),m_dwLastError(0),m_lpErrMsg(),m_EType(ET_SE)
	{//结构化异常
		//以下开关语句中的常量定义在ntstatus.h中 
		switch(m_nSEHCode)
		{
		case 0xC0000029://STATUS_INVALID_UNWIND_TARGET:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("展开(unwind)操作中遭遇无效的展开位置"));
			}
			break;
		case EXCEPTION_ACCESS_VIOLATION:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程尝试对虚地址的一次无效访问."));
			}
			break;
		case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程尝试对有硬件数组边界检查的一次越界访问."));
			}
			break;
		case EXCEPTION_BREAKPOINT:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("遇到断点."));
			}
			break;
		case EXCEPTION_DATATYPE_MISALIGNMENT:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程尝试在不支持对齐的硬件上访问未对齐的数据."));
			}
			break;
		case EXCEPTION_FLT_DENORMAL_OPERAND:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("浮点数操作数太小."));
			}
			break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("浮点数被零除."));
			}
			break;
		case EXCEPTION_FLT_INEXACT_RESULT:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("浮点数结果无法被正确描述."));
			}
			break;
		case EXCEPTION_FLT_INVALID_OPERATION:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("未知的浮点数异常."));
			}
			break;
		case EXCEPTION_FLT_OVERFLOW:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("浮点数操作中指数高于指定类型允许的数量级."));
			}
			break;
		case EXCEPTION_FLT_STACK_CHECK:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("浮点数操作中导致的栈溢出."));
			}
			break;
		case EXCEPTION_FLT_UNDERFLOW:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("浮点数操作中指数低于指定类型需要的数量级."));
			}
			break;
		case EXCEPTION_GUARD_PAGE:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程访问到被PAGE_GUARD修饰分配的内存."));
			}
			break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程试图执行不可用指令."));
			}
			break;
		case EXCEPTION_IN_PAGE_ERROR:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程试图访问一个系统当前无法载入的缺失页."));
			}
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("整数被零除."));
			}
			break;
		case EXCEPTION_INT_OVERFLOW:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("整数操作结果进位超出最高有效位."));
			}
			break;
		case EXCEPTION_INVALID_DISPOSITION:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("异常处理过程返回了一个不可用控点给异常发分发过程."));
			}
			break;
		case EXCEPTION_INVALID_HANDLE:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程使用了一个不可用的内核对象句柄(该句柄可能已关闭)."));
			}
			break;
		case EXCEPTION_NONCONTINUABLE_EXCEPTION:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程试图在不可继续的异常之后继续执行."));
			}
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程试图通过执行一条指令，完成一个不允许在当前计算机模式的操作."));
			}
			break;
		case EXCEPTION_SINGLE_STEP:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("某指令执行完毕信号, 可能来自一个跟踪陷阱或其他单指令机制."));
			}
			break;
		case EXCEPTION_STACK_OVERFLOW:
			{
				StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN,_T("线程栈空间耗尽."));
			}
			break;
		default://未处理的异常！
			StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN, _T("CGRSException类未知异常,code=0x%08X"), 
				m_nSEHCode);
			break;
		};
	}

	//CGRSException( LPCTSTR pszSrcFile, INT iLine,DWORD dwLastError )
	//	:m_nSEHCode(0),m_EP(nullptr),m_dwLastError(dwLastError),m_lpErrMsg(),m_EType(ET_LastError)
	//{
	//	ToString(pszSrcFile,iLine,dwLastError);
	//}

	//CGRSException( EM_GRS_EXCEPTION EType,LPCTSTR pszFmt,... )
	//	:m_nSEHCode(0),m_EP(nullptr),m_dwLastError(0),m_lpErrMsg(),m_EType(EType)
	//{
	//	va_list va;
	//	va_start(va, pszFmt);
	//	if( nullptr != m_lpErrMsg )
	//	{
	//		StringCchVPrintf(m_lpErrMsg,GRS_ERRBUF_MAXLEN,pszFmt,va);
	//	}
	//	va_end(va);
	//}

	CGRSException( LPCTSTR pszFmt , ... )
		:m_nSEHCode(0),m_EP(nullptr),m_dwLastError(0),m_lpErrMsg(),m_EType( ET_Customer )
	{
		va_list va;
		va_start(va, pszFmt);

		if( nullptr != m_lpErrMsg )
		{
			StringCchVPrintf(m_lpErrMsg,GRS_ERRBUF_MAXLEN,pszFmt,va);
		}
		va_end(va);
	}

	virtual ~CGRSException()
	{
	}
public:
	VOID ToString(LPCTSTR pszSrcFile, INT iLine , DWORD dwErrorCode )
	{//工具方法帮助得到一个错误代码的错误信息 
    //注意因为这个方法申请了对象内部的字符串内存，
    //为了能够在对象失效时正常释放所以没有把这个方法设计成静态方法
		ZeroMemory(m_lpErrMsg, GRS_ERRBUF_MAXLEN * sizeof(TCHAR));

		LPTSTR lpMsgBuf = nullptr;

		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, dwErrorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, nullptr);

		size_t szLen = 0;
		StringCchLength(lpMsgBuf, GRS_ERRBUF_MAXLEN, &szLen);

		if (szLen > 0)
		{
			StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN, _T("%s(%d):发生错误%d，原因%s")
				, pszSrcFile
				, iLine
				, dwErrorCode
				, lpMsgBuf);

			LocalFree(lpMsgBuf);
		}
		else
		{
			StringCchPrintf(m_lpErrMsg, GRS_ERRBUF_MAXLEN, _T("%s(%d):发生错误%d，原因不明")
				, pszSrcFile
				, iLine
				, dwErrorCode);
		}
	}

public:
	LPCTSTR GetReason() const
	{
		return m_lpErrMsg;
	}

	DWORD GetErrCode() const
	{
		return (0 == m_dwLastError)?m_nSEHCode:m_dwLastError;
	}

	EM_GRS_EXCEPTION GetType()
	{
		return m_EType;
	}

    LPCTSTR GetTypeString()
    {
        static const TCHAR* pszTypeString[] = {
            _T("Empty"),
            _T("SE"),
            _T("Thread Last Error"),
            _T("Customer"),
			_T("COM"),
            nullptr
        };

        if( m_EType < sizeof(pszTypeString)/sizeof(pszTypeString[0]))
        {
            return pszTypeString[m_EType];
        }
        return nullptr;
    }
};

__inline void GRS_SEH_Handle(unsigned int code, struct _EXCEPTION_POINTERS *ep)
{//注意下面的异常抛出方式，这样保证所有的异常在栈上并且是自动对象
	throw CGRSException(code,ep);
}

#ifndef GRS_EH_INIT
#define GRS_EH_INIT() _set_se_translator(GRS_SEH_Handle);
#endif // !GRS_EH_INIT

#ifndef GRS_THROW_LASTERROR
#define GRS_THROW_LASTERROR() throw CGRSException(__WFILE__,__LINE__);
#endif // !GRS_THROW_LASTERROR

#ifndef GRS_THROW_COM_ERROR
#define GRS_THROW_COM_ERROR(hr) throw CGRSException(__WFILE__,__LINE__,(hr));
#endif // !GRS_THROW_COM_ERROR


inline void GRSThrowIfFailed(LPCWSTR pszSrcFile,UINT nLine, HRESULT hr)
{
	if ( FAILED( hr ) )
	{
		// 在此行中设置断点，以捕获 Win32 API 错误。
		throw CGRSException(pszSrcFile,nLine,hr);
	}
}

#define GRS_THROW_IF_FAILED(hr) GRSThrowIfFailed( __WFILE__,__LINE__,(hr) )