#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <fstream>  //for ifstream
#include <strsafe.h>
#include <atlconv.h> //for T2A
#include <atlcoll.h>
#include <wrl.h> //添加WTL支持 方便使用COM
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3d12.h>//for d3d12
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#include <wincodec.h> //for WIC

#include "..\WindowsCommons\DDSTextureLoader12.h"

using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#ifndef GRS_BLOCK

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 SkyBox Sample")

#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); }}

//新定义的宏用于上取整除法
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))

//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

// 内存分配的宏定义
#define GRS_ALLOC(sz)		::HeapAlloc(GetProcessHeap(),0,(sz))
#define GRS_CALLOC(sz)		::HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(sz))
#define GRS_CREALLOC(p,sz)	::HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(p),(sz))
#define GRS_SAFE_FREE(p)		if( nullptr != (p) ){ ::HeapFree( ::GetProcessHeap(),0,(p) ); (p) = nullptr; }

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



#endif // !GRS_BLOCK

struct ST_GRS_VERTEX
{//这次我们额外加入了每个顶点的法线，但Shader中还暂时没有用
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTex;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

struct ST_GRS_SKYBOX_VERTEX
{//天空盒子的顶点结构
public:
	XMFLOAT4 m_v4Position;
public:
	ST_GRS_SKYBOX_VERTEX() :m_v4Position() {}
	ST_GRS_SKYBOX_VERTEX(float x, float y, float z)
		:m_v4Position(x, y, z, 1.0f)
	{}
	ST_GRS_SKYBOX_VERTEX& operator = (const ST_GRS_SKYBOX_VERTEX& vt)
	{
		m_v4Position = vt.m_v4Position;
	}
};

struct ST_GRS_FRAME_MVP_BUFFER
{
	XMFLOAT4X4 m_MVP;
	XMFLOAT4X4 m_mWorld;
	XMFLOAT4   m_v4EyePos;
};

UINT g_nCurrentSamplerNO = 1; //当前使用的采样器索引 ，这里默认使用第一个
UINT g_nSampleMaxCnt = 5;		//创建五个典型的采样器

//初始的默认摄像机的位置
XMFLOAT3 g_f3EyePos = XMFLOAT3(0.0f, 2.0f, -10.0f); //眼睛位置
XMFLOAT3 g_f3LockAt = XMFLOAT3(0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
XMFLOAT3 g_f3HeapUp = XMFLOAT3(0.0f, 1.0f, 0.0f);    //头部正上方位置

float g_fYaw = 0.0f;			// 绕正Z轴的旋转量.
float g_fPitch = 0.0f;			// 绕XZ平面的旋转量

double g_fPalstance = 10.0f * XM_PI / 180.0f;	//物体旋转的角速度，单位：弧度/秒

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	::CoInitialize(nullptr);  //for WIC & COM

	const UINT							nFrameBackBufCount = 3u;
	int									iWndWidth = 1024;
	int									iWndHeight = 768;
	UINT								nCurrentFrameIndex = 0;
	UINT								nDXGIFactoryFlags = 0U;
	

	HWND								hWnd = nullptr;
	MSG									msg = {};
	TCHAR								pszAppPath[MAX_PATH] = {};

	ST_GRS_FRAME_MVP_BUFFER*			pMVPBufEarth = nullptr;

	ST_GRS_FRAME_MVP_BUFFER*			pMVPBufSkybox = nullptr;
	//常量缓冲区大小上对齐到256Bytes边界
	SIZE_T								szMVPBuf = GRS_UPPER(sizeof(ST_GRS_FRAME_MVP_BUFFER), 256);

	float								fSphereSize = 3.0f;

	D3D12_VERTEX_BUFFER_VIEW			stVBVEarth = {};
	D3D12_INDEX_BUFFER_VIEW				stIBVEarth = {};

	D3D12_VERTEX_BUFFER_VIEW			stVBVSkybox = {};
	D3D12_INDEX_BUFFER_VIEW				stIBVSkybox = {};

	UINT64								n64FenceValue = 0ui64;
	HANDLE								hEventFence = nullptr;

	UINT								nTxtWEarth = 0u;
	UINT								nTxtHEarth = 0u;
	UINT								nTxtWSkybox = 0u;
	UINT								nTxtHSkybox = 0u;
	UINT								nBPPEarth = 0u;
	UINT								nBPPSkybox = 0u;
	UINT								nRowPitchEarth = 0;
	UINT								nRowPitchSkybox = 0;
	UINT64								n64szUploadBufEarth = 0;
	UINT64								n64szUploadBufSkybox = 0;

	DXGI_FORMAT							emRTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT							emDSFormat = DXGI_FORMAT_D32_FLOAT;
	DXGI_FORMAT							emTxtFmtEarth = DXGI_FORMAT_UNKNOWN;
	const float							faClearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT	stTxtLayoutsEarth = {};
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT	stTxtLayoutsSkybox = {};
	D3D12_RESOURCE_DESC					stTextureDesc = {};
	D3D12_RESOURCE_DESC					stDestDesc = {};

	UINT								nRTVDescriptorSize = 0;
	UINT								nSRVDescriptorSize = 0;
	UINT								nSamplerDescriptorSize = 0; //采样器大小
	

	D3D12_VIEWPORT						stViewPort = { 0.0f, 0.0f, static_cast<float>(iWndWidth), static_cast<float>(iWndHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D12_RECT							stScissorRect = { 0, 0, static_cast<LONG>(iWndWidth), static_cast<LONG>(iWndHeight) };

	//球体的网格数据
	ST_GRS_VERTEX*						pstSphereVertices = nullptr;
	UINT								nSphereVertexCnt = 0;
	UINT*								pSphereIndices = nullptr;
	UINT								nSphereIndexCnt = 0;

	//Sky Box的网格数据
	UINT								nSkyboxIndexCnt = 0;

	//======================================================================================================
	//加载Skybox的Cube Map需要的变量
	std::unique_ptr<uint8_t[]>			ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> arSubResources;
	DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
	bool								bIsCube = false;
	//======================================================================================================

	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIAdapter1>				pIAdapter1;

	ComPtr<ID3D12Device4>				pID3D12Device4;
	ComPtr<ID3D12CommandQueue>			pICMDQueue;

	ComPtr<ID3D12CommandAllocator>		pICmdAllocDirect;
	ComPtr<ID3D12CommandAllocator>		pICmdAllocSkybox;
	ComPtr<ID3D12CommandAllocator>		pICmdAllocEarth;
	ComPtr<ID3D12GraphicsCommandList>	pICmdListDirect;
	ComPtr<ID3D12GraphicsCommandList>   pIBundlesSkybox;
	ComPtr<ID3D12GraphicsCommandList>   pIBundlesEarth;

	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;
	ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBufCount];
	ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
	ComPtr<ID3D12DescriptorHeap>		pIDSVHeap;			//深度缓冲描述符堆
	ComPtr<ID3D12Resource>				pIDepthStencilBuffer; //深度蜡板缓冲区

	ComPtr<ID3D12Heap>					pIRESHeapEarth;
	ComPtr<ID3D12Heap>					pIUploadHeapEarth;
	ComPtr<ID3D12Heap>					pIRESHeapSkybox;
	ComPtr<ID3D12Heap>					pIUploadHeapSkybox;

	ComPtr<ID3D12Resource>				pITextureEarth;
	ComPtr<ID3D12Resource>				pITextureUploadEarth;
	ComPtr<ID3D12Resource>			    pICBUploadEarth;
	ComPtr<ID3D12Resource>				pIVBEarth;
	ComPtr<ID3D12Resource>				pIIBEarth;

	ComPtr<ID3D12Resource>				pITextureSkybox;
	ComPtr<ID3D12Resource>				pITextureUploadSkybox;
	ComPtr<ID3D12Resource>			    pICBUploadSkybox;
	ComPtr<ID3D12Resource>				pIVBSkybox;

	ComPtr<ID3D12DescriptorHeap>		pISRVHpEarth;
	ComPtr<ID3D12DescriptorHeap>		pISampleHpEarth;
	ComPtr<ID3D12DescriptorHeap>		pISRVHpSkybox;
	ComPtr<ID3D12DescriptorHeap>		pISampleHpSkybox;

	ComPtr<ID3D12Fence>					pIFence;

	ComPtr<ID3D12RootSignature>			pIRootSignature;
	ComPtr<ID3D12PipelineState>			pIPSOEarth;
	ComPtr<ID3D12PipelineState>			pIPSOSkyBox;

	ComPtr<IWICImagingFactory>			pIWICFactory;
	ComPtr<IWICBitmapDecoder>			pIWICDecoder;
	ComPtr<IWICBitmapFrameDecode>		pIWICFrame;
	ComPtr<IWICBitmapSource>			pIBMPEarth;

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

			hWnd = CreateWindowW(GRS_WND_CLASS_NAME
				, GRS_WND_TITLE
				, dwWndStyle
				, posX
				, posY
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
		}

		// 使用WIC加载图片，并转换为DXGI兼容的格式
		{
			ComPtr<IWICFormatConverter> pIConverter;
			ComPtr<IWICComponentInfo> pIWICmntinfo;
			WICPixelFormatGUID wpf = {};
			GUID tgFormat = {};
			WICComponentType type;
			ComPtr<IWICPixelFormatInfo> pIWICPixelinfo;

			//---------------------------------------------------------------------------------------------
			//使用纯COM方式创建WIC类厂对象，也是调用WIC第一步要做的事情
			GRS_THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pIWICFactory)));

