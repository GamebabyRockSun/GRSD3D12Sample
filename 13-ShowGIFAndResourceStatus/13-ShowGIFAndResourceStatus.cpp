#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <commdlg.h>
#include <tchar.h>
#include <strsafe.h>
#include <atlconv.h>	//For A2T Macro
#include <wrl.h>		//添加WTL支持 方便使用COM
#include <fstream>  //for ifstream
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3d12.h>       //for d3d12
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#include <wincodec.h>   //for WIC

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
#define GRS_WND_TITLE	_T("GRS D3D12 Sample DirectComputer Show GIF & Resource Status")

#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); }}

//新定义的宏用于上取整除法
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

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

#define GRS_SET_DXGI_DEBUGNAME(x)								GRS_SetDXGIDebugName(x, L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED(x, n)			GRS_SetDXGIDebugNameIndexed(x[n], L#x, n)

#define GRS_SET_DXGI_DEBUGNAME_COMPTR(x)							GRS_SetDXGIDebugName(x.Get(), L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED_COMPTR(x, n)		GRS_SetDXGIDebugNameIndexed(x[n].Get(), L#x, n)


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

//////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif

struct ST_GRS_VERTEX
{
	XMFLOAT4 m_v4Position;		//Position
	XMFLOAT2 m_vTxc;		//Texcoord
	XMFLOAT3 m_vNor;		//Normal
};

struct ST_GRS_FRAME_MVP_BUFFER
{
	XMFLOAT4X4 m_MVP;			//经典的Model-view-projection(MVP)矩阵.
};

//--------------------------------------------------------------------------------------------------
//GIF Image Information struct and macro

#define GRS_ARGB_A(ARGBColor) (((float)((ARGBColor & 0xFF000000) >> 24))/255.0f)
#define GRS_ARGB_R(ARGBColor) (((float)((ARGBColor & 0x00FF0000) >> 16))/255.0f)
#define GRS_ARGB_G(ARGBColor) (((float)((ARGBColor & 0x0000FF00) >>  8))/255.0f)
#define GRS_ARGB_B(ARGBColor) (((float)( ARGBColor & 0x000000FF))/255.0f)

struct ST_GRS_GIF_FRAME_PARAM
{
	XMFLOAT4 m_c4BkColor;
	UINT	 m_nLeftTop[2];
	UINT	 m_nFrameWH[2];
	UINT	 m_nFrame;
	UINT 	 m_nDisposal;
};

enum EM_GRS_DISPOSAL_METHODS
{
	DM_UNDEFINED = 0,
	DM_NONE = 1,
	DM_BACKGROUND = 2,
	DM_PREVIOUS = 3
};
struct ST_GRS_GIF
{
	ComPtr<IWICBitmapDecoder>	m_pIWICDecoder;
	UINT						m_nTotalLoopCount;
	UINT						m_nLoopNumber;
	BOOL						m_bHasLoop;
	UINT						m_nFrames;
	UINT						m_nCurrentFrame;
	WICColor					m_nBkColor;
	UINT						m_nWidth;
	UINT						m_nHeight;
	UINT						m_nPixelWidth;
	UINT						m_nPixelHeight;
};

struct ST_GRS_RECT_F
{
	float m_fLeft;
	float m_fTop;
	float m_fRight;
	float m_fBottom;
};

struct ST_GRS_GIF_FRAME
{
	ComPtr<IWICBitmapFrameDecode>	m_pIWICGIFFrame;
	ComPtr<IWICBitmapSource>		m_pIWICGIFFrameBMP;
	DXGI_FORMAT						m_enTextureFormat;
	UINT							m_nFrameDisposal;
	UINT							m_nFrameDelay;
	UINT						    m_nLeftTop[2];
	UINT							m_nFrameWH[2];
	//ST_GRS_RECT_F					m_rtGIFFrame;
};
//--------------------------------------------------------------------------------------------------

//初始的默认摄像机的位置
XMVECTOR g_v4EyePos = XMVectorSet(0.0f, 2.0f, -5.0f, 0.0f); //眼睛位置
XMVECTOR g_v4LookAt = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);    //眼睛所盯的位置
XMVECTOR g_v4UpDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);    //头部正上方位置

double g_fPalstance = 10.0f * XM_PI / 180.0f;	//物体旋转的角速度，单位：弧度/秒

//GIF 文件信息
TCHAR						 g_pszAppPath[MAX_PATH] = {};
WCHAR						 g_pszTexcuteFileName[MAX_PATH] = {};
ST_GRS_GIF				     g_stGIF = {};

ComPtr<ID3D12Device4>		 g_pID3D12Device4;
ComPtr<IWICImagingFactory>	 g_pIWICFactory;
ComPtr<ID3D12Resource>		 g_pIRWTexture;
ComPtr<ID3D12DescriptorHeap> g_pICSSRVHeap;	  // Computer Shader CBV SRV Heap
HANDLE						 g_hEventFence = nullptr;
BOOL LoadGIF(LPCWSTR pszGIFFileName, IWICImagingFactory* pIWICFactory, ST_GRS_GIF& g_stGIF, const WICColor& nDefaultBkColor = 0u);
BOOL LoadGIFFrame(IWICImagingFactory* pIWICFactory, ST_GRS_GIF& g_stGIF, ST_GRS_GIF_FRAME& stGIFFrame);
BOOL UploadGIFFrame(ID3D12Device4* pID3D12Device4, ID3D12GraphicsCommandList* pICMDList, IWICImagingFactory* pIWICFactory, ST_GRS_GIF_FRAME& stGIFFrame, ID3D12Resource** pIGIFFrame, ID3D12Resource** pIGIFFrameUpload);

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);


