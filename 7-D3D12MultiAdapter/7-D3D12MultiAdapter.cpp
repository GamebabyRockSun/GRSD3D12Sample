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
#define GRS_WND_TITLE	_T("GRS D3D12 MultiAdapter Sample")

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

// 顶点结构
struct ST_GRS_VERTEX_MODULE
{
	XMFLOAT4 m_vPos;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

// 辅助显卡上渲染单位矩形的顶点结构
struct ST_GRS_VERTEX_QUAD
{
	XMFLOAT4 m_vPos;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
};

const UINT g_nFrameBackBufCount = 3u;
// 显卡参数集合
struct ST_GRS_GPU_PARAMS
{
	UINT								m_nIndex;
	UINT								m_nszRTV;
	UINT								m_nszSRVCBVUAV;

	ComPtr<ID3D12Device4>				m_pID3DDevice;
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
	ST_GRS_MVP*							pMVPBuf;
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
BOOL LoadMeshVertex(const CHAR*pszMeshFileName, UINT&nVertexCnt, ST_GRS_VERTEX_MODULE*&ppVertex, UINT*&ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	::CoInitialize(nullptr);  //for WIC & COM
	try
	{
		int									iWndWidth = 1024;
		int									iWndHeight = 768;
		HWND								hWnd = nullptr;
		MSG									msg = {};
		TCHAR								pszAppPath[MAX_PATH] = {};

		UINT								nDXGIFactoryFlags = 0U;
		UINT								nCurrentFrameIndex = 0u;
		DXGI_FORMAT							emRTFmt = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT							emDSFmt = DXGI_FORMAT_D24_UNORM_S8_UINT;

		CD3DX12_VIEWPORT					stViewPort(0.0f, 0.0f, static_cast<float>(iWndWidth), static_cast<float>(iWndHeight));
		CD3DX12_RECT						stScissorRect(0, 0, static_cast<LONG>(iWndWidth), static_cast<LONG>(iWndHeight));

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
		CD3DX12_RESOURCE_DESC				stRenderTargetDesc = {};
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

		HANDLE								hFenceEvent = nullptr;
		UINT64								n64CurrentFenceValue = 0;
		UINT64								n64FenceValue = 1ui64;


		// 得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
		{
			UINT nBytes = GetCurrentDirectory(MAX_PATH, pszAppPath);
			if (MAX_PATH == nBytes)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
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

			ShowWindow(hWnd, nCmdShow);
			UpdateWindow(hWnd);
		}

		// 打开显示子系统的调试支持
		{
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
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
		{//判定主次显卡的方法，与DirectX12 示例中的方法不同，我们使用的稍微准确一些
		 //，它里面用的就是序号0默认是集显1是独显然后按主次创建

			D3D12_FEATURE_DATA_ARCHITECTURE stArchitecture = {};
			DXGI_ADAPTER_DESC1 stAdapterDesc = {};
			IDXGIAdapter1*	pIAdapterTmp	= nullptr;
			ID3D12Device4*	pID3DDeviceTmp	= nullptr;
			IDXGIOutput*	pIOutput		= nullptr;
			HRESULT			hrEnumOutput	= S_OK;
			UINT			i = 0;
			for (i = 0;
				(i < nMaxGPUParams) &&
				SUCCEEDED(pIDXGIFactory6->EnumAdapterByGpuPreference(
					i
					, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
					, IID_PPV_ARGS(&pIAdapterTmp)));
				++i)
			{
				ZeroMemory(&stAdapterDesc, sizeof(DXGI_ADAPTER_DESC1));
				GRS_THROW_IF_FAILED(pIAdapterTmp->GetDesc1(&stAdapterDesc));

				if (stAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{//跳过软件虚拟适配器设备，
				 //注释这个if判断，可以使用一个真实GPU和一个虚拟软适配器来看双GPU的示例
					continue;
				}

				if ( i == nIDGPUMain )
				{	
					GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp
						, D3D_FEATURE_LEVEL_12_1
						, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pID3DDevice)));
				}
				else
				{	
					GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp
						, D3D_FEATURE_LEVEL_12_1
						, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pID3DDevice)));
				}

			//for ( UINT nAdapterIndex = 0; DXGI_ERROR_NOT_FOUND != pIDXGIFactory5->EnumAdapters1(nAdapterIndex, &pIAdapterTmp); ++ nAdapterIndex)
			//{
			//	
			//	pIAdapterTmp->GetDesc1(&stAdapterDesc);

			//	if (stAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			//	{//跳过软件虚拟适配器设备
			//		GRS_SAFE_RELEASE(pIAdapterTmp);
			//		continue;
			//	}

