#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <fstream>			//for ifstream
#include <wrl.h>			//添加WTL支持 方便使用COM
#include <atlcoll.h>		//for atl array
#include <strsafe.h>		//for StringCchxxxxx function
#include <dxgi1_6.h>
#include <d3d12.h>			//for d3d12
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
#define GRS_WND_TITLE	_T("GRS DirectX12 Render To Texture Sample")

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
struct ST_GRS_VERTEX
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

// 常量缓冲区
struct ST_GRS_MVP
{
	XMFLOAT4X4 m_MVP;			//经典的Model-view-projection(MVP)矩阵.
};

// 矩形框的顶点结构
struct ST_GRS_VERTEX_QUAD
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT4 m_vClr;		//Color
	XMFLOAT2 m_vTxc;		//Texcoord
};

// 正交投影的常量缓冲
struct ST_GRS_CB_MVO
{
	XMFLOAT4X4 m_mMVO;	   //Model * View * Orthographic 
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
	ST_GRS_MVP*							pMVPBuf;
	D3D12_VERTEX_BUFFER_VIEW			stVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				stIndexsBufferView;
};

//初始的默认摄像机的位置
//第一个摄像机在-Z轴向
XMFLOAT3 g_f3EyePos1 = XMFLOAT3(0.0f, 5.0f, -10.0f);  //眼睛位置
XMFLOAT3 g_f3LockAt1 = XMFLOAT3(0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
XMFLOAT3 g_f3HeapUp1 = XMFLOAT3(0.0f, 1.0f, 0.0f);    //头部正上方位置

float g_fYaw1 = 0.0f;			// 绕正Z轴的旋转量.
float g_fPitch1 = 0.0f;			// 绕XZ平面的旋转量

double g_fPalstance = 10.0f * XM_PI / 180.0f;	//物体旋转的角速度，单位：弧度/秒

//初始的默认摄像机的位置
//第二个摄像机用于渲染到纹理，位置在y轴正上方
XMFLOAT3 g_f3EyePos2 = XMFLOAT3(0.0f, 15.0f, 0.0f);   //眼睛位置
XMFLOAT3 g_f3LockAt2 = XMFLOAT3(0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
XMFLOAT3 g_f3HeapUp2 = XMFLOAT3(0.0f, 0.0f, 1.0f);    //头部正上方位置，注意第二个摄像机是竖直向下的，所以“上”方，变成了Z轴正向

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL LoadMeshVertex(const CHAR*pszMeshFileName, UINT&nVertexCnt, ST_GRS_VERTEX*&ppVertex, UINT*&ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	::CoInitialize(nullptr);  //for COM

	int									iWndWidth = 1024;
	int									iWndHeight = 768;

	HWND								hWnd = nullptr;
	MSG									msg = {};
	TCHAR								pszAppPath[MAX_PATH] = {};

	D3D12_VIEWPORT						stViewPort = { 0.0f, 0.0f, static_cast<float>(iWndWidth), static_cast<float>(iWndHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D12_RECT							stScissorRect = { 0, 0, static_cast<LONG>(iWndWidth), static_cast<LONG>(iWndHeight) };
	//被渲染纹理的清除颜色，创建时清除色和渲染前清除色保持一致，否则会出现一个未得到优化的警告
	const float							f4RTTexClearColor[] = { 0.8f, 0.8f, 0.8f, 0.0f };

	const UINT							nFrameBackBufCount = 3u;
	UINT								nCurrentFrameIndex = 0;

	UINT								nRTVDescriptorSize = 0U;
	UINT								nSRVDescriptorSize = 0U;
	UINT								nSampleDescriptorSize = 0U;

	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIFactory6>				pIDXGIFactory6;
	ComPtr<IDXGIAdapter1>				pIAdapter1;
	ComPtr<ID3D12Device4>				pID3D12Device4;
	ComPtr<ID3D12CommandQueue>			pIMainCmdQueue;
	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;
	ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBufCount];
	ComPtr<ID3D12Resource>				pIDepthStencilBuffer;	//深度蜡板缓冲区
	ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
	ComPtr<ID3D12DescriptorHeap>		pIDSVHeap;				//深度缓冲描述符堆
	

	DXGI_FORMAT							emRTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT							emDSFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ComPtr<ID3D12Fence>					pIFence;
	UINT64								n64FenceValue = 1ui64;
	HANDLE								hEventFence = nullptr;

	ComPtr<ID3D12CommandAllocator>		pICmdAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pICmdList;

	CAtlArray<HANDLE>					arHWaited;
	CAtlArray<HANDLE>					arHSubThread;

	ComPtr<ID3DBlob>					pIVSShader;
	ComPtr<ID3DBlob>					pIPSShader;
	ComPtr<ID3D12RootSignature>			pIRSPass1;
	ComPtr<ID3D12PipelineState>			pIPSOPass1;

	ComPtr<ID3D12Resource>				pIResRenderTarget;  //Render Target
	ComPtr<ID3D12Resource>				pIDSTex;
	ComPtr<ID3D12DescriptorHeap>		pIDHRTVTex;
	ComPtr<ID3D12DescriptorHeap>		pIDHDSVTex;


	ComPtr<ID3D12RootSignature>			pIRSQuad;
	ComPtr<ID3D12PipelineState>			pIPSOQuad;
	ComPtr<ID3D12DescriptorHeap>		pIDHQuad;
	ComPtr<ID3D12DescriptorHeap>		pIDHSampleQuad;
	ComPtr<ID3D12Resource>				pIVBQuad;
	ComPtr<ID3D12Resource>			    pICBMVO;	//常量缓冲

	D3D12_VERTEX_BUFFER_VIEW			stVBViewQuad = {};
	ST_GRS_CB_MVO*						pMOV = nullptr;
	SIZE_T								szCBMOVBuf = GRS_UPPER(sizeof(ST_GRS_CB_MVO), 256);

	// 场景物体参数
	const UINT							nMaxObject = 3;
	const UINT							nSphere = 0;
	const UINT							nCube = 1;
	const UINT							nPlane = 2;
	ST_GRS_MODULE_PARAMS				stModuleParams[nMaxObject] = {};

	// 常量缓冲区大小上对齐到256Bytes边界
	// D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT
	SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);
	UINT								nQuadVertexCnt = 0;

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

	D3D12_RESOURCE_DESC stBufferResSesc = {};
	stBufferResSesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	stBufferResSesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	stBufferResSesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	stBufferResSesc.Format = DXGI_FORMAT_UNKNOWN;
	stBufferResSesc.Width = 0;
	stBufferResSesc.Height = 1;
	stBufferResSesc.DepthOrArraySize = 1;
	stBufferResSesc.MipLevels = 1;
	stBufferResSesc.SampleDesc.Count = 1;
	stBufferResSesc.SampleDesc.Quality = 0;

	try
	{
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

		// 枚举高性能适配器来创建D3D设备对象
		{
			GRS_THROW_IF_FAILED(pIDXGIFactory6->EnumAdapterByGpuPreference(
				0
				, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
				, IID_PPV_ARGS(&pIAdapter1)));

			GRS_THROW_IF_FAILED(D3D12CreateDevice(
				pIAdapter1.Get()
				, D3D_FEATURE_LEVEL_12_1
				, IID_PPV_ARGS(&pID3D12Device4)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3D12Device4);

			TCHAR pszWndTitle[MAX_PATH] = {};
			DXGI_ADAPTER_DESC1 stAdapterDesc = {};
			GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc));
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s (GPU:%s)")
				, pszWndTitle
				, stAdapterDesc.Description);
			::SetWindowText(hWnd, pszWndTitle);

			//得到每个描述符元素的大小
			nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			nSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			nSampleDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}

		// 创建直接命令队列直接命令列表
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pIMainCmdQueue)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIMainCmdQueue);
		
			// 创建直接命令列表
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICmdAlloc)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdAlloc);
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICmdAlloc.Get(), nullptr, IID_PPV_ARGS(&pICmdList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdList);
		}
		
		// 创建交换链
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = nFrameBackBufCount;
			stSwapChainDesc.Width = iWndWidth;
			stSwapChainDesc.Height = iWndHeight;
			stSwapChainDesc.Format = emRTFormat;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				pIMainCmdQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
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
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVHeap);

			
			//---------------------------------------------------------------------------------------------
			D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
			for (UINT i = 0; i < nFrameBackBufCount; i++)
			{//这个循环暴漏了描述符堆实际上是个数组的本质
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&pIARenderTargets[i])));
				GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIARenderTargets, i);
				pID3D12Device4->CreateRenderTargetView(pIARenderTargets[i].Get(), nullptr, stRTVHandle);
				stRTVHandle.ptr += nRTVDescriptorSize;
			}

			// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

		}

		// 创建深度缓冲及深度缓冲描述符堆
		{
			D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
			stDepthOptimizedClearValue.Format = emDSFormat;
			stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

			//使用隐式默认堆创建一个深度蜡板缓冲区，
			//因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
			D3D12_RESOURCE_DESC stDSResDesc = {};
			stDSResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stDSResDesc.Alignment = 0;
			stDSResDesc.Format = emDSFormat;
			stDSResDesc.Width = iWndWidth;
			stDSResDesc.Height = iWndHeight;
			stDSResDesc.DepthOrArraySize = 1;
			stDSResDesc.MipLevels = 0;
			stDSResDesc.SampleDesc.Count = 1;
			stDSResDesc.SampleDesc.Quality = 0;
			stDSResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			stDSResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&stDefautHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stDSResDesc
				, D3D12_RESOURCE_STATE_DEPTH_WRITE
				, &stDepthOptimizedClearValue
				, IID_PPV_ARGS(&pIDepthStencilBuffer)
			));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDepthStencilBuffer);

			D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
			stDepthStencilDesc.Format = emDSFormat;
			stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&pIDSVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDSVHeap);

			pID3D12Device4->CreateDepthStencilView(pIDepthStencilBuffer.Get()
				, &stDepthStencilDesc
				, pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
		}

		// 创建渲染物体用的管线的根签名和状态对象
		{//这个例子中，所有物体使用相同的根签名，因为渲染过程中需要的参数是一样的
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
			// 检测是否支持V1.1版本的根签名
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
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
			stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//CBV是所有Shader可见
			stRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
			stRootParameters[0].DescriptorTable.pDescriptorRanges = &stDSPRanges[0];

			stRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//SRV仅PS可见
			stRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
			stRootParameters[1].DescriptorTable.pDescriptorRanges = &stDSPRanges[1];

			stRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//SAMPLE仅PS可见
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

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSPass1)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSPass1);

			UINT compileFlags = 0;
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
			//编译为行矩阵形式	   
			compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s9-D3D12RenderToTexture\\Shader\\TextureCube.hlsl"), pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "VSMain", "vs_5_0", compileFlags, 0, &pIVSShader, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "PSMain", "ps_5_0", compileFlags, 0, &pIPSShader, nullptr));

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
			stPSODesc.pRootSignature = pIRSPass1.Get();
			stPSODesc.VS.BytecodeLength = pIVSShader->GetBufferSize();
			stPSODesc.VS.pShaderBytecode = pIVSShader->GetBufferPointer();
			stPSODesc.PS.BytecodeLength = pIPSShader->GetBufferSize();
			stPSODesc.PS.pShaderBytecode = pIPSShader->GetBufferPointer();

			stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
			stPSODesc.BlendState.IndependentBlendEnable = FALSE;
			stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.SampleDesc.Count = 1;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRTFormat;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.DepthStencilState.DepthEnable = TRUE;			//打开深度缓冲		
			stPSODesc.DSVFormat = emDSFormat;
			stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//启用深度缓存写入功能
			stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //深度测试函数（该值为普通的深度测试，即较小值写入）

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc, IID_PPV_ARGS(&pIPSOPass1)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOPass1);
		}

		// 创建渲染矩形用的管线的根签名和状态对象
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
			// 检测是否支持V1.1版本的根签名
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{// 1.0版 直接丢异常退出了
				GRS_THROW_IF_FAILED(E_NOTIMPL);
			}

			//创建渲染矩形的根签名对象
			D3D12_DESCRIPTOR_RANGE1 stDSPRanges[3] = {};
			stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			stDSPRanges[0].NumDescriptors = 1;
			stDSPRanges[0].BaseShaderRegister = 0;
			stDSPRanges[0].RegisterSpace = 0;
			stDSPRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
			stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

			stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
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
			stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			stRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
			stRootParameters[0].DescriptorTable.pDescriptorRanges = &stDSPRanges[0];

			stRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
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

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSQuad);

			ComPtr<ID3DBlob> pIBlobVertexShader;
			ComPtr<ID3DBlob> pIBlobPixelShader;
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif
			compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s9-D3D12RenderToTexture\\Shader\\Quad.hlsl"), pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "VSMain", "vs_5_0", compileFlags, 0, &pIBlobVertexShader, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "PSMain", "ps_5_0", compileFlags, 0, &pIBlobPixelShader, nullptr));

			D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// 创建 graphics pipeline state object (PSO)对象
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.InputLayout = { stInputElementDescs, _countof(stInputElementDescs) };
			stPSODesc.pRootSignature = pIRSQuad.Get();
			stPSODesc.VS.BytecodeLength = pIBlobVertexShader->GetBufferSize();
			stPSODesc.VS.pShaderBytecode = pIBlobVertexShader->GetBufferPointer();
			stPSODesc.PS.BytecodeLength = pIBlobPixelShader->GetBufferSize();
			stPSODesc.PS.pShaderBytecode = pIBlobPixelShader->GetBufferPointer();

			stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
			stPSODesc.BlendState.IndependentBlendEnable = FALSE;
			stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			stPSODesc.DepthStencilState.DepthEnable = FALSE;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRTFormat;
			stPSODesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPSOQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOQuad);
		}

		// 创建渲染目标纹理
		{
			D3D12_RESOURCE_DESC stRenderTargetResDesc = {};
			stRenderTargetResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stRenderTargetResDesc.Alignment = 0;
			stRenderTargetResDesc.Width = iWndWidth;
			stRenderTargetResDesc.Height = iWndHeight;
			stRenderTargetResDesc.DepthOrArraySize = 1;
			stRenderTargetResDesc.MipLevels = 1;
			stRenderTargetResDesc.Format = emRTFormat;
			stRenderTargetResDesc.SampleDesc.Count = 1;
			stRenderTargetResDesc.SampleDesc.Quality = 0;
			stRenderTargetResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			stRenderTargetResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		
			D3D12_CLEAR_VALUE stClear = {};
			stClear.Format = emRTFormat;
			memcpy(&stClear.Color, &f4RTTexClearColor, 4 * sizeof(float));

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&stDefautHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stRenderTargetResDesc
				, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
				, &stClear
				, IID_PPV_ARGS(&pIResRenderTarget)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIResRenderTarget);

			//创建RTV(渲染目标视图)描述符堆
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = 1;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIDHRTVTex)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHRTVTex);

			pID3D12Device4->CreateRenderTargetView(pIResRenderTarget.Get(), nullptr, pIDHRTVTex->GetCPUDescriptorHandleForHeapStart());

			D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
			stDepthOptimizedClearValue.Format = emDSFormat;
			stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

			D3D12_RESOURCE_DESC stDSResDesc = {};
			stDSResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stDSResDesc.Alignment = 0;
			stDSResDesc.Width = iWndWidth;
			stDSResDesc.Height = iWndHeight;
			stDSResDesc.DepthOrArraySize = 1;
			stDSResDesc.MipLevels = 0;
			stDSResDesc.Format = emDSFormat;
			stDSResDesc.SampleDesc.Count = 1;
			stDSResDesc.SampleDesc.Quality = 0;
			stDSResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			stDSResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			//使用隐式默认堆创建一个深度蜡板缓冲区，
			//因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&stDefautHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stDSResDesc
				, D3D12_RESOURCE_STATE_DEPTH_WRITE
				, &stDepthOptimizedClearValue
				, IID_PPV_ARGS(&pIDSTex)
			));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDSTex);

			D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
			stDepthStencilDesc.Format = emDSFormat;
			stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&pIDHDSVTex)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHDSVTex);

			pID3D12Device4->CreateDepthStencilView(pIDSTex.Get()
				, &stDepthStencilDesc
				, pIDHDSVTex->GetCPUDescriptorHandleForHeapStart());
		}

		// 创建矩形框的顶点缓冲
		{
			//矩形框的顶点结构变量
			//注意这里定义了一个单位大小的正方形，后面通过缩放和位移矩阵变换到合适的大小，这个技术常用于Spirit、UI等2D渲染
			ST_GRS_VERTEX_QUAD stTriangleVertices[] =
			{
				{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f },	{ 0.0f, 0.0f }  },
				{ { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f },	{ 1.0f, 0.0f }  },
				{ { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },	{ 0.0f, 1.0f }  },
				{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },	{ 1.0f, 1.0f }  }
			};

			UINT nQuadVBSize = sizeof(stTriangleVertices);

			nQuadVertexCnt = _countof(stTriangleVertices);

			stBufferResSesc.Width = nQuadVBSize;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&stUploadHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferResSesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIVBQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVBQuad);

			UINT8* pVertexDataBegin = nullptr;
			D3D12_RANGE stReadRange = { 0, 0 };		// We do not intend to read from this resource on the CPU.
			GRS_THROW_IF_FAILED(pIVBQuad->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, stTriangleVertices, sizeof(stTriangleVertices));
			pIVBQuad->Unmap(0, nullptr);

			stVBViewQuad.BufferLocation = pIVBQuad->GetGPUVirtualAddress();
			stVBViewQuad.StrideInBytes = sizeof(ST_GRS_VERTEX_QUAD);
			stVBViewQuad.SizeInBytes = nQuadVBSize;
		}

		// 创建矩形框渲染需要的SRV CBV Sample等
		{
			stBufferResSesc.Width = szCBMOVBuf;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&stUploadHeapProps
				, D3D12_HEAP_FLAG_NONE
				, &stBufferResSesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBMVO)));

			GRS_THROW_IF_FAILED(pICBMVO->Map(0, nullptr, reinterpret_cast<void**>(&pMOV)));

			D3D12_DESCRIPTOR_HEAP_DESC stHeapDesc = {};
			stHeapDesc.NumDescriptors = 2;
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			//SRV堆
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHQuad);

			// SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emRTFormat;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			// 使用渲染目标作为Shader的纹理资源
			pID3D12Device4->CreateShaderResourceView(pIResRenderTarget.Get(), &stSRVDesc, pIDHQuad->GetCPUDescriptorHandleForHeapStart());

			// CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = pICBMVO->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = static_cast<UINT>(szCBMOVBuf);

			D3D12_CPU_DESCRIPTOR_HANDLE stSRVCBVHandle = pIDHQuad->GetCPUDescriptorHandleForHeapStart();
			stSRVCBVHandle.ptr += nSRVDescriptorSize;

			pID3D12Device4->CreateConstantBufferView(&cbvDesc, stSRVCBVHandle);

			//Sample
			stHeapDesc.NumDescriptors = 1;  //只有一个Sample
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSampleQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHSampleQuad);

			// Sample View
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

			pID3D12Device4->CreateSampler(&stSamplerDesc, pIDHSampleQuad->GetCPUDescriptorHandleForHeapStart());
		}

		// 准备多个物体参数
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
			ST_GRS_VERTEX*						pstVertices = nullptr;
			UINT*								pnIndices = nullptr;
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
				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE
					, IID_PPV_ARGS(&stModuleParams[i].pIBundleAlloc)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundleAlloc.Get(), _T("pIBundleAlloc"), i);

				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE
					, stModuleParams[i].pIBundleAlloc.Get(), nullptr, IID_PPV_ARGS(&stModuleParams[i].pIBundle)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundle.Get(), _T("pIBundle"), i);

				//加载DDS
				pbDDSData.release();
				stArSubResources.clear();
				GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(pID3D12Device4.Get()
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
				pID3D12Device4->GetCopyableFootprints(&stTXDesc, 0, static_cast<UINT>(stArSubResources.size())
					, 0, nullptr, nullptr, nullptr, &n64szUpSphere);
				

				// 在隐式堆上创建上传资源
				stBufferResSesc.Width = n64szUpSphere;
				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stBufferResSesc
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
				pID3D12Device4->GetCopyableFootprints(&stDefaultResDesc, nFirstSubresource, nNumSubresources, 0, pLayouts, pNumRows, pRowSizesInBytes, &n64RequiredSize);

				BYTE* pData = nullptr;
				HRESULT hr = stModuleParams[i].pITextureUpload->Map(0, nullptr, reinterpret_cast<void**>(&pData));
				if (FAILED(hr))
				{
					return 0;
				}

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
					pICmdList->CopyBufferRegion(
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

						pICmdList->CopyTextureRegion(&stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr);
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

				pICmdList->ResourceBarrier(1, &stUploadTransResBarrier);

				//创建SRV CBV堆
				D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
				stSRVHeapDesc.NumDescriptors = 2; //1 SRV + 1 CBV
				stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISRVCBVHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISRVCBVHp.Get(), _T("pISRVCBVHp"), i);
				
				//创建SRV
				D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
				stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				stSRVDesc.Format = stTXDesc.Format;
				stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				stSRVDesc.Texture2D.MipLevels = 1;

				D3D12_CPU_DESCRIPTOR_HANDLE stCBVSRVHandle = stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart();
				stCBVSRVHandle.ptr += nSRVDescriptorSize;

				pID3D12Device4->CreateShaderResourceView(stModuleParams[i].pITexture.Get()
					, &stSRVDesc
					, stCBVSRVHandle);

				// 创建Sample
				D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
				stSamplerHeapDesc.NumDescriptors = 1;
				stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
				stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stSamplerHeapDesc
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

				pID3D12Device4->CreateSampler(&stSamplerDesc
					, stModuleParams[i].pISampleHp->GetCPUDescriptorHandleForHeapStart());

				//加载网格数据
				LoadMeshVertex(stModuleParams[i].pszMeshFile, stModuleParams[i].nVertexCnt, pstVertices, pnIndices);
				stModuleParams[i].nIndexCnt = stModuleParams[i].nVertexCnt;

				//创建 Vertex Buffer 仅使用Upload隐式堆
				stBufferResSesc.Width = stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX);
				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stBufferResSesc
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIVertexBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIVertexBuffer.Get(), _T("pIVertexBuffer"), i);
				
				//使用map-memcpy-unmap大法将数据传至顶点缓冲对象
				UINT8* pVertexDataBegin = nullptr;
				D3D12_RANGE stReadRange = { 0, 0 };		// We do not intend to read from this resource on the CPU.

				GRS_THROW_IF_FAILED(stModuleParams[i].pIVertexBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
				memcpy(pVertexDataBegin, pstVertices, stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX));
				stModuleParams[i].pIVertexBuffer->Unmap(0, nullptr);

				//创建 Index Buffer 仅使用Upload隐式堆
				stBufferResSesc.Width = stModuleParams[i].nIndexCnt * sizeof(UINT);
				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stBufferResSesc
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
				stModuleParams[i].stVertexBufferView.StrideInBytes = sizeof(ST_GRS_VERTEX);
				stModuleParams[i].stVertexBufferView.SizeInBytes = stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX);

				//创建Index Buffer View
				stModuleParams[i].stIndexsBufferView.BufferLocation = stModuleParams[i].pIIndexsBuffer->GetGPUVirtualAddress();
				stModuleParams[i].stIndexsBufferView.Format = DXGI_FORMAT_R32_UINT;
				stModuleParams[i].stIndexsBufferView.SizeInBytes = stModuleParams[i].nIndexCnt * sizeof(UINT);

				GRS_SAFE_FREE(pstVertices);
				GRS_SAFE_FREE(pnIndices);

				// 创建常量缓冲 注意缓冲尺寸设置为256边界对齐大小
				stBufferResSesc.Width = szMVPBuf;
				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
					&stUploadHeapProps
					, D3D12_HEAP_FLAG_NONE
					, &stBufferResSesc
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
				pID3D12Device4->CreateConstantBufferView(&cbvDesc, stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart());
			}
		}

		// 记录场景物体渲染的捆绑包
		// 本例中两边渲染都用到这个捆绑包，这里显示出了捆绑包在代码简化重用方面的优势
		{
			//	两编渲染中捆绑包是一样的
			for (int i = 0; i < nMaxObject; i++)
			{
				stModuleParams[i].pIBundle->SetGraphicsRootSignature(pIRSPass1.Get());
				stModuleParams[i].pIBundle->SetPipelineState(pIPSOPass1.Get());

				ID3D12DescriptorHeap* ppHeapsSphere[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
				stModuleParams[i].pIBundle->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);

				//设置SRV
				D3D12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSphere = stModuleParams[i].pISRVCBVHp->GetGPUDescriptorHandleForHeapStart();
				stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(0, stGPUCBVHandleSphere);

				stGPUCBVHandleSphere.ptr += nSRVDescriptorSize;
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

		// 创建围栏对象
		{
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIFence);

			//创建一个Event同步对象，用于等待围栏事件通知
			hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hEventFence == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		UINT64 n64fence = 0;
		CAtlArray<ID3D12CommandList*> arCmdList;

		// 执行第二个Copy命令并等待所有的纹理都上传到了默认堆中
		{
			GRS_THROW_IF_FAILED(pICmdList->Close());
			arCmdList.RemoveAll();
			arCmdList.Add(pICmdList.Get());
			pIMainCmdQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

			// 等待纹理资源正式复制完成先
			const UINT64 fence = n64FenceValue;
			GRS_THROW_IF_FAILED(pIMainCmdQueue->Signal(pIFence.Get(), fence));
			n64FenceValue++;
			GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(fence, hEventFence));
		}

		D3D12_RESOURCE_BARRIER stRTVStateTransBarrier = {};

		stRTVStateTransBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stRTVStateTransBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stRTVStateTransBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	
		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;

		// 计算旋转角度需要的变量
		double dModelRotationYAngle = 0.0f;

		XMMATRIX mxRotY;
		// 计算投影矩阵
		XMMATRIX mxProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)iWndWidth / (FLOAT)iWndHeight, 0.1f, 1000.0f);

		// 计算正交投影矩阵 Orthographic
		// 基于左上角是坐标原点 X正方向向右 Y正方向向下 与窗口坐标系相同
		// 这里的算法来自于古老的HGE引擎核心
		// Orthographic
		XMMATRIX mxOrthographic = XMMatrixScaling(1.0f, -1.0f, 1.0f); //上下翻转，配合之前的Quad的Vertex坐标一起使用
		mxOrthographic = XMMatrixMultiply(mxOrthographic, XMMatrixTranslation(-0.5f, +0.5f, 0.0f)); //微量偏移，矫正像素位置
		mxOrthographic = XMMatrixMultiply(
			mxOrthographic
			, XMMatrixOrthographicOffCenterLH(stViewPort.TopLeftX
				, stViewPort.TopLeftX + stViewPort.Width
				, -(stViewPort.TopLeftY + stViewPort.Height)
				, -stViewPort.TopLeftY
				, stViewPort.MinDepth
				, stViewPort.MaxDepth)
		);
		// View * Orthographic 正交投影时，视矩阵通常是一个单位矩阵，这里之所以乘一下以表示与射影投影一致
		mxOrthographic = XMMatrixMultiply(XMMatrixIdentity(), mxOrthographic);

		DWORD dwRet = 0;
		BOOL bExit = FALSE;

		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		// 开始消息循环，并在其中不断渲染
		while (!bExit)
		{//注意这里我们调整了消息循环，将等待时间设置为0，同时将定时性的渲染，改成了每次循环都渲染
		 //特别注意这次等待与之前不同
			//主线程进入等待
			dwRet = ::MsgWaitForMultipleObjects(1, &hEventFence, FALSE, INFINITE, QS_ALLINPUT);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{//上一批命令已经执行结束，开始新一帧渲染
				//OnUpdate()
				{//计算旋转矩阵
					n64tmCurrent = ::GetTickCount();
					//计算旋转的角度：旋转角度(弧度) = 时间(秒) * 角速度(弧度/秒)
					//下面这句代码相当于经典游戏消息循环中的OnUpdate函数中需要做的事情
					dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * g_fPalstance;

					n64tmFrameStart = n64tmCurrent;

					//旋转角度是2PI周期的倍数，去掉周期数，只留下相对0弧度开始的小于2PI的弧度即可
					if (dModelRotationYAngle > XM_2PI)
					{
						dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
					}
					mxRotY = XMMatrixRotationY(static_cast<float>(dModelRotationYAngle));

					//计算 视矩阵 view * 裁剪矩阵 projection
					XMMATRIX xmMVP = XMMatrixMultiply(XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos2)
						, XMLoadFloat3(&g_f3LockAt2)
						, XMLoadFloat3(&g_f3HeapUp2))
						, mxProjection);

					//计算球体的MVP
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

				//第一遍渲染，渲染到纹理
				{
					nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

					GRS_THROW_IF_FAILED(pICmdAlloc->Reset());
					GRS_THROW_IF_FAILED(pICmdList->Reset(pICmdAlloc.Get(), pIPSOPass1.Get()));

					//设置屏障将渲染目标纹理从资源转换为渲染目标状态
					stRTVStateTransBarrier.Transition.pResource = pIResRenderTarget.Get();
					stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
					stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
					pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

					D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIDHRTVTex->GetCPUDescriptorHandleForHeapStart();
					D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = pIDHDSVTex->GetCPUDescriptorHandleForHeapStart();

					//设置渲染目标
					pICmdList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);

					pICmdList->RSSetViewports(1, &stViewPort);
					pICmdList->RSSetScissorRects(1, &stScissorRect);

					pICmdList->ClearRenderTargetView(stRTVHandle, f4RTTexClearColor, 0, nullptr);
					pICmdList->ClearDepthStencilView(stDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

					//第一次执行每个场景物体的的捆绑包，渲染到纹理
					for (int i = 0; i < nMaxObject; i++)
					{
						ID3D12DescriptorHeap* ppHeapsSkybox[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
						pICmdList->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
						pICmdList->ExecuteBundle(stModuleParams[i].pIBundle.Get());
					}

					//渲染完毕将渲染目标纹理由渲染目标状态转换回纹理状态，并准备显示
					stRTVStateTransBarrier.Transition.pResource = pIResRenderTarget.Get();
					stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
					stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
					pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

					//关闭命令列表，去执行
					GRS_THROW_IF_FAILED(pICmdList->Close());
					arCmdList.RemoveAll();
					arCmdList.Add(pICmdList.Get());
					pIMainCmdQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					//开始同步GPU与CPU的执行
					n64fence = n64FenceValue;
					GRS_THROW_IF_FAILED(pIMainCmdQueue->Signal(pIFence.Get(), n64fence));
					n64FenceValue++;
					if (pIFence->GetCompletedValue() < n64fence)
					{
						GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(n64fence, hEventFence));
						WaitForSingleObject(hEventFence, INFINITE);
					}
					GRS_THROW_IF_FAILED(pICmdAlloc->Reset());
					GRS_THROW_IF_FAILED(pICmdList->Reset(pICmdAlloc.Get(), pIPSOPass1.Get()));
				}

				
				{
					//计算 视矩阵 view * 裁剪矩阵 projection
					XMMATRIX xmMVP = XMMatrixMultiply(XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos1)
						, XMLoadFloat3(&g_f3LockAt1)
						, XMLoadFloat3(&g_f3HeapUp1))
						, mxProjection);

					//计算球体的MVP
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

				//第二遍渲染，渲染到屏幕，并显示纹理渲染的结果
				{
					stRTVStateTransBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
					stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
					stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
					pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

					//偏移描述符指针到指定帧缓冲视图位置
					D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
					stRTVHandle.ptr += ( nCurrentFrameIndex * nRTVDescriptorSize );
					D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = pIDSVHeap->GetCPUDescriptorHandleForHeapStart();
					//设置渲染目标
					pICmdList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);

					pICmdList->RSSetViewports(1, &stViewPort);
					pICmdList->RSSetScissorRects(1, &stScissorRect);
					const float arClearColor[] = { 0.0f, 0.0f, 0.5f, 1.0f };
					pICmdList->ClearRenderTargetView(stRTVHandle, arClearColor, 0, nullptr);
					pICmdList->ClearDepthStencilView(stDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

					//再一次执行每个场景物体的的捆绑包，渲染到屏幕
					for (int i = 0; i < nMaxObject; i++)
					{
						ID3D12DescriptorHeap* ppHeapsSkybox[] = { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
						pICmdList->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
						pICmdList->ExecuteBundle(stModuleParams[i].pIBundle.Get());
					}

					// 渲染矩形框
					// 设置矩形框的位置，即矩形左上角的坐标，注意因为正交投影的缘故，这里单位是像素
					XMMATRIX xmMVO = XMMatrixMultiply(
						XMMatrixTranslation(10.0f, 10.0f, 0.0f)
						, mxOrthographic);

					// 设置矩形框的大小，单位同样是像素
					xmMVO = XMMatrixMultiply(
						XMMatrixScaling(320.0f, 240.0f, 1.0f)
						, xmMVO);

					//设置MVO
					XMStoreFloat4x4(&pMOV->m_mMVO, xmMVO);

					// 绘制矩形
					pICmdList->SetGraphicsRootSignature(pIRSQuad.Get());
					pICmdList->SetPipelineState(pIPSOQuad.Get());
					ID3D12DescriptorHeap* ppHeapsQuad[] = { pIDHQuad.Get(),pIDHSampleQuad.Get() };
					pICmdList->SetDescriptorHeaps(_countof(ppHeapsQuad), ppHeapsQuad);

					D3D12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandle(pIDHQuad->GetGPUDescriptorHandleForHeapStart());
					pICmdList->SetGraphicsRootDescriptorTable(0, stGPUCBVHandle);
					stGPUCBVHandle.ptr += nSRVDescriptorSize;
					pICmdList->SetGraphicsRootDescriptorTable(1, stGPUCBVHandle);
					pICmdList->SetGraphicsRootDescriptorTable(2, pIDHSampleQuad->GetGPUDescriptorHandleForHeapStart());

					pICmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
					pICmdList->IASetVertexBuffers(0, 1, &stVBViewQuad);
					//Draw Call！！！
					pICmdList->DrawInstanced(nQuadVertexCnt, 1, 0, 0);
					
					stRTVStateTransBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
					stRTVStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
					stRTVStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
					pICmdList->ResourceBarrier(1, &stRTVStateTransBarrier);

					//关闭命令列表，去执行
					GRS_THROW_IF_FAILED(pICmdList->Close());
					arCmdList.RemoveAll();
					arCmdList.Add(pICmdList.Get());
					pIMainCmdQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

					//提交画面
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					//开始同步GPU与CPU的执行，先记录围栏标记值
					n64fence = n64FenceValue;
					GRS_THROW_IF_FAILED(pIMainCmdQueue->Signal(pIFence.Get(), n64fence));
					n64FenceValue++;
					GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(n64fence, hEventFence));
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
		{//按空格键切换不同的采样器看效果，以明白每种采样器具体的含义
			//UINT g_nCurrentSamplerNO = 0; //当前使用的采样器索引
			//UINT g_nSampleMaxCnt = 5;		//创建五个典型的采样器
			//++g_nCurrentSamplerNO;
			//g_nCurrentSamplerNO %= g_nSampleMaxCnt;

			//=================================================================================================
			//重新设置球体的捆绑包
			//pICmdListSphere->Reset(pICmdAllocSphere.Get(), pIPSOPass1.Get());
			//pICmdListSphere->SetGraphicsRootSignature(pIRSPass1.Get());
			//pICmdListSphere->SetPipelineState(pIPSOPass1.Get());
			//ID3D12DescriptorHeap* ppHeapsSphere[] = { pISRVCBVHp.Get(),pISampleHp.Get() };
			//pICmdListSphere->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);
			////设置SRV
			//pICmdListSphere->SetGraphicsRootDescriptorTable(0, pISRVCBVHp->GetGPUDescriptorHandleForHeapStart());

			//CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSphere(pISRVCBVHp->GetGPUDescriptorHandleForHeapStart()
			//	, 1
			//	, pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
			////设置CBV
			//pICmdListSphere->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleSphere);
			//CD3DX12_GPU_DESCRIPTOR_HANDLE hGPUSamplerSphere(pISampleHp->GetGPUDescriptorHandleForHeapStart()
			//	, g_nCurrentSamplerNO
			//	, nSamplerDescriptorSize);
			////设置Sample
			//pICmdListSphere->SetGraphicsRootDescriptorTable(2, hGPUSamplerSphere);
			////注意我们使用的渲染手法是三角形列表，也就是通常的Mesh网格
			//pICmdListSphere->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			//pICmdListSphere->IASetVertexBuffers(0, 1, &stVBV);
			//pICmdListSphere->IASetIndexBuffer(&stIBV);

			////Draw Call！！！
			//pICmdListSphere->DrawIndexedInstanced(nIndexCnt, 1, 0, 0, 0);
			//pICmdListSphere->Close();
			//=================================================================================================
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
		//XMVECTOR g_f3EyePos1 = XMVectorSet(0.0f, 5.0f, -10.0f, 0.0f); //眼睛位置
		//XMVECTOR g_f3LockAt1 = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);  //眼睛所盯的位置
		//XMVECTOR g_f3HeapUp1 = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  //头部正上方位置
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
			g_fPitch1 += fTurnSpeed;
		}

		if (VK_DOWN == n16KeyCode)
		{
			g_fPitch1 -= fTurnSpeed;
		}

		if (VK_RIGHT == n16KeyCode)
		{
			g_fYaw1 -= fTurnSpeed;
		}

		if (VK_LEFT == n16KeyCode)
		{
			g_fYaw1 += fTurnSpeed;
		}

		// Prevent looking too far up or down.
		g_fPitch1 = min(g_fPitch1, XM_PIDIV4);
		g_fPitch1 = max(-XM_PIDIV4, g_fPitch1);

		// Move the camera in model space.
		float x = move.x * -cosf(g_fYaw1) - move.z * sinf(g_fYaw1);
		float z = move.x * sinf(g_fYaw1) - move.z * cosf(g_fYaw1);
		g_f3EyePos1.x += x * fMoveSpeed;
		g_f3EyePos1.z += z * fMoveSpeed;

		// Determine the look direction.
		float r = cosf(g_fPitch1);
		g_f3LockAt1.x = r * sinf(g_fYaw1);
		g_f3LockAt1.y = sinf(g_fPitch1);
		g_f3LockAt1.z = r * cosf(g_fYaw1);

		if (VK_TAB == n16KeyCode)
		{//按Tab键还原摄像机位置
			g_f3EyePos1 = XMFLOAT3(0.0f, 0.0f, -10.0f); //眼睛位置
			g_f3LockAt1 = XMFLOAT3(0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
			g_f3HeapUp1 = XMFLOAT3(0.0f, 1.0f, 0.0f);    //头部正上方位置
		}

	}

	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

BOOL LoadMeshVertex(const CHAR*pszMeshFileName, UINT&nVertexCnt, ST_GRS_VERTEX*&ppVertex, UINT*&ppIndices)
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

		ppVertex = (ST_GRS_VERTEX*)GRS_CALLOC(nVertexCnt * sizeof(ST_GRS_VERTEX));
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
