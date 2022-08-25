#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <tchar.h>
#include <commdlg.h>
#include <wrl.h> //添加WTL支持 方便使用COM
#include <strsafe.h>

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

using namespace DirectX;
using namespace Microsoft;
using namespace Microsoft::WRL;

#include "../Commons/GRS_Def.h"
#include "../Commons/GRS_Mem.h"
#include "../Commons/GRS_D3D12_Utility.h"
#include "../Commons/GRS_Texture_Loader.h"

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GRS_WND_CLASS _T("GRS Game Window Class")
#define GRS_WND_TITLE _T("GRS DirectX12 Sample: HDR Texture & GS Cube Map")

const UINT		g_nFrameBackBufCount = 3u;

struct ST_GRS_D3D12_DEVICE
{
    DWORD                               m_dwWndStyle;
    INT                                 m_iWndWidth;
    INT							        m_iWndHeight;
    HWND						        m_hWnd;
    RECT                                m_rtWnd;
    RECT                                m_rtWndClient;

    const float						    m_faClearColor[4] = { 0.17647f, 0.549f, 0.941176f, 1.0f };
    D3D_FEATURE_LEVEL					m_emFeatureLevel = D3D_FEATURE_LEVEL_12_1;
    ComPtr<IDXGIFactory6>				m_pIDXGIFactory6;
    ComPtr<ID3D12Device4>				m_pID3D12Device4;

    ComPtr<ID3D12CommandQueue>			m_pIMainCMDQueue;
    ComPtr<ID3D12CommandAllocator>		m_pIMainCMDAlloc;
    ComPtr<ID3D12GraphicsCommandList>	m_pIMainCMDList;

    BOOL                                m_bFullScreen;
    BOOL                                m_bSupportTearing;

    UINT			                    m_nFrameIndex;
    DXGI_FORMAT		                    m_emRenderTargetFormat;
    DXGI_FORMAT                         m_emDepthStencilFormat;

    UINT                                m_nRTVDescriptorSize;
    UINT                                m_nDSVDescriptorSize;
    UINT                                m_nSamplerDescriptorSize;
    UINT                                m_nCBVSRVDescriptorSize;

    UINT64						        m_n64FenceValue;
    HANDLE						        m_hEventFence;

    D3D12_VIEWPORT                      m_stViewPort;
    D3D12_RECT                          m_stScissorRect;

    ComPtr<ID3D12Fence>			        m_pIFence;
    ComPtr<IDXGISwapChain3>		        m_pISwapChain3;
    ComPtr<ID3D12DescriptorHeap>        m_pIRTVHeap;
    ComPtr<ID3D12Resource>		        m_pIARenderTargets[g_nFrameBackBufCount];
    ComPtr<ID3D12DescriptorHeap>        m_pIDSVHeap;
    ComPtr<ID3D12Resource>		        m_pIDepthStencilBuffer;
};

struct ST_GRS_RENDER_TARGET_ARRAY_DATA
{
    UINT                                m_nRTWidth;
    UINT                                m_nRTHeight;
    D3D12_VIEWPORT                      m_stViewPort;
    D3D12_RECT                          m_stScissorRect;

    DXGI_FORMAT                         m_emRTAFormat;

    D3D12_RESOURCE_BARRIER              m_stRTABarriers;

    ComPtr<ID3D12DescriptorHeap>        m_pIRTAHeap;
    ComPtr<ID3D12Resource>              m_pITextureRTA;
    ComPtr<ID3D12Resource>              m_pITextureRTADownload;
};

struct ST_GRS_PSO
{
    ComPtr<ID3D12RootSignature>         m_pIRS;
    ComPtr<ID3D12PipelineState>         m_pIPSO;

    ComPtr<ID3D12CommandAllocator>		m_pICmdAlloc;
    ComPtr<ID3D12GraphicsCommandList>   m_pICmdBundles;

    ComPtr<ID3D12CommandSignature>      m_pICmdSignature;
};

TCHAR    g_pszAppPath[MAX_PATH] = {};
TCHAR    g_pszShaderPath[MAX_PATH] = {};

XMVECTOR g_v4EyePos = XMVectorSet(0.0f, 1.0f, -10.0f, 0.0f);  //眼睛位置
XMVECTOR g_v4LookAt = XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f);	//眼睛所盯的位置
XMVECTOR g_v4UpDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);   //头部正上方位置

float    g_fNear = 0.1f;
float    g_fFar = 1000.0f;

BOOL     g_bWorldRotate = TRUE;

// 场景的旋转角速度（弧度/s）
float    g_fPalstance = 15.0f;
// 场景旋转的角度：旋转角
float    g_fWorldRotateAngle = 0;


D3D12_HEAP_PROPERTIES                       g_stDefaultHeapProps = {};
D3D12_HEAP_PROPERTIES                       g_stUploadHeapProps = {};
D3D12_RESOURCE_DESC                         g_stBufferResSesc = {};