			//	//第一种方法，通过判定那个显卡带有输出
			//	hrEnumOutput = pIAdapterTmp->EnumOutputs(0, &pIOutput);

			//	if ( SUCCEEDED(hrEnumOutput) && nullptr != pIOutput)
			//	{//该适配器带有显示输出，通常是集显（针对笔记本的情况）
			//		//我们将集显称为Main Device，因为用它来后处理和最终输出
			//		GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp
			//			, D3D_FEATURE_LEVEL_12_1
			//			, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pID3DDevice)));
			//	}
			//	else
			//	{//不带显示输出的，通常是独显（针对笔记本的情况）
			//		//我们用独显来完成主场景渲染，当然它就是渲染到纹理，后面会看到我们使用的是共享显存的纹理
			//		GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pID3DDevice)));
			//	}
			//
			//	GRS_SAFE_RELEASE(pIOutput);


				//第二种判定主次显卡的方法，就是看谁是UMA的谁不是，这个在之前的教程示例中已经详细讲解过

				//GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapterTmp, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3DDeviceTmp)));
				//GRS_THROW_IF_FAILED(pID3DDeviceTmp->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE
				//	, &stArchitecture, sizeof(D3D12_FEATURE_DATA_ARCHITECTURE)));
				////或者我们通过判定是否UMA架构来决定谁主谁辅
				//if ( stArchitecture.UMA )
				//{
				//	if ( nullptr == pID3DDeviceSecondary.Get() )
				//	{
				//		pID3DDeviceSecondary.Attach(pID3DDeviceTmp);
				//		pID3DDeviceTmp = nullptr;
				//	}
				//	else
				//	{
				//		//你的显卡太多了，你自己看怎么用吧，方法都告诉你了
				//	}
				//}
				//else
				//{
				//	if ( nullptr == pID3DDeviceMain.Get() )
				//	{
				//		pID3DDeviceMain.Attach(pID3DDeviceTmp);
				//		pID3DDeviceTmp = nullptr;
				//	}
				//	else
				//	{
				//		//你的显卡太多了，你自己看怎么用吧，方法都告诉你了
				//	}

				//}

				//GRS_SAFE_RELEASE(pID3DDeviceTmp);

				GRS_SAFE_RELEASE(pIAdapterTmp);
			}

