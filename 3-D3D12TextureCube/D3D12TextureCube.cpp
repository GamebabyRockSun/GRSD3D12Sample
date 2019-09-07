#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
//添加WTL支持 方便使用COM
#include <wrl.h>
using namespace Microsoft;
using namespace Microsoft::WRL;

#include <dxgi1_6.h>
#include <DirectXMath.h>
using namespace DirectX;
//for d3d12
#include <d3d12.h>
#include <d3dcompiler.h>

//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

//for WIC
#include <wincodec.h>

#include "..\WindowsCommons\d3dx12.h"

#define GRS_WND_CLASS_NAME _T("Game Window Class")
#define GRS_WND_TITLE	_T("DirectX12 Texture Sample")

#define GRS_THROW_IF_FAILED(hr) if (FAILED(hr)){ throw CGRSCOMException(hr); }

#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))

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

struct WICTranslate
{
	GUID wic;
	DXGI_FORMAT format;
};

static WICTranslate g_WICFormats[] =
{//WIC格式与DXGI像素格式的对应表，该表中的格式为被支持的格式
	{ GUID_WICPixelFormat128bppRGBAFloat,       DXGI_FORMAT_R32G32B32A32_FLOAT },

	{ GUID_WICPixelFormat64bppRGBAHalf,         DXGI_FORMAT_R16G16B16A16_FLOAT },
	{ GUID_WICPixelFormat64bppRGBA,             DXGI_FORMAT_R16G16B16A16_UNORM },

	{ GUID_WICPixelFormat32bppRGBA,             DXGI_FORMAT_R8G8B8A8_UNORM },
	{ GUID_WICPixelFormat32bppBGRA,             DXGI_FORMAT_B8G8R8A8_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppBGR,              DXGI_FORMAT_B8G8R8X8_UNORM }, // DXGI 1.1

	{ GUID_WICPixelFormat32bppRGBA1010102XR,    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM }, // DXGI 1.1
	{ GUID_WICPixelFormat32bppRGBA1010102,      DXGI_FORMAT_R10G10B10A2_UNORM },

	{ GUID_WICPixelFormat16bppBGRA5551,         DXGI_FORMAT_B5G5R5A1_UNORM },
	{ GUID_WICPixelFormat16bppBGR565,           DXGI_FORMAT_B5G6R5_UNORM },

	{ GUID_WICPixelFormat32bppGrayFloat,        DXGI_FORMAT_R32_FLOAT },
	{ GUID_WICPixelFormat16bppGrayHalf,         DXGI_FORMAT_R16_FLOAT },
	{ GUID_WICPixelFormat16bppGray,             DXGI_FORMAT_R16_UNORM },
	{ GUID_WICPixelFormat8bppGray,              DXGI_FORMAT_R8_UNORM },

	{ GUID_WICPixelFormat8bppAlpha,             DXGI_FORMAT_A8_UNORM },
};

// WIC 像素格式转换表.
struct WICConvert
{
	GUID source;
	GUID target;
};

