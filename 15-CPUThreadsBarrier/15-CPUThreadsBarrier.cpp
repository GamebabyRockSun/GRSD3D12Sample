#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <atlconv.h>
#include <atlcoll.h>  //for atl array
#include <strsafe.h>  //for StringCchxxxxx function
#include <time.h>	// for time

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 Render Thread Pool Based On Barrier Kernel Objects Sample")

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

//利用等待线程句柄自己精确延时一定的毫秒数,用于替代不精确的系统函数Sleep
#define GRS_SLEEP(dwMilliseconds) WaitForSingleObject(GetCurrentThread(),dwMilliseconds)

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
	//当前渲染Pass的序号，当然如果你乐意，完全可以用更有意义的String来代替Pass的序号，使用map来关联Pass名称和对应的资源
	UINT								m_nPassNO;					
 };

// 假设我们需要g_nMaxThread个渲染子线程，若需要动态调整就需要将渲染子线程参数用动态数组包装，如：CAtlArray、stl::vector等
const UINT								g_nMaxThread = 64;
ST_GRS_THREAD_PARAMS					g_stThreadParams[g_nMaxThread] = {};

// 线程池管理内核同步对象
HANDLE									g_hEventPassOver;
HANDLE									g_hSemaphore;
SYNCHRONIZATION_BARRIER					g_stCPUThdBarrier = {};


