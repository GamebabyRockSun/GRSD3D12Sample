#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <commdlg.h>
#include <wrl.h> //添加WTL支持 方便使用COM
#include <strsafe.h>
#include <fstream>  //for ifstream

#include <atlbase.h>
#include <atlcoll.h>
#include <atlchecked.h>
#include <atlstr.h>
#include <atlconv.h>
#include <atlexcept.h>

#include <DirectXMath.h>

#include <dxgi1_6.h>
#include <d3d12.h> //for d3d12
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include "../Commons/GRS_Mem.h"
#include "../Commons/GRS_D3D12_Utility.h"

using namespace std;
using namespace DirectX;
using namespace Microsoft;
using namespace Microsoft::WRL;

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GRS_SLEEP(dwMilliseconds)  WaitForSingleObject(GetCurrentThread(),dwMilliseconds)
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ ATLTRACE("Error: 0x%08x\n",_hr); AtlThrow(_hr); }}
#define GRS_SAFE_RELEASE(p) if(nullptr != (p)){(p)->Release();(p)=nullptr;}
//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((size_t)(((A)+((B)-1))&~((B) - 1)))
//上取整除法
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))

#define GRS_WND_CLASS _T("GRS Game Window Class")
#define GRS_WND_TITLE _T("GRS DirectX12 PBR Base With Point Light Sample")

struct ST_GRS_CB_MVP
{
    XMFLOAT4X4 mxWorld;                   //世界矩阵，这里其实是Model->World的转换矩阵
    XMFLOAT4X4 mxView;                    //视矩阵
    XMFLOAT4X4 mxProjection;              //投影矩阵
    XMFLOAT4X4 mxViewProj;                //视矩阵*投影
    XMFLOAT4X4 mxMVP;                     //世界*视矩阵*投影
};

// Camera
struct ST_CB_CAMERA //: register( b1 )
{
    XMFLOAT4 m_v4CameraPos;
};

#define GRS_LIGHT_COUNT 8
// lights
struct ST_CB_LIGHTS //: register( b2 )
{
    XMFLOAT4 m_v4LightPos[GRS_LIGHT_COUNT];
    XMFLOAT4 m_v4LightClr[GRS_LIGHT_COUNT];
};

// PBR material parameters
struct ST_CB_PBR_MATERIAL //: register( b3 )
{
    XMFLOAT3   m_v3Albedo;      // 反射率
    float      m_fMetallic;     // 金属度
    float      m_fRoughness;    // 粗糙度
    float      m_fAO;           // 环境光遮蔽
};

struct ST_GRS_VERTEX
{
    XMFLOAT4 m_v4Position;		//Position
    XMFLOAT4 m_v4Normal;		//Normal
    XMFLOAT2 m_v2UV;		    //Texcoord
};

TCHAR g_pszAppPath[MAX_PATH] = {};

XMVECTOR g_v4EyePos = XMVectorSet( 0.0f, 0.0f, -3.0f, 0.0f );	//眼睛位置
XMVECTOR g_v4LookAt = XMVectorSet( 0.0f, 0.0f, 2.0f, 0.0f );		//眼睛所盯的位置
XMVECTOR g_v4UpDir = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );		    //头部正上方位置

// 缩放系数
float g_fScaling = 1.0f;

// 材质参数
float g_fMetallic = 0.01f;      // q-a 键增减
float g_fRoughness = 0.01f;     // w-s 键增减
float g_fAO = 0.03f;            // e-d 键增减

UINT        g_nCurAlbedo = 0;   // Tab键切换
XMFLOAT3    g_pv3Albedo[] =
{
    {0.97f,0.96f,0.91f}     // 银 Silver 
    ,{0.91f,0.92f,0.92f}    // 铝 Aluminum
    ,{0.76f,0.73f,0.69f}    // 钛 Titanium
    ,{0.77f,0.78f,0.78f}    // 铁 Iron
    ,{0.83f,0.81f,0.78f}    // 铂 Platinum
    ,{1.00f,0.85f,0.57f}    // 金 Gold
    ,{0.98f,0.90f,0.59f}    // 黄铜 Brass
    ,{0.97f,0.74f,0.62f}    // 铜 Copper
};
const UINT g_nAlbedoCount = _countof( g_pv3Albedo );

// 物体旋转的角速度，单位：弧度/秒
double g_fPalstance = 10.0f * XM_PI / 180.0f;

BOOL LoadMeshVertex( const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices );
LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