			//使用WIC类厂对象接口加载纹理图片，并得到一个WIC解码器对象接口，图片信息就在这个接口代表的对象中了
			WCHAR pszTexcuteFileName[MAX_PATH] = {};
			StringCchPrintfW(pszTexcuteFileName, MAX_PATH, _T("%sAssets\\金星.jpg"), pszAppPath);

			GRS_THROW_IF_FAILED(pIWICFactory->CreateDecoderFromFilename(
				pszTexcuteFileName,              // 文件名
				nullptr,                            // 不指定解码器，使用默认
				GENERIC_READ,                    // 访问权限
				WICDecodeMetadataCacheOnDemand,  // 若需要就缓冲数据 
				&pIWICDecoder                    // 解码器对象
			));
			// 获取第一帧图片(因为GIF等格式文件可能会有多帧图片，其他的格式一般只有一帧图片)
			// 实际解析出来的往往是位图格式数据
			GRS_THROW_IF_FAILED(pIWICDecoder->GetFrame(0, &pIWICFrame));
			//获取WIC图片格式
			GRS_THROW_IF_FAILED(pIWICFrame->GetPixelFormat(&wpf));
			//通过第一道转换之后获取DXGI的等价格式
			if (GetTargetPixelFormat(&wpf, &tgFormat))
			{
				emTxtFmtEarth = GetDXGIFormatFromPixelFormat(&tgFormat);
			}

			if (DXGI_FORMAT_UNKNOWN == emTxtFmtEarth)
			{// 不支持的图片格式 目前退出了事 
			 // 一般 在实际的引擎当中都会提供纹理格式转换工具，
			 // 图片都需要提前转换好，所以不会出现不支持的现象
				throw CGRSCOMException(S_FALSE);
			}