UINT __stdcall RenderThread(void* pParam);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	int						iWndWidth = 1024;
	int						iWndHeight = 768;
	HWND					hWnd = nullptr;
	MSG						msg = {};

	UINT					nMaxPass = 5; //假设一帧渲染需要至少5遍渲染（5 Pass），可以根据你的渲染手法（technology）来动态调整
	UINT					nPassNO = 0;    //当前渲染Pass的序号
	UINT					nFrameCnt = 0; //总渲染帧数

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

			// 最大化命令行窗口
			ShowWindow(GetConsoleWindow(), SW_MAXIMIZE);    //窗体最大化
		}

		// 创建用于CPU子渲染线程池的同步对象
		{
			// 通知线程池启动渲染的信标量，初始信标个数为0，最大信标个数即线程池线程个数
			g_hSemaphore = ::CreateSemaphore(nullptr, 0, g_nMaxThread, nullptr);
			if (!g_hSemaphore)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}
			
			// 线程池Pass渲染结束事件句柄
			g_hEventPassOver = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (!g_hEventPassOver)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			// 创建CPU线程屏障对象，允许进入的线程数即线程池线程个数
			if (!InitializeSynchronizationBarrier(&g_stCPUThdBarrier, g_nMaxThread, -1))
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		// 启动多个渲染线程
		{
			GRS_PRINTF(_T("主线程启动【%d】个子线程\n"),g_nMaxThread);
			// 设置各线程的共性参数
			for (int i = 0; i < g_nMaxThread; i++)
			{
				//编排线程序号，使得CPU线程池看上去像个GPU上的Computer Engine，每个线程都有序号
				//当然如果你高兴，你也可以进一步模仿编排为GPU Thread Box的3D序号
				g_stThreadParams[i].m_nThreadIndex = i;		
				//初始化渲染遍数的序号为0，只要是渲染至少都应该有一遍渲染，不然启动渲染线程池干嘛？
				g_stThreadParams[i].m_nPassNO = 0;			

				//以暂停方式创建线程（线程的暂停创建方式是一种优雅的方式，看教程六搞明白为啥）
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

		DWORD dwRet = 0;
		BOOL bExit = FALSE;

		// 创建一个计时器对象，模拟GPU渲染时的延时
		HANDLE hTimer = CreateWaitableTimer(NULL, FALSE, NULL);
		LARGE_INTEGER liDueTime = {};
		liDueTime.QuadPart = -1i64;//1秒后开始计时
		SetWaitableTimer(hTimer, &liDueTime, 10, NULL, NULL, 0);

		//这里添加定时器对象的意思表示模拟GPU进行第二遍Copy动作的延时
		arHWaited.Add(hTimer);

		// 初始化完了再显示窗口
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		//设置下随机数发生器种子
		srand((unsigned)time(NULL));

		GRS_PRINTF(_T("主线程开始消息循环:\n"));
		
		//15、开始主消息循环，并在其中不断渲染
		while (!bExit)
		{
			//主线程进入等待
			// 注意这里 fWaitAll = FALSE了，
			// 这样，实质上我们可以在arHWaited数组中添加任意其他需要等待的对象句柄
			// 而被添加的句柄都可以单独促使MsgWaitForMultipleObjects返回
			// 从而可以利用返回值精确高效的处理对应句柄表示的事件
			// 如：Input、Music、Sound、Net、Video等等
			dwRet = ::MsgWaitForMultipleObjects(static_cast<DWORD>(arHWaited.GetCount()), arHWaited.GetData(), FALSE, INFINITE, QS_ALLINPUT);
			dwRet -= WAIT_OBJECT_0;
			switch (dwRet)
			{
			case 0:
			{
				if ( nPassNO < nMaxPass )
				{//
					arHWaited.RemoveAll();
					// 准备等待第n遍渲染结束
					arHWaited.Add(g_hEventPassOver);

					GRS_PRINTF(_T("主线程通知子线程开始第【%d】遍渲染\n"), nPassNO);

					//通知各线程开始渲染
					for (int i = 0; i < g_nMaxThread; i++)
					{
						//设定子线程渲染Pass序号
						g_stThreadParams[i].m_nPassNO = nPassNO;
					}
					// 释放信标量至最大子线程数，相当于通知所有子线程开始渲染运行
					ReleaseSemaphore(g_hSemaphore, g_nMaxThread, nullptr);

					GRS_PRINTF(_T("  主线程进行第【%d】遍渲染\n"), nPassNO);
					// Sleep(20ms) 模拟主线程的第nPassNo遍渲染
					GRS_SLEEP(20);

					++nPassNO; //递增到下一遍渲染
				}
				else
				{// 一帧渲染结束了，可以ExecuteCommandLists了
					arHWaited.RemoveAll();
					// 模拟等待GPU同步
					arHWaited.Add(hTimer);
					
					// 接着你可以动态的调整下一帧需要的Pass数量，我们这里设置个5以内的随机数模拟这种情况
					nMaxPass = rand() % 5 + 1;
					nPassNO = 0;

					GRS_PRINTF(_T("主线程完成第【%d】帧渲染，下一帧需要【%d】遍渲染\n"), nFrameCnt,nMaxPass);
					++nFrameCnt;
				}
			}
			break;
			case 1:
			{
				GRS_PRINTF(_T("主线程（第【%d】遍渲染中）：处理消息\n"), nPassNO);
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
			break;
			default:
			{

			}
			break;
			}

			//---------------------------------------------------------------------------------------------
			// 检测一下线程的活动情况，如果有线程已经退出了，就退出主线程循环
			// 当然这个检测可以更灵活，不一定每次都检测，为了示例简洁性搞成这样，你可以设置n次循环后检查一次，或一帧结束后检查一次等等
			// 另外也可以在这里伺机进行复杂的线程池伸缩（增减子线程数量操作）等，动态调整下一轮循环（或下一场景）需要的线程池线程数量等
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
			// 关闭子线程内核对象句柄
			::CloseHandle(g_stThreadParams[i].m_hThisThread);
		}
		// 关闭信标量
		::CloseHandle(g_hSemaphore);
		// 删除CPU线程屏障
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

	//子线程输出空两个空格
	TCHAR pszPreSpace[] = _T("  ");
	
	GRS_USEPRINTF();
	
	try
	{
		if (nullptr == pThdPms)
		{//参数异常，抛异常终止线程
			throw CGRSCOMException(E_INVALIDARG);
		}

		GRS_PRINTF(_T("%s子线程【%d】开始进入渲染循环\n"), pszPreSpace,pThdPms->m_nThreadIndex);
		//6、渲染循环
		while (!bQuit)
		{
			// 等待主线程通知开始渲染，同时仅接收主线程Post过来的消息，目前就是为了等待WM_QUIT消息
			dwRet = ::MsgWaitForMultipleObjects(1, &g_hSemaphore, FALSE, INFINITE, QS_ALLPOSTMESSAGE);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{
				// OnUpdate()
				{
					GRS_PRINTF(_T("%s子线程【%d】执行第【%d】遍渲染：OnUpdate()\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nPassNO);
				}

				// OnThreadRender()
				{
					GRS_PRINTF(_T("%s子线程【%d】执行第【%d】遍渲染：OnThreadRender()\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nPassNO);
				}

				//延迟20毫秒，模拟子线程渲染运行
				GRS_SLEEP(20);

				//完成渲染（即关闭命令列表，并设置同步对象通知主线程开始执行）

				if (EnterSynchronizationBarrier(&g_stCPUThdBarrier, 0))
				{
					GRS_PRINTF(_T("%s子线程【%d】：通知主线程完成第【%d】遍渲染\n"), pszPreSpace, pThdPms->m_nThreadIndex, pThdPms->m_nPassNO);

					SetEvent(g_hEventPassOver);
				}
			}
			break;
			case 1:
			{//处理消息
				GRS_PRINTF(_T("%s子线程【%d】：处理消息\n"), pszPreSpace, pThdPms->m_nThreadIndex);
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