int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	try
	{
		GRS_THROW_IF_FAILED(::CoInitialize(nullptr));  //for WIC & COM
		HWND												hWnd = nullptr;
		MSG													msg = {};

		int													iWidth = 1024;
		int													iHeight = 768;

		const UINT										nFrameBackBufCount = 3u;
		UINT												nCurrentFrameIndex = 0;
		DXGI_FORMAT								emRenderTarget = DXGI_FORMAT_R8G8B8A8_UNORM;
		const float										c4ClearColor[] = { 0.2f, 0.5f, 1.0f, 1.0f };

		ComPtr<IDXGIFactory5>				pIDXGIFactory5;
		ComPtr<IDXGIFactory6>				pIDXGIFactory6;
		ComPtr<IDXGIAdapter1>				pIAdapter1;


		ComPtr<ID3D12CommandQueue>			pICMDQueue;  //Direct Command Queue (3D Engine & Other Engine)
		ComPtr<ID3D12CommandQueue>			pICSQueue;	 //Computer Command Queue (Computer Engine)

		ComPtr<IDXGISwapChain1>						pISwapChain1;
		ComPtr<IDXGISwapChain3>						pISwapChain3;
		ComPtr<ID3D12Resource>						pIARenderTargets[nFrameBackBufCount];
		ComPtr<ID3D12DescriptorHeap>				pIRTVHeap;

		ComPtr<ID3D12Fence>								pIFence;

		UINT64														n64FenceValue = 0ui64;

		ComPtr<ID3D12CommandAllocator>		pICMDAlloc;
		ComPtr<ID3D12GraphicsCommandList>	pICMDList;

		ComPtr<ID3D12CommandAllocator>		pICSAlloc;		// Computer Command Alloc
		ComPtr<ID3D12GraphicsCommandList>	pICSList;		// Computer Command List

		ComPtr<ID3D12RootSignature>		pIRootSignature;
		ComPtr<ID3D12PipelineState>			pIPipelineState;

		ComPtr<ID3D12RootSignature>		pIRSComputer;	// Computer Shader Root Signature
		ComPtr<ID3D12PipelineState>			pIPSOComputer;  // Computer Pipeline Status Object

		UINT													nRTVDescriptorSize = 0U;
		UINT													nSRVDescriptorSize = 0U;

		ComPtr<ID3D12Resource>				pICBVRes;
		ComPtr<ID3D12Resource>				pIVBRes;
		ComPtr<ID3D12Resource>				pIIBRes;
		ComPtr<ID3D12Resource>				pITexture;
		ComPtr<ID3D12Resource>				pITextureUpload;
		ComPtr<ID3D12DescriptorHeap>		pISRVHeap;

		ComPtr<ID3D12Resource>				pICBGIFFrameInfo; // Computer Shader Const Buffer 

		SIZE_T													szGIFFrameInfo = GRS_UPPER(sizeof(ST_GRS_GIF_FRAME_PARAM), 256);
		ST_GRS_GIF_FRAME_PARAM* pstGIFFrameInfo = nullptr;

		D3D12_VIEWPORT								stViewPort = { 0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
		D3D12_RECT										stScissorRect = { 0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight) };

		ST_GRS_FRAME_MVP_BUFFER* pMVPBuffer = nullptr;
		SIZE_T													szMVPBuffer = GRS_UPPER(sizeof(ST_GRS_FRAME_MVP_BUFFER), 256);

		UINT													nVertexCnt = 0;
		D3D12_VERTEX_BUFFER_VIEW			stVertexBufferView = {};
		D3D12_INDEX_BUFFER_VIEW				stIndexBufferView = {};

		ST_GRS_GIF_FRAME								stGIFFrame = {};

		// 得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
		{
			if (0 == ::GetModuleFileName(nullptr, g_pszAppPath, MAX_PATH))
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			WCHAR* lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
			if (lastSlash)
			{//删除Exe文件名
				*(lastSlash) = _T('\0');
			}

			lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
			if (lastSlash)
			{//删除x64路径
				*(lastSlash) = _T('\0');
			}

			lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
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
			RECT rtWnd = { 0, 0, iWidth, iHeight };
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

		// 创建DXGI Factory对象		
		{
			UINT nDXGIFactoryFlags = 0U;
#if defined(_DEBUG)
			// 打开显示子系统的调试支持
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
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			//获取IDXGIFactory6接口
			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		// 枚举最高性能适配器来创建设备对象
		{
			GRS_THROW_IF_FAILED(pIDXGIFactory6->EnumAdapterByGpuPreference(0
				, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
				, IID_PPV_ARGS(&pIAdapter1)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIAdapter1);
			// 创建D3D12.1的设备
			GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter1.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&g_pID3D12Device4)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pID3D12Device4);

			DXGI_ADAPTER_DESC1 stAdapterDesc = {};
			GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc));

			TCHAR pszWndTitle[MAX_PATH] = {};
			GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc));
			::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
			StringCchPrintf(pszWndTitle
				, MAX_PATH
				, _T("%s (GPU:%s)")
				, pszWndTitle
				, stAdapterDesc.Description);
			::SetWindowText(hWnd, pszWndTitle);
		}

		// 创建命令队列
		{
			D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			//创建直接命令队列
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICMDQueue)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDQueue);

			stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

			//创建计算命令队列
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICSQueue)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICSQueue);
		}

		// 创建命令列表&分配器
		{
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pICMDAlloc)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAlloc);
			// 创建图形命令列表
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pICMDAlloc.Get(), pIPipelineState.Get(), IID_PPV_ARGS(&pICMDList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDList);

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&pICSAlloc)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICSAlloc);
			// 创建计算命令列表
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, pICSAlloc.Get(), nullptr, IID_PPV_ARGS(&pICSList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICSList);

		}

		// 创建交换链
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = nFrameBackBufCount;
			stSwapChainDesc.Width = iWidth;
			stSwapChainDesc.Height = iHeight;
			stSwapChainDesc.Format = emRenderTarget;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
				pICMDQueue.Get(),
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));

			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

			//得到当前后缓冲区的序号，也就是下一个将要呈送显示的缓冲区的序号
			//注意此处使用了高版本的SwapChain接口的函数
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);
			nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVHeap);
			//得到每个描述符元素的大小
			nRTVDescriptorSize = g_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			nSRVDescriptorSize = g_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			//创建RTV的描述符
			D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
			for (UINT i = 0; i < nFrameBackBufCount; i++)
			{
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&pIARenderTargets[i])));
				GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIARenderTargets, i);
				g_pID3D12Device4->CreateRenderTargetView(pIARenderTargets[i].Get(), nullptr, stRTVHandle);
				stRTVHandle.ptr += nRTVDescriptorSize;
			}
			// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		}

		// 创建围栏，用于CPU与GPU线程之间的同步
		{
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence)));
			n64FenceValue = 1;
			// 创建一个Event同步对象，用于等待围栏事件通知
			g_hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (g_hEventFence == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		// 创建根描述符
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
			// 检测是否支持V1.1版本的根签名
			stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (FAILED(g_pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof(stFeatureData))))
			{
				stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			D3D12_STATIC_SAMPLER_DESC stSamplerDesc[1] = {};
			stSamplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			stSamplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			stSamplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			stSamplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			stSamplerDesc[0].MipLODBias = 0;
			stSamplerDesc[0].MaxAnisotropy = 0;
			stSamplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			stSamplerDesc[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			stSamplerDesc[0].MinLOD = 0.0f;
			stSamplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
			stSamplerDesc[0].ShaderRegister = 0;
			stSamplerDesc[0].RegisterSpace = 0;
			stSamplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc = {};

			if (D3D_ROOT_SIGNATURE_VERSION_1_1 == stFeatureData.HighestVersion)
			{
				// 在GPU上执行SetGraphicsRootDescriptorTable后，我们不修改命令列表中的SRV，因此我们可以使用默认Rang行为:
				// D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
				D3D12_DESCRIPTOR_RANGE1 stDSPRanges1[2] = {};
				stDSPRanges1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDSPRanges1[0].NumDescriptors = 1;
				stDSPRanges1[0].BaseShaderRegister = 0;
				stDSPRanges1[0].RegisterSpace = 0;
				stDSPRanges1[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
				stDSPRanges1[0].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges1[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDSPRanges1[1].NumDescriptors = 1;
				stDSPRanges1[1].BaseShaderRegister = 0;
				stDSPRanges1[1].RegisterSpace = 0;
				stDSPRanges1[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
				stDSPRanges1[1].OffsetInDescriptorsFromTableStart = 0;

				D3D12_ROOT_PARAMETER1 stRootParameters1[2] = {};
				stRootParameters1[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				stRootParameters1[0].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[0].DescriptorTable.pDescriptorRanges = &stDSPRanges1[0];

				stRootParameters1[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters1[1].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[1].DescriptorTable.pDescriptorRanges = &stDSPRanges1[1];

				stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
				stRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
				stRootSignatureDesc.Desc_1_1.NumParameters = _countof(stRootParameters1);
				stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters1;
				stRootSignatureDesc.Desc_1_1.NumStaticSamplers = _countof(stSamplerDesc);
				stRootSignatureDesc.Desc_1_1.pStaticSamplers = stSamplerDesc;
			}
			else
			{
				D3D12_DESCRIPTOR_RANGE stDSPRanges[2] = {};
				stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDSPRanges[0].NumDescriptors = 1;
				stDSPRanges[0].BaseShaderRegister = 0;
				stDSPRanges[0].RegisterSpace = 0;
				stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDSPRanges[1].NumDescriptors = 1;
				stDSPRanges[1].BaseShaderRegister = 0;
				stDSPRanges[1].RegisterSpace = 0;
				stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

				D3D12_ROOT_PARAMETER stRootParameters[2] = {};
				stRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				stRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[0].DescriptorTable.pDescriptorRanges = &stDSPRanges[0];

				stRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[1].DescriptorTable.pDescriptorRanges = &stDSPRanges[1];

				stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
				stRootSignatureDesc.Desc_1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
				stRootSignatureDesc.Desc_1_0.NumParameters = _countof(stRootParameters);
				stRootSignatureDesc.Desc_1_0.pParameters = stRootParameters;
				stRootSignatureDesc.Desc_1_0.NumStaticSamplers = _countof(stSamplerDesc);
				stRootSignatureDesc.Desc_1_0.pStaticSamplers = stSamplerDesc;
			}

			ComPtr<ID3DBlob> pISignatureBlob;
			ComPtr<ID3DBlob> pIErrorBlob;

			GRS_THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRootSignatureDesc
				, &pISignatureBlob
				, &pIErrorBlob));

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRootSignature)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRootSignature);

			//------------------------------------------------------------------------------------------------------------
			// 创建计算管线的根签名
			D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRSComputerDesc = {};
			if (D3D_ROOT_SIGNATURE_VERSION_1_1 == stFeatureData.HighestVersion)
			{
				// 在GPU上执行SetGraphicsRootDescriptorTable后，我们不修改命令列表中的SRV，因此我们可以使用默认Rang行为:
				// D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
				D3D12_DESCRIPTOR_RANGE1 stDSPRanges1[3] = {};
				stDSPRanges1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDSPRanges1[0].NumDescriptors = 1; // 1 Const Buffer View + 1 Texture View
				stDSPRanges1[0].BaseShaderRegister = 0;
				stDSPRanges1[0].RegisterSpace = 0;
				stDSPRanges1[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
				stDSPRanges1[0].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges1[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDSPRanges1[1].NumDescriptors = 1; // 1 Const Buffer View + 1 Texture View
				stDSPRanges1[1].BaseShaderRegister = 0;
				stDSPRanges1[1].RegisterSpace = 0;
				stDSPRanges1[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
				stDSPRanges1[1].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges1[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				stDSPRanges1[2].NumDescriptors = 1;
				stDSPRanges1[2].BaseShaderRegister = 0;
				stDSPRanges1[2].RegisterSpace = 0;
				stDSPRanges1[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
				stDSPRanges1[2].OffsetInDescriptorsFromTableStart = 0;

				D3D12_ROOT_PARAMETER1 stRootParameters1[3] = {};
				stRootParameters1[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters1[0].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[0].DescriptorTable.pDescriptorRanges = &stDSPRanges1[0];

				stRootParameters1[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters1[1].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[1].DescriptorTable.pDescriptorRanges = &stDSPRanges1[1];

				stRootParameters1[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters1[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters1[2].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters1[2].DescriptorTable.pDescriptorRanges = &stDSPRanges1[2];

				stRSComputerDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
				stRSComputerDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
				stRSComputerDesc.Desc_1_1.NumParameters = _countof(stRootParameters1);
				stRSComputerDesc.Desc_1_1.pParameters = stRootParameters1;
				stRSComputerDesc.Desc_1_1.NumStaticSamplers = 0;
				stRSComputerDesc.Desc_1_1.pStaticSamplers = nullptr;
			}
			else
			{
				D3D12_DESCRIPTOR_RANGE stDSPRanges[3] = {};
				stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				stDSPRanges[0].NumDescriptors = 1;		// 1 Const Buffer View + 1 Texture View
				stDSPRanges[0].BaseShaderRegister = 0;
				stDSPRanges[0].RegisterSpace = 0;
				stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				stDSPRanges[1].NumDescriptors = 1;		// 1 Const Buffer View + 1 Texture View
				stDSPRanges[1].BaseShaderRegister = 0;
				stDSPRanges[1].RegisterSpace = 0;
				stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

				stDSPRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				stDSPRanges[2].NumDescriptors = 1;
				stDSPRanges[2].BaseShaderRegister = 0;
				stDSPRanges[2].RegisterSpace = 0;
				stDSPRanges[2].OffsetInDescriptorsFromTableStart = 0;

				D3D12_ROOT_PARAMETER stRootParameters[3] = {};
				stRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[0].DescriptorTable.pDescriptorRanges = &stDSPRanges[0];

				stRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[1].DescriptorTable.pDescriptorRanges = &stDSPRanges[1];

				stRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				stRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				stRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
				stRootParameters[2].DescriptorTable.pDescriptorRanges = &stDSPRanges[2];

				stRSComputerDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
				stRSComputerDesc.Desc_1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
				stRSComputerDesc.Desc_1_0.NumParameters = _countof(stRootParameters);
				stRSComputerDesc.Desc_1_0.pParameters = stRootParameters;
				stRSComputerDesc.Desc_1_0.NumStaticSamplers = 0;
				stRSComputerDesc.Desc_1_0.pStaticSamplers = nullptr;
			}

			pISignatureBlob.Reset();
			pIErrorBlob.Reset();

			GRS_THROW_IF_FAILED(D3D12SerializeVersionedRootSignature(&stRSComputerDesc, &pISignatureBlob, &pIErrorBlob));

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRSComputer)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSComputer);
		}

		// 编译Shader创建渲染管线状态对象
		{ {
				ComPtr<ID3DBlob> pIVSCode;
				ComPtr<ID3DBlob> pIPSCode;
#if defined(_DEBUG)
				// Enable better shader debugging with the graphics debugging tools.
				UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				UINT nShaderCompileFlags = 0;
#endif
				//编译为行矩阵形式	   
				nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

				TCHAR pszShaderFileName[MAX_PATH] = {};
				StringCchPrintf(pszShaderFileName, MAX_PATH, _T("%s13-ShowGIFAndResourceStatus\\Shader\\13-ShowGIFAndResourceStatus.hlsl"), g_pszAppPath);

				GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSCode, nullptr));
				GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName, nullptr, nullptr
					, "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, nullptr));

				// Define the vertex input layout.
				D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				// 创建 graphics pipeline state object (PSO)对象
				D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
				stPSODesc.InputLayout = { stInputElementDescs, _countof(stInputElementDescs) };
				stPSODesc.pRootSignature = pIRootSignature.Get();

				stPSODesc.VS.pShaderBytecode = pIVSCode->GetBufferPointer();
				stPSODesc.VS.BytecodeLength = pIVSCode->GetBufferSize();

				stPSODesc.PS.pShaderBytecode = pIPSCode->GetBufferPointer();
				stPSODesc.PS.BytecodeLength = pIPSCode->GetBufferSize();

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
				stPSODesc.RTVFormats[0] = emRenderTarget;
				stPSODesc.SampleDesc.Count = 1;

				GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc, IID_PPV_ARGS(&pIPipelineState)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPipelineState);
			} }

		// 创建计算管线状态对象
		{

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT nShaderCompileFlags = 0;
#endif
			//编译为行矩阵形式	   
			//nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName
				, MAX_PATH
				, _T("%s13-ShowGIFAndResourceStatus\\Shader\\13-ShowGIF_CS.hlsl")
				, g_pszAppPath);

			ComPtr<ID3DBlob> pICSCode;
			ComPtr<ID3DBlob> pIErrMsg;
			CHAR pszErrMsg[MAX_PATH] = {};
			HRESULT hr = S_OK;

			hr = D3DCompileFromFile(pszShaderFileName
				, nullptr
				, nullptr
				, "ShowGIFCS"
				, "cs_5_0"
				, nShaderCompileFlags
				, 0
				, &pICSCode
				, &pIErrMsg);
			if (FAILED(hr))
			{
				StringCchPrintfA(pszErrMsg, MAX_PATH, "\n%s\n", (CHAR*)pIErrMsg->GetBufferPointer());
				::OutputDebugStringA(pszErrMsg);
				throw CGRSCOMException(hr);
			}

			// 创建 graphics pipeline state object (PSO)对象
			D3D12_COMPUTE_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.pRootSignature = pIRSComputer.Get();
			stPSODesc.CS.pShaderBytecode = pICSCode->GetBufferPointer();
			stPSODesc.CS.BytecodeLength = pICSCode->GetBufferSize();

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateComputePipelineState(&stPSODesc, IID_PPV_ARGS(&pIPSOComputer)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIPSOComputer);
		}

		// 创建常量缓冲
		{
			D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_UPLOAD };

			D3D12_RESOURCE_DESC stResDesc = {};
			stResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			stResDesc.Format = DXGI_FORMAT_UNKNOWN;
			stResDesc.Width = szMVPBuffer;
			stResDesc.Height = 1;
			stResDesc.DepthOrArraySize = 1;
			stResDesc.MipLevels = 1;
			stResDesc.SampleDesc.Count = 1;
			stResDesc.SampleDesc.Quality = 0;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp,
				D3D12_HEAP_FLAG_NONE,
				&stResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pICBVRes)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBVRes);

			// Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
			GRS_THROW_IF_FAILED(pICBVRes->Map(0, nullptr, reinterpret_cast<void**>(&pMVPBuffer)));

			//-----------------------------------------------------------------------------------------------------
			stResDesc.Width = szGIFFrameInfo;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp,
				D3D12_HEAP_FLAG_NONE,
				&stResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pICBGIFFrameInfo)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICBGIFFrameInfo);

			// Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
			GRS_THROW_IF_FAILED(pICBGIFFrameInfo->Map(0, nullptr, reinterpret_cast<void**>(&pstGIFFrameInfo)));
			//-----------------------------------------------------------------------------------------------------
		}

		// 创建顶点缓冲
		{
			ST_GRS_VERTEX* pstVertices = nullptr;
			UINT* pnIndices = nullptr;
			USES_CONVERSION;
			CHAR pszMeshFileName[MAX_PATH] = {};
			StringCchPrintfA(pszMeshFileName, MAX_PATH, "%sAssets\\Cube.txt", T2A(g_pszAppPath));
			//载入方块
			LoadMeshVertex(pszMeshFileName, nVertexCnt, pstVertices, pnIndices);

			UINT nIndexCnt = nVertexCnt;

			UINT nVertexBufferSize = nVertexCnt * sizeof(ST_GRS_VERTEX);

			D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_UPLOAD };

			D3D12_RESOURCE_DESC stResDesc = {};
			stResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			stResDesc.Format = DXGI_FORMAT_UNKNOWN;
			stResDesc.Width = nVertexBufferSize;
			stResDesc.Height = 1;
			stResDesc.DepthOrArraySize = 1;
			stResDesc.MipLevels = 1;
			stResDesc.SampleDesc.Count = 1;
			stResDesc.SampleDesc.Quality = 0;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp,
				D3D12_HEAP_FLAG_NONE,
				&stResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIVBRes)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVBRes);

			UINT8* pVertexDataBegin = nullptr;
			D3D12_RANGE stReadRange = { 0, 0 };
			GRS_THROW_IF_FAILED(pIVBRes->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pstVertices, nVertexBufferSize);
			pIVBRes->Unmap(0, nullptr);

			stVertexBufferView.BufferLocation = pIVBRes->GetGPUVirtualAddress();
			stVertexBufferView.StrideInBytes = sizeof(ST_GRS_VERTEX);
			stVertexBufferView.SizeInBytes = nVertexBufferSize;

			stResDesc.Width = nIndexCnt * sizeof(UINT);
			//创建 Index Buffer 仅使用Upload隐式堆
			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp
				, D3D12_HEAP_FLAG_NONE
				, &stResDesc
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIIBRes)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIIBRes);

			UINT8* pIndexDataBegin = nullptr;
			GRS_THROW_IF_FAILED(pIIBRes->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, pnIndices, nIndexCnt * sizeof(UINT));
			pIIBRes->Unmap(0, nullptr);

			//创建Index Buffer View
			stIndexBufferView.BufferLocation = pIIBRes->GetGPUVirtualAddress();
			stIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
			stIndexBufferView.SizeInBytes = nIndexCnt * sizeof(UINT);

			::HeapFree(::GetProcessHeap(), 0, pstVertices);
			::HeapFree(::GetProcessHeap(), 0, pnIndices);
		}

		// 使用WIC创建并加载一个2D纹理
		{
			//使用纯COM方式创建WIC类厂对象，也是调用WIC第一步要做的事情
			GRS_THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&g_pIWICFactory)));
			//使用WIC类厂对象接口加载纹理图片，并得到一个WIC解码器对象接口，图片信息就在这个接口代表的对象中了
			StringCchPrintfW(g_pszTexcuteFileName, MAX_PATH, _T("%sAssets\\MM.gif"), g_pszAppPath);

			OPENFILENAME ofn;
			RtlZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = nullptr;
			ofn.lpstrFilter = L"*Gif 文件\0*.gif\0";
			ofn.lpstrFile = g_pszTexcuteFileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrTitle = L"请选择一个要显示的GIF文件...";
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

			if (!GetOpenFileName(&ofn))
			{
				StringCchPrintfW(g_pszTexcuteFileName, MAX_PATH, _T("%sAssets\\MM.gif"), g_pszAppPath);
			}

			//只载入文件，但不生成纹理，在渲染循环中再生成纹理
			if (!LoadGIF(g_pszTexcuteFileName, g_pIWICFactory.Get(), g_stGIF))
			{
				throw CGRSCOMException(E_FAIL);
			}

			//----------------------------------------------------------------------------------------------------------
			// 创建Computer Shader 需要的背景 RWTexture2D 资源
			DXGI_FORMAT emFmtRWTexture = DXGI_FORMAT_R8G8B8A8_UNORM;
			D3D12_RESOURCE_DESC stRWTextureDesc = {};
			stRWTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			stRWTextureDesc.MipLevels = 1;
			stRWTextureDesc.Format = emFmtRWTexture;
			stRWTextureDesc.Width = g_stGIF.m_nPixelWidth;
			stRWTextureDesc.Height = g_stGIF.m_nPixelHeight;
			stRWTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			stRWTextureDesc.DepthOrArraySize = 1;
			stRWTextureDesc.SampleDesc.Count = 1;
			stRWTextureDesc.SampleDesc.Quality = 0;

			D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_DEFAULT };

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
				&stHeapProp
				, D3D12_HEAP_FLAG_NONE
				, &stRWTextureDesc
				, D3D12_RESOURCE_STATE_COMMON
				, nullptr
				, IID_PPV_ARGS(&g_pIRWTexture)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pIRWTexture);

		}

		// 创建SRV堆 (Shader Resource View Heap)
		{
			D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};
			stSRVHeapDesc.NumDescriptors = 2;  //1 Texutre SRV + 1 CBV
			stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc, IID_PPV_ARGS(&pISRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pISRVHeap);

			// 创建SRV描述符
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format = stGIFFrame.m_enTextureFormat;
			stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			stSRVDesc.Texture2D.MipLevels = 1;
			// 注意使用RWTexture作为渲染用的纹理 也就是经过Computer Shader 预处理一下先
			g_pID3D12Device4->CreateShaderResourceView(g_pIRWTexture.Get(), &stSRVDesc, pISRVHeap->GetCPUDescriptorHandleForHeapStart());

			// CBV
			D3D12_CONSTANT_BUFFER_VIEW_DESC stCBVDesc = {};
			stCBVDesc.BufferLocation = pICBVRes->GetGPUVirtualAddress();
			stCBVDesc.SizeInBytes = static_cast<UINT>(szMVPBuffer);

			D3D12_CPU_DESCRIPTOR_HANDLE stSRVHandle = pISRVHeap->GetCPUDescriptorHandleForHeapStart();
			stSRVHandle.ptr += nSRVDescriptorSize;
			g_pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stSRVHandle);


			//-------------------------------------------------------------------------------------------------------------
			// 创建 Computer Shader 需要的 Descriptor Heap
			stSRVHeapDesc.NumDescriptors = 3;  // 1 CBV + 1 Texutre SRV + 1 UAV

			GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateDescriptorHeap(&stSRVHeapDesc, IID_PPV_ARGS(&g_pICSSRVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pICSSRVHeap);


			stCBVDesc.BufferLocation = pICBGIFFrameInfo->GetGPUVirtualAddress();
			stCBVDesc.SizeInBytes = static_cast<UINT>(szGIFFrameInfo);

			stSRVHandle = g_pICSSRVHeap->GetCPUDescriptorHandleForHeapStart();
			// Computer Shader CBV
			g_pID3D12Device4->CreateConstantBufferView(&stCBVDesc, stSRVHandle);

			stSRVHandle.ptr += nSRVDescriptorSize;
			// Computer Shader SRV
			//g_pID3D12Device4->CreateShaderResourceView(pITexture.Get(), &stSRVDesc, stSRVHandle);			

			stSRVHandle.ptr += nSRVDescriptorSize;

			// Computer Shader UAV
			D3D12_UNORDERED_ACCESS_VIEW_DESC stUAVDesc = {};
			stUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			stUAVDesc.Format = g_pIRWTexture->GetDesc().Format;
			stUAVDesc.Texture2D.MipSlice = 0;

			g_pID3D12Device4->CreateUnorderedAccessView(g_pIRWTexture.Get(), nullptr, &stUAVDesc, stSRVHandle);
		}

		// 填充资源屏障结构
		D3D12_RESOURCE_BARRIER stBeginRenderRTResBarrier = {};
		stBeginRenderRTResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stBeginRenderRTResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stBeginRenderRTResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
		stBeginRenderRTResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		stBeginRenderRTResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		stBeginRenderRTResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		D3D12_RESOURCE_BARRIER stEndRenderRTResBarrier = {};
		stEndRenderRTResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stEndRenderRTResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stEndRenderRTResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
		stEndRenderRTResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		stEndRenderRTResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		stEndRenderRTResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
		DWORD dwRet = 0;
		BOOL bExit = FALSE;

		ULONGLONG n64tmFrameStart = ::GetTickCount64();
		ULONGLONG n64tmCurrent = n64tmFrameStart;
		ULONGLONG n64GIFPlayDelay = stGIFFrame.m_nFrameDelay;
		BOOL bReDrawFrame = FALSE;
		//计算旋转角度需要的变量
		double dModelRotationYAngle = 0.0f;


		GRS_THROW_IF_FAILED(pICSList->Close());
		GRS_THROW_IF_FAILED(pICMDList->Close());
		SetEvent(g_hEventFence);

		//初始化都结束以后再显示窗口
		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);

		// 开始消息循环，并在其中不断渲染
		while (!bExit)
		{
			dwRet = ::MsgWaitForMultipleObjects(1, &g_hEventFence, FALSE, INFINITE, QS_ALLINPUT);
			switch (dwRet - WAIT_OBJECT_0)
			{
			case 0:
			{
				n64tmCurrent = ::GetTickCount();
				//OnUpdate()
				{// OnUpdate()
					//计算旋转的角度：旋转角度(弧度) = 时间(秒) * 角速度(弧度/秒)
					//下面这句代码相当于经典游戏消息循环中的OnUpdate函数中需要做的事情
					dModelRotationYAngle += ((n64tmCurrent - n64tmFrameStart) / 1000.0f) * g_fPalstance;
					//旋转角度是2PI周期的倍数，去掉周期数，只留下相对0弧度开始的小于2PI的弧度即可
					if (dModelRotationYAngle > XM_2PI)
					{
						dModelRotationYAngle = fmod(dModelRotationYAngle, XM_2PI);
					}

					//模型矩阵 model
					XMMATRIX xmRot = XMMatrixRotationY(static_cast<float>(dModelRotationYAngle));

					//计算 模型矩阵 model * 视矩阵 view
					XMMATRIX xmMVP = XMMatrixMultiply(xmRot, XMMatrixLookAtLH(g_v4EyePos, g_v4LookAt, g_v4UpDir));

					//投影矩阵 projection
					xmMVP = XMMatrixMultiply(xmMVP, (XMMatrixPerspectiveFovLH(XM_PIDIV4, (FLOAT)iWidth / (FLOAT)iHeight, 0.1f, 1000.0f)));

					XMStoreFloat4x4(&pMVPBuffer->m_MVP, xmMVP);
				}

				//OnComputerShader()
				{
					if (0 == g_stGIF.m_nCurrentFrame || (n64tmCurrent - n64tmFrameStart) >= n64GIFPlayDelay)
					{
						GRS_THROW_IF_FAILED(pICSAlloc->Reset());
						GRS_THROW_IF_FAILED(pICSList->Reset(pICSAlloc.Get(), pIPSOComputer.Get()));

						stGIFFrame.m_pIWICGIFFrameBMP.Reset();
						stGIFFrame.m_pIWICGIFFrame.Reset();
						stGIFFrame.m_nFrameDelay = 0;

						if (!LoadGIFFrame(g_pIWICFactory.Get(), g_stGIF, stGIFFrame))
						{
							throw CGRSCOMException(E_FAIL);
						}

						pITexture.Reset();
						pITextureUpload.Reset();

						if (!UploadGIFFrame(g_pID3D12Device4.Get()
							, pICSList.Get()
							, g_pIWICFactory.Get()
							, stGIFFrame
							, pITexture.GetAddressOf()
							, pITextureUpload.GetAddressOf()))
						{
							throw CGRSCOMException(E_FAIL);
						}

						// 设置GIF的背景色，注意Shader中颜色值一般是RGBA格式
						pstGIFFrameInfo->m_c4BkColor = XMFLOAT4(
							GRS_ARGB_R(g_stGIF.m_nBkColor)
							, GRS_ARGB_G(g_stGIF.m_nBkColor)
							, GRS_ARGB_B(g_stGIF.m_nBkColor)
							, GRS_ARGB_A(g_stGIF.m_nBkColor));
						pstGIFFrameInfo->m_nFrame = g_stGIF.m_nCurrentFrame;
						pstGIFFrameInfo->m_nDisposal = stGIFFrame.m_nFrameDisposal;

						pstGIFFrameInfo->m_nLeftTop[0] = stGIFFrame.m_nLeftTop[0];	//Frame Image Left
						pstGIFFrameInfo->m_nLeftTop[1] = stGIFFrame.m_nLeftTop[1];	//Frame Image Top
						pstGIFFrameInfo->m_nFrameWH[0] = stGIFFrame.m_nFrameWH[0];	//Frame Image Width
						pstGIFFrameInfo->m_nFrameWH[1] = stGIFFrame.m_nFrameWH[1];	//Frame Image Height

						D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
						stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
						stSRVDesc.Format = stGIFFrame.m_enTextureFormat;
						stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
						stSRVDesc.Texture2D.MipLevels = 1;

						D3D12_CPU_DESCRIPTOR_HANDLE stSRVHandle = g_pICSSRVHeap->GetCPUDescriptorHandleForHeapStart();
						stSRVHandle.ptr += nSRVDescriptorSize;
						g_pID3D12Device4->CreateShaderResourceView(pITexture.Get(), &stSRVDesc, stSRVHandle);

						n64GIFPlayDelay = stGIFFrame.m_nFrameDelay;

						//更新到下一帧帧号
						g_stGIF.m_nCurrentFrame = (++g_stGIF.m_nCurrentFrame) % g_stGIF.m_nFrames;

						bReDrawFrame = TRUE;
					}
					else
					{
						if (!SUCCEEDED(ULongLongSub(n64GIFPlayDelay, (n64tmCurrent - n64tmFrameStart), &n64GIFPlayDelay)))
						{// 这个调用失败说明一定是已经超时到下一帧的时间了，那就开始下一循环绘制下一帧
							n64GIFPlayDelay = 0;
						}
						bReDrawFrame = FALSE; //只更新延迟时间，不绘制
					}

					if (bReDrawFrame)
					{
						// 开始计算管线运行
						D3D12_RESOURCE_BARRIER stRWResBeginBarrier = {};
						stRWResBeginBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
						stRWResBeginBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
						stRWResBeginBarrier.Transition.pResource = g_pIRWTexture.Get();
						stRWResBeginBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
						stRWResBeginBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						stRWResBeginBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

						pICSList->ResourceBarrier(1, &stRWResBeginBarrier);

						pICSList->SetComputeRootSignature(pIRSComputer.Get());
						pICSList->SetPipelineState(pIPSOComputer.Get());

						ID3D12DescriptorHeap* ppHeaps[] = { g_pICSSRVHeap.Get() };
						pICSList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

						D3D12_GPU_DESCRIPTOR_HANDLE stSRVGPUHandle = g_pICSSRVHeap->GetGPUDescriptorHandleForHeapStart();

						pICSList->SetComputeRootDescriptorTable(0, stSRVGPUHandle);
						stSRVGPUHandle.ptr += nSRVDescriptorSize;
						pICSList->SetComputeRootDescriptorTable(1, stSRVGPUHandle);
						stSRVGPUHandle.ptr += nSRVDescriptorSize;
						pICSList->SetComputeRootDescriptorTable(2, stSRVGPUHandle);

						// Computer!
						// 注意：按子帧大小来发起计算线程
						pICSList->Dispatch(stGIFFrame.m_nFrameWH[0], stGIFFrame.m_nFrameWH[1], 1);
						//pICSList->Dispatch(g_stGIF.m_nWidth, g_stGIF.m_nHeight, 1);

						// 开始计算管线运行
						D3D12_RESOURCE_BARRIER stRWResEndBarrier = {};
						stRWResEndBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
						stRWResEndBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
						stRWResEndBarrier.Transition.pResource = g_pIRWTexture.Get();
						stRWResEndBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
						stRWResEndBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
						stRWResEndBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

						pICSList->ResourceBarrier(1, &stRWResEndBarrier);

						GRS_THROW_IF_FAILED(pICSList->Close());
						//执行命令列表
						ID3D12CommandList* ppComputeCommandLists[] = { pICSList.Get() };
						pICSQueue->ExecuteCommandLists(_countof(ppComputeCommandLists), ppComputeCommandLists);
						UINT64 n64CurrentFenceValue = n64FenceValue;
						GRS_THROW_IF_FAILED(pICSQueue->Signal(pIFence.Get(), n64CurrentFenceValue));
						//3D 渲染引擎 等待 Computer 引擎执行结束
						GRS_THROW_IF_FAILED(pICMDQueue->Wait(pIFence.Get(), n64CurrentFenceValue));
						n64FenceValue++;
					}
				}

				//OnRender()
				{
					//获取新的后缓冲序号，因为Present真正完成时后缓冲的序号就更新了
					nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();
					//命令分配器先Reset一下
					GRS_THROW_IF_FAILED(pICMDAlloc->Reset());
					//Reset命令列表，并重新指定命令分配器和PSO对象
					GRS_THROW_IF_FAILED(pICMDList->Reset(pICMDAlloc.Get(), pIPipelineState.Get()));
					//开始记录命令
					pICMDList->SetGraphicsRootSignature(pIRootSignature.Get());
					ID3D12DescriptorHeap* ppHeaps[] = { pISRVHeap.Get() };
					pICMDList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
					D3D12_GPU_DESCRIPTOR_HANDLE stSRVHandle = pISRVHeap->GetGPUDescriptorHandleForHeapStart();
					pICMDList->SetGraphicsRootDescriptorTable(0, stSRVHandle);
					stSRVHandle.ptr += nSRVDescriptorSize;
					pICMDList->SetGraphicsRootDescriptorTable(1, stSRVHandle);
					pICMDList->RSSetViewports(1, &stViewPort);
					pICMDList->RSSetScissorRects(1, &stScissorRect);

					// 通过资源屏障判定后缓冲已经切换完毕可以开始渲染了
					stBeginRenderRTResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
					pICMDList->ResourceBarrier(1, &stBeginRenderRTResBarrier);

					stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
					stRTVHandle.ptr += nCurrentFrameIndex * nRTVDescriptorSize;
					//设置渲染目标
					pICMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, nullptr);
					pICMDList->ClearRenderTargetView(stRTVHandle, c4ClearColor, 0, nullptr);

					// 继续记录命令，并真正开始新一帧的渲染
					pICMDList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

					pICMDList->IASetVertexBuffers(0, 1, &stVertexBufferView);
					pICMDList->IASetIndexBuffer(&stIndexBufferView);
					//Draw Call！！！
					pICMDList->DrawInstanced(nVertexCnt, 1, 0, 0);

					//又一个资源屏障，用于确定渲染已经结束可以提交画面去显示了
					stEndRenderRTResBarrier.Transition.pResource = pIARenderTargets[nCurrentFrameIndex].Get();
					pICMDList->ResourceBarrier(1, &stEndRenderRTResBarrier);
					//关闭命令列表，可以去执行了
					GRS_THROW_IF_FAILED(pICMDList->Close());

					//执行命令列表
					ID3D12CommandList* ppCommandLists[] = { pICMDList.Get() };
					pICMDQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

					//提交画面
					GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

					//开始同步GPU与CPU的执行，先记录围栏标记值
					const UINT64 n64CurrentFenceValue = n64FenceValue;
					GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence.Get(), n64CurrentFenceValue));
					GRS_THROW_IF_FAILED(pICSQueue->Wait(pIFence.Get(), n64CurrentFenceValue))
						GRS_THROW_IF_FAILED(pIFence->SetEventOnCompletion(n64CurrentFenceValue, g_hEventFence));
					n64FenceValue++;
				}

				n64tmFrameStart = n64tmCurrent;
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
	//::CoUninitialize();
	return 0;
}

