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
{
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

//
// WICPixelFormat nearest conversion table.
//
struct WICConvert
{
	GUID source;
	GUID target;
};

static WICConvert g_WICConvert[] =
{
	//
	// Note target GUID in this conversion table must be one of those directly supported formats (above).
	//

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
{
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
{
	for (size_t i = 0; i < _countof(g_WICFormats); ++i)
	{
		if (InlineIsEqualGUID(g_WICFormats[i].wic, *pPixelFormat))
		{
			return g_WICFormats[i].format;
		}
	}

	return DXGI_FORMAT_UNKNOWN;
}

struct GRS_VERTEX
{
	XMFLOAT3 m_vPos;		//Position
	XMFLOAT2 m_vTxc;		//Texcoord
};

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	const UINT nFrameBackBufCount = 3u;

	int iWidth = 1024;
	int iHeight = 768;
	UINT nFrameIndex = 0;
	UINT nFrame = 0;

	UINT nDXGIFactoryFlags = 0U;
	UINT nRTVDescriptorSize = 0U;

	HWND hWnd = nullptr;
	MSG	msg = {};

	float fAspectRatio = 3.0f;

	D3D12_VERTEX_BUFFER_VIEW stVertexBufferView = {};

	UINT64 n64FenceValue = 0ui64;
	HANDLE hFenceEvent = nullptr;

	UINT nTextureW = 0u;
	UINT nTextureH = 0u;
	DXGI_FORMAT stTextureFormat = DXGI_FORMAT_UNKNOWN;

	CD3DX12_VIEWPORT stViewPort(0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight));
	CD3DX12_RECT	 stScissorRect(0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight));

	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIAdapter1>				pIAdapter;
	ComPtr<ID3D12Device4>				pID3DDevice;
	ComPtr<ID3D12CommandQueue>			pICommandQueue;
	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;
	ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
	ComPtr<ID3D12DescriptorHeap>		pISRVHeap;
	ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBufCount];
	ComPtr<ID3D12Resource>				pITexcute;
	ComPtr<ID3D12CommandAllocator>		pICommandAllocator;
	ComPtr<ID3D12RootSignature>			pIRootSignature;
	ComPtr<ID3D12PipelineState>			pIPipelineState;
	ComPtr<ID3D12GraphicsCommandList>	pICommandList;
	ComPtr<ID3D12Resource>				pIVertexBuffer;
	ComPtr<ID3D12Fence>					pIFence;

	ComPtr<IWICImagingFactory>			pIWICFactory;
	ComPtr<IWICBitmapDecoder>			pIWICDecoder;
	ComPtr<IWICBitmapFrameDecode>		pIWICFrame;

	try
	{
		::CoInitialize(nullptr);  //for WIC & COM
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
			return FALSE;
		}

		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		//---------------------------------------------------------------------------------------------
		//1、创建WIC类厂对象
		GRS_THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIWICFactory)));

		//加载纹理图片		
		WCHAR* pszTexcuteFileName = _T("D:\\Projects_2018_08\\D3D12 Tutorials\\2-D3D12WICTexture\\Texture\\bear.jpg");
		GRS_THROW_IF_FAILED(pIWICFactory->CreateDecoderFromFilename(
			pszTexcuteFileName,              // 文件名
			NULL,                            // 不指定解码器，使用默认
			GENERIC_READ,                    // 访问权限
			WICDecodeMetadataCacheOnDemand,  // 若需要就缓冲数据 
			&pIWICDecoder                    // 解码器对象
		));

		// 获取第一帧图片
		GRS_THROW_IF_FAILED(pIWICDecoder->GetFrame(0, &pIWICFrame));
		//获得图片大小
		GRS_THROW_IF_FAILED(pIWICFrame->GetSize(&nTextureW, &nTextureH));

		WICPixelFormatGUID wpf = {};
		//获取图片格式
		GRS_THROW_IF_FAILED(pIWICFrame->GetPixelFormat(&wpf));
		GUID tgFormat = {};

		//转换为DXGI的等价格式
		if (GetTargetPixelFormat(&wpf, &tgFormat))
		{
			stTextureFormat = GetDXGIFormatFromPixelFormat(&tgFormat);
		}

		if (DXGI_FORMAT_UNKNOWN == stTextureFormat)
		{
			throw CGRSCOMException(S_FALSE);
		}

		//---------------------------------------------------------------------------------------------