ST_GRS_D3D12_DEVICE                         g_stD3DDevices = {};

void OnSize(UINT width, UINT height, bool minimized);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
    USES_CONVERSION;
    try
    {
        ::CoInitialize(nullptr);  //for WIC & COM
        // 0-1、初始化全局参数
        {
            g_stD3DDevices.m_iWndWidth = 1280;
            g_stD3DDevices.m_iWndHeight = 800;
            g_stD3DDevices.m_hWnd = nullptr;
            g_stD3DDevices.m_nFrameIndex = 0;
            g_stD3DDevices.m_emRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            g_stD3DDevices.m_emDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
            g_stD3DDevices.m_nRTVDescriptorSize = 0U;
            g_stD3DDevices.m_nDSVDescriptorSize = 0U;
            g_stD3DDevices.m_nSamplerDescriptorSize = 0u;
            g_stD3DDevices.m_nCBVSRVDescriptorSize = 0u;
            g_stD3DDevices.m_stViewPort = { 0.0f, 0.0f, static_cast<float>(g_stD3DDevices.m_iWndWidth), static_cast<float>(g_stD3DDevices.m_iWndHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
            g_stD3DDevices.m_stScissorRect = { 0, 0, static_cast<LONG>(g_stD3DDevices.m_iWndWidth), static_cast<LONG>(g_stD3DDevices.m_iWndHeight) };
            g_stD3DDevices.m_n64FenceValue = 0ui64;
            g_stD3DDevices.m_hEventFence = nullptr;

            g_stDefaultHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
            g_stDefaultHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            g_stDefaultHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            g_stDefaultHeapProps.CreationNodeMask = 0;
            g_stDefaultHeapProps.VisibleNodeMask = 0;

            g_stUploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
            g_stUploadHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            g_stUploadHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            g_stUploadHeapProps.CreationNodeMask = 0;
            g_stUploadHeapProps.VisibleNodeMask = 0;

            g_stBufferResSesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            g_stBufferResSesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            g_stBufferResSesc.Flags = D3D12_RESOURCE_FLAG_NONE;
            g_stBufferResSesc.Format = DXGI_FORMAT_UNKNOWN;
            g_stBufferResSesc.Width = 0;
            g_stBufferResSesc.Height = 1;
            g_stBufferResSesc.DepthOrArraySize = 1;
            g_stBufferResSesc.MipLevels = 1;
            g_stBufferResSesc.SampleDesc.Count = 1;
            g_stBufferResSesc.SampleDesc.Quality = 0;
        }

        // 0-2、得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
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

            ::StringCchPrintf(g_pszShaderPath
                , MAX_PATH
                , _T("%s16-HDRTexture2CubeMapWithGS\\Shader")
                , g_pszAppPath);
        }

        // 1-1、创建窗口
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
            wcex.lpszClassName = GRS_WND_CLASS;

            ::RegisterClassEx(&wcex);

            g_stD3DDevices.m_dwWndStyle = WS_OVERLAPPEDWINDOW;

            RECT rtWnd = { 0, 0, g_stD3DDevices.m_iWndWidth, g_stD3DDevices.m_iWndHeight };
            ::AdjustWindowRect(&rtWnd, g_stD3DDevices.m_dwWndStyle, FALSE);

            // 计算窗口居中的屏幕坐标
            INT posX = (::GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
            INT posY = (::GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

            g_stD3DDevices.m_hWnd = ::CreateWindowW(GRS_WND_CLASS
                , GRS_WND_TITLE
                , g_stD3DDevices.m_dwWndStyle
                , posX
                , posY
                , rtWnd.right - rtWnd.left
                , rtWnd.bottom - rtWnd.top
                , nullptr
                , nullptr
                , hInstance
                , nullptr);

            if (!g_stD3DDevices.m_hWnd)
            {
                GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        // 2-1、创建DXGI Factory对象
        {
            UINT nDXGIFactoryFlags = 0U;
#if defined(_DEBUG)
            // 打开显示子系统的调试支持
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();
                // 打开附加的调试支持
                nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
#endif
            ComPtr<IDXGIFactory5> pIDXGIFactory5;
            GRS_THROW_IF_FAILED(::CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);

            //获取IDXGIFactory6接口
            GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&g_stD3DDevices.m_pIDXGIFactory6));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(g_stD3DDevices.m_pIDXGIFactory6);

            // 检测Tearing支持
            HRESULT hr = g_stD3DDevices.m_pIDXGIFactory6->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING
                , &g_stD3DDevices.m_bSupportTearing
                , sizeof(g_stD3DDevices.m_bSupportTearing));
            g_stD3DDevices.m_bSupportTearing = SUCCEEDED(hr) && g_stD3DDevices.m_bSupportTearing;
        }

        // 2-2、枚举适配器创建设备
        { //选择高性能的显卡来创建3D设备对象
            ComPtr<IDXGIAdapter1> pIAdapter1;
            GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIDXGIFactory6->EnumAdapterByGpuPreference(0
                , DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                , IID_PPV_ARGS(&pIAdapter1)));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(pIAdapter1);

            // 创建D3D12.1的设备
            GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter1.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&g_stD3DDevices.m_pID3D12Device4)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stD3DDevices.m_pID3D12Device4);


            DXGI_ADAPTER_DESC1 stAdapterDesc = {};
            TCHAR pszWndTitle[MAX_PATH] = {};
            GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc));
            ::GetWindowText(g_stD3DDevices.m_hWnd, pszWndTitle, MAX_PATH);
            StringCchPrintf(pszWndTitle
                , MAX_PATH
                , _T("%s(GPU[0]:%s)")
                , pszWndTitle
                , stAdapterDesc.Description);
            ::SetWindowText(g_stD3DDevices.m_hWnd, pszWndTitle);
        }

        // 2-3、得到每个描述符元素的大小
        {
            g_stD3DDevices.m_nRTVDescriptorSize
                = g_stD3DDevices.m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            g_stD3DDevices.m_nDSVDescriptorSize
                = g_stD3DDevices.m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            g_stD3DDevices.m_nSamplerDescriptorSize
                = g_stD3DDevices.m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            g_stD3DDevices.m_nCBVSRVDescriptorSize
                = g_stD3DDevices.m_pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // 2-4、创建命令队列&命令列表
        {
            D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
            stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            GRS_THROW_IF_FAILED(g_stD3DDevices.m_pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&g_stD3DDevices.m_pIMainCMDQueue)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stD3DDevices.m_pIMainCMDQueue);

            GRS_THROW_IF_FAILED(g_stD3DDevices.m_pID3D12Device4->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT
                , IID_PPV_ARGS(&g_stD3DDevices.m_pIMainCMDAlloc)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stD3DDevices.m_pIMainCMDAlloc);

            // 创建图形命令列表
            GRS_THROW_IF_FAILED(g_stD3DDevices.m_pID3D12Device4->CreateCommandList(
                0
                , D3D12_COMMAND_LIST_TYPE_DIRECT
                , g_stD3DDevices.m_pIMainCMDAlloc.Get()
                , nullptr
                , IID_PPV_ARGS(&g_stD3DDevices.m_pIMainCMDList)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stD3DDevices.m_pIMainCMDList);
        }

        // 2-5、创建交换链
        {
            DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
            stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
            stSwapChainDesc.Width = g_stD3DDevices.m_iWndWidth;
            stSwapChainDesc.Height = g_stD3DDevices.m_iWndHeight;
            stSwapChainDesc.Format = g_stD3DDevices.m_emRenderTargetFormat;
            stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            stSwapChainDesc.SampleDesc.Count = 1;
            stSwapChainDesc.Scaling = DXGI_SCALING_NONE;

            // 检测并启用Tearing支持
            stSwapChainDesc.Flags = g_stD3DDevices.m_bSupportTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

            ComPtr<IDXGISwapChain1> pISwapChain1;

            GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIDXGIFactory6->CreateSwapChainForHwnd(
                g_stD3DDevices.m_pIMainCMDQueue.Get(),		// 交换链需要命令队列，Present命令要执行
                g_stD3DDevices.m_hWnd,
                &stSwapChainDesc,
                nullptr,
                nullptr,
                &pISwapChain1
            ));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

            GRS_THROW_IF_FAILED(pISwapChain1.As(&g_stD3DDevices.m_pISwapChain3));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(g_stD3DDevices.m_pISwapChain3);

            //得到当前后缓冲区的序号，也就是下一个将要呈送显示的缓冲区的序号
            //注意此处使用了高版本的SwapChain接口的函数
            g_stD3DDevices.m_nFrameIndex = g_stD3DDevices.m_pISwapChain3->GetCurrentBackBufferIndex();

            //创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
            {
                D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
                stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;
                stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

                GRS_THROW_IF_FAILED(g_stD3DDevices.m_pID3D12Device4->CreateDescriptorHeap(
                    &stRTVHeapDesc
                    , IID_PPV_ARGS(&g_stD3DDevices.m_pIRTVHeap)));
                GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stD3DDevices.m_pIRTVHeap);

            }

            //创建RTV的描述符
            {
                D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
                    = g_stD3DDevices.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                for (UINT i = 0; i < g_nFrameBackBufCount; i++)
                {
                    GRS_THROW_IF_FAILED(g_stD3DDevices.m_pISwapChain3->GetBuffer(i
                        , IID_PPV_ARGS(&g_stD3DDevices.m_pIARenderTargets[i])));

                    GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(g_stD3DDevices.m_pIARenderTargets, i);

                    g_stD3DDevices.m_pID3D12Device4->CreateRenderTargetView(g_stD3DDevices.m_pIARenderTargets[i].Get()
                        , nullptr
                        , stRTVHandle);

                    stRTVHandle.ptr += g_stD3DDevices.m_nRTVDescriptorSize;
                }
            }

            if (g_stD3DDevices.m_bSupportTearing)
            {// 如果支持Tearing，那么就关闭系统的ALT + Entery全屏键支持，而改用我们自己处理
                GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIDXGIFactory6->MakeWindowAssociation(g_stD3DDevices.m_hWnd, DXGI_MWA_NO_ALT_ENTER));
            }

        }

        // 2-6、创建深度缓冲及深度缓冲描述符堆
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC   stDepthStencilDesc = {};
            D3D12_CLEAR_VALUE               stDepthOptimizedClearValue = {};
            D3D12_RESOURCE_DESC             stDSTex2DDesc = {};
            D3D12_DESCRIPTOR_HEAP_DESC      stDSVHeapDesc = {};

            stDepthStencilDesc.Format = g_stD3DDevices.m_emDepthStencilFormat;
            stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

            stDepthOptimizedClearValue.Format = g_stD3DDevices.m_emDepthStencilFormat;
            stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
            stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

            stDSTex2DDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            stDSTex2DDesc.Alignment = 0;
            stDSTex2DDesc.Width = g_stD3DDevices.m_iWndWidth;
            stDSTex2DDesc.Height = g_stD3DDevices.m_iWndHeight;
            stDSTex2DDesc.DepthOrArraySize = 1;
            stDSTex2DDesc.MipLevels = 0;
            stDSTex2DDesc.Format = g_stD3DDevices.m_emDepthStencilFormat;
            stDSTex2DDesc.SampleDesc.Count = 1;
            stDSTex2DDesc.SampleDesc.Quality = 0;
            stDSTex2DDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            stDSTex2DDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            stDSVHeapDesc.NumDescriptors = 1;
            stDSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            stDSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            //使用隐式默认堆创建一个深度蜡板缓冲区，
            //因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
            GRS_THROW_IF_FAILED(g_stD3DDevices.m_pID3D12Device4->CreateCommittedResource(
                &g_stDefaultHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stDSTex2DDesc
                , D3D12_RESOURCE_STATE_DEPTH_WRITE
                , &stDepthOptimizedClearValue
                , IID_PPV_ARGS(&g_stD3DDevices.m_pIDepthStencilBuffer)
            ));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stD3DDevices.m_pIDepthStencilBuffer);
            //==============================================================================================

            // 创建 DSV Heap
            GRS_THROW_IF_FAILED(g_stD3DDevices.m_pID3D12Device4->CreateDescriptorHeap(&stDSVHeapDesc, IID_PPV_ARGS(&g_stD3DDevices.m_pIDSVHeap)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stD3DDevices.m_pIDSVHeap);
            // 创建 DSV
            g_stD3DDevices.m_pID3D12Device4->CreateDepthStencilView(g_stD3DDevices.m_pIDepthStencilBuffer.Get()
                , &stDepthStencilDesc
                , g_stD3DDevices.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
        }

        // 2-7、创建围栏
        {
            GRS_THROW_IF_FAILED(g_stD3DDevices.m_pID3D12Device4->CreateFence(0
                , D3D12_FENCE_FLAG_NONE
                , IID_PPV_ARGS(&g_stD3DDevices.m_pIFence)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stD3DDevices.m_pIFence);

            // 初始化 Fence 计数为 1
            g_stD3DDevices.m_n64FenceValue = 1;

            // 创建一个Event同步对象，用于等待围栏事件通知
            // 本质上是为了 CPU 与 GPU 的同步
            g_stD3DDevices.m_hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (g_stD3DDevices.m_hEventFence == nullptr)
            {
                GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        // 2-8、检测根签名版本，现在只支持1.1以上的，之前的就丢弃退出了，不然兼容的代码太啰嗦了也毫无营养
        {
            D3D12_FEATURE_DATA_ROOT_SIGNATURE stFeatureData = {};
            // 检测是否支持V1.1版本的根签名
            stFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
            if (FAILED(g_stD3DDevices.m_pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE
                , &stFeatureData
                , sizeof(stFeatureData))))
            { // 1.0版 直接丢异常退出了
                ATLTRACE("系统不支持V1.1的跟签名，直接退出了，需要运行自行修改创建跟签名的代码回1.0版。");
                GRS_THROW_IF_FAILED(E_NOTIMPL);
            }
        }
        CAtlArray<ID3D12CommandList*> arCmdList;
        arCmdList.Add(g_stD3DDevices.m_pIMainCMDList.Get());

        D3D12_RESOURCE_BARRIER              stBeginResBarrier = {};
        D3D12_RESOURCE_BARRIER              stEneResBarrier = {};

        // 8、填充渲染目标的资源屏障结构
        {
            stBeginResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            stBeginResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            stBeginResBarrier.Transition.pResource = g_stD3DDevices.m_pIARenderTargets[g_stD3DDevices.m_nFrameIndex].Get();
            stBeginResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            stBeginResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            stBeginResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            stEneResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            stEneResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            stEneResBarrier.Transition.pResource = g_stD3DDevices.m_pIARenderTargets[g_stD3DDevices.m_nFrameIndex].Get();
            stEneResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            stEneResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            stEneResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }

        DWORD dwRet = 0;
        BOOL bExit = FALSE;
        D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = g_stD3DDevices.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = g_stD3DDevices.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart();

        FLOAT fStartTime = (FLOAT)::GetTickCount64();
        FLOAT fCurrentTime = fStartTime;
        FLOAT fTimeInSeconds = (fCurrentTime - fStartTime) / 1000.0f;
        FLOAT fDeltaTime = fCurrentTime - fStartTime;

        // 先关闭和重置一下，使消息循环可以正常运转
        GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIMainCMDList->Close());
        SetEvent(g_stD3DDevices.m_hEventFence);

        // 开始正式消息循环前才显示主窗口
        ShowWindow(g_stD3DDevices.m_hWnd, nCmdShow);
        UpdateWindow(g_stD3DDevices.m_hWnd);

        MSG msg = {};
        // 9、消息循环
        while (!bExit)
        {
            dwRet = ::MsgWaitForMultipleObjects(1, &g_stD3DDevices.m_hEventFence, FALSE, INFINITE, QS_ALLINPUT);
            switch (dwRet - WAIT_OBJECT_0)
            {
            case 0:
            {
                //-----------------------------------------------------------------------------------------------------
                // OnUpdate()
                // Animation Calculate etc.
                //-----------------------------------------------------------------------------------------------------
                fCurrentTime = (FLOAT)::GetTickCount64();
                fDeltaTime = fCurrentTime - fStartTime;

                // Update! with fDeltaTime or fCurrentTime & fStartTime in MS

                //计算旋转的角度：旋转角度(弧度) = 时间(秒) * 角速度(弧度/秒)        
                if (g_bWorldRotate)
                {// 旋转时 角度才累加 否则就固定不动
                    g_fWorldRotateAngle += ((fDeltaTime) / 1000.0f) * (g_fPalstance * XM_PI / 180.0f);
                }

                //旋转角度是2PI周期的倍数，去掉周期数，只留下相对0弧度开始的小于2PI的弧度即可
                if (g_fWorldRotateAngle > XM_2PI)
                {
                    g_fWorldRotateAngle = (float)::fmod(g_fWorldRotateAngle, XM_2PI);
                }
                fStartTime = fCurrentTime;
                //-----------------------------------------------------------------------------------------------------
                // OnRender()
                //----------------------------------------------------------------------------------------------------- 
                //命令分配器先Reset一下
                GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIMainCMDAlloc->Reset());
                GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIMainCMDList->Reset(g_stD3DDevices.m_pIMainCMDAlloc.Get(), nullptr));

                g_stD3DDevices.m_nFrameIndex = g_stD3DDevices.m_pISwapChain3->GetCurrentBackBufferIndex();

                g_stD3DDevices.m_pIMainCMDList->RSSetViewports(1, &g_stD3DDevices.m_stViewPort);
                g_stD3DDevices.m_pIMainCMDList->RSSetScissorRects(1, &g_stD3DDevices.m_stScissorRect);

                // 通过资源屏障判定后缓冲已经切换完毕可以开始渲染了
                stBeginResBarrier.Transition.pResource = g_stD3DDevices.m_pIARenderTargets[g_stD3DDevices.m_nFrameIndex].Get();
                g_stD3DDevices.m_pIMainCMDList->ResourceBarrier(1, &stBeginResBarrier);

                // 设置渲染目标
                stRTVHandle = g_stD3DDevices.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                stRTVHandle.ptr += (size_t)g_stD3DDevices.m_nFrameIndex * g_stD3DDevices.m_nRTVDescriptorSize;
                g_stD3DDevices.m_pIMainCMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);

                // 继续记录命令，并真正开始新一帧的渲染
                g_stD3DDevices.m_pIMainCMDList->ClearRenderTargetView(stRTVHandle, g_stD3DDevices.m_faClearColor, 0, nullptr);
                g_stD3DDevices.m_pIMainCMDList->ClearDepthStencilView(g_stD3DDevices.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart()
                    , D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

                //-----------------------------------------------------------------------------------------------------
                // Draw !
                //-----------------------------------------------------------------------------------------------------

                //-----------------------------------------------------------------------------------------------------

                //又一个资源屏障，用于确定渲染已经结束可以提交画面去显示了
                stEneResBarrier.Transition.pResource = g_stD3DDevices.m_pIARenderTargets[g_stD3DDevices.m_nFrameIndex].Get();
                g_stD3DDevices.m_pIMainCMDList->ResourceBarrier(1, &stEneResBarrier);

                //关闭命令列表，可以去执行了
                GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIMainCMDList->Close());

                //执行命令列表
                arCmdList.RemoveAll();
                arCmdList.Add(g_stD3DDevices.m_pIMainCMDList.Get());
                g_stD3DDevices.m_pIMainCMDQueue->ExecuteCommandLists(static_cast<UINT>(arCmdList.GetCount()), arCmdList.GetData());

                //提交画面
                // 如果支持Tearing 那么就用Tearing方式呈现画面
                UINT nPresentFlags = (g_stD3DDevices.m_bSupportTearing && !g_stD3DDevices.m_bFullScreen) ? DXGI_PRESENT_ALLOW_TEARING : 0;
                GRS_THROW_IF_FAILED(g_stD3DDevices.m_pISwapChain3->Present(0, nPresentFlags));

                //开始同步GPU与CPU的执行，先记录围栏标记值
                const UINT64 n64CurrentFenceValue = g_stD3DDevices.m_n64FenceValue;
                GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIMainCMDQueue->Signal(g_stD3DDevices.m_pIFence.Get(), n64CurrentFenceValue));
                ++g_stD3DDevices.m_n64FenceValue;
                GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, g_stD3DDevices.m_hEventFence));
                //-----------------------------------------------------------------------------------------------------
            }
            break;
            case 1:
            {// 处理消息
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
            default:
                break;
            }
        }
    }
    catch (CAtlException& e)
    {//发生了COM异常
        e;
    }
    catch (...)
    {

    }
    return 0;
}

