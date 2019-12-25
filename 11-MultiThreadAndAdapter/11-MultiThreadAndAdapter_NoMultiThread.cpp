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
#include "..\WindowsCommons\d3dx12.h"
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
#define GRS_WND_TITLE	_T("GRS D3D12 MultiThread & MultiAdapter Sample")

#define GRS_SAFE_RELEASE(p) if(p){(p)->Release();(p)=nullptr;}
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); }}

//新定义的宏用于上取整除法
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

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

const UINT g_nFrameBackBufCount = 3u;
// 显卡参数集合
struct ST_GRS_GPU_PARAMS
{
	UINT								m_nIndex;
	UINT								m_nRTVDescriptorSize;
	UINT								m_nSRVDescriptorSize;
	HANDLE								m_hEventFence;

	ComPtr<ID3D12Device4>				m_pID3D12Device4;
	ComPtr<ID3D12CommandQueue>			m_pICMDQueue;

	ComPtr<ID3D12Resource>				m_pIRTRes[g_nFrameBackBufCount];
	ComPtr<ID3D12DescriptorHeap>		m_pIRTVHeap;
	ComPtr<ID3D12Resource>				m_pIDSRes;
	ComPtr<ID3D12DescriptorHeap>		m_pIDSVHeap;

	ComPtr<ID3D12DescriptorHeap>		m_pISRVHeap;
	ComPtr<ID3D12DescriptorHeap>		m_pISampleHeap;

	ComPtr<ID3D12RootSignature>			m_pIRS;
	ComPtr<ID3D12PipelineState>			m_pIPSO;

	ComPtr<ID3D12Heap>					m_pICrossAdapterHeap;
	ComPtr<ID3D12Resource>				m_pICrossAdapterResPerFrame[g_nFrameBackBufCount];

	ComPtr<ID3D12CommandAllocator>		m_pICMDAlloc;
	ComPtr<ID3D12GraphicsCommandList>	m_pICMDList;

	ComPtr<ID3D12Fence>					m_pIFence;
	ComPtr<ID3D12Fence>					m_pISharedFence;
};

// 常量缓冲区
struct ST_GRS_MVP
{
	XMFLOAT4X4 m_MVP;			//经典的Model-view-projection(MVP)矩阵.
};

// 场景物体参数
struct ST_GRS_MODULE_PARAMS
{
	UINT								m_nModuleIndex;

	XMFLOAT4							m_v4ModelPos;
	TCHAR								m_pszDDSFile[MAX_PATH];
	CHAR								m_pszMeshFile[MAX_PATH];
	UINT								m_nVertexCnt;
	UINT								m_nIndexCnt;

	ComPtr<ID3D12Resource>				m_pIVertexBuffer;
	ComPtr<ID3D12Resource>				m_pIIndexsBuffer;
	ComPtr<ID3D12Resource>				m_pITexture;
	ComPtr<ID3D12Resource>				m_pITextureUpload;
	ComPtr<ID3D12Resource>			    m_pICBRes;

	ComPtr<ID3D12DescriptorHeap>		m_pSRVHeap;
	ComPtr<ID3D12DescriptorHeap>		m_pISampleHeap;

	ComPtr<ID3D12CommandAllocator>		m_pIBundleAlloc;
	ComPtr<ID3D12GraphicsCommandList>	m_pIBundle;

	ST_GRS_MVP*							m_pMVPBuf;
	D3D12_VERTEX_BUFFER_VIEW			m_stVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW				m_stIndexsBufferView;
};