#if defined(_DEBUG)
		{//打开显示子系统的调试支持
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
		//2、创建DXGI Factory对象
		GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
		// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
		GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

		//---------------------------------------------------------------------------------------------
		//3、枚举适配器，并选择合适的适配器来创建3D设备对象
		for (UINT adapterIndex = 1; DXGI_ERROR_NOT_FOUND != pIDXGIFactory5->EnumAdapters1(adapterIndex, &pIAdapter); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc = {};
			pIAdapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{//跳过软件虚拟适配器设备
				continue;
			}
			//检查适配器对D3D支持的兼容级别，这里直接要求支持12.1的能力，注意返回接口的那个参数被置为了nullptr，这样
			//就不会实际创建一个设备了，也不用我们啰嗦的再调用release来释放接口。这也是一个重要的技巧，请记住！
			if (SUCCEEDED(D3D12CreateDevice(pIAdapter.Get(), D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
		//---------------------------------------------------------------------------------------------
		//4、创建D3D12.1的设备
		GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3DDevice)));

		//---------------------------------------------------------------------------------------------
		//5、创建直接命令队列
		D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
		stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICommandQueue)));

		//---------------------------------------------------------------------------------------------
		//6、创建交换链
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

		//---------------------------------------------------------------------------------------------
		//10、创建SRV堆 (Shader Resource View Heap)
		D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
		stSRVHeapDesc.NumDescriptors = 1;
		stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		GRS_THROW_IF_FAILED(pID3DDevice->CreateDescriptorHeap(&stSRVHeapDesc, IID_PPV_ARGS(&pISRVHeap)));

		//---------------------------------------------------------------------------------------------
		//11、创建根描述符
		D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
		// 检测是否支持V1.1版本的根签名
		stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(pID3DDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
		{
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}
		// 在GPU上执行SetGraphicsRootDescriptorTable后，我们不修改命令列表中的SRV，因此我们可以使用默认Rang行为:
		// D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
		CD3DX12_DESCRIPTOR_RANGE1 stDSPRanges[1];
		stDSPRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 stRootParameters[1];
		stRootParameters[0].InitAsDescriptorTable(1, &stDSPRanges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC stSamplerDesc = {};
		stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
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
		stRootSignatureDesc.Init_1_1(_countof(stRootParameters), stRootParameters
			, 1, &stSamplerDesc
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

		//---------------------------------------------------------------------------------------------
		// 12、编译Shader创建渲染管线状态对象
		ComPtr<ID3DBlob> pIBlobVertexShader;
		ComPtr<ID3DBlob> pIBlobPixelShader;
#if defined(_DEBUG)
		// Enable better shader debugging with the graphics debugging tools.
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		TCHAR pszShaderFileName[] = _T("D:\\Projects_2018_08\\D3D12 Tutorials\\2-D3D12WICTexture\\Shader\\Texture.hlsl");
		GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &pIBlobVertexShader, nullptr));
		GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pIBlobPixelShader, nullptr));

		// Define the vertex input layout.
		D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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

		GRS_THROW_IF_FAILED(pID3DDevice->CreateGraphicsPipelineState(&stPSODesc, IID_PPV_ARGS(&pIPipelineState)));

		//---------------------------------------------------------------------------------------------
		// 13、创建命令列表分配器
		GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pICommandAllocator)));

		//---------------------------------------------------------------------------------------------
		// 14、创建图形命令列表
		GRS_THROW_IF_FAILED(pID3DDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pICommandAllocator.Get(), pIPipelineState.Get(), IID_PPV_ARGS(&pICommandList)));

		//---------------------------------------------------------------------------------------------
		// 15、创建顶点缓冲
		// 定义正方形的3D数据结构
		GRS_VERTEX stTriangleVertices[] =
		{
			{ { -0.25f* fAspectRatio, -0.25f * fAspectRatio, 0.0f}, { 0.0f, 1.0f } },	// Bottom left.
			{ { -0.25f* fAspectRatio, 0.25f * fAspectRatio, 0.0f}, { 0.0f, 0.0f } },	// Top left.
			{ { 0.25f* fAspectRatio, -0.25f * fAspectRatio, 0.0f }, { 1.0f, 1.0f } },	// Bottom right.
			{ { 0.25f* fAspectRatio, 0.25f * fAspectRatio, 0.0f}, { 1.0f, 0.0f } },		// Top right.
		};

		const UINT nVertexBufferSize = sizeof(stTriangleVertices);
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
		stVertexBufferView.StrideInBytes = sizeof(GRS_VERTEX);
		stVertexBufferView.SizeInBytes = nVertexBufferSize;

		//---------------------------------------------------------------------------------------------
		// 16、创建并加装一个2D纹理
		ComPtr<ID3D12Resource> pITextureUpload;

		D3D12_RESOURCE_DESC stTextureDesc = {};
		stTextureDesc.MipLevels = 1;
		stTextureDesc.Format = stTextureFormat;
		stTextureDesc.Width = nTextureW;
		stTextureDesc.Height = nTextureH;
		stTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		stTextureDesc.DepthOrArraySize = 1;
		stTextureDesc.SampleDesc.Count = 1;
		stTextureDesc.SampleDesc.Quality = 0;
		stTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		
		GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&stTextureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&pITexcute)));

		const UINT64 n64UploadBufferSize = GetRequiredIntermediateSize(pITexcute.Get(), 0, 1);
		void* pbTextureData = ::HeapAlloc(::GetProcessHeap(), 0, n64UploadBufferSize);
		if (nullptr == pbTextureData)
		{
			throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
		}

		//GRS_THROW_IF_FAILED(pIWICFrame->CopyPixels(nullptr
		//	, static_cast<UINT>(n64UploadBufferSize / nTextureH )
		//	, static_cast<UINT>(n64UploadBufferSize)
		//	, reinterpret_cast<BYTE*>(pbTextureData)));
		{
			const UINT rowPitch = static_cast<UINT>(n64UploadBufferSize / nTextureH);
			const UINT cellPitch = rowPitch >> 3;		// The width of a cell in the checkboard texture.
			const UINT cellHeight = nTextureW >> 3;	// The height of a cell in the checkerboard texture.
			const UINT textureSize = static_cast<UINT>(n64UploadBufferSize);
			UINT nTexturePixelSize = static_cast<UINT>(n64UploadBufferSize / nTextureH / nTextureH);

			UINT8* pData = reinterpret_cast<UINT8*>(pbTextureData);

			for (UINT n = 0; n < textureSize; n += nTexturePixelSize)
			{
				UINT x = n % rowPitch;
				UINT y = n / rowPitch;
				UINT i = x / cellPitch;
				UINT j = y / cellHeight;

				if (i % 2 == j % 2)
				{
					pData[n] = 0x00;		// R
					pData[n + 1] = 0x00;	// G
					pData[n + 2] = 0x00;	// B
					pData[n + 3] = 0xff;	// A
				}
				else
				{
					pData[n] = 0xff;		// R
					pData[n + 1] = 0xff;	// G
					pData[n + 2] = 0xff;	// B
					pData[n + 3] = 0xff;	// A
				}
			}
		}

		// 创建用于上传纹理的资源
		GRS_THROW_IF_FAILED(pID3DDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(n64UploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&pITextureUpload)));

		D3D12_SUBRESOURCE_DATA stTextureData = {};
		stTextureData.pData = pbTextureData;
		stTextureData.RowPitch = n64UploadBufferSize / nTextureH;
		stTextureData.SlicePitch = n64UploadBufferSize;

		UpdateSubresources(pICommandList.Get(), pITexcute.Get(), pITextureUpload.Get(), 0, 0, 1, &stTextureData);
		pICommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pITexcute.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		//---------------------------------------------------------------------------------------------
		// 最终创建SRV描述符
		D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
		stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		stSRVDesc.Format = stTextureDesc.Format;
		stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		stSRVDesc.Texture2D.MipLevels = 1;
		pID3DDevice->CreateShaderResourceView(pITexcute.Get(), &stSRVDesc, pISRVHeap->GetCPUDescriptorHandleForHeapStart());

		//---------------------------------------------------------------------------------------------
		// 执行并等待纹理资源上传完成
		GRS_THROW_IF_FAILED(pICommandList->Close());
		ID3D12CommandList* ppCommandLists[] = { pICommandList.Get() };
		pICommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

		//---------------------------------------------------------------------------------------------
		// 14、创建一个同步对象——围栏，用于等待渲染完成，因为现在Draw Call是异步的了
		GRS_THROW_IF_FAILED(pID3DDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence)));
		n64FenceValue = 1;

		//---------------------------------------------------------------------------------------------
		// 15、创建一个Event同步对象，用于等待围栏事件通知
		hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (hFenceEvent == nullptr)
		{
			GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
		}

		//---------------------------------------------------------------------------------------------
		//等待纹理资源正式上传完成先
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
		//命令分配器先Reset一下
		GRS_THROW_IF_FAILED(pICommandAllocator->Reset());
		//Reset命令列表，并重新指定命令分配器和PSO对象
		GRS_THROW_IF_FAILED(pICommandList->Reset(pICommandAllocator.Get(), pIPipelineState.Get()));

		//---------------------------------------------------------------------------------------------
		::HeapFree(::GetProcessHeap(), 0, pbTextureData);

		//---------------------------------------------------------------------------------------------
		//16、创建定时器对象，以便于创建高效的消息循环
		HANDLE phWait = CreateWaitableTimer(NULL, FALSE, NULL);
		LARGE_INTEGER liDueTime = {};
		liDueTime.QuadPart = -1i64;//1秒后开始计时
		SetWaitableTimer(phWait, &liDueTime, 1, NULL, NULL, 0);//40ms的周期

		//---------------------------------------------------------------------------------------------
		//17、开始消息循环，并在其中不断渲染
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
				pICommandList->SetGraphicsRootSignature(pIRootSignature.Get());
				ID3D12DescriptorHeap* ppHeaps[] = { pISRVHeap.Get() };
				pICommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
				pICommandList->SetGraphicsRootDescriptorTable(0, pISRVHeap->GetGPUDescriptorHandleForHeapStart());
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
				pICommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				pICommandList->IASetVertexBuffers(0, 1, &stVertexBufferView);

				//---------------------------------------------------------------------------------------------
				//Draw Call！！！
				pICommandList->DrawInstanced(_countof(stTriangleVertices), 1, 0, 0);

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
		//::CoUninitialize();
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