void OnSize(UINT width, UINT height, bool minimized)
{
    if ((width == g_stD3DDevices.m_iWndWidth && height == g_stD3DDevices.m_iWndHeight)
        || minimized
        || (nullptr == g_stD3DDevices.m_pISwapChain3)
        || (0 == width)
        || (0 == height))
    {
        return;
    }

    try
    {
        // 1、消息循环旁路
        if (WAIT_OBJECT_0 != ::WaitForSingleObject(g_stD3DDevices.m_hEventFence, INFINITE))
        {
            GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
        }

        // 2、释放获取的后缓冲对象接口
        for (UINT i = 0; i < g_nFrameBackBufCount; i++)
        {
            g_stD3DDevices.m_pIARenderTargets[i].Reset();

        }

        // 3、交换链重置大小
        DXGI_SWAP_CHAIN_DESC stSWDesc = {};
        g_stD3DDevices.m_pISwapChain3->GetDesc(&stSWDesc);

        HRESULT hr = g_stD3DDevices.m_pISwapChain3->ResizeBuffers(g_nFrameBackBufCount
            , width
            , height
            , stSWDesc.BufferDesc.Format
            , stSWDesc.Flags);

        GRS_THROW_IF_FAILED(hr);

        // 4、得到当前后缓冲索引
        g_stD3DDevices.m_nFrameIndex = g_stD3DDevices.m_pISwapChain3->GetCurrentBackBufferIndex();

        // 5、保存变换后的尺寸
        g_stD3DDevices.m_iWndWidth = width;
        g_stD3DDevices.m_iWndHeight = height;

        // 6、创建RTV的描述符
        ComPtr<ID3D12Device> pID3D12Device;
        ComPtr<ID3D12Device4> pID3D12Device4;
        GRS_THROW_IF_FAILED(g_stD3DDevices.m_pIRTVHeap->GetDevice(IID_PPV_ARGS(&pID3D12Device)));
        GRS_THROW_IF_FAILED(pID3D12Device.As(&pID3D12Device4));
        {
            D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
                = g_stD3DDevices.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
            for (UINT i = 0; i < g_nFrameBackBufCount; i++)
            {
                GRS_THROW_IF_FAILED(g_stD3DDevices.m_pISwapChain3->GetBuffer(i
                    , IID_PPV_ARGS(&g_stD3DDevices.m_pIARenderTargets[i])));

                GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(g_stD3DDevices.m_pIARenderTargets, i);

                pID3D12Device4->CreateRenderTargetView(g_stD3DDevices.m_pIARenderTargets[i].Get()
                    , nullptr
                    , stRTVHandle);

                stRTVHandle.ptr += g_stD3DDevices.m_nRTVDescriptorSize;
            }
        }

        // 7、创建深度缓冲及深度缓冲描述符堆
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC   stDepthStencilDesc = {};
            D3D12_CLEAR_VALUE               stDepthOptimizedClearValue = {};
            D3D12_RESOURCE_DESC             stDSTex2DDesc = {};

            stDepthOptimizedClearValue.Format = g_stD3DDevices.m_emDepthStencilFormat;
            stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
            stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

            stDSTex2DDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            stDSTex2DDesc.Alignment = 0;
            stDSTex2DDesc.Width = g_stD3DDevices.m_iWndWidth;
            stDSTex2DDesc.Height = g_stD3DDevices.m_iWndHeight;
            stDSTex2DDesc.DepthOrArraySize = 1;
            stDSTex2DDesc.MipLevels = 0;
            stDSTex2DDesc.Format = g_stD3DDevices.m_emDepthStencilFormat;
            stDSTex2DDesc.SampleDesc.Count = 1;
            stDSTex2DDesc.SampleDesc.Quality = 0;
            stDSTex2DDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            stDSTex2DDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            // 释放深度缓冲先
            g_stD3DDevices.m_pIDepthStencilBuffer.Reset();

            //使用隐式默认堆创建一个深度蜡板缓冲区，
            //因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
            GRS_THROW_IF_FAILED(g_stD3DDevices.m_pID3D12Device4->CreateCommittedResource(
                &g_stDefaultHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &stDSTex2DDesc
                , D3D12_RESOURCE_STATE_DEPTH_WRITE
                , &stDepthOptimizedClearValue
                , IID_PPV_ARGS(&g_stD3DDevices.m_pIDepthStencilBuffer)
            ));

            stDepthStencilDesc.Format = g_stD3DDevices.m_emDepthStencilFormat;
            stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

            // 然后直接重建描述符
            g_stD3DDevices.m_pID3D12Device4->CreateDepthStencilView(g_stD3DDevices.m_pIDepthStencilBuffer.Get()
                , &stDepthStencilDesc
                , g_stD3DDevices.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
        }

        // 8、更新视口大小和齐次包围盒大小
        g_stD3DDevices.m_stViewPort.TopLeftX = 0.0f;
        g_stD3DDevices.m_stViewPort.TopLeftY = 0.0f;
        g_stD3DDevices.m_stViewPort.Width = static_cast<float>(g_stD3DDevices.m_iWndWidth);
        g_stD3DDevices.m_stViewPort.Height = static_cast<float>(g_stD3DDevices.m_iWndHeight);

        g_stD3DDevices.m_stScissorRect.left = static_cast<LONG>(g_stD3DDevices.m_stViewPort.TopLeftX);
        g_stD3DDevices.m_stScissorRect.right = static_cast<LONG>(g_stD3DDevices.m_stViewPort.TopLeftX + g_stD3DDevices.m_stViewPort.Width);
        g_stD3DDevices.m_stScissorRect.top = static_cast<LONG>(g_stD3DDevices.m_stViewPort.TopLeftY);
        g_stD3DDevices.m_stScissorRect.bottom = static_cast<LONG>(g_stD3DDevices.m_stViewPort.TopLeftY + g_stD3DDevices.m_stViewPort.Height);


        // 9、设置下事件状态，从消息旁路返回主消息循环        
        SetEvent(g_stD3DDevices.m_hEventFence);
    }
    catch (CAtlException& e)
    {//发生了COM异常
        e;
    }
    catch (...)
    {

    }

}

