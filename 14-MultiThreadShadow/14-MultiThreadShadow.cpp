#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <wrl.h> //添加WTL支持 方便使用COM

#include <atlconv.h>
#include <atlcoll.h>  //for atl array
#include <strsafe.h>  //for StringCchxxxxx function

#include <dxgi1_6.h>
#include <d3d12.h> //for d3d12
#include <d3dcompiler.h>

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include <DirectXMath.h>
#include "..\WindowsCommons\DDSTextureLoader12.h"

//using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#ifndef GRS_BLOCK

#define GRS_WND_CLASS_NAME _T("GRS Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 Multi Thread Shadow Sample")

#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ throw CGRSCOMException(_hr); }}

//新定义的宏用于上取整除法
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

// 内存分配的宏定义
#define GRS_ALLOC(sz)				::HeapAlloc(GetProcessHeap(),0,(sz))
#define GRS_CALLOC(sz)			::HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(sz))
#define GRS_CREALLOC(p,sz)		::HeapReAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,(p),(sz))
#define GRS_SAFE_FREE(p)			if( nullptr != (p) ){ ::HeapFree( ::GetProcessHeap(),0,(p) ); (p) = nullptr; }
//------------------------------------------------------------------------------------------------------------
// 为了调试加入下面的内联函数和宏定义，为每个接口对象设置名称，方便查看调试输出
#if defined(_DEBUG)
inline void GRS_SetD3D12DebugName( ID3D12Object* pObject, LPCWSTR name )
{
    pObject->SetName( name );
}

inline void GRS_SetD3D12DebugNameIndexed( ID3D12Object* pObject, LPCWSTR name, UINT index )
{
    WCHAR _DebugName[MAX_PATH] = {};
    if ( SUCCEEDED( StringCchPrintfW( _DebugName, _countof( _DebugName ), L"%s[%u]", name, index ) ) )
    {
        pObject->SetName( _DebugName );
    }
}
#else

inline void GRS_SetD3D12DebugName( ID3D12Object*, LPCWSTR )
{
}
inline void GRS_SetD3D12DebugNameIndexed( ID3D12Object*, LPCWSTR, UINT )
{
}

#endif

#define GRS_SET_D3D12_DEBUGNAME(x)						GRS_SetD3D12DebugName(x, L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED(x, n)			GRS_SetD3D12DebugNameIndexed(x[n], L#x, n)

#define GRS_SET_D3D12_DEBUGNAME_COMPTR(x)				GRS_SetD3D12DebugName(x.Get(), L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(x, n)	GRS_SetD3D12DebugNameIndexed(x[n].Get(), L#x, n)

#if defined(_DEBUG)
inline void GRS_SetDXGIDebugName( IDXGIObject* pObject, LPCWSTR name )
{
    size_t szLen = 0;
    StringCchLengthW( name, MAX_PATH, &szLen );
    pObject->SetPrivateData( WKPDID_D3DDebugObjectName, static_cast<UINT>( szLen - 1 ), name );
}

inline void GRS_SetDXGIDebugNameIndexed( IDXGIObject* pObject, LPCWSTR name, UINT index )
{
    size_t szLen = 0;
    WCHAR _DebugName[MAX_PATH] = {};
    if ( SUCCEEDED( StringCchPrintfW( _DebugName, _countof( _DebugName ), L"%s[%u]", name, index ) ) )
    {
        StringCchLengthW( _DebugName, _countof( _DebugName ), &szLen );
        pObject->SetPrivateData( WKPDID_D3DDebugObjectName, static_cast<UINT>( szLen ), _DebugName );
    }
}
#else

inline void GRS_SetDXGIDebugName( IDXGIObject*, LPCWSTR )
{
}
inline void GRS_SetDXGIDebugNameIndexed( IDXGIObject*, LPCWSTR, UINT )
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
    CGRSCOMException( HRESULT hr ) : m_hrError( hr )
    {
    }
    HRESULT Error() const
    {
        return m_hrError;
    }
private:
    const HRESULT m_hrError;
};
#endif // !GRS_BLOCK

// 顶点结构
struct ST_GRS_VERTEX
{
public:
    XMFLOAT4 m_v4Position;	//m_v4Position
    XMFLOAT4 m_v4Normal;	//m_v4Normal
    XMFLOAT2 m_v2Texcoord;	//Texcoord
    XMFLOAT4 m_v4Tangent;	//Tangent
public:
    ST_GRS_VERTEX() :
        m_v4Position( 0.0f, 0.0f, 0.0f, 1.0f ),
        m_v4Normal( 0.0f, 0.0f, 0.0f, 0.0f ),
        m_v4Tangent( 0.0f, 0.0f, 0.0f, 0.0f ),
        m_v2Texcoord( 0.0f, 0.0f )
    {
    }
    ST_GRS_VERTEX( float px, float py, float pz,
        float nx, float ny, float nz,
        float tx, float ty, float tz,
        float u, float v ) :
        m_v4Position( px, py, pz, 1.0f )
        , m_v4Normal( nx, ny, nz, 0.0f )
        , m_v4Tangent( tx, ty, tz, 0.0f )
        , m_v2Texcoord( u, v )
    {
    }
    ST_GRS_VERTEX( const ST_GRS_VERTEX& stV )
        :
        m_v4Position( stV.m_v4Position )
        , m_v4Normal( stV.m_v4Normal )
        , m_v4Tangent( stV.m_v4Tangent )
        , m_v2Texcoord( stV.m_v2Texcoord )
    {
    }
public:
    ST_GRS_VERTEX& operator = ( const ST_GRS_VERTEX& stV )
    {
        m_v4Position = stV.m_v4Position;
        m_v4Normal = stV.m_v4Normal;
        m_v4Tangent = stV.m_v4Tangent;
        m_v2Texcoord = stV.m_v2Texcoord;
        return *this;
    }
};

// 常量缓冲区
struct ST_GRS_MVP
{
    XMFLOAT4X4 g_mxModel;
    XMFLOAT4X4 g_mxView;
    XMFLOAT4X4 g_mxProjection;
};

// 矩形框的顶点结构
struct ST_GRS_VERTEX_QUAD
{//这个结构体改编自HGE引擎，作为一个纪念吧
    XMFLOAT4 m_v4Position;	//Position
    XMFLOAT4 m_vClr;		//Color
    XMFLOAT2 m_vTxc;		//Texcoord
};

// 正交投影的常量缓冲
struct ST_GRS_CB_MVO
{
    XMFLOAT4X4 m_mMVO;	   //Model * View * Orthographic 
};

// 光源状态结构体
struct ST_GRS_LIGHT
{
    XMFLOAT4 m_v4Position;
    XMFLOAT4 m_v4Direction;
    XMFLOAT4 m_v4Color;
    XMFLOAT4 m_v4Falloff;
    XMFLOAT4X4 m_mxView;
    XMFLOAT4X4 m_mxProjection;
};

//光源个数
#define GRS_NUM_LIGHTS 3

// 本例中使用的全局光源结构体
struct ST_GRS_LIGHTBUFFER
{
    XMFLOAT4 m_v4AmbientColor;
    ST_GRS_LIGHT m_stLights[GRS_NUM_LIGHTS];
    UINT m_bIsSampleShadowMap;
};

//渲染状态变量
enum EM_GRS_RENDER_STATUS
{
    EM_GRS_RS_GPUCOPY = 0x0,
    EM_GRS_RS_STARTSHADOW = 0x1,
    EM_GRS_RS_STARTRENDER = 0x2,
    EM_GRS_RS_END = 0x3
};

// 渲染子线程参数
struct ST_GRS_THREAD_PARAMS
{
    UINT										m_nThreadIndex;				//序号
    DWORD									m_dwThisThreadID;
    HANDLE									m_hThisThread;
    DWORD									m_dwMainThreadID;
    HANDLE									m_hMainThread;

    HANDLE									m_hRunEvent;
    HANDLE									m_hEventShadowOver;
    HANDLE									m_hEventRenderOver;

    UINT										m_nCurrentFrameIndex;	//当前渲染后缓冲序号
    ULONGLONG							m_nStartTime;					//当前帧开始时间
    ULONGLONG							m_nCurrentTime;				//当前时间
    EM_GRS_RENDER_STATUS		m_nState;							//当前渲染状态 1 渲染阴影 2 正常渲染

    XMFLOAT4								m_v4ModelPos;
    TCHAR										m_pszDiffuseFile[MAX_PATH];
    TCHAR										m_pszNormalFile[MAX_PATH];

    ID3D12Device4* m_pID3D12Device4;

    ID3D12CommandAllocator* m_pICmdAllocShadow;
    ID3D12GraphicsCommandList* m_pICmdListShadow;
    ID3D12CommandAllocator* m_pICmdAlloc;
    ID3D12GraphicsCommandList* m_pICmdList;

    ID3D12RootSignature* m_pIRSShadowPass;
    ID3D12PipelineState* m_pIPSOShadowPass;
    ID3D12RootSignature* m_pIRSSecondPass;
    ID3D12PipelineState* m_pIPSOSecondPass;

    ID3D12DescriptorHeap* m_pISampleHeap;

    ID3D12Resource* m_pITexShadowMap;
    ID3D12Resource* m_pICBLights;
};

// 窗口大小
int                             g_iWndWidth = 1024;
int                             g_iWndHeight = 768;

// 阴影用的深度缓冲的分辨率
int                             g_iDepthWidth = 1024;
int                             g_iDepthHeight = 1024;

