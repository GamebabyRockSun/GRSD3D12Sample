#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <fstream>		//for ifstream
#include <wrl.h>		//添加WTL支持 方便使用COM
#include <atlconv.h>	//for T2A
#include <atlcoll.h>	//for atl array
#include <strsafe.h>	//for StringCchxxxxx function
#include <dxgi1_6.h>
#include <d3d12.h>		//for d3d12
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#include <DirectXMath.h>

#include "..\WindowsCommons\DDSTextureLoader12.h"

using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS D3D12 MultiAdapter Sample")

#define GRS_SAFE_RELEASE(p) if(p){(p)->Release();(p)=nullptr;}
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); }}

//新定义的宏用于上取整除法
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

// 内存分配的宏定义
#define GRS_ALLOC(sz)		::HeapAlloc(GetProcessHeap(),0,(sz))
#define GRS_CALLOC(sz)		::HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(sz))
#define GRS_CREALLOC(p,sz)	::HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(p),(sz))
#define GRS_SAFE_FREE(p)	if( nullptr != (p) ){ ::HeapFree( ::GetProcessHeap(),0,(p) ); (p) = nullptr; }

//------------------------------------------------------------------------------------------------------------
// 为了调试加入下面的内联函数和宏定义，为每个接口对象设置名称，方便查看调试输出
#if defined(_DEBUG)
inline void GRS_SetD3D12DebugName(ID3D12Object* pObject, LPCWSTR name)
{
	pObject->SetName(name);
}

inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		pObject->SetName(_DebugName);
	}
}
#else

inline void GRS_SetD3D12DebugName(ID3D12Object*, LPCWSTR)
{
}
inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_D3D12_DEBUGNAME(x)						GRS_SetD3D12DebugName(x, L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED(x, n)			GRS_SetD3D12DebugNameIndexed(x[n], L#x, n)

#define GRS_SET_D3D12_DEBUGNAME_COMPTR(x)				GRS_SetD3D12DebugName(x.Get(), L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(x, n)	GRS_SetD3D12DebugNameIndexed(x[n].Get(), L#x, n)

#if defined(_DEBUG)
inline void GRS_SetDXGIDebugName(IDXGIObject* pObject, LPCWSTR name)
{
	size_t szLen = 0;
	StringCchLengthW(name, 50, &szLen);
	pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen - 1), name);
}

inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject* pObject, LPCWSTR name, UINT index)
{
	size_t szLen = 0;
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		StringCchLengthW(_DebugName, _countof(_DebugName), &szLen);
		pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen), _DebugName);
	}
}
#else