void SwitchFullScreenWindow()
{
    if (g_stD3DDevices.m_bFullScreen)
    {// 还原为窗口化
        SetWindowLong(g_stD3DDevices.m_hWnd, GWL_STYLE, g_stD3DDevices.m_dwWndStyle);

        SetWindowPos(
            g_stD3DDevices.m_hWnd,
            HWND_NOTOPMOST,
            g_stD3DDevices.m_rtWnd.left,
            g_stD3DDevices.m_rtWnd.top,
            g_stD3DDevices.m_rtWnd.right - g_stD3DDevices.m_rtWnd.left,
            g_stD3DDevices.m_rtWnd.bottom - g_stD3DDevices.m_rtWnd.top,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(g_stD3DDevices.m_hWnd, SW_NORMAL);
    }
    else
    {// 全屏显示
        // 保存窗口的原始尺寸
        GetWindowRect(g_stD3DDevices.m_hWnd, &g_stD3DDevices.m_rtWnd);

        // 使窗口无边框，以便客户区可以填满屏幕。  
        SetWindowLong(g_stD3DDevices.m_hWnd
            , GWL_STYLE
            , g_stD3DDevices.m_dwWndStyle
            & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

        RECT stFullscreenWindowRect = {};
        ComPtr<IDXGIOutput> pIOutput;
        DXGI_OUTPUT_DESC stOutputDesc = {};
        HRESULT _hr = g_stD3DDevices.m_pISwapChain3->GetContainingOutput(&pIOutput);
        if (SUCCEEDED(_hr) &&
            SUCCEEDED(pIOutput->GetDesc(&stOutputDesc)))
        {// 使用显示器最大显示尺寸
            stFullscreenWindowRect = stOutputDesc.DesktopCoordinates;
        }
        else
        {// 获取系统设置的分辨率
            DEVMODE stDevMode = {};
            stDevMode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &stDevMode);

            stFullscreenWindowRect = {
                stDevMode.dmPosition.x,
                stDevMode.dmPosition.y,
                stDevMode.dmPosition.x + static_cast<LONG>(stDevMode.dmPelsWidth),
                stDevMode.dmPosition.y + static_cast<LONG>(stDevMode.dmPelsHeight)
            };
        }

        // 设置窗口最大化
        SetWindowPos(
            g_stD3DDevices.m_hWnd,
            HWND_TOPMOST,
            stFullscreenWindowRect.left,
            stFullscreenWindowRect.top,
            stFullscreenWindowRect.right,
            stFullscreenWindowRect.bottom,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(g_stD3DDevices.m_hWnd, SW_MAXIMIZE);
    }

    g_stD3DDevices.m_bFullScreen = !!!g_stD3DDevices.m_bFullScreen;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_DESTROY:
    {
        // 旁路一下消息循环
        if (WAIT_OBJECT_0 == ::WaitForSingleObject(g_stD3DDevices.m_hEventFence, INFINITE)
            && g_stD3DDevices.m_bSupportTearing
            && g_stD3DDevices.m_pISwapChain3)
        {// 支持Tearing时，自动退出下全屏状态
            g_stD3DDevices.m_pISwapChain3->SetFullscreenState(FALSE, nullptr);
        }
        PostQuitMessage(0);
    }
    break;
    case WM_KEYDOWN:
    {
        USHORT n16KeyCode = (wParam & 0xFF);
        if (VK_ESCAPE == n16KeyCode)
        {// Esc键退出
            ::DestroyWindow(hWnd);
        }

        if (VK_TAB == n16KeyCode)
        {// 按Tab键 
            
        }

        if (VK_SPACE == n16KeyCode)
        {// 按Space键
            
        }

        if (VK_F3 == n16KeyCode)
        {// F3键

        }

        //-------------------------------------------------------------------------------
        // 基本摄像机位置移动操作
        float fDelta = 1.0f;
        XMVECTOR vDelta = { 0.0f,0.0f,fDelta,0.0f };
        // Z-轴
        if (VK_UP == n16KeyCode || 'w' == n16KeyCode || 'W' == n16KeyCode)
        {
            g_v4EyePos = XMVectorAdd(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorAdd(g_v4LookAt, vDelta);
        }

        if (VK_DOWN == n16KeyCode || 's' == n16KeyCode || 'S' == n16KeyCode)
        {
            g_v4EyePos = XMVectorSubtract(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorSubtract(g_v4LookAt, vDelta);
        }

        // X-轴
        vDelta = { fDelta,0.0f,0.0f,0.0f };
        if (VK_LEFT == n16KeyCode || 'a' == n16KeyCode || 'A' == n16KeyCode)
        {
            g_v4EyePos = XMVectorAdd(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorAdd(g_v4LookAt, vDelta);
        }

        if (VK_RIGHT == n16KeyCode || 'd' == n16KeyCode || 'D' == n16KeyCode)
        {
            g_v4EyePos = XMVectorSubtract(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorSubtract(g_v4LookAt, vDelta);
        }

        // Y-轴
        vDelta = { 0.0f,fDelta,0.0f,0.0f };
        if (VK_PRIOR == n16KeyCode || 'r' == n16KeyCode || 'R' == n16KeyCode)
        {
            g_v4EyePos = XMVectorAdd(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorAdd(g_v4LookAt, vDelta);
        }

        if (VK_NEXT == n16KeyCode || 'f' == n16KeyCode || 'F' == n16KeyCode)
        {
            g_v4EyePos = XMVectorSubtract(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorSubtract(g_v4LookAt, vDelta);
        }
        //-------------------------------------------------------------------------------

        if (VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode)
        {// + 键  
        }

        if (VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode)
        {// - 键 
        }

        if (VK_MULTIPLY == n16KeyCode)
        {// * 键  控制旋转速度 增加             
            g_fPalstance = ((g_fPalstance >= 180.0f) ? 180.0f : (g_fPalstance + 1.0f));
        }

        if (VK_DIVIDE == n16KeyCode)
        {// / 键 控制旋转速度 减少      
            g_fPalstance = ((g_fPalstance > 1.0f) ? (g_fPalstance - 1.0f) : 1.0f);
        }

        if (VK_SHIFT == n16KeyCode)
        {// 控制是否旋转
            g_bWorldRotate = !!!g_bWorldRotate;
        }
    }
    break;
    case WM_SIZE:
    {
        ::GetClientRect(hWnd, &g_stD3DDevices.m_rtWndClient);
        OnSize(g_stD3DDevices.m_rtWndClient.right - g_stD3DDevices.m_rtWndClient.left
            , g_stD3DDevices.m_rtWndClient.bottom - g_stD3DDevices.m_rtWndClient.top
            , wParam == SIZE_MINIMIZED);
        return 0; // 不再让系统继续处理
    }
    break;
    case WM_GETMINMAXINFO:
    {// 限制窗口最小大小为1024*768
        MINMAXINFO* pMMInfo = (MINMAXINFO*)lParam;
        pMMInfo->ptMinTrackSize.x = 1024;
        pMMInfo->ptMinTrackSize.y = 768;
        return 0;
    }
    break;
    case WM_SYSKEYDOWN:
    {
        if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
        {// 处理Alt + Entry
            if (g_stD3DDevices.m_bSupportTearing)
            {
                SwitchFullScreenWindow();
                return 0;
            }
        }
    }
    break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