			//---------------------------------------------------------------------------------------------
			if ( nullptr == stGPUParams[nIDGPUMain].m_pID3DDevice.Get() 
				|| nullptr == stGPUParams[nIDGPUSecondary].m_pID3DDevice.Get() )
			{// 可怜的机器上居然没有两个以上的显卡 还是先退出了事 当然你可以使用软适配器凑活看下例子
				OutputDebugString(_T("\n机器中显卡数量不足两个，示例程序退出！\n"));
				throw CGRSCOMException(E_FAIL);
			}

			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPUParams[nIDGPUMain].m_pID3DDevice);
			GRS_SET_D3D12_DEBUGNAME_COMPTR(stGPUParams[nIDGPUSecondary].m_pID3DDevice);
		}

		// 创建命令队列
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			for (int i = 0; i < nMaxGPUParams; i++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[i].m_pID3DDevice->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&stGPUParams[i].m_pICmdQueue)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[i].m_pICmdQueue.Get(), _T("m_pICmdQueue"), i);

				//借用这个循环完成辅助参数的赋值，注意描述符堆上的每个描述符的大小可能因不同的GPU而不同，所以单独作为GPU的参数
				//同时将这些值一次性取出并存储，就不用每次调用函数而导致性能受损和调用错误引起的意外问题
				stGPUParams[i].m_nIndex = i;
				stGPUParams[i].m_nszRTV = stGPUParams[i].m_pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				stGPUParams[i].m_nszSRVCBVUAV = stGPUParams[i].m_pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}

			//复制命令队列创建在独显上，因为独显的性能强悍一些
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICmdQueueCopy)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICmdQueueCopy);
		}

		// 创建围栏对象，以及多GPU同步围栏对象
		{
			for (int iGPUIndex = 0; iGPUIndex < nMaxGPUParams; iGPUIndex++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIFence)));
			}

			// 在主显卡上创建一个可共享的围栏对象
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateFence(0
				, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER
				, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pISharedFence)));

			// 共享这个围栏，通过句柄方式
			HANDLE hFenceShared = nullptr;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateSharedHandle(
				stGPUParams[nIDGPUMain].m_pISharedFence.Get(),
				nullptr,
				GENERIC_ALL,
				nullptr,
				&hFenceShared));

			// 在辅助显卡上打开这个围栏对象完成共享
			HRESULT hrOpenSharedHandleResult = stGPUParams[nIDGPUSecondary].m_pID3DDevice->OpenSharedHandle(hFenceShared
				, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pISharedFence));

			// 先关闭句柄，再判定是否共享成功
			::CloseHandle(hFenceShared);
			GRS_THROW_IF_FAILED(hrOpenSharedHandleResult);

			hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hFenceEvent == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
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
			stRTVHeapDesc.NumDescriptors	= g_nFrameBackBufCount;
			stRTVHeapDesc.Type				= D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags				= D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			for (int i = 0; i < nMaxGPUParams; i++)
			{
				GRS_THROW_IF_FAILED(stGPUParams[i].m_pID3DDevice->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&stGPUParams[i].m_pIDHRTV)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[i].m_pIDHRTV.Get(), _T("m_pIDHRTV"), i);
			}
			CD3DX12_CLEAR_VALUE   stClearValue(stSwapChainDesc.Format, arf4ClearColor);
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

			for ( UINT iGPUIndex = 0; iGPUIndex < nMaxGPUParams; iGPUIndex++ )
			{
				CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(stGPUParams[iGPUIndex].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart());

				for (UINT j = 0; j < g_nFrameBackBufCount; j++)
				{
					if ( iGPUIndex == nIDGPUSecondary )
					{
						GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(j, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIRTRes[j])));
					}
					else
					{
						GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommittedResource(
							&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
							D3D12_HEAP_FLAG_NONE,
							&stRenderTargetDesc,
							D3D12_RESOURCE_STATE_COMMON,
							&stClearValue,
							IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIRTRes[j])));
					}

					stGPUParams[iGPUIndex].m_pID3DDevice->CreateRenderTargetView(stGPUParams[iGPUIndex].m_pIRTRes[j].Get(), nullptr, rtvHandle);
					rtvHandle.Offset(1, stGPUParams[iGPUIndex].m_nszRTV);

					GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
						, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pICmdAllocPerFrame[j])));

					if ( iGPUIndex == nIDGPUMain )
					{ //创建主显卡上的复制命令队列用的命令分配器，每帧使用一个分配器
						GRS_THROW_IF_FAILED( stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_COPY
							, IID_PPV_ARGS(&pICmdAllocCopyPerFrame[j])));
					}
				}

				// 创建每个GPU的命令列表对象
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommandList(0
					, D3D12_COMMAND_LIST_TYPE_DIRECT
					, stGPUParams[iGPUIndex].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get()
					, nullptr
					, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pICmdList)));

				
				if (SUCCEEDED(StringCchPrintfW(pszDebugName,MAX_PATH, L"stGPUParams[%u].m_pICmdList", iGPUIndex)))
				{
					stGPUParams[iGPUIndex].m_pICmdList->SetName(pszDebugName);
				}
				
				if ( iGPUIndex == nIDGPUMain )
				{// 在主显卡上创建复制命令列表对象
					GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommandList(0
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

				//使用隐式默认堆创建一个深度蜡板缓冲区，
				//因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateCommittedResource(
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
				GRS_THROW_IF_FAILED(stGPUParams[iGPUIndex].m_pID3DDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&stGPUParams[iGPUIndex].m_pIDHDSVTex)));
				GRS_SetD3D12DebugNameIndexed(stGPUParams[iGPUIndex].m_pIDHDSVTex.Get(), _T("m_pIDHDSVTex"), iGPUIndex);

				stGPUParams[iGPUIndex].m_pID3DDevice->CreateDepthStencilView(stGPUParams[iGPUIndex].m_pIDSTex.Get()
					, &stDepthStencilDesc
					, stGPUParams[iGPUIndex].m_pIDHDSVTex->GetCPUDescriptorHandleForHeapStart());
			}

		}

		// 创建跨适配器的共享资源堆
		{
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS stOptions = {};
			// 通过检测带有显示输出的显卡是否支持跨显卡资源来决定跨显卡的资源如何创建
			GRS_THROW_IF_FAILED( stGPUParams[nIDGPUSecondary].m_pID3DDevice->CheckFeatureSupport(
				D3D12_FEATURE_D3D12_OPTIONS, reinterpret_cast<void*>(&stOptions), sizeof(stOptions)));

			bCrossAdapterTextureSupport = stOptions.CrossAdapterRowMajorTextureSupported;

			UINT64 n64szTexture = 0;
			D3D12_RESOURCE_DESC stCrossAdapterResDesc = {};

			if ( bCrossAdapterTextureSupport )
			{
				// 如果支持那么直接创建跨显卡资源堆
				stCrossAdapterResDesc = stRenderTargetDesc;
				stCrossAdapterResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
				stCrossAdapterResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

				D3D12_RESOURCE_ALLOCATION_INFO stTextureInfo = stGPUParams[nIDGPUMain].m_pID3DDevice->GetResourceAllocationInfo(0, 1, &stCrossAdapterResDesc);
				n64szTexture = stTextureInfo.SizeInBytes;
			}
			else
			{
				// 如果不支持，那么我们就需要先在主显卡上创建用于复制渲染结果的资源堆，然后再共享堆到辅助显卡上
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT stResLayout = {};
				stGPUParams[nIDGPUMain].m_pID3DDevice->GetCopyableFootprints(&stRenderTargetDesc, 0, 1, 0, &stResLayout, nullptr, nullptr, nullptr);
				n64szTexture = GRS_UPPER(stResLayout.Footprint.RowPitch * stResLayout.Footprint.Height, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
				stCrossAdapterResDesc = CD3DX12_RESOURCE_DESC::Buffer(n64szTexture, D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER);
			}

			// 创建跨显卡共享的资源堆
			CD3DX12_HEAP_DESC stCrossHeapDesc(
				n64szTexture * g_nFrameBackBufCount,
				D3D12_HEAP_TYPE_DEFAULT,
				0,
				D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateHeap(&stCrossHeapDesc
				, IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pICrossAdapterHeap)) );
		
			HANDLE hHeap = nullptr;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateSharedHandle(
				stGPUParams[nIDGPUMain].m_pICrossAdapterHeap.Get(),
				nullptr,
				GENERIC_ALL,
				nullptr,
				&hHeap));

			HRESULT hrOpenSharedHandle = stGPUParams[nIDGPUSecondary].m_pID3DDevice->OpenSharedHandle(hHeap
				, IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pICrossAdapterHeap));

			// 先关闭句柄，再判定是否共享成功
			::CloseHandle(hHeap);

			GRS_THROW_IF_FAILED(hrOpenSharedHandle);

			// 以定位方式在共享堆上创建每个显卡上的资源
			for ( UINT nFrameIndex = 0; nFrameIndex < g_nFrameBackBufCount; nFrameIndex++ )
			{
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreatePlacedResource(
					stGPUParams[nIDGPUMain].m_pICrossAdapterHeap.Get(),
					n64szTexture * nFrameIndex,
					&stCrossAdapterResDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nFrameIndex])));

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreatePlacedResource(
					stGPUParams[nIDGPUSecondary].m_pICrossAdapterHeap.Get(),
					n64szTexture * nFrameIndex,
					&stCrossAdapterResDesc,
					bCrossAdapterTextureSupport ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE,
					nullptr,
					IID_PPV_ARGS(&stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nFrameIndex])));

				if ( ! bCrossAdapterTextureSupport )
				{
					// If the primary adapter's render target must be shared as a buffer,
					// create a texture resource to copy it into on the secondary adapter.
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateCommittedResource(
						&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
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
		{//这个例子中，所有物体使用相同的根签名，因为渲染过程中需要的参数是一样的
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};

			// 检测是否支持V1.1版本的根签名
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE
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

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSMain)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSMain);

			//------------------------------------------------------------------------------------------------

			// 创建渲染Quad的根签名对象，注意这个根签名在辅助显卡上用
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			CD3DX12_DESCRIPTOR_RANGE1 stDRQuad[2];
			stDRQuad[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			stDRQuad[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

			CD3DX12_ROOT_PARAMETER1 stRPQuad[2];
			stRPQuad[0].InitAsDescriptorTable(1, &stDRQuad[0], D3D12_SHADER_VISIBILITY_PIXEL);//SRV仅PS可见
			stRPQuad[1].InitAsDescriptorTable(1, &stDRQuad[1], D3D12_SHADER_VISIBILITY_PIXEL);//SAMPLE仅PS可见

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

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSQuad)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSQuad);			
		}

		// 编译Shader创建渲染管线状态对象
		{
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT nShaderCompileFlags = 0;
#endif
			//编译为行矩阵形式	   
			nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s\\Shader\\TextureModule.hlsl"), pszAppPath);

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
			stPSODesc.VS = CD3DX12_SHADER_BYTECODE(pIVSMain.Get());
			stPSODesc.PS = CD3DX12_SHADER_BYTECODE(pIPSMain.Get());
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

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPSOMain)));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOMain);

			//---------------------------------------------------------------------------------------------------
			//用于渲染单位矩形的管道状态对象
			TCHAR pszUnitQuadShader[MAX_PATH] = {};
			StringCchPrintf(pszUnitQuadShader, MAX_PATH, _T("%s\\Shader\\UnitQuad.hlsl"), pszAppPath);
			
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
			stPSOQuadDesc.VS = CD3DX12_SHADER_BYTECODE(pIVSQuad.Get());
			stPSOQuadDesc.PS = CD3DX12_SHADER_BYTECODE(pIPSQuad.Get());
			stPSOQuadDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			stPSOQuadDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			stPSOQuadDesc.SampleMask = UINT_MAX;
			stPSOQuadDesc.SampleDesc.Count = 1;
			stPSOQuadDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSOQuadDesc.NumRenderTargets = 1;
			stPSOQuadDesc.RTVFormats[0] = emRTFmt;
			stPSOQuadDesc.DepthStencilState.StencilEnable = FALSE;
			stPSOQuadDesc.DepthStencilState.DepthEnable = FALSE;

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateGraphicsPipelineState(&stPSOQuadDesc
				, IID_PPV_ARGS(&pIPSOQuad)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOQuad);
		}

		// 准备多个物体参数
		{
			USES_CONVERSION;
			// 物体个性参数
			StringCchPrintf(stModuleParams[nSphere].pszDDSFile, MAX_PATH, _T("%s\\Mesh\\sphere.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nSphere].pszMeshFile, MAX_PATH, "%s\\Mesh\\sphere.txt", T2A(pszAppPath));
			stModuleParams[nSphere].v4ModelPos = XMFLOAT4(2.0f, 2.0f, 0.0f, 1.0f);

			// 立方体个性参数
			StringCchPrintf(stModuleParams[nCube].pszDDSFile, MAX_PATH, _T("%s\\Mesh\\Cube.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nCube].pszMeshFile, MAX_PATH, "%s\\Mesh\\Cube.txt", T2A(pszAppPath));
			stModuleParams[nCube].v4ModelPos = XMFLOAT4(-2.0f, 2.0f, 0.0f, 1.0f);

			// 平板个性参数
			StringCchPrintf(stModuleParams[nPlane].pszDDSFile, MAX_PATH, _T("%s\\Mesh\\Plane.dds"), pszAppPath);
			StringCchPrintfA(stModuleParams[nPlane].pszMeshFile, MAX_PATH, "%s\\Mesh\\Plane.txt", T2A(pszAppPath));
			stModuleParams[nPlane].v4ModelPos = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

			// Mesh Value
			ST_GRS_VERTEX_MODULE*				pstVertices = nullptr;
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
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE
					, IID_PPV_ARGS(&stModuleParams[i].pIBundleAlloc)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundleAlloc.Get(), _T("pIBundleAlloc"), i);

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE
					, stModuleParams[i].pIBundleAlloc.Get(), nullptr, IID_PPV_ARGS(&stModuleParams[i].pIBundle)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIBundle.Get(), _T("pIBundle"), i);

				//加载DDS
				pbDDSData.release();
				stArSubResources.clear();
				GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(stGPUParams[nIDGPUMain].m_pID3DDevice.Get()
					, stModuleParams[i].pszDDSFile
					, stModuleParams[i].pITexture.GetAddressOf()
					, pbDDSData
					, stArSubResources
					, SIZE_MAX
					, &emAlphaMode
					, &bIsCube));

				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITexture.Get(), _T("pITexture"), i);

				UINT64 n64szUpSphere = GetRequiredIntermediateSize(
					stModuleParams[i].pITexture.Get()
					, 0
					, static_cast<UINT>(stArSubResources.size()));
				D3D12_RESOURCE_DESC stTXDesc = stModuleParams[i].pITexture->GetDesc();

				//创建上传堆
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(n64szUpSphere)
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pITextureUpload)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pITextureUpload.Get(), _T("pITextureUpload"), i);

				//上传DDS
				UpdateSubresources(stGPUParams[nIDGPUMain].m_pICmdList.Get()
					, stModuleParams[i].pITexture.Get()
					, stModuleParams[i].pITextureUpload.Get()
					, 0
					, 0
					, static_cast<UINT>(stArSubResources.size())
					, stArSubResources.data());

				//同步
				stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
					stModuleParams[i].pITexture.Get()
					, D3D12_RESOURCE_STATE_COPY_DEST
					, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));


				//创建SRV CBV堆
				D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
				stSRVHeapDesc.NumDescriptors = 2; //1 SRV + 1 CBV
				stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateDescriptorHeap(&stSRVHeapDesc
					, IID_PPV_ARGS(&stModuleParams[i].pISRVCBVHp)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pISRVCBVHp.Get(), _T("pISRVCBVHp"), i);

				//创建SRV
				D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
				stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				stSRVDesc.Format = stTXDesc.Format;
				stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				stSRVDesc.Texture2D.MipLevels = 1;

				CD3DX12_CPU_DESCRIPTOR_HANDLE stCBVSRVHandle(
					stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart()
					, 1
					, stGPUParams[nIDGPUMain].m_pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

				stGPUParams[nIDGPUMain].m_pID3DDevice->CreateShaderResourceView(stModuleParams[i].pITexture.Get()
					, &stSRVDesc
					, stCBVSRVHandle);

				// 创建Sample
				D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
				stSamplerHeapDesc.NumDescriptors = 1;
				stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
				stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateDescriptorHeap(&stSamplerHeapDesc
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

				stGPUParams[nIDGPUMain].m_pID3DDevice->CreateSampler(&stSamplerDesc
					, stModuleParams[i].pISampleHp->GetCPUDescriptorHandleForHeapStart());

				//加载网格数据
				LoadMeshVertex(stModuleParams[i].pszMeshFile, stModuleParams[i].nVertexCnt, pstVertices, pnIndices);
				stModuleParams[i].nIndexCnt = stModuleParams[i].nVertexCnt;

				//创建 Vertex Buffer 仅使用Upload隐式堆
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE))
					, D3D12_RESOURCE_STATE_GENERIC_READ
					, nullptr
					, IID_PPV_ARGS(&stModuleParams[i].pIVertexBuffer)));
				GRS_SetD3D12DebugNameIndexed(stModuleParams[i].pIVertexBuffer.Get(), _T("pIVertexBuffer"), i);

				//使用map-memcpy-unmap大法将数据传至顶点缓冲对象
				UINT8* pVertexDataBegin = nullptr;
				CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.

				GRS_THROW_IF_FAILED(stModuleParams[i].pIVertexBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
				memcpy(pVertexDataBegin, pstVertices, stModuleParams[i].nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
				stModuleParams[i].pIVertexBuffer->Unmap(0, nullptr);

				//创建 Index Buffer 仅使用Upload隐式堆
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(stModuleParams[i].nIndexCnt * sizeof(UINT))
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

				::HeapFree(::GetProcessHeap(), 0, pstVertices);
				::HeapFree(::GetProcessHeap(), 0, pnIndices);

				// 创建常量缓冲 注意缓冲尺寸设置为256边界对齐大小
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pID3DDevice->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
					, D3D12_HEAP_FLAG_NONE
					, &CD3DX12_RESOURCE_DESC::Buffer(szMVPBuf)
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
				stGPUParams[nIDGPUMain].m_pID3DDevice->CreateConstantBufferView(&cbvDesc, stModuleParams[i].pISRVCBVHp->GetCPUDescriptorHandleForHeapStart());
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
				CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSphere(stModuleParams[i].pISRVCBVHp->GetGPUDescriptorHandleForHeapStart());
				stModuleParams[i].pIBundle->SetGraphicsRootDescriptorTable(0, stGPUCBVHandleSphere);

				stGPUCBVHandleSphere.Offset(1, stGPUParams[nIDGPUMain].m_pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
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

			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(nszVBQuad),
				D3D12_RESOURCE_STATE_COPY_DEST,
				nullptr,
				IID_PPV_ARGS(&pIVBQuad)));

			// 这次我们特意演示了如何将顶点缓冲上传到默认堆上的方式，与纹理上传默认堆实际是一样的
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateCommittedResource(
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

			UpdateSubresources<1>(stGPUParams[nIDGPUSecondary].m_pICmdList.Get(), pIVBQuad.Get(), pIVBQuadUpload.Get(), 0, 0, 1, &stVBDataQuad);
			stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pIVBQuad.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

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
				GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, hFenceEvent));
				WaitForSingleObject(hFenceEvent, INFINITE);
			}
		}

		// 创建在辅助显卡上渲染单位矩形用的SRV堆和Sample堆
		{
			D3D12_DESCRIPTOR_HEAP_DESC stHeapDesc = {};
			stHeapDesc.NumDescriptors = g_nFrameBackBufCount;
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			//SRV堆
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSRVSecondary)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDHSRVSecondary);

			// SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emRTFmt;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			CD3DX12_CPU_DESCRIPTOR_HANDLE stSrvHandle(pIDHSRVSecondary->GetCPUDescriptorHandleForHeapStart());

			for (int i = 0; i < g_nFrameBackBufCount; i++)
			{
				if (!bCrossAdapterTextureSupport)
				{
					// 使用复制的渲染目标纹理作为Shader的纹理资源
					stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateShaderResourceView(
						pISecondaryAdapterTexutrePerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				else
				{
					// 直接使用共享的渲染目标复制的纹理作为纹理资源
					stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateShaderResourceView(
						stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[i].Get()
						, &stSRVDesc
						, stSrvHandle);
				}
				stSrvHandle.Offset(1, stGPUParams[nIDGPUSecondary].m_nszSRVCBVUAV);
			}


			// 创建Sample Descriptor Heap 采样器描述符堆
			stHeapDesc.NumDescriptors = 1;  //只有一个Sample
			stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateDescriptorHeap(&stHeapDesc, IID_PPV_ARGS(&pIDHSampleSecondary)));
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

			stGPUParams[nIDGPUSecondary].m_pID3DDevice->CreateSampler(&stSamplerDesc, pIDHSampleSecondary->GetCPUDescriptorHandleForHeapStart());
		}

		// 执行第二个Copy命令并等待所有的纹理都上传到了默认堆中
		{
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdList->Close());
			ID3D12CommandList* ppRenderCommandLists[] = { stGPUParams[nIDGPUMain].m_pICmdList.Get() };
			stGPUParams[nIDGPUMain].m_pICmdQueue->ExecuteCommandLists(_countof(ppRenderCommandLists), ppRenderCommandLists);

			n64CurrentFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pICmdQueue->Signal(stGPUParams[nIDGPUMain].m_pIFence.Get(), n64CurrentFenceValue));
			n64FenceValue++;
			GRS_THROW_IF_FAILED(stGPUParams[nIDGPUMain].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, hFenceEvent));
	
		}

		GRS_THROW_IF_FAILED(pICmdListCopy->Close());
	
		// Time Value 时间轴变量
		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		// 计算旋转角度需要的变量
		double dModelRotationYAngle = 0.0f;

		XMMATRIX mxRotY;
		// 计算投影矩阵
		XMMATRIX mxProjection = XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)iWndWidth / (FLOAT)iWndHeight, 0.1f, 1000.0f);

		DWORD dwRet = 0;
		BOOL bExit = FALSE;
		//17、开始消息循环，并在其中不断渲染
		while (!bExit)
		{//消息循环，将等待超时值设置为0，同时将定时性的渲染，改成了每次循环都渲染
			dwRet = ::MsgWaitForMultipleObjects(1, &hFenceEvent, FALSE, INFINITE, QS_ALLINPUT);
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
					stGPUParams[nIDGPUMain].m_pICmdList->Reset(	stGPUParams[nIDGPUMain].m_pICmdAllocPerFrame[nCurrentFrameIndex].Get(), pIPSOMain.Get());

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

						stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_COMMON
								, D3D12_RESOURCE_STATE_RENDER_TARGET));

						CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(
							stGPUParams[nIDGPUMain].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPUParams[nIDGPUMain].m_nszRTV);
						CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
							stGPUParams[nIDGPUMain].m_pIDHDSVTex->GetCPUDescriptorHandleForHeapStart());

						stGPUParams[nIDGPUMain].m_pICmdList->OMSetRenderTargets(
							1, &rtvHandle, false, &dsvHandle);
						stGPUParams[nIDGPUMain].m_pICmdList->ClearRenderTargetView(
							rtvHandle, arf4ClearColor, 0, nullptr);
						stGPUParams[nIDGPUMain].m_pICmdList->ClearDepthStencilView(
							dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
						
						//执行实际的物体绘制渲染，Draw Call！
						for (int i = 0; i < nMaxObject; i++)
						{
							ID3D12DescriptorHeap* ppHeapsSkybox[] 
								= { stModuleParams[i].pISRVCBVHp.Get(),stModuleParams[i].pISampleHp.Get() };
							stGPUParams[nIDGPUMain].m_pICmdList->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
							stGPUParams[nIDGPUMain].m_pICmdList->ExecuteBundle(stModuleParams[i].pIBundle.Get());
						}
						
						stGPUParams[nIDGPUMain].m_pICmdList->ResourceBarrier(1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_RENDER_TARGET
								, D3D12_RESOURCE_STATE_COMMON));

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

							stGPUParams[nIDGPUMain].m_pID3DDevice->GetCopyableFootprints(&stRenderTargetDesc, 0, 1, 0, &stRenderTargetLayout, nullptr, nullptr, nullptr);

							CD3DX12_TEXTURE_COPY_LOCATION dest(stGPUParams[nIDGPUMain].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get(), stRenderTargetLayout);
							CD3DX12_TEXTURE_COPY_LOCATION src(stGPUParams[nIDGPUMain].m_pIRTRes[nCurrentFrameIndex].Get(), 0);
							CD3DX12_BOX box(0, 0, iWndWidth, iWndHeight);

							pICmdListCopy->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);
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
							D3D12_RESOURCE_BARRIER stResBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
								pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get(),
								D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
								D3D12_RESOURCE_STATE_COPY_DEST);

							stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stResBarrier);

							// Copy the shared buffer contents into the texture using the memory
							// layout prescribed by the texture.
							D3D12_RESOURCE_DESC stSecondaryAdapterTexture = pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex]->GetDesc();
							D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTextureLayout = {};

							stGPUParams[nIDGPUSecondary].m_pID3DDevice->GetCopyableFootprints(&stSecondaryAdapterTexture, 0, 1, 0, &stTextureLayout, nullptr, nullptr, nullptr);

							CD3DX12_TEXTURE_COPY_LOCATION dest(pISecondaryAdapterTexutrePerFrame[nCurrentFrameIndex].Get(), 0);
							CD3DX12_TEXTURE_COPY_LOCATION src(stGPUParams[nIDGPUSecondary].m_pICrossAdapterResPerFrame[nCurrentFrameIndex].Get(), stTextureLayout);
							CD3DX12_BOX box(0, 0, iWndWidth, iWndHeight);

							stGPUParams[nIDGPUSecondary].m_pICmdList->CopyTextureRegion(&dest, 0, 0, 0, &src, &box);

							stResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
							stResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
							stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stResBarrier);
						}

						stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootSignature(pIRSQuad.Get());
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetPipelineState(pIPSOQuad.Get());
						ID3D12DescriptorHeap* ppHeaps[] = { pIDHSRVSecondary.Get(),pIDHSampleSecondary.Get() };
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

						stGPUParams[nIDGPUSecondary].m_pICmdList->RSSetViewports(1, &stViewPort);
						stGPUParams[nIDGPUSecondary].m_pICmdList->RSSetScissorRects(1, &stScissorRect);

						D3D12_RESOURCE_BARRIER stRTSecondaryBarriers = CD3DX12_RESOURCE_BARRIER::Transition(
							stGPUParams[nIDGPUSecondary].m_pIRTRes[nCurrentFrameIndex].Get()
							, D3D12_RESOURCE_STATE_PRESENT
							, D3D12_RESOURCE_STATE_RENDER_TARGET);

						stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1, &stRTSecondaryBarriers);

						CD3DX12_CPU_DESCRIPTOR_HANDLE rtvSecondaryHandle(
							stGPUParams[nIDGPUSecondary].m_pIDHRTV->GetCPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPUParams[nIDGPUSecondary].m_nszRTV);
						stGPUParams[nIDGPUSecondary].m_pICmdList->OMSetRenderTargets(1, &rtvSecondaryHandle, false, nullptr);
						float f4ClearColor[] = { 1.0f,0.0f,0.0f,1.0f }; //故意使用与主显卡渲染目标不同的清除色，查看是否有“露底”的问题
						stGPUParams[nIDGPUSecondary].m_pICmdList->ClearRenderTargetView(rtvSecondaryHandle, f4ClearColor, 0, nullptr);

						// 开始绘制矩形
						CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(
							pIDHSRVSecondary->GetGPUDescriptorHandleForHeapStart()
							, nCurrentFrameIndex
							, stGPUParams[nIDGPUSecondary].m_nszSRVCBVUAV);
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootDescriptorTable(0, srvHandle);
						stGPUParams[nIDGPUSecondary].m_pICmdList->SetGraphicsRootDescriptorTable(1, pIDHSampleSecondary->GetGPUDescriptorHandleForHeapStart());

						stGPUParams[nIDGPUSecondary].m_pICmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
						stGPUParams[nIDGPUSecondary].m_pICmdList->IASetVertexBuffers(0, 1, &pstVBVQuad);
						// Draw Call!
						stGPUParams[nIDGPUSecondary].m_pICmdList->DrawInstanced(4, 1, 0, 0);

						// 设置好同步围栏
						stGPUParams[nIDGPUSecondary].m_pICmdList->ResourceBarrier(1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								stGPUParams[nIDGPUSecondary].m_pIRTRes[nCurrentFrameIndex].Get()
								, D3D12_RESOURCE_STATE_RENDER_TARGET
								, D3D12_RESOURCE_STATE_PRESENT));

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
					GRS_THROW_IF_FAILED(stGPUParams[nIDGPUSecondary].m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, hFenceEvent));
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

BOOL LoadMeshVertex(const CHAR*pszMeshFileName, UINT&nVertexCnt, ST_GRS_VERTEX_MODULE*&ppVertex, UINT*&ppIndices)
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

		ppVertex = (ST_GRS_VERTEX_MODULE*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(ST_GRS_VERTEX_MODULE));
		ppIndices = (UINT*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(UINT));

		for (UINT i = 0; i < nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_vPos.x >> ppVertex[i].m_vPos.y >> ppVertex[i].m_vPos.z;
			ppVertex[i].m_vPos.w = 1.0f;
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