inline void GRS_SetDXGIDebugName(IDXGIObject*, LPCWSTR)
{
}
inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_DXGI_DEBUGNAME(x)						GRS_SetDXGIDebugName(x, L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED(x, n)			GRS_SetDXGIDebugNameIndexed(x[n], L#x, n)

#define GRS_SET_DXGI_DEBUGNAME_COMPTR(x)				GRS_SetDXGIDebugName(x.Get(), L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED_COMPTR(x, n)		GRS_SetDXGIDebugNameIndexed(x[n].Get(), L#x, n)
//------------------------------------------------------------------------------------------------------------

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

// 顶点结构
struct ST_GRS_VERTEX_MODULE
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

// 辅助显卡上渲染单位矩形的顶点结构
struct ST_GRS_VERTEX_QUAD
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
};

const UINT g_nFrameBackBufCount = 3u;
// 显卡参数集合
struct ST_GRS_GPU_PARAMS
{
	UINT								m_nIndex;
	UINT								m_nszRTV;
	UINT								m_nszSRVCBVUAV;

	ComPtr<ID3D12Device4>				m_pID3D12Device4;
	ComPtr<ID3D12CommandQueue>			m_pICmdQueue;
	ComPtr<ID3D12DescriptorHeap>		m_pIDHRTV;
	ComPtr<ID3D12Resource>				m_pIRTRes[g_nFrameBackBufCount];
	ComPtr<ID3D12CommandAllocator>		m_pICmdAllocPerFrame[g_nFrameBackBufCount];
	ComPtr<ID3D12GraphicsCommandList>	m_pICmdList;
	ComPtr<ID3D12Heap>					m_pICrossAdapterHeap;
	ComPtr<ID3D12Resource>				m_pICrossAdapterResPerFrame[g_nFrameBackBufCount];
	ComPtr<ID3D12Fence>					m_pIFence;
	ComPtr<ID3D12Fence>					m_pISharedFence;
	ComPtr<ID3D12Resource>				m_pIDSTex;
	ComPtr<ID3D12DescriptorHeap>		m_pIDHDSVTex;
};

// 常量缓冲区
struct ST_GRS_MVP
{
	XMFLOAT4X4 m_MVP;			//经典的Model-view-projection(MVP)矩阵.
};

// 场景物体参数
struct ST_GRS_MODULE_PARAMS
{
	UINT								nIndex;
	XMFLOAT4							v4ModelPos;
	TCHAR								pszDDSFile[MAX_PATH];
	CHAR								pszMeshFile[MAX_PATH];
	UINT								nVertexCnt;
	UINT								nIndexCnt;
	ComPtr<ID3D12Resource>				pITexture;
	ComPtr<ID3D12Resource>				pITextureUpload;
	ComPtr<ID3D12DescriptorHeap>		pISampleHp;
	ComPtr<ID3D12Resource>				pIVertexBuffer;
	ComPtr<ID3D12Resource>				pIIndexsBuffer;
	ComPtr<ID3D12DescriptorHeap>		pISRVCBVHp;
	ComPtr<ID3D12Resource>			    pIConstBuffer;
	ComPtr<ID3D12CommandAllocator>		pIBundleAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pIBundle;
	ST_GRS_MVP* pMVPBuf;
	D3D12_VERTEX_BUFFER_VIEW			stVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				stIndexsBufferView;
};


//初始的默认摄像机的位置
XMFLOAT3 g_f3EyePos = XMFLOAT3(0.0f, 5.0f, -10.0f);  //眼睛位置
XMFLOAT3 g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
XMFLOAT3 g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //头部正上方位置

float g_fYaw = 0.0f;								// 绕正Z轴的旋转量.
float g_fPitch = 0.0f;								// 绕XZ平面的旋转量
double g_fPalstance = 10.0f * XM_PI / 180.0f;		//物体旋转的角速度，单位：弧度/秒


LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX_MODULE*& ppVertex, UINT*& ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	::CoInitialize(nullptr);  //for WIC & COM
	try
	{
		UINT								nWndWidth = 1024;
		UINT								nWndHeight = 768;
		HWND								hWnd = nullptr;
		MSG									msg = {};
		TCHAR								pszAppPath[MAX_PATH] = {};

		
		UINT								nCurrentFrameIndex = 0u;
		DXGI_FORMAT							emRTFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT							emDSFmt = DXGI_FORMAT_D24_UNORM_S8_UINT;

		D3D12_VIEWPORT						stViewPort = { 0.0f, 0.0f, static_cast<float>(nWndWidth), static_cast<float>(nWndHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D12_RECT							stScissorRect = { 0, 0, static_cast<LONG>(nWndWidth), static_cast<LONG>(nWndHeight) };

		const UINT							nMaxGPUParams = 2;			//演示仅支持两个GPU进行渲染
		const UINT							nIDGPUMain = 0;
		const UINT							nIDGPUSecondary = 1;
		ST_GRS_GPU_PARAMS					stGPUParams[nMaxGPUParams] = {};

		// 场景物体参数
		const UINT							nMaxObject = 3;
		const UINT							nSphere = 0;
		const UINT							nCube = 1;
		const UINT							nPlane = 2;
		ST_GRS_MODULE_PARAMS				stModuleParams[nMaxObject] = {};

		// 常量缓冲区大小上对齐到256Bytes边界
		SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);

		// 用于复制渲染结果的复制命令队列、命令分配器、命令列表对象
		ComPtr<ID3D12CommandQueue>			pICmdQueueCopy;
		ComPtr<ID3D12CommandAllocator>		pICmdAllocCopyPerFrame[g_nFrameBackBufCount];
		ComPtr<ID3D12GraphicsCommandList>	pICmdListCopy;

		//当不能直接共享资源时，就在辅助显卡上创建独立的资源，用于复制主显卡渲染结果过来
		ComPtr<ID3D12Resource>				pISecondaryAdapterTexutrePerFrame[g_nFrameBackBufCount];
		BOOL								bCrossAdapterTextureSupport = FALSE;
		D3D12_RESOURCE_DESC					stRenderTargetDesc = {};
		const float							arf4ClearColor[4] = { 0.2f, 0.5f, 1.0f, 1.0f };

		ComPtr<IDXGIFactory5>				pIDXGIFactory5;
		ComPtr<IDXGIFactory6>				pIDXGIFactory6;
		ComPtr<IDXGISwapChain1>				pISwapChain1;
		ComPtr<IDXGISwapChain3>				pISwapChain3;

		ComPtr<ID3DBlob>					pIVSMain;
		ComPtr<ID3DBlob>					pIPSMain;
		ComPtr<ID3D12RootSignature>			pIRSMain;
		ComPtr<ID3D12PipelineState>			pIPSOMain;

		ComPtr<ID3DBlob>					pIVSQuad;
		ComPtr<ID3DBlob>					pIPSQuad;
		ComPtr<ID3D12RootSignature>			pIRSQuad;
		ComPtr<ID3D12PipelineState>			pIPSOQuad;

		ComPtr<ID3D12Resource>				pIVBQuad;
		ComPtr<ID3D12Resource>				pIVBQuadUpload;
		D3D12_VERTEX_BUFFER_VIEW			pstVBVQuad;

		ComPtr<ID3D12DescriptorHeap>		pIDHSRVSecondary;
		ComPtr<ID3D12DescriptorHeap>		pIDHSampleSecondary;

		HANDLE								hEventFence = nullptr;
		UINT64								n64CurrentFenceValue = 0;
		UINT64								n64FenceValue = 1ui64;


		D3D12_HEAP_PROPERTIES stDefautHeapProps = {};
		stDefautHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		stDefautHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		stDefautHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		stDefautHeapProps.CreationNodeMask = 0;
		stDefautHeapProps.VisibleNodeMask = 0;

		D3D12_HEAP_PROPERTIES stUploadHeapProps = {};
		stUploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		stUploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		stUploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		stUploadHeapProps.CreationNodeMask = 0;
		stUploadHeapProps.VisibleNodeMask = 0;

		// 得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
		{
			if (0 == ::GetModuleFileName(nullptr, pszAppPath, MAX_PATH))
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			WCHAR* lastSlash = _tcsrchr(pszAppPath, _T('\\'));
			if (lastSlash)
			{//删除Exe文件名
				*(lastSlash) = _T('\0');
			}

			lastSlash = _tcsrchr(pszAppPath, _T('\\'));
			if (lastSlash)
			{//删除x64路径
				*(lastSlash) = _T('\0');
			}

			lastSlash = _tcsrchr(pszAppPath, _T('\\'));
			if (lastSlash)
			{//删除Debug 或 Release路径
				*(lastSlash + 1) = _T('\0');
			}
		}

		// 创建窗口
		{
			//---------------------------------------------------------------------------------------------
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
			RECT rtWnd = { 0, 0, nWndWidth, nWndHeight };
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

		// 创建DXGI Factory对象
		{
			UINT nDXGIFactoryFlags = 0U;
			// 打开显示子系统的调试支持
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// 打开附加的调试支持
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif			
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			//获取IDXGIFactory6接口
			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		// 枚举适配器创建设备
		{// 使用DXGI的新函数EnumAdapterByGpuPreference来从高到低枚举系统中的显卡
			DXGI_ADAPTER_DESC1 stAdapterDesc[nMaxGPUParams] = {};
		    IDXGIAdapter1* pIAdapterTmp = nullptr;
			UINT			i = 0;
			for (i = 0;
				(i < nMaxGPUParams) &&
				SUCCEEDED(pIDXGIFactory6->EnumAdapterByGpuPreference(
					i
					, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
					, IID_PPV_ARGS(&pIAdapterTmp)));
				++i)
			{
				GRS_THROW_IF_FAILED(pIAdapterTmp->GetDesc1(&stAdapterDesc[i]));

				if (stAdapterDesc[i].Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{//跳过软件虚拟适配器设备，
				 //注释这个if判断，可以使用一个真实GPU和一个虚拟软适配器来看双GPU的示例
					continue;
				}

				if (i == nIDGPUMain)
				{
					GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp
						, D3D_FEATURE_LEVEL_12_1
						, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pID3D12Device4)));
				}
				else
				{
					GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp
						, D3D_FEATURE_LEVEL_12_1
						, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pID3D12Device4)));
				}
				GRS_SAFE_RELEASE(pIAdapterTmp);
			}

			//---------------------------------------------------------------------------------------------
			if (nullptr == stGPUParams[nIDGPUMain].m_pID3D12Device4.Get()
				|| nullptr == stGPUParams[nIDGPUSecondary].m_pID3D12Device4.Get())
			{// 可怜的机器上居然没有两个以上的显卡 还是先退出了事 当然你可以使用软适配器凑活看下例子
				OutputDebugString(_T("\n机器中显卡数量不足两个，示例程序退出！\n"));
				throw CGRSCOMException(E_FAIL);
			}

			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPUParams[nIDGPUMain].m_pID3D12Device4);
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPUParams[nIDGPUSecondary].m_pID3D12Device4);

			TCHAR pszWndTitle[MAX_PATH] = {};
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s(GPU[0]:%s & GPU[1]:%s)")
				, pszWndTitle
				, stAdapterDesc[nIDGPUMain].Description
				, stAdapterDesc[nIDGPUSecondary].Description);
			::SetWindowText(hWnd, pszWndTitle);
		}

		// 创建命令队列
		if(TRUE)
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			for (int i = 0; i < nMaxGPUParams; i++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[i].m_pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&stGPUParams[i].m_pICmdQueue)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[i].m_pICmdQueue.Get(), _T("m_pICmdQueue"), i);

				//借用这个循环完成辅助参数的赋值，注意描述符堆上的每个描述符的大小可能因不同的GPU而不同，所以单独作为GPU的参数
				//同时将这些值一次性取出并存储，就不用每次调用函数而导致性能受损和调用错误引起的意外问题
				stGPUParams[i].m_nIndex = i;
				stGPUParams[i].m_nszRTV = stGPUParams[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				stGPUParams[i].m_nszSRVCBVUAV = stGPUParams[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}

			// 复制命令队列创建在独显上, 我们主要用独显来渲染，大量资源需要复制到独显上
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICmdQueueCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdQueueCopy);
		}

		// 创建围栏对象，以及多GPU同步围栏对象
		{
			for (int iGPUIndex = 0; iGPUIndex < nMaxGPUParams; iGPUIndex++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIFence)));
			}

			// 在主显卡上创建一个可共享的围栏对象
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateFence(0
				, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER
				, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pISharedFence)));

			// 共享这个围栏，通过句柄方式
			HANDLE hFenceShared = nullptr;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateSharedHandle(
				stGPUParams[nIDGPUMain].m_pISharedFence.Get(),
				nullptr,
				GENERIC_ALL,
				nullptr,
				&hFenceShared));

			// 在辅助显卡上打开这个围栏对象完成共享
			HRESULT hrOpenSharedHandleResult = stGPUParams[nIDGPUSecondary].m_pID3D12Device4->OpenSharedHandle(hFenceShared
				, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pISharedFence));

			// 先关闭句柄，再判定是否共享成功
			::CloseHandle(hFenceShared);

			GRS_THROW_IF_FAILED(hrOpenSharedHandleResult);

			hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hEventFence == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		// 创建交换链
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
			stSwapChainDesc.Width = nWndWidth;
			stSwapChainDesc.Height = nWndHeight;
			stSwapChainDesc.Format = emRTFmt;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				stGPUParams[nIDGPUSecondary].m_pICmdQueue.Get(),		// 使用接驳了显示器的显卡的命令队列做为交换链的命令队列
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

			//注意此处使用了高版本的SwapChain接口的函数
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);

			// 获取当前第一个供绘制的后缓冲序号
			nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
			D3D12_DESCRIPTOR_HEAP_DESC		stRTVHeapDesc = {};
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;			
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			for (int i = 0; i < nMaxGPUParams; i++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[i].m_pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&stGPUParams[i].m_pIDHRTV)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[i].m_pIDHRTV.Get(), _T("m_pIDHRTV"), i);
			}

			D3D12_CLEAR_VALUE   stClearValue = { };
			stClearValue.Format	  = stSwapChainDesc.Format;
			stClearValue.Color[0] = arf4ClearColor[0];
			stClearValue.Color[1] = arf4ClearColor[1];
			stClearValue.Color[2] = arf4ClearColor[2];
			stClearValue.Color[3] = arf4ClearColor[3];

			stRenderTargetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stRenderTargetDesc.Alignment = 0;
			stRenderTargetDesc.Format = stSwapChainDesc.Format;
			stRenderTargetDesc.Width = stSwapChainDesc.Width;
			stRenderTargetDesc.Height = stSwapChainDesc.Height;
			stRenderTargetDesc.DepthOrArraySize = 1;
			stRenderTargetDesc.MipLevels = 1;
			stRenderTargetDesc.SampleDesc.Count = stSwapChainDesc.SampleDesc.Count;
			stRenderTargetDesc.SampleDesc.Quality = stSwapChainDesc.SampleDesc.Quality;
			stRenderTargetDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			stRenderTargetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

			WCHAR pszDebugName[MAX_PATH] = {};
			for (UINT iGPUIndex = 0; iGPUIndex < nMaxGPUParams; iGPUIndex++)
			{
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = stGPUParams[iGPUIndex].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart();

				for (UINT j = 0; j < g_nFrameBackBufCount; j++)
				{
					if (iGPUIndex == nIDGPUSecondary)
					{
						GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(j, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIRTRes[j])));
					}
					else
					{
						GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommittedResource(
							&stDefautHeapProps,
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_COMMON,
							&stClearValue,
							IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIRTRes[j])));
					}

					stGPUParams[iGPUIndex].m_pID3D12Device4->CreateRenderTargetView(stGPUParams[iGPUIndex].m_pIRTRes[j].Get(), nullptr, rtvHandle);
					rtvHandle.ptr += stGPUParams[iGPUIndex].m_nszRTV;

					GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
						, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pICmdAllocPerFrame[j])));

					if (iGPUIndex == nIDGPUMain)
					{ //创建主显卡上的复制命令队列用的命令分配器，每帧使用一个分配器
						GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY
							, IID_PPV_ARGS(&pICmdAllocCopyPerFrame[j])));
					}
				}

				// 创建每个GPU的命令列表对象
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommandList(0
					, D3D12_COMMAND_LIST_TYPE_DIRECT
					, stGPUParams[iGPUIndex].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get()
					, nullptr
					, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pICmdList)));


				if (SUCCEEDED(StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPUParams[%u].m_pICmdList", iGPUIndex)))
				{
					stGPUParams[iGPUIndex].m_pICmdList->SetName(pszDebugName);
				}

				if ( iGPUIndex == nIDGPUMain )
				{// 在主显卡上创建复制命令列表对象
					GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommandList(0
						, D3D12_COMMAND_LIST_TYPE_COPY
						, pICmdAllocCopyPerFrame[nCurrentFrameIndex].Get()
						, nullptr
						, IID_PPV_ARGS(&pICmdListCopy)));

					GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdListCopy);
				}

				// 创建每个显卡上的深度蜡板缓冲区
				D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
				stDepthOptimizedClearValue.Format = emDSFmt;
				stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
				stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

				D3D12_RESOURCE_DESC stDSResDesc = {};
				stDSResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				stDSResDesc.Alignment = 0;
				stDSResDesc.Format = emDSFmt;
				stDSResDesc.Width = nWndWidth;
				stDSResDesc.Height = nWndHeight;
				stDSResDesc.DepthOrArraySize = 1;
				stDSResDesc.MipLevels = 0;
				stDSResDesc.SampleDesc.Count = 1;
				stDSResDesc.SampleDesc.Quality = 0;
				stDSResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				stDSResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

				//使用隐式默认堆创建一个深度蜡板缓冲区，
				//因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateCommittedResource(
					&stDefautHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stDSResDesc
					, D3D12_RESOURCE_STATE_DEPTH_WRITE
					, &stDepthOptimizedClearValue
					, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIDSTex)
				));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[iGPUIndex].m_pIDSTex.Get(), _T("m_pIDSTex"), iGPUIndex);

				D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
				stDepthStencilDesc.Format = emDSFmt;
				stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

				D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
				dsvHeapDesc.NumDescriptors = 1;
				dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3D12Device4->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIDHDSVTex)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[iGPUIndex].m_pIDHDSVTex.Get(), _T("m_pIDHDSVTex"), iGPUIndex);

				stGPUParams[iGPUIndex].m_pID3D12Device4->CreateDepthStencilView(stGPUParams[iGPUIndex].m_pIDSTex.Get()
					, &stDepthStencilDesc
					, stGPUParams[iGPUIndex].m_pIDHDSVTex->GetCPUDescriptorHandleForHeapStart());
			}

			// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		}

		// 创建跨适配器的共享资源堆
		if(TRUE)
		{
			{
				D3D12_FEATURE_DATA_D3D12_OPTIONS stOptions = {};
				// 通过检测带有显示输出的显卡是否支持跨显卡资源来决定跨显卡的资源如何创建
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CheckFeatureSupport(
					D3D12_FEATURE_D3D12_OPTIONS, reinterpret_cast<void*>(&stOptions), sizeof(stOptions)));

				bCrossAdapterTextureSupport = stOptions.CrossAdapterRowMajorTextureSupported;

				UINT64 n64szTexture = 0;
				D3D12_RESOURCE_DESC stCrossAdapterResDesc = {};

				if ( bCrossAdapterTextureSupport )
				{// 这表示可以在显卡间直接共享Texture，这就省去了Texture来回复制的麻烦
					// 如果支持那么直接创建跨显卡资源堆
					stCrossAdapterResDesc = stRenderTargetDesc;
					stCrossAdapterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
					stCrossAdapterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

					D3D12_RESOURCE_ALLOCATION_INFO stTextureInfo
						= stGPUParams[nIDGPUMain].m_pID3D12Device4->GetResourceAllocationInfo(0, 1, &stCrossAdapterResDesc);
					n64szTexture = stTextureInfo.SizeInBytes;
				}
				else
				{// 这表示只能在多个显卡间共享Buffer，这也是目前D3D12共享资源的最低要求，通常异构显卡目前只能共享Buffer
				 // 只能共享Buffer时，只能通过共享的Buffer来回复制Texture，在示例中就主要是复制Render Target Texture
					// 如果不支持，那么我们就需要先在主显卡上创建用于复制渲染结果的资源堆，然后再共享堆到辅助显卡上
					D3D12_PLACED_SUBRESOURCE_FOOTPRINT stResLayout = {};
					stGPUParams[nIDGPUMain].m_pID3D12Device4->GetCopyableFootprints(
						&stRenderTargetDesc, 0, 1, 0, &stResLayout, nullptr, nullptr, nullptr);

					n64szTexture = GRS_UPPER(stResLayout.Footprint.RowPitch * stResLayout.Footprint.Height
						, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

					stCrossAdapterResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
					stCrossAdapterResDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
					stCrossAdapterResDesc.Width = n64szTexture;
					stCrossAdapterResDesc.Height = 1;
					stCrossAdapterResDesc.DepthOrArraySize = 1;
					stCrossAdapterResDesc.MipLevels = 1;
					stCrossAdapterResDesc.Format = DXGI_FORMAT_UNKNOWN;
					stCrossAdapterResDesc.SampleDesc.Count = 1;
					stCrossAdapterResDesc.SampleDesc.Quality = 0;
					stCrossAdapterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
					stCrossAdapterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
				}

				// 创建跨显卡共享的资源堆，注意是默认堆
				D3D12_HEAP_DESC stCrossHeapDesc = {};
				stCrossHeapDesc.SizeInBytes = n64szTexture * g_nFrameBackBufCount;
				stCrossHeapDesc.Alignment = 0;
				stCrossHeapDesc.Flags = D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
				stCrossHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
				stCrossHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
				stCrossHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
				stCrossHeapDesc.Properties.CreationNodeMask = 0;
				stCrossHeapDesc.Properties.VisibleNodeMask = 0;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateHeap(&stCrossHeapDesc
					, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pICrossAdapterHeap)));

				// 在主显卡上得到共享堆的句柄
				HANDLE hHeap = nullptr;
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateSharedHandle(
					stGPUParams[nIDGPUMain].m_pICrossAdapterHeap.Get(),
					nullptr,
					GENERIC_ALL,
					nullptr,
					&hHeap));

				// 在第二个显卡上打开句柄，实质就是完成了资源堆的共享
				HRESULT hrOpenSharedHandle = stGPUParams[nIDGPUSecondary].m_pID3D12Device4->OpenSharedHandle(hHeap
					, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pICrossAdapterHeap));

				// 先关闭句柄，再判定是否共享成功
				::CloseHandle(hHeap);

				GRS_THROW_IF_FAILED(hrOpenSharedHandle);

				// 以定位方式在共享堆上创建每个显卡上的资源
				for ( UINT nFrameIndex = 0; nFrameIndex < g_nFrameBackBufCount; nFrameIndex++ )
				{
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreatePlacedResource(
						stGPUParams[nIDGPUMain].m_pICrossAdapterHeap.Get(),
						n64szTexture * nFrameIndex,
						&stCrossAdapterResDesc,
						D3D12_RESOURCE_STATE_COPY_DEST,
						nullptr,
						IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nFrameIndex])));

					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreatePlacedResource(
						stGPUParams[nIDGPUSecondary].m_pICrossAdapterHeap.Get(),
						n64szTexture * nFrameIndex,
						&stCrossAdapterResDesc,
						bCrossAdapterTextureSupport ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE,
						nullptr,
						IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nFrameIndex])));

					if (!bCrossAdapterTextureSupport)
					{
						// 如果主适配器的渲染目标必须作为缓冲区共享，那么创建一个纹理资源将其复制到辅助适配器上。
						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateCommittedResource(
							&stDefautHeapProps,
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
							nullptr,
							IID_PPV_ARGS(&pISecondaryAdapterTexutrePerFrame[nFrameIndex])));
					}
				}
			}
		}

		// 创建根签名
		if(TRUE)
		{ //这个例子中，所有物体使用相同的根签名，因为渲染过程中需要的参数是一样的
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};

			// 检测是否支持V1.1版本的根签名
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE
				, &stFeatureData, sizeof(stFeatureData))))
			{// 1.0版 直接丢异常退出了
				GRS_THROW_IF_FAILED(E_NOTIMPL);
			}

			D3D12_DESCRIPTOR_RANGE1 stDSPRanges[3] = {};

			stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			stDSPRanges[0].NumDescriptors = 1;
			stDSPRanges[0].BaseShaderRegister = 0;
			stDSPRanges[0].RegisterSpace = 0;
			stDSPRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

			stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			stDSPRanges[1].NumDescriptors = 1;
			stDSPRanges[1].BaseShaderRegister = 0;
			stDSPRanges[1].RegisterSpace = 0;
			stDSPRanges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

			stDSPRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			stDSPRanges[2].NumDescriptors = 1;
			stDSPRanges[2].BaseShaderRegister = 0;
			stDSPRanges[2].RegisterSpace = 0;
			stDSPRanges[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDSPRanges[2].OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER1 stRootParameters[3] = {};

			stRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			stRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
			stRootParameters[0].DescriptorTable.pDescriptorRanges = &stDSPRanges[0];

			stRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			stRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
			stRootParameters[1].DescriptorTable.pDescriptorRanges = &stDSPRanges[1];

			stRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			stRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
			stRootParameters[2].DescriptorTable.pDescriptorRanges = &stDSPRanges[2];


			D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc = {};
			stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
			stRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			stRootSignatureDesc.Desc_1_1.NumParameters = _countof(stRootParameters);
			stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters;
			stRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
			stRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;


			ComPtr<ID3DBlob> pISignatureBlob;
			ComPtr<ID3DBlob> pIErrorBlob;
			GRS_THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRootSignatureDesc
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSMain)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSMain);
			//------------------------------------------------------------------------------------------------

			// 创建渲染Quad的根签名对象，注意这个根签名在辅助显卡上用
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			D3D12_DESCRIPTOR_RANGE1 stDRQuad[2] = {};
			stDRQuad[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			stDRQuad[0].NumDescriptors = 1;
			stDRQuad[0].BaseShaderRegister = 0;
			stDRQuad[0].RegisterSpace = 0;
			stDRQuad[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDRQuad[0].OffsetInDescriptorsFromTableStart = 0;

			stDRQuad[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			stDRQuad[1].NumDescriptors = 1;
			stDRQuad[1].BaseShaderRegister = 0;
			stDRQuad[1].RegisterSpace = 0;
			stDRQuad[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
			stDRQuad[1].OffsetInDescriptorsFromTableStart = 0;

			D3D12_ROOT_PARAMETER1 stRPQuad[2] = {};
			stRPQuad[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRPQuad[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //SRV仅PS可见
			stRPQuad[0].DescriptorTable.NumDescriptorRanges = 1;
			stRPQuad[0].DescriptorTable.pDescriptorRanges = &stDRQuad[0];

			stRPQuad[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRPQuad[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; //SAMPLE仅PS可见
			stRPQuad[1].DescriptorTable.NumDescriptorRanges = 1;
			stRPQuad[1].DescriptorTable.pDescriptorRanges = &stDRQuad[1];

			D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRSQuadDesc = {};

			stRSQuadDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
			stRSQuadDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			stRSQuadDesc.Desc_1_1.NumParameters = _countof(stRPQuad);
			stRSQuadDesc.Desc_1_1.pParameters = stRPQuad;
			stRSQuadDesc.Desc_1_1.NumStaticSamplers = 0;
			stRSQuadDesc.Desc_1_1.pStaticSamplers = nullptr;

			pISignatureBlob.Reset();
			pIErrorBlob.Reset();
			GRS_THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRSQuadDesc
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSQuad)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSQuad);
		}

		// 编译Shader创建渲染管线状态对象
		if(TRUE)
		{
			UINT nShaderCompileFlags = 0;
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
			//编译为行矩阵形式	   
			nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s7-D3D12MultiAdapter\\Shader\\TextureModule.hlsl"), pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSMain, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSMain, nullptr));

			// 我们多添加了一个法线的定义，但目前Shader中我们并没有使用
			D3D12_INPUT_ELEMENT_DESC stIALayoutSphere[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// 创建 graphics pipeline state object (PSO)对象
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.InputLayout = { stIALayoutSphere, _countof(stIALayoutSphere) };
			stPSODesc.pRootSignature = pIRSMain.Get();
			stPSODesc.VS.BytecodeLength = pIVSMain->GetBufferSize();
			stPSODesc.VS.pShaderBytecode = pIVSMain->GetBufferPointer();
			stPSODesc.PS.BytecodeLength = pIPSMain->GetBufferSize();
			stPSODesc.PS.pShaderBytecode = pIPSMain->GetBufferPointer();

			stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
			stPSODesc.BlendState.IndependentBlendEnable = FALSE;
			stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.SampleDesc.Count = 1;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRTFmt;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.DepthStencilState.DepthEnable = TRUE;			//打开深度缓冲		
			stPSODesc.DSVFormat = emDSFmt;
			stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//启用深度缓存写入功能
			stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //深度测试函数（该值为普通的深度测试，即较小值写入）				

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPSOMain)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOMain);

			//---------------------------------------------------------------------------------------------------
			//用于渲染单位矩形的管道状态对象
			TCHAR pszUnitQuadShader[MAX_PATH] = {};
			StringCchPrintf(pszUnitQuadShader, MAX_PATH, _T("%s7-D3D12MultiAdapter\\Shader\\UnitQuad.hlsl"), pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszUnitQuadShader, nullptr, nullptr
				, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSQuad, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszUnitQuadShader, nullptr, nullptr
				, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSQuad, nullptr));

			// 我们多添加了一个法线的定义，但目前Shader中我们并没有使用
			D3D12_INPUT_ELEMENT_DESC stIALayoutQuad[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// 创建 graphics pipeline state object (PSO)对象
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSOQuadDesc = {};
			stPSOQuadDesc.InputLayout = { stIALayoutQuad, _countof(stIALayoutQuad) };
			stPSOQuadDesc.pRootSignature = pIRSQuad.Get();
			stPSOQuadDesc.VS.BytecodeLength = pIVSQuad->GetBufferSize();
			stPSOQuadDesc.VS.pShaderBytecode = pIVSQuad->GetBufferPointer();
			stPSOQuadDesc.PS.BytecodeLength = pIPSQuad->GetBufferSize();
			stPSOQuadDesc.PS.pShaderBytecode = pIPSQuad->GetBufferPointer();

			stPSOQuadDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			stPSOQuadDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			stPSOQuadDesc.BlendState.AlphaToCoverageEnable = FALSE;
			stPSOQuadDesc.BlendState.IndependentBlendEnable = FALSE;
			stPSOQuadDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			stPSOQuadDesc.SampleMask = UINT_MAX;
			stPSOQuadDesc.SampleDesc.Count = 1;
			stPSOQuadDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSOQuadDesc.NumRenderTargets = 1;
			stPSOQuadDesc.RTVFormats[0] = emRTFmt;
			stPSOQuadDesc.DepthStencilState.StencilEnable = FALSE;
			stPSOQuadDesc.DepthStencilState.DepthEnable = FALSE;

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateGraphicsPipelineState(&stPSOQuadDesc
				, IID_PPV_ARGS(&pIPSOQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOQuad);
		}

		// 准备多个物体参数
		if(TRUE) 
		{
			USES_CONVERSION;
			// 物体个性参数
			StringCchPrintf(stModuleParams[nSphere].pszDDSFile, MAX_PATH, _T("%sAssets\\sphere.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nSphere].pszMeshFile, MAX_PATH, "%sAssets\\sphere.txt", T2A(pszAppPath));
			stModuleParams[nSphere].v4ModelPos = XMFLOAT4(2.0f, 2.0f, 0.0f, 1.0f);

			// 立方体个性参数
			StringCchPrintf(stModuleParams[nCube].pszDDSFile, MAX_PATH, _T("%sAssets\\Cube.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nCube].pszMeshFile, MAX_PATH, "%sAssets\\Cube.txt", T2A(pszAppPath));
			stModuleParams[nCube].v4ModelPos = XMFLOAT4(-2.0f, 2.0f, 0.0f, 1.0f);

			// 平板个性参数
			StringCchPrintf(stModuleParams[nPlane].pszDDSFile, MAX_PATH, _T("%sAssets\\Plane.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nPlane].pszMeshFile, MAX_PATH, "%sAssets\\Plane.txt", T2A(pszAppPath));
			stModuleParams[nPlane].v4ModelPos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

			// Mesh Value
			ST_GRS_VERTEX_MODULE* pstVertices = nullptr;
			UINT* pnIndices = nullptr;
			// DDS Value
			std::unique_ptr<uint8_t[]>			pbDDSData;
			std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
			DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
			bool								bIsCube = false;

			// 场景物体的共性参数，也就是各线程的共性参数
			for (int i = 0; i < nMaxObject; i++)
			{
				stModuleParams[i].nIndex = i;

				//创建每个Module的捆绑包
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE
					, IID_PPV_ARGS(&stModuleParams[i].pIBundleAlloc)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundleAlloc.Get(), _T("pIBundleAlloc"), i);

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE
					, stModuleParams[i].pIBundleAlloc.Get(), nullptr, IID_PPV_ARGS(&stModuleParams[i].pIBundle)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundle.Get(), _T("pIBundle"), i);

				//加载DDS
				pbDDSData.release();
				stArSubResources.clear();

				GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(stGPUParams[nIDGPUMain].m_pID3D12Device4.Get()
					, stModuleParams[i].pszDDSFile
					, stModuleParams[i].pITexture.GetAddressOf()
					, pbDDSData
					, stArSubResources
					, SIZE_MAX
					, &emAlphaMode
					, &bIsCube));

				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITexture.Get(), _T("pITexture"), i);

				D3D12_RESOURCE_DESC stTXDesc = stModuleParams[i].pITexture->GetDesc();
				UINT64 n64szUpSphere = 0;
				stGPUParams[nIDGPUMain].m_pID3D12Device4->GetCopyableFootprints(&stTXDesc, 0, static_cast<UINT>(stArSubResources.size())
					, 0, nullptr, nullptr, nullptr, &n64szUpSphere);

				D3D12_RESOURCE_DESC stUploadBufDesc = {};
				stUploadBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				stUploadBufDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
				stUploadBufDesc.Width = n64szUpSphere;
				stUploadBufDesc.Height = 1;
				stUploadBufDesc.DepthOrArraySize = 1;
				stUploadBufDesc.MipLevels = 1;
				stUploadBufDesc.Format = DXGI_FORMAT_UNKNOWN;
				stUploadBufDesc.SampleDesc.Count = 1;
				stUploadBufDesc.SampleDesc.Quality = 0;
				stUploadBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
				stUploadBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

				//创建上传堆
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stUploadBufDesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pITextureUpload)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITextureUpload.Get(), _T("pITextureUpload"), i);

				//上传DDS
				UINT nFirstSubresource = 0;
				UINT nNumSubresources = static_cast<UINT>(stArSubResources.size());
				D3D12_RESOURCE_DESC stUploadResDesc = stModuleParams[i].pITextureUpload->GetDesc();
				D3D12_RESOURCE_DESC stDefaultResDesc = stModuleParams[i].pITexture->GetDesc();

				UINT64 n64RequiredSize = 0;
				SIZE_T szMemToAlloc = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT)
					+ sizeof(UINT)
					+ sizeof(UINT64))
					* nNumSubresources;

				void* pMem = GRS_CALLOC(static_cast<SIZE_T>(szMemToAlloc));

				if (nullptr == pMem)
				{
					throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
				}

				D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
				UINT64* pRowSizesInBytes = reinterpret_cast<UINT64*>(pLayouts + nNumSubresources);
				UINT* pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + nNumSubresources);

				// 这里是第二次调用GetCopyableFootprints，就得到了所有子资源的详细信息
				stGPUParams[nIDGPUMain].m_pID3D12Device4->GetCopyableFootprints(&stDefaultResDesc, nFirstSubresource, nNumSubresources, 0, pLayouts, pNumRows, pRowSizesInBytes, &n64RequiredSize);

				BYTE* pData = nullptr;
				GRS_THROW_IF_FAILED(stModuleParams[i].pITextureUpload->Map(0, nullptr, reinterpret_cast<void**>(&pData)));

				// 第一遍Copy！注意3重循环每重的意思
				for (UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes)
				{// SubResources
					if (pRowSizesInBytes[nSubRes] > (SIZE_T)-1)
					{
						throw CGRSCOMException(E_FAIL);
					}

					D3D12_MEMCPY_DEST stCopyDestData = { pData + pLayouts[nSubRes].Offset
						, pLayouts[nSubRes].Footprint.RowPitch
						, pLayouts[nSubRes].Footprint.RowPitch * pNumRows[nSubRes]
					};

					for (UINT z = 0; z < pLayouts[nSubRes].Footprint.Depth; ++z)
					{// Mipmap
						BYTE* pDestSlice = reinterpret_cast<BYTE*>(stCopyDestData.pData) + stCopyDestData.SlicePitch * z;
						const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(stArSubResources[nSubRes].pData) + stArSubResources[nSubRes].SlicePitch * z;
						for (UINT y = 0; y < pNumRows[nSubRes]; ++y)
						{// Rows
							memcpy(pDestSlice + stCopyDestData.RowPitch * y,
								pSrcSlice + stArSubResources[nSubRes].RowPitch * y,
								(SIZE_T)pRowSizesInBytes[nSubRes]);
						}
					}
				}
				stModuleParams[i].pITextureUpload->Unmap(0, nullptr);

				// 第二次Copy！
				if (stDefaultResDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
				{// Buffer 一次性复制就可以了，因为没有行对齐和行大小不一致的问题，Buffer中行数就是1
					stGPUParams[nIDGPUMain].m_pICmdList->CopyBufferRegion(
						stModuleParams[i].pITexture.Get(), 0, stModuleParams[i].pITextureUpload.Get(), pLayouts[0].Offset, pLayouts[0].Footprint.Width);
				}
				else
				{
					for (UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes)
					{
						D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
						stDstCopyLocation.pResource = stModuleParams[i].pITexture.Get();
						stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
						stDstCopyLocation.SubresourceIndex = nSubRes;

						D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
						stSrcCopyLocation.pResource = stModuleParams[i].pITextureUpload.Get();
						stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
						stSrcCopyLocation.PlacedFootprint = pLayouts[nSubRes];

						stGPUParams[nIDGPUMain].m_pICmdList->CopyTextureRegion(&stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr);
					}
				}

				GRS_SAFE_FREE(pMem);

				//同步
				D3D12_RESOURCE_BARRIER stUploadTransResBarrier = {};
				stUploadTransResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				stUploadTransResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				stUploadTransResBarrier.Transition.pResource = stModuleParams[i].pITexture.Get();
				stUploadTransResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				stUploadTransResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
				stUploadTransResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1, &stUploadTransResBarrier);


				//创建SRV CBV堆
				D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
				stSRVHeapDesc.NumDescriptors = 2; //1 SRV + 1 CBV
				stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISRVCBVHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISRVCBVHp.Get(), _T("pISRVCBVHp"), i);

				//创建SRV
				D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
				stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				stSRVDesc.Format = stTXDesc.Format;
				stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				stSRVDesc.Texture2D.MipLevels = 1;

				D3D12_CPU_DESCRIPTOR_HANDLE stCBVSRVHandle = stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart();
				stCBVSRVHandle.ptr += stGPUParams[nIDGPUMain].m_nszSRVCBVUAV;

				stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateShaderResourceView(stModuleParams[i].pITexture.Get()
					, &stSRVDesc
					, stCBVSRVHandle);

				// 创建Sample
				D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
				stSamplerHeapDesc.NumDescriptors = 1;
				stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
				stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateDescriptorHeap(&stSamplerHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISampleHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISampleHp.Get(), _T("pISampleHp"), i);

				D3D12_SAMPLER_DESC stSamplerDesc = {};
				stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
				stSamplerDesc.MinLOD = 0;
				stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
				stSamplerDesc.MipLODBias = 0.0f;
				stSamplerDesc.MaxAnisotropy = 1;
				stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
				stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
				stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

				stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateSampler(&stSamplerDesc
					, stModuleParams[i].pISampleHp->GetCPUDescriptorHandleForHeapStart());

				//加载网格数据
				LoadMeshVertex(stModuleParams[i].pszMeshFile, stModuleParams[i].nVertexCnt, pstVertices, pnIndices);
				stModuleParams[i].nIndexCnt = stModuleParams[i].nVertexCnt;

				//创建 Vertex Buffer 仅使用Upload隐式堆
				stUploadBufDesc.Width = stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE);
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stUploadBufDesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIVertexBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIVertexBuffer.Get(), _T("pIVertexBuffer"), i);

				//使用map-memcpy-unmap大法将数据传至顶点缓冲对象
				UINT8* pVertexDataBegin = nullptr;
				D3D12_RANGE stReadRange = { 0, 0 };		// We do not intend to read from this resource on the CPU.

				GRS_THROW_IF_FAILED(stModuleParams[i].pIVertexBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
				memcpy(pVertexDataBegin, pstVertices, stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
				stModuleParams[i].pIVertexBuffer->Unmap(0, nullptr);

				//创建 Index Buffer 仅使用Upload隐式堆
				stUploadBufDesc.Width = stModuleParams[i].nIndexCnt * sizeof(UINT);
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stUploadBufDesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIIndexsBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIIndexsBuffer.Get(), _T("pIIndexsBuffer"), i);

				UINT8* pIndexDataBegin = nullptr;
				GRS_THROW_IF_FAILED(stModuleParams[i].pIIndexsBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
				memcpy(pIndexDataBegin, pnIndices, stModuleParams[i].nIndexCnt * sizeof(UINT));
				stModuleParams[i].pIIndexsBuffer->Unmap(0, nullptr);

				//创建Vertex Buffer View
				stModuleParams[i].stVertexBufferView.BufferLocation = stModuleParams[i].pIVertexBuffer->GetGPUVirtualAddress();
				stModuleParams[i].stVertexBufferView.StrideInBytes = sizeof(ST_GRS_VERTEX_MODULE);
				stModuleParams[i].stVertexBufferView.SizeInBytes = stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE);

				//创建Index Buffer View
				stModuleParams[i].stIndexsBufferView.BufferLocation = stModuleParams[i].pIIndexsBuffer->GetGPUVirtualAddress();
				stModuleParams[i].stIndexsBufferView.Format = DXGI_FORMAT_R32_UINT;
				stModuleParams[i].stIndexsBufferView.SizeInBytes = stModuleParams[i].nIndexCnt * sizeof(UINT);

				GRS_SAFE_FREE(pstVertices);
				GRS_SAFE_FREE(pnIndices);

				// 创建常量缓冲 注意缓冲尺寸设置为256边界对齐大小
				stUploadBufDesc.Width = szMVPBuf;
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stUploadBufDesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIConstBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIConstBuffer.Get(), _T("pIConstBuffer"), i);
				// Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
				GRS_THROW_IF_FAILED(stModuleParams[i].pIConstBuffer->Map(0, nullptr, reinterpret_cast<void**>(&stModuleParams[i].pMVPBuf)));
				//---------------------------------------------------------------------------------------------

				// 创建CBV
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.BufferLocation = stModuleParams[i].pIConstBuffer->GetGPUVirtualAddress();
				cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);
				stGPUParams[nIDGPUMain].m_pID3D12Device4->CreateConstantBufferView(&cbvDesc, stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart());
			}
		}

		// 记录场景物体渲染的捆绑包
		{
			{
				//	两编渲染中捆绑包是一样的
				for (int i = 0; i < nMaxObject; i++)
				{
					stModuleParams[i].pIBundle->SetGraphicsRootSignature(pIRSMain.Get());
					stModuleParams[i].pIBundle->SetPipelineState(pIPSOMain.Get());

					ID3D12DescriptorHeap* ppHeapsSphere[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
					stModuleParams[i].pIBundle->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);

					//设置SRV
					D3D12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSphere = stModuleParams[i].pISRVCBVHp->GetGPUDescriptorHandleForHeapStart();
					stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(0, stGPUCBVHandleSphere);

					stGPUCBVHandleSphere.ptr += stGPUParams[nIDGPUMain].m_nszSRVCBVUAV;
					//设置CBV
					stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleSphere);

					//设置Sample
					stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(2, stModuleParams[i].pISampleHp->GetGPUDescriptorHandleForHeapStart());

					//注意我们使用的渲染手法是三角形列表，也就是通常的Mesh网格
					stModuleParams[i].pIBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					stModuleParams[i].pIBundle->IASetVertexBuffers(0, 1, &stModuleParams[i].stVertexBufferView);
					stModuleParams[i].pIBundle->IASetIndexBuffer(&stModuleParams[i].stIndexsBufferView);

					//Draw Call！！！
					stModuleParams[i].pIBundle->DrawIndexedInstanced(stModuleParams[i].nIndexCnt, 1, 0, 0, 0);
					stModuleParams[i].pIBundle->Close();
				}
				//End for
			}
		}

		// 建辅助显卡用来渲染或后处理用的矩形框
		{
			ST_GRS_VERTEX_QUAD stVertexQuad[] =
			{	//(   x,     y,    z,    w   )  (  u,    v   )
				{ { -1.0f,  1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },	// Top left.
				{ {  1.0f,  1.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },	// Top right.
				{ { -1.0f, -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },	// Bottom left.
				{ {  1.0f, -1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },	// Bottom right.
			};

			const UINT nszVBQuad = sizeof(stVertexQuad);


			D3D12_RESOURCE_DESC stBufferDesc = {};
			stBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stBufferDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			stBufferDesc.Width = nszVBQuad;
			stBufferDesc.Height = 1;
			stBufferDesc.DepthOrArraySize = 1;
			stBufferDesc.MipLevels = 1;
			stBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
			stBufferDesc.SampleDesc.Count = 1;
			stBufferDesc.SampleDesc.Quality = 0;
			stBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateCommittedResource(
				&stDefautHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferDesc
				, D3D12_RESOURCE_STATE_COPY_DEST
				, nullptr
				, IID_PPV_ARGS(&pIVBQuad)));

			// 这次我们特意演示了如何将顶点缓冲上传到默认堆上的方式，与纹理上传默认堆实际是一样的
			stBufferDesc.Width = nszVBQuad;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateCommittedResource(
				&stUploadHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIVBQuadUpload)));

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTxtLayouts = {};
			D3D12_RESOURCE_DESC stDestDesc = pIVBQuad->GetDesc();
			UINT64 n64RequiredSize = 0u;
			UINT   nNumSubresources = 1u;  //我们只有一副图片，即子资源个数为1
			UINT64 n64TextureRowSizes = 0u;
			UINT   nTextureRowNum = 0u;

			stGPUParams[nIDGPUSecondary].m_pID3D12Device4->GetCopyableFootprints(&stDestDesc
				, 0
				, nNumSubresources
				, 0
				, &stTxtLayouts
				, &nTextureRowNum
				, &n64TextureRowSizes
				, &n64RequiredSize);

			BYTE* pData = nullptr;
			GRS_THROW_IF_FAILED(pIVBQuadUpload->Map(0, NULL, reinterpret_cast<void**>(&pData)));

			BYTE* pDestSlice = reinterpret_cast<BYTE*>(pData) + stTxtLayouts.Offset;
			const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(stVertexQuad);
			for (UINT y = 0; y < nTextureRowNum; ++y)
			{
				memcpy(pDestSlice + static_cast<SIZE_T>(stTxtLayouts.Footprint.RowPitch) * y
					, pSrcSlice + static_cast<SIZE_T>(nszVBQuad) * y
					, nszVBQuad);
			}
			//取消映射 对于易变的数据如每帧的变换矩阵等数据，可以撒懒不用Unmap了，
			//让它常驻内存,以提高整体性能，因为每次Map和Unmap是很耗时的操作
			//因为现在起码都是64位系统和应用了，地址空间是足够的，被长期占用不会影响什么
			pIVBQuadUpload->Unmap(0, NULL);

			//向命令队列发出从上传堆复制纹理数据到默认堆的命令
			D3D12_TEXTURE_COPY_LOCATION stDst = {};
			stDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			stDst.pResource = pIVBQuad.Get();
			stDst.SubresourceIndex = 0;

			D3D12_TEXTURE_COPY_LOCATION stSrc = {};
			stSrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			stSrc.pResource = pIVBQuadUpload.Get();
			stSrc.PlacedFootprint = stTxtLayouts;

			stGPUParams[nIDGPUSecondary].m_pICmdList->CopyBufferRegion(pIVBQuad.Get(), 0, pIVBQuadUpload.Get(), 0, nszVBQuad);

			D3D12_RESOURCE_BARRIER stUploadTransResBarrier = {};

			stUploadTransResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stUploadTransResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			stUploadTransResBarrier.Transition.pResource = pIVBQuad.Get();
			stUploadTransResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			stUploadTransResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			stUploadTransResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1,&stUploadTransResBarrier);

			pstVBVQuad.BufferLocation = pIVBQuad->GetGPUVirtualAddress();
			pstVBVQuad.StrideInBytes = sizeof(ST_GRS_VERTEX_QUAD);
			pstVBVQuad.SizeInBytes = sizeof(stVertexQuad);

			// 执行命令，将矩形顶点缓冲上传到第二个显卡的默认堆上
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdList->Close());
			ID3D12CommandList* ppRenderCommandLists[] = { stGPUParams[nIDGPUSecondary].m_pICmdList.Get() };
			stGPUParams[nIDGPUSecondary].m_pICmdQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

			n64CurrentFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdQueue->Signal(stGPUParams[nIDGPUSecondary].m_pIFence.Get(), n64CurrentFenceValue));
			n64FenceValue++;
			if (stGPUParams[nIDGPUSecondary].m_pIFence->GetCompletedValue() < n64CurrentFenceValue)
			{
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, hEventFence));
				WaitForSingleObject(hEventFence, INFINITE);
			}
		}

		// 创建在辅助显卡上渲染单位矩形用的SRV堆和Sample堆
		{
			D3D12_DESCRIPTOR_HEAP_DESC stHeapDesc = {};
			stHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			//SRV堆
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSRVSecondary)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHSRVSecondary);

			// SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emRTFmt;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			D3D12_CPU_DESCRIPTOR_HANDLE stSrvHandle = pIDHSRVSecondary->GetCPUDescriptorHandleForHeapStart();

			for (int i = 0; i < g_nFrameBackBufCount; i++)
			{
				if (!bCrossAdapterTextureSupport)
				{
					// 使用复制的渲染目标纹理作为Shader的纹理资源
					stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateShaderResourceView(
						pISecondaryAdapterTexutrePerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				else
				{
					// 直接使用共享的渲染目标复制的纹理作为纹理资源
					stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateShaderResourceView(
						stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				stSrvHandle.ptr += stGPUParams[nIDGPUSecondary].m_nszSRVCBVUAV;
			}


			// 创建Sample Descriptor Heap 采样器描述符堆
			stHeapDesc.NumDescriptors = 1;  //只有一个Sample
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSampleSecondary)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHSampleSecondary);

			// 创建Sample View 实际就是采样器
			D3D12_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			stGPUParams[nIDGPUSecondary].m_pID3D12Device4->CreateSampler(&stSamplerDesc, pIDHSampleSecondary->GetCPUDescriptorHandleForHeapStart());
		}

		// 执行第二个Copy命令并等待所有的纹理都上传到了默认堆中
		{
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdList->Close());
			ID3D12CommandList* ppRenderCommandLists[] = { stGPUParams[nIDGPUMain].m_pICmdList.Get() };
			stGPUParams[nIDGPUMain].m_pICmdQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

			n64CurrentFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdQueue->Signal(stGPUParams[nIDGPUMain].m_pIFence.Get(), n64CurrentFenceValue));
			n64FenceValue++;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, hEventFence));

		}

		GRS_THROW_IF_FAILED(pICmdListCopy->Close());

		D3D12_RESOURCE_BARRIER stRTVStateTransBarrier = {};

		stRTVStateTransBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stRTVStateTransBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stRTVStateTransBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		// Time Value 时间轴变量
		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		// 计算旋转角度需要的变量
		double dModelRotationYAngle = 0.0f;

		XMMATRIX mxRotY;
		// 计算投影矩阵
		XMMATRIX mxProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)nWndWidth / (FLOAT)nWndHeight, 0.1f, 1000.0f);

		DWORD dwRet = 0;
		BOOL bExit = FALSE;

		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		//17、开始消息循环，并在其中不断渲染
		while (!bExit)
		{//消息循环，将等待超时值设置为0，同时将定时性的渲染，改成了每次循环都渲染
			dwRet = ::MsgWaitForMultipleObjects(1, &hEventFence, FALSE, INFINITE, QS_ALLINPUT);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{//开始新的一帧渲染
				//OnUpdate()
				{//计算 Module 模型矩阵 * 视矩阵 view * 裁剪矩阵 projection
					n64tmCurrent = ::GetTickCount();
					//计算旋转的角度：旋转角度(弧度) = 时间(秒) * 角速度(弧度/秒)
					dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * g_fPalstance;

					n64tmFrameStart = n64tmCurrent;

					//旋转角度是2PI周期的倍数，去掉周期数，只留下相对0弧度开始的小于2PI的弧度即可
					if (dModelRotationYAngle > XM_2PI)
					{
						dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
					}
					mxRotY = XMMatrixRotationY(static_cast<float>(dModelRotationYAngle));

					//注意实际的变换应当是 Module * World * View * Projection
					//此处实际World是一个单位矩阵，故省略了
					//而Module矩阵是先位移再旋转，也可以将旋转矩阵理解为就是World矩阵
					XMMATRIX xmMVP = XMMatrixMultiply(XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos)
						, XMLoadFloat3(&g_f3LockAt)
						, XMLoadFloat3(&g_f3HeapUp))
						, mxProjection);

					//计算物体的MVP
					xmMVP = XMMatrixMultiply(mxRotY, xmMVP);

					for (int i = 0; i < nMaxObject; i++)
					{
						XMMATRIX xmModulePos = XMMatrixTranslation(stModuleParams[i].v4ModelPos.x
							, stModuleParams[i].v4ModelPos.y
							, stModuleParams[i].v4ModelPos.z);

						xmModulePos = XMMatrixMultiply(xmModulePos, xmMVP);

						XMStoreFloat4x4(&stModuleParams[i].pMVPBuf->m_MVP, xmModulePos);
					}
				}

				//OnRender()
				{
					// 获取帧号
					nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

					stGPUParams[nIDGPUMain].m_pICmdAllocPerFrame[nCurrentFrameIndex]->Reset();
					stGPUParams[nIDGPUMain].m_pICmdList->Reset(stGPUParams[nIDGPUMain].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get(), pIPSOMain.Get());

					pICmdAllocCopyPerFrame[nCurrentFrameIndex]->Reset();
					pICmdListCopy->Reset(pICmdAllocCopyPerFrame[nCurrentFrameIndex].Get(), pIPSOMain.Get());

					stGPUParams[nIDGPUSecondary].m_pICmdAllocPerFrame[nCurrentFrameIndex]->Reset();
					stGPUParams[nIDGPUSecondary].m_pICmdList->Reset(stGPUParams[nIDGPUSecondary].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get(), pIPSOQuad.Get());

					// 主显卡渲染
					{
						stGPUParams[nIDGPUMain].m_pICmdList->SetGraphicsRootSignature(pIRSMain.Get());
						stGPUParams[nIDGPUMain].m_pICmdList->SetPipelineState(pIPSOMain.Get());
						stGPUParams[nIDGPUMain].m_pICmdList->RSSetViewports(1, &stViewPort);
						stGPUParams[nIDGPUMain].m_pICmdList->RSSetScissorRects(1, &stScissorRect);

						stRTVStateTransBarrier.Transition.pResource = stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get();
						stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
						stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

						stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

						D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = stGPUParams[nIDGPUMain].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart();
						rtvHandle.ptr += (nCurrentFrameIndex * stGPUParams[nIDGPUMain].m_nszRTV);
						D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = stGPUParams[nIDGPUMain].m_pIDHDSVTex->GetCPUDescriptorHandleForHeapStart();

						stGPUParams[nIDGPUMain].m_pICmdList->OMSetRenderTargets(
							1, &rtvHandle, false, &stDSVHandle);
						stGPUParams[nIDGPUMain].m_pICmdList->ClearRenderTargetView(
							rtvHandle, arf4ClearColor, 0, nullptr);
						stGPUParams[nIDGPUMain].m_pICmdList->ClearDepthStencilView(
							stDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

						//执行实际的物体绘制渲染，Draw Call！
						for (int i = 0; i < nMaxObject; i++)
						{
							ID3D12DescriptorHeap* ppHeapsSkybox[]
								= { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
							stGPUParams[nIDGPUMain].m_pICmdList->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
							stGPUParams[nIDGPUMain].m_pICmdList->ExecuteBundle(stModuleParams[i].pIBundle.Get());
						}
						
						stRTVStateTransBarrier.Transition.pResource = stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get();
						stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
						stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;

						stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdList->Close());
					}

					{// 第一步：在主显卡的主命令队列上执行主命令列表
						ID3D12CommandList* ppRenderCommandLists[]
							= { stGPUParams[nIDGPUMain].m_pICmdList.Get() };
						stGPUParams[nIDGPUMain].m_pICmdQueue->ExecuteCommandLists(
							_countof(ppRenderCommandLists), ppRenderCommandLists);

						n64CurrentFenceValue = n64FenceValue;
						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdQueue->Signal(
							stGPUParams[nIDGPUMain].m_pIFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}

					// 将主显卡的渲染结果复制到共享纹理资源中
					{
						if (bCrossAdapterTextureSupport)
						{
							// 如果适配器支持跨适配器行主纹理，只需将该纹理复制到跨适配器纹理中。
							pICmdListCopy->CopyResource(
								stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get());
						}
						else
						{
							// 如果适配器不支持跨适配器行主纹理，则将纹理复制为缓冲区，
							// 以便可以显式地管理纹理行间距。使用呈现目标指定的内存布局将中间呈现目标复制到共享缓冲区中。
							D3D12_RESOURCE_DESC stRenderTargetDesc = stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stRenderTargetLayout = {};

							stGPUParams[nIDGPUMain].m_pID3D12Device4->GetCopyableFootprints(&stRenderTargetDesc, 0, 1, 0, &stRenderTargetLayout, nullptr, nullptr, nullptr);

							D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
							stDstCopyLocation.pResource = stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get();
							stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
							stDstCopyLocation.PlacedFootprint = stRenderTargetLayout;

							D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
							stSrcCopyLocation.pResource = stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get();
							stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
							stSrcCopyLocation.SubresourceIndex = 0;

							D3D12_BOX stCopyBox = { 0, 0, nWndWidth, nWndHeight };

							pICmdListCopy->CopyBufferRegion(stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, 0
								, stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get()
								, 0, stRenderTargetDesc.Width * stRenderTargetDesc.Height);
						}

						GRS_THROW_IF_FAILED(pICmdListCopy->Close());
					}

					{// 第二步：使用主显卡上的复制命令队列完成渲染目标资源到共享资源间的复制
					// 通过调用命令队列的Wait命令实现同一个GPU上的各命令队列之间的等待同步 
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Wait(stGPUParams[nIDGPUMain].m_pIFence.Get(), n64CurrentFenceValue));

						ID3D12CommandList* ppCopyCommandLists[] = { pICmdListCopy.Get() };
						pICmdQueueCopy->ExecuteCommandLists(_countof(ppCopyCommandLists), ppCopyCommandLists);

						n64CurrentFenceValue = n64FenceValue;
						// 复制命令的信号设置在共享的围栏对象上这样使得第二个显卡的命令队列可以在这个围栏上等待
						// 从而完成不同GPU命令队列间的同步等待
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Signal(stGPUParams[nIDGPUMain].m_pISharedFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}

					// 开始辅助显卡的渲染，通常是后处理，比如运动模糊等，我们这里直接就是把画面绘制出来
					{
						if (!bCrossAdapterTextureSupport)
						{
							// 将共享堆中的缓冲区复制到辅助适配器可以从中取样的纹理中。
							stRTVStateTransBarrier.Transition.pResource = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get();
							stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

							stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

							// Copy the shared buffer contents into the texture using the memory
							// layout prescribed by the texture.
							D3D12_RESOURCE_DESC stSecondaryAdapterTexture = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTextureLayout = {};

							stGPUParams[nIDGPUSecondary].m_pID3D12Device4->GetCopyableFootprints(&stSecondaryAdapterTexture, 0, 1, 0, &stTextureLayout, nullptr, nullptr, nullptr);

							D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
							stDstCopyLocation.pResource = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get();
							stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
							stDstCopyLocation.SubresourceIndex = 0;

							D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
							stSrcCopyLocation.pResource = stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get();
							stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
							stSrcCopyLocation.PlacedFootprint = stTextureLayout;

							D3D12_BOX stCopyBox = { 0, 0, nWndWidth, nWndHeight };

							stGPUParams[nIDGPUSecondary].m_pICmdList->CopyBufferRegion(pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get()
								, 0
								, stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, 0
								, stSecondaryAdapterTexture.Width * stSecondaryAdapterTexture.Height);

							stRTVStateTransBarrier.Transition.pResource = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get();
							stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
							stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

							stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);
						}

						stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootSignature(pIRSQuad.Get());
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetPipelineState(pIPSOQuad.Get());
						ID3D12DescriptorHeap* ppHeaps[] = { pIDHSRVSecondary.Get(),pIDHSampleSecondary.Get() };
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

						stGPUParams[nIDGPUSecondary].m_pICmdList->RSSetViewports(1, &stViewPort);
						stGPUParams[nIDGPUSecondary].m_pICmdList->RSSetScissorRects(1, &stScissorRect);

						stRTVStateTransBarrier.Transition.pResource = stGPUParams[nIDGPUSecondary].m_pIRTRes[nCurrentFrameIndex].Get();
						stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
						stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

						stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

						D3D12_CPU_DESCRIPTOR_HANDLE rtvSecondaryHandle = stGPUParams[nIDGPUSecondary].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart();
						rtvSecondaryHandle.ptr += (nCurrentFrameIndex * stGPUParams[nIDGPUSecondary].m_nszRTV);
						stGPUParams[nIDGPUSecondary].m_pICmdList->OMSetRenderTargets(1, &rtvSecondaryHandle, false, nullptr);
						float f4ClearColor[] = { 1.0f,0.0f,0.0f,1.0f }; //故意使用与主显卡渲染目标不同的清除色，查看是否有“露底”的问题
						stGPUParams[nIDGPUSecondary].m_pICmdList->ClearRenderTargetView(rtvSecondaryHandle, f4ClearColor, 0, nullptr);

						// 开始绘制矩形
						D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = pIDHSRVSecondary->GetGPUDescriptorHandleForHeapStart();
						srvHandle.ptr += (nCurrentFrameIndex * stGPUParams[nIDGPUSecondary].m_nszSRVCBVUAV);
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootDescriptorTable(0, srvHandle);
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootDescriptorTable(1, pIDHSampleSecondary->GetGPUDescriptorHandleForHeapStart());

						stGPUParams[nIDGPUSecondary].m_pICmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
						stGPUParams[nIDGPUSecondary].m_pICmdList->IASetVertexBuffers(0, 1, &pstVBVQuad);
						// Draw Call!
						stGPUParams[nIDGPUSecondary].m_pICmdList->DrawInstanced(4, 1, 0, 0);

						// 设置好同步围栏
						stRTVStateTransBarrier.Transition.pResource = stGPUParams[nIDGPUSecondary].m_pIRTRes[nCurrentFrameIndex].Get();
						stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
						stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

						stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdList->Close());
					}

					{// 第三步：使用辅助显卡上的主命令队列执行命令列表
						// 辅助显卡上的主命令队列通过等待共享的围栏对象最终完成了与主显卡之间的同步
						GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdQueue->Wait(
							stGPUParams[nIDGPUSecondary].m_pISharedFence.Get()
							, n64CurrentFenceValue));

						ID3D12CommandList* ppBlurCommandLists[] = { stGPUParams[nIDGPUSecondary].m_pICmdList.Get() };
						stGPUParams[nIDGPUSecondary].m_pICmdQueue->ExecuteCommandLists(_countof(ppBlurCommandLists), ppBlurCommandLists);
					}

					// 执行Present命令最终呈现画面
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					n64CurrentFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pICmdQueue->Signal(stGPUParams[nIDGPUSecondary].m_pIFence.Get(), n64CurrentFenceValue));
					n64FenceValue++;
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, hEventFence));
				}
			}
			break;
			case 1:
			{//处理消息
				while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
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
			case WAIT_TIMEOUT:
			{

			}
			break;

			default:
				break;
			}
		}
	}
	catch (CGRSCOMException& e)
	{//发生了COM异常
		e;
	}
