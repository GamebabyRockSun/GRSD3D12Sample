#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <atlconv.h>
#include <atlcoll.h>  //for atl array
#include <strsafe.h>  //for StringCchxxxxx function

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 Multi Thread Shadow Sample")

//控制台输出方法
#define GRS_INIT_OUTPUT() 	if (!::AllocConsole()){throw CGRSCOMException(HRESULT_FROM_WIN32(::GetLastError()));}\
		SetConsoleTitle(GRS_WND_TITLE);
#define GRS_FREE_OUTPUT()	::_tsystem(_T("PAUSE"));\
							::FreeConsole();

#define GRS_USEPRINTF() TCHAR pBuf[1024] = {};TCHAR pszOutput[1024] = {};
#define GRS_PRINTF(...) \
    StringCchPrintf(pBuf,1024,__VA_ARGS__);\
	StringCchPrintf(pszOutput,1024,_T("【ID:% 8u】：%s"),::GetCurrentThreadId(),pBuf);\
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),pszOutput,lstrlen(pszOutput),nullptr,nullptr);

class CGRSCOMException
{
public:
	CGRSCOMException(HRESULT hr) : m_hrError(hr)
	{
	}
	HRESULT Error() const
	{
		return m_hrError;
	}
private:
	const HRESULT m_hrError;
};

// 渲染子线程参数
struct ST_GRS_THREAD_PARAMS
{
	UINT								m_nThreadIndex;				//序号
	DWORD								m_dwThisThreadID;
	HANDLE								m_hThisThread;
	DWORD								m_dwMainThreadID;
	HANDLE								m_hMainThread;

	HANDLE								m_hRunEvent;

	UINT								m_nStatus;					//当前渲染状态

};

const UINT								g_nMaxThread = 6;
ST_GRS_THREAD_PARAMS					g_stThreadParams[g_nMaxThread] = {};
HANDLE									g_hEventShadowOver;
HANDLE									g_hEventRenderOver;
SYNCHRONIZATION_BARRIER					g_stCPUThdBarrier = {};

UINT __stdcall RenderThread(void* pParam);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	int						iWndWidth = 1024;
	int						iWndHeight = 768;
	HWND					hWnd = nullptr;
	MSG						msg = {};

	CAtlArray<HANDLE>		arHWaited;
	CAtlArray<HANDLE>		arHSubThread;
	GRS_USEPRINTF();
	try
	{
		GRS_INIT_OUTPUT();
		// 创建窗口
		{
			WNDCLASSEX wcex = {};
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_GLOBALCLASS;
			wcex.lpfnWndProc = WndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);		//防止无聊的背景重绘
			wcex.lpszClassName = GRS_WND_CLASS_NAME;
			RegisterClassEx(&wcex);

			DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
			RECT rtWnd = { 0, 0, iWndWidth, iWndHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			// 计算窗口居中的屏幕坐标
			INT posX = (GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
			INT posY = (GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

			hWnd = CreateWindowW(GRS_WND_CLASS_NAME, GRS_WND_TITLE, dwWndStyle
				, posX, posY, rtWnd.right - rtWnd.left, rtWnd.bottom - rtWnd.top
				, nullptr, nullptr, hInstance, nullptr);

			if (!hWnd)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		{
			g_hEventShadowOver = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
			g_hEventRenderOver = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);

			InitializeSynchronizationBarrier(&g_stCPUThdBarrier, g_nMaxThread, -1);
		}

		// 启动多个渲染线程
		{
			GRS_PRINTF(_T("主线程启动【%d】个子线程\n"),g_nMaxThread);
			// 设置各线程的共性参数
			for (int i = 0; i < g_nMaxThread; i++)
			{
				g_stThreadParams[i].m_nThreadIndex = i;		//记录序号
				g_stThreadParams[i].m_hRunEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
				g_stThreadParams[i].m_nStatus = 0;

				//以暂停方式创建线程
				g_stThreadParams[i].m_hThisThread = (HANDLE)_beginthreadex(nullptr,
					0, RenderThread, (void*)&g_stThreadParams[i],
					CREATE_SUSPENDED, (UINT*)&g_stThreadParams[i].m_dwThisThreadID);

				//然后判断线程创建是否成功
				if (nullptr == g_stThreadParams[i].m_hThisThread
					|| reinterpret_cast<HANDLE>(-1) == g_stThreadParams[i].m_hThisThread)
				{
					throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
				}

				arHSubThread.Add(g_stThreadParams[i].m_hThisThread);
			}

			//逐一启动线程
			for (int i = 0; i < g_nMaxThread; i++)
			{
				::ResumeThread(g_stThreadParams[i].m_hThisThread);
			}
		}

		//主消息循环状态变量:0，表示需要完成GPU上的第二个Copy动作 1、表示通知各线程去渲染 2、表示各线程渲染命令记录结束可以提交执行了
		UINT nStates = 0; //初始状态为0
		DWORD dwRet = 0;
		DWORD dwWaitCnt = 0;
		UINT64 n64fence = 0;
		BOOL bExit = FALSE;

		// 创建一个计时器对象，模拟GPU渲染时的延时
		HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);
		LARGE_INTEGER liDueTime = {};
		liDueTime.QuadPart = -1i64;//1秒后开始计时
		SetWaitableTimer(hTimer, &liDueTime, 10, NULL, NULL, 0);

		arHWaited.Add(hTimer);

		// 初始化完了再显示窗口
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);
		GRS_PRINTF(_T("主线程开始消息循环:\n"));
		//15、开始主消息循环，并在其中不断渲染
		while (!bExit)
		{
			//主线程进入等待
			dwWaitCnt = static_cast<DWORD>(arHWaited.GetCount());
			dwRet = ::MsgWaitForMultipleObjects(dwWaitCnt, arHWaited.GetData(), FALSE, INFINITE, QS_ALLINPUT);
			dwRet -= WAIT_OBJECT_0;

			if (0 == dwRet)
			{
				switch (nStates)
				{
				case 0://状态0，表示等到各子线程加载资源完毕，此时执行一次命令列表完成各子线程要求的资源上传的第二个Copy命令
				{
					nStates = 1;

					arHWaited.RemoveAll();

					// 模拟等待GPU同步
					arHWaited.Add(hTimer);
					GRS_PRINTF(_T("主线程状态【0】，开始第一次执行命令列表，即完成资源上传显存的第二个Copy动作\n"));
				}
				break;
				case 1:// 状态1 等到命令队列执行结束（也即CPU等到GPU执行结束），开始新一轮渲染
				{
					//OnUpdate()
					{
						GRS_PRINTF(_T("主线程状态【1】：OnUpdate()\n"));
					}

					nStates = 2;
					arHWaited.RemoveAll();
					// 准备等待阴影渲染结束
					arHWaited.Add(g_hEventShadowOver);

					GRS_PRINTF(_T("主线程状态【1】：通知子线程开始第一遍阴影渲染\n"));

					//通知各线程开始渲染
					for (int i = 0; i < g_nMaxThread; i++)
					{
						//设定渲染状态为渲染阴影
						g_stThreadParams[i].m_nStatus = 1;

						SetEvent(g_stThreadParams[i].m_hRunEvent);
					}
				}
				break;
				case 2:// 状态2 表示所有的渲染命令列表都记录完成了，开始后处理和执行命令列表
				{
					nStates = 3;

					arHWaited.RemoveAll();
					// 准备等待正常渲染结束
					arHWaited.Add(g_hEventRenderOver);

					GRS_PRINTF(_T("主线程状态【2】：通知子线程开始第二遍正常渲染\n"));

					//通知各线程开始渲染
					for (int i = 0; i < g_nMaxThread; i++)
					{
						//设定渲染状态为第二遍正常渲染
						g_stThreadParams[i].m_nStatus = 2;

						SetEvent(g_stThreadParams[i].m_hRunEvent);
					}
				}
				break;
				case 3:
				{
					nStates = 1;

					arHWaited.RemoveAll();
					// 模拟等待GPU同步
					arHWaited.Add(hTimer);

					GRS_PRINTF(_T("主线程状态【3】：按顺序执行所有的命令列表，并准备等待GPU渲染完成通知\n"));
				}
				break;
				default:// 不可能的情况，但为了避免讨厌的编译警告或意外情况保留这个default
				{
					bExit = TRUE;
				}
				break;
				}
			}
			else if( 1 == dwRet )
			{
				GRS_PRINTF(_T("主线程状态【%d】：处理消息\n"),nStates);
				//处理消息
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_NOYIELD))
				{
					if (WM_QUIT != msg.message)
					{
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
					else
					{
						bExit = TRUE;
					}
				}
			}
			else
			{


			}

			//---------------------------------------------------------------------------------------------
			//检测一下线程的活动情况，如果有线程已经退出了，就退出循环
			dwRet = WaitForMultipleObjects(static_cast<DWORD>(arHSubThread.GetCount()), arHSubThread.GetData(), FALSE, 0);
			dwRet -= WAIT_OBJECT_0;
			if (dwRet >= 0 && dwRet < g_nMaxThread)
			{
				bExit = TRUE;
			}
		}

	}
	catch (CGRSCOMException & e)
	{//发生了COM异常
		e;
	}

	try
	{
		GRS_PRINTF(_T("主线程通知各子线程退出\n"));
		// 通知子线程退出
		for (int i = 0; i < g_nMaxThread; i++)
		{
			::PostThreadMessage(g_stThreadParams[i].m_dwThisThreadID, WM_QUIT, 0, 0);
		}

		// 等待所有子线程退出
		DWORD dwRet = WaitForMultipleObjects(static_cast<DWORD>(arHSubThread.GetCount()), arHSubThread.GetData(), TRUE, INFINITE);

		// 清理所有子线程资源
		for (int i = 0; i < g_nMaxThread; i++)
		{
			::CloseHandle(g_stThreadParams[i].m_hThisThread);
			::CloseHandle(g_stThreadParams[i].m_hRunEvent);
		}

		DeleteSynchronizationBarrier(&g_stCPUThdBarrier);
		GRS_PRINTF(_T("各子线程全部退出，主线程清理资源后退出\n"));
	}
	catch (CGRSCOMException & e)
	{//发生了COM异常
		e;
	}
	::CoUninitialize();
	GRS_FREE_OUTPUT();
	return 0;
}