BOOL LoadGIF(LPCWSTR pszGIFFileName, IWICImagingFactory* pIWICFactory, ST_GRS_GIF& g_stGIF, const WICColor& nDefaultBkColor)
{
	BOOL bRet = TRUE;
	try
	{
		PROPVARIANT					stCOMPropValue = {};
		DWORD						dwBGColor = 0;
		BYTE						bBkColorIndex = 0;
		WICColor					pPaletteColors[256] = {};
		UINT						nColorsCopied = 0;
		IWICPalette* pIWICPalette = NULL;
		IWICMetadataQueryReader* pIGIFMetadate = NULL;

		PropVariantInit(&stCOMPropValue);

		GRS_THROW_IF_FAILED(pIWICFactory->CreateDecoderFromFilename(
			pszGIFFileName,						// 文件名
			NULL,								// 不指定解码器，使用默认
			GENERIC_READ,						// 访问权限
			WICDecodeMetadataCacheOnDemand,		// 若需要就缓冲数据 
			&g_stGIF.m_pIWICDecoder				// 解码器对象
		));

		GRS_THROW_IF_FAILED(g_stGIF.m_pIWICDecoder->GetFrameCount(&g_stGIF.m_nFrames));
		GRS_THROW_IF_FAILED(g_stGIF.m_pIWICDecoder->GetMetadataQueryReader(&pIGIFMetadate));
		if (
			SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/GlobalColorTableFlag", &stCOMPropValue))
			&& VT_BOOL == stCOMPropValue.vt
			&& stCOMPropValue.boolVal
			)
		{// 如果有背景色，则读取背景色
			PropVariantClear(&stCOMPropValue);
			if (SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/BackgroundColorIndex", &stCOMPropValue)))
			{
				if (VT_UI1 == stCOMPropValue.vt)
				{
					bBkColorIndex = stCOMPropValue.bVal;
				}
				GRS_THROW_IF_FAILED(pIWICFactory->CreatePalette(&pIWICPalette));
				GRS_THROW_IF_FAILED(g_stGIF.m_pIWICDecoder->CopyPalette(pIWICPalette));
				GRS_THROW_IF_FAILED(pIWICPalette->GetColors(ARRAYSIZE(pPaletteColors), pPaletteColors, &nColorsCopied));

				// Check whether background color is outside range 
				if (bBkColorIndex <= nColorsCopied)
				{
					g_stGIF.m_nBkColor = pPaletteColors[bBkColorIndex];
				}
			}
		}
		else
		{
			g_stGIF.m_nBkColor = nDefaultBkColor;
		}

		PropVariantClear(&stCOMPropValue);

		if (
			SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/Width", &stCOMPropValue))
			&& VT_UI2 == stCOMPropValue.vt
			)
		{
			g_stGIF.m_nWidth = stCOMPropValue.uiVal;
		}
		PropVariantClear(&stCOMPropValue);

		if (
			SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/Height", &stCOMPropValue))
			&& VT_UI2 == stCOMPropValue.vt
			)
		{
			g_stGIF.m_nHeight = stCOMPropValue.uiVal;
		}
		PropVariantClear(&stCOMPropValue);

		if (
			SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/logscrdesc/PixelAspectRatio", &stCOMPropValue))
			&& VT_UI1 == stCOMPropValue.vt
			)
		{
			UINT uPixelAspRatio = stCOMPropValue.bVal;
			if (uPixelAspRatio != 0)
			{
				FLOAT pixelAspRatio = (uPixelAspRatio + 15.f) / 64.f;
				if (pixelAspRatio > 1.f)
				{
					g_stGIF.m_nPixelWidth = g_stGIF.m_nWidth;
					g_stGIF.m_nPixelHeight = static_cast<UINT>(g_stGIF.m_nHeight / pixelAspRatio);
				}
				else
				{
					g_stGIF.m_nPixelWidth = static_cast<UINT>(g_stGIF.m_nWidth * pixelAspRatio);
					g_stGIF.m_nPixelHeight = g_stGIF.m_nHeight;
				}
			}
			else
			{
				g_stGIF.m_nPixelWidth = g_stGIF.m_nWidth;
				g_stGIF.m_nPixelHeight = g_stGIF.m_nHeight;
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/appext/application", &stCOMPropValue))
			&& stCOMPropValue.vt == (VT_UI1 | VT_VECTOR)
			&& stCOMPropValue.caub.cElems == 11
			&& (!memcmp(stCOMPropValue.caub.pElems, "NETSCAPE2.0", stCOMPropValue.caub.cElems)
				||
				!memcmp(stCOMPropValue.caub.pElems, "ANIMEXTS1.0", stCOMPropValue.caub.cElems)))
		{
			PropVariantClear(&stCOMPropValue);

			if (
				SUCCEEDED(pIGIFMetadate->GetMetadataByName(L"/appext/data", &stCOMPropValue))
				&& stCOMPropValue.vt == (VT_UI1 | VT_VECTOR)
				&& stCOMPropValue.caub.cElems >= 4
				&& stCOMPropValue.caub.pElems[0] > 0
				&& stCOMPropValue.caub.pElems[1] == 1
				)
			{
				g_stGIF.m_nTotalLoopCount = MAKEWORD(stCOMPropValue.caub.pElems[2], stCOMPropValue.caub.pElems[3]);

				if (g_stGIF.m_nTotalLoopCount != 0)
				{
					g_stGIF.m_bHasLoop = TRUE;
				}
				else
				{
					g_stGIF.m_bHasLoop = FALSE;
				}
			}
		}
	}
	catch (CGRSCOMException& e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}

BOOL LoadGIFFrame(IWICImagingFactory* pIWICFactory, ST_GRS_GIF& g_stGIF, ST_GRS_GIF_FRAME& stGIFFrame)
{
	BOOL bRet = TRUE;
	try
	{
		WICPixelFormatGUID  stGuidRawFormat = {};
		WICPixelFormatGUID	stGuidTargetFormat = {};
		ComPtr<IWICFormatConverter>			pIWICConverter;
		ComPtr<IWICMetadataQueryReader>		pIWICMetadata;

		stGIFFrame.m_enTextureFormat = DXGI_FORMAT_UNKNOWN;
		//解码指定的帧
		GRS_THROW_IF_FAILED(g_stGIF.m_pIWICDecoder->GetFrame(g_stGIF.m_nCurrentFrame, &stGIFFrame.m_pIWICGIFFrame));
		//获取WIC图片格式
		GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrame->GetPixelFormat(&stGuidRawFormat));
		//通过第一道转换之后获取DXGI的等价格式
		if (GetTargetPixelFormat(&stGuidRawFormat, &stGuidTargetFormat))
		{
			stGIFFrame.m_enTextureFormat = GetDXGIFormatFromPixelFormat(&stGuidTargetFormat);
		}

		if (DXGI_FORMAT_UNKNOWN == stGIFFrame.m_enTextureFormat)
		{// 不支持的图片格式 目前退出了事 
		 // 一般 在实际的引擎当中都会提供纹理格式转换工具，
		 // 图片都需要提前转换好，所以不会出现不支持的现象
			throw CGRSCOMException(S_FALSE);
		}

		// 格式转换
		if (!InlineIsEqualGUID(stGuidRawFormat, stGuidTargetFormat))
		{// 这个判断很重要，如果原WIC格式不是直接能转换为DXGI格式的图片时
		 // 我们需要做的就是转换图片格式为能够直接对应DXGI格式的形式
			//创建图片格式转换器

			GRS_THROW_IF_FAILED(pIWICFactory->CreateFormatConverter(&pIWICConverter));

			//初始化一个图片转换器，实际也就是将图片数据进行了格式转换
			GRS_THROW_IF_FAILED(pIWICConverter->Initialize(
				stGIFFrame.m_pIWICGIFFrame.Get(),                // 输入原图片数据
				stGuidTargetFormat,						 // 指定待转换的目标格式
				WICBitmapDitherTypeNone,         // 指定位图是否有调色板，现代都是真彩位图，不用调色板，所以为None
				NULL,                            // 指定调色板指针
				0.f,                             // 指定Alpha阀值
				WICBitmapPaletteTypeCustom       // 调色板类型，实际没有使用，所以指定为Custom
			));
			// 调用QueryInterface方法获得对象的位图数据源接口
			GRS_THROW_IF_FAILED(pIWICConverter.As(&stGIFFrame.m_pIWICGIFFrameBMP));
		}
		else
		{
			//图片数据格式不需要转换，直接获取其位图数据源接口
			GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrame.As(&stGIFFrame.m_pIWICGIFFrameBMP));
		}
		// 读取GIF的详细信息
		GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrame->GetMetadataQueryReader(&pIWICMetadata));

		PROPVARIANT stCOMPropValue = {};
		PropVariantInit(&stCOMPropValue);

		// 获取帧延迟时间
		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/grctlext/Delay", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				GRS_THROW_IF_FAILED(UIntMult(stCOMPropValue.uiVal, 10, &stGIFFrame.m_nFrameDelay));
				if (0 == stGIFFrame.m_nFrameDelay)
				{
					stGIFFrame.m_nFrameDelay = 100;
				}
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/imgdesc/Left", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nLeftTop[0] = static_cast<UINT>(stCOMPropValue.uiVal);
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/imgdesc/Top", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nLeftTop[1] = static_cast<UINT>(stCOMPropValue.uiVal);
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/imgdesc/Width", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nFrameWH[0] = static_cast<UINT>(stCOMPropValue.uiVal);
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/imgdesc/Height", &stCOMPropValue)))
		{
			if (VT_UI2 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nFrameWH[1] = static_cast<UINT>(stCOMPropValue.uiVal);
			}
		}
		PropVariantClear(&stCOMPropValue);

		if (SUCCEEDED(pIWICMetadata->GetMetadataByName(L"/grctlext/Disposal", &stCOMPropValue)))
		{
			if (VT_UI1 == stCOMPropValue.vt)
			{
				stGIFFrame.m_nFrameDisposal = stCOMPropValue.bVal;
			}
		}
		else
		{
			stGIFFrame.m_nFrameDisposal = DM_UNDEFINED;
		}

		PropVariantClear(&stCOMPropValue);
	}
	catch (CGRSCOMException& e)
	{//发生了COM异常
		e;
		bRet = FALSE;
	}
	return bRet;

}