#if defined(_DEBUG)
	{
		ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
	}
#endif
	::CoUninitialize();
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
		USHORT n16KeyCode = (wParam & 0xFF);
		if (VK_SPACE == n16KeyCode)
		{//

		}
		if (VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode)
		{
			//double g_fPalstance = 10.0f * XM_PI / 180.0f;	//物体旋转的角速度，单位：弧度/秒
			g_fPalstance += 10 * XM_PI / 180.0f;
			if (g_fPalstance > XM_PI)
			{
				g_fPalstance = XM_PI;
			}
			//XMMatrixOrthographicOffCenterLH()
		}

		if (VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode)
		{
			g_fPalstance -= 10 * XM_PI / 180.0f;
			if (g_fPalstance < 0.0f)
			{
				g_fPalstance = XM_PI / 180.0f;
			}
		}

		//根据用户输入变换
		//XMVECTOR g_f3EyePos = XMVectorSet(0.0f, 5.0f, -10.0f, 0.0f); //眼睛位置
		//XMVECTOR g_f3LockAt = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);  //眼睛所盯的位置
		//XMVECTOR g_f3HeapUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  //头部正上方位置
		XMFLOAT3 move(0, 0, 0);
		float fMoveSpeed = 2.0f;
		float fTurnSpeed = XM_PIDIV2 * 0.005f;

		if ('w' == n16KeyCode || 'W' == n16KeyCode)
		{
			move.z -= 1.0f;
		}

		if ('s' == n16KeyCode || 'S' == n16KeyCode)
		{
			move.z += 1.0f;
		}

		if ('d' == n16KeyCode || 'D' == n16KeyCode)
		{
			move.x += 1.0f;
		}

		if ('a' == n16KeyCode || 'A' == n16KeyCode)
		{
			move.x -= 1.0f;
		}

		if (fabs(move.x) > 0.1f && fabs(move.z) > 0.1f)
		{
			XMVECTOR vector = XMVector3Normalize(XMLoadFloat3(&move));
			move.x = XMVectorGetX(vector);
			move.z = XMVectorGetZ(vector);
		}

		if (VK_UP == n16KeyCode)
		{
			g_fPitch += fTurnSpeed;
		}

		if (VK_DOWN == n16KeyCode)
		{
			g_fPitch -= fTurnSpeed;
		}

		if (VK_RIGHT == n16KeyCode)
		{
			g_fYaw -= fTurnSpeed;
		}

		if (VK_LEFT == n16KeyCode)
		{
			g_fYaw += fTurnSpeed;
		}

		// Prevent looking too far up or down.
		g_fPitch = min(g_fPitch, XM_PIDIV4);
		g_fPitch = max(-XM_PIDIV4, g_fPitch);

		// Move the camera in model space.
		float x = move.x * -cosf(g_fYaw) - move.z * sinf(g_fYaw);
		float z = move.x * sinf(g_fYaw) - move.z * cosf(g_fYaw);
		g_f3EyePos.x += x * fMoveSpeed;
		g_f3EyePos.z += z * fMoveSpeed;

		// Determine the look direction.
		float r = cosf(g_fPitch);
		g_f3LockAt.x = r * sinf(g_fYaw);
		g_f3LockAt.y = sinf(g_fPitch);
		g_f3LockAt.z = r * cosf(g_fYaw);

		if (VK_TAB == n16KeyCode)
		{//按Tab键还原摄像机位置
			g_f3EyePos = XMFLOAT3(0.0f, 0.0f, -10.0f); //眼睛位置
			g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
			g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //头部正上方位置
		}
	}

	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX_MODULE*& ppVertex, UINT*& ppIndices)
{
	ifstream fin;
	char input;
	BOOL bRet = TRUE;
	try
	{
		fin.open(pszMeshFileName);
		if (fin.fail())
		{
			throw CGRSCOMException(E_FAIL);
		}
		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin >> nVertexCnt;

		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin.get(input);
		fin.get(input);

		ppVertex = (ST_GRS_VERTEX_MODULE*)GRS_CALLOC(nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
		ppIndices = (UINT*)GRS_CALLOC(nVertexCnt * sizeof(UINT));

		for (UINT i = 0; i < nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_v4Position.x >> ppVertex[i].m_v4Position.y >> ppVertex[i].m_v4Position.z;
			ppVertex[i].m_v4Position.w = 1.0f;
			fin >> ppVertex[i].m_vTex.x >> ppVertex[i].m_vTex.y;
			fin >> ppVertex[i].m_vNor.x >> ppVertex[i].m_vNor.y >> ppVertex[i].m_vNor.z;

			ppIndices[i] = i;
		}

	}
	catch (CGRSCOMException& e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}