D3D12_VIEWPORT					g_stViewPort = { 0.0f, 0.0f, static_cast<float>( g_iWndWidth ), static_cast<float>( g_iWndHeight ) , D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
D3D12_VIEWPORT					g_stDepthViewPort = { 0.0f, 0.0f, static_cast<float>( g_iWndWidth ), static_cast<float>( g_iWndHeight ) , D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
D3D12_RECT						g_stScissorRect = { 0, 0, static_cast<LONG>( g_iWndWidth ), static_cast<LONG>( g_iWndHeight ) };

//初始的默认摄像机的位置
XMFLOAT3						g_f3EyePos = XMFLOAT3( 0.0f, 5.0f, -10.0f );  //眼睛位置
XMFLOAT3						g_f3HeapUp = XMFLOAT3( 0.0f, 1.0f, 0.0f );    //头部正上方位置
XMFLOAT3						g_f3LockAt = XMFLOAT3( 0.0f, 0.0f, 0.0f );    //眼睛所盯的位置

float							g_fYaw = 0.0f;			// 绕正Z轴的旋转量.
float							g_fPitch = 0.0f;			// 绕XZ平面的旋转量

double							g_fPalstance = 3.0f * XM_PI / 180.0f;	//物体旋转的角速度，单位：弧度/秒

XMFLOAT4X4						g_mxWorld = {}; //World Matrix
XMFLOAT4X4						g_mxView = {}; //World Matrix
XMFLOAT4X4						g_mxProjection = {}; //World Matrix

// 全局线程参数
const UINT						g_nMaxThread = 3;
const UINT						g_nThdSphere = 0;
const UINT						g_nThdCube = 1;
const UINT						g_nThdPlane = 2;
ST_GRS_THREAD_PARAMS		    g_stThreadParams[g_nMaxThread] = {};

ST_GRS_LIGHTBUFFER*             g_pstLights = nullptr;

const UINT						g_nFrameBackBufCount = 3u;
UINT							g_nRTVDescriptorSize = 0U;
UINT							g_nSRVDescriptorSize = 0U;
UINT                            g_nDSVDescriptorSize = 0U;

ComPtr<ID3D12Resource>			g_pIARenderTargets[g_nFrameBackBufCount];

ComPtr<ID3D12DescriptorHeap>	g_pIRTVHeap;
ComPtr<ID3D12DescriptorHeap>	g_pIDSVHeap;				//深度缓冲描述符堆

TCHAR							g_pszAppPath[MAX_PATH] = {};

typedef CAtlArray<ST_GRS_VERTEX> CGRSMeshVertex;
typedef CAtlArray<UINT32>		 CGRSMeshIndex;

UINT __stdcall RenderThread( void* pParam );
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );

BOOL LoadBox( FLOAT width, FLOAT height, FLOAT depth, UINT32 numSubdivisions, CGRSMeshVertex& arVertex, CGRSMeshIndex& arIndices );
BOOL LoadSphere( FLOAT radius, UINT32 sliceCount, UINT32 stackCount, CGRSMeshVertex& arVertex, CGRSMeshIndex& arIndices );
BOOL LoadGrid( float width, float depth, UINT32 m, UINT32 n, CGRSMeshVertex& arVertex, CGRSMeshIndex& arIndices );

int APIENTRY _tWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow )
{
    ::CoInitialize( nullptr );  //for WIC & COM

    HWND								hWnd = nullptr;
    MSG									msg = {};

    const float							faClearColor[] = { 0.2f, 0.5f, 1.0f, 1.0f };

    ComPtr<IDXGIFactory5>				pIDXGIFactory5;
    ComPtr<IDXGIFactory6>				pIDXGIFactory6;
    ComPtr<IDXGIAdapter1>				pIAdapter1;
    ComPtr<ID3D12Device4>				pID3D12Device4;

    ComPtr<ID3D12CommandQueue>			pIMainCmdQueue;

    D3D12_FEATURE_DATA_ROOT_SIGNATURE	stFeatureData = {};

    ComPtr<ID3D12Fence>					pIFence;
    UINT64								n64FenceValue = 1ui64;
    HANDLE								hEventFence = nullptr;

    ComPtr<IDXGISwapChain1>				pISwapChain1;
    ComPtr<IDXGISwapChain3>				pISwapChain3;

    ComPtr<ID3D12Resource>				pIDepthShadowBuffer;	//阴影用的深度缓冲区
    ComPtr<ID3D12Resource>				pIDepthStencilBuffer;   //正常渲染的深度蜡板缓冲区

    UINT								nCurrentFrameIndex = 0;
    DXGI_FORMAT							emRTFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT							emDSFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DXGI_FORMAT							emDepthShadowFormat = DXGI_FORMAT_D32_FLOAT;

    ComPtr<ID3D12CommandAllocator>		pICmdAllocPre;
    ComPtr<ID3D12GraphicsCommandList>	pICmdListPre;

    ComPtr<ID3D12CommandAllocator>		pICmdAllocMid;
    ComPtr<ID3D12GraphicsCommandList>	pICmdListMid;

    ComPtr<ID3D12CommandAllocator>		pICmdAllocPost;
    ComPtr<ID3D12GraphicsCommandList>	pICmdListPost;

    CAtlArray<HANDLE>					arHWaited;
    CAtlArray<HANDLE>					arHSubThread;

    ComPtr<ID3D12RootSignature>			pIRSShadowPass;
    ComPtr<ID3D12PipelineState>			pIPSOShadowPass;
    ComPtr<ID3D12RootSignature>			pIRSSecondPass;
    ComPtr<ID3D12PipelineState>			pIPSOSecondPass;
    ComPtr<ID3D12DescriptorHeap>		pISampleHeap;

    //--------------------------------------------------------------------------------------------------------
    // 将阴影深度视图以UI小窗口形式显示出来需要的变量
    ComPtr<ID3D12CommandAllocator>		pICmdAllocQuad;
    ComPtr<ID3D12GraphicsCommandList>	pICmdBundlesQuad;
    ComPtr<ID3D12RootSignature>			pIRSQuad;
    ComPtr<ID3D12PipelineState>			pIPSOQuad;
    ComPtr<ID3D12Resource>				pIVBQuad;
    ComPtr<ID3D12Resource>			    pICBMVO;	//常量缓冲
    ComPtr<ID3D12DescriptorHeap>		pISRVHeapQuad;
    ComPtr<ID3D12DescriptorHeap>		pISampleHeapQuad;
    D3D12_VERTEX_BUFFER_VIEW			stVBViewQuad = {};
    ST_GRS_CB_MVO* pMOV = nullptr;
    SIZE_T								szCBMOVBuf = GRS_UPPER( sizeof( ST_GRS_CB_MVO ), 256 );
    //--------------------------------------------------------------------------------------------------------

    ComPtr<ID3D12Resource>			    pICBLights;
    SIZE_T								szLightBuf = GRS_UPPER( sizeof( ST_GRS_LIGHTBUFFER ), 256 );

    float								fNearPlane = 1.0f;
    float								fFarPlane = 125.0f;

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

    D3D12_RESOURCE_BARRIER stResStateTransBarrier = {};
    stResStateTransBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    stResStateTransBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    stResStateTransBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    try
    {
        // 0、得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
        {
            if ( 0 == ::GetModuleFileName( nullptr, g_pszAppPath, MAX_PATH ) )
            {
                GRS_THROW_IF_FAILED( HRESULT_FROM_WIN32( GetLastError() ) );
            }

            WCHAR* lastSlash = _tcsrchr( g_pszAppPath, _T( '\\' ) );
            if ( lastSlash )
            {//删除Exe文件名
                *( lastSlash ) = _T( '\0' );
            }

            lastSlash = _tcsrchr( g_pszAppPath, _T( '\\' ) );
            if ( lastSlash )
            {//删除x64路径
                *( lastSlash ) = _T( '\0' );
            }

            lastSlash = _tcsrchr( g_pszAppPath, _T( '\\' ) );
            if ( lastSlash )
            {//删除Debug 或 Release路径
                *( lastSlash + 1 ) = _T( '\0' );
            }
        }

        // 1、创建窗口
        {
            WNDCLASSEX wcex = {};
            wcex.cbSize = sizeof( WNDCLASSEX );
            wcex.style = CS_GLOBALCLASS;
            wcex.lpfnWndProc = WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = hInstance;
            wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
            wcex.hbrBackground = (HBRUSH) GetStockObject( NULL_BRUSH );		//防止无聊的背景重绘
            wcex.lpszClassName = GRS_WND_CLASS_NAME;
            RegisterClassEx( &wcex );

            DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
            RECT rtWnd = { 0, 0, g_iWndWidth, g_iWndHeight };
            AdjustWindowRect( &rtWnd, dwWndStyle, FALSE );

            // 计算窗口居中的屏幕坐标
            INT posX = ( GetSystemMetrics( SM_CXSCREEN ) - rtWnd.right - rtWnd.left ) / 2;
            INT posY = ( GetSystemMetrics( SM_CYSCREEN ) - rtWnd.bottom - rtWnd.top ) / 2;

            hWnd = CreateWindowW( GRS_WND_CLASS_NAME, GRS_WND_TITLE, dwWndStyle
                , posX, posY, rtWnd.right - rtWnd.left, rtWnd.bottom - rtWnd.top
                , nullptr, nullptr, hInstance, nullptr );

            if ( !hWnd )
            {
                throw CGRSCOMException( HRESULT_FROM_WIN32( GetLastError() ) );
            }
        }

        // 2、创建DXGI Factory对象
        {
            UINT nDXGIFactoryFlags = 0U;
#if defined(_DEBUG)
            // 打开显示子系统的调试支持
            ComPtr<ID3D12Debug> debugController;
            if ( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( &debugController ) ) ) )
            {
                debugController->EnableDebugLayer();
                // 打开附加的调试支持
                nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
#endif		
            GRS_THROW_IF_FAILED( CreateDXGIFactory2( nDXGIFactoryFlags, IID_PPV_ARGS( &pIDXGIFactory5 ) ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIDXGIFactory5 );
            //获取IDXGIFactory6接口
            GRS_THROW_IF_FAILED( pIDXGIFactory5.As( &pIDXGIFactory6 ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIDXGIFactory6 );
        }

        // 3、枚举适配器创建设备
        {//通过最新的DXGI EnumAdapterByGpuPreference接口找到系统中性能最强的显卡来创建设备

            GRS_THROW_IF_FAILED( pIDXGIFactory6->EnumAdapterByGpuPreference( 0
                , DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                , IID_PPV_ARGS( &pIAdapter1 ) ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIAdapter1 );

            // 创建D3D12.1的设备
            GRS_THROW_IF_FAILED( D3D12CreateDevice( pIAdapter1.Get()
                , D3D_FEATURE_LEVEL_12_1
                , IID_PPV_ARGS( &pID3D12Device4 ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pID3D12Device4 );


            DXGI_ADAPTER_DESC1 stAdapterDesc = {};
            TCHAR pszWndTitle[MAX_PATH] = {};
            GRS_THROW_IF_FAILED( pIAdapter1->GetDesc1( &stAdapterDesc ) );
            ::GetWindowText( hWnd, pszWndTitle, MAX_PATH );
            StringCchPrintf( pszWndTitle
                , MAX_PATH
                , _T( "%s(GPU[0]:%s)" )
                , pszWndTitle
                , stAdapterDesc.Description );
            ::SetWindowText( hWnd, pszWndTitle );
        }

        // 4、创建命令队列及命令列表
        {
            D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
            stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandQueue( &stQueueDesc, IID_PPV_ARGS( &pIMainCmdQueue ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCmdQueue );

            // 创建直接命令列表
            // 预处理命令列表
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT
                , IID_PPV_ARGS( &pICmdAllocPre ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICmdAllocPre );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT
                , pICmdAllocPre.Get(), nullptr, IID_PPV_ARGS( &pICmdListPre ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICmdListPre );

            // 中间处理命令列表
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT
                , IID_PPV_ARGS( &pICmdAllocMid ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICmdAllocMid );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT
                , pICmdAllocMid.Get(), nullptr, IID_PPV_ARGS( &pICmdListMid ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICmdListMid );

            //后处理命令列表
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT
                , IID_PPV_ARGS( &pICmdAllocPost ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICmdAllocPost );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT
                , pICmdAllocPost.Get(), nullptr, IID_PPV_ARGS( &pICmdListPost ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICmdListPost );
        }

        // 5、创建围栏对象
        {
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &pIFence ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIFence );

            //创建一个Event同步对象，用于等待围栏事件通知
            hEventFence = CreateEvent( nullptr, FALSE, FALSE, nullptr );
            if ( hEventFence == nullptr )
            {
                GRS_THROW_IF_FAILED( HRESULT_FROM_WIN32( GetLastError() ) );
            }
        }

        // 6、创建交换链
        {
            DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
            stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
            stSwapChainDesc.Width = g_iWndWidth;
            stSwapChainDesc.Height = g_iWndHeight;
            stSwapChainDesc.Format = emRTFormat;
            stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            stSwapChainDesc.SampleDesc.Count = 1;

            GRS_THROW_IF_FAILED( pIDXGIFactory5->CreateSwapChainForHwnd(
                pIMainCmdQueue.Get(),		// Swap chain needs the queue so that it can force a flush on it.
                hWnd,
                &stSwapChainDesc,
                nullptr,
                nullptr,
                &pISwapChain1
            ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pISwapChain1 );

            //注意此处使用了高版本的SwapChain接口的函数
            GRS_THROW_IF_FAILED( pISwapChain1.As( &pISwapChain3 ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pISwapChain3 );

            // 获取当前第一个供绘制的后缓冲序号
            nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

            //创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
            D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
            stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;
            stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stRTVHeapDesc, IID_PPV_ARGS( &g_pIRTVHeap ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( g_pIRTVHeap );

            //得到每个描述符元素的大小
            g_nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
            g_nSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
            g_nDSVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );
            //---------------------------------------------------------------------------------------------
            D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = g_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
            for ( UINT i = 0; i < g_nFrameBackBufCount; i++ )
            {//这个循环暴漏了描述符堆实际上是个数组的本质
                GRS_THROW_IF_FAILED( pISwapChain3->GetBuffer( i, IID_PPV_ARGS( &g_pIARenderTargets[i] ) ) );
                GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR( g_pIARenderTargets, i );
                pID3D12Device4->CreateRenderTargetView( g_pIARenderTargets[i].Get(), nullptr, stRTVHandle );
                stRTVHandle.ptr += g_nRTVDescriptorSize;
            }

            // 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
            GRS_THROW_IF_FAILED( pIDXGIFactory5->MakeWindowAssociation( hWnd, DXGI_MWA_NO_ALT_ENTER ) );
        }

        // 7、创建深度缓冲及深度缓冲描述符堆
        {
            D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
            stDepthOptimizedClearValue.Format = emDepthShadowFormat;
            stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
            stDepthOptimizedClearValue.DepthStencil.Stencil = 0;
            
                
            D3D12_RESOURCE_DESC stDepthShadowResDesc = {};
            stDepthShadowResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            stDepthShadowResDesc.Alignment = 0;
            stDepthShadowResDesc.Format = emDepthShadowFormat;
            stDepthShadowResDesc.Width = g_iDepthWidth;
            stDepthShadowResDesc.Height = g_iDepthHeight;
            stDepthShadowResDesc.DepthOrArraySize = 1;
            stDepthShadowResDesc.MipLevels = 0;
            stDepthShadowResDesc.SampleDesc.Count = 1;
            stDepthShadowResDesc.SampleDesc.Quality = 0;
            stDepthShadowResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            stDepthShadowResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            //创建用于阴影渲染的深度缓冲区
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stDefautHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stDepthShadowResDesc
                , D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                , &stDepthOptimizedClearValue
                , IID_PPV_ARGS( &pIDepthShadowBuffer )
            ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIDepthShadowBuffer );

            stDepthOptimizedClearValue.Format = emDSFormat;

            D3D12_RESOURCE_DESC stDSResDesc = {};
            stDSResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            stDSResDesc.Alignment = 0;
            stDSResDesc.Format = emDSFormat;
            stDSResDesc.Width = g_iWndWidth;
            stDSResDesc.Height = g_iWndHeight;
            stDSResDesc.DepthOrArraySize = 1;
            stDSResDesc.MipLevels = 0;
            stDSResDesc.SampleDesc.Count = 1;
            stDSResDesc.SampleDesc.Quality = 0;
            stDSResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            stDSResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

            // 创建第二遍正常渲染用的深度蜡板缓冲区
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stDefautHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stDSResDesc
                , D3D12_RESOURCE_STATE_DEPTH_WRITE
                , &stDepthOptimizedClearValue
                , IID_PPV_ARGS( &pIDepthStencilBuffer )
            ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIDepthStencilBuffer );


            D3D12_DESCRIPTOR_HEAP_DESC stDSVHeapDesc = {};
            stDSVHeapDesc.NumDescriptors = 2;		// 1 Shadow Depth View + 1 Depth Stencil View
            stDSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            stDSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stDSVHeapDesc, IID_PPV_ARGS( &g_pIDSVHeap ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( g_pIDSVHeap );

            D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
            stDepthStencilDesc.Format = emDepthShadowFormat;
            stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

            D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle( g_pIDSVHeap->GetCPUDescriptorHandleForHeapStart() );

            pID3D12Device4->CreateDepthStencilView( pIDepthShadowBuffer.Get(), &stDepthStencilDesc, stDSVHandle );

            stDSVHandle.ptr += g_nDSVDescriptorSize;

            stDepthStencilDesc.Format = emDSFormat;

            pID3D12Device4->CreateDepthStencilView( pIDepthStencilBuffer.Get(), &stDepthStencilDesc, stDSVHandle );

        }

        // 8、创建根签名
        {//这个例子中，所有物体使用相同的根签名，因为渲染过程中需要的参数是一样的
            // 检测是否支持V1.1版本的根签名
            stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
            if ( FAILED( pID3D12Device4->CheckFeatureSupport( D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof( stFeatureData ) ) ) )
            {// 1.0版 直接丢异常退出了
                GRS_THROW_IF_FAILED( E_NOTIMPL );
            }

            D3D12_DESCRIPTOR_RANGE1 stDSPRanges[3];
            stDSPRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            stDSPRanges[0].NumDescriptors = 2; //2 Const Buffer View
            stDSPRanges[0].BaseShaderRegister = 0;
            stDSPRanges[0].RegisterSpace = 0;
            stDSPRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
            stDSPRanges[0].OffsetInDescriptorsFromTableStart = 0;

            stDSPRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            stDSPRanges[1].NumDescriptors = 3; //3 Texture View
            stDSPRanges[1].BaseShaderRegister = 0;
            stDSPRanges[1].RegisterSpace = 0;
            stDSPRanges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
            stDSPRanges[1].OffsetInDescriptorsFromTableStart = 0;

            stDSPRanges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
            stDSPRanges[2].NumDescriptors = 2; // 2 Sampler
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
            stRootSignatureDesc.Desc_1_1.NumParameters = _countof( stRootParameters );
            stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters;
            stRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
            stRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

            ComPtr<ID3DBlob> pISignatureBlob;
            ComPtr<ID3DBlob> pIErrorBlob;
            GRS_THROW_IF_FAILED( D3D12SerializeVersionedRootSignature( &stRootSignatureDesc
                , &pISignatureBlob
                , &pIErrorBlob ) );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateRootSignature( 0
                , pISignatureBlob->GetBufferPointer()
                , pISignatureBlob->GetBufferSize()
                , IID_PPV_ARGS( &pIRSSecondPass ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIRSSecondPass );

            //----------------------------------------------------------------------------------------------------------------------------
            D3D12_DESCRIPTOR_RANGE1 stShadowSRVRanges[1] = {};
            stShadowSRVRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            stShadowSRVRanges[0].NumDescriptors = 1; //1 Const Buffer View
            stShadowSRVRanges[0].BaseShaderRegister = 0;
            stShadowSRVRanges[0].RegisterSpace = 0;
            stShadowSRVRanges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
            stShadowSRVRanges[0].OffsetInDescriptorsFromTableStart = 0;

            D3D12_ROOT_PARAMETER1 stRSShadowParams[1] = {};
            stRSShadowParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            stRSShadowParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;//仅Vertex Shader 可见
            stRSShadowParams[0].DescriptorTable.NumDescriptorRanges = 1;
            stRSShadowParams[0].DescriptorTable.pDescriptorRanges = &stShadowSRVRanges[0];

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRSShasowDesc = {};
            stRSShasowDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
            stRSShasowDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
            stRSShasowDesc.Desc_1_1.NumParameters = _countof( stRSShadowParams );
            stRSShasowDesc.Desc_1_1.pParameters = stRSShadowParams;
            stRSShasowDesc.Desc_1_1.NumStaticSamplers = 0;
            stRSShasowDesc.Desc_1_1.pStaticSamplers = nullptr;

            pISignatureBlob.Reset();
            pIErrorBlob.Reset();
            GRS_THROW_IF_FAILED( D3D12SerializeVersionedRootSignature( &stRSShasowDesc
                , &pISignatureBlob
                , &pIErrorBlob ) );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateRootSignature( 0
                , pISignatureBlob->GetBufferPointer()
                , pISignatureBlob->GetBufferSize()
                , IID_PPV_ARGS( &pIRSShadowPass ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIRSShadowPass );
        }

        // 9、编译Shader创建渲染管线状态对象
        if( TRUE )
        {
            UINT nShaderCompileFlags = 0;
#if defined(_DEBUG)
            // Enable better shader debugging with the graphics debugging tools.
            nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            //编译为行矩阵形式	   
            nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

            TCHAR pszShaderFileName[MAX_PATH] = {};
            CHAR pszErrMsg[MAX_PATH] = {};
            ComPtr<ID3DBlob> pIVSCode;
            ComPtr<ID3DBlob> pIPSCode;
            ComPtr<ID3DBlob> pIErrMsg;
            StringCchPrintf( pszShaderFileName, MAX_PATH, _T( "%s14-MultiThreadShadow\\Shader\\VSMain.hlsl" ), g_pszAppPath );

            HRESULT hr = D3DCompileFromFile( pszShaderFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE
                , "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIVSCode, &pIErrMsg );
            if ( FAILED( hr ) )
            {
                StringCchPrintfA( pszErrMsg
                    , MAX_PATH
                    , "\n%s\n"
                    , (CHAR*) pIErrMsg->GetBufferPointer() );
                ::OutputDebugStringA( pszErrMsg );
                throw CGRSCOMException( hr );
            }

            pIErrMsg.Reset();
            StringCchPrintf( pszShaderFileName, MAX_PATH, _T( "%s14-MultiThreadShadow\\Shader\\PSMain.hlsl" ), g_pszAppPath );

            hr = D3DCompileFromFile( pszShaderFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE
                , "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIPSCode, &pIErrMsg );
            if ( FAILED( hr ) )
            {
                StringCchPrintfA( pszErrMsg
                    , MAX_PATH
                    , "\n%s\n"
                    , (CHAR*) pIErrMsg->GetBufferPointer() );
                ::OutputDebugStringA( pszErrMsg );
                throw CGRSCOMException( hr );
            }

            // 我们多添加了一个法线的定义，但目前Shader中我们并没有使用
            D3D12_INPUT_ELEMENT_DESC stIALayoutSphere[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            // 创建 graphics pipeline state object (PSO)对象
            D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
            stPSODesc.InputLayout = { stIALayoutSphere, _countof( stIALayoutSphere ) };
            stPSODesc.pRootSignature = pIRSSecondPass.Get();
            stPSODesc.VS.BytecodeLength = pIVSCode->GetBufferSize();
            stPSODesc.VS.pShaderBytecode = pIVSCode->GetBufferPointer();
            stPSODesc.PS.BytecodeLength = pIPSCode->GetBufferSize();
            stPSODesc.PS.pShaderBytecode = pIPSCode->GetBufferPointer();

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
            stPSODesc.DSVFormat = emDSFormat;
            stPSODesc.DepthStencilState.StencilEnable = FALSE;
            stPSODesc.DepthStencilState.DepthEnable = TRUE;			//打开深度缓冲				
            stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//启用深度缓存写入功能
            stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //深度测试函数（该值为普通的深度测试，即较小值写入）

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateGraphicsPipelineState( &stPSODesc
                , IID_PPV_ARGS( &pIPSOSecondPass ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIPSOSecondPass );

            //---------------------------------------------------------------------------------------------
            // Shadow Pass Pipeline State Object
            stPSODesc.pRootSignature = pIRSShadowPass.Get();
            stPSODesc.PS.BytecodeLength = 0;
            stPSODesc.PS.pShaderBytecode = nullptr;
            stPSODesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
            stPSODesc.DSVFormat = emDepthShadowFormat;
            stPSODesc.NumRenderTargets = 0;

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateGraphicsPipelineState( &stPSODesc, IID_PPV_ARGS( &pIPSOShadowPass ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIPSOShadowPass );

        }

        // 10、创建渲染UI矩形（渲染阴影视图）用的管线的根签名、管线状态对象、矩形框VB、CB、采样器（堆）、SRV Heap、CBV、SRV
        {
            //--------------------------------------------------------------------------------------------------------------
            //创建渲染矩形的根签名对象
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
            stRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
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
            stRootSignatureDesc.Desc_1_1.NumParameters = _countof( stRootParameters );
            stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters;
            stRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
            stRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;
  
            ComPtr<ID3DBlob> pISignatureBlob;
            ComPtr<ID3DBlob> pIErrorBlob;

            GRS_THROW_IF_FAILED( D3D12SerializeVersionedRootSignature( &stRootSignatureDesc
                , &pISignatureBlob
                , &pIErrorBlob ) );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateRootSignature( 0
                , pISignatureBlob->GetBufferPointer()
                , pISignatureBlob->GetBufferSize()
                , IID_PPV_ARGS( &pIRSQuad ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIRSQuad );

            //--------------------------------------------------------------------------------------------------------------
            UINT nShaderCompileFlags = 0;
#if defined(_DEBUG)
            // Enable better shader debugging with the graphics debugging tools.
            nShaderCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            nShaderCompileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

            ComPtr<ID3DBlob> pIBlobVertexShader;
            ComPtr<ID3DBlob> pIBlobPixelShader;

            TCHAR pszShaderFileName[MAX_PATH] = {};
            StringCchPrintf( pszShaderFileName, MAX_PATH, _T( "%s14-MultiThreadShadow\\Shader\\Quad.hlsl" ), g_pszAppPath );

            GRS_THROW_IF_FAILED( D3DCompileFromFile( pszShaderFileName, nullptr, nullptr
                , "VSMain", "vs_5_0", nShaderCompileFlags, 0, &pIBlobVertexShader, nullptr ) );
            GRS_THROW_IF_FAILED( D3DCompileFromFile( pszShaderFileName, nullptr, nullptr
                , "PSMain", "ps_5_0", nShaderCompileFlags, 0, &pIBlobPixelShader, nullptr ) );

            D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "COLOR",	  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            };

            // 创建 graphics pipeline state object (PSO)对象
            D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
            stPSODesc.InputLayout = { stInputElementDescs, _countof( stInputElementDescs ) };
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

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateGraphicsPipelineState( &stPSODesc, IID_PPV_ARGS( &pIPSOQuad ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIPSOQuad );

            //--------------------------------------------------------------------------------------------------------------

            ST_GRS_VERTEX_QUAD stQuadVertices[] =
            {
                { { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f },	{ 0.0f, 0.0f }  },
                { { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f },	{ 1.0f, 0.0f }  },
                { { 0.0f, 1.0f, 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f },	{ 0.0f, 1.0f }  },
                { { 1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },	{ 1.0f, 1.0f }  }
            };

            UINT nQuadVBSize = sizeof( stQuadVertices );
            UINT nQuadVertexCnt = _countof( stQuadVertices );

            stBufferResSesc.Width = nQuadVBSize;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                  &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIVBQuad ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVBQuad );

            UINT8* pVertexDataBegin = nullptr;
            D3D12_RANGE stReadRange = { 0, 0 };		// We do not intend to read from this resource on the CPU.
            GRS_THROW_IF_FAILED( pIVBQuad->Map( 0, &stReadRange, reinterpret_cast<void**>( &pVertexDataBegin ) ) );
            memcpy( pVertexDataBegin, (void*) &stQuadVertices[0], sizeof( stQuadVertices ) );
            pIVBQuad->Unmap( 0, nullptr );

            stVBViewQuad.BufferLocation = pIVBQuad->GetGPUVirtualAddress();
            stVBViewQuad.StrideInBytes = sizeof( ST_GRS_VERTEX_QUAD );
            stVBViewQuad.SizeInBytes = nQuadVBSize;

            //--------------------------------------------------------------------------------------------------------------
            stBufferResSesc.Width = szCBMOVBuf;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBMVO ) ) );

            GRS_THROW_IF_FAILED( pICBMVO->Map( 0, nullptr, reinterpret_cast<void**>( &pMOV ) ) );

            //--------------------------------------------------------------------------------------------------------------

            D3D12_DESCRIPTOR_HEAP_DESC stHeapDesc = {};
            stHeapDesc.NumDescriptors = 2;
            stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            stHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            //SRV堆
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stHeapDesc, IID_PPV_ARGS( &pISRVHeapQuad ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pISRVHeapQuad );

            D3D12_CPU_DESCRIPTOR_HANDLE stCPUSRVHeapQuad = pISRVHeapQuad->GetCPUDescriptorHandleForHeapStart() ;

            // CBV
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = pICBMVO->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = static_cast<UINT>( szCBMOVBuf );

            pID3D12Device4->CreateConstantBufferView( &cbvDesc, stCPUSRVHeapQuad );

            stCPUSRVHeapQuad.ptr += g_nSRVDescriptorSize ;

            // SRV
            D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
            stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            stSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
            stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            stSRVDesc.Texture2D.MipLevels = 1;
            // 使用阴影深度2D纹理作为Shader的纹理资源
            pID3D12Device4->CreateShaderResourceView( pIDepthShadowBuffer.Get(), &stSRVDesc, stCPUSRVHeapQuad );

            //Sample
            stHeapDesc.NumDescriptors = 1;  //只有一个Sample
            stHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stHeapDesc, IID_PPV_ARGS( &pISampleHeapQuad ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pISampleHeapQuad );

            // Sample View
            D3D12_SAMPLER_DESC stSamplerDesc = {};
            stSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            stSamplerDesc.MinLOD = 0;
            stSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
            stSamplerDesc.MipLODBias = 0.0f;
            stSamplerDesc.MaxAnisotropy = 1;
            stSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            stSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            stSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            stSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

            pID3D12Device4->CreateSampler( &stSamplerDesc, pISampleHeapQuad->GetCPUDescriptorHandleForHeapStart() );

            //--------------------------------------------------------------------------------------------------------------
            // 计算正交投影矩阵 Orthographic
            // 基于左上角是坐标原点 X正方向向右 Y正方向向下 与窗口坐标系相同
            // 这里的算法来自于古老的HGE引擎核心
            // Orthographic
            XMMATRIX mxOrthographic = XMMatrixScaling( 1.0f, -1.0f, 1.0f ); //上下翻转，配合之前的Quad的Vertex坐标一起使用
            mxOrthographic = XMMatrixMultiply( mxOrthographic, XMMatrixTranslation( -0.5f, +0.5f, 0.0f ) ); //微量偏移，矫正像素位置
            mxOrthographic = XMMatrixMultiply(
                mxOrthographic
                , XMMatrixOrthographicOffCenterLH( g_stViewPort.TopLeftX
                    , g_stViewPort.TopLeftX + g_stViewPort.Width
                    , -( g_stViewPort.TopLeftY + g_stViewPort.Height )
                    , -g_stViewPort.TopLeftY
                    , g_stViewPort.MinDepth
                    , g_stViewPort.MaxDepth )
            );
            // View * Orthographic 正交投影时，视矩阵通常是一个单位矩阵，这里之所以乘一下以表示与射影投影一致
            XMMATRIX mxView = XMMatrixIdentity();
            mxOrthographic = XMMatrixMultiply( mxView, mxOrthographic );

            // 渲染矩形框
            // 设置矩形框的位置，即矩形左上角的坐标，注意因为正交投影的缘故，这里单位是像素
            XMMATRIX xmMVO = XMMatrixMultiply(
                XMMatrixTranslation( 10.0f, 10.0f, 0.0f )
                , mxOrthographic );

            // 设置矩形框的大小，单位同样是像素
            xmMVO = XMMatrixMultiply(
                XMMatrixScaling( 320.0f, 240.0f, 1.0f )
                , xmMVO );

            //设置MVO
            XMStoreFloat4x4( &pMOV->m_mMVO, xmMVO );

            //--------------------------------------------------------------------------------------------------------------

            // 创建绘制矩形的捆绑包
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_BUNDLE
                , IID_PPV_ARGS( &pICmdAllocQuad ) ) );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_BUNDLE
                , pICmdAllocQuad.Get(), pIPSOQuad.Get(), IID_PPV_ARGS( &pICmdBundlesQuad ) ) );

            pICmdBundlesQuad->SetGraphicsRootSignature( pIRSQuad.Get() );
            pICmdBundlesQuad->SetPipelineState( pIPSOQuad.Get() );

            ID3D12DescriptorHeap* ppHeapsQuad[] = { pISRVHeapQuad.Get(),pISampleHeapQuad.Get() };
            pICmdBundlesQuad->SetDescriptorHeaps( _countof( ppHeapsQuad ), ppHeapsQuad );

            D3D12_GPU_DESCRIPTOR_HANDLE stGPUSRVHandle = pISRVHeapQuad->GetGPUDescriptorHandleForHeapStart() ;
            pICmdBundlesQuad->SetGraphicsRootDescriptorTable( 0, stGPUSRVHandle );
            stGPUSRVHandle.ptr += g_nSRVDescriptorSize ;
            pICmdBundlesQuad->SetGraphicsRootDescriptorTable( 1, stGPUSRVHandle );
            pICmdBundlesQuad->SetGraphicsRootDescriptorTable( 2, pISampleHeapQuad->GetGPUDescriptorHandleForHeapStart() );

            pICmdBundlesQuad->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
            pICmdBundlesQuad->IASetVertexBuffers( 0, 1, &stVBViewQuad );
            //Draw Call！！！
            pICmdBundlesQuad->DrawInstanced( nQuadVertexCnt, 1, 0, 0 );
            pICmdBundlesQuad->Close();

            //--------------------------------------------------------------------------------------------------------------
        }

        // 11、创建Sample
        if ( TRUE )
        {
            D3D12_DESCRIPTOR_HEAP_DESC stSamplerHeapDesc = {};
            stSamplerHeapDesc.NumDescriptors = 2;  // 1 Wrap Sample + 1 Clamp Sample
            stSamplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            stSamplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stSamplerHeapDesc, IID_PPV_ARGS( &pISampleHeap ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pISampleHeap );

            const UINT nSamplerDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
            D3D12_CPU_DESCRIPTOR_HANDLE stSamplerHandle = pISampleHeap->GetCPUDescriptorHandleForHeapStart();

            D3D12_SAMPLER_DESC stWrapSamplerDesc = {};
            stWrapSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            stWrapSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            stWrapSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            stWrapSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            stWrapSamplerDesc.MinLOD = 0;
            stWrapSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
            stWrapSamplerDesc.MipLODBias = 0.0f;
            stWrapSamplerDesc.MaxAnisotropy = 1;
            stWrapSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            stWrapSamplerDesc.BorderColor[0]
                = stWrapSamplerDesc.BorderColor[1]
                = stWrapSamplerDesc.BorderColor[2]
                = stWrapSamplerDesc.BorderColor[3] = 0;
            pID3D12Device4->CreateSampler( &stWrapSamplerDesc, stSamplerHandle );

            stSamplerHandle.ptr += nSamplerDescriptorSize;

            D3D12_SAMPLER_DESC stClampSamplerDesc = {};
            stClampSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            stClampSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            stClampSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            stClampSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            stClampSamplerDesc.MipLODBias = 0.0f;
            stClampSamplerDesc.MaxAnisotropy = 1;
            stClampSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            stClampSamplerDesc.BorderColor[0]
                = stClampSamplerDesc.BorderColor[1]
                = stClampSamplerDesc.BorderColor[2]
                = stClampSamplerDesc.BorderColor[3] = 0;
            stClampSamplerDesc.MinLOD = 0;
            stClampSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
            pID3D12Device4->CreateSampler( &stClampSamplerDesc, stSamplerHandle );
        }

        // 12、创建全局灯光
        {
            // 计算透视矩阵（本例中光源 摄像机都使用同一个透视矩阵）
            XMStoreFloat4x4( &g_mxProjection, XMMatrixPerspectiveFovLH( XM_PIDIV4, (FLOAT) g_iWndWidth / (FLOAT) g_iWndHeight, fNearPlane, fFarPlane ) );
            //XMMatrixShadow()
            stBufferResSesc.Width = szLightBuf; //注意缓冲尺寸设置为256边界对齐大小
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBLights ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBLights );

            // Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
            GRS_THROW_IF_FAILED( pICBLights->Map( 0, nullptr, reinterpret_cast<void**>( &g_pstLights ) ) );

            g_pstLights->m_bIsSampleShadowMap = 1;
            g_pstLights->m_v4AmbientColor = XMFLOAT4( 0.1f, 0.1f, 0.1f, 1.0f );

            for ( int i = 0; i < GRS_NUM_LIGHTS; i++ )
            {
                g_pstLights->m_stLights[i].m_v4Position = { 0.0f, 5.0f, -10.0f, 1.0f };
                g_pstLights->m_stLights[i].m_v4Direction = { 0.0, 0.0f, 1.0f, 0.0f };
                g_pstLights->m_stLights[i].m_v4Falloff = { 80.0f, 1.0f, 0.0f, 1.0f };
                g_pstLights->m_stLights[i].m_v4Color = { 0.9f, 0.9f, 0.9f, 1.0f };

                XMVECTOR eye = XMLoadFloat4( &g_pstLights->m_stLights[i].m_v4Position );
                XMVECTOR at = XMVectorAdd( eye, XMLoadFloat4( &g_pstLights->m_stLights[i].m_v4Direction ) );
                XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

                XMStoreFloat4x4( &g_pstLights->m_stLights[i].m_mxView, XMMatrixLookAtLH( eye, at, up ) );

                g_pstLights->m_stLights[i].m_mxProjection = g_mxProjection;
            }

            g_pstLights->m_stLights[1].m_v4Color = { 0.0f, 0.0f, 0.0f, 1.0f };
            g_pstLights->m_stLights[2].m_v4Color = { 0.0f, 0.0f, 0.0f, 1.0f };
        }

        // 13、准备参数并启动多个渲染线程
        {
            USES_CONVERSION;
            // 球体个性参数
            StringCchPrintf( g_stThreadParams[g_nThdSphere].m_pszDiffuseFile, MAX_PATH, _T( "%sAssets\\Earth_512.dds" ), g_pszAppPath );
            StringCchPrintf( g_stThreadParams[g_nThdSphere].m_pszNormalFile, MAX_PATH, _T( "%sAssets\\Earth_512_Normal.dds" ), g_pszAppPath );
            g_stThreadParams[g_nThdSphere].m_v4ModelPos = XMFLOAT4( 2.0f, 1.0f, 0.0f, 1.0f );

            // 立方体个性参数
            StringCchPrintf( g_stThreadParams[g_nThdCube].m_pszDiffuseFile, MAX_PATH, _T( "%sAssets\\Cube.dds" ), g_pszAppPath );
            StringCchPrintf( g_stThreadParams[g_nThdCube].m_pszNormalFile, MAX_PATH, _T( "%sAssets\\Cube_NRM.dds" ), g_pszAppPath );
            g_stThreadParams[g_nThdCube].m_v4ModelPos = XMFLOAT4( -2.0f, 1.0f, 0.0f, 1.0f );

            // 平板个性参数
            StringCchPrintf( g_stThreadParams[g_nThdPlane].m_pszDiffuseFile, MAX_PATH, _T( "%sAssets\\Plane.dds" ), g_pszAppPath );
            StringCchPrintf( g_stThreadParams[g_nThdPlane].m_pszNormalFile, MAX_PATH, _T( "%sAssets\\Plane_NRM.dds" ), g_pszAppPath );
            g_stThreadParams[g_nThdPlane].m_v4ModelPos = XMFLOAT4( 0.0f, 0.0f, 0.0f, 1.0f );


            // 物体的共性参数，也就是各线程的共性参数
            for ( int i = 0; i < g_nMaxThread; i++ )
            {
                g_stThreadParams[i].m_nThreadIndex = i;		//记录序号

                //创建每个线程需要的命令列表和复制命令队列
                GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT
                    , IID_PPV_ARGS( &g_stThreadParams[i].m_pICmdAlloc ) ) );
                GRS_SetD3D12DebugNameIndexed( g_stThreadParams[i].m_pICmdAlloc, _T( "pIThreadCmdAlloc" ), i );

                GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT
                    , g_stThreadParams[i].m_pICmdAlloc, nullptr, IID_PPV_ARGS( &g_stThreadParams[i].m_pICmdList ) ) );
                GRS_SetD3D12DebugNameIndexed( g_stThreadParams[i].m_pICmdList, _T( "pIThreadCmdList" ), i );

                GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT
                    , IID_PPV_ARGS( &g_stThreadParams[i].m_pICmdAllocShadow ) ) );
                GRS_SetD3D12DebugNameIndexed( g_stThreadParams[i].m_pICmdAllocShadow, _T( "pIThreadCmdAllocShadow" ), i );

                GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList( 0, D3D12_COMMAND_LIST_TYPE_DIRECT
                    , g_stThreadParams[i].m_pICmdAllocShadow, nullptr, IID_PPV_ARGS( &g_stThreadParams[i].m_pICmdListShadow ) ) );
                GRS_SetD3D12DebugNameIndexed( g_stThreadParams[i].m_pICmdListShadow, _T( "pIThreadCmdListShadow" ), i );

                g_stThreadParams[i].m_dwMainThreadID = ::GetCurrentThreadId();
                g_stThreadParams[i].m_hMainThread = ::GetCurrentThread();
                g_stThreadParams[i].m_hRunEvent = ::CreateEvent( nullptr, FALSE, FALSE, nullptr );
                g_stThreadParams[i].m_hEventShadowOver = ::CreateEvent( nullptr, FALSE, FALSE, nullptr );
                g_stThreadParams[i].m_hEventRenderOver = ::CreateEvent( nullptr, FALSE, FALSE, nullptr );
                g_stThreadParams[i].m_pID3D12Device4 = pID3D12Device4.Get();

                g_stThreadParams[i].m_pIRSShadowPass = pIRSShadowPass.Get();
                g_stThreadParams[i].m_pIPSOShadowPass = pIPSOShadowPass.Get();
                g_stThreadParams[i].m_pIRSSecondPass = pIRSSecondPass.Get();
                g_stThreadParams[i].m_pIPSOSecondPass = pIPSOSecondPass.Get();
                g_stThreadParams[i].m_pISampleHeap = pISampleHeap.Get();
                g_stThreadParams[i].m_pITexShadowMap = pIDepthShadowBuffer.Get();
                g_stThreadParams[i].m_pICBLights = pICBLights.Get();

                g_stThreadParams[i].m_nState = EM_GRS_RS_GPUCOPY; //初始状态为GPU Copy状态

                arHWaited.Add( g_stThreadParams[i].m_hEventRenderOver ); //添加到被等待队列里

                //以暂停方式创建线程
                g_stThreadParams[i].m_hThisThread = (HANDLE) _beginthreadex( nullptr,
                    0, RenderThread, (void*) &g_stThreadParams[i],
                    CREATE_SUSPENDED, (UINT*) &g_stThreadParams[i].m_dwThisThreadID );

                //然后判断线程创建是否成功
                if ( nullptr == g_stThreadParams[i].m_hThisThread
                    || reinterpret_cast<HANDLE>( -1 ) == g_stThreadParams[i].m_hThisThread )
                {
                    throw CGRSCOMException( HRESULT_FROM_WIN32( GetLastError() ) );
                }

                arHSubThread.Add( g_stThreadParams[i].m_hThisThread );
            }

            //逐一启动线程
            for ( int i = 0; i < g_nMaxThread; i++ )
            {
                ::ResumeThread( g_stThreadParams[i].m_hThisThread );
            }
        }

        //主消息循环状态变量:0，表示需要完成GPU上的第二个Copy动作 1、表示通知各线程去渲染 2、表示各线程渲染命令记录结束可以提交执行了
        EM_GRS_RENDER_STATUS nStates = EM_GRS_RS_GPUCOPY; //初始状态为GPU Copy
        DWORD dwRet = 0;
        DWORD dwWaitCnt = 0;
        UINT64 n64fence = 0;

        CAtlArray<ID3D12CommandList*> arCmdList;

        BOOL bExit = FALSE;

        ULONGLONG n64tmFrameStart = ::GetTickCount64();
        ULONGLONG n64tmCurrent = n64tmFrameStart;
        //计算旋转角度需要的变量
        double dModelRotationYAngle = 0.0f;

        //起始的时候关闭一下两个命令列表，因为我们在开始渲染的时候都需要先reset它们，为了防止报错故先Close
        GRS_THROW_IF_FAILED( pICmdListPre->Close() );
        GRS_THROW_IF_FAILED( pICmdListMid->Close() );
        GRS_THROW_IF_FAILED( pICmdListPost->Close() );

        SetTimer( hWnd, WM_USER + 100, 1, nullptr ); //这句为了保证MsgWaitForMultipleObjects 在 wait for all 为True时 能够及时返回

        // 初始化完了再显示窗口
        ShowWindow( hWnd, nCmdShow );
        UpdateWindow( hWnd );

        // 15、开始主消息循环，并在其中不断渲染
        while ( !bExit )
        {
            //主线程进入等待
            dwWaitCnt = static_cast<DWORD>( arHWaited.GetCount() );
            dwRet = ::MsgWaitForMultipleObjects( dwWaitCnt, arHWaited.GetData(), TRUE, INFINITE, QS_ALLINPUT );
            dwRet -= WAIT_OBJECT_0;

            if ( 0 == dwRet )
            {
                switch ( nStates )
                {
                case EM_GRS_RS_GPUCOPY:
                {//状态EM_GRS_RS_GPUCOPY：执行资源上传的第二个Copy命令
                    arCmdList.RemoveAll();
                    //执行命令列表
                    arCmdList.Add( g_stThreadParams[g_nThdSphere].m_pICmdList );
                    arCmdList.Add( g_stThreadParams[g_nThdCube].m_pICmdList );
                    arCmdList.Add( g_stThreadParams[g_nThdPlane].m_pICmdList );

                    pIMainCmdQueue->ExecuteCommandLists( static_cast<UINT>( arCmdList.GetCount() ), arCmdList.GetData() );

                    //---------------------------------------------------------------------------------------------
                    // 开始同步GPU与CPU的执行，先记录围栏标记值
                    n64fence = n64FenceValue;
                    GRS_THROW_IF_FAILED( pIMainCmdQueue->Signal( pIFence.Get(), n64fence ) );
                    n64FenceValue++;
                    GRS_THROW_IF_FAILED( pIFence->SetEventOnCompletion( n64fence, hEventFence ) );

                    nStates = EM_GRS_RS_STARTSHADOW; //跳转到阴影渲染状态

                    arHWaited.RemoveAll();
                    arHWaited.Add( hEventFence );
                }
                break;
                case EM_GRS_RS_STARTSHADOW:
                {// 状态EM_GRS_RS_STARTSHADOW：渲染阴影
                    // 注意只在状态1 调用OnUpdate这个过程，只有阴影渲染，正常渲染结束了才再次更新，两遍渲染才是一个完整的周期
                    //OnUpdate()
                    {
                        //关于时间的基本运算都放在了主线程中
                        //真实的引擎或程序中建议时间值也作为一个每帧更新的参数从主线程获取并传给各子线程
                        n64tmCurrent = ::GetTickCount64();
                        //计算旋转的角度：旋转角度(弧度) = 时间(秒) * 角速度(弧度/秒)
                        //下面这句代码相当于经典游戏消息循环中的OnUpdate函数中需要做的事情
                        dModelRotationYAngle += ( ( n64tmCurrent - n64tmFrameStart ) / 1000.0f ) * g_fPalstance;

                        //旋转角度是2PI周期的倍数，去掉周期数，只留下相对0弧度开始的小于2PI的弧度即可
                        if ( dModelRotationYAngle > XM_2PI )
                        {
                            dModelRotationYAngle = fmod( dModelRotationYAngle, XM_2PI );
                        }

                        //计算 World 矩阵 这里是个旋转矩阵
                        //XMStoreFloat4x4(&g_mxWorld, XMMatrixRotationY(static_cast<float>(dModelRotationYAngle)));

                        XMStoreFloat4x4( &g_mxWorld, XMMatrixIdentity() );

                        // 变换光源
                        for ( int i = 0; i < GRS_NUM_LIGHTS; i++ )
                        {
                            XMVECTOR eye = XMLoadFloat4( &g_pstLights->m_stLights[i].m_v4Position );
                            XMVECTOR at = { 0.0f,0.0f,0.0f,0.0f };
                            XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

                            XMStoreFloat4x4( &g_pstLights->m_stLights[i].m_mxView
                                , XMMatrixLookAtLH( eye, at, up ) );
                        }
                        // 渲染阴影 以第一个光源的空间为 视空间
                        g_mxView = g_pstLights->m_stLights[0].m_mxView;
                    }

                    GRS_THROW_IF_FAILED( pICmdAllocPre->Reset() );
                    GRS_THROW_IF_FAILED( pICmdListPre->Reset( pICmdAllocPre.Get(), pIPSOSecondPass.Get() ) );

                    GRS_THROW_IF_FAILED( pICmdAllocMid->Reset() );
                    GRS_THROW_IF_FAILED( pICmdListMid->Reset( pICmdAllocMid.Get(), pIPSOSecondPass.Get() ) );

                    GRS_THROW_IF_FAILED( pICmdAllocPost->Reset() );
                    GRS_THROW_IF_FAILED( pICmdListPost->Reset( pICmdAllocPost.Get(), pIPSOSecondPass.Get() ) );

                    nCurrentFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

                    //渲染前处理
                    {
                        stResStateTransBarrier.Transition.pResource = pIDepthShadowBuffer.Get();
                        stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                        stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

                        pICmdListPre->ResourceBarrier( 1, &stResStateTransBarrier );

                        //DSV堆上第一个描述符就是阴影的深度缓冲
                        D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = g_pIDSVHeap->GetCPUDescriptorHandleForHeapStart() ;
                        //设置渲染目标（注意阴影Pass不要RTV）
                        pICmdListPre->OMSetRenderTargets( 0, nullptr, FALSE, &stDSVHandle );
                        // 阴影贴图与窗口分辨率不同，需要改变视口（viewport）的参数以适应阴影贴图的尺寸。
                        // 如果不更新视口，最后的深度贴图要么太小要么就不完整。
                        pICmdListPre->RSSetViewports( 1, &g_stDepthViewPort );
                        pICmdListPre->RSSetScissorRects( 1, &g_stScissorRect );

                        pICmdListPre->ClearDepthStencilView( stDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );
                    }

                    arHWaited.RemoveAll();
                    for ( int i = 0; i < g_nMaxThread; i++ )
                    {
                        //添加句柄到等待数组，等待各线程通知说阴影pass已经完成
                        arHWaited.Add( g_stThreadParams[i].m_hEventShadowOver );
                        //状态设置为 EM_GRS_RS_STARTSHADOW 通知各线程开始阴影渲染
                        g_stThreadParams[i].m_nState = EM_GRS_RS_STARTSHADOW;
                        //统一设置当前帧号
                        g_stThreadParams[i].m_nCurrentFrameIndex = nCurrentFrameIndex;
                        //统一设置帧时间
                        g_stThreadParams[i].m_nStartTime = n64tmFrameStart;
                        g_stThreadParams[i].m_nCurrentTime = n64tmCurrent;
                    }

                    nStates = EM_GRS_RS_STARTRENDER; //跳转到正常渲染状态

                    //注意上下两个循环最好不要并到一起，防止不必要的额外多线程竞争问题，参数统一设置完，再通知各线程开始干活
                    //类似打牌游戏，先把牌发给每个人之后，游戏才开始，而不是谁拿了牌就可以先出

                    //通知各线程开始渲染
                    for ( int i = 0; i < g_nMaxThread; i++ )
                    {
                        SetEvent( g_stThreadParams[i].m_hRunEvent );
                    }
                }
                break;
                case EM_GRS_RS_STARTRENDER:
                {// 状态EM_GRS_RS_STARTRENDER：阴影渲染结束 开始正常渲染
                    // OnUpdate() 恢复View Projection 矩阵到摄像机空间
                    {
                        //计算 World 矩阵 这里是个旋转矩阵
                        //XMStoreFloat4x4(&g_mxWorld, XMMatrixRotationY(static_cast<float>(dModelRotationYAngle)));

                        //计算 摄像机空间视矩阵 view
                        XMStoreFloat4x4( &g_mxView
                            , XMMatrixLookAtLH(
                                XMLoadFloat3( &g_f3EyePos )
                                , XMLoadFloat3( &g_f3LockAt )
                                , XMLoadFloat3( &g_f3HeapUp ) ) );
                    }

                    //Second Pass 渲染前处理
                    {
                        //将第一遍渲染的阴影深度视图转换为资源状态
                        stResStateTransBarrier.Transition.pResource = pIDepthShadowBuffer.Get();
                        stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
                        stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                        pICmdListMid->ResourceBarrier( 1, &stResStateTransBarrier );

                        //转换渲染目标状态
                        stResStateTransBarrier.Transition.pResource = g_pIARenderTargets[nCurrentFrameIndex].Get();
                        stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                        stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                        pICmdListMid->ResourceBarrier( 1, &stResStateTransBarrier );

                        //偏移描述符指针到指定帧缓冲视图位置
                        D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = g_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                        stRTVHandle.ptr += ( nCurrentFrameIndex * g_nRTVDescriptorSize );

                        //第二个DSV是正常渲染用的DSV
                        D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = g_pIDSVHeap->GetCPUDescriptorHandleForHeapStart();
                        stDSVHandle.ptr += g_nDSVDescriptorSize;

                        //设置渲染目标
                        pICmdListMid->OMSetRenderTargets( 1, &stRTVHandle, FALSE, &stDSVHandle );

                        pICmdListMid->RSSetViewports( 1, &g_stViewPort );
                        pICmdListMid->RSSetScissorRects( 1, &g_stScissorRect );

                        pICmdListMid->ClearRenderTargetView( stRTVHandle, faClearColor, 0, nullptr );
                        pICmdListMid->ClearDepthStencilView( stDSVHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );
                    }

                    arHWaited.RemoveAll();
                    for ( int i = 0; i < g_nMaxThread; i++ )
                    {
                        // 准备在Render Over事件上等待
                        arHWaited.Add( g_stThreadParams[i].m_hEventRenderOver );
                        //状态设置为 EM_GRS_RS_STARTSHADOW 通知各线程开始阴影渲染
                        g_stThreadParams[i].m_nState = EM_GRS_RS_STARTRENDER;
                        //继续设置当前帧号 时间啥的 其实没必要，因为作为一帧来说渲染过程中就不再取Current Time了
                        g_stThreadParams[i].m_nCurrentFrameIndex = nCurrentFrameIndex;
                        g_stThreadParams[i].m_nStartTime = n64tmFrameStart;
                        g_stThreadParams[i].m_nCurrentTime = n64tmCurrent;
                    }

                    nStates = EM_GRS_RS_END;
                    //为啥分开两个循环，上面已经说了，这里不赘述了
                    //通知各线程开始渲染
                    for ( int i = 0; i < g_nMaxThread; i++ )
                    {
                        SetEvent( g_stThreadParams[i].m_hRunEvent );
                    }
                }
                break;
                case EM_GRS_RS_END:// 状态3 两遍渲染结束了，可以提交命令列表执行，或者继续搞后处理
                {
                    {
                        //偏移描述符指针到指定帧缓冲视图位置
                        D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = g_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                        stRTVHandle.ptr += ( nCurrentFrameIndex * g_nRTVDescriptorSize );

                        //第二个DSV是正常渲染用的DSV
                        D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = g_pIDSVHeap->GetCPUDescriptorHandleForHeapStart();
                        stDSVHandle.ptr += g_nDSVDescriptorSize;
                        //设置渲染目标
                        pICmdListPost->OMSetRenderTargets( 1, &stRTVHandle, FALSE, &stDSVHandle );

                        pICmdListPost->RSSetViewports( 1, &g_stViewPort );
                        pICmdListPost->RSSetScissorRects( 1, &g_stScissorRect );

                        //以上的调用是必须的，多个命令列表记录命令并同时渲染同一帧时
                        //必须保证基本的渲染目标，视口、裁切矩形等都被设置为是一致的

                        ID3D12DescriptorHeap* ppHeapsQuad[] = { pISRVHeapQuad.Get(),pISampleHeapQuad.Get() };
                        pICmdListPost->SetDescriptorHeaps( _countof( ppHeapsQuad ), ppHeapsQuad );
                        pICmdListPost->ExecuteBundle( pICmdBundlesQuad.Get() );
                    }
                    //切换渲染目标状态
                    stResStateTransBarrier.Transition.pResource = g_pIARenderTargets[nCurrentFrameIndex].Get();
                    stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                    stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
                    pICmdListPost->ResourceBarrier( 1, &stResStateTransBarrier );

                    //关闭命令列表，可以去执行了
                    GRS_THROW_IF_FAILED( pICmdListPre->Close() );
                    GRS_THROW_IF_FAILED( pICmdListMid->Close() );
                    GRS_THROW_IF_FAILED( pICmdListPost->Close() );

                    arCmdList.RemoveAll();
                    //添加命令列表到数组（注意命令列表排队的方式）
                    arCmdList.Add( pICmdListPre.Get() );
                    arCmdList.Add( g_stThreadParams[g_nThdSphere].m_pICmdListShadow );
                    arCmdList.Add( g_stThreadParams[g_nThdCube].m_pICmdListShadow );
                    arCmdList.Add( g_stThreadParams[g_nThdPlane].m_pICmdListShadow );
                    arCmdList.Add( pICmdListMid.Get() );
                    arCmdList.Add( g_stThreadParams[g_nThdSphere].m_pICmdList );
                    arCmdList.Add( g_stThreadParams[g_nThdCube].m_pICmdList );
                    arCmdList.Add( g_stThreadParams[g_nThdPlane].m_pICmdList );
                    arCmdList.Add( pICmdListPost.Get() );

                    pIMainCmdQueue->ExecuteCommandLists( static_cast<UINT>( arCmdList.GetCount() ), arCmdList.GetData() );

                    //提交画面
                    GRS_THROW_IF_FAILED( pISwapChain3->Present( 1, 0 ) );

                    //开始同步GPU与CPU的执行，先记录围栏标记值
                    n64fence = n64FenceValue;
                    GRS_THROW_IF_FAILED( pIMainCmdQueue->Signal( pIFence.Get(), n64fence ) );
                    n64FenceValue++;
                    GRS_THROW_IF_FAILED( pIFence->SetEventOnCompletion( n64fence, hEventFence ) );

                    nStates = EM_GRS_RS_STARTSHADOW;

                    arHWaited.RemoveAll();
                    arHWaited.Add( hEventFence );

                    //更新下帧开始时间为上一帧开始时间
                    n64tmFrameStart = n64tmCurrent;
                }
                break;
                default:// 不可能的情况，但为了避免讨厌的编译警告或意外情况保留这个default
                {
                    bExit = TRUE;
                }
                break;
                }

                //处理消息
                while ( ::PeekMessage( &msg, NULL, 0, 0, PM_REMOVE | PM_NOYIELD ) )
                {
                    if ( WM_QUIT != msg.message )
                    {
                        ::TranslateMessage( &msg );
                        ::DispatchMessage( &msg );
                    }
                    else
                    {
                        bExit = TRUE;
                    }
                }
            }
            else
            {// MsgWait 超时或者出错，又或者别等待的有Mutex之类，现在我们的例子里还不会出现，so
                bExit = TRUE; //退出拉倒
            }
            //---------------------------------------------------------------------------------------------
            //检测一下线程的活动情况，如果有线程已经退出了，就退出循环
            dwRet = WaitForMultipleObjects( static_cast<DWORD>( arHSubThread.GetCount() ), arHSubThread.GetData(), FALSE, 0 );
            dwRet -= WAIT_OBJECT_0;
            if ( dwRet >= 0 && dwRet < g_nMaxThread )
            {
                bExit = TRUE;
            }
        }
        KillTimer( hWnd, WM_USER + 100 );
    }
    catch ( CGRSCOMException& e )
    {//发生了COM异常
        e;
    }

    try
    {
        // 通知子线程退出
        for ( int i = 0; i < g_nMaxThread; i++ )
        {
            ::PostThreadMessage( g_stThreadParams[i].m_dwThisThreadID, WM_QUIT, 0, 0 );
        }

        // 等待所有子线程退出
        DWORD dwRet = WaitForMultipleObjects( static_cast<DWORD>( arHSubThread.GetCount() ), arHSubThread.GetData(), TRUE, INFINITE );

        // 清理所有子线程资源
        for ( int i = 0; i < g_nMaxThread; i++ )
        {
            ::CloseHandle( g_stThreadParams[i].m_hThisThread );
            ::CloseHandle( g_stThreadParams[i].m_hEventShadowOver );
            ::CloseHandle( g_stThreadParams[i].m_hEventRenderOver );
            g_stThreadParams[i].m_pICmdList->Release();
            g_stThreadParams[i].m_pICmdAlloc->Release();
            g_stThreadParams[i].m_pICmdListShadow->Release();
            g_stThreadParams[i].m_pICmdAllocShadow->Release();
        }

        //::CoUninitialize();
    }
    catch ( CGRSCOMException& e )
    {//发生了COM异常
        e;
    }
    ::CoUninitialize();
    return 0;
}