static WICConvert g_WICConvert[] =
{
	// 目标格式一定是最接近的被支持的格式
	{ GUID_WICPixelFormatBlackWhite,            GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat1bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat2bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat4bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat8bppIndexed,           GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat2bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM
	{ GUID_WICPixelFormat4bppGray,              GUID_WICPixelFormat8bppGray }, // DXGI_FORMAT_R8_UNORM

	{ GUID_WICPixelFormat16bppGrayFixedPoint,   GUID_WICPixelFormat16bppGrayHalf }, // DXGI_FORMAT_R16_FLOAT
	{ GUID_WICPixelFormat32bppGrayFixedPoint,   GUID_WICPixelFormat32bppGrayFloat }, // DXGI_FORMAT_R32_FLOAT

	{ GUID_WICPixelFormat16bppBGR555,           GUID_WICPixelFormat16bppBGRA5551 }, // DXGI_FORMAT_B5G5R5A1_UNORM

	{ GUID_WICPixelFormat32bppBGR101010,        GUID_WICPixelFormat32bppRGBA1010102 }, // DXGI_FORMAT_R10G10B10A2_UNORM

	{ GUID_WICPixelFormat24bppBGR,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat24bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPBGRA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat32bppPRGBA,            GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM

	{ GUID_WICPixelFormat48bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat48bppBGR,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppBGRA,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPBGRA,            GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat48bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppBGRFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppBGRAFixedPoint,   GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBFixedPoint,    GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat48bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
	{ GUID_WICPixelFormat64bppRGBHalf,          GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT

	{ GUID_WICPixelFormat128bppPRGBAFloat,      GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFloat,        GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBAFixedPoint,  GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat128bppRGBFixedPoint,   GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT
	{ GUID_WICPixelFormat32bppRGBE,             GUID_WICPixelFormat128bppRGBAFloat }, // DXGI_FORMAT_R32G32B32A32_FLOAT

	{ GUID_WICPixelFormat32bppCMYK,             GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppCMYK,             GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat40bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat80bppCMYKAlpha,        GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM

	{ GUID_WICPixelFormat32bppRGB,              GUID_WICPixelFormat32bppRGBA }, // DXGI_FORMAT_R8G8B8A8_UNORM
	{ GUID_WICPixelFormat64bppRGB,              GUID_WICPixelFormat64bppRGBA }, // DXGI_FORMAT_R16G16B16A16_UNORM
	{ GUID_WICPixelFormat64bppPRGBAHalf,        GUID_WICPixelFormat64bppRGBAHalf }, // DXGI_FORMAT_R16G16B16A16_FLOAT
};

bool GetTargetPixelFormat(const GUID* pSourceFormat, GUID* pTargetFormat)
{//查表确定兼容的最接近格式是哪个
	*pTargetFormat = *pSourceFormat;
	for (size_t i = 0; i < _countof(g_WICConvert); ++i)
	{
		if (InlineIsEqualGUID(g_WICConvert[i].source, *pSourceFormat))
		{
			*pTargetFormat = g_WICConvert[i].target;
			return true;
		}
	}
	return false;
}

DXGI_FORMAT GetDXGIFormatFromPixelFormat(const GUID* pPixelFormat)
{//查表确定最终对应的DXGI格式是哪一个
	for (size_t i = 0; i < _countof(g_WICFormats); ++i)
	{
		if (InlineIsEqualGUID(g_WICFormats[i].wic, *pPixelFormat))
		{
			return g_WICFormats[i].format;
		}
	}
	return DXGI_FORMAT_UNKNOWN;
}

struct ST_GRS_VERTEX
{
	XMFLOAT3 m_vPos;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

struct ST_GRS_FRAME_MVP_BUFFER
{
	XMFLOAT4X4 m_MVP;			//经典的Model-view-projection(MVP)矩阵.
};

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	::CoInitialize(nullptr);  //for WIC & COM

	const UINT nFrameBackBufCount = 3u;

	int iWidth = 1024;
	int iHeight = 768;
	UINT nFrameIndex = 0;
	UINT nFrame = 0;

	UINT nDXGIFactoryFlags = 0U;
	UINT nRTVDescriptorSize = 0U;

	HWND hWnd = nullptr;
	MSG	msg = {};

	float fBoxSize = 3.0f;
	float fTCMax = 3.0f;

	D3D12_VERTEX_BUFFER_VIEW stVertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW stIndexBufferView = {};

	UINT64 n64FenceValue = 0ui64;
	HANDLE hFenceEvent = nullptr;

	UINT nTextureW = 0u;
	UINT nTextureH = 0u;
	UINT nBPP = 0u;
	UINT nPicRowPitch = 0;
	UINT64 n64UploadBufferSize = 0;
	DXGI_FORMAT stTextureFormat = DXGI_FORMAT_UNKNOWN;
	UINT64 n64RequiredSize = 0u;
	UINT   nNumSubresources = 1u;  //我们只有一副图片，即子资源个数为1
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT stTxtLayouts = {};
	UINT64 n64TextureRowSizes = 0u;
	UINT   nTextureRowNum = 0u;
	D3D12_RESOURCE_DESC stDestDesc = {};
	D3D12_RESOURCE_DESC stTextureDesc = {};

	SIZE_T szMVPBuffer = GRS_UPPER_DIV(sizeof(ST_GRS_FRAME_MVP_BUFFER), 256) * 256;

	CD3DX12_VIEWPORT stViewPort(0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight));
	CD3DX12_RECT	 stScissorRect(0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight));

	ST_GRS_FRAME_MVP_BUFFER* pMVPBuffer = nullptr;

	float fAngle = XM_PI / 180;

	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIAdapter1>				pIAdapter;

	ComPtr<ID3D12Device4>				pID3DDevice;
	ComPtr<ID3D12CommandQueue>			pICommandQueue;

	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;

	ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
	ComPtr<ID3D12DescriptorHeap>		pISRVHeap;

	ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBufCount];
	ComPtr<ID3D12Heap>					pITextureHeap;
	ComPtr<ID3D12Resource>				pITexture;
	ComPtr<ID3D12Heap>					pIUploadHeap;
	ComPtr<ID3D12Resource>				pITextureUpload;
	ComPtr<ID3D12Resource>			    pICBVUpload;
	ComPtr<ID3D12Resource>				pIVertexBuffer;
	ComPtr<ID3D12Resource>				pIIndexBuffer;

	ComPtr<ID3D12CommandAllocator>		pICommandAllocator;
	ComPtr<ID3D12GraphicsCommandList>	pICommandList;

	ComPtr<ID3D12RootSignature>			pIRootSignature;
	ComPtr<ID3D12PipelineState>			pIPipelineState;
	ComPtr<ID3D12Fence>					pIFence;

	ComPtr<IWICImagingFactory>			pIWICFactory;
	ComPtr<IWICBitmapDecoder>			pIWICDecoder;
	ComPtr<IWICBitmapFrameDecode>		pIWICFrame;
	ComPtr<IWICBitmapSource>			pIBMP;

	try
	{
		//---------------------------------------------------------------------------------------------
		// 1、创建窗口
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

			RECT rtWnd = { 0, 0, iWidth, iHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			hWnd = CreateWindowW(GRS_WND_CLASS_NAME
				, GRS_WND_TITLE
				, dwWndStyle
				, CW_USEDEFAULT
				, 0
				, rtWnd.right - rtWnd.left
				, rtWnd.bottom - rtWnd.top
				, nullptr
				, nullptr
				, hInstance
				, nullptr);

			if (!hWnd)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			ShowWindow(hWnd, nCmdShow);
			UpdateWindow(hWnd);
		}

		//打开显示子系统的调试支持
		{
			//---------------------------------------------------------------------------------------------
#if defined(_DEBUG)
			{
				ComPtr<ID3D12Debug> debugController;
				if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
				{
					debugController->EnableDebugLayer();
					// 打开附加的调试支持
					nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
				}
			}
#endif
			//---------------------------------------------------------------------------------------------
		}

		//2、创建DXGI Factory对象
		{
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		}

		//---------------------------------------------------------------------------------------------
		//3、枚举适配器，通过判定是否NUMA架构的适配器来确定用独显创建设备
		
		{
			DXGI_ADAPTER_DESC1 desc = {};
			D3D12_FEATURE_DATA_ARCHITECTURE stArchitecture = {};
			for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pIDXGIFactory5->EnumAdapters1(adapterIndex, &pIAdapter); ++adapterIndex)
			{
				DXGI_ADAPTER_DESC1 desc = {};
				pIAdapter->GetDesc1(&desc);

				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{//跳过软件虚拟适配器设备
					continue;
				}

				GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3DDevice)));
				GRS_THROW_IF_FAILED(pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE
					, &stArchitecture, sizeof(D3D12_FEATURE_DATA_ARCHITECTURE)));

				if ( !stArchitecture.UMA )
				{
					break;
				}

				pID3DDevice.Reset();
			}

			//---------------------------------------------------------------------------------------------
			if ( nullptr == pID3DDevice.Get() )
			{// 可怜的机器上居然没有独显 还是先退出了事 
				throw CGRSCOMException(E_FAIL);
			}
		}
		//---------------------------------------------------------------------------------------------
		//5、创建直接命令队列
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICommandQueue)));
		}

		//---------------------------------------------------------------------------------------------
		//6、创建交换链和RTV描述符
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = nFrameBackBufCount;
			stSwapChainDesc.Width = iWidth;
			stSwapChainDesc.Height = iHeight;
			stSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				pICommandQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));

			//---------------------------------------------------------------------------------------------
			//7、得到当前后缓冲区的序号，也就是下一个将要呈送显示的缓冲区的序号
			//注意此处使用了高版本的SwapChain接口的函数
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//---------------------------------------------------------------------------------------------
			//8、创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVHeap)));
			//得到每个描述符元素的大小
			nRTVDescriptorSize = pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			//---------------------------------------------------------------------------------------------
			//9、创建RTV的描述符
			CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT i = 0; i < nFrameBackBufCount; i++)
			{
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&pIARenderTargets[i])));
				pID3DDevice->CreateRenderTargetView(pIARenderTargets[i].Get(), nullptr, stRTVHandle);
				stRTVHandle.Offset(1, nRTVDescriptorSize);
			}
		}

		//---------------------------------------------------------------------------------------------
		//10、创建SRV CBV堆 (Shader Resource View Heap)
		{
			D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
			stSRVHeapDesc.NumDescriptors = 2; //1 SRV + 1 CBV
			stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&stSRVHeapDesc, IID_PPV_ARGS(&pISRVHeap)));
		}

		//---------------------------------------------------------------------------------------------
		//11、创建根签名
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
			// 检测是否支持V1.1版本的根签名
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}
			// 在GPU上执行SetGraphicsRootDescriptorTable后，我们不修改命令列表中的SRV，因此我们可以使用默认Rang行为:
			// D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
			CD3DX12_DESCRIPTOR_RANGE1 stDSPRanges[2];
			stDSPRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
			stDSPRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

			CD3DX12_ROOT_PARAMETER1 stRootParameters[2];
			stRootParameters[0].InitAsDescriptorTable(1, &stDSPRanges[0], D3D12_SHADER_VISIBILITY_PIXEL); //SRV仅PS可见
			stRootParameters[1].InitAsDescriptorTable(1, &stDSPRanges[1], D3D12_SHADER_VISIBILITY_ALL); //CBV是所有Shader可见

			D3D12_STATIC_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			stSamplerDesc.MipLODBias = 0;
			stSamplerDesc.MaxAnisotropy = 0;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			stSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			stSamplerDesc.MinLOD = 0.0f;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.ShaderRegister = 0;
			stSamplerDesc.RegisterSpace = 0;
			stSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc;
			stRootSignatureDesc.Init_1_1(_countof(stRootParameters)
				, stRootParameters
				, 1
				, &stSamplerDesc
				, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> pISignatureBlob;
			ComPtr<ID3DBlob> pIErrorBlob;
			GRS_THROW_IF_FAILED(D3DX12SerializeVersionedRootSignature(&stRootSignatureDesc
				, stFeatureData.HighestVersion
				, &pISignatureBlob
				, &pIErrorBlob));
			GRS_THROW_IF_FAILED(pID3DDevice->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRootSignature)));
		}


		//---------------------------------------------------------------------------------------------
		// 12、编译Shader创建渲染管线状态对象
		{
			ComPtr<ID3DBlob> pIBlobVertexShader;
			ComPtr<ID3DBlob> pIBlobPixelShader;
#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif
			//编译为行矩阵形式	   
			compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[] = _T("D:\\Projects_2018_08\\D3D12 Tutorials\\3-D3D12TextureCube\\Shader\\TextureCube.hlsl");

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "VSMain", "vs_5_0", compileFlags, 0, &pIBlobVertexShader, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "PSMain", "ps_5_0", compileFlags, 0, &pIBlobPixelShader, nullptr));

			// Define the vertex input layout.
			D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// 创建 graphics pipeline state object (PSO)对象
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.InputLayout = { stInputElementDescs, _countof(stInputElementDescs) };
			stPSODesc.pRootSignature = pIRootSignature.Get();
			stPSODesc.VS = CD3DX12_SHADER_BYTECODE(pIBlobVertexShader.Get());
			stPSODesc.PS = CD3DX12_SHADER_BYTECODE(pIBlobPixelShader.Get());
			stPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			stPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			stPSODesc.DepthStencilState.DepthEnable = FALSE;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
			stPSODesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pID3DDevice->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPipelineState)));
		}

		//创建命令列表
		{
			//---------------------------------------------------------------------------------------------
			// 13、创建命令列表分配器
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICommandAllocator)));

			//---------------------------------------------------------------------------------------------
			// 14、创建图形命令列表
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICommandAllocator.Get(), pIPipelineState.Get(), IID_PPV_ARGS(&pICommandList)));
		}


		//---------------------------------------------------------------------------------------------
		// 15、创建顶点缓冲
		// 定义正方形的3D数据结构
		ST_GRS_VERTEX stTriangleVertices[] = {
			{ {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f* fTCMax}, {0.0f,  0.0f, -1.0f} },
			{ {1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f* fTCMax, 0.0f* fTCMax},  {0.0f,  0.0f, -1.0f} },
			{ {-1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f* fTCMax, 1.0f* fTCMax}, {0.0f,  0.0f, -1.0f} },
			{ {-1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax}, {0.0f,  0.0f, -1.0f} },
			{ {1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f, 0.0f, -1.0f} },
			{ {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  0.0f, -1.0f} },
			{ {1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
			{ {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
			{ {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
			{ {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
			{ {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
			{ {1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {1.0f,  0.0f,  0.0f} },
			{ {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
			{ {-1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
			{ {1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax}, {0.0f,  0.0f,  1.0f} },
			{ {1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
			{ {-1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
			{ {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  0.0f,  1.0f} },
			{ {-1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
			{ {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
			{ {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
			{ {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
			{ {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
			{ {-1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax}, {-1.0f,  0.0f,  0.0f} },
			{ {-1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
			{ {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
			{ {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
			{ {-1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
			{ {1.0f * fBoxSize,  1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f,  1.0f,  0.0f} },
			{ {1.0f * fBoxSize,  1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {0.0f,  1.0f,  0.0f }},
			{ {-1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {0.0f * fTCMax, 0.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
			{ {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
			{ {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
			{ {-1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {0.0f * fTCMax, 1.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
			{ {1.0f * fBoxSize, -1.0f * fBoxSize, -1.0f * fBoxSize}, {1.0f * fTCMax, 0.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
			{ {1.0f * fBoxSize, -1.0f * fBoxSize,  1.0f * fBoxSize}, {1.0f * fTCMax, 1.0f * fTCMax},  {0.0f, -1.0f,  0.0f} },
		};

		const UINT nVertexBufferSize = sizeof(stTriangleVertices);

		UINT32 pBoxIndices[] //立方体索引数组
			= {
			0,1,2,
			3,4,5,

			6,7,8,
			9,10,11,

			12,13,14,
			15,16,17,

			18,19,20,
			21,22,23,

			24,25,26,
			27,28,29,

			30,31,32,
			33,34,35,
		};

		const UINT nszIndexBuffer = sizeof(pBoxIndices);

		//创建顶点和索引缓冲
		{
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(nVertexBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIVertexBuffer)));

			UINT8* pVertexDataBegin = nullptr;

			CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.
			GRS_THROW_IF_FAILED(pIVertexBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, stTriangleVertices, sizeof(stTriangleVertices));
			pIVertexBuffer->Unmap(0, nullptr);

			stVertexBufferView.BufferLocation = pIVertexBuffer->GetGPUVirtualAddress();
			stVertexBufferView.StrideInBytes = sizeof(ST_GRS_VERTEX);
			stVertexBufferView.SizeInBytes = nVertexBufferSize;

			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(nszIndexBuffer),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIIndexBuffer)));

			UINT8* pIndexDataBegin = nullptr;
			GRS_THROW_IF_FAILED(pIIndexBuffer->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, pBoxIndices, nszIndexBuffer);
			pIIndexBuffer->Unmap(0, nullptr);

			stIndexBufferView.BufferLocation = pIIndexBuffer->GetGPUVirtualAddress();
			stIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
			stIndexBufferView.SizeInBytes = nszIndexBuffer;
		}

		//---------------------------------------------------------------------------------------------
		// 16、使用WIC创建并加载一个2D纹理
		{
			//使用纯COM方式创建WIC类厂对象，也是调用WIC第一步要做的事情
			GRS_THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIWICFactory)));

			//使用WIC类厂对象接口加载纹理图片，并得到一个WIC解码器对象接口，图片信息就在这个接口代表的对象中了
			WCHAR* pszTexcuteFileName = _T("D:\\Projects_2018_08\\D3D12 Tutorials\\3-D3D12TextureCube\\Texture\\bear.jpg");
			GRS_THROW_IF_FAILED(pIWICFactory->CreateDecoderFromFilename(
				pszTexcuteFileName,              // 文件名
				NULL,                            // 不指定解码器，使用默认
				GENERIC_READ,                    // 访问权限
				WICDecodeMetadataCacheOnDemand,  // 若需要就缓冲数据 
				&pIWICDecoder                    // 解码器对象
			));

			// 获取第一帧图片(因为GIF等格式文件可能会有多帧图片，其他的格式一般只有一帧图片)
			// 实际解析出来的往往是位图格式数据
			GRS_THROW_IF_FAILED(pIWICDecoder->GetFrame(0, &pIWICFrame));

			WICPixelFormatGUID wpf = {};
			//获取WIC图片格式
			GRS_THROW_IF_FAILED(pIWICFrame->GetPixelFormat(&wpf));
			GUID tgFormat = {};

			//通过第一道转换之后获取DXGI的等价格式
			if (GetTargetPixelFormat(&wpf, &tgFormat))
			{
				stTextureFormat = GetDXGIFormatFromPixelFormat(&tgFormat);
			}

			if (DXGI_FORMAT_UNKNOWN == stTextureFormat)
			{// 不支持的图片格式 目前退出了事 
			 // 一般 在实际的引擎当中都会提供纹理格式转换工具，
			 // 图片都需要提前转换好，所以不会出现不支持的现象
				throw CGRSCOMException(S_FALSE);
			}


			if (!InlineIsEqualGUID(wpf, tgFormat))
			{// 这个判断很重要，如果原WIC格式不是直接能转换为DXGI格式的图片时
			 // 我们需要做的就是转换图片格式为能够直接对应DXGI格式的形式
				//创建图片格式转换器
				ComPtr<IWICFormatConverter> pIConverter;
				GRS_THROW_IF_FAILED(pIWICFactory->CreateFormatConverter(&pIConverter));

				//初始化一个图片转换器，实际也就是将图片数据进行了格式转换
				GRS_THROW_IF_FAILED(pIConverter->Initialize(
					pIWICFrame.Get(),                // 输入原图片数据
					tgFormat,						 // 指定待转换的目标格式
					WICBitmapDitherTypeNone,         // 指定位图是否有调色板，现代都是真彩位图，不用调色板，所以为None
					NULL,                            // 指定调色板指针
					0.f,                             // 指定Alpha阀值
					WICBitmapPaletteTypeCustom       // 调色板类型，实际没有使用，所以指定为Custom
				));
				// 调用QueryInterface方法获得对象的位图数据源接口
				GRS_THROW_IF_FAILED(pIConverter.As(&pIBMP));
			}
			else
			{
				//图片数据格式不需要转换，直接获取其位图数据源接口
				GRS_THROW_IF_FAILED(pIWICFrame.As(&pIBMP));
			}
			//获得图片大小（单位：像素）
			GRS_THROW_IF_FAILED(pIBMP->GetSize(&nTextureW, &nTextureH));

			//获取图片像素的位大小的BPP（Bits Per Pixel）信息，用以计算图片行数据的真实大小（单位：字节）
			ComPtr<IWICComponentInfo> pIWICmntinfo;
			GRS_THROW_IF_FAILED(pIWICFactory->CreateComponentInfo(tgFormat, pIWICmntinfo.GetAddressOf()));

			WICComponentType type;
			GRS_THROW_IF_FAILED(pIWICmntinfo->GetComponentType(&type));

			if (type != WICPixelFormat)
			{
				throw CGRSCOMException(S_FALSE);
			}

			ComPtr<IWICPixelFormatInfo> pIWICPixelinfo;
			GRS_THROW_IF_FAILED(pIWICmntinfo.As(&pIWICPixelinfo));

			// 到这里终于可以得到BPP了，这也是我看的比较吐血的地方，为了BPP居然饶了这么多环节
			GRS_THROW_IF_FAILED(pIWICPixelinfo->GetBitsPerPixel(&nBPP));

			// 计算图片实际的行大小（单位：字节），这里使用了一个上取整除法即（A+B-1）/B ，
			// 这曾经被传说是微软的面试题,希望你已经对它了如指掌
			nPicRowPitch = (uint64_t(nTextureW) * uint64_t(nBPP) + 7u) / 8u;
		}

		//---------------------------------------------------------------------------------------------
		//
		{
			stTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stTextureDesc.MipLevels = 1;
			stTextureDesc.Format = stTextureFormat; //DXGI_FORMAT_R8G8B8A8_UNORM;
			stTextureDesc.Width = nTextureW;
			stTextureDesc.Height = nTextureH;
			stTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			stTextureDesc.DepthOrArraySize = 1;
			stTextureDesc.SampleDesc.Count = 1;
			stTextureDesc.SampleDesc.Quality = 0;


			//创建默认堆上的资源，类型是Texture2D，GPU对默认堆资源的访问速度是最快的
			//因为纹理资源一般是不易变的资源，所以我们通常使用上传堆复制到默认堆中
			//在传统的D3D11及以前的D3D接口中，这些过程都被封装了，我们只能指定创建时的类型为默认堆 
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
				, D3D12_HEAP_FLAG_NONE
				, &stTextureDesc				//可以使用CD3DX12_RESOURCE_DESC::Tex2D来简化结构体的初始化
				, D3D12_RESOURCE_STATE_COPY_DEST
				, nullptr
				, IID_PPV_ARGS(&pITexture)));

			//获取上传堆资源缓冲的大小，这个尺寸通常大于实际图片的尺寸
			n64UploadBufferSize = GetRequiredIntermediateSize(pITexture.Get(), 0, 1);

			// 创建用于上传纹理的资源,注意其类型是Buffer
			// 上传堆对于GPU访问来说性能是很差的，
			// 所以对于几乎不变的数据尤其像纹理都是
			// 通过它来上传至GPU访问更高效的默认堆中
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(n64UploadBufferSize),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pITextureUpload)));

			//按照资源缓冲大小来分配实际图片数据存储的内存大小
			void* pbPicData = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, n64UploadBufferSize);
			if (nullptr == pbPicData)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			//从图片中读取出数据
			GRS_THROW_IF_FAILED(pIBMP->CopyPixels(nullptr
				, nPicRowPitch
				, static_cast<UINT>(nPicRowPitch * nTextureH)   //注意这里才是图片数据真实的大小，这个值通常小于缓冲的大小
				, reinterpret_cast<BYTE*>(pbPicData)));

			//{//下面这段代码来自DX12的示例，直接通过填充缓冲绘制了一个黑白方格的纹理
			// //还原这段代码，然后注释上面的CopyPixels调用可以看到黑白方格纹理的效果
			//	const UINT rowPitch = nPicRowPitch; //nTextureW * 4; //static_cast<UINT>(n64UploadBufferSize / nTextureH);
			//	const UINT cellPitch = rowPitch >> 3;		// The width of a cell in the checkboard texture.
			//	const UINT cellHeight = nTextureW >> 3;	// The height of a cell in the checkerboard texture.
			//	const UINT textureSize = static_cast<UINT>(n64UploadBufferSize);
			//	UINT nTexturePixelSize = static_cast<UINT>(n64UploadBufferSize / nTextureH / nTextureW);

			//	UINT8* pData = reinterpret_cast<UINT8*>(pbPicData);

			//	for (UINT n = 0; n < textureSize; n += nTexturePixelSize)
			//	{
			//		UINT x = n % rowPitch;
			//		UINT y = n / rowPitch;
			//		UINT i = x / cellPitch;
			//		UINT j = y / cellHeight;

			//		if (i % 2 == j % 2)
			//		{
			//			pData[n] = 0x00;		// R
			//			pData[n + 1] = 0x00;	// G
			//			pData[n + 2] = 0x00;	// B
			//			pData[n + 3] = 0xff;	// A
			//		}
			//		else
			//		{
			//			pData[n] = 0xff;		// R
			//			pData[n + 1] = 0xff;	// G
			//			pData[n + 2] = 0xff;	// B
			//			pData[n + 3] = 0xff;	// A
			//		}
			//	}
			//}

			//获取向上传堆拷贝纹理数据的一些纹理转换尺寸信息
			//对于复杂的DDS纹理这是非常必要的过程
			stDestDesc = pITexture->GetDesc();

			pID3DDevice->GetCopyableFootprints(&stDestDesc
				, 0
				, nNumSubresources
				, 0
				, &stTxtLayouts
				, &nTextureRowNum
				, &n64TextureRowSizes
				, &n64RequiredSize);

			//因为上传堆实际就是CPU传递数据到GPU的中介
			//所以我们可以使用熟悉的Map方法将它先映射到CPU内存地址中
			//然后我们按行将数据复制到上传堆中
			//需要注意的是之所以按行拷贝是因为GPU资源的行大小
			//与实际图片的行大小是有差异的,二者的内存边界对齐要求是不一样的
			BYTE* pData = nullptr;
			GRS_THROW_IF_FAILED(pITextureUpload->Map(0, NULL, reinterpret_cast<void**>(&pData)));

			BYTE* pDestSlice = reinterpret_cast<BYTE*>(pData) + stTxtLayouts.Offset;
			const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(pbPicData);
			for (UINT y = 0; y < nTextureRowNum; ++y)
			{
				memcpy(pDestSlice + static_cast<SIZE_T>(stTxtLayouts.Footprint.RowPitch) * y
					, pSrcSlice + static_cast<SIZE_T>(nPicRowPitch) * y
					, nPicRowPitch);
			}
			//取消映射 对于易变的数据如每帧的变换矩阵等数据，可以撒懒不用Unmap了，
			//让它常驻内存,以提高整体性能，因为每次Map和Unmap是很耗时的操作
			//因为现在起码都是64位系统和应用了，地址空间是足够的，被长期占用不会影响什么
			pITextureUpload->Unmap(0, NULL);

			//释放图片数据，做一个干净的程序员
			::HeapFree(::GetProcessHeap(), 0, pbPicData);
		}

		//向命令队列发出从上传堆复制纹理数据到默认堆的命令
		{
			CD3DX12_TEXTURE_COPY_LOCATION Dst(pITexture.Get(), 0);
			CD3DX12_TEXTURE_COPY_LOCATION Src(pITextureUpload.Get(), stTxtLayouts);
			pICommandList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);

			//设置一个资源屏障，同步并确认复制操作完成
			//直接使用结构体然后调用的形式
			D3D12_RESOURCE_BARRIER stResBar = {};
			stResBar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stResBar.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			stResBar.Transition.pResource = pITexture.Get();
			stResBar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			stResBar.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			stResBar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			pICommandList->ResourceBarrier(1, &stResBar);

			//或者使用D3DX12库中的工具类调用的等价形式，下面的方式更简洁一些
			//pICommandList->ResourceBarrier(1
			//	, &CD3DX12_RESOURCE_BARRIER::Transition(pITexture.Get()
			//	, D3D12_RESOURCE_STATE_COPY_DEST
			//	, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			//);
					// 创建常量缓冲 注意缓冲尺寸设置为256边界对齐大小
			GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(szMVPBuffer),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pICBVUpload)));

			// Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
			GRS_THROW_IF_FAILED(pICBVUpload->Map(0, nullptr, reinterpret_cast<void**>(&pMVPBuffer)));

		}

		//---------------------------------------------------------------------------------------------
		// 创建SRV描述符
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = stTextureDesc.Format;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;

			pID3DDevice->CreateShaderResourceView(pITexture.Get(), &stSRVDesc, pISRVHeap->GetCPUDescriptorHandleForHeapStart());

			//创建CBV描述符
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = pICBVUpload->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuffer);

			CD3DX12_CPU_DESCRIPTOR_HANDLE cbvSrvHandle(pISRVHeap->GetCPUDescriptorHandleForHeapStart()
				, 1
				, pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

			pID3DDevice->CreateConstantBufferView(&cbvDesc, cbvSrvHandle);
		}
		//---------------------------------------------------------------------------------------------
		// 执行命令列表并等待纹理资源上传完成，这一步是必须的
		{
			GRS_THROW_IF_FAILED(pICommandList->Close());
			ID3D12CommandList* ppCommandLists[] = { pICommandList.Get() };
			pICommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

			//---------------------------------------------------------------------------------------------
			// 17、创建一个同步对象——围栏，用于等待渲染完成，因为现在Draw Call是异步的了
			GRS_THROW_IF_FAILED(pID3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence)));
			n64FenceValue = 1;

			//---------------------------------------------------------------------------------------------
			// 18、创建一个Event同步对象，用于等待围栏事件通知
			hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hFenceEvent == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			//---------------------------------------------------------------------------------------------
			// 19、等待纹理资源正式复制完成先
			const UINT64 fence = n64FenceValue;
			GRS_THROW_IF_FAILED(pICommandQueue->Signal(pIFence.Get(), fence));
			n64FenceValue++;

			//---------------------------------------------------------------------------------------------
			// 看命令有没有真正执行到围栏标记的这里，没有就利用事件去等待，注意使用的是命令队列对象的指针
			if (pIFence->GetCompletedValue() < fence)
			{
				GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(fence, hFenceEvent));
				WaitForSingleObject(hFenceEvent, INFINITE);
			}

			//---------------------------------------------------------------------------------------------
			//命令分配器先Reset一下，刚才已经执行过了一个复制纹理的命令
			GRS_THROW_IF_FAILED(pICommandAllocator->Reset());
			//Reset命令列表，并重新指定命令分配器和PSO对象
			GRS_THROW_IF_FAILED(pICommandList->Reset(pICommandAllocator.Get(), pIPipelineState.Get()));
		}
		//---------------------------------------------------------------------------------------------


		//---------------------------------------------------------------------------------------------
		//20、创建定时器对象，以便于创建高效的消息循环
		HANDLE phWait = CreateWaitableTimer(NULL, FALSE, NULL);
		LARGE_INTEGER liDueTime = {};
		liDueTime.QuadPart = -1i64;//1秒后开始计时
		SetWaitableTimer(phWait, &liDueTime, 1, NULL, NULL, 0);//40ms的周期

		//---------------------------------------------------------------------------------------------
		//21、开始消息循环，并在其中不断渲染
		DWORD dwRet = 0;
		BOOL bExit = FALSE;
		while (!bExit)
		{
			dwRet = ::MsgWaitForMultipleObjects(1, &phWait, FALSE, INFINITE, QS_ALLINPUT);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			case WAIT_TIMEOUT:
			{//计时器时间到
				//GRS_TRACE(_T("开始第%u帧渲染{Frame Index = %u}：\n"),nFrame,nFrameIndex);
				//开始记录命令
				//---------------------------------------------------------------------------------------------
				// 准备一个简单的旋转MVP矩阵 让方块转起来
				fAngle += fAngle / 100;
				if (fAngle >= XM_2PI)
				{
					fAngle = XM_PI / 180;
				}

				//模型矩阵 model
				XMMATRIX xmRot = XMMatrixRotationY(fAngle);

				//视矩阵 view
				XMVECTOR Eye = XMVectorSet(0.0f, 5.0f, -10.0f, 0.0f); //眼睛位置
				XMVECTOR At = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);  //眼睛所盯的位置
				XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  //头部正上方位置
				XMMATRIX xmMVP = XMMatrixMultiply(xmRot, XMMatrixLookAtLH(Eye, At, Up));
				//投影矩阵 projection
				xmMVP = XMMatrixMultiply(xmMVP, (XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)4.0f / (FLOAT)3.0f, 0.1f, 1000.0f)));

				XMStoreFloat4x4(&pMVPBuffer->m_MVP, xmMVP);
				//---------------------------------------------------------------------------------------------


				pICommandList->SetGraphicsRootSignature(pIRootSignature.Get());
				ID3D12DescriptorHeap* ppHeaps[] = { pISRVHeap.Get() };
				pICommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
				pICommandList->SetGraphicsRootDescriptorTable(0, pISRVHeap->GetGPUDescriptorHandleForHeapStart());

				CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandle(pISRVHeap->GetGPUDescriptorHandleForHeapStart()
					, 1
					, pID3DDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

				pICommandList->SetGraphicsRootDescriptorTable(1, stGPUCBVHandle);

				pICommandList->RSSetViewports(1, &stViewPort);
				pICommandList->RSSetScissorRects(1, &stScissorRect);

				//---------------------------------------------------------------------------------------------
				// 通过资源屏障判定后缓冲已经切换完毕可以开始渲染了
				pICommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pIARenderTargets[nFrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

				CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart(), nFrameIndex, nRTVDescriptorSize);
				//设置渲染目标
				pICommandList->OMSetRenderTargets(1, &stRTVHandle, FALSE, nullptr);

				// 继续记录命令，并真正开始新一帧的渲染
				const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
				pICommandList->ClearRenderTargetView(stRTVHandle, clearColor, 0, nullptr);

				pICommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				pICommandList->IASetVertexBuffers(0, 1, &stVertexBufferView);
				pICommandList->IASetIndexBuffer(&stIndexBufferView);

				//---------------------------------------------------------------------------------------------
				//Draw Call！！！
				pICommandList->DrawIndexedInstanced(_countof(pBoxIndices), 1, 0, 0, 0);
				//pICommandList->DrawInstanced(_countof(stTriangleVertices), 1, 0, 0);

				//---------------------------------------------------------------------------------------------
				//又一个资源屏障，用于确定渲染已经结束可以提交画面去显示了
				pICommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pIARenderTargets[nFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
				//关闭命令列表，可以去执行了
				GRS_THROW_IF_FAILED(pICommandList->Close());

				//---------------------------------------------------------------------------------------------
				//执行命令列表
				ID3D12CommandList* ppCommandLists[] = { pICommandList.Get() };
				pICommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

				//---------------------------------------------------------------------------------------------
				//提交画面
				GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

				//---------------------------------------------------------------------------------------------
				//开始同步GPU与CPU的执行，先记录围栏标记值
				const UINT64 fence = n64FenceValue;
				GRS_THROW_IF_FAILED(pICommandQueue->Signal(pIFence.Get(), fence));
				n64FenceValue++;

				//---------------------------------------------------------------------------------------------
				// 看命令有没有真正执行到围栏标记的这里，没有就利用事件去等待，注意使用的是命令队列对象的指针
				if (pIFence->GetCompletedValue() < fence)
				{
					GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(fence, hFenceEvent));
					WaitForSingleObject(hFenceEvent, INFINITE);
				}
				//执行到这里说明一个命令队列完整的执行完了，在这里就代表我们的一帧已经渲染完了，接着准备执行下一帧渲染

				//---------------------------------------------------------------------------------------------
				//获取新的后缓冲序号，因为Present真正完成时后缓冲的序号就更新了
				nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

				//---------------------------------------------------------------------------------------------
				//命令分配器先Reset一下
				GRS_THROW_IF_FAILED(pICommandAllocator->Reset());
				//Reset命令列表，并重新指定命令分配器和PSO对象
				GRS_THROW_IF_FAILED(pICommandList->Reset(pICommandAllocator.Get(), pIPipelineState.Get()));

				//GRS_TRACE(_T("第%u帧渲染结束.\n"), nFrame++);
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
			default:
				break;
			}
		}
		pICBVUpload->Unmap(0, nullptr);
	}
	catch (CGRSCOMException& e)
	{//发生了COM异常
		e;
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
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