			if (!InlineIsEqualGUID(wpf, tgFormat))
			{// 这个判断很重要，如果原WIC格式不是直接能转换为DXGI格式的图片时
			 // 我们需要做的就是转换图片格式为能够直接对应DXGI格式的形式
				//创建图片格式转换器
				GRS_THROW_IF_FAILED(pIWICFactory->CreateFormatConverter(&pIConverter));
				//初始化一个图片转换器，实际也就是将图片数据进行了格式转换
				GRS_THROW_IF_FAILED(pIConverter->Initialize(
					pIWICFrame.Get(),                // 输入原图片数据
					tgFormat,						 // 指定待转换的目标格式
					WICBitmapDitherTypeNone,         // 指定位图是否有调色板，现代都是真彩位图，不用调色板，所以为None
					nullptr,                            // 指定调色板指针
					0.f,                             // 指定Alpha阀值
					WICBitmapPaletteTypeCustom       // 调色板类型，实际没有使用，所以指定为Custom
				));
				// 调用QueryInterface方法获得对象的位图数据源接口
				GRS_THROW_IF_FAILED(pIConverter.As(&pIBMPEarth));
			}
			else
			{
				//图片数据格式不需要转换，直接获取其位图数据源接口
				GRS_THROW_IF_FAILED(pIWICFrame.As(&pIBMPEarth));
			}
			//获得图片大小（单位：像素）
			GRS_THROW_IF_FAILED(pIBMPEarth->GetSize(&nTxtWEarth, &nTxtHEarth));
			//获取图片像素的位大小的BPP（Bits Per Pixel）信息，用以计算图片行数据的真实大小（单位：字节）
			GRS_THROW_IF_FAILED(pIWICFactory->CreateComponentInfo(tgFormat, pIWICmntinfo.GetAddressOf()));
			GRS_THROW_IF_FAILED(pIWICmntinfo->GetComponentType(&type));
			if (type != WICPixelFormat)
			{
				throw CGRSCOMException(S_FALSE);
			}
			GRS_THROW_IF_FAILED(pIWICmntinfo.As(&pIWICPixelinfo));
			// 到这里终于可以得到BPP了，这也是我看的比较吐血的地方，为了BPP居然饶了这么多环节
			GRS_THROW_IF_FAILED(pIWICPixelinfo->GetBitsPerPixel(&nBPPEarth));
			// 计算图片实际的行大小（单位：字节），这里使用了一个上取整除法即（A+B-1）/B ，
			// 这曾经被传说是微软的面试题,希望你已经对它了如指掌
			nRowPitchEarth = GRS_UPPER_DIV(uint64_t(nTxtWEarth) * uint64_t(nBPPEarth), 8);
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
			
		}

		// 枚举适配器创建设备
		{//选择NUMA架构的独显来创建3D设备对象,暂时先不支持集显了，当然你可以修改这些行为
			DXGI_ADAPTER_DESC1 stAdapterDesc1 = {};
			D3D12_FEATURE_DATA_ARCHITECTURE stArchitecture = {};
			for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pIDXGIFactory5->EnumAdapters1(adapterIndex, &pIAdapter1); ++adapterIndex)
			{
				pIAdapter1->GetDesc1(&stAdapterDesc1);

				if (stAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{//跳过软件虚拟适配器设备
					continue;
				}

				GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter1.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3D12Device4)));
				GRS_THROW_IF_FAILED(pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE
					, &stArchitecture, sizeof(D3D12_FEATURE_DATA_ARCHITECTURE)));

				if (!stArchitecture.UMA)
				{
					break;
				}

				pID3D12Device4.Reset();
			}

			//---------------------------------------------------------------------------------------------
			if (nullptr == pID3D12Device4.Get())
			{// 可怜的机器上居然没有独显 还是先退出了事 
				throw CGRSCOMException(E_FAIL);
			}

			TCHAR pszWndTitle[MAX_PATH] = {};
			GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc1));
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s (GPU:%s)")
				, pszWndTitle
				, stAdapterDesc1.Description);
			::SetWindowText(hWnd, pszWndTitle);
			
			nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			nSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			nSamplerDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
		}

		// 创建直接命令队列
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICMDQueue)));
		}

		// 创建直接命令列表、捆绑包
		{
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICmdAllocDirect)));
			//创建直接命令列表，在其上可以执行几乎所有的引擎命令（3D图形引擎、计算引擎、复制引擎等）
			//注意初始时并没有使用PSO对象，此时其实这个命令列表依然可以记录命令
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICmdAllocDirect.Get(), nullptr, IID_PPV_ARGS(&pICmdListDirect)));

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE
				, IID_PPV_ARGS(&pICmdAllocEarth)));
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE
				, pICmdAllocEarth.Get(), nullptr, IID_PPV_ARGS(&pIBundlesEarth)));

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE
				, IID_PPV_ARGS(&pICmdAllocSkybox)));
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE
				, pICmdAllocSkybox.Get(), nullptr, IID_PPV_ARGS(&pIBundlesSkybox)));
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
				pICMDQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));

			//注意此处使用了高版本的SwapChain接口的函数
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVHeap)));
			
			//---------------------------------------------------------------------------------------------
			D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = { pIRTVHeap->GetCPUDescriptorHandleForHeapStart() };
			for (UINT i = 0; i < nFrameBackBufCount; i++)
			{//这个循环暴漏了描述符堆实际上是个数组的本质
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&pIARenderTargets[i])));
				pID3D12Device4->CreateRenderTargetView(pIARenderTargets[i].Get(), nullptr, stRTVHandle);
				stRTVHandle.ptr += nRTVDescriptorSize;
			}

			// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		}

		// 创建深度缓冲及深度缓冲描述符堆
		{
			D3D12_HEAP_PROPERTIES stDSBufHeapDesc = {};
			
			stDSBufHeapDesc.Type = D3D12_HEAP_TYPE_DEFAULT;
			stDSBufHeapDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			stDSBufHeapDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			stDSBufHeapDesc.CreationNodeMask = 0;
			stDSBufHeapDesc.VisibleNodeMask = 0;

			D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
			stDepthStencilDesc.Format = emDSFormat;
			stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

			D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
			depthOptimizedClearValue.Format = emDSFormat;
			depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			depthOptimizedClearValue.DepthStencil.Stencil = 0;

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
				& stDSBufHeapDesc
				, D3D12_HEAP_FLAG_NONE
				, &stDSResDesc
				, D3D12_RESOURCE_STATE_DEPTH_WRITE
				, &depthOptimizedClearValue
				, IID_PPV_ARGS(&pIDepthStencilBuffer)
			));

			//==============================================================================================
			D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
			dsvHeapDesc.NumDescriptors = 1;
			dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&pIDSVHeap)));

			pID3D12Device4->CreateDepthStencilView(pIDepthStencilBuffer.Get()
				, &stDepthStencilDesc
				, pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
		}

		// 创建 SRV CBV Sample堆
		{
			//我们将纹理视图描述符和CBV描述符放在一个描述符堆上
			D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
			stSRVHeapDesc.NumDescriptors = 2; //1 SRV + 1 CBV
			stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc, IID_PPV_ARGS(&pISRVHpEarth)));

			D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
			stSamplerHeapDesc.NumDescriptors = g_nSampleMaxCnt;
			stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
			stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stSamplerHeapDesc, IID_PPV_ARGS(&pISampleHpEarth)));


			//===================================================================================================
			//Skybox 的 SRV CBV Sample 堆
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc, IID_PPV_ARGS(&pISRVHpSkybox)));
			stSamplerHeapDesc.NumDescriptors = 1; //天空盒子就一个采样器
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stSamplerHeapDesc, IID_PPV_ARGS(&pISampleHpSkybox)));

			
		}

		// 创建根签名
		{//这个例子中，球体和Skybox使用相同的根签名，因为渲染过程中需要的参数是一样的
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
			// 检测是否支持V1.1版本的根签名
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{// 1.0版 直接丢异常退出了
				GRS_THROW_IF_FAILED(E_NOTIMPL);
			}
			// 在GPU上执行SetGraphicsRootDescriptorTable后，我们不修改命令列表中的SRV，因此我们可以使用默认Rang行为:
			// D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
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
				, IID_PPV_ARGS(&pIRootSignature)));

		}

		// 编译Shader创建渲染管线状态对象
		{

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT nCompileFlags = 0;
#endif
			ComPtr<ID3DBlob>					pIVSEarth;
			ComPtr<ID3DBlob>					pIPSEarth;

			//编译为行矩阵形式	   
			nCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s5-SkyBox\\Shader\\TextureCube.hlsl"), pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "VSMain", "vs_5_0", nCompileFlags, 0, &pIVSEarth, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
				, "PSMain", "ps_5_0", nCompileFlags, 0, &pIPSEarth, nullptr));

			// 我们多添加了一个法线的定义，但目前Shader中我们并没有使用
			D3D12_INPUT_ELEMENT_DESC stIALayoutEarth[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// 创建 graphics pipeline state object (PSO)对象
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.InputLayout = { stIALayoutEarth, _countof(stIALayoutEarth) };
			stPSODesc.pRootSignature = pIRootSignature.Get();
			stPSODesc.VS.BytecodeLength = pIVSEarth->GetBufferSize();
			stPSODesc.VS.pShaderBytecode = pIVSEarth->GetBufferPointer();
			stPSODesc.PS.BytecodeLength = pIPSEarth->GetBufferSize();
			stPSODesc.PS.pShaderBytecode = pIPSEarth->GetBufferPointer();

			stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
			stPSODesc.BlendState.IndependentBlendEnable = FALSE;
			stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRTFormat;
			stPSODesc.DSVFormat = emDSFormat;
			stPSODesc.DepthStencilState.DepthEnable = TRUE;
			stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//启用深度缓存写入功能
			stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //深度测试函数（该值为普通的深度测试）
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPSOEarth)));

			//编译为行矩阵形式	   
			nCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszSMFileSkybox[MAX_PATH] = {};
			StringCchPrintf(pszSMFileSkybox, MAX_PATH, _T("%s5-SkyBox\\Shader\\SkyBox.hlsl"), pszAppPath);

			ComPtr<ID3DBlob>					pIVSSkybox;
			ComPtr<ID3DBlob>					pIPSSkybox;

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszSMFileSkybox, nullptr, nullptr
				, "SkyboxVS", "vs_5_0", nCompileFlags, 0, &pIVSSkybox, nullptr));
			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszSMFileSkybox, nullptr, nullptr
				, "SkyboxPS", "ps_5_0", nCompileFlags, 0, &pIPSSkybox, nullptr));

			// 天空盒子只有顶点只有位置参数
			D3D12_INPUT_ELEMENT_DESC stIALayoutSkybox[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// 创建Skybox的(PSO)对象 
			stPSODesc.InputLayout = { stIALayoutSkybox, _countof(stIALayoutSkybox) };
			//stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
			stPSODesc.DepthStencilState.DepthEnable = FALSE;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;
			stPSODesc.VS.BytecodeLength = pIVSSkybox->GetBufferSize();
			stPSODesc.VS.pShaderBytecode = pIVSSkybox->GetBufferPointer();
			stPSODesc.PS.BytecodeLength = pIPSSkybox->GetBufferSize();
			stPSODesc.PS.pShaderBytecode = pIPSSkybox->GetBufferPointer();

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc
				, IID_PPV_ARGS(&pIPSOSkyBox)));
		}

		// 创建纹理的默认堆、上传堆并加载纹理
		{
			D3D12_HEAP_DESC stTextureHeapDesc = {};
			//为堆指定纹理图片至少2倍大小的空间，这里没有详细去计算了，只是指定了一个足够大的空间，够放纹理就行
			//实际应用中也是要综合考虑分配堆的大小，以便可以重用堆
			stTextureHeapDesc.SizeInBytes = GRS_UPPER(2 * nRowPitchEarth * nTxtHEarth, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
			//指定堆的对齐方式，这里使用了默认的64K边界对齐，因为我们暂时不需要MSAA支持
			stTextureHeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			stTextureHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;		//默认堆类型
			stTextureHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			stTextureHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			//拒绝渲染目标纹理、拒绝深度蜡板纹理，实际就只是用来摆放普通纹理
			stTextureHeapDesc.Flags = D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_BUFFERS;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateHeap(&stTextureHeapDesc, IID_PPV_ARGS(&pIRESHeapEarth)));

			// 创建2D纹理		
			stTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stTextureDesc.MipLevels = 1;
			stTextureDesc.Format = emTxtFmtEarth; //DXGI_FORMAT_R8G8B8A8_UNORM;
			stTextureDesc.Width = nTxtWEarth;
			stTextureDesc.Height = nTxtHEarth;
			stTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			stTextureDesc.DepthOrArraySize = 1;
			stTextureDesc.SampleDesc.Count = 1;
			stTextureDesc.SampleDesc.Quality = 0;
			//-----------------------------------------------------------------------------------------------------------
			//使用“定位方式”来创建纹理，注意下面这个调用内部实际已经没有存储分配和释放的实际操作了，所以性能很高
			//同时可以在这个堆上反复调用CreatePlacedResource来创建不同的纹理，当然前提是它们不在被使用的时候，才考虑
			//重用堆
			GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(
				pIRESHeapEarth.Get()
				, 0
				, &stTextureDesc				//可以使用CD3DX12_RESOURCE_DESC::Tex2D来简化结构体的初始化
				, D3D12_RESOURCE_STATE_COPY_DEST
				, nullptr
				, IID_PPV_ARGS(&pITextureEarth)));
			//-----------------------------------------------------------------------------------------------------------
			//获取上传堆资源缓冲的大小，这个尺寸通常大于实际图片的尺寸
			D3D12_RESOURCE_DESC stCopyDstDesc = pITextureEarth->GetDesc();
			pID3D12Device4->GetCopyableFootprints(&stCopyDstDesc, 0, 1, 0, nullptr, nullptr, nullptr, &n64szUploadBufEarth);

			//-----------------------------------------------------------------------------------------------------------
			// 创建上传堆
			D3D12_HEAP_DESC stUploadHeapDesc = {  };
			//尺寸依然是实际纹理数据大小的2倍并64K边界对齐大小
			stUploadHeapDesc.SizeInBytes = GRS_UPPER(2 * n64szUploadBufEarth, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
			//注意上传堆肯定是Buffer类型，可以不指定对齐方式，其默认是64k边界对齐
			stUploadHeapDesc.Alignment = 0;
			stUploadHeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;		//上传堆类型
			stUploadHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			stUploadHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			//上传堆就是缓冲，可以摆放任意数据
			stUploadHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateHeap(&stUploadHeapDesc, IID_PPV_ARGS(&pIUploadHeapEarth)));

			//-----------------------------------------------------------------------------------------------------------
			// 使用“定位方式”创建用于上传纹理数据的缓冲资源
			D3D12_RESOURCE_DESC stUploadBufDesc = {};
			stUploadBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stUploadBufDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			stUploadBufDesc.Width = n64szUploadBufEarth;
			stUploadBufDesc.Height = 1;
			stUploadBufDesc.DepthOrArraySize = 1;
			stUploadBufDesc.MipLevels = 1;
			stUploadBufDesc.Format = DXGI_FORMAT_UNKNOWN;
			stUploadBufDesc.SampleDesc.Count = 1;
			stUploadBufDesc.SampleDesc.Quality = 0;
			stUploadBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stUploadBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(pIUploadHeapEarth.Get()
				, 0
				, &stUploadBufDesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pITextureUploadEarth)));

			// 加载图片数据至上传堆，即完成第一个Copy动作，从memcpy函数可知这是由CPU完成的

			//按照资源缓冲大小来分配实际图片数据存储的内存大小
			void* pbPicData = GRS_CALLOC(n64szUploadBufEarth);
			if (nullptr == pbPicData)
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			//从图片中读取出数据
			GRS_THROW_IF_FAILED(pIBMPEarth->CopyPixels(nullptr
				, nRowPitchEarth
				, static_cast<UINT>(nRowPitchEarth * nTxtHEarth)   //注意这里才是图片数据真实的大小，这个值通常小于缓冲的大小
				, reinterpret_cast<BYTE*>(pbPicData)));

			//{//下面这段代码来自DX12的示例，直接通过填充缓冲绘制了一个黑白方格的纹理
			// //还原这段代码，然后注释上面的CopyPixels调用可以看到黑白方格纹理的效果
			//	const UINT rowPitch = nRowPitchEarth; //nTxtWEarth * 4; //static_cast<UINT>(n64szUploadBufEarth / nTxtHEarth);
			//	const UINT cellPitch = rowPitch >> 3;		// The width of a cell in the checkboard texture.
			//	const UINT cellHeight = nTxtWEarth >> 3;	// The height of a cell in the checkerboard texture.
			//	const UINT textureSize = static_cast<UINT>(n64szUploadBufEarth);
			//	UINT nTexturePixelSize = static_cast<UINT>(n64szUploadBufEarth / nTxtHEarth / nTxtWEarth);

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

			UINT   nNumSubresources = 1u;  //我们只有一副图片，即子资源个数为1
			UINT   nTextureRowNum = 0u;
			UINT64 n64TextureRowSizes = 0u;
			UINT64 n64RequiredSize = 0u;

			stDestDesc = pITextureEarth->GetDesc();

			pID3D12Device4->GetCopyableFootprints(&stDestDesc
				, 0
				, nNumSubresources
				, 0
				, &stTxtLayoutsEarth
				, &nTextureRowNum
				, &n64TextureRowSizes
				, &n64RequiredSize);

			//因为上传堆实际就是CPU传递数据到GPU的中介
			//所以我们可以使用熟悉的Map方法将它先映射到CPU内存地址中
			//然后我们按行将数据复制到上传堆中
			//需要注意的是之所以按行拷贝是因为GPU资源的行大小
			//与实际图片的行大小是有差异的,二者的内存边界对齐要求是不一样的
			BYTE* pData = nullptr;
			GRS_THROW_IF_FAILED(pITextureUploadEarth->Map(0, nullptr, reinterpret_cast<void**>(&pData)));

			BYTE* pDestSlice = reinterpret_cast<BYTE*>(pData) + stTxtLayoutsEarth.Offset;
			BYTE* pSrcSlice = reinterpret_cast<BYTE*>(pbPicData);
			for (UINT y = 0; y < nTextureRowNum; ++y)
			{
				memcpy(pDestSlice + static_cast<SIZE_T>(stTxtLayoutsEarth.Footprint.RowPitch)* y
					, pSrcSlice + static_cast<SIZE_T>(nRowPitchEarth)* y
					, nRowPitchEarth);
			}
			//取消映射 对于易变的数据如每帧的变换矩阵等数据，可以撒懒不用Unmap了，
			//让它常驻内存,以提高整体性能，因为每次Map和Unmap是很耗时的操作
			//因为现在起码都是64位系统和应用了，地址空间是足够的，被长期占用不会影响什么
			pITextureUploadEarth->Unmap(0, nullptr);

			//释放图片数据，做一个干净的程序员
			GRS_SAFE_FREE(pbPicData);
		}

		// 使用DDSLoader辅助函数加载Skybox的纹理
		{
			TCHAR pszSkyboxTextureFile[MAX_PATH] = {};
			//使用“Assets\\sky_cube.dds”时需要XMMATRIX(1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1)乘以xmView
			StringCchPrintf(pszSkyboxTextureFile, MAX_PATH, _T("%sAssets\\Sky_cube_1024.dds"), pszAppPath);

			ID3D12Resource* pIResSkyBox = nullptr;
			GRS_THROW_IF_FAILED(LoadDDSTextureFromFile(
				pID3D12Device4.Get()
				, pszSkyboxTextureFile
				, &pIResSkyBox
				, ddsData
				, arSubResources
				, SIZE_MAX
				, &emAlphaMode
				, &bIsCube));

			//上面函数加载的纹理在隐式默认堆上，两个Copy动作需要我们自己完成
			pITextureSkybox.Attach(pIResSkyBox);
			//=====================================================================================================
			//获取Skybox的资源大小，并创建上传堆
			D3D12_RESOURCE_DESC stCopyDstDesc = pITextureSkybox->GetDesc();
			// 这里是第一次调用GetCopyableFootprints
			pID3D12Device4->GetCopyableFootprints(&stCopyDstDesc
				, 0, static_cast<UINT>(arSubResources.size()), 0, nullptr, nullptr, nullptr, &n64szUploadBufSkybox);
			
			D3D12_HEAP_DESC stUploadHeapDesc = {  };
			//尺寸依然是实际纹理数据大小的2倍并64K边界对齐大小
			stUploadHeapDesc.SizeInBytes = GRS_UPPER(2 * n64szUploadBufSkybox, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
			//注意上传堆肯定是Buffer类型，可以不指定对齐方式，其默认是64k边界对齐
			stUploadHeapDesc.Alignment = 0;
			stUploadHeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;		//上传堆类型
			stUploadHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			stUploadHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			//上传堆就是缓冲，可以摆放任意数据
			stUploadHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateHeap(&stUploadHeapDesc, IID_PPV_ARGS(&pIUploadHeapSkybox)));

			D3D12_RESOURCE_DESC stUploadBufDesc = {};
			stUploadBufDesc.Dimension			= D3D12_RESOURCE_DIMENSION_BUFFER;
			stUploadBufDesc.Alignment			= D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			stUploadBufDesc.Width				= n64szUploadBufSkybox;
			stUploadBufDesc.Height				= 1;
			stUploadBufDesc.DepthOrArraySize	= 1;
			stUploadBufDesc.MipLevels			= 1;
			stUploadBufDesc.Format				= DXGI_FORMAT_UNKNOWN;
			stUploadBufDesc.SampleDesc.Count	= 1;
			stUploadBufDesc.SampleDesc.Quality	= 0;
			stUploadBufDesc.Layout				= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stUploadBufDesc.Flags				= D3D12_RESOURCE_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(pIUploadHeapSkybox.Get()
				, 0
				, &stUploadBufDesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pITextureUploadSkybox)));

			// 上传Skybox的纹理，注意因为这里的天空盒子资源是DDS格式的，并且有多个Mipmap，所以子资源的数量很多
			UINT nFirstSubresource = 0;
			UINT nNumSubresources = static_cast<UINT>(arSubResources.size());
			D3D12_RESOURCE_DESC stUploadResDesc = pITextureUploadSkybox->GetDesc();
			D3D12_RESOURCE_DESC stDefaultResDesc = pITextureSkybox->GetDesc();

			UINT64 n64RequiredSize = 0;
			SIZE_T szMemToAlloc = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) 
				+ sizeof(UINT) 
				+ sizeof(UINT64)) 
				* nNumSubresources;

			void* pMem = GRS_CALLOC(static_cast<SIZE_T>(szMemToAlloc));

			if ( nullptr == pMem )
			{
				throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
			}

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
			UINT64* pRowSizesInBytes = reinterpret_cast<UINT64*>(pLayouts + nNumSubresources);
			UINT* pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + nNumSubresources);

			// 这里是第二次调用GetCopyableFootprints，就得到了所有子资源的详细信息
			pID3D12Device4->GetCopyableFootprints(&stDefaultResDesc, nFirstSubresource, nNumSubresources, 0, pLayouts, pNumRows, pRowSizesInBytes, &n64RequiredSize);

			BYTE* pData = nullptr;
			HRESULT hr = pITextureUploadSkybox->Map(0, nullptr, reinterpret_cast<void**>(&pData));
			if (FAILED(hr))
			{
				return 0;
			}
			
			// 第一遍Copy！注意3重循环每重的意思
			for (UINT i = 0; i < nNumSubresources; ++i)
			{// SubResources
				if ( pRowSizesInBytes[i] > (SIZE_T)-1 )
				{
					throw CGRSCOMException(E_FAIL);
				}

				D3D12_MEMCPY_DEST stCopyDestData = { pData + pLayouts[i].Offset
					, pLayouts[i].Footprint.RowPitch
					, pLayouts[i].Footprint.RowPitch * pNumRows[i] 
				};
				
				for (UINT z = 0; z < pLayouts[i].Footprint.Depth; ++z)
				{// Mipmap
					BYTE* pDestSlice = reinterpret_cast<BYTE*>(stCopyDestData.pData) + stCopyDestData.SlicePitch * z;
					const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(arSubResources[i].pData) + arSubResources[i].SlicePitch * z;
					for (UINT y = 0; y < pNumRows[i]; ++y)
					{// Rows
						memcpy(pDestSlice + stCopyDestData.RowPitch * y,
							pSrcSlice + arSubResources[i].RowPitch * y,
							(SIZE_T)pRowSizesInBytes[i]);
					}
				}
			}
			pITextureUploadSkybox->Unmap(0, nullptr);

			// 第二次Copy！
			if (stDefaultResDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			{// Buffer 一次性复制就可以了，因为没有行对齐和行大小不一致的问题，Buffer中行数就是1
				pICmdListDirect->CopyBufferRegion(
					pITextureSkybox.Get(), 0, pITextureUploadSkybox.Get(), pLayouts[0].Offset, pLayouts[0].Footprint.Width);
			}
			else
			{
				for ( UINT i = 0; i < nNumSubresources; ++i )
				{
					D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
					stDstCopyLocation.pResource = pITextureSkybox.Get();
					stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
					stDstCopyLocation.SubresourceIndex = i;

					D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
					stSrcCopyLocation.pResource = pITextureUploadSkybox.Get();
					stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
					stSrcCopyLocation.PlacedFootprint = pLayouts[i];

					pICmdListDirect->CopyTextureRegion(&stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr);
				}
			}

			D3D12_RESOURCE_BARRIER stTransResBarrier = {};
	
			stTransResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stTransResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			stTransResBarrier.Transition.pResource = pITextureSkybox.Get();
			stTransResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			stTransResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			stTransResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			pICmdListDirect->ResourceBarrier(1, &stTransResBarrier);
		}

		//向直接命令列表发出从上传堆复制纹理数据到默认堆的命令，执行并同步等待，即完成第二个Copy动作，由GPU上的复制引擎完成
		//注意此时直接命令列表还没有绑定PSO对象，因此它也是不能执行3D图形命令的，但是可以执行复制命令，因为复制引擎不需要什么
		//额外的状态设置之类的参数
		{
			D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
			stDstCopyLocation.pResource = pITextureEarth.Get();
			stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			stDstCopyLocation.SubresourceIndex = 0;

			D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
			stSrcCopyLocation.pResource = pITextureUploadEarth.Get();
			stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			stSrcCopyLocation.PlacedFootprint = stTxtLayoutsEarth;

			pICmdListDirect->CopyTextureRegion(&stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr);

			//设置一个资源屏障，同步并确认复制操作完成
			//直接使用结构体然后调用的形式
			D3D12_RESOURCE_BARRIER stResBar = {};
			stResBar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stResBar.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			stResBar.Transition.pResource = pITextureEarth.Get();
			stResBar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			stResBar.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			stResBar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			pICmdListDirect->ResourceBarrier(1, &stResBar);

		}

		// 执行第二个Copy命令并确定所有的纹理都上传到了默认堆中
		{
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence)));
			n64FenceValue = 1;
			//创建一个Event同步对象，用于等待围栏事件通知
			hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hEventFence == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			// 执行命令列表并等待纹理资源上传完成，这一步是必须的
			GRS_THROW_IF_FAILED(pICmdListDirect->Close());

			ID3D12CommandList* ppCommandLists[] = { pICmdListDirect.Get() };
			pICMDQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

			// 等待纹理资源正式复制完成先
			const UINT64 fence = n64FenceValue;
			GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence.Get(), fence));
			n64FenceValue++;
			GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(fence, hEventFence));
		}

		// 加载球体的网格数据
		{
			ifstream fin;
			char input;
			USES_CONVERSION;
			char pModuleFileName[MAX_PATH] = {};
			StringCchPrintfA(pModuleFileName, MAX_PATH, "%sAssets\\sphere.txt", T2A(pszAppPath));
			fin.open(pModuleFileName);

			if (fin.fail())
			{
				throw CGRSCOMException(E_FAIL);
			}
			fin.get(input);
			while (input != ':')
			{
				fin.get(input);
			}
			fin >> nSphereVertexCnt;
			nSphereIndexCnt = nSphereVertexCnt;
			fin.get(input);
			while (input != ':')
			{
				fin.get(input);
			}
			fin.get(input);
			fin.get(input);

			pstSphereVertices = (ST_GRS_VERTEX*)GRS_CALLOC(nSphereVertexCnt * sizeof(ST_GRS_VERTEX));
			pSphereIndices = (UINT*)GRS_CALLOC(nSphereVertexCnt * sizeof(UINT));

			for (UINT i = 0; i < nSphereVertexCnt; i++)
			{
				fin >> pstSphereVertices[i].m_v4Position.x >> pstSphereVertices[i].m_v4Position.y >> pstSphereVertices[i].m_v4Position.z;
				pstSphereVertices[i].m_v4Position.w = 1.0f;
				fin >> pstSphereVertices[i].m_vTex.x >> pstSphereVertices[i].m_vTex.y;
				fin >> pstSphereVertices[i].m_vNor.x >> pstSphereVertices[i].m_vNor.y >> pstSphereVertices[i].m_vNor.z;

				pSphereIndices[i] = i;
			}
		}

		// 使用“定位方式”创建顶点缓冲和索引缓冲，使用与上传纹理数据缓冲相同的一个上传堆
		{
			UINT64 n64BufferOffset = GRS_UPPER(n64szUploadBufEarth, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

			D3D12_RESOURCE_DESC stBufResDesc = {};
			stBufResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stBufResDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			stBufResDesc.Width = nSphereVertexCnt * sizeof(ST_GRS_VERTEX);
			stBufResDesc.Height = 1;
			stBufResDesc.DepthOrArraySize = 1;
			stBufResDesc.MipLevels = 1;
			stBufResDesc.Format = DXGI_FORMAT_UNKNOWN;
			stBufResDesc.SampleDesc.Count = 1;
			stBufResDesc.SampleDesc.Quality = 0;
			stBufResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stBufResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			//使用定位方式在相同的上传堆上以“定位方式”创建顶点缓冲，注意第二个参数指出了堆中的偏移位置
			//按照堆边界对齐的要求，我们主动将偏移位置对齐到了64k的边界上			
			GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(
				pIUploadHeapEarth.Get()
				, n64BufferOffset
				, &stBufResDesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIVBEarth)));

			//使用map-memcpy-unmap大法将数据传至顶点缓冲对象
			//注意顶点缓冲使用是和上传纹理数据缓冲相同的一个堆，这很神奇
			UINT8* pVertexDataBegin = nullptr;
			D3D12_RANGE stReadRange = { 0, 0 };		// We do not intend to read from this resource on the CPU.

			GRS_THROW_IF_FAILED(pIVBEarth->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pstSphereVertices, nSphereVertexCnt * sizeof(ST_GRS_VERTEX));
			pIVBEarth->Unmap(0, nullptr);

			GRS_SAFE_FREE(pstSphereVertices);

			//创建资源视图，实际可以简单理解为指向顶点缓冲的显存指针
			stVBVEarth.BufferLocation = pIVBEarth->GetGPUVirtualAddress();
			stVBVEarth.StrideInBytes = sizeof(ST_GRS_VERTEX);
			stVBVEarth.SizeInBytes = nSphereVertexCnt * sizeof(ST_GRS_VERTEX);

			//计算边界对齐的正确的偏移位置
			n64BufferOffset = GRS_UPPER(n64BufferOffset + nSphereVertexCnt * sizeof(ST_GRS_VERTEX), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

			stBufResDesc.Width = nSphereIndexCnt * sizeof(UINT);

			GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(
				pIUploadHeapEarth.Get()
				, n64BufferOffset
				, &stBufResDesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIIBEarth)));

			UINT8* pIndexDataBegin = nullptr;
			GRS_THROW_IF_FAILED(pIIBEarth->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, pSphereIndices, nSphereIndexCnt * sizeof(UINT));
			pIIBEarth->Unmap(0, nullptr);

			GRS_SAFE_FREE(pSphereIndices);

			stIBVEarth.BufferLocation = pIIBEarth->GetGPUVirtualAddress();
			stIBVEarth.Format = DXGI_FORMAT_R32_UINT;
			stIBVEarth.SizeInBytes = nSphereIndexCnt * sizeof(UINT);

			n64BufferOffset = GRS_UPPER(n64BufferOffset + nSphereIndexCnt * sizeof(UINT), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

			stBufResDesc.Width = szMVPBuf;
			// 创建常量缓冲 注意缓冲尺寸设置为256边界对齐大小
			GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(
				pIUploadHeapEarth.Get()
				, n64BufferOffset
				, &stBufResDesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBUploadEarth)));

			// Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
			GRS_THROW_IF_FAILED(pICBUploadEarth->Map(0, nullptr, reinterpret_cast<void**>(&pMVPBufEarth)));
			//---------------------------------------------------------------------------------------------
		}

		// 加载天空盒（远平面型）
		{

			float fHighW = -1.0f - (1.0f / (float)iWndWidth);
			float fHighH = -1.0f - (1.0f / (float)iWndHeight);
			float fLowW = 1.0f + (1.0f / (float)iWndWidth);
			float fLowH = 1.0f + (1.0f / (float)iWndHeight);

			ST_GRS_SKYBOX_VERTEX stSkyboxVertices[4] = {};

			stSkyboxVertices[0].m_v4Position = XMFLOAT4(fLowW, fLowH, 1.0f, 1.0f);
			stSkyboxVertices[1].m_v4Position = XMFLOAT4(fLowW, fHighH, 1.0f, 1.0f);
			stSkyboxVertices[2].m_v4Position = XMFLOAT4(fHighW, fLowH, 1.0f, 1.0f);
			stSkyboxVertices[3].m_v4Position = XMFLOAT4(fHighW, fHighH, 1.0f, 1.0f);

			nSkyboxIndexCnt = 4;
			//---------------------------------------------------------------------------------------------
			//加载天空盒子的数据
			D3D12_RESOURCE_DESC stBufResDesc = {};
			stBufResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stBufResDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
			stBufResDesc.Width = nSkyboxIndexCnt * sizeof(ST_GRS_SKYBOX_VERTEX);
			stBufResDesc.Height = 1;
			stBufResDesc.DepthOrArraySize = 1;
			stBufResDesc.MipLevels = 1;
			stBufResDesc.Format = DXGI_FORMAT_UNKNOWN;
			stBufResDesc.SampleDesc.Count = 1;
			stBufResDesc.SampleDesc.Quality = 0;
			stBufResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stBufResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			UINT64 n64BufferOffset = GRS_UPPER(n64szUploadBufSkybox, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

			GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(
				pIUploadHeapSkybox.Get()
				, n64BufferOffset
				, &stBufResDesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIVBSkybox)));

			//使用map-memcpy-unmap大法将数据传至顶点缓冲对象
			//注意顶点缓冲使用是和上传纹理数据缓冲相同的一个堆，这很神奇
			ST_GRS_SKYBOX_VERTEX* pVertexDataBegin = nullptr;

			GRS_THROW_IF_FAILED(pIVBSkybox->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, stSkyboxVertices, nSkyboxIndexCnt * sizeof(ST_GRS_SKYBOX_VERTEX));
			pIVBSkybox->Unmap(0, nullptr);

			//创建资源视图，实际可以简单理解为指向顶点缓冲的显存指针
			stVBVSkybox.BufferLocation = pIVBSkybox->GetGPUVirtualAddress();
			stVBVSkybox.StrideInBytes = sizeof(ST_GRS_SKYBOX_VERTEX);
			stVBVSkybox.SizeInBytes = nSkyboxIndexCnt * sizeof(ST_GRS_SKYBOX_VERTEX);

			//计算边界对齐的正确的偏移位置
			n64BufferOffset = GRS_UPPER(n64BufferOffset + nSkyboxIndexCnt * sizeof(ST_GRS_SKYBOX_VERTEX), D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

			// 创建常量缓冲 注意缓冲尺寸设置为256边界对齐大小
			stBufResDesc.Width = szMVPBuf;
			GRS_THROW_IF_FAILED(pID3D12Device4->CreatePlacedResource(
				pIUploadHeapSkybox.Get()
				, n64BufferOffset
				, &stBufResDesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pICBUploadSkybox)));

			// Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
			GRS_THROW_IF_FAILED(pICBUploadSkybox->Map(0, nullptr, reinterpret_cast<void**>(&pMVPBufSkybox)));
			//---------------------------------------------------------------------------------------------
		}

		// 创建SRV描述符
		{
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = emTxtFmtEarth;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;
			pID3D12Device4->CreateShaderResourceView(pITextureEarth.Get(), &stSRVDesc, pISRVHpEarth->GetCPUDescriptorHandleForHeapStart());

			D3D12_RESOURCE_DESC stDescSkybox = pITextureSkybox->GetDesc();
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
			stSRVDesc.Format = stDescSkybox.Format;
			stSRVDesc.TextureCube.MipLevels = stDescSkybox.MipLevels;
			pID3D12Device4->CreateShaderResourceView(pITextureSkybox.Get(), &stSRVDesc, pISRVHpSkybox->GetCPUDescriptorHandleForHeapStart());
		}

		// 创建CBV描述符
		{
			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = pICBUploadEarth->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);

			D3D12_CPU_DESCRIPTOR_HANDLE stSRVCBVHandle = pISRVHpEarth->GetCPUDescriptorHandleForHeapStart();
			stSRVCBVHandle.ptr += nSRVDescriptorSize;

			pID3D12Device4->CreateConstantBufferView(&cbvDesc, stSRVCBVHandle);

			//---------------------------------------------------------------------------------------------
			cbvDesc.BufferLocation = pICBUploadSkybox->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = static_cast<UINT>(szMVPBuf);

			D3D12_CPU_DESCRIPTOR_HANDLE cbvSrvHandleSkybox = { pISRVHpSkybox->GetCPUDescriptorHandleForHeapStart() };
			cbvSrvHandleSkybox.ptr += nSRVDescriptorSize;

			pID3D12Device4->CreateConstantBufferView(&cbvDesc, cbvSrvHandleSkybox);
			//---------------------------------------------------------------------------------------------

		}

		// 创建各种采样器
		{
			D3D12_CPU_DESCRIPTOR_HANDLE hSamplerHeap = pISampleHpEarth->GetCPUDescriptorHandleForHeapStart();

			D3D12_SAMPLER_DESC stSamplerDesc = {};
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

			// Sampler 1
			stSamplerDesc.BorderColor[0] = 1.0f;
			stSamplerDesc.BorderColor[1] = 0.0f;
			stSamplerDesc.BorderColor[2] = 1.0f;
			stSamplerDesc.BorderColor[3] = 1.0f;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			pID3D12Device4->CreateSampler(&stSamplerDesc, hSamplerHeap);

			hSamplerHeap.ptr += nSamplerDescriptorSize;

			// Sampler 2
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			pID3D12Device4->CreateSampler(&stSamplerDesc, hSamplerHeap);

			hSamplerHeap.ptr += nSamplerDescriptorSize;

			// Sampler 3
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			pID3D12Device4->CreateSampler(&stSamplerDesc, hSamplerHeap);

			hSamplerHeap.ptr += nSamplerDescriptorSize;

			// Sampler 4
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
			pID3D12Device4->CreateSampler(&stSamplerDesc, hSamplerHeap);

			hSamplerHeap.ptr += nSamplerDescriptorSize;

			// Sampler 5
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
			pID3D12Device4->CreateSampler(&stSamplerDesc, hSamplerHeap);

			//---------------------------------------------------------------------------------------------
			//创建Skybox的采样器
			stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

			stSamplerDesc.MinLOD = 0;
			stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc.MipLODBias = 0.0f;
			stSamplerDesc.MaxAnisotropy = 1;
			stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			stSamplerDesc.BorderColor[0] = 0.0f;
			stSamplerDesc.BorderColor[1] = 0.0f;
			stSamplerDesc.BorderColor[2] = 0.0f;
			stSamplerDesc.BorderColor[3] = 0.0f;
			stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			pID3D12Device4->CreateSampler(&stSamplerDesc, pISampleHpSkybox->GetCPUDescriptorHandleForHeapStart());
			//---------------------------------------------------------------------------------------------

		}

		// 用捆绑包记录固化的命令
		{
			//球体的捆绑包
			pIBundlesEarth->SetGraphicsRootSignature(pIRootSignature.Get());
			pIBundlesEarth->SetPipelineState(pIPSOEarth.Get());
			ID3D12DescriptorHeap* ppHeapsEarth[] = { pISRVHpEarth.Get(),pISampleHpEarth.Get() };
			pIBundlesEarth->SetDescriptorHeaps(_countof(ppHeapsEarth), ppHeapsEarth);
			//设置SRV
			pIBundlesEarth->SetGraphicsRootDescriptorTable(0, pISRVHpEarth->GetGPUDescriptorHandleForHeapStart());

			D3D12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleEarth = pISRVHpEarth->GetGPUDescriptorHandleForHeapStart();
			stGPUCBVHandleEarth.ptr += nSRVDescriptorSize;

			//设置CBV
			pIBundlesEarth->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleEarth);

			D3D12_GPU_DESCRIPTOR_HANDLE hGPUSamplerEarth = pISampleHpEarth->GetGPUDescriptorHandleForHeapStart();
			hGPUSamplerEarth.ptr += (g_nCurrentSamplerNO * nSamplerDescriptorSize);

			//设置Sample
			pIBundlesEarth->SetGraphicsRootDescriptorTable(2, hGPUSamplerEarth);
			//注意我们使用的渲染手法是三角形列表，也就是通常的Mesh网格
			pIBundlesEarth->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pIBundlesEarth->IASetVertexBuffers(0, 1, &stVBVEarth);
			pIBundlesEarth->IASetIndexBuffer(&stIBVEarth);

			//Draw Call！！！
			pIBundlesEarth->DrawIndexedInstanced(nSphereIndexCnt, 1, 0, 0, 0);
			pIBundlesEarth->Close();



			//Skybox的捆绑包
			pIBundlesSkybox->SetPipelineState(pIPSOSkyBox.Get());
			pIBundlesSkybox->SetGraphicsRootSignature(pIRootSignature.Get());
			ID3D12DescriptorHeap* ppHeaps[] = { pISRVHpSkybox.Get(),pISampleHpSkybox.Get() };
			pIBundlesSkybox->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
			//设置SRV
			pIBundlesSkybox->SetGraphicsRootDescriptorTable(0, pISRVHpSkybox->GetGPUDescriptorHandleForHeapStart());

			D3D12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleSkybox = pISRVHpSkybox->GetGPUDescriptorHandleForHeapStart();
			stGPUCBVHandleSkybox.ptr += nSRVDescriptorSize;
			//设置CBV
			pIBundlesSkybox->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleSkybox);
			pIBundlesSkybox->SetGraphicsRootDescriptorTable(2, pISampleHpSkybox->GetGPUDescriptorHandleForHeapStart());
			pIBundlesSkybox->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pIBundlesSkybox->IASetVertexBuffers(0, 1, &stVBVSkybox);

			//Draw Call！！！
			pIBundlesSkybox->DrawInstanced(4, 1, 0, 0);
			pIBundlesSkybox->Close();
		}

		D3D12_RESOURCE_BARRIER stBeginResBarrier = {};
		D3D12_RESOURCE_BARRIER stEndResBarrier = {};
		{
			stBeginResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stBeginResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			stBeginResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
			stBeginResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			stBeginResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			stBeginResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;


			stEndResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			stEndResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			stEndResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
			stEndResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			stEndResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			stEndResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		}

		// 记录帧开始时间，和当前时间，以循环结束为界
		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		//计算旋转角度需要的变量
		double dModelRotationYAngle = 0.0f;

		DWORD dwRet = 0;
		BOOL bExit = FALSE;

		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		// 开始消息循环，并在其中不断渲染
		while (!bExit)
		{//注意这里我们调整了消息循环，将等待时间设置为0，同时将定时性的渲染，改成了每次循环都渲染
		 //但这不表示说MsgWait函数就没啥用了，坚持使用它是因为后面例子如果想加入多线程控制就非常简单了
			dwRet = ::MsgWaitForMultipleObjects(1, &hEventFence, FALSE, INFINITE, QS_ALLINPUT);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{//一批命令已经执行结束，开始新的一帧渲染
				//OnUpdate()
				{// 准备一个简单的旋转MVP矩阵 让方块转起来
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

					XMMATRIX xmView = XMMatrixLookAtLH(XMLoadFloat3(&g_f3EyePos)
						, XMLoadFloat3(&g_f3LockAt)
						, XMLoadFloat3(&g_f3HeapUp));

					XMMATRIX xmProj = XMMatrixPerspectiveFovLH(XM_PIDIV4
						, (FLOAT)iWndWidth / (FLOAT)iWndHeight, 1.0f, 2000.0f);
					//使用“Assets\\sky_cube.dds”时需要XMMATRIX(1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1)乘以xmView
					XMMATRIX xmSkyBox = xmView;// XMMatrixMultiply(XMMATRIX(1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1), xmView);
					xmSkyBox = XMMatrixMultiply(xmSkyBox, xmProj);
					xmSkyBox = XMMatrixInverse(nullptr, xmSkyBox);
					//设置Skybox的MVP
					XMStoreFloat4x4(&pMVPBufSkybox->m_MVP, xmSkyBox);

					//计算 视矩阵 view * 裁剪矩阵 projection
					XMMATRIX xmVP = XMMatrixMultiply(xmView, xmProj);

					pMVPBufSkybox->m_v4EyePos = XMFLOAT4(g_f3EyePos.x, g_f3EyePos.y, g_f3EyePos.z, 1.0f);

					//模型矩阵 model 这里是放大后旋转
					XMMATRIX xmRot = XMMatrixMultiply(XMMatrixScaling(fSphereSize, fSphereSize, fSphereSize)
						, XMMatrixRotationY(static_cast<float>(dModelRotationYAngle)));

					//计算球体的MVP
					xmVP = XMMatrixMultiply(xmRot, xmVP);

					XMStoreFloat4x4(&pMVPBufEarth->m_MVP, xmVP);
				}

				//获取新的后缓冲序号，因为Present真正完成时后缓冲的序号就更新了
				nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();
				//命令分配器先Reset一下
				GRS_THROW_IF_FAILED(pICmdAllocDirect->Reset());
				//Reset命令列表，并重新指定命令分配器和PSO对象
				GRS_THROW_IF_FAILED(pICmdListDirect->Reset(pICmdAllocDirect.Get(), pIPSOEarth.Get()));

				// 通过资源屏障判定后缓冲已经切换完毕可以开始渲染了
				stBeginResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
				pICmdListDirect->ResourceBarrier(1, &stBeginResBarrier);

				//偏移描述符指针到指定帧缓冲视图位置
				D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
				stRTVHandle.ptr += (nCurrentFrameIndex * nRTVDescriptorSize);
				D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = pIDSVHeap->GetCPUDescriptorHandleForHeapStart();
				//设置渲染目标
				pICmdListDirect->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);
				pICmdListDirect->RSSetViewports(1, &stViewPort);
				pICmdListDirect->RSSetScissorRects(1, &stScissorRect);

				// 继续记录命令，并真正开始新一帧的渲染
				pICmdListDirect->ClearRenderTargetView(stRTVHandle, faClearColor, 0, nullptr);
				pICmdListDirect->ClearDepthStencilView(pIDSVHeap->GetCPUDescriptorHandleForHeapStart()
					, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

				//===============================================================================================
				//31、执行Skybox的捆绑包
				ID3D12DescriptorHeap* ppHeapsSkybox[] = { pISRVHpSkybox.Get(),pISampleHpSkybox.Get() };
				pICmdListDirect->SetDescriptorHeaps(_countof(ppHeapsSkybox), ppHeapsSkybox);
				pICmdListDirect->ExecuteBundle(pIBundlesSkybox.Get());
				//===============================================================================================

				//===============================================================================================
				//32、执行球体的捆绑包
				ID3D12DescriptorHeap* ppHeapsEarth[] = { pISRVHpEarth.Get(),pISampleHpEarth.Get() };
				pICmdListDirect->SetDescriptorHeaps(_countof(ppHeapsEarth), ppHeapsEarth);
				pICmdListDirect->ExecuteBundle(pIBundlesEarth.Get());
				//===============================================================================================

				//又一个资源屏障，用于确定渲染已经结束可以提交画面去显示了
				stEndResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
				pICmdListDirect->ResourceBarrier(1, &stEndResBarrier);
				//关闭命令列表，可以去执行了
				GRS_THROW_IF_FAILED(pICmdListDirect->Close());

				//执行命令列表
				ID3D12CommandList* ppCommandLists[] = { pICmdListDirect.Get() };
				pICMDQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);


				//提交画面
				GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));


				//开始同步GPU与CPU的执行，先记录围栏标记值
				const UINT64 fence = n64FenceValue;
				GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence.Get(), fence));
				n64FenceValue++;
				GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(fence, hEventFence));
			}
			break;
			case 1:
			{//处理消息
				while (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
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
		//::CoUninitialize();
	}
	catch (CGRSCOMException & e)
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
	case WM_KEYDOWN:
	{
		USHORT n16KeyCode = (wParam & 0xFF);
		if (VK_SPACE == n16KeyCode)
		{//按空格键切换不同的采样器看效果，以明白每种采样器具体的含义
			//UINT g_nCurrentSamplerNO = 0; //当前使用的采样器索引
			//UINT g_nSampleMaxCnt = 5;		//创建五个典型的采样器
			++g_nCurrentSamplerNO;
			g_nCurrentSamplerNO %= g_nSampleMaxCnt;

			//=================================================================================================
			//重新设置球体的捆绑包
			//pIBundlesEarth->Reset(pICmdAllocEarth.Get(), pIPSOEarth.Get());
			//pIBundlesEarth->SetGraphicsRootSignature(pIRootSignature.Get());
			//pIBundlesEarth->SetPipelineState(pIPSOEarth.Get());
			//ID3D12DescriptorHeap* ppHeapsEarth[] = { pISRVHpEarth.Get(),pISampleHpEarth.Get() };
			//pIBundlesEarth->SetDescriptorHeaps(_countof(ppHeapsEarth), ppHeapsEarth);
			////设置SRV
			//pIBundlesEarth->SetGraphicsRootDescriptorTable(0, pISRVHpEarth->GetGPUDescriptorHandleForHeapStart());

			//CD3DX12_GPU_DESCRIPTOR_HANDLE stGPUCBVHandleEarth(pISRVHpEarth->GetGPUDescriptorHandleForHeapStart()
			//	, 1
			//	, pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
			////设置CBV
			//pIBundlesEarth->SetGraphicsRootDescriptorTable(1, stGPUCBVHandleEarth);
			//CD3DX12_GPU_DESCRIPTOR_HANDLE hGPUSamplerEarth(pISampleHpEarth->GetGPUDescriptorHandleForHeapStart()
			//	, g_nCurrentSamplerNO
			//	, nSamplerDescriptorSize);
			////设置Sample
			//pIBundlesEarth->SetGraphicsRootDescriptorTable(2, hGPUSamplerEarth);
			////注意我们使用的渲染手法是三角形列表，也就是通常的Mesh网格
			//pIBundlesEarth->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			//pIBundlesEarth->IASetVertexBuffers(0, 1, &stVBVEarth);
			//pIBundlesEarth->IASetIndexBuffer(&stIBVEarth);

			////Draw Call！！！
			//pIBundlesEarth->DrawIndexedInstanced(nSphereIndexCnt, 1, 0, 0, 0);
			//pIBundlesEarth->Close();
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