UINT __stdcall RenderThread( void* pParam )
{
    ST_GRS_THREAD_PARAMS* pThdPms = static_cast<ST_GRS_THREAD_PARAMS*>( pParam );
    try
    {
        if ( nullptr == pThdPms )
        {//参数异常，抛异常终止线程
            throw CGRSCOMException( E_INVALIDARG );
        }

        ComPtr<ID3D12Resource>				pITexDiffuse;
        ComPtr<ID3D12Resource>				pITexDiffuseUpload;
        ComPtr<ID3D12Resource>				pITexNormal;
        ComPtr<ID3D12Resource>				pITexNormalUpload;
        ComPtr<ID3D12Resource>				pITexShadow;
        ComPtr<ID3D12Resource>				pITexShadowUpload;

        ComPtr<ID3D12Resource>				pIVB;
        ComPtr<ID3D12Resource>				pIIB;

        ComPtr<ID3D12Resource>			    pICBWVP;
        ComPtr<ID3D12DescriptorHeap>		pICBVSRVHeap;

        UINT								nSRVSize
            = pThdPms->m_pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

        ST_GRS_MVP* pMVPBufModule = nullptr;
        SIZE_T								szMVPBuf = GRS_UPPER( sizeof( ST_GRS_MVP ), 256 );

        D3D12_VERTEX_BUFFER_VIEW			stVBV = {};
        D3D12_INDEX_BUFFER_VIEW				stIBV = {};

        XMMATRIX						mxPosModule = XMMatrixTranslationFromVector( XMLoadFloat4( &pThdPms->m_v4ModelPos ) );  //当前渲染物体的位置
        // Mesh Value
        UINT								nIndexCnt = 0;
        UINT								nVertexCnt = 0;

        // DDS Value
        std::unique_ptr<uint8_t[]>			pbDDSData;
        std::vector<D3D12_SUBRESOURCE_DATA> stArSubResources;
        DDS_ALPHA_MODE						emAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
        bool								                bIsCube = false;

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

        D3D12_RESOURCE_BARRIER stResStateTransBarrier = {};
        stResStateTransBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        stResStateTransBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        stResStateTransBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        //1、加载DDS纹理
        {
            //-------------------------------------------------------------------------------------------------------------------------------
            // Diffuse Texture
            GRS_THROW_IF_FAILED( LoadDDSTextureFromFile( pThdPms->m_pID3D12Device4, pThdPms->m_pszDiffuseFile, pITexDiffuse.GetAddressOf()
                , pbDDSData, stArSubResources, SIZE_MAX, &emAlphaMode, &bIsCube ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pITexDiffuse );

            UINT64 n64szUpSphere = 0;
            D3D12_RESOURCE_DESC stTXDesc = pITexDiffuse->GetDesc();
            pThdPms->m_pID3D12Device4->GetCopyableFootprints( &stTXDesc, 0, static_cast<UINT>( stArSubResources.size() )
                , 0, nullptr, nullptr, nullptr, &n64szUpSphere );
            
            stBufferResSesc.Width = n64szUpSphere;
            GRS_THROW_IF_FAILED( pThdPms->m_pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pITexDiffuseUpload ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pITexDiffuseUpload );

            //UpdateSubresources( pThdPms->m_pICmdList
            //    , pITexDiffuse.Get()
            //    , pITexDiffuseUpload.Get()
            //    , 0
            //    , 0
            //    , static_cast<UINT>( stArSubResources.size() )
            //    , stArSubResources.data() );

            // 两次Copy上传资源
            UINT nFirstSubresource = 0;
            UINT nNumSubresources = static_cast<UINT>( stArSubResources.size() );
            D3D12_RESOURCE_DESC stUploadResDesc = pITexDiffuseUpload->GetDesc();
            D3D12_RESOURCE_DESC stDefaultResDesc = pITexDiffuse->GetDesc();

            UINT64 n64RequiredSize = 0;
            SIZE_T szMemToAlloc = static_cast<UINT64>( sizeof( D3D12_PLACED_SUBRESOURCE_FOOTPRINT )
                + sizeof( UINT )
                + sizeof( UINT64 ) )
                * nNumSubresources;

            void* pMem = GRS_CALLOC( static_cast<SIZE_T>( szMemToAlloc ) );

            if ( nullptr == pMem )
            {
                throw CGRSCOMException( HRESULT_FROM_WIN32( GetLastError() ) );
            }

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>( pMem );
            UINT64* pRowSizesInBytes = reinterpret_cast<UINT64*>( pLayouts + nNumSubresources );
            UINT* pNumRows = reinterpret_cast<UINT*>( pRowSizesInBytes + nNumSubresources );

            // 这里是第二次调用GetCopyableFootprints，就得到了所有子资源的详细信息
            pThdPms->m_pID3D12Device4->GetCopyableFootprints( &stDefaultResDesc, nFirstSubresource, nNumSubresources, 0, pLayouts, pNumRows, pRowSizesInBytes, &n64RequiredSize );

            BYTE* pData = nullptr;
            GRS_THROW_IF_FAILED( pITexDiffuseUpload->Map( 0, nullptr, reinterpret_cast<void**>( &pData ) ) );
            // 第一遍Copy！注意3重循环每重的意思
            for ( UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes )
            {// SubResources
                if ( pRowSizesInBytes[nSubRes] > ( SIZE_T )-1 )
                {
                    throw CGRSCOMException( E_FAIL );
                }

                D3D12_MEMCPY_DEST stCopyDestData = { pData + pLayouts[nSubRes].Offset
                    , pLayouts[nSubRes].Footprint.RowPitch
                    , pLayouts[nSubRes].Footprint.RowPitch * pNumRows[nSubRes]
                };

                for ( UINT z = 0; z < pLayouts[nSubRes].Footprint.Depth; ++z )
                {// Mipmap
                    BYTE* pDestSlice = reinterpret_cast<BYTE*>( stCopyDestData.pData ) + stCopyDestData.SlicePitch * z;
                    const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>( stArSubResources[nSubRes].pData ) + stArSubResources[nSubRes].SlicePitch * z;
                    for ( UINT y = 0; y < pNumRows[nSubRes]; ++y )
                    {// Rows
                        memcpy( pDestSlice + stCopyDestData.RowPitch * y,
                            pSrcSlice + stArSubResources[nSubRes].RowPitch * y,
                            (SIZE_T) pRowSizesInBytes[nSubRes] );
                    }
                }
            }
            pITexDiffuseUpload->Unmap( 0, nullptr );

            // 第二次Copy！
            if ( stDefaultResDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER )
            {// Buffer 一次性复制就可以了，因为没有行对齐和行大小不一致的问题，Buffer中行数就是1
                pThdPms->m_pICmdList ->CopyBufferRegion(
                    pITexDiffuse.Get(), 0, pITexDiffuseUpload.Get(), pLayouts[0].Offset, pLayouts[0].Footprint.Width );
            }
            else
            {
                for ( UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes )
                {
                    D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
                    stDstCopyLocation.pResource = pITexDiffuse.Get();
                    stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    stDstCopyLocation.SubresourceIndex = nSubRes;

                    D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
                    stSrcCopyLocation.pResource = pITexDiffuseUpload.Get();
                    stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    stSrcCopyLocation.PlacedFootprint = pLayouts[nSubRes];

                    pThdPms->m_pICmdList->CopyTextureRegion( &stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr );
                }
            }
            GRS_SAFE_FREE( pMem );

            //同步
            stResStateTransBarrier.Transition.pResource = pITexDiffuse.Get();
            stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            pThdPms->m_pICmdList->ResourceBarrier( 1, &stResStateTransBarrier );

            //-------------------------------------------------------------------------------------------------------------------------------
            // Normal Texture
            GRS_THROW_IF_FAILED( LoadDDSTextureFromFile( pThdPms->m_pID3D12Device4, pThdPms->m_pszNormalFile, pITexNormal.GetAddressOf()
                , pbDDSData, stArSubResources, SIZE_MAX, &emAlphaMode, &bIsCube ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pITexNormal );

            stTXDesc = pITexNormal->GetDesc();
            pThdPms->m_pID3D12Device4->GetCopyableFootprints( &stTXDesc, 0, static_cast<UINT>( stArSubResources.size() )
                , 0, nullptr, nullptr, nullptr, &n64szUpSphere );

            stBufferResSesc.Width = n64szUpSphere;
            GRS_THROW_IF_FAILED( pThdPms->m_pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pITexNormalUpload ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pITexNormalUpload );

            // 两次Copy上传资源
            {
                UINT nFirstSubresource = 0;
                UINT nNumSubresources = static_cast<UINT>( stArSubResources.size() );
                D3D12_RESOURCE_DESC stUploadResDesc = pITexNormalUpload->GetDesc();
                D3D12_RESOURCE_DESC stDefaultResDesc = pITexNormal->GetDesc();

                UINT64 n64RequiredSize = 0;
                SIZE_T szMemToAlloc = static_cast<UINT64>( sizeof( D3D12_PLACED_SUBRESOURCE_FOOTPRINT )
                    + sizeof( UINT )
                    + sizeof( UINT64 ) )
                    * nNumSubresources;

                void* pMem = GRS_CALLOC( static_cast<SIZE_T>( szMemToAlloc ) );

                if ( nullptr == pMem )
                {
                    throw CGRSCOMException( HRESULT_FROM_WIN32( GetLastError() ) );
                }

                D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>( pMem );
                UINT64* pRowSizesInBytes = reinterpret_cast<UINT64*>( pLayouts + nNumSubresources );
                UINT* pNumRows = reinterpret_cast<UINT*>( pRowSizesInBytes + nNumSubresources );

                // 这里是第二次调用GetCopyableFootprints，就得到了所有子资源的详细信息
                pThdPms->m_pID3D12Device4->GetCopyableFootprints( &stDefaultResDesc, nFirstSubresource, nNumSubresources, 0, pLayouts, pNumRows, pRowSizesInBytes, &n64RequiredSize );

                BYTE* pData = nullptr;
                GRS_THROW_IF_FAILED( pITexNormalUpload->Map( 0, nullptr, reinterpret_cast<void**>( &pData ) ) );
                // 第一遍Copy！注意3重循环每重的意思
                for ( UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes )
                {// SubResources
                    if ( pRowSizesInBytes[nSubRes] > ( SIZE_T )-1 )
                    {
                        throw CGRSCOMException( E_FAIL );
                    }

                    D3D12_MEMCPY_DEST stCopyDestData = { pData + pLayouts[nSubRes].Offset
                        , pLayouts[nSubRes].Footprint.RowPitch
                        , pLayouts[nSubRes].Footprint.RowPitch * pNumRows[nSubRes]
                    };

                    for ( UINT z = 0; z < pLayouts[nSubRes].Footprint.Depth; ++z )
                    {// Mipmap
                        BYTE* pDestSlice = reinterpret_cast<BYTE*>( stCopyDestData.pData ) + stCopyDestData.SlicePitch * z;
                        const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>( stArSubResources[nSubRes].pData ) + stArSubResources[nSubRes].SlicePitch * z;
                        for ( UINT y = 0; y < pNumRows[nSubRes]; ++y )
                        {// Rows
                            memcpy( pDestSlice + stCopyDestData.RowPitch * y,
                                pSrcSlice + stArSubResources[nSubRes].RowPitch * y,
                                (SIZE_T) pRowSizesInBytes[nSubRes] );
                        }
                    }
                }
                pITexNormalUpload->Unmap( 0, nullptr );

                // 第二次Copy！
                if ( stDefaultResDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER )
                {// Buffer 一次性复制就可以了，因为没有行对齐和行大小不一致的问题，Buffer中行数就是1
                    pThdPms->m_pICmdList->CopyBufferRegion(
                        pITexNormal.Get(), 0, pITexNormalUpload.Get(), pLayouts[0].Offset, pLayouts[0].Footprint.Width );
                }
                else
                {
                    for ( UINT nSubRes = 0; nSubRes < nNumSubresources; ++nSubRes )
                    {
                        D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
                        stDstCopyLocation.pResource = pITexNormal.Get();
                        stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                        stDstCopyLocation.SubresourceIndex = nSubRes;

                        D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
                        stSrcCopyLocation.pResource = pITexNormalUpload.Get();
                        stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                        stSrcCopyLocation.PlacedFootprint = pLayouts[nSubRes];

                        pThdPms->m_pICmdList->CopyTextureRegion( &stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr );
                    }
                }
                GRS_SAFE_FREE( pMem );
            }


            //同步
            stResStateTransBarrier.Transition.pResource = pITexNormal.Get();
            stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            pThdPms->m_pICmdList->ResourceBarrier( 1, &stResStateTransBarrier );
        }

        //2、创建世界矩阵等的常量缓冲 
        {
            stBufferResSesc.Width = szMVPBuf;//注意缓冲尺寸设置为256边界对齐大小
            GRS_THROW_IF_FAILED( pThdPms->m_pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBWVP ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBWVP );

            // Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
            GRS_THROW_IF_FAILED( pICBWVP->Map( 0, nullptr, reinterpret_cast<void**>( &pMVPBufModule ) ) );

        }

        //3、创建SRV堆 CBV SRV
        {
            D3D12_DESCRIPTOR_HEAP_DESC stSRVCBVHPDesc = {};
            stSRVCBVHPDesc.NumDescriptors = 5; // 2 CBV + 3 SRV
            stSRVCBVHPDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            stSRVCBVHPDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            GRS_THROW_IF_FAILED( pThdPms->m_pID3D12Device4->CreateDescriptorHeap( &stSRVCBVHPDesc, IID_PPV_ARGS( &pICBVSRVHeap ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBVSRVHeap );

            D3D12_CPU_DESCRIPTOR_HANDLE stDSHeapHandle =  pICBVSRVHeap->GetCPUDescriptorHandleForHeapStart() ;

            // 创建第一个CBV 世界矩阵 视矩阵 透视矩阵 
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = pICBWVP->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = static_cast<UINT>( szMVPBuf );

            pThdPms->m_pID3D12Device4->CreateConstantBufferView( &cbvDesc, stDSHeapHandle );

            stDSHeapHandle.ptr += nSRVSize ;

            // 创建第二个CBV 全局光源信息
            cbvDesc.BufferLocation = pThdPms->m_pICBLights->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = GRS_UPPER( sizeof( ST_GRS_LIGHTBUFFER ), 256 );

            pThdPms->m_pID3D12Device4->CreateConstantBufferView( &cbvDesc, stDSHeapHandle );

            stDSHeapHandle.ptr += nSRVSize;

            //创建 Diffuse Texture SRV
            D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
            stSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            stSRVDesc.Format = pITexDiffuse->GetDesc().Format;
            stSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            stSRVDesc.Texture2D.MipLevels = 1;

            pThdPms->m_pID3D12Device4->CreateShaderResourceView( pITexDiffuse.Get(), &stSRVDesc, stDSHeapHandle );

            stDSHeapHandle.ptr += nSRVSize;

            //创建 Normal Texture SRV
            stSRVDesc.Format = pITexNormal->GetDesc().Format;
            pThdPms->m_pID3D12Device4->CreateShaderResourceView( pITexNormal.Get(), &stSRVDesc, stDSHeapHandle );

            stDSHeapHandle.ptr += nSRVSize;

            //创建 Shadow Texture SRV
            stSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;//pThdPms->m_pITexShadowMap->GetDesc().Format;
            pThdPms->m_pID3D12Device4->CreateShaderResourceView( pThdPms->m_pITexShadowMap, &stSRVDesc, stDSHeapHandle );
        }

        //4、加载网格数据
        {
            CGRSMeshVertex arVertex;
            CGRSMeshIndex arIndex;
            BOOL bRet = TRUE;
            switch ( pThdPms->m_nThreadIndex )
            {
            case g_nThdSphere:
            {
                bRet = LoadSphere( 1.0f, 20, 20, arVertex, arIndex );
            }
            break;
            case g_nThdCube:
            {
                bRet = LoadBox( 2.0f, 2.0f, 2.0f, 3, arVertex, arIndex );
            }
            break;
            case g_nThdPlane:
            {
                bRet = LoadGrid( 10.0f, 10.0f, 60, 40, arVertex, arIndex );
            }
            break;
            default:
                bRet = FALSE;
                break;
            }

            if ( !bRet )
            {
                throw CGRSCOMException( E_INVALIDARG );
            }

            nVertexCnt = (UINT) arVertex.GetCount();
            nIndexCnt = (UINT) arIndex.GetCount();

            //创建 ST_GRS_VERTEX Buffer 仅使用Upload隐式堆
            stBufferResSesc.Width = nVertexCnt * sizeof( ST_GRS_VERTEX );
            GRS_THROW_IF_FAILED( pThdPms->m_pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIVB ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIVB );

            //使用map-memcpy-unmap大法将数据传至顶点缓冲对象
            UINT8* pVertexDataBegin = nullptr;
            D3D12_RANGE stReadRange = { 0, 0 };		// We do not intend to read from this resource on the CPU.

            GRS_THROW_IF_FAILED( pIVB->Map( 0, &stReadRange, reinterpret_cast<void**>( &pVertexDataBegin ) ) );
            memcpy( pVertexDataBegin, arVertex.GetData(), nVertexCnt * sizeof( ST_GRS_VERTEX ) );
            pIVB->Unmap( 0, nullptr );

            //创建 Index Buffer 仅使用Upload隐式堆
            stBufferResSesc.Width = nIndexCnt * sizeof( UINT );
            GRS_THROW_IF_FAILED( pThdPms->m_pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIIB ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIIB );

            UINT8* pIndexDataBegin = nullptr;
            GRS_THROW_IF_FAILED( pIIB->Map( 0, &stReadRange, reinterpret_cast<void**>( &pIndexDataBegin ) ) );
            memcpy( pIndexDataBegin, arIndex.GetData(), nIndexCnt * sizeof( UINT ) );
            pIIB->Unmap( 0, nullptr );

            //创建ST_GRS_VERTEX Buffer View
            stVBV.BufferLocation = pIVB->GetGPUVirtualAddress();
            stVBV.StrideInBytes = sizeof( ST_GRS_VERTEX );
            stVBV.SizeInBytes = nVertexCnt * sizeof( ST_GRS_VERTEX );

            //创建Index Buffer View
            stIBV.BufferLocation = pIIB->GetGPUVirtualAddress();
            stIBV.Format = DXGI_FORMAT_R32_UINT;
            stIBV.SizeInBytes = nIndexCnt * sizeof( UINT );
        }

        //5、设置事件对象 通知并切回主线程 完成资源的第二个Copy命令
        {
            GRS_THROW_IF_FAILED( pThdPms->m_pICmdList->Close() );
            //第一次通知主线程本线程加载资源完毕
            ::SetEvent( pThdPms->m_hEventRenderOver ); // 设置信号，通知主线程本线程资源加载完毕
        }

        // 仅用于球体跳动物理特效的变量
        float fJmpSpeed = 0.001f; //跳动速度
        float fUp = 1.0f;
        float fRawYPos = pThdPms->m_v4ModelPos.y;

        //计算旋转角度需要的变量
        double dModelRotationYAngle = 0.0f;

        DWORD dwRet = 0;
        BOOL  bQuit = FALSE;
        MSG   msg = {};

        GRS_THROW_IF_FAILED( pThdPms->m_pICmdListShadow->Close() );
        //6、渲染循环
        while ( !bQuit )
        {
            // 等待主线程通知开始渲染，同时仅接收主线程Post过来的消息，目前就是为了等待WM_QUIT消息
            dwRet = ::MsgWaitForMultipleObjects( 1, &pThdPms->m_hRunEvent, FALSE, INFINITE, QS_ALLPOSTMESSAGE );
            switch ( dwRet - WAIT_OBJECT_0 )
            {
            case 0:
            {
                switch ( pThdPms->m_nState )
                {
                case EM_GRS_RS_STARTSHADOW:
                {//Shadow Pass
                    //注意只更新一次场景，后面两遍渲染都用相同的常量缓冲了
                    // OnUpdate()
                    {
                        // 计算平移矩阵（物体相对于世界空间的位置）
                        mxPosModule = XMMatrixTranslationFromVector( XMLoadFloat4( &pThdPms->m_v4ModelPos ) );

                        //==============================================================================
                        //使用主线程更新的统一帧时间更新下物体的物理状态等，这里仅演示球体上下跳动的样子
                        //启发式的向大家说明利用多线程渲染时，物理变换可以在不同的线程中，并且在不同的CPU上并行的执行
                        if ( g_nThdSphere == pThdPms->m_nThreadIndex )
                        {// 球体多一个弹跳
                            if ( pThdPms->m_v4ModelPos.y >= 2.0f * fRawYPos )
                            {
                                fUp = -1.0f;
                                pThdPms->m_v4ModelPos.y = 2.0f * fRawYPos;
                            }

                            if ( pThdPms->m_v4ModelPos.y <= fRawYPos )
                            {
                                fUp = 1.0f;
                                pThdPms->m_v4ModelPos.y = fRawYPos;
                            }

                            pThdPms->m_v4ModelPos.y += fUp * fJmpSpeed * static_cast<float>( pThdPms->m_nCurrentTime - pThdPms->m_nStartTime );

                            mxPosModule = XMMatrixTranslationFromVector( XMLoadFloat4( &pThdPms->m_v4ModelPos ) );
                        }
                        //==============================================================================

                        //计算旋转角度
                        dModelRotationYAngle += ( ( pThdPms->m_nCurrentTime - pThdPms->m_nStartTime ) / 1000.0f ) * g_fPalstance;

                        //旋转角度是2PI周期的倍数，去掉周期数，只留下相对0弧度开始的小于2PI的弧度即可
                        if ( dModelRotationYAngle > XM_2PI )
                        {
                            dModelRotationYAngle = fmod( dModelRotationYAngle, XM_2PI );
                        }
                        // 计算相对于世界矩阵Y轴的旋转矩阵
                        XMMATRIX xmRot = XMMatrixRotationY( static_cast<float>( dModelRotationYAngle ) );

                        // 平移->旋转
                        mxPosModule = XMMatrixMultiply( mxPosModule, xmRot );
                        // Module * World
                        mxPosModule = XMMatrixMultiply( mxPosModule, XMLoadFloat4x4( &g_mxWorld ) );

                        XMStoreFloat4x4( &pMVPBufModule->g_mxModel, mxPosModule );

                        pMVPBufModule->g_mxView = g_mxView;
                        pMVPBufModule->g_mxProjection = g_mxProjection;
                    }

                    //命令分配器先Reset一下，刚才已经执行过了一个复制纹理的命令
                    GRS_THROW_IF_FAILED( pThdPms->m_pICmdAllocShadow->Reset() );
                    //Reset命令列表，并重新指定命令分配器和PSO对象
                    GRS_THROW_IF_FAILED( pThdPms->m_pICmdListShadow->Reset( pThdPms->m_pICmdAllocShadow, pThdPms->m_pIPSOShadowPass ) );
                    // OnThreadShadowRender()
                    {
                        //---------------------------------------------------------------------------------------------
                        //设置对应的渲染目标和视裁剪框(这是渲染子线程必须要做的步骤，基本也就是所谓多线程渲染的核心秘密所在了)
                        {
                            D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = g_pIDSVHeap->GetCPUDescriptorHandleForHeapStart() ;
                            //设置渲染目标(阴影渲染只绘制到深度缓冲就行)
                            pThdPms->m_pICmdListShadow->OMSetRenderTargets( 0, nullptr, FALSE, &stDSVHandle );
                            pThdPms->m_pICmdListShadow->RSSetViewports( 1, &g_stViewPort );
                            pThdPms->m_pICmdListShadow->RSSetScissorRects( 1, &g_stScissorRect );
                        }
                        //---------------------------------------------------------------------------------------------

                        //渲染（实质就是记录渲染命令列表）
                        if ( g_nThdPlane != pThdPms->m_nThreadIndex )
                        {//平面不参与阴影渲染
                            pThdPms->m_pICmdListShadow->SetGraphicsRootSignature( pThdPms->m_pIRSShadowPass );
                            pThdPms->m_pICmdListShadow->SetPipelineState( pThdPms->m_pIPSOShadowPass );
                            ID3D12DescriptorHeap* ppHeapsSphere[] = { pICBVSRVHeap.Get() };
                            pThdPms->m_pICmdListShadow->SetDescriptorHeaps( _countof( ppHeapsSphere ), ppHeapsSphere );

                            D3D12_GPU_DESCRIPTOR_HANDLE stGPUSRVHandle = pICBVSRVHeap->GetGPUDescriptorHandleForHeapStart() ;
                            //设置CBV
                            pThdPms->m_pICmdListShadow->SetGraphicsRootDescriptorTable( 0, stGPUSRVHandle );

                            //注意我们使用的渲染手法是三角形列表，也就是通常的Mesh网格
                            pThdPms->m_pICmdListShadow->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
                            pThdPms->m_pICmdListShadow->IASetVertexBuffers( 0, 1, &stVBV );
                            pThdPms->m_pICmdListShadow->IASetIndexBuffer( &stIBV );

                            //Draw Call！！！
                            pThdPms->m_pICmdListShadow->DrawIndexedInstanced( nIndexCnt, 1, 0, 0, 0 );
                        }

                        //完成渲染（即关闭命令列表，并设置同步对象通知主线程开始执行）
                        {
                            GRS_THROW_IF_FAILED( pThdPms->m_pICmdListShadow->Close() );
                            ::SetEvent( pThdPms->m_hEventShadowOver ); // 设置信号，通知主线程阴影渲染完毕
                        }
                    }
                }
                break;
                case EM_GRS_RS_STARTRENDER:
                {//Second Pass Scene Render
                    // OnUpdate()
                    {// 重新更新下View 换回摄像机空间
                        pMVPBufModule->g_mxView = g_mxView;
                    }
                    //命令分配器先Reset一下，刚才已经执行过了一个复制纹理的命令
                    GRS_THROW_IF_FAILED( pThdPms->m_pICmdAlloc->Reset() );
                    //Reset命令列表，并重新指定命令分配器和PSO对象
                    GRS_THROW_IF_FAILED( pThdPms->m_pICmdList->Reset( pThdPms->m_pICmdAlloc, pThdPms->m_pIPSOSecondPass ) );
                    // OnThreadRender()
                    {
                        //---------------------------------------------------------------------------------------------
                        //设置对应的渲染目标和视裁剪框(这是渲染子线程必须要做的步骤，基本也就是所谓多线程渲染的核心秘密所在了)
                        {
                            D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = g_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                            stRTVHandle.ptr += ( pThdPms->m_nCurrentFrameIndex * g_nRTVDescriptorSize );
                            D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = g_pIDSVHeap->GetCPUDescriptorHandleForHeapStart();
                            stDSVHandle.ptr += g_nDSVDescriptorSize;
                            //设置渲染目标
                            pThdPms->m_pICmdList->OMSetRenderTargets( 1, &stRTVHandle, FALSE, &stDSVHandle );
                            pThdPms->m_pICmdList->RSSetViewports( 1, &g_stViewPort );
                            pThdPms->m_pICmdList->RSSetScissorRects( 1, &g_stScissorRect );
                        }
                        //---------------------------------------------------------------------------------------------

                        //渲染（实质就是记录渲染命令列表）
                        {
                            pThdPms->m_pICmdList->SetGraphicsRootSignature( pThdPms->m_pIRSSecondPass );
                            pThdPms->m_pICmdList->SetPipelineState( pThdPms->m_pIPSOSecondPass );
                            ID3D12DescriptorHeap* ppHeapsSphere[] = { pICBVSRVHeap.Get(),pThdPms->m_pISampleHeap };
                            pThdPms->m_pICmdList->SetDescriptorHeaps( _countof( ppHeapsSphere ), ppHeapsSphere );

                            D3D12_GPU_DESCRIPTOR_HANDLE stGPUSRVHandle =  pICBVSRVHeap->GetGPUDescriptorHandleForHeapStart() ;
                            //设置CBV
                            pThdPms->m_pICmdList->SetGraphicsRootDescriptorTable( 0, stGPUSRVHandle );

                            stGPUSRVHandle.ptr += ( 2 * nSRVSize );
                            //设置SRV
                            pThdPms->m_pICmdList->SetGraphicsRootDescriptorTable( 1, stGPUSRVHandle );

                            //设置Sample
                            pThdPms->m_pICmdList->SetGraphicsRootDescriptorTable( 2, pThdPms->m_pISampleHeap->GetGPUDescriptorHandleForHeapStart() );

                            //注意我们使用的渲染手法是三角形列表，也就是通常的Mesh网格
                            pThdPms->m_pICmdList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
                            pThdPms->m_pICmdList->IASetVertexBuffers( 0, 1, &stVBV );
                            pThdPms->m_pICmdList->IASetIndexBuffer( &stIBV );

                            //Draw Call！！！
                            pThdPms->m_pICmdList->DrawIndexedInstanced( nIndexCnt, 1, 0, 0, 0 );
                        }

                        //完成渲染（即关闭命令列表，并设置同步对象通知主线程开始执行）
                        {
                            GRS_THROW_IF_FAILED( pThdPms->m_pICmdList->Close() );
                            ::SetEvent( pThdPms->m_hEventRenderOver ); // 设置信号，通知主线程本线程渲染完毕

                        }
                    }
                }
                break;
                default:
                {
                    bQuit = TRUE;
                }
                break;
                }
            }
            break;
            case 1:
            {//处理消息
                while ( ::PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
                {//这里只可能是别的线程发过来的消息，用于更复杂的场景
                    if ( WM_QUIT != msg.message )
                    {
                        ::TranslateMessage( &msg );
                        ::DispatchMessage( &msg );
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
    }
    catch ( CGRSCOMException& )
    {

    }

    return 0;
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch ( message )
    {
    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;
    case WM_KEYDOWN:
    {
        USHORT n16KeyCode = ( wParam & 0xFF );
        if ( VK_SPACE == n16KeyCode )
        {//按空格键切换不同的采样器看效果，以明白每种采样器具体的含义

        }
        if ( VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode )
        {
            //double g_fPalstance = 10.0f * XM_PI / 180.0f;	//物体旋转的角速度，单位：弧度/秒
            g_fPalstance += 10 * XM_PI / 180.0f;
            if ( g_fPalstance > XM_PI )
            {
                g_fPalstance = XM_PI;
            }
            //XMMatrixOrthographicOffCenterLH()
        }

        if ( VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode )
        {
            g_fPalstance -= 10 * XM_PI / 180.0f;
            if ( g_fPalstance < 0.0f )
            {
                g_fPalstance = XM_PI / 180.0f;
            }
        }

        //根据用户输入变换
        //XMVECTOR g_f3EyePos = XMVectorSet(0.0f, 5.0f, -10.0f, 0.0f); //眼睛位置
        //XMVECTOR g_f3LockAt = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);  //眼睛所盯的位置
        //XMVECTOR g_f3HeapUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);  //头部正上方位置
        XMFLOAT3 move( 0, 0, 0 );
        float fMoveSpeed = 2.0f;
        float fTurnSpeed = XM_PIDIV2 * 0.005f;

        if ( 'w' == n16KeyCode || 'W' == n16KeyCode )
        {
            move.z -= 1.0f;
        }

        if ( 's' == n16KeyCode || 'S' == n16KeyCode )
        {
            move.z += 1.0f;
        }

        if ( 'd' == n16KeyCode || 'D' == n16KeyCode )
        {
            move.x += 1.0f;
        }

        if ( 'a' == n16KeyCode || 'A' == n16KeyCode )
        {
            move.x -= 1.0f;
        }

        if ( fabs( move.x ) > 0.1f && fabs( move.z ) > 0.1f )
        {
            XMVECTOR vector = XMVector3Normalize( XMLoadFloat3( &move ) );
            move.x = XMVectorGetX( vector );
            move.z = XMVectorGetZ( vector );
        }

        if ( VK_UP == n16KeyCode )
        {
            g_fPitch += fTurnSpeed;
        }

        if ( VK_DOWN == n16KeyCode )
        {
            g_fPitch -= fTurnSpeed;
        }

        if ( VK_RIGHT == n16KeyCode )
        {
            g_fYaw -= fTurnSpeed;
        }

        if ( VK_LEFT == n16KeyCode )
        {
            g_fYaw += fTurnSpeed;
        }

        // Prevent looking too far up or down.
        g_fPitch = min( g_fPitch, XM_PIDIV4 );
        g_fPitch = max( -XM_PIDIV4, g_fPitch );

        // Move the camera in model space.
        float x = move.x * -cosf( g_fYaw ) - move.z * sinf( g_fYaw );
        float z = move.x * sinf( g_fYaw ) - move.z * cosf( g_fYaw );
        g_f3EyePos.x += x * fMoveSpeed;
        g_f3EyePos.z += z * fMoveSpeed;

        // Determine the look direction.
        float r = cosf( g_fPitch );
        g_f3LockAt.x = r * sinf( g_fYaw );
        g_f3LockAt.y = sinf( g_fPitch );
        g_f3LockAt.z = r * cosf( g_fYaw );

        if ( VK_TAB == n16KeyCode )
        {//按Tab键还原摄像机位置
            g_f3EyePos = XMFLOAT3( 0.0f, 0.0f, -10.0f ); //眼睛位置
            g_f3LockAt = XMFLOAT3( 0.0f, 0.0f, 0.0f );    //眼睛所盯的位置
            g_f3HeapUp = XMFLOAT3( 0.0f, 1.0f, 0.0f );    //头部正上方位置
        }

    }

    break;
    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }
    return 0;
}

ST_GRS_VERTEX MidPoint( const ST_GRS_VERTEX& v0, const ST_GRS_VERTEX& v1 )
{
    XMVECTOR p0 = XMLoadFloat4( &v0.m_v4Position );
    XMVECTOR p1 = XMLoadFloat4( &v1.m_v4Position );

    XMVECTOR n0 = XMLoadFloat4( &v0.m_v4Normal );
    XMVECTOR n1 = XMLoadFloat4( &v1.m_v4Normal );

    XMVECTOR tan0 = XMLoadFloat4( &v0.m_v4Tangent );
    XMVECTOR tan1 = XMLoadFloat4( &v1.m_v4Tangent );

    XMVECTOR tex0 = XMLoadFloat2( &v0.m_v2Texcoord );
    XMVECTOR tex1 = XMLoadFloat2( &v1.m_v2Texcoord );

    // Compute the midpoints of all the attributes.  Vectors need to be normalized
    // since linear interpolating can make them not unit length.  
    XMVECTOR pos = 0.5f * ( p0 + p1 );
    XMVECTOR normal = XMVector4Normalize( 0.5f * ( n0 + n1 ) );
    XMVECTOR tangent = XMVector4Normalize( 0.5f * ( tan0 + tan1 ) );
    XMVECTOR tex = 0.5f * ( tex0 + tex1 );

    ST_GRS_VERTEX stMidPoint;
    XMStoreFloat4( &stMidPoint.m_v4Position, pos );
    XMStoreFloat4( &stMidPoint.m_v4Normal, normal );
    XMStoreFloat4( &stMidPoint.m_v4Tangent, tangent );
    XMStoreFloat2( &stMidPoint.m_v2Texcoord, tex );

    return stMidPoint;
}

void Subdivide( CGRSMeshVertex& arVertex, CGRSMeshIndex& arIndices )
{
    // Save a copy of the input geometry.
    CGRSMeshVertex tmpV;
    CGRSMeshIndex tmpI;

    tmpV.Append( arVertex );
    tmpI.Append( arIndices );

    arVertex.RemoveAll();
    arIndices.RemoveAll();

    //       v1
    //       *
    //      / \
	//     /   \
	//  m0*-----*m1
    //   / \   / \
	//  /   \ /   \
	// *-----*-----*
    // v0    m2     v2

    UINT32 numTris = (UINT32) tmpI.GetCount() / 3;
    for ( UINT32 i = 0; i < numTris; ++i )
    {
        ST_GRS_VERTEX v0 = tmpV[tmpI[i * 3 + 0]];
        ST_GRS_VERTEX v1 = tmpV[tmpI[i * 3 + 1]];
        ST_GRS_VERTEX v2 = tmpV[tmpI[i * 3 + 2]];

        //
        // Generate the midpoints.
        //

        ST_GRS_VERTEX m0 = MidPoint( v0, v1 );
        ST_GRS_VERTEX m1 = MidPoint( v1, v2 );
        ST_GRS_VERTEX m2 = MidPoint( v0, v2 );

        //
        // Add new geometry.
        //

        arVertex.Add( v0 ); // 0
        arVertex.Add( v1 ); // 1
        arVertex.Add( v2 ); // 2
        arVertex.Add( m0 ); // 3
        arVertex.Add( m1 ); // 4
        arVertex.Add( m2 ); // 5

        arIndices.Add( i * 6 + 0 );
        arIndices.Add( i * 6 + 3 );
        arIndices.Add( i * 6 + 5 );

        arIndices.Add( i * 6 + 3 );
        arIndices.Add( i * 6 + 4 );
        arIndices.Add( i * 6 + 5 );

        arIndices.Add( i * 6 + 5 );
        arIndices.Add( i * 6 + 4 );
        arIndices.Add( i * 6 + 2 );

        arIndices.Add( i * 6 + 3 );
        arIndices.Add( i * 6 + 1 );
        arIndices.Add( i * 6 + 4 );
    }
}

BOOL LoadBox( FLOAT width
    , FLOAT height
    , FLOAT depth
    , UINT32 numSubdivisions
    , CGRSMeshVertex& arVertex
    , CGRSMeshIndex& arIndices )
{
    float w2 = 0.5f * width;
    float h2 = 0.5f * height;
    float d2 = 0.5f * depth;

    arVertex.SetCount( 24 );
    // Fill in the front face vertex data.
    arVertex[0] = ST_GRS_VERTEX( -w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f );
    arVertex[1] = ST_GRS_VERTEX( -w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f );
    arVertex[2] = ST_GRS_VERTEX( +w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f );
    arVertex[3] = ST_GRS_VERTEX( +w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f );

    // Fill in the back face vertex data.
    arVertex[4] = ST_GRS_VERTEX( -w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f );
    arVertex[5] = ST_GRS_VERTEX( +w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f );
    arVertex[6] = ST_GRS_VERTEX( +w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f );
    arVertex[7] = ST_GRS_VERTEX( -w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f );

    // Fill in the top face vertex data.
    arVertex[8] = ST_GRS_VERTEX( -w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f );
    arVertex[9] = ST_GRS_VERTEX( -w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f );
    arVertex[10] = ST_GRS_VERTEX( +w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f );
    arVertex[11] = ST_GRS_VERTEX( +w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f );

    // Fill in the bottom face vertex data.
    arVertex[12] = ST_GRS_VERTEX( -w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f );
    arVertex[13] = ST_GRS_VERTEX( +w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f );
    arVertex[14] = ST_GRS_VERTEX( +w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f );
    arVertex[15] = ST_GRS_VERTEX( -w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f );

    // Fill in the left face vertex data.
    arVertex[16] = ST_GRS_VERTEX( -w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f );
    arVertex[17] = ST_GRS_VERTEX( -w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f );
    arVertex[18] = ST_GRS_VERTEX( -w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f );
    arVertex[19] = ST_GRS_VERTEX( -w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f );

    // Fill in the right face vertex data.
    arVertex[20] = ST_GRS_VERTEX( +w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f );
    arVertex[21] = ST_GRS_VERTEX( +w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f );
    arVertex[22] = ST_GRS_VERTEX( +w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f );
    arVertex[23] = ST_GRS_VERTEX( +w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f );

    //
    // Create the indices.
    //
    arIndices.SetCount( 36 );

    // Fill in the front face index data
    arIndices[0] = 0; arIndices[1] = 1; arIndices[2] = 2;
    arIndices[3] = 0; arIndices[4] = 2; arIndices[5] = 3;

    // Fill in the back face index data
    arIndices[6] = 4; arIndices[7] = 5; arIndices[8] = 6;
    arIndices[9] = 4; arIndices[10] = 6; arIndices[11] = 7;

    // Fill in the top face index data
    arIndices[12] = 8; arIndices[13] = 9; arIndices[14] = 10;
    arIndices[15] = 8; arIndices[16] = 10; arIndices[17] = 11;

    // Fill in the bottom face index data
    arIndices[18] = 12; arIndices[19] = 13; arIndices[20] = 14;
    arIndices[21] = 12; arIndices[22] = 14; arIndices[23] = 15;

    // Fill in the left face index data
    arIndices[24] = 16; arIndices[25] = 17; arIndices[26] = 18;
    arIndices[27] = 16; arIndices[28] = 18; arIndices[29] = 19;

    // Fill in the right face index data
    arIndices[30] = 20; arIndices[31] = 21; arIndices[32] = 22;
    arIndices[33] = 20; arIndices[34] = 22; arIndices[35] = 23;

    // Put a cap on the number of subdivisions.
    numSubdivisions = numSubdivisions < 6u ? numSubdivisions : 6u;

    for ( UINT32 i = 0; i < numSubdivisions; ++i )
    {
        Subdivide( arVertex, arIndices );
    }
    return arVertex.GetCount() > 0 && arIndices.GetCount() > 0;
}
BOOL LoadSphere( FLOAT radius
    , UINT32 sliceCount
    , UINT32 stackCount
    , CGRSMeshVertex& arVertex
    , CGRSMeshIndex& arIndices )
{
    ST_GRS_VERTEX topVertex( 0.0f, +radius, 0.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f );
    ST_GRS_VERTEX bottomVertex( 0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f );

    arVertex.Add( topVertex );

    float phiStep = XM_PI / stackCount;
    float thetaStep = 2.0f * XM_PI / sliceCount;
    float phi = 0.0f;
    float theta = 0.0f;
    ST_GRS_VERTEX v;

    // Compute vertices for each stack ring (do not count the poles as rings).
    for ( UINT32 i = 1; i <= stackCount - 1; ++i )
    {
        phi = i * phiStep;
        // Vertices of ring.
        for ( UINT32 j = 0; j <= sliceCount; ++j )
        {
            theta = j * thetaStep;

            // spherical to cartesian
            v.m_v4Position.x = radius * sinf( phi ) * cosf( theta );
            v.m_v4Position.y = radius * cosf( phi );
            v.m_v4Position.z = radius * sinf( phi ) * sinf( theta );
            v.m_v4Position.w = 1.0f;

            // Partial derivative of P with respect to theta
            v.m_v4Tangent.x = -radius * sinf( phi ) * sinf( theta );
            v.m_v4Tangent.y = 0.0f;
            v.m_v4Tangent.z = +radius * sinf( phi ) * cosf( theta );
            v.m_v4Tangent.w = 0.0f;

            XMVECTOR T = XMLoadFloat4( &v.m_v4Tangent );
            XMStoreFloat4( &v.m_v4Tangent, XMVector4Normalize( T ) );

            XMVECTOR p = XMLoadFloat4( &v.m_v4Position );
            XMStoreFloat4( &v.m_v4Normal, XMVector4Normalize( p ) );

            v.m_v2Texcoord.x = theta / XM_2PI;
            v.m_v2Texcoord.y = phi / XM_PI;

            arVertex.Add( v );
        }
    }

    arVertex.Add( bottomVertex );

    //
    // Compute indices for top stack.  The top stack was written first to the vertex buffer
    // and connects the top pole to the first ring.
    //

    for ( UINT32 i = 1; i <= sliceCount; ++i )
    {
        arIndices.Add( 0 );
        arIndices.Add( i + 1 );
        arIndices.Add( i );
    }

    //
    // Compute indices for inner stacks (not connected to poles).
    //

    // Offset the indices to the index of the first vertex in the first ring.
    // This is just skipping the top pole vertex.
    UINT32 baseIndex = 1;
    UINT32 ringVertexCount = sliceCount + 1;
    for ( UINT32 i = 0; i < stackCount - 2; ++i )
    {
        for ( UINT32 j = 0; j < sliceCount; ++j )
        {
            arIndices.Add( baseIndex + i * ringVertexCount + j );
            arIndices.Add( baseIndex + i * ringVertexCount + j + 1 );
            arIndices.Add( baseIndex + ( i + 1 ) * ringVertexCount + j );

            arIndices.Add( baseIndex + ( i + 1 ) * ringVertexCount + j );
            arIndices.Add( baseIndex + i * ringVertexCount + j + 1 );
            arIndices.Add( baseIndex + ( i + 1 ) * ringVertexCount + j + 1 );
        }
    }

    //
    // Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
    // and connects the bottom pole to the bottom ring.
    //

    // South pole vertex was added last.
    UINT32 southPoleIndex = (UINT32) arVertex.GetCount() - 1;

    // Offset the indices to the index of the first vertex in the last ring.
    baseIndex = southPoleIndex - ringVertexCount;

    for ( UINT32 i = 0; i < sliceCount; ++i )
    {
        arIndices.Add( southPoleIndex );
        arIndices.Add( baseIndex + i );
        arIndices.Add( baseIndex + i + 1 );
    }

    return arVertex.GetCount() > 0 && arIndices.GetCount() > 0;
}

BOOL LoadGrid( float width
    , float depth
    , UINT32 m
    , UINT32 n
    , CGRSMeshVertex& arVertex
    , CGRSMeshIndex& arIndices )
{
    UINT32 vertexCount = m * n;
    UINT32 faceCount = ( m - 1 ) * ( n - 1 ) * 2;

    //
    // Create the vertices.
    //

    float halfWidth = 0.5f * width;
    float halfDepth = 0.5f * depth;

    float dx = width / ( n - 1 );
    float dz = depth / ( m - 1 );

    float du = 1.0f / ( n - 1 );
    float dv = 1.0f / ( m - 1 );

    arVertex.SetCount( vertexCount );

    for ( UINT32 i = 0; i < m; ++i )
    {
        float z = halfDepth - i * dz;
        for ( UINT32 j = 0; j < n; ++j )
        {
            float x = -halfWidth + j * dx;

            arVertex[i * n + j].m_v4Position = XMFLOAT4( x, 0.0f, z, 1.0f );
            arVertex[i * n + j].m_v4Normal = XMFLOAT4( 0.0f, 1.0f, 0.0f, 0.0f );
            arVertex[i * n + j].m_v4Tangent = XMFLOAT4( 1.0f, 0.0f, 0.0f, 0.0f );

            // Stretch texture over grid.
            arVertex[i * n + j].m_v2Texcoord.x = j * du;
            arVertex[i * n + j].m_v2Texcoord.y = i * dv;
        }
    }

    //
    // Create the indices.
    //
    arIndices.SetCount( faceCount * 3 );
    // Iterate over each quad and compute indices.
    UINT32 k = 0;
    for ( UINT32 i = 0; i < m - 1; ++i )
    {
        for ( UINT32 j = 0; j < n - 1; ++j )
        {
            arIndices[k] = i * n + j;
            arIndices[k + 1] = i * n + j + 1;
            arIndices[k + 2] = ( i + 1 ) * n + j;

            arIndices[k + 3] = ( i + 1 ) * n + j;
            arIndices[k + 4] = i * n + j + 1;
            arIndices[k + 5] = ( i + 1 ) * n + j + 1;

            k += 6; // next quad
        }
    }

    return arVertex.GetCount() > 0 && arIndices.GetCount() > 0;
}