UINT __stdcall RenderThread(void* pParam)
{
	ST_GRS_THREAD_PARAMS* pThdPms = static_cast<ST_GRS_THREAD_PARAMS*>(pParam);
	DWORD dwRet = 0;
	BOOL  bQuit = FALSE;
	MSG   msg = {};
	TCHAR pszPreSpace[MAX_PATH] = {};
	GRS_USEPRINTF();
	try
	{
		if (nullptr == pThdPms)
		{//参数异常，抛异常终止线程
			throw CGRSCOMException(E_INVALIDARG);
		}

		for (UINT i = 0; i <= pThdPms->m_nThreadIndex; i++)
		{
			pszPreSpace[i] = _T(' ');
		}
		GRS_PRINTF(_T("%s子线程【%d】开始进入渲染循环\n"), pszPreSpace,pThdPms->m_nThreadIndex);
		//6、渲染循环
		while (!bQuit)
		{
			// 等待主线程通知开始渲染，同时仅接收主线程Post过来的消息，目前就是为了等待WM_QUIT消息
			dwRet = ::MsgWaitForMultipleObjects(1, &pThdPms->m_hRunEvent, FALSE, INFINITE, QS_ALLPOSTMESSAGE);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{
				switch (pThdPms->m_nStatus)
				{
				case 1:
				{
					// OnUpdate()
					{
						GRS_PRINTF(_T("%s子线程【%d】状态【%d】：OnUpdate()\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nStatus);
					}

					// OnThreadRender()
					{
						GRS_PRINTF(_T("%s子线程【%d】状态【%d】：OnThreadRender()\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nStatus);
					}

					//完成渲染（即关闭命令列表，并设置同步对象通知主线程开始执行）

					if (EnterSynchronizationBarrier(&g_stCPUThdBarrier, 0))
					{
						GRS_PRINTF(_T("%s子线程【%d】状态【%d】：通知主线程完成第一遍阴影渲染\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nStatus);

						SetEvent(g_hEventShadowOver);
					}
				}
				break;
				case 2:
				{
					// OnUpdate()
					{
						GRS_PRINTF(_T("%s子线程【%d】状态【%d】：OnUpdate()\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nStatus);
					}

					// OnThreadRender()
					{
						GRS_PRINTF(_T("%s子线程【%d】状态【%d】：OnThreadRender()\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nStatus);
					}
					//完成渲染（即关闭命令列表，并设置同步对象通知主线程开始执行）

					if (EnterSynchronizationBarrier(&g_stCPUThdBarrier, 0))
					{
						GRS_PRINTF(_T("%s子线程【%d】状态【%d】：通知主线程完成第二遍正常渲染\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nStatus);
						SetEvent(g_hEventRenderOver);
					}
				}
				break;
				default:
				{

				}
				break;
				}

			}
			break;
			case 1:
			{//处理消息
				GRS_PRINTF(_T("%s子线程【%d】状态【%d】：处理消息\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nStatus);
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
				{//这里只可能是别的线程发过来的消息，用于更复杂的场景
					if (WM_QUIT != msg.message)
					{
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
					else
					{
						bQuit = TRUE;
					}
				}
			}
			break;
			case WAIT_TIMEOUT:
				break;
			default:
				break;
			}
		}

		GRS_PRINTF(_T("%s子线程【%d】退出\n"), pszPreSpace, pThdPms->m_nThreadIndex);
	}
	catch (CGRSCOMException&)
	{

	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	{

	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