int APIENTRY _tWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow )
{
    int									iWidth = 1280;
    int									iHeight = 800;
    HWND								hWnd = nullptr;
    MSG									msg = {};

    const UINT						    nFrameBackBufCount = 3u;
    UINT								nFrameIndex = 0;
    DXGI_FORMAT				            emRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT                         emDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
    const float						    faClearColor[] = { 0.17647f, 0.549f, 0.941176f, 1.0f };

    UINT								nRTVDescriptorSize = 0U;
    UINT								nCBVSRVDescriptorSize = 0;
    UINT								nSamplerDescriptorSize = 0;

    D3D12_VIEWPORT			            stViewPort = { 0.0f, 0.0f, static_cast<float>( iWidth ), static_cast<float>( iHeight ), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    D3D12_RECT					        stScissorRect = { 0, 0, static_cast<LONG>( iWidth ), static_cast<LONG>( iHeight ) };

    D3D_FEATURE_LEVEL					emFeatureLevel = D3D_FEATURE_LEVEL_12_1;

    ComPtr<IDXGIFactory6>				pIDXGIFactory6;
    ComPtr<ID3D12Device4>				pID3D12Device4;

    ComPtr<ID3D12CommandQueue>			pIMainCMDQueue;
    ComPtr<ID3D12CommandAllocator>		pIMainCMDAlloc;
    ComPtr<ID3D12GraphicsCommandList>	pIMainCMDList;

    ComPtr<ID3D12Fence>					pIFence;
    UINT64								n64FenceValue = 0ui64;
    HANDLE								hEventFence = nullptr;

    ComPtr<IDXGISwapChain3>				pISwapChain3;
    ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;
    ComPtr<ID3D12Resource>				pIARenderTargets[nFrameBackBufCount];
    ComPtr<ID3D12DescriptorHeap>		pIDSVHeap;
    ComPtr<ID3D12Resource>				pIDepthStencilBuffer;

    const UINT                          nConstBufferCount = 4;  // 本示例中需要4个Const Buffer

    ComPtr<ID3D12RootSignature>		    pIRootSignature;
    ComPtr<ID3D12PipelineState>			pIPSOModel;

    ComPtr<ID3D12DescriptorHeap>		pICBVSRVHeap;

    UINT                                nIndexCnt = 0;
    ComPtr<ID3D12Resource>				pIVB;
    ComPtr<ID3D12Resource>				pIIB;

    D3D12_VERTEX_BUFFER_VIEW		    stVBV = {};
    D3D12_INDEX_BUFFER_VIEW			    stIBV = {};

    ComPtr<ID3D12Resource>			    pICBMVP;
    ST_GRS_CB_MVP*                      pstCBMVP = nullptr;
    ComPtr<ID3D12Resource>			    pICBCameraPos;
    ST_CB_CAMERA*                       pstCBCameraPos = nullptr;
    ComPtr<ID3D12Resource>			    pICBLights;
    ST_CB_LIGHTS*                       pstCBLights = nullptr;
    ComPtr<ID3D12Resource>			    pICBPBRMaterial;
    ST_CB_PBR_MATERIAL*                 pstCBPBRMaterial = nullptr;

    D3D12_HEAP_PROPERTIES stDefaultHeapProps = {};
    stDefaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    stDefaultHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    stDefaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    stDefaultHeapProps.CreationNodeMask = 0;
    stDefaultHeapProps.VisibleNodeMask = 0;

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

    USES_CONVERSION;
    try
    {
        ::CoInitialize( nullptr );  //for WIC & COM

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
            wcex.lpszClassName = GRS_WND_CLASS;
            RegisterClassEx( &wcex );

            DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
            RECT rtWnd = { 0, 0, iWidth, iHeight };
            AdjustWindowRect( &rtWnd, dwWndStyle, FALSE );

            // 计算窗口居中的屏幕坐标
            INT posX = ( GetSystemMetrics( SM_CXSCREEN ) - rtWnd.right - rtWnd.left ) / 2;
            INT posY = ( GetSystemMetrics( SM_CYSCREEN ) - rtWnd.bottom - rtWnd.top ) / 2;

            hWnd = CreateWindowW( GRS_WND_CLASS
                , GRS_WND_TITLE
                , dwWndStyle
                , posX
                , posY
                , rtWnd.right - rtWnd.left
                , rtWnd.bottom - rtWnd.top
                , nullptr
                , nullptr
                , hInstance
                , nullptr );

            if ( !hWnd )
            {
                GRS_THROW_IF_FAILED( HRESULT_FROM_WIN32( GetLastError() ) );
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
            ComPtr<IDXGIFactory5> pIDXGIFactory5;
            GRS_THROW_IF_FAILED( CreateDXGIFactory2( nDXGIFactoryFlags, IID_PPV_ARGS( &pIDXGIFactory5 ) ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIDXGIFactory5 );

            //获取IDXGIFactory6接口
            GRS_THROW_IF_FAILED( pIDXGIFactory5.As( &pIDXGIFactory6 ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIDXGIFactory6 );
        }

        // 3、枚举适配器创建设备
        { // 直接枚举最高性能的显卡来运行示例
            ComPtr<IDXGIAdapter1> pIAdapter1;
            GRS_THROW_IF_FAILED( pIDXGIFactory6->EnumAdapterByGpuPreference( 0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS( &pIAdapter1 ) ) );
            GRS_SET_DXGI_DEBUGNAME_COMPTR( pIAdapter1 );

            // 创建D3D12.1的设备
            GRS_THROW_IF_FAILED( D3D12CreateDevice( pIAdapter1.Get(), emFeatureLevel, IID_PPV_ARGS( &pID3D12Device4 ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pID3D12Device4 );

            DXGI_ADAPTER_DESC1 stAdapterDesc = {};
            TCHAR pszWndTitle[MAX_PATH] = {};
            GRS_THROW_IF_FAILED( pIAdapter1->GetDesc1( &stAdapterDesc ) );
            ::GetWindowText( hWnd, pszWndTitle, MAX_PATH );
            StringCchPrintf( pszWndTitle, MAX_PATH, _T( "%s (GPU[0]:%s)" ), pszWndTitle, stAdapterDesc.Description );
            ::SetWindowText( hWnd, pszWndTitle );

            // 得到每个描述符元素的大小
            nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
            nSamplerDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
            nCBVSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
        }

        // 4、创建命令队列&命令列表
        {
            D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
            stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandQueue( &stQueueDesc, IID_PPV_ARGS( &pIMainCMDQueue ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCMDQueue );

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &pIMainCMDAlloc ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCMDAlloc );

            // 创建图形命令列表
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT, pIMainCMDAlloc.Get(), nullptr, IID_PPV_ARGS( &pIMainCMDList ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIMainCMDList );
        }

        // 5、创建交换链、深度缓冲、描述符堆、描述符
        {
            DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
            stSwapChainDesc.BufferCount = nFrameBackBufCount;
            stSwapChainDesc.Width = 0;//iWidth;
            stSwapChainDesc.Height = 0;// iHeight;
            stSwapChainDesc.Format = emRenderTargetFormat;
            stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            stSwapChainDesc.SampleDesc.Count = 1;
            stSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            stSwapChainDesc.Scaling = DXGI_SCALING_NONE;

            // 交换链需要命令队列，Present命令要执行
            ComPtr<IDXGISwapChain1> pISwapChain1;
            GRS_THROW_IF_FAILED( pIDXGIFactory6->CreateSwapChainForHwnd(
                pIMainCMDQueue.Get(), hWnd, &stSwapChainDesc, nullptr, nullptr, &pISwapChain1 ) );

            GRS_SET_DXGI_DEBUGNAME_COMPTR( pISwapChain1 );

            GRS_THROW_IF_FAILED( pISwapChain1.As( &pISwapChain3 ) );

            GRS_SET_DXGI_DEBUGNAME_COMPTR( pISwapChain3 );

            //得到当前后缓冲区的序号，也就是下一个将要呈送显示的缓冲区的序号
            //注意此处使用了高版本的SwapChain接口的函数
            nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

            //创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
            {
                D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
                stRTVHeapDesc.NumDescriptors = nFrameBackBufCount;
                stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

                GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stRTVHeapDesc, IID_PPV_ARGS( &pIRTVHeap ) ) );
                GRS_SET_D3D12_DEBUGNAME_COMPTR( pIRTVHeap );
            }

           //创建RTV的描述符
            {
                D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
                    = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                for ( UINT i = 0; i < nFrameBackBufCount; i++ )
                {
                    GRS_THROW_IF_FAILED( pISwapChain3->GetBuffer( i, IID_PPV_ARGS( &pIARenderTargets[i] ) ) );
                    GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR( pIARenderTargets, i );
                    pID3D12Device4->CreateRenderTargetView( pIARenderTargets[i].Get(), nullptr, stRTVHandle );
                    stRTVHandle.ptr += nRTVDescriptorSize;
                }
            }

            // 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
            // 注意将这个屏蔽Alt+Enter组合键的动作放到了这里，是因为直到这个时候DXGI Factory通过Create Swapchain动作才知道了HWND是哪个窗口
            // 因为组合键的处理在Windows中都是基于窗口过程的，没有窗口句柄就不知道是哪个窗口以及窗口过程，之前的屏蔽就没有意义了
            GRS_THROW_IF_FAILED( pIDXGIFactory6->MakeWindowAssociation( hWnd, DXGI_MWA_NO_ALT_ENTER ) );


            // 创建深度缓冲、深度缓冲描述符堆
            D3D12_DEPTH_STENCIL_VIEW_DESC stDepthStencilDesc = {};
            stDepthStencilDesc.Format = emDepthStencilFormat;
            stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

            D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
            depthOptimizedClearValue.Format = emDepthStencilFormat;
            depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
            depthOptimizedClearValue.DepthStencil.Stencil = 0;

            D3D12_RESOURCE_DESC stDSTex2DDesc = {};
            stDSTex2DDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            stDSTex2DDesc.Alignment = 0;
            stDSTex2DDesc.Width = iWidth;
            stDSTex2DDesc.Height = iHeight;
            stDSTex2DDesc.DepthOrArraySize = 1;
            stDSTex2DDesc.MipLevels = 0;
            stDSTex2DDesc.Format = emDepthStencilFormat;
            stDSTex2DDesc.SampleDesc.Count = 1;
            stDSTex2DDesc.SampleDesc.Quality = 0;
            stDSTex2DDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            stDSTex2DDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            //使用隐式默认堆创建一个深度蜡板缓冲区，
            //因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stDefaultHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stDSTex2DDesc
                , D3D12_RESOURCE_STATE_DEPTH_WRITE
                , &depthOptimizedClearValue
                , IID_PPV_ARGS( &pIDepthStencilBuffer )
            ) );

            //==============================================================================================
            D3D12_DESCRIPTOR_HEAP_DESC stDSVHeapDesc = {};
            stDSVHeapDesc.NumDescriptors = 1;
            stDSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            stDSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stDSVHeapDesc, IID_PPV_ARGS( &pIDSVHeap ) ) );

            pID3D12Device4->CreateDepthStencilView( pIDepthStencilBuffer.Get(), &stDepthStencilDesc, pIDSVHeap->GetCPUDescriptorHandleForHeapStart() );
        }

        // 6、创建围栏
        if ( TRUE )
        {
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &pIFence ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIFence );

            n64FenceValue = 1;

            // 创建一个Event同步对象，用于等待围栏事件通知
            hEventFence = CreateEvent( nullptr, FALSE, FALSE, nullptr );
            if ( hEventFence == nullptr )
            {
                GRS_THROW_IF_FAILED( HRESULT_FROM_WIN32( GetLastError() ) );
            }
        }

        // 7、编译Shader创建渲染管线状态对象
        {
            // 先创建根签名
            D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
            // 检测是否支持V1.1版本的根签名
            stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
            if ( FAILED( pID3D12Device4->CheckFeatureSupport( D3D12_FEATURE_ROOT_SIGNATURE, &stFeatureData, sizeof( stFeatureData ) ) ) )
            {// 1.0版 直接丢异常退出了
                GRS_THROW_IF_FAILED( E_NOTIMPL );
            }

            D3D12_DESCRIPTOR_RANGE1 stDSPRanges1[1] = {};
            stDSPRanges1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
            stDSPRanges1[0].NumDescriptors = nConstBufferCount;
            stDSPRanges1[0].BaseShaderRegister = 0;
            stDSPRanges1[0].RegisterSpace = 0;
            stDSPRanges1[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
            stDSPRanges1[0].OffsetInDescriptorsFromTableStart = 0;

            D3D12_ROOT_PARAMETER1 stRootParameters1[1] = {};
            stRootParameters1[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            stRootParameters1[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
            stRootParameters1[0].DescriptorTable.NumDescriptorRanges = 1;
            stRootParameters1[0].DescriptorTable.pDescriptorRanges = &stDSPRanges1[0];

            D3D12_VERSIONED_ROOT_SIGNATURE_DESC stRootSignatureDesc = {};
            stRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
            stRootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
            stRootSignatureDesc.Desc_1_1.NumParameters = _countof( stRootParameters1 );
            stRootSignatureDesc.Desc_1_1.pParameters = stRootParameters1;
            stRootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
            stRootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;

            ComPtr<ID3DBlob> pISignatureBlob;
            ComPtr<ID3DBlob> pIErrorBlob;

            HRESULT _hr = D3D12SerializeVersionedRootSignature( &stRootSignatureDesc, &pISignatureBlob, &pIErrorBlob );

            if ( FAILED( _hr ) )
            {
                ATLTRACE( "Create Root Signature Error：%s\n", (CHAR*) ( pIErrorBlob ? pIErrorBlob->GetBufferPointer() : "未知错误" ) );
                AtlThrow( _hr );
            }

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateRootSignature( 0, pISignatureBlob->GetBufferPointer()
                , pISignatureBlob->GetBufferSize(), IID_PPV_ARGS( &pIRootSignature ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIRootSignature );

            // 接着以函数方式编译Shader
            UINT compileFlags = 0;
#if defined(_DEBUG)
            // Enable better shader debugging with the graphics debugging tools.
            compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            //编译为行矩阵形式	   
            compileFlags |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

            TCHAR pszShaderFileName[MAX_PATH] = {};
            ComPtr<ID3DBlob> pIVSModel;
            ComPtr<ID3DBlob> pIPSModel;
            pIErrorBlob.Reset();
            StringCchPrintf( pszShaderFileName, MAX_PATH, _T( "%s18-PBR-Base-Point-Lights\\Shader\\VS.hlsl" ), g_pszAppPath );
            HRESULT hr = D3DCompileFromFile( pszShaderFileName, nullptr, nullptr, "VSMain", "vs_5_1", compileFlags, 0, &pIVSModel, &pIErrorBlob );

            if ( FAILED( hr ) )
            {
                ATLTRACE( "编译 Vertex Shader：\"%s\" 发生错误：%s\n"
                    , T2A( pszShaderFileName )
                    , pIErrorBlob ? pIErrorBlob->GetBufferPointer() : "无法读取文件！" );
                GRS_THROW_IF_FAILED( hr );
            }
            pIErrorBlob.Reset();
            StringCchPrintf( pszShaderFileName, MAX_PATH, _T( "%s18-PBR-Base-Point-Lights\\Shader\\PS_PBR_Base_Point_Lights.hlsl" ), g_pszAppPath );
            hr = D3DCompileFromFile( pszShaderFileName, nullptr, nullptr, "PSMain", "ps_5_1", compileFlags, 0, &pIPSModel, &pIErrorBlob );
            if ( FAILED( hr ) )
            {
                ATLTRACE( "编译 Pixel Shader：\"%s\" 发生错误：%s\n"
                    , T2A( pszShaderFileName )
                    , pIErrorBlob ? pIErrorBlob->GetBufferPointer() : "无法读取文件！" );
                GRS_THROW_IF_FAILED( hr );
            }

            D3D12_INPUT_ELEMENT_DESC stIALayoutSphere[] =
            {
                { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "NORMAL",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
                { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            };

           // 创建 graphics pipeline state object (PSO)对象
            D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
            stPSODesc.InputLayout = { stIALayoutSphere, _countof( stIALayoutSphere ) };

            stPSODesc.pRootSignature = pIRootSignature.Get();
            stPSODesc.VS.pShaderBytecode = pIVSModel->GetBufferPointer();
            stPSODesc.VS.BytecodeLength = pIVSModel->GetBufferSize();
            stPSODesc.PS.pShaderBytecode = pIPSModel->GetBufferPointer();
            stPSODesc.PS.BytecodeLength = pIPSModel->GetBufferSize();

            stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

            stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
            stPSODesc.BlendState.IndependentBlendEnable = FALSE;
            stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            stPSODesc.DSVFormat = emDepthStencilFormat;
            stPSODesc.DepthStencilState.DepthEnable = TRUE;
            stPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;//启用深度缓存写入功能
            stPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;     //深度测试函数（该值为普通的深度测试）
            stPSODesc.DepthStencilState.StencilEnable = FALSE;

            stPSODesc.SampleMask = UINT_MAX;
            stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            stPSODesc.NumRenderTargets = 1;
            stPSODesc.RTVFormats[0] = emRenderTargetFormat;
            stPSODesc.SampleDesc.Count = 1;

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateGraphicsPipelineState( &stPSODesc, IID_PPV_ARGS( &pIPSOModel ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIPSOModel );
        }

        // 8、创建 SRV CBV Sample堆
        if ( TRUE )
        {
            //将纹理视图描述符和CBV描述符放在一个描述符堆上
            D3D12_DESCRIPTOR_HEAP_DESC stSRVHeapDesc = {};

            stSRVHeapDesc.NumDescriptors = nConstBufferCount; // nConstBufferCount * CBV + nTextureCount * SRV
            stSRVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            stSRVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            GRS_THROW_IF_FAILED( pID3D12Device4->CreateDescriptorHeap( &stSRVHeapDesc, IID_PPV_ARGS( &pICBVSRVHeap ) ) );

            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBVSRVHeap );
        }

        // 9、创建常量缓冲和对应描述符
        {
            // MVP Const Buffer
            stBufferResSesc.Width = GRS_UPPER( sizeof( ST_GRS_CB_MVP ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc //注意缓冲尺寸设置为256边界对齐大小
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBMVP ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBMVP );

            // Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
            GRS_THROW_IF_FAILED( pICBMVP->Map( 0, nullptr, reinterpret_cast<void**>( &pstCBMVP ) ) );

            stBufferResSesc.Width = GRS_UPPER( sizeof( ST_CB_CAMERA ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc //注意缓冲尺寸设置为256边界对齐大小
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBCameraPos ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBCameraPos );

            // Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
            GRS_THROW_IF_FAILED( pICBCameraPos->Map( 0, nullptr, reinterpret_cast<void**>( &pstCBCameraPos ) ) );

            stBufferResSesc.Width = GRS_UPPER( sizeof( ST_CB_LIGHTS ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc //注意缓冲尺寸设置为256边界对齐大小
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBLights ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBLights );

            // Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
            GRS_THROW_IF_FAILED( pICBLights->Map( 0, nullptr, reinterpret_cast<void**>( &pstCBLights ) ) );

            stBufferResSesc.Width = GRS_UPPER( sizeof( ST_CB_PBR_MATERIAL ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc //注意缓冲尺寸设置为256边界对齐大小
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pICBPBRMaterial ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pICBPBRMaterial );

            // Map 之后就不再Unmap了 直接复制数据进去 这样每帧都不用map-copy-unmap浪费时间了
            GRS_THROW_IF_FAILED( pICBPBRMaterial->Map( 0, nullptr, reinterpret_cast<void**>( &pstCBPBRMaterial ) ) );

            // Create Const Buffer View 
            D3D12_CPU_DESCRIPTOR_HANDLE stCBVHandle = pICBVSRVHeap->GetCPUDescriptorHandleForHeapStart();

            // 创建第一个CBV 世界矩阵 视矩阵 透视矩阵 
            D3D12_CONSTANT_BUFFER_VIEW_DESC stCBVDesc = {};
            stCBVDesc.BufferLocation = pICBMVP->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_GRS_CB_MVP ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            pID3D12Device4->CreateConstantBufferView( &stCBVDesc, stCBVHandle );

            stCBVHandle.ptr += nCBVSRVDescriptorSize;
            stCBVDesc.BufferLocation = pICBCameraPos->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_CB_CAMERA ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            pID3D12Device4->CreateConstantBufferView( &stCBVDesc, stCBVHandle );

            stCBVHandle.ptr += nCBVSRVDescriptorSize;
            stCBVDesc.BufferLocation = pICBLights->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_CB_LIGHTS ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            pID3D12Device4->CreateConstantBufferView( &stCBVDesc, stCBVHandle );

            stCBVHandle.ptr += nCBVSRVDescriptorSize;
            stCBVDesc.BufferLocation = pICBPBRMaterial->GetGPUVirtualAddress();
            stCBVDesc.SizeInBytes = (UINT) GRS_UPPER( sizeof( ST_CB_PBR_MATERIAL ), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT );
            pID3D12Device4->CreateConstantBufferView( &stCBVDesc, stCBVHandle );
        }

        // 10、创建D3D12的模型资源，并复制数据
        {
            CHAR pszMeshFile[MAX_PATH] = {};
            StringCchPrintfA( pszMeshFile, MAX_PATH, "%sAssets\\sphere.txt", T2A( g_pszAppPath ) );

            ST_GRS_VERTEX* pstVertices = nullptr;
            UINT* pnIndices = nullptr;
            UINT  nVertexCnt = 0;
            LoadMeshVertex( pszMeshFile, nVertexCnt, pstVertices, pnIndices );
            nIndexCnt = nVertexCnt;

            stBufferResSesc.Width = nVertexCnt * sizeof( ST_GRS_VERTEX );
            //创建 Vertex Buffer 仅使用Upload隐式堆
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
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
            memcpy( pVertexDataBegin, pstVertices, nVertexCnt * sizeof( ST_GRS_VERTEX ) );
            pIVB->Unmap( 0, nullptr );

            //创建 Index Buffer 仅使用Upload隐式堆
            stBufferResSesc.Width = nIndexCnt * sizeof( UINT );
            GRS_THROW_IF_FAILED( pID3D12Device4->CreateCommittedResource(
                &stUploadHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stBufferResSesc
                , D3D12_RESOURCE_STATE_GENERIC_READ
                , nullptr
                , IID_PPV_ARGS( &pIIB ) ) );
            GRS_SET_D3D12_DEBUGNAME_COMPTR( pIIB );

            UINT8* pIndexDataBegin = nullptr;
            GRS_THROW_IF_FAILED( pIIB->Map( 0, &stReadRange, reinterpret_cast<void**>( &pIndexDataBegin ) ) );
            memcpy( pIndexDataBegin, pnIndices, nIndexCnt * sizeof( UINT ) );
            pIIB->Unmap( 0, nullptr );

            //创建Vertex Buffer View
            stVBV.BufferLocation = pIVB->GetGPUVirtualAddress();
            stVBV.StrideInBytes = sizeof( ST_GRS_VERTEX );
            stVBV.SizeInBytes = nVertexCnt * sizeof( ST_GRS_VERTEX );

            //创建Index Buffer View
            stIBV.BufferLocation = pIIB->GetGPUVirtualAddress();
            stIBV.Format = DXGI_FORMAT_R32_UINT;
            stIBV.SizeInBytes = nIndexCnt * sizeof( UINT );

            GRS_SAFE_FREE( pstVertices );
            GRS_SAFE_FREE( pnIndices );
        }

        DWORD dwRet = 0;
        BOOL bExit = FALSE;
        D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = pIDSVHeap->GetCPUDescriptorHandleForHeapStart();

        GRS_THROW_IF_FAILED( pIMainCMDList->Close() );
        ::SetEvent( hEventFence );

        FLOAT fStartTime = ( FLOAT )::GetTickCount64();
        FLOAT fCurrentTime = fStartTime;
        //计算旋转角度需要的变量
        double dModelRotationYAngle = 0.0f;

        ShowWindow( hWnd, nCmdShow );
        UpdateWindow( hWnd );

        // 16、消息循环
        while ( !bExit )
        {
            dwRet = ::MsgWaitForMultipleObjects( 1, &hEventFence, FALSE, INFINITE, QS_ALLINPUT );
            switch ( dwRet - WAIT_OBJECT_0 )
            {
            case 0:
            {
                //-----------------------------------------------------------------------------------------------------
                // OnUpdate()
                // Model Matrix
                // -----
                // 缩放
                XMMATRIX mxModel = XMMatrixScaling( g_fScaling, g_fScaling, g_fScaling );
                // -----
                // 旋转
                fCurrentTime = ( float )::GetTickCount64();
                //计算旋转的角度：旋转角度(弧度) = 时间(秒) * 角速度(弧度/秒)
                //下面这句代码相当于经典游戏消息循环中的OnUpdate函数中需要做的事情
                dModelRotationYAngle += ( ( fCurrentTime - fStartTime ) / 1000.0f ) * g_fPalstance;
                fStartTime = fCurrentTime;
                //旋转角度是2PI周期的倍数，去掉周期数，只留下相对0弧度开始的小于2PI的弧度即可
                if ( dModelRotationYAngle > XM_2PI )
                {
                    dModelRotationYAngle = fmod( dModelRotationYAngle, XM_2PI );
                }
                //计算 World 矩阵 这里是个旋转矩阵
                mxModel = XMMatrixMultiply( mxModel, XMMatrixRotationY( static_cast<float>( dModelRotationYAngle ) ) );

                // -----
                // 位移
                mxModel = XMMatrixMultiply( mxModel, XMMatrixTranslation( 0.0f, 0.0f, 0.0f ) );

                // World Matrix
                XMMATRIX mxWorld = XMMatrixIdentity();
                mxWorld = XMMatrixMultiply( mxModel, mxWorld );
                XMStoreFloat4x4( &pstCBMVP->mxWorld, mxWorld );

                // View Matrix
                XMMATRIX mxView = XMMatrixLookAtLH( g_v4EyePos, g_v4LookAt, g_v4UpDir );
                XMStoreFloat4x4( &pstCBMVP->mxView, mxView );

                // Projection Matrix
                XMMATRIX mxProjection = XMMatrixPerspectiveFovLH( XM_PIDIV4, (FLOAT) iWidth / (FLOAT) iHeight, 0.1f, 10000.0f );
                XMStoreFloat4x4( &pstCBMVP->mxProjection, mxProjection );

                // View Matrix * Projection Matrix
                XMMATRIX mxViewProj = XMMatrixMultiply( mxView, mxProjection );
                XMStoreFloat4x4( &pstCBMVP->mxViewProj, mxViewProj );

                // World Matrix * View Matrix * Projection Matrix
                XMMATRIX mxMVP = XMMatrixMultiply( mxWorld, mxViewProj );
                XMStoreFloat4x4( &pstCBMVP->mxMVP, mxMVP );

                // Camera Position
                XMStoreFloat4( &pstCBCameraPos->m_v4CameraPos, g_v4EyePos );

                // Lights
                {
                    float fZ = -3.0f;
                    // 4个前置灯
                    pstCBLights->m_v4LightPos[0] = { 0.0f,0.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[0] = { 23.47f,21.31f,20.79f, 1.0f };

                    pstCBLights->m_v4LightPos[1] = { 0.0f,3.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[1] = { 53.47f,41.31f,40.79f, 1.0f };

                    pstCBLights->m_v4LightPos[2] = { 3.0f,-3.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[2] = { 23.47f,21.31f,20.79f, 1.0f };

                    pstCBLights->m_v4LightPos[3] = { -3.0f,-3.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[3] = { 23.47f,21.31f,20.79f, 1.0f };

                    // 其它方位灯
                    fZ = 0.0f;
                    float fY = 5.0f;
                    pstCBLights->m_v4LightPos[4] = { 0.0f,0.0f,fZ,1.0f };
                    pstCBLights->m_v4LightClr[4] = { 23.47f,21.31f,20.79f, 1.0f };

                    pstCBLights->m_v4LightPos[5] = { 0.0f,-fY,fZ,1.0f };
                    pstCBLights->m_v4LightClr[5] = { 53.47f,41.31f,40.79f, 1.0f };

                    pstCBLights->m_v4LightPos[6] = { fY,fY,fZ,1.0f };
                    pstCBLights->m_v4LightClr[6] = { 23.47f,21.31f,20.79f, 1.0f };

                    pstCBLights->m_v4LightPos[7] = { -fY,fY,fZ,1.0f };
                    pstCBLights->m_v4LightClr[7] = { 23.47f,21.31f,20.79f, 1.0f };
                }

                // PBR Material          
                pstCBPBRMaterial->m_v3Albedo = g_pv3Albedo[g_nCurAlbedo];
                pstCBPBRMaterial->m_fMetallic = g_fMetallic; // 0.04 - 1.0 之间
                pstCBPBRMaterial->m_fRoughness = g_fRoughness;
                pstCBPBRMaterial->m_fAO = g_fAO;
                //-----------------------------------------------------------------------------------------------------

                //-----------------------------------------------------------------------------------------------------
                // OnRender()
                //----------------------------------------------------------------------------------------------------- 
                //命令分配器先Reset一下
                GRS_THROW_IF_FAILED( pIMainCMDAlloc->Reset() );
                //Reset命令列表，并重新指定命令分配器和PSO对象
                GRS_THROW_IF_FAILED( pIMainCMDList->Reset( pIMainCMDAlloc.Get(), pIPSOModel.Get() ) );

                nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

                pIMainCMDList->RSSetViewports( 1, &stViewPort );
                pIMainCMDList->RSSetScissorRects( 1, &stScissorRect );

                // 通过资源屏障判定后缓冲已经切换完毕可以开始渲染了
                stResStateTransBarrier.Transition.pResource = pIARenderTargets[nFrameIndex].Get();
                stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
                stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
                pIMainCMDList->ResourceBarrier( 1, &stResStateTransBarrier );

                //开始记录命令
                stRTVHandle = pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                stRTVHandle.ptr += (size_t) nFrameIndex * nRTVDescriptorSize;

                //设置渲染目标
                pIMainCMDList->OMSetRenderTargets( 1, &stRTVHandle, FALSE, &stDSVHandle );

                // 继续记录命令，并真正开始新一帧的渲染
                pIMainCMDList->ClearRenderTargetView( stRTVHandle, faClearColor, 0, nullptr );
                pIMainCMDList->ClearDepthStencilView( pIDSVHeap->GetCPUDescriptorHandleForHeapStart()
                    , D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );

                // 渲染物体
                pIMainCMDList->SetGraphicsRootSignature( pIRootSignature.Get() );

                // 注意我们使用的渲染手法是三角形列表，也就是通常的Mesh网格
                pIMainCMDList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

                pIMainCMDList->IASetVertexBuffers( 0, 1, &stVBV );
                pIMainCMDList->IASetIndexBuffer( &stIBV );

                ID3D12DescriptorHeap* ppHeaps[] = { pICBVSRVHeap.Get() };
                pIMainCMDList->SetDescriptorHeaps( _countof( ppHeaps ), ppHeaps );

                D3D12_GPU_DESCRIPTOR_HANDLE stGPUSRVHandle = { pICBVSRVHeap->GetGPUDescriptorHandleForHeapStart() };
                // 设置CBV
                pIMainCMDList->SetGraphicsRootDescriptorTable( 0, stGPUSRVHandle );

                // Draw Call
                pIMainCMDList->DrawIndexedInstanced( nIndexCnt, 1, 0, 0, 0 );

                //又一个资源屏障，用于确定渲染已经结束可以提交画面去显示了
                stResStateTransBarrier.Transition.pResource = pIARenderTargets[nFrameIndex].Get();
                stResStateTransBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
                stResStateTransBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
                pIMainCMDList->ResourceBarrier( 1, &stResStateTransBarrier );

                //关闭命令列表，可以去执行了
                GRS_THROW_IF_FAILED( pIMainCMDList->Close() );

                //执行命令列表
                ID3D12CommandList* ppCommandLists[] = { pIMainCMDList.Get() };
                pIMainCMDQueue->ExecuteCommandLists( _countof( ppCommandLists ), ppCommandLists );

                //提交画面
                GRS_THROW_IF_FAILED( pISwapChain3->Present( 1, 0 ) );

                //开始同步GPU与CPU的执行，先记录围栏标记值
                const UINT64 n64CurrentFenceValue = n64FenceValue;
                GRS_THROW_IF_FAILED( pIMainCMDQueue->Signal( pIFence.Get(), n64CurrentFenceValue ) );
                n64FenceValue++;
                GRS_THROW_IF_FAILED( pIFence->SetEventOnCompletion( n64CurrentFenceValue, hEventFence ) );
                //-----------------------------------------------------------------------------------------------------
            }
            break;
            case 1:
            {
                while ( ::PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
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
            default:
                break;
            }
        }
    }
    catch ( CAtlException& e )
    {//发生了COM异常
        e;
    }
    catch ( ... )
    {

    }
    return 0;
}

BOOL LoadMeshVertex( const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices )
{
    ifstream fin;
    char input;
    BOOL bRet = TRUE;
    try
    {
        fin.open( pszMeshFileName );
        if ( fin.fail() )
        {
            AtlThrow( E_FAIL );
        }
        fin.get( input );
        while ( input != ':' )
        {
            fin.get( input );
        }
        fin >> nVertexCnt;

        fin.get( input );
        while ( input != ':' )
        {
            fin.get( input );
        }
        fin.get( input );
        fin.get( input );

        ppVertex = (ST_GRS_VERTEX*) GRS_CALLOC( nVertexCnt * sizeof( ST_GRS_VERTEX ) );
        ppIndices = (UINT*) GRS_CALLOC( nVertexCnt * sizeof( UINT ) );

        for ( UINT i = 0; i < nVertexCnt; i++ )
        {
            fin >> ppVertex[i].m_v4Position.x >> ppVertex[i].m_v4Position.y >> ppVertex[i].m_v4Position.z;
            ppVertex[i].m_v4Position.w = 1.0f;      //点的第4维坐标设为1
            fin >> ppVertex[i].m_v2UV.x >> ppVertex[i].m_v2UV.y;
            fin >> ppVertex[i].m_v4Normal.x >> ppVertex[i].m_v4Normal.y >> ppVertex[i].m_v4Normal.z;
            ppVertex[i].m_v4Normal.w = 0.0f;        //纯向量的第4维坐标设为0
            ppIndices[i] = i;
        }
    }
    catch ( CAtlException& e )
    {
        e;
        bRet = FALSE;
    }
    return bRet;
}

LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch ( message )
    {
    case WM_DESTROY:
    {
        PostQuitMessage( 0 );
    }
    break;
    case WM_KEYDOWN:
    {
        USHORT n16KeyCode = ( wParam & 0xFF );

        if ( VK_TAB == n16KeyCode )
        {// 按Tab键
            g_nCurAlbedo += 1;
            if ( g_nCurAlbedo >= g_nAlbedoCount )
            {
                g_nCurAlbedo = 0;
            }
        }

        float fDelta = 0.2f;
        XMVECTOR vDelta = { 0.0f,0.0f,fDelta,0.0f };
        // Z-轴
        if ( VK_UP == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorAdd( g_v4LookAt, vDelta );
        }

        if ( VK_DOWN == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorSubtract( g_v4LookAt, vDelta );
        }
        // X-轴
        vDelta = { fDelta,0.0f,0.0f,0.0f };
        if ( VK_LEFT == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorAdd( g_v4LookAt, vDelta );
        }

        if ( VK_RIGHT == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorSubtract( g_v4LookAt, vDelta );
        }

        // Y-轴
        vDelta = { 0.0f,fDelta,0.0f,0.0f };
        if ( VK_PRIOR == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorAdd( g_v4LookAt, vDelta );
        }

        if ( VK_NEXT == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract( g_v4EyePos, vDelta );
            g_v4LookAt = XMVectorSubtract( g_v4LookAt, vDelta );
        }

        // 缩放
        fDelta = 0.2f;
        if ( VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode )
        {
            g_fScaling += fDelta;
        }

        if ( VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode )
        {
            g_fScaling -= fDelta;
            if ( g_fScaling <= fDelta )
            {
                g_fScaling = fDelta;
            }
        }

        // 金属度
        fDelta = 0.04f;
        if ( 'q' == n16KeyCode || 'Q' == n16KeyCode )
        {
            g_fMetallic += fDelta;
            if ( g_fMetallic > 1.0f )
            {
                g_fMetallic = 1.0f;
            }
            ATLTRACE( L"Metallic = %11.6f\n", g_fMetallic );
        }

        if ( 'a' == n16KeyCode || 'A' == n16KeyCode )
        {
            g_fMetallic -= fDelta;
            if ( g_fMetallic < fDelta )
            {
                g_fMetallic = fDelta;
            }
            ATLTRACE( L"Metallic = %11.6f\n", g_fMetallic );
        }

        // 粗糙度
        fDelta = 0.05f;
        if ( 'w' == n16KeyCode || 'W' == n16KeyCode )
        {
            g_fRoughness += fDelta;
            if ( g_fRoughness > 1.0f )
            {
                g_fRoughness = 1.0f;
            }
            ATLTRACE( L"Roughness = %11.6f\n", g_fRoughness );
        }

        if ( 's' == n16KeyCode || 'S' == n16KeyCode )
        {
            g_fRoughness -= fDelta;
            if ( g_fRoughness < 0.0f )
            {
                g_fRoughness = 0.0f;
            }
            ATLTRACE( L"Roughness = %11.6f\n", g_fRoughness );
        }

        // 环境遮挡
        fDelta = 0.1f;
        if ( 'e' == n16KeyCode || 'E' == n16KeyCode )
        {
            g_fAO += fDelta;
            if ( g_fAO > 1.0f )
            {
                g_fAO = 1.0f;
            }
            ATLTRACE( L"Ambient Occlusion = %11.6f\n", g_fAO );
        }

        if ( 'd' == n16KeyCode || 'D' == n16KeyCode )
        {
            g_fAO -= fDelta;
            if ( g_fAO < 0.0f )
            {
                g_fAO = 0.0f;
            }
            ATLTRACE( L"Ambient Occlusion = %11.6f\n", g_fAO );
        }

    }
    break;

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return DefWindowProc( hWnd, message, wParam, lParam );
}