BOOL UploadGIFFrame(ID3D12Device4* pID3D12Device4, ID3D12GraphicsCommandList* pICMDList, IWICImagingFactory* pIWICFactory, ST_GRS_GIF_FRAME& stGIFFrame, ID3D12Resource** pIGIFFrame, ID3D12Resource** pIGIFFrameUpload)
{
	BOOL bRet = TRUE;
	try
	{
		UINT				nTextureW = 0;
		UINT				nTextureH = 0;
		UINT				nBPP = 0; // Bit Per Pixel
		UINT				nTextureRowPitch = 0;

		UINT64				n64TextureRowSizes = 0u;
		UINT64				n64UploadBufferSize = 0;
		UINT				nTextureRowNum = 0;
		D3D12_RESOURCE_DESC	stTextureDesc = {};
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT	stTxtLayouts = {};
		VOID* pGIFFrameData = nullptr;
		VOID* pUploadBufData = nullptr;

		//获得图片大小（单位：像素）
		GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrameBMP->GetSize(&nTextureW, &nTextureH));

		if (nTextureW != stGIFFrame.m_nFrameWH[0] || nTextureH != stGIFFrame.m_nFrameWH[1])
		{//从属性解析出的帧画面大小与转换后的画面大小不一致
			throw CGRSCOMException(S_FALSE);
		}

		//获取图片像素的位大小的BPP（Bits Per Pixel）信息，用以计算图片行数据的真实大小（单位：字节）
		ComPtr<IWICComponentInfo> pIWICmntinfo;
		WICPixelFormatGUID stGuidTargetFormat = {};
		stGIFFrame.m_pIWICGIFFrameBMP->GetPixelFormat(&stGuidTargetFormat);
		GRS_THROW_IF_FAILED(pIWICFactory->CreateComponentInfo(stGuidTargetFormat, pIWICmntinfo.GetAddressOf()));

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
		nTextureRowPitch = GRS_UPPER_DIV(nTextureW * nBPP, 8u);

		// 根据图片信息，填充2D纹理资源的信息结构体
		stTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		stTextureDesc.MipLevels = 1;
		stTextureDesc.Format = stGIFFrame.m_enTextureFormat;
		stTextureDesc.Width = nTextureW;
		stTextureDesc.Height = nTextureH;
		stTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		stTextureDesc.DepthOrArraySize = 1;
		stTextureDesc.SampleDesc.Count = 1;
		stTextureDesc.SampleDesc.Quality = 0;

		D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_DEFAULT };

		//创建默认堆上的资源，类型是Texture2D，GPU对默认堆资源的访问速度是最快的
		//因为纹理资源一般是不易变的资源，所以我们通常使用上传堆复制到默认堆中
		//在传统的D3D11及以前的D3D接口中，这些过程都被封装了，我们只能指定创建时的类型为默认堆 
		GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
			&stHeapProp
			, D3D12_HEAP_FLAG_NONE
			, &stTextureDesc				//可以使用CD3DX12_RESOURCE_DESC::Tex2D来简化结构体的初始化
			, D3D12_RESOURCE_STATE_COPY_DEST
			, nullptr
			, IID_PPV_ARGS(pIGIFFrame)));
		GRS_SET_D3D12_DEBUGNAME(*pIGIFFrame);

		//获取上传堆资源缓冲的大小，这个尺寸通常大于实际图片的尺寸
		stTextureDesc = (*pIGIFFrame)->GetDesc();

		// 这个方法是很有用的一个方法，从Buffer复制数据到纹理，或者从纹理复制数据到Buffer，这个方法都会告诉我们很多相关的辅助的信息
		// 其实主要的信息就是对于纹理来说行的大小，对齐方式等信息
		pID3D12Device4->GetCopyableFootprints(&stTextureDesc
			, 0
			, 1
			, 0
			, &stTxtLayouts
			, &nTextureRowNum
			, &n64TextureRowSizes
			, &n64UploadBufferSize);

		// 创建用于上传纹理的资源,注意其类型是Buffer
		// 上传堆对于GPU访问来说性能是很差的，
		// 所以对于几乎不变的数据尤其像纹理都是
		// 通过它来上传至GPU访问更高效的默认堆中
		stHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC stUploadBufDesc = {};
		stUploadBufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		stUploadBufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		stUploadBufDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		stUploadBufDesc.Format = DXGI_FORMAT_UNKNOWN;
		stUploadBufDesc.Width = n64UploadBufferSize;
		stUploadBufDesc.Height = 1;
		stUploadBufDesc.DepthOrArraySize = 1;
		stUploadBufDesc.MipLevels = 1;
		stUploadBufDesc.SampleDesc.Count = 1;
		stUploadBufDesc.SampleDesc.Quality = 0;

		GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
			&stHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&stUploadBufDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(pIGIFFrameUpload)));
		GRS_SET_D3D12_DEBUGNAME(*pIGIFFrameUpload);

		//按照资源缓冲大小来分配实际图片数据存储的内存大小
		pGIFFrameData = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, n64UploadBufferSize);
		if (nullptr == pGIFFrameData)
		{
			throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));
		}

		//从图片中读取出数据
		GRS_THROW_IF_FAILED(stGIFFrame.m_pIWICGIFFrameBMP->CopyPixels(nullptr
			, nTextureRowPitch
			, static_cast<UINT>(nTextureRowPitch * nTextureH)   //注意这里才是图片数据真实的大小，这个值通常小于缓冲的大小
			, reinterpret_cast<BYTE*>(pGIFFrameData)));

		//--------------------------------------------------------------------------------------------------------------
		// 第一个Copy命令，从内存复制到上传堆（共享内存）中
		//因为上传堆实际就是CPU传递数据到GPU的中介
		//所以我们可以使用熟悉的Map方法将它先映射到CPU内存地址中
		//然后我们按行将数据复制到上传堆中
		//需要注意的是之所以按行拷贝是因为GPU资源的行大小
		//与实际图片的行大小是有差异的,二者的内存边界对齐要求是不一样的
		GRS_THROW_IF_FAILED((*pIGIFFrameUpload)->Map(0, NULL, reinterpret_cast<void**>(&pUploadBufData)));

		BYTE* pDestSlice = reinterpret_cast<BYTE*>(pUploadBufData) + stTxtLayouts.Offset;
		const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>(pGIFFrameData);
		for (UINT y = 0; y < nTextureRowNum; ++y)
		{
			// 第一个Copy命令，由CPU完成，将数据从内存复制到上传堆（共享内存）中
			memcpy(pDestSlice + static_cast<SIZE_T>(stTxtLayouts.Footprint.RowPitch) * y
				, pSrcSlice + static_cast<SIZE_T>(nTextureRowPitch) * y
				, nTextureRowPitch);
		}
		//取消映射 对于易变的数据如每帧的变换矩阵等数据，可以撒懒不用Unmap了，
		//让它常驻内存,以提高整体性能，因为每次Map和Unmap是很耗时的操作
		//因为现在起码都是64位系统和应用了，地址空间是足够的，被长期占用不会影响什么
		(*pIGIFFrameUpload)->Unmap(0, NULL);

		//释放图片数据，做一个干净的程序员
		::HeapFree(::GetProcessHeap(), 0, pGIFFrameData);
		//--------------------------------------------------------------------------------------------------------------

		//--------------------------------------------------------------------------------------------------------------
		// 第二个Copy命令，由GPU的Copy Engine完成，从上传堆（共享内存）复制纹理数据到默认堆（显存）中
		D3D12_TEXTURE_COPY_LOCATION stDst = {};
		stDst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		stDst.pResource = *pIGIFFrame;
		stDst.SubresourceIndex = 0;

		D3D12_TEXTURE_COPY_LOCATION stSrc = {};
		stSrc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		stSrc.pResource = *pIGIFFrameUpload;
		stSrc.PlacedFootprint = stTxtLayouts;

		//第二个Copy动作由GPU来完成，将Texture数据从上传堆（共享内存）复制到默认堆（显存）中
		pICMDList->CopyTextureRegion(&stDst, 0, 0, 0, &stSrc, nullptr);
		//--------------------------------------------------------------------------------------------------------------

		// 设置一个资源屏障，默认堆上的纹理数据复制完成后，将纹理的状态从复制目标转换为Shader资源
		// 这里需要注意的是在这个例子中，状态转换后的纹理只用于Pixel Shader访问，其他的Shader没法访问
		D3D12_RESOURCE_BARRIER stResBar = {};
		stResBar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		stResBar.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		stResBar.Transition.pResource = *pIGIFFrame;
		stResBar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		stResBar.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
		stResBar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

		pICMDList->ResourceBarrier(1, &stResBar);
	}
	catch (CGRSCOMException& e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices)
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

		ppVertex = (ST_GRS_VERTEX*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(ST_GRS_VERTEX));
		ppIndices = (UINT*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(UINT));

		for (UINT i = 0; i < nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_v4Position.x >> ppVertex[i].m_v4Position.y >> ppVertex[i].m_v4Position.z;
			ppVertex[i].m_v4Position.w = 1.0f;
			fin >> ppVertex[i].m_vTxc.x >> ppVertex[i].m_vTxc.y;
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

		if (VK_TAB == n16KeyCode)
		{// 按Tab键切换选择另外的GIF来显示
			// 1、消息循环旁路
			if ( WAIT_OBJECT_0 != ::WaitForSingleObject(g_hEventFence, INFINITE) )
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			OPENFILENAME ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = L"*Gif 文件\0*.gif\0";
			ofn.lpstrFile = g_pszTexcuteFileName;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrTitle = L"请选择一个要显示的GIF文件...";
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

			if (!GetOpenFileName(&ofn))
			{
				StringCchPrintfW(g_pszTexcuteFileName, MAX_PATH, _T("%sAssets\\MM.gif"), g_pszAppPath);
			}
			else
			{
				// 注意这里清理了全局变量g_stGIF，但这不会引起错误
				// 原因是使用了MsgWaitForMultipleObjects函数之后，消息处理和渲染实际上被分支(旁路)了
				// 即处理消息时，渲染过程用不到这个变量，当CPU渲染过程时，肯定不会处理这个消息
				g_stGIF.m_pIWICDecoder.Reset();
				g_stGIF.m_nTotalLoopCount = 0;
				g_stGIF.m_nLoopNumber = 0;
				g_stGIF.m_bHasLoop = 0;
				g_stGIF.m_nFrames = 0;
				g_stGIF.m_nCurrentFrame = 0;
				g_stGIF.m_nBkColor = 0;
				g_stGIF.m_nWidth = 0;
				g_stGIF.m_nHeight = 0;
				g_stGIF.m_nPixelWidth = 0;
				g_stGIF.m_nPixelHeight = 0;
				//只载入文件，但不生成纹理，在渲染循环中再生成纹理
				if (!LoadGIF(g_pszTexcuteFileName, g_pIWICFactory.Get(), g_stGIF))
				{
					throw CGRSCOMException(E_FAIL);
				}

				//重新创建一下RWTexture，也就是我们的“画板” 
				D3D12_RESOURCE_DESC stRWTextureDesc = {};
				stRWTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				stRWTextureDesc.MipLevels = 1;
				stRWTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				stRWTextureDesc.Width = g_stGIF.m_nPixelWidth;
				stRWTextureDesc.Height = g_stGIF.m_nPixelHeight;
				stRWTextureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				stRWTextureDesc.DepthOrArraySize = 1;
				stRWTextureDesc.SampleDesc.Count = 1;
				stRWTextureDesc.SampleDesc.Quality = 0;

				D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_DEFAULT };

				// 这里可能会引起问题，即我们释放了RW纹理资源，但可能GPU还在使用中
				// 虽然消息处理和渲染过程被旁路了，但是消息处理是CPU的事情，而渲染是GPU的事情
				// 这俩货现在可是并行工作哦，看你会不会碰到报错,
				// 当然改正的办法就是调用WaitForSingleObject方法等待一下GPU工作完了再继续
				// 我比较懒，先不改了，等有空再改咯，当然你能看明白我在说什么，那么这个潜在的Bug就自己动手改了吧
				// ---------------------------------------------------------------------------------------------
				// 以上是之前的注释，本想偷懒，可没想到我就先遇到问题了，当然不是每次都会发生错误
				// 解决的方法当时Wait一下Fence Event，这里依然使用最懒的方法，就是在上面等个20ms完事
				// 这个等待不能是INFINITE，如果没有渲染也没有设置这个事件句柄给Fence的话，它永远不会有信号，那样等待就像死锁了一样

				//DWORD dwRet = WaitForSingleObject(g_hEventFence, 40);

				g_pIRWTexture.Reset();

				GRS_THROW_IF_FAILED(g_pID3D12Device4->CreateCommittedResource(
					&stHeapProp
					, D3D12_HEAP_FLAG_NONE
					, &stRWTextureDesc
					, D3D12_RESOURCE_STATE_COMMON
					, nullptr
					, IID_PPV_ARGS(&g_pIRWTexture)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(g_pIRWTexture);

				D3D12_CPU_DESCRIPTOR_HANDLE stSRVHandle = g_pICSSRVHeap->GetCPUDescriptorHandleForHeapStart();
				stSRVHandle.ptr += 2 * g_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				// Computer Shader UAV
				D3D12_UNORDERED_ACCESS_VIEW_DESC stUAVDesc = {};
				stUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				stUAVDesc.Format = g_pIRWTexture->GetDesc().Format;
				stUAVDesc.Texture2D.MipSlice = 0;

				g_pID3D12Device4->CreateUnorderedAccessView(g_pIRWTexture.Get(), nullptr, &stUAVDesc, stSRVHandle);

				//这里Set一下，实质上就是消除了刚刚那个Wait的副作用，因为我们主消息循环里也是Wait这个Event Handle来保持循环状态的
				//if (WAIT_OBJECT_0 == dwRet)
				//{
					SetEvent(g_hEventFence);
				//}
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
		}

		if (VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode)
		{
			g_fPalstance -= 10 * XM_PI / 180.0f;
			if (g_fPalstance < 0.0f)
			{
				g_fPalstance = XM_PI / 180.0f;
			}
		}
	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