struct ST_GRS_PEROBJECT_CB
{
	UINT		m_nFun;
	float		m_fQuatLevel;    //量化bit数，取值2-6
	float		m_fWaterPower;  //表示水彩扩展力度，单位为像素
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

//初始的默认摄像机的位置
XMFLOAT3 g_f3EyePos = XMFLOAT3(0.0f, 5.0f, -10.0f);  //眼睛位置
XMFLOAT3 g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
XMFLOAT3 g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //头部正上方位置

float g_fYaw = 0.0f;								// 绕正Z轴的旋转量.
float g_fPitch = 0.0f;								// 绕XZ平面的旋转量
double g_fPalstance = 10.0f * XM_PI / 180.0f;		//物体旋转的角速度，单位：弧度/秒

//后处理需要的参数，设置为全局变量方便在窗口过程中根据按键输入调整
UINT								g_nFunNO = 0;	  //当前使用效果函数的序号（按空格键循环切换）
UINT								g_nMaxFunNO = 2;	  //总的效果函数个数
float								g_fQuatLevel = 2.0f;   //量化bit数，取值2-6
float								g_fWaterPower = 40.0f;  //表示水彩扩展力度，单位为像素

//使用哪个版本PS做最后的模糊处理：0 原样输出 1 矩阵形式高斯模糊PS 2 双向分离优化后的高斯模糊PS（来自微软官方示例）
UINT								g_nUsePSID = 1;			

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& m_nVertexCnt, ST_GRS_VERTEX_MODULE*& ppVertex, UINT*& ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	try
	{
		TCHAR								pszAppPath[MAX_PATH] = {};
		HWND								hWnd = nullptr;
		MSG									msg = {};

		int									iWndWidth = 1024;
		int									iWndHeight = 768;

		UINT								nDXGIFactoryFlags = 0U;
		UINT								nCurrentFrameIndex = 0u;
		DXGI_FORMAT							emRTFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT							emDSFmt = DXGI_FORMAT_D24_UNORM_S8_UINT;

		CD3DX12_VIEWPORT					stViewPort(0.0f, 0.0f, static_cast<float>(iWndWidth), static_cast<float>(iWndHeight));
		CD3DX12_RECT						stScissorRect(0, 0, static_cast<LONG>(iWndWidth), static_cast<LONG>(iWndHeight));

		const UINT							c_nMaxGPUCnt = 2;			//演示仅支持两个GPU进行渲染
		const UINT							c_nMainGPU = 0;
		const UINT							c_nSecondGPU = 1;
		ST_GRS_GPU_PARAMS					stGPU[c_nMaxGPUCnt] = {};

		// 场景物体参数
		const UINT							c_nMaxObjects = 3;
		const UINT							c_nSphere = 0;
		const UINT							c_nCube = 1;
		const UINT							c_nPlane = 2;
		ST_GRS_MODULE_PARAMS				stObject[c_nMaxObjects] = {};

		// 常量缓冲区大小上对齐到256Bytes边界
		SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_MVP), 256);

		// 用于复制渲染结果的复制命令队列、命令分配器、命令列表对象
		ComPtr<ID3D12CommandQueue>			pICmdQueueCopy;
		ComPtr<ID3D12CommandAllocator>		pICmdAllocCopy;
		ComPtr<ID3D12GraphicsCommandList>	pICmdListCopy;

		//当不能直接共享资源时，就在辅助显卡上创建独立的资源，用于复制主显卡渲染结果过来
		ComPtr<ID3D12Resource>				pISecondAdapterTexutrePerFrame[g_nFrameBackBufCount];
		BOOL								bCrossAdapterTextureSupport = FALSE;
		CD3DX12_RESOURCE_DESC				stRenderTargetDesc = {};
		const float							v4ClearColor[4] = { 0.2f, 0.5f, 1.0f, 1.0f };

		ComPtr<IDXGIFactory5>				pIDXGIFactory5;
		ComPtr<IDXGIFactory6>				pIDXGIFactory6;
		ComPtr<IDXGISwapChain1>				pISwapChain1;
		ComPtr<IDXGISwapChain3>				pISwapChain3;

		//辅助显卡上后处理需要的资源变量（Second Pass）
		ComPtr<ID3D12Resource>				pIOffLineRTRes[g_nFrameBackBufCount]; //离线渲染的渲染目标资源
		ComPtr<ID3D12DescriptorHeap>		pIRTVOffLine;						  //离线渲染RTV

		SIZE_T								szSecondPassCB = GRS_UPPER(sizeof(ST_GRS_PEROBJECT_CB), 256);
		ST_GRS_PEROBJECT_CB*				pstCBSecondPass = nullptr;
		ComPtr<ID3D12Resource>				pICBResSecondPass;
		ComPtr<ID3D12Resource>				pIVBQuad;
		ComPtr<ID3D12Resource>				pIVBQuadUpload;
		D3D12_VERTEX_BUFFER_VIEW			pstVBVQuad;
		ComPtr<ID3D12Resource>				pINoiseTexture;
		ComPtr<ID3D12Resource>				pINoiseTextureUpload;

		//辅助显卡上用于高斯模糊后处理的资源变量（Third Pass）
		ComPtr<ID3D12CommandAllocator>		pICMDAllocThirdPass;
		ComPtr<ID3D12GraphicsCommandList>	pICMDListThirdPass;
		ComPtr<ID3D12RootSignature>			pIRSThirdPass;
		ComPtr<ID3D12PipelineState>			pIPSOThirdPass;
		ComPtr<ID3D12DescriptorHeap>		pISRVHeapThirdPass;
		ComPtr<ID3D12DescriptorHeap>		pISampleHeapThirdPass;

		ComPtr<ID3D12PipelineState>			pIPSODoNothing;		//简单的显示颜色的PS的管线状态对象
		ComPtr<ID3D12PipelineState>			pIPSOBothwayBlur;	//双向高斯模糊的合并版

		CAtlArray<HANDLE>					arHWaited;
		UINT64								n64CurrentFenceValue = 0;
		UINT64								n64FenceValue = 1ui64;

		GRS_THROW_IF_FAILED(::CoInitialize(nullptr));
		// 得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
		{
			UINT nBytes = GetCurrentDirectory(MAX_PATH, pszAppPath);
			if (MAX_PATH == nBytes)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			WCHAR* lastSlash = _tcsrchr(pszAppPath, _T('\\'));
			if (lastSlash)
			{
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

		// 打开显示子系统的调试支持
		{
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> pIDebugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pIDebugController))))
			{
				pIDebugController->EnableDebugLayer();
				// 打开附加的调试支持
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif
		}

		// 创建DXGI Factory对象
		{
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
			//获取IDXGIFactory6接口
			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		// 枚举适配器创建设备
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			D3D12_FEATURE_DATA_ARCHITECTURE stArchitecture = {};
			DXGI_ADAPTER_DESC1				stAdapterDesc[c_nMaxGPUCnt] = {};

			IDXGIAdapter1* pIAdapterTmp = nullptr;
			ID3D12Device4* pID3DDeviceTmp = nullptr;
			IDXGIOutput* pIOutput = nullptr;

			HRESULT							hrEnumOutput = S_OK;
			UINT							i = 0;
			for (i = 0;
				(i < c_nMaxGPUCnt) &&
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

				GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp
					, D3D_FEATURE_LEVEL_12_1
					, IID_PPV_ARGS(&stGPU[i].m_pID3D12Device4)));
			
				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[i].m_pID3D12Device4);

				GRS_THROW_IF_FAILED(
					stGPU[i].m_pID3D12Device4->CreateCommandQueue(
						&stQueueDesc, IID_PPV_ARGS(&stGPU[i].m_pICMDQueue)));
				GRS_SetD3D12DebugNameIndexed(stGPU[i].m_pICMDQueue.Get(), _T("m_pICMDQueue"), i);

				stGPU[i].m_nIndex = i;
				stGPU[i].m_nRTVDescriptorSize
					= stGPU[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				stGPU[i].m_nSRVDescriptorSize
					= stGPU[i].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				GRS_SAFE_RELEASE(pIAdapterTmp);
			}

			//---------------------------------------------------------------------------------------------
			if (nullptr == stGPU[c_nMainGPU].m_pID3D12Device4.Get()
				|| nullptr == stGPU[c_nSecondGPU].m_pID3D12Device4.Get())
			{// 可怜的机器上居然没有两个以上的显卡 还是先退出了事 当然你可以使用软适配器凑活看下例子
				OutputDebugString(_T("\n机器中显卡数量不足两个，示例程序退出！\n"));
				throw CGRSCOMException(E_FAIL);
			}

			//复制命令队列创建在独显上，因为独显的性能强悍一些
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			GRS_THROW_IF_FAILED(
				stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandQueue(
					&stQueueDesc, IID_PPV_ARGS(&pICmdQueueCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdQueueCopy);

			//将被使用的设备名称显示到窗口标题里
			TCHAR pszWndTitle[MAX_PATH] = {};
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s(GPU[0]:%s & GPU[1]:%s)")
				, pszWndTitle
				, stAdapterDesc[c_nMainGPU].Description
				, stAdapterDesc[c_nSecondGPU].Description);
			::SetWindowText(hWnd, pszWndTitle);
		}

		// 创建围栏对象，以及多GPU同步围栏对象
		{
			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				GRS_THROW_IF_FAILED(
					stGPU[i].m_pID3D12Device4->CreateFence(
						0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&stGPU[i].m_pIFence)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[i].m_pIFence);

				stGPU[i].m_hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				if (stGPU[i].m_hEventFence == nullptr)
				{
					GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
				}
			}

			// 在主显卡上创建一个可共享的围栏对象
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateFence(0
				, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER
				, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pISharedFence)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nMainGPU].m_pISharedFence);

			// 共享这个围栏，通过句柄方式
			HANDLE hFenceShared = nullptr;
			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateSharedHandle(
				stGPU[c_nMainGPU].m_pISharedFence.Get(),
				nullptr,
				GENERIC_ALL,
				nullptr,
				&hFenceShared));

			// 在辅助显卡上打开这个围栏对象完成共享
			HRESULT hrOpenSharedHandleResult
				= stGPU[c_nSecondGPU].m_pID3D12Device4->OpenSharedHandle(hFenceShared
					, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pISharedFence));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pISharedFence);
			// 先关闭句柄，再判定是否共享成功
			::CloseHandle(hFenceShared);
			GRS_THROW_IF_FAILED(hrOpenSharedHandleResult);
		}

		// 创建命令列表
		{
			WCHAR pszDebugName[MAX_PATH] = {};
			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				GRS_THROW_IF_FAILED(
					stGPU[i].m_pID3D12Device4->CreateCommandAllocator(
						D3D12_COMMAND_LIST_TYPE_DIRECT
						, IID_PPV_ARGS(&stGPU[i].m_pICMDAlloc)));

				if (SUCCEEDED(StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPU[%u].m_pICMDAlloc", i)))
				{
					stGPU[i].m_pICMDAlloc->SetName(pszDebugName);
				}

				// 创建每个GPU的命令列表对象
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommandList(0
					, D3D12_COMMAND_LIST_TYPE_DIRECT
					, stGPU[i].m_pICMDAlloc.Get()
					, nullptr
					, IID_PPV_ARGS(&stGPU[i].m_pICMDList)));

				if (SUCCEEDED(StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPU[%u].m_pICMDList", i)))
				{
					stGPU[i].m_pICMDList->SetName(pszDebugName);
				}
			}

			// 在主显卡上创建复制命令列表（所谓能力越强，责任也就越大！）
			GRS_THROW_IF_FAILED(
				stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_COPY
					, IID_PPV_ARGS(&pICmdAllocCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdAllocCopy);

			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(0
				, D3D12_COMMAND_LIST_TYPE_COPY
				, pICmdAllocCopy.Get()
				, nullptr
				, IID_PPV_ARGS(&pICmdListCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdListCopy);

			// 在辅助显卡上创建Third Pass 渲染需要的命令分配器和命令列表
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_DIRECT
					, IID_PPV_ARGS(&pICMDAllocThirdPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAllocThirdPass);

			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommandList(0
					, D3D12_COMMAND_LIST_TYPE_DIRECT
					, pICMDAllocThirdPass.Get()
					, nullptr
					, IID_PPV_ARGS(&pICMDListThirdPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDListThirdPass);
		}

		// 创建交换链
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
			stSwapChainDesc.Width = iWndWidth;
			stSwapChainDesc.Height = iWndHeight;
			stSwapChainDesc.Format = emRTFmt;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				stGPU[c_nSecondGPU].m_pICMDQueue.Get(),		// 使用接驳了显示器的显卡的命令队列做为交换链的命令队列
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
			stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			for (int i = 0; i < c_nMaxGPUCnt; i++)
			{
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateDescriptorHeap(
					&stRTVHeapDesc, IID_PPV_ARGS(&stGPU[i].m_pIRTVHeap)));
				GRS_SetD3D12DebugNameIndexed(stGPU[i].m_pIRTVHeap.Get(), _T("m_pIRTVHeap"), i);
			}

			//Off-Line Render Target View
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
				&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVOffLine)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVOffLine);

			CD3DX12_CLEAR_VALUE   stClearValue(stSwapChainDesc.Format, v4ClearColor);
			stRenderTargetDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				stSwapChainDesc.Format,
				stSwapChainDesc.Width,
				stSwapChainDesc.Height,
				1u, 1u,
				stSwapChainDesc.SampleDesc.Count,
				stSwapChainDesc.SampleDesc.Quality,
				D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
				D3D12_TEXTURE_LAYOUT_UNKNOWN, 0u);

			WCHAR pszDebugName[MAX_PATH] = {};
			CD3DX12_CPU_DESCRIPTOR_HANDLE stHRTVOffLine(pIRTVOffLine->GetCPUDescriptorHandleForHeapStart());
			for (UINT i = 0; i < c_nMaxGPUCnt; i++)
			{
				CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(stGPU[i].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart());

				for (UINT j = 0; j < g_nFrameBackBufCount; j++)
				{
					if (i == c_nSecondGPU)
					{
						GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(j, IID_PPV_ARGS(&stGPU[i].m_pIRTRes[j])));

						// Create Second Adapter Off-Line Render Target
						GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommittedResource(
							&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
							&stClearValue,
							IID_PPV_ARGS(&pIOffLineRTRes[j])));

						GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIOffLineRTRes, j);
						// Off-Line Render Target View
						stGPU[i].m_pID3D12Device4->CreateRenderTargetView(pIOffLineRTRes[j].Get(), nullptr, stHRTVOffLine);
						stHRTVOffLine.Offset(1, stGPU[i].m_nRTVDescriptorSize);
					}
					else
					{
						GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommittedResource(
							&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_COMMON,
							&stClearValue,
							IID_PPV_ARGS(&stGPU[i].m_pIRTRes[j])));
					}

					StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPU[%u].m_pIRTRes[%u]", i, j);
					stGPU[i].m_pIRTRes[j]->SetName(pszDebugName);

					stGPU[i].m_pID3D12Device4->CreateRenderTargetView(stGPU[i].m_pIRTRes[j].Get(), nullptr, stRTVHandle);
					stRTVHandle.Offset(1, stGPU[i].m_nRTVDescriptorSize);
				}

				// 创建每个显卡上的深度蜡板缓冲区
				D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
				stDepthOptimizedClearValue.Format = emDSFmt;
				stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
				stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

				//使用隐式默认堆创建一个深度蜡板缓冲区，
				//因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Tex2D(
						emDSFmt
						, iWndWidth
						, iWndHeight
						, 1
						, 0
						, 1
						, 0
						, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
					, D3D12_RESOURCE_STATE_DEPTH_WRITE
					, &stDepthOptimizedClearValue
					, IID_PPV_ARGS(&stGPU[i].m_pIDSRes)
				));

				StringCchPrintfW(pszDebugName, MAX_PATH, L"stGPU[%u].m_pIDSRes", i);
				stGPU[i].m_pIDSRes->SetName(pszDebugName);

			
				// Create DSV Heap
				D3D12_DESCRIPTOR_HEAP_DESC stDSVHeapDesc = {};
				stDSVHeapDesc.NumDescriptors = 1;
				stDSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				stDSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				
				GRS_THROW_IF_FAILED(stGPU[i].m_pID3D12Device4->CreateDescriptorHeap(
					&stDSVHeapDesc, IID_PPV_ARGS(&stGPU[i].m_pIDSVHeap)));
				GRS_SetD3D12DebugNameIndexed(stGPU[i].m_pIDSVHeap.Get(), _T("m_pIDSVHeap"), i);

				// Create DSV
				D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
				stDepthStencilDesc.Format = emDSFmt;
				stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

				stGPU[i].m_pID3D12Device4->CreateDepthStencilView(stGPU[i].m_pIDSRes.Get()
					, &stDepthStencilDesc
					, stGPU[i].m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
			}
		}

		// 创建跨适配器的共享资源堆
		{
			{
				D3D12_FEATURE_DATA_D3D12_OPTIONS stOptions = {};
				// 通过检测带有显示输出的显卡是否支持跨显卡资源来决定跨显卡的资源如何创建
				GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CheckFeatureSupport(
					D3D12_FEATURE_D3D12_OPTIONS, reinterpret_cast<void*>(&stOptions), sizeof(stOptions)));

				bCrossAdapterTextureSupport = stOptions.CrossAdapterRowMajorTextureSupported;

				UINT64 n64szTexture = 0;
				D3D12_RESOURCE_DESC stCrossAdapterResDesc = {};

				if (bCrossAdapterTextureSupport)
				{
					// 如果支持那么直接创建跨显卡资源堆
					stCrossAdapterResDesc = stRenderTargetDesc;
					stCrossAdapterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
					stCrossAdapterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

					D3D12_RESOURCE_ALLOCATION_INFO stTextureInfo
						= stGPU[c_nMainGPU].m_pID3D12Device4->GetResourceAllocationInfo(0, 1, &stCrossAdapterResDesc);
					n64szTexture = stTextureInfo.SizeInBytes;
				}
				else
				{
					// 如果不支持，那么我们就需要先在主显卡上创建用于复制渲染结果的资源堆，然后再共享堆到辅助显卡上
					D3D12_PLACED_SUBRESOURCE_FOOTPRINT stResLayout = {};
					stGPU[c_nMainGPU].m_pID3D12Device4->GetCopyableFootprints(&stRenderTargetDesc, 0, 1, 0, &stResLayout, nullptr, nullptr, nullptr);
					n64szTexture = GRS_UPPER(stResLayout.Footprint.RowPitch * stResLayout.Footprint.Height, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
					stCrossAdapterResDesc = CD3DX12_RESOURCE_DESC::Buffer(n64szTexture, D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER);
				}

				// 创建跨显卡共享的资源堆
				CD3DX12_HEAP_DESC stCrossHeapDesc(
					n64szTexture * g_nFrameBackBufCount,
					D3D12_HEAP_TYPE_DEFAULT,
					0,
					D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateHeap(&stCrossHeapDesc
					, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pICrossAdapterHeap)));

				HANDLE hHeap = nullptr;
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateSharedHandle(
					stGPU[c_nMainGPU].m_pICrossAdapterHeap.Get(),
					nullptr,
					GENERIC_ALL,
					nullptr,
					&hHeap));

				HRESULT hrOpenSharedHandle = stGPU[c_nSecondGPU].m_pID3D12Device4->OpenSharedHandle(hHeap
					, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pICrossAdapterHeap));

				// 先关闭句柄，再判定是否共享成功
				::CloseHandle(hHeap);

				GRS_THROW_IF_FAILED(hrOpenSharedHandle);

				// 以定位方式在共享堆上创建每个显卡上的资源
				for (UINT i = 0; i < g_nFrameBackBufCount; i++)
				{
					GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreatePlacedResource(
						stGPU[c_nMainGPU].m_pICrossAdapterHeap.Get(),
						n64szTexture * i,
						&stCrossAdapterResDesc,
						D3D12_RESOURCE_STATE_COPY_DEST,
						nullptr,
						IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pICrossAdapterResPerFrame[i])));

					GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreatePlacedResource(
						stGPU[c_nSecondGPU].m_pICrossAdapterHeap.Get(),
						n64szTexture * i,
						&stCrossAdapterResDesc,
						bCrossAdapterTextureSupport ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE,
						nullptr,
						IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pICrossAdapterResPerFrame[i])));

					if (!bCrossAdapterTextureSupport)
					{
						// If the primary adapter's render target must be shared as a buffer,
						// create a texture resource to copy it into on the secondary adapter.
						GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
							&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
							nullptr,
							IID_PPV_ARGS(&pISecondAdapterTexutrePerFrame[i])));
					}
				}

			}
		}
		// 创建根签名
		{//这个例子中，所有物体使用相同的根签名，因为渲染过程中需要的参数是一样的
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};

			// 检测是否支持V1.1版本的根签名
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE
				, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			CD3DX12_DESCRIPTOR_RANGE1 stDSPRanges[3];
			stDSPRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			stDSPRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			stDSPRanges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER1 stRootParameters[3];
			stRootParameters[0].InitAsDescriptorTable(1, &stDSPRanges[0], D3D12_SHADER_VISIBILITY_ALL); //CBV是所有Shader可见
			stRootParameters[1].InitAsDescriptorTable(1, &stDSPRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);//SRV仅PS可见
			stRootParameters[2].InitAsDescriptorTable(1, &stDSPRanges[2], D3D12_SHADER_VISIBILITY_PIXEL);//SAMPLE仅PS可见

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc;
			stRootSignatureDesc.Init_1_1(_countof(stRootParameters)
				, stRootParameters
				, 0
				, nullptr
				, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> pISignatureBlob;
			ComPtr<ID3DBlob> pIErrorBlob;
			GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRootSignatureDesc
				, stFeatureData.HighestVersion
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pIRS)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nMainGPU].m_pIRS);

			//------------------------------------------------------------------------------------------------
			// Second Pass
			// 创建渲染Quad的根签名对象，注意这个根签名在辅助显卡上用
			CD3DX12_DESCRIPTOR_RANGE1 stDRQuad[3];
			stDRQuad[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			stDRQuad[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);
			stDRQuad[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER1 stRPQuad[3];
			stRPQuad[0].InitAsDescriptorTable(1, &stDRQuad[0], D3D12_SHADER_VISIBILITY_PIXEL);	//CBV所有Shader可见
			stRPQuad[1].InitAsDescriptorTable(1, &stDRQuad[1], D3D12_SHADER_VISIBILITY_PIXEL);	//SRV仅PS可见
			stRPQuad[2].InitAsDescriptorTable(1, &stDRQuad[2], D3D12_SHADER_VISIBILITY_PIXEL);	//SAMPLE仅PS可见

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRSQuadDesc;
			stRSQuadDesc.Init_1_1(_countof(stRPQuad)
				, stRPQuad
				, 0
				, nullptr
				, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			pISignatureBlob.Reset();
			pIErrorBlob.Reset();
			GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRSQuadDesc
				, stFeatureData.HighestVersion
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pIRS)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pIRS);

			//-----------------------------------------------------------------------------------------------------
			// Third Pass
			CD3DX12_DESCRIPTOR_RANGE1 stDRThridPass[2];
			stDRThridPass[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			stDRThridPass[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER1 stRPThirdPass[2];
			stRPThirdPass[0].InitAsDescriptorTable(1, &stDRThridPass[0], D3D12_SHADER_VISIBILITY_PIXEL);	//CBV所有Shader可见
			stRPThirdPass[1].InitAsDescriptorTable(1, &stDRThridPass[1], D3D12_SHADER_VISIBILITY_PIXEL);	//SRV仅PS可见

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRSThirdPass;
			stRSThirdPass.Init_1_1(_countof(stRPThirdPass)
				, stRPThirdPass
				, 0
				, nullptr
				, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			pISignatureBlob.Reset();
			pIErrorBlob.Reset();

			GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRSThirdPass
				, stFeatureData.HighestVersion
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSThirdPass)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSThirdPass);
		}

		// 编译Shader创建渲染管线状态对象
		{ {
#if defined(_DEBUG)
				// Enable better shader debugging with the graphics debugging tools.
				UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				UINT nShaderCompileFlags = 0;
#endif
				//编译为行矩阵形式	   
				nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

				TCHAR pszShaderFileName[MAX_PATH] = {};

				StringCchPrintf(pszShaderFileName
					, MAX_PATH
					, _T("%s11-MultiThreadAndAdapter\\Shader\\11-MultiThreadAndAdapter.hlsl")
					, pszAppPath);

				ComPtr<ID3DBlob> pVSCode;
				ComPtr<ID3DBlob> pPSCode;
				ComPtr<ID3DBlob> pErrMsg;
				CHAR pszErrMsg[MAX_PATH] = {};
				HRESULT hr = S_OK;

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pVSCode, &pErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pErrMsg)
					{
						StringCchPrintfA(pszErrMsg
							, MAX_PATH
							, "\n%s\n"
							, (CHAR*)pErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				pErrMsg.Reset();

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pPSCode, &pErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pErrMsg)
					{
						StringCchPrintfA(pszErrMsg
							, MAX_PATH
							, "\n%s\n"
							, (CHAR*)pErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}


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
				stPSODesc.pRootSignature = stGPU[c_nMainGPU].m_pIRS.Get();
				stPSODesc.VS = CD3DX12_SHADER_BYTECODE(pVSCode.Get());
				stPSODesc.PS = CD3DX12_SHADER_BYTECODE(pPSCode.Get());
				stPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				stPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
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

				GRS_THROW_IF_FAILED(
					stGPU[c_nMainGPU].m_pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc
						, IID_PPV_ARGS(&stGPU[c_nMainGPU].m_pIPSO)));

				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nMainGPU].m_pIPSO);

				//---------------------------------------------------------------------------------------------------
				//用于渲染单位矩形的管道状态对象(Second Pass)

				StringCchPrintf(pszShaderFileName, MAX_PATH
					, _T("%s11-MultiThreadAndAdapter\\Shader\\11-QuadVS.hlsl")
					, pszAppPath);

				pVSCode.Reset();
				pPSCode.Reset();
				pErrMsg.Reset();

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pVSCode, &pErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				pErrMsg.Reset();

				StringCchPrintf(pszShaderFileName, MAX_PATH
					, _T("%s11-MultiThreadAndAdapter\\Shader\\11-WaterColourPS.hlsl")
					, pszAppPath);

				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pPSCode, &pErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				D3D12_INPUT_ELEMENT_DESC stIALayoutQuad[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				// 创建 graphics pipeline state object (PSO)对象
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSOQuadDesc = {};
				stPSOQuadDesc.InputLayout = { stIALayoutQuad, _countof(stIALayoutQuad) };
				stPSOQuadDesc.pRootSignature = stGPU[c_nSecondGPU].m_pIRS.Get();
				stPSOQuadDesc.VS = CD3DX12_SHADER_BYTECODE(pVSCode.Get());
				stPSOQuadDesc.PS = CD3DX12_SHADER_BYTECODE(pPSCode.Get());
				stPSOQuadDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				stPSOQuadDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				stPSOQuadDesc.SampleMask = UINT_MAX;
				stPSOQuadDesc.SampleDesc.Count = 1;
				stPSOQuadDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				stPSOQuadDesc.NumRenderTargets = 1;
				stPSOQuadDesc.RTVFormats[0] = emRTFmt;
				stPSOQuadDesc.DepthStencilState.StencilEnable = FALSE;
				stPSOQuadDesc.DepthStencilState.DepthEnable = FALSE;

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOQuadDesc
						, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pIPSO)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pIPSO);

				//---------------------------------------------------------------------------------------------------
				// Third Pass

				pPSCode.Reset();
				pErrMsg.Reset();

				StringCchPrintf(pszShaderFileName, MAX_PATH
					, _T("%s11-MultiThreadAndAdapter\\Shader\\11-GaussianBlurPS.hlsl")
					, pszAppPath);

				//第三趟渲染，只编译PS就可以了，VS就用之前的QuadVS即可，都是后处理主要玩PS
				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pPSCode, &pErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				// 创建 graphics pipeline state object (PSO)对象
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSOThirdPassDesc = {};
				stPSOThirdPassDesc.InputLayout = { stIALayoutQuad, _countof(stIALayoutQuad) };
				stPSOThirdPassDesc.pRootSignature = pIRSThirdPass.Get();
				stPSOThirdPassDesc.VS = CD3DX12_SHADER_BYTECODE(pVSCode.Get());
				stPSOThirdPassDesc.PS = CD3DX12_SHADER_BYTECODE(pPSCode.Get());
				stPSOThirdPassDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				stPSOThirdPassDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
				stPSOThirdPassDesc.SampleMask = UINT_MAX;
				stPSOThirdPassDesc.SampleDesc.Count = 1;
				stPSOThirdPassDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				stPSOThirdPassDesc.NumRenderTargets = 1;
				stPSOThirdPassDesc.RTVFormats[0] = emRTFmt;
				stPSOThirdPassDesc.DepthStencilState.StencilEnable = FALSE;
				stPSOThirdPassDesc.DepthStencilState.DepthEnable = FALSE;

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOThirdPassDesc
						, IID_PPV_ARGS(&pIPSOThirdPass)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOThirdPass);

				// DoNothing PS PSO
				pPSCode.Reset();
				pErrMsg.Reset();

				StringCchPrintf(pszShaderFileName, MAX_PATH
					, _T("%s11-MultiThreadAndAdapter\\Shader\\11-DoNothingPS.hlsl")
					, pszAppPath);

				//第三趟渲染，只编译PS就可以了，VS就用之前的QuadVS即可，都是后处理主要玩PS
				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pPSCode, &pErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				stPSOThirdPassDesc.PS = CD3DX12_SHADER_BYTECODE(pPSCode.Get());
				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOThirdPassDesc
						, IID_PPV_ARGS(&pIPSODoNothing)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSODoNothing);

				// DoNothing PS PSO
				pPSCode.Reset();
				pErrMsg.Reset();

				StringCchPrintf(pszShaderFileName, MAX_PATH
					, _T("%s11-MultiThreadAndAdapter\\Shader\\blurShaders_MS.hlsl")
					, pszAppPath);

				//第三趟渲染，只编译PS就可以了，VS就用之前的QuadVS即可，都是后处理主要玩PS
				hr = D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSSimpleBlur", "ps_5_0", nShaderCompileFlags, 0, &pPSCode, &pErrMsg);
				if (FAILED(hr))
				{
					if (nullptr != pErrMsg)
					{
						StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pErrMsg->GetBufferPointer());
						::OutputDebugStringA(pszErrMsg);
					}
					throw CGRSCOMException(hr);
				}

				stPSOThirdPassDesc.PS = CD3DX12_SHADER_BYTECODE(pPSCode.Get());

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateGraphicsPipelineState(
						&stPSOThirdPassDesc
						, IID_PPV_ARGS(&pIPSOBothwayBlur)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOBothwayBlur);

			}}

		// 加载DDS噪声纹理，注意用于后处理，所以加载到第二个显卡上
		{ {
				TCHAR pszNoiseTexture[MAX_PATH] = {};
				StringCchPrintf(pszNoiseTexture, MAX_PATH, _T("%sMesh\\Noise1.dds"), pszAppPath);
				std::unique_ptr<uint8_t[]>			pbDDSData;
				std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
				DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
				bool								bIsCube = false;

				GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(
					stGPU[c_nSecondGPU].m_pID3D12Device4.Get()
					, pszNoiseTexture
					, pINoiseTexture.GetAddressOf()
					, pbDDSData
					, stArSubResources
					, SIZE_MAX
					, &emAlphaMode
					, &bIsCube));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pINoiseTexture);

				UINT64 n64szUpSphere = GetRequiredIntermediateSize(
					pINoiseTexture.Get()
					, 0
					, static_cast<UINT>(stArSubResources.size()));

				D3D12_RESOURCE_DESC stTXDesc = pINoiseTexture->GetDesc();

				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
						&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
						, D3D12_HEAP_FLAG_NONE
						, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
						, D3D12_RESOURCE_STATE_GENERIC_READ
						, nullptr
						, IID_PPV_ARGS(&pINoiseTextureUpload)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pINoiseTextureUpload);

				//执行两个Copy动作将纹理上传到默认堆中
				UpdateSubresources(stGPU[c_nSecondGPU].m_pICMDList.Get()
					, pINoiseTexture.Get()
					, pINoiseTextureUpload.Get()
					, 0
					, 0
					, static_cast<UINT>(stArSubResources.size())
					, stArSubResources.data());

				//同步
				stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1
					, &CD3DX12_RESOURCE_BARRIER::Transition(pINoiseTexture.Get()
						, D3D12_RESOURCE_STATE_COPY_DEST
						, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

			}}

		// 准备多个物体参数
		{
			// 物体个性参数
			USES_CONVERSION;

			StringCchPrintf(stObject[c_nSphere].m_pszDDSFile, MAX_PATH, _T("%s\\Mesh\\sphere.dds"), pszAppPath);
			StringCchPrintfA(stObject[c_nSphere].m_pszMeshFile, MAX_PATH, "%s\\Mesh\\sphere.txt", T2A(pszAppPath));
			stObject[c_nSphere].m_v4ModelPos = XMFLOAT4(2.0f, 2.0f, 0.0f, 1.0f);

			// 立方体个性参数
			StringCchPrintf(stObject[c_nCube].m_pszDDSFile, MAX_PATH, _T("%s\\Mesh\\Cube.dds"), pszAppPath);
			StringCchPrintfA(stObject[c_nCube].m_pszMeshFile, MAX_PATH, "%s\\Mesh\\Cube.txt", T2A(pszAppPath));
			stObject[c_nCube].m_v4ModelPos = XMFLOAT4(-2.0f, 2.0f, 0.0f, 1.0f);

			// 平板个性参数
			StringCchPrintf(stObject[c_nPlane].m_pszDDSFile, MAX_PATH, _T("%s\\Mesh\\Plane.dds"), pszAppPath);
			StringCchPrintfA(stObject[c_nPlane].m_pszMeshFile, MAX_PATH, "%s\\Mesh\\Plane.txt", T2A(pszAppPath));
			stObject[c_nPlane].m_v4ModelPos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

			// Mesh Value
			ST_GRS_VERTEX_MODULE* pstVertices = nullptr;
			UINT* pnIndices = nullptr;
			// DDS Value
			std::unique_ptr<uint8_t[]>			pbDDSData;
			std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
			DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
			bool								bIsCube = false;

			// 场景物体的共性参数，也就是各线程的共性参数
			for (int i = 0; i < c_nMaxObjects; i++)
			{
				stObject[i].m_nModuleIndex = i;

				//创建每个Module的捆绑包
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE
					, IID_PPV_ARGS(&stObject[i].m_pIBundleAlloc)));
				GRS_SetD3D12DebugNameIndexed(stObject[i].m_pIBundleAlloc.Get(), _T("m_pIBundleAlloc"), i);

				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE
					, stObject[i].m_pIBundleAlloc.Get(), nullptr, IID_PPV_ARGS(&stObject[i].m_pIBundle)));
				GRS_SetD3D12DebugNameIndexed(stObject[i].m_pIBundle.Get(), _T("m_pIBundle"), i);

				//加载DDS
				pbDDSData.release();
				stArSubResources.clear();
				GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(stGPU[c_nMainGPU].m_pID3D12Device4.Get()
					, stObject[i].m_pszDDSFile
					, stObject[i].m_pITexture.GetAddressOf()
					, pbDDSData
					, stArSubResources
					, SIZE_MAX
					, &emAlphaMode
					, &bIsCube));

				GRS_SetD3D12DebugNameIndexed(stObject[i].m_pITexture.Get(), _T("m_pITexture"), i);

				UINT64 n64szUpSphere = GetRequiredIntermediateSize(
					stObject[i].m_pITexture.Get()
					, 0
					, static_cast<UINT>(stArSubResources.size()));
				D3D12_RESOURCE_DESC stTXDesc = stObject[i].m_pITexture->GetDesc();

				//创建上传堆
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stObject[i].m_pITextureUpload)));
				GRS_SetD3D12DebugNameIndexed(stObject[i].m_pITextureUpload.Get(), _T("m_pITextureUpload"), i);

				//上传DDS
				UpdateSubresources(stGPU[c_nMainGPU].m_pICMDList.Get()
					, stObject[i].m_pITexture.Get()
					, stObject[i].m_pITextureUpload.Get()
					, 0
					, 0
					, static_cast<UINT>(stArSubResources.size())
					, stArSubResources.data());

				//同步
				stGPU[c_nMainGPU].m_pICMDList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
					stObject[i].m_pITexture.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));


				//创建SRV CBV堆
				D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
				stSRVHeapDesc.NumDescriptors = 2; //1 SRV + 1 CBV
				stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc
					, IID_PPV_ARGS(&stObject[i].m_pSRVHeap)));
				GRS_SetD3D12DebugNameIndexed(stObject[i].m_pSRVHeap.Get(), _T("m_pSRVHeap"), i);

				//创建SRV
				D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
				stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				stSRVDesc.Format = stTXDesc.Format;
				stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				stSRVDesc.Texture2D.MipLevels = 1;

				CD3DX12_CPU_DESCRIPTOR_HANDLE stCBVSRVHandle(
					stObject[i].m_pSRVHeap->GetCPUDescriptorHandleForHeapStart()
					, 1
					, stGPU[c_nMainGPU].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

				stGPU[c_nMainGPU].m_pID3D12Device4->CreateShaderResourceView(stObject[i].m_pITexture.Get()
					, &stSRVDesc
					, stCBVSRVHandle);

				// 创建Sample
				D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
				stSamplerHeapDesc.NumDescriptors = 1;
				stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
				stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateDescriptorHeap(&stSamplerHeapDesc
					, IID_PPV_ARGS(&stObject[i].m_pISampleHeap)));
				GRS_SetD3D12DebugNameIndexed(stObject[i].m_pISampleHeap.Get(), _T("m_pISampleHeap"), i);

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

				stGPU[c_nMainGPU].m_pID3D12Device4->CreateSampler(&stSamplerDesc
					, stObject[i].m_pISampleHeap->GetCPUDescriptorHandleForHeapStart());

				//加载网格数据
				LoadMeshVertex(stObject[i].m_pszMeshFile, stObject[i].m_nVertexCnt, pstVertices, pnIndices);
				stObject[i].m_nIndexCnt = stObject[i].m_nVertexCnt;

				//创建 Vertex Buffer 仅使用Upload隐式堆
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(stObject[i].m_nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE))
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stObject[i].m_pIVertexBuffer)));
				GRS_SetD3D12DebugNameIndexed(stObject[i].m_pIVertexBuffer.Get(), _T("m_pIVertexBuffer"), i);

				//使用map-memcpy-unmap大法将数据传至顶点缓冲对象
				UINT8* pVertexDataBegin = nullptr;
				CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.

				GRS_THROW_IF_FAILED(stObject[i].m_pIVertexBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
				memcpy(pVertexDataBegin, pstVertices, stObject[i].m_nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
				stObject[i].m_pIVertexBuffer->Unmap(0, nullptr);

				//创建 Index Buffer 仅使用Upload隐式堆
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(stObject[i].m_nIndexCnt * sizeof(UINT))
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stObject[i].m_pIIndexsBuffer)));
				GRS_SetD3D12DebugNameIndexed(stObject[i].m_pIIndexsBuffer.Get(), _T("m_pIIndexsBuffer"), i);

				UINT8* pIndexDataBegin = nullptr;
				GRS_THROW_IF_FAILED(stObject[i].m_pIIndexsBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
				memcpy(pIndexDataBegin, pnIndices, stObject[i].m_nIndexCnt * sizeof(UINT));
				stObject[i].m_pIIndexsBuffer->Unmap(0, nullptr);

				//创建Vertex Buffer View
				stObject[i].m_stVertexBufferView.BufferLocation = stObject[i].m_pIVertexBuffer->GetGPUVirtualAddress();
				stObject[i].m_stVertexBufferView.StrideInBytes = sizeof(ST_GRS_VERTEX_MODULE);
				stObject[i].m_stVertexBufferView.SizeInBytes = stObject[i].m_nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE);

				//创建Index Buffer View
				stObject[i].m_stIndexsBufferView.BufferLocation = stObject[i].m_pIIndexsBuffer->GetGPUVirtualAddress();
				stObject[i].m_stIndexsBufferView.Format = DXGI_FORMAT_R32_UINT;
				stObject[i].m_stIndexsBufferView.SizeInBytes = stObject[i].m_nIndexCnt * sizeof(UINT);

				::HeapFree(::GetProcessHeap(), 0, pstVertices);
				::HeapFree(::GetProcessHeap(), 0, pnIndices);

				// 创建常量缓冲 注意缓冲尺寸设置为256边界对齐大小
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(szMVPBuf)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stObject[i].m_pICBRes)));
				GRS_SetD3D12DebugNameIndexed(stObject[i].m_pICBRes.Get(), _T("m_pICBRes"), i);
				// Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
				GRS_THROW_IF_FAILED(stObject[i].m_pICBRes->Map(0, nullptr, reinterpret_cast<void**>(&stObject[i].m_pMVPBuf)));
				//---------------------------------------------------------------------------------------------

				// 创建CBV
				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.BufferLocation = stObject[i].m_pICBRes->GetGPUVirtualAddress();
				cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);
				stGPU[c_nMainGPU].m_pID3D12Device4->CreateConstantBufferView(&cbvDesc, stObject[i].m_pSRVHeap->GetCPUDescriptorHandleForHeapStart());
			}
		}

		// 记录场景物体渲染的捆绑包
		{{
				//在主显卡上创建渲染物体的捆绑包
				for (int i = 0; i < c_nMaxObjects; i++)
				{
					stObject[i].m_pIBundle->SetGraphicsRootSignature(stGPU[c_nMainGPU].m_pIRS.Get());
					stObject[i].m_pIBundle->SetPipelineState(stGPU[c_nMainGPU].m_pIPSO.Get());

					ID3D12DescriptorHeap* ppHeapsSphere[] = { stObject[i].m_pSRVHeap.Get(),stObject[i].m_pISampleHeap.Get() };
					stObject[i].m_pIBundle->SetDescriptorHeaps(_countof(ppHeapsSphere), ppHeapsSphere);

					//设置SRV
					CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSphere(stObject[i].m_pSRVHeap->GetGPUDescriptorHandleForHeapStart());
					stObject[i].m_pIBundle->SetGraphicsRootDescriptorTable(0, stGPUCBVHandleSphere);

					stGPUCBVHandleSphere.Offset(1, stGPU[c_nMainGPU].m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
					//设置CBV
					stObject[i].m_pIBundle->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleSphere);

					//设置Sample
					stObject[i].m_pIBundle->SetGraphicsRootDescriptorTable(2, stObject[i].m_pISampleHeap->GetGPUDescriptorHandleForHeapStart());

					//注意我们使用的渲染手法是三角形列表，也就是通常的Mesh网格
					stObject[i].m_pIBundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					stObject[i].m_pIBundle->IASetVertexBuffers(0, 1, &stObject[i].m_stVertexBufferView);
					stObject[i].m_pIBundle->IASetIndexBuffer(&stObject[i].m_stIndexsBufferView);

					//Draw Call！！！
					stObject[i].m_pIBundle->DrawIndexedInstanced(stObject[i].m_nIndexCnt, 1, 0, 0, 0);
					stObject[i].m_pIBundle->Close();
				}//End for
		}}

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

			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(nszVBQuad),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&pIVBQuad)));

			// 这次我们特意演示了如何将顶点缓冲上传到默认堆上的方式，与纹理上传默认堆实际是一样的
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(nszVBQuad),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIVBQuadUpload)));

			D3D12_SUBRESOURCE_DATA stVBDataQuad = {};
			stVBDataQuad.pData = reinterpret_cast<UINT8*>(stVertexQuad);
			stVBDataQuad.RowPitch = nszVBQuad;
			stVBDataQuad.SlicePitch = stVBDataQuad.RowPitch;

			UpdateSubresources<1>(stGPU[c_nSecondGPU].m_pICMDList.Get(), pIVBQuad.Get(), pIVBQuadUpload.Get(), 0, 0, 1, &stVBDataQuad);
			stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pIVBQuad.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

			pstVBVQuad.BufferLocation = pIVBQuad->GetGPUVirtualAddress();
			pstVBVQuad.StrideInBytes = sizeof(ST_GRS_VERTEX_QUAD);
			pstVBVQuad.SizeInBytes = sizeof(stVertexQuad);

			// 执行命令，将矩形顶点缓冲上传到第二个显卡的默认堆上
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDList->Close());
			ID3D12CommandList* ppRenderCommandLists[] = { stGPU[c_nSecondGPU].m_pICMDList.Get() };
			stGPU[c_nSecondGPU].m_pICMDQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

			n64CurrentFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pICMDQueue->Signal(stGPU[c_nSecondGPU].m_pIFence.Get(), n64CurrentFenceValue));
			n64FenceValue++;

			if (stGPU[c_nSecondGPU].m_pIFence->GetCompletedValue() < n64CurrentFenceValue)
			{
				GRS_THROW_IF_FAILED(
					stGPU[c_nSecondGPU].m_pIFence->SetEventOnCompletion(
						n64CurrentFenceValue
						, stGPU[c_nSecondGPU].m_hEventFence));
				WaitForSingleObject(stGPU[c_nSecondGPU].m_hEventFence, INFINITE);
			}
		}

		// 创建在辅助显卡上渲染单位矩形用的常量缓冲，CBV、SRV、Sample
		{
			// 创建辅助显卡上的常量缓冲区
			GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(szSecondPassCB) //注意缓冲尺寸设置为256边界对齐大小
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBResSecondPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBResSecondPass);

			GRS_THROW_IF_FAILED(pICBResSecondPass->Map(0, nullptr, reinterpret_cast<void**>(&pstCBSecondPass)));

			D3D12_DESCRIPTOR_HEAP_DESC stHeapDesc = {};
			stHeapDesc.NumDescriptors = g_nFrameBackBufCount + 2; //1 Const Bufer + 1 Texture + g_nFrameBackBufCount * Texture
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			//SRV堆
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stHeapDesc, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pISRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pISRVHeap);

			// Third Pass SRV Heap
			stHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stHeapDesc, IID_PPV_ARGS(&pISRVHeapThirdPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISRVHeapThirdPass);

			// CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = pICBResSecondPass->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = static_cast<UINT>(szSecondPassCB);

			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateConstantBufferView(&cbvDesc
				, stGPU[c_nSecondGPU].m_pISRVHeap->GetCPUDescriptorHandleForHeapStart());

			// SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emRTFmt;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			CD3DX12_CPU_DESCRIPTOR_HANDLE stSrvHandle(
				stGPU[c_nSecondGPU].m_pISRVHeap->GetCPUDescriptorHandleForHeapStart()
				, 1
				, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

			if (!bCrossAdapterTextureSupport)
			{
				// 使用复制的渲染目标纹理作为Shader的纹理资源
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
					pISecondAdapterTexutrePerFrame[nCurrentFrameIndex].Get()
					, &stSRVDesc
					, stSrvHandle);
			}
			else
			{
				// 直接使用共享的渲染目标复制的纹理作为纹理资源
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
					stGPU[c_nSecondGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
					, &stSRVDesc
					, stSrvHandle);
			}

			stSrvHandle.Offset(1, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);
			// Noise Texture SRV
			D3D12_RESOURCE_DESC stTXDesc = pINoiseTexture->GetDesc();
			stSRVDesc.Format = stTXDesc.Format;
			//创建噪声纹理描述符
			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
				pINoiseTexture.Get(), &stSRVDesc, stSrvHandle);

			//----------------------------------------------------------------------------------------
			// Third Pass SRV
			stSRVDesc.Format = emRTFmt;
			CD3DX12_CPU_DESCRIPTOR_HANDLE stThirdPassSrvHandle(
				pISRVHeapThirdPass->GetCPUDescriptorHandleForHeapStart());
			for (int i = 0; i < g_nFrameBackBufCount; i++)
			{
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
					pIOffLineRTRes[i].Get()
					, &stSRVDesc
					, stThirdPassSrvHandle);
				stThirdPassSrvHandle.Offset(1, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);
			}

			//----------------------------------------------------------------------------------------

			// 创建Sample Descriptor Heap 采样器描述符堆
			stHeapDesc.NumDescriptors = 1;  //只有一个Sample
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stHeapDesc, IID_PPV_ARGS(&stGPU[c_nSecondGPU].m_pISampleHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPU[c_nSecondGPU].m_pISampleHeap);

			// Third Pass Sample Heap
			GRS_THROW_IF_FAILED(
				stGPU[c_nSecondGPU].m_pID3D12Device4->CreateDescriptorHeap(
					&stHeapDesc, IID_PPV_ARGS(&pISampleHeapThirdPass)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISampleHeapThirdPass);

			// 创建Sample View 实际就是采样器
			D3D12_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateSampler(
				&stSamplerDesc
				, stGPU[c_nSecondGPU].m_pISampleHeap->GetCPUDescriptorHandleForHeapStart());

			// Third Pass Sample
			stGPU[c_nSecondGPU].m_pID3D12Device4->CreateSampler(
				&stSamplerDesc
				, pISampleHeapThirdPass->GetCPUDescriptorHandleForHeapStart());
		}

		// 执行主显卡上第二个Copy命令并等待所有的纹理都上传到了默认堆中
		{ {
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pICMDList->Close());
				ID3D12CommandList* ppRenderCommandLists[] = { stGPU[c_nMainGPU].m_pICMDList.Get() };
				stGPU[c_nMainGPU].m_pICMDQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

				n64CurrentFenceValue = n64FenceValue;
				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pICMDQueue->Signal(stGPU[c_nMainGPU].m_pIFence.Get(), n64CurrentFenceValue));

				GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, stGPU[c_nMainGPU].m_hEventFence));
				arHWaited.Add(stGPU[c_nMainGPU].m_hEventFence);
				n64FenceValue++;
			}}

		GRS_THROW_IF_FAILED(pICmdListCopy->Close());
		GRS_THROW_IF_FAILED(pICMDListThirdPass->Close());

		// Time Value 时间轴变量
		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		// 计算旋转角度需要的变量
		double dModelRotationYAngle = 0.0f;

		XMMATRIX mxRotY;
		// 计算投影矩阵
		XMMATRIX mxProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)iWndWidth / (FLOAT)iWndHeight, 0.1f, 1000.0f);

		//初始化都结束以后，再显示窗口
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		DWORD dwRet = 0;
		BOOL bExit = FALSE;
		//17、开始消息循环，并在其中不断渲染
		while (!bExit)
		{//消息循环
			dwRet = ::MsgWaitForMultipleObjects(
				static_cast<DWORD>(arHWaited.GetCount()), arHWaited.GetData(), FALSE, INFINITE, QS_ALLINPUT);
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

					for (int i = 0; i < c_nMaxObjects; i++)
					{
						XMMATRIX xmModulePos = XMMatrixTranslation(stObject[i].m_v4ModelPos.x
							, stObject[i].m_v4ModelPos.y
							, stObject[i].m_v4ModelPos.z);

						xmModulePos = XMMatrixMultiply(xmModulePos, xmMVP);

						XMStoreFloat4x4(&stObject[i].m_pMVPBuf->m_MVP, xmModulePos);
					}
				}

				//更新第二遍渲染的常量缓冲区，可以放在OnUpdate里
				{
					pstCBSecondPass->m_nFun = g_nFunNO;
					pstCBSecondPass->m_fQuatLevel = g_fQuatLevel;
					pstCBSecondPass->m_fWaterPower = g_fWaterPower;
				}

				//OnRender()
				{
					// 获取帧号
					nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

					// 主显卡渲染(First Pass)
					{
						stGPU[c_nMainGPU].m_pICMDAlloc->Reset();
						stGPU[c_nMainGPU].m_pICMDList->Reset(stGPU[c_nMainGPU].m_pICMDAlloc.Get(), stGPU[c_nMainGPU].m_pIPSO.Get());

						stGPU[c_nMainGPU].m_pICMDList->SetGraphicsRootSignature(stGPU[c_nMainGPU].m_pIRS.Get());
						stGPU[c_nMainGPU].m_pICMDList->SetPipelineState(stGPU[c_nMainGPU].m_pIPSO.Get());
						stGPU[c_nMainGPU].m_pICMDList->RSSetViewports(1, &stViewPort);
						stGPU[c_nMainGPU].m_pICMDList->RSSetScissorRects(1, &stScissorRect);

						stGPU[c_nMainGPU].m_pICMDList->ResourceBarrier(1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_COMMON
								, D3D12_RESOURCE_STATE_RENDER_TARGET));

						CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(
							stGPU[c_nMainGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nMainGPU].m_nRTVDescriptorSize);
						CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
							stGPU[c_nMainGPU].m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());

						stGPU[c_nMainGPU].m_pICMDList->OMSetRenderTargets(
							1, &stRTVHandle, false, &dsvHandle);
						stGPU[c_nMainGPU].m_pICMDList->ClearRenderTargetView(
							stRTVHandle, v4ClearColor, 0, nullptr);
						stGPU[c_nMainGPU].m_pICMDList->ClearDepthStencilView(
							dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

						//执行实际的物体绘制渲染，Draw Call！
						for (int i = 0; i < c_nMaxObjects; i++)
						{
							ID3D12DescriptorHeap* ppHeapsSkybox[]
								= { stObject[i].m_pSRVHeap.Get(),stObject[i].m_pISampleHeap.Get() };
							stGPU[c_nMainGPU].m_pICMDList->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
							stGPU[c_nMainGPU].m_pICMDList->ExecuteBundle(stObject[i].m_pIBundle.Get());
						}

						stGPU[c_nMainGPU].m_pICMDList->ResourceBarrier(1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_RENDER_TARGET
								, D3D12_RESOURCE_STATE_COMMON));

						GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pICMDList->Close());

						// 主命令队列上执行主命令列表
						ID3D12CommandList* ppRenderCommandLists[]
							= { stGPU[c_nMainGPU].m_pICMDList.Get() };
						stGPU[c_nMainGPU].m_pICMDQueue->ExecuteCommandLists(
							_countof(ppRenderCommandLists), ppRenderCommandLists);

						n64CurrentFenceValue = n64FenceValue;
						GRS_THROW_IF_FAILED(stGPU[c_nMainGPU].m_pICMDQueue->Signal(
							stGPU[c_nMainGPU].m_pIFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}

					// 将主显卡的渲染结果复制到共享纹理资源中(Copy Main Render Target become Shader Resource)
					{
						pICmdAllocCopy->Reset();
						pICmdListCopy->Reset(pICmdAllocCopy.Get(), nullptr);

						if (bCrossAdapterTextureSupport)
						{
							// 如果适配器支持跨适配器行主纹理，只需将该纹理复制到跨适配器纹理中。
							pICmdListCopy->CopyResource(
								stGPU[c_nMainGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get());
						}
						else
						{
							// 如果适配器不支持跨适配器行主纹理，则将纹理复制为缓冲区，
							// 以便可以显式地管理纹理行间距。使用呈现目标指定的内存布局将中间呈现目标复制到共享缓冲区中。
							D3D12_RESOURCE_DESC stRenderTargetDesc
								= stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stRenderTargetLayout = {};

							stGPU[c_nMainGPU].m_pID3D12Device4->GetCopyableFootprints(
								&stRenderTargetDesc, 0, 1, 0, &stRenderTargetLayout, nullptr, nullptr, nullptr);

							CD3DX12_TEXTURE_COPY_LOCATION dest(
								stGPU[c_nMainGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, stRenderTargetLayout);
							CD3DX12_TEXTURE_COPY_LOCATION src(
								stGPU[c_nMainGPU].m_pIRTRes[nCurrentFrameIndex].Get()
								, 0);
							CD3DX12_BOX box(0, 0, iWndWidth, iWndHeight);

							pICmdListCopy->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);
						}

						GRS_THROW_IF_FAILED(pICmdListCopy->Close());

						// 使用主显卡上的复制命令队列完成渲染目标资源到共享资源间的复制
						// 通过调用命令队列的Wait命令实现同一个GPU上的各命令队列之间的等待同步 
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Wait(stGPU[c_nMainGPU].m_pIFence.Get(), n64CurrentFenceValue));

						ID3D12CommandList* ppCopyCommandLists[] = { pICmdListCopy.Get() };
						pICmdQueueCopy->ExecuteCommandLists(_countof(ppCopyCommandLists), ppCopyCommandLists);

						n64CurrentFenceValue = n64FenceValue;
						// 复制命令的信号设置在共享的围栏对象上这样使得第二个显卡的命令队列可以在这个围栏上等待
						// 从而完成不同GPU命令队列间的同步等待
						GRS_THROW_IF_FAILED(pICmdQueueCopy->Signal(
							stGPU[c_nMainGPU].m_pISharedFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}

					// 开始辅助显卡的渲染，首先调用水彩画效果（Second Pass）
					{
						stGPU[c_nSecondGPU].m_pICMDAlloc->Reset();
						stGPU[c_nSecondGPU].m_pICMDList->Reset(
							stGPU[c_nSecondGPU].m_pICMDAlloc.Get(), stGPU[c_nSecondGPU].m_pIPSO.Get());

						// 如果不支持跨显卡纹理共享，那么就从共享堆复制资源过来，相当于从Upload堆复制数据到Default堆上
						if (!bCrossAdapterTextureSupport)
						{
							// 将共享堆中的缓冲区复制到辅助适配器可以从中取样的纹理中。
							D3D12_RESOURCE_BARRIER stResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
								pISecondAdapterTexutrePerFrame[nCurrentFrameIndex].Get(),
								D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
								D3D12_RESOURCE_STATE_COPY_DEST);

							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResBarrier);

							D3D12_RESOURCE_DESC stSecondaryAdapterTexture
								= pISecondAdapterTexutrePerFrame[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTextureLayout = {};

							stGPU[c_nSecondGPU].m_pID3D12Device4->GetCopyableFootprints(
								&stSecondaryAdapterTexture, 0, 1, 0, &stTextureLayout, nullptr, nullptr, nullptr);

							CD3DX12_TEXTURE_COPY_LOCATION dest(
								pISecondAdapterTexutrePerFrame[nCurrentFrameIndex].Get(), 0);
							CD3DX12_TEXTURE_COPY_LOCATION src(
								stGPU[c_nSecondGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
								, stTextureLayout);
							CD3DX12_BOX box(0, 0, iWndWidth, iWndHeight);

							stGPU[c_nSecondGPU].m_pICMDList->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);

							stResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
							stResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stResBarrier);
						}

						//将第一遍渲染的结果作为纹理
						{
							CD3DX12_CPU_DESCRIPTOR_HANDLE stSrvHandle(
								stGPU[c_nSecondGPU].m_pISRVHeap->GetCPUDescriptorHandleForHeapStart()
								, 1
								, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

							D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
							stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
							stSRVDesc.Format = emRTFmt;
							stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
							stSRVDesc.Texture2D.MipLevels = 1;

							if (!bCrossAdapterTextureSupport)
							{
								// 使用复制的渲染目标纹理作为Shader的纹理资源
								stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
									pISecondAdapterTexutrePerFrame[nCurrentFrameIndex].Get()
									, &stSRVDesc
									, stSrvHandle);
							}
							else
							{
								// 直接使用共享的渲染目标复制的纹理作为纹理资源
								stGPU[c_nSecondGPU].m_pID3D12Device4->CreateShaderResourceView(
									stGPU[c_nSecondGPU].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get()
									, &stSRVDesc
									, stSrvHandle);
							}
						}

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootSignature(stGPU[c_nSecondGPU].m_pIRS.Get());
						stGPU[c_nSecondGPU].m_pICMDList->SetPipelineState(stGPU[c_nSecondGPU].m_pIPSO.Get());
						ID3D12DescriptorHeap* ppHeaps[]
							= { stGPU[c_nSecondGPU].m_pISRVHeap.Get()
								,stGPU[c_nSecondGPU].m_pISampleHeap.Get() };
						stGPU[c_nSecondGPU].m_pICMDList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

						stGPU[c_nSecondGPU].m_pICMDList->RSSetViewports(1, &stViewPort);
						stGPU[c_nSecondGPU].m_pICMDList->RSSetScissorRects(1, &stScissorRect);

						D3D12_RESOURCE_BARRIER stRTSecondaryBarriers = CD3DX12_RESOURCE_BARRIER::Transition(
							pIOffLineRTRes[nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
							, D3D12_RESOURCE_STATE_RENDER_TARGET);

						stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &stRTSecondaryBarriers);

						CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVSecondaryHandle(
							pIRTVOffLine->GetCPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nSecondGPU].m_nRTVDescriptorSize);
						stGPU[c_nSecondGPU].m_pICMDList->OMSetRenderTargets(1, &stRTVSecondaryHandle, false, nullptr);
						stGPU[c_nSecondGPU].m_pICMDList->ClearRenderTargetView(stRTVSecondaryHandle, v4ClearColor, 0, nullptr);

						// 开始绘制矩形
						CD3DX12_GPU_DESCRIPTOR_HANDLE stSRVHandle(
							stGPU[c_nSecondGPU].m_pISRVHeap->GetGPUDescriptorHandleForHeapStart()
							, 0
							, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(0, stSRVHandle);
						stSRVHandle.Offset(1, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);
						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(1, stSRVHandle);
						stGPU[c_nSecondGPU].m_pICMDList->SetGraphicsRootDescriptorTable(2
							, stGPU[c_nSecondGPU].m_pISampleHeap->GetGPUDescriptorHandleForHeapStart());

						stGPU[c_nSecondGPU].m_pICMDList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
						stGPU[c_nSecondGPU].m_pICMDList->IASetVertexBuffers(0, 1, &pstVBVQuad);
						// Draw Call!
						stGPU[c_nSecondGPU].m_pICMDList->DrawInstanced(4, 1, 0, 0);

						// 设置好同步围栏
						stGPU[c_nSecondGPU].m_pICMDList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
								pIOffLineRTRes[nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_RENDER_TARGET
								, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

						GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDList->Close());

						// 使用辅助显卡上的主命令队列执行命令列表
						// 辅助显卡上的主命令队列通过等待共享的围栏对象最终完成了与主显卡之间的同步
						// 注意Wait在ExecuteCommandLists之前，这样就保证了主显卡渲染完毕并且复制完毕后辅助显卡才会开始执行第二遍渲染
						GRS_THROW_IF_FAILED(stGPU[c_nSecondGPU].m_pICMDQueue->Wait(stGPU[c_nSecondGPU].m_pISharedFence.Get(), n64CurrentFenceValue));
					}

					// 调用高斯模糊效果（Third Pass）
					{{
						pICMDAllocThirdPass->Reset();
						pICMDListThirdPass->Reset(pICMDAllocThirdPass.Get(), pIPSOThirdPass.Get());

						//将第二遍渲染的结果作为纹理
						CD3DX12_CPU_DESCRIPTOR_HANDLE stSrvHandle(
							pISRVHeapThirdPass->GetCPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						pICMDListThirdPass->SetGraphicsRootSignature(pIRSThirdPass.Get());

						if( 0 == g_nUsePSID )
						{
							pICMDListThirdPass->SetPipelineState(pIPSODoNothing.Get());
						}
						else if( 1 == g_nUsePSID )
						{
							pICMDListThirdPass->SetPipelineState(pIPSOThirdPass.Get());
						}
						else
						{
							pICMDListThirdPass->SetPipelineState(pIPSOBothwayBlur.Get());
						}
						
						ID3D12DescriptorHeap* ppHeaps[]
							= { pISRVHeapThirdPass.Get()
								,pISampleHeapThirdPass.Get() };
						pICMDListThirdPass->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

						pICMDListThirdPass->RSSetViewports(1, &stViewPort);
						pICMDListThirdPass->RSSetScissorRects(1, &stScissorRect);

						D3D12_RESOURCE_BARRIER stRTSecondaryBarriers = CD3DX12_RESOURCE_BARRIER::Transition(
							stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_PRESENT
							, D3D12_RESOURCE_STATE_RENDER_TARGET);

						pICMDListThirdPass->ResourceBarrier(1, &stRTSecondaryBarriers);

						CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(
							stGPU[c_nSecondGPU].m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nSecondGPU].m_nRTVDescriptorSize);
						pICMDListThirdPass->OMSetRenderTargets(1, &stRTVHandle, false, nullptr);
						float f4ClearColor[] = { 1.0f,0.0f,1.0f,1.0f }; //故意使用与主显卡渲染目标不同的清除色，查看是否有“露底”的问题
						pICMDListThirdPass->ClearRenderTargetView(stRTVHandle, f4ClearColor, 0, nullptr);

						// 开始绘制矩形
						CD3DX12_GPU_DESCRIPTOR_HANDLE stSRVHandle(
							pISRVHeapThirdPass->GetGPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPU[c_nSecondGPU].m_nSRVDescriptorSize);

						pICMDListThirdPass->SetGraphicsRootDescriptorTable(0, stSRVHandle);
						pICMDListThirdPass->SetGraphicsRootDescriptorTable(1
							, pISampleHeapThirdPass->GetGPUDescriptorHandleForHeapStart());

						pICMDListThirdPass->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
						pICMDListThirdPass->IASetVertexBuffers(0, 1, &pstVBVQuad);
						// Draw Call!
						pICMDListThirdPass->DrawInstanced(4, 1, 0, 0);

						// 设置好同步围栏
						pICMDListThirdPass->ResourceBarrier(1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								stGPU[c_nSecondGPU].m_pIRTRes[nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_RENDER_TARGET
								, D3D12_RESOURCE_STATE_PRESENT));

						GRS_THROW_IF_FAILED(pICMDListThirdPass->Close());
					}}

					ID3D12CommandList* ppBlurCommandLists[]
						= { stGPU[c_nSecondGPU].m_pICMDList.Get(), pICMDListThirdPass.Get() };
					stGPU[c_nSecondGPU].m_pICMDQueue->ExecuteCommandLists(_countof(ppBlurCommandLists), ppBlurCommandLists);

					// 执行Present命令最终呈现画面
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					n64CurrentFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(
						stGPU[c_nSecondGPU].m_pICMDQueue->Signal(
							stGPU[c_nSecondGPU].m_pIFence.Get(), n64CurrentFenceValue));
					GRS_THROW_IF_FAILED(
						stGPU[c_nSecondGPU].m_pIFence->SetEventOnCompletion(
							n64CurrentFenceValue, stGPU[c_nSecondGPU].m_hEventFence));
					n64FenceValue++;

					//开始渲染后只需要在辅助显卡的事件句柄上等待即可
					//因为辅助显卡已经在GPU侧与主显卡通过Wait命令进行了同步
					arHWaited.RemoveAll();
					arHWaited.Add(stGPU[c_nSecondGPU].m_hEventFence);
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
	catch (CGRSCOMException & e)
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
			g_nFunNO += 1;
			if (g_nFunNO >= g_nMaxFunNO)
			{
				g_nFunNO = 0;
			}
		}

		if ( VK_TAB == n16KeyCode )
		{
			++ g_nUsePSID;
			if ( g_nUsePSID > 2 )
			{
				g_nUsePSID = 0;
			}
		}

		if (1 == g_nFunNO)
		{//当进行水彩画效果渲染时控制生效
			if ('Q' == n16KeyCode || 'q' == n16KeyCode)
			{//q 增加水彩取色半径
				g_fWaterPower += 1.0f;
				if (g_fWaterPower >= 64.0f)
				{
					g_fWaterPower = 8.0f;
				}
			}

			if ('E' == n16KeyCode || 'e' == n16KeyCode)
			{//e 控制量化参数，范围 2 - 6
				g_fQuatLevel += 1.0f;
				if (g_fQuatLevel > 6.0f)
				{
					g_fQuatLevel = 2.0f;
				}
			}
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

		if ( VK_ESCAPE == n16KeyCode )
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

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& m_nVertexCnt, ST_GRS_VERTEX_MODULE*& ppVertex, UINT*& ppIndices)
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
		fin >> m_nVertexCnt;

		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin.get(input);
		fin.get(input);

		ppVertex = (ST_GRS_VERTEX_MODULE*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, m_nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
		ppIndices = (UINT*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, m_nVertexCnt * sizeof(UINT));

		for (UINT i = 0; i < m_nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_v4Position.x >> ppVertex[i].m_v4Position.y >> ppVertex[i].m_v4Position.z;
			ppVertex[i].m_v4Position.w = 1.0f;
			fin >> ppVertex[i].m_vTex.x >> ppVertex[i].m_vTex.y;
			fin >> ppVertex[i].m_vNor.x >> ppVertex[i].m_vNor.y >> ppVertex[i].m_vNor.z;

			ppIndices[i] = i;
		}
	}
	catch (CGRSCOMException & e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}