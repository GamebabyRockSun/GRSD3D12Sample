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

#include "../Commons/GRS_D3D12_Utility.h"

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define GRS_SLEEP(dwMilliseconds)  WaitForSingleObject(GetCurrentThread(),dwMilliseconds)
#define GRS_THROW_IF_FAILED(hr) {HRESULT _hr = (hr);if (FAILED(_hr)){ ATLTRACE("Error: 0x%08x\n",_hr); AtlThrow(_hr); }}
//更简洁的向上边界对齐算法 内存管理中常用 请记住
#define GRS_UPPER(A,B) ((size_t)(((A)+((B)-1))&~((B) - 1)))
//上取整除法
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))

#define GRS_WND_CLASS _T("GRS Game Window Class")
#define GRS_WND_TITLE _T("GRS DirectX12 Sample: Resize Window")

const UINT		g_nFrameBackBufCount = 3u;

struct ST_GRS_GPU_TARGET_STATUS
{
    DWORD                           m_dwWndStyle;
    INT                             m_iWndWidth;
    INT							    m_iWndHeight;
    HWND						    m_hWnd;
    RECT                            m_rtWnd;
    RECT                            m_rtWndClient;

    BOOL                            m_bFullScreen;
    BOOL                            m_bSupportTearing;

    UINT			                m_nFrameIndex;
    DXGI_FORMAT		                m_emRenderTargetFormat;
    DXGI_FORMAT                     m_emDepthStencilFormat;
    UINT                            m_nRTVDescriptorSize;
    UINT                            m_nDSVDescriptorSize;
    UINT                            m_nSamplerDescriptorSize;
    UINT                            m_nCBVSRVDescriptorSize;

    UINT64						    m_n64FenceValue;
    HANDLE						    m_hEventFence;

    D3D12_VIEWPORT                  m_stViewPort;
    D3D12_RECT                      m_stScissorRect;

    ComPtr<ID3D12Fence>			    m_pIFence;
    ComPtr<IDXGISwapChain3>		    m_pISwapChain3;
    ComPtr<ID3D12DescriptorHeap>    m_pIRTVHeap;
    ComPtr<ID3D12Resource>		    m_pIARenderTargets[g_nFrameBackBufCount];
    ComPtr<ID3D12DescriptorHeap>    m_pIDSVHeap;
    ComPtr<ID3D12Resource>		    m_pIDepthStencilBuffer;
};

TCHAR g_pszAppPath[MAX_PATH] = {};

XMVECTOR g_v4EyePos = XMVectorSet(0.0f, 2.0f, -100.0f, 0.0f);	//眼睛位置
XMVECTOR g_v4LookAt = XMVectorSet(0.0f, 1.0f, 1.0f, 0.0f);		//眼睛所盯的位置
XMVECTOR g_v4UpDir = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);		//头部正上方位置

ST_GRS_GPU_TARGET_STATUS        g_stGPUStatus = {};
D3D12_HEAP_PROPERTIES           g_stDefautHeapProps = {};
D3D12_HEAP_PROPERTIES           g_stUploadHeapProps = {};
D3D12_RESOURCE_DESC             g_stBufferResSesc = {};
D3D12_DEPTH_STENCIL_VIEW_DESC   g_stDepthStencilDesc = {};
D3D12_CLEAR_VALUE               g_stDepthOptimizedClearValue = {};
D3D12_RESOURCE_DESC             g_stDSTex2DDesc = {};
D3D12_DESCRIPTOR_HEAP_DESC      g_stDSVHeapDesc = {};

void OnSize(UINT width, UINT height, bool minimized);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
    const float						    faClearColor[] = { 0.17647f, 0.549f, 0.941176f, 1.0f };
    D3D_FEATURE_LEVEL					emFeatureLevel = D3D_FEATURE_LEVEL_12_1;
    ComPtr<IDXGIFactory6>				pIDXGIFactory6;
    ComPtr<ID3D12Device4>				pID3D12Device4;

    ComPtr<ID3D12CommandQueue>			pIMainCMDQueue;
    ComPtr<ID3D12CommandAllocator>		pIMainCMDAlloc;
    ComPtr<ID3D12GraphicsCommandList>	pIMainCMDList;

    D3D12_RESOURCE_BARRIER              stBeginResBarrier = {};
    D3D12_RESOURCE_BARRIER              stEneResBarrier = {};
    D3D12_RESOURCE_BARRIER              stResStateTransBarrier = {};

    USES_CONVERSION;

    g_stGPUStatus.m_iWndWidth = 1280;
    g_stGPUStatus.m_iWndHeight = 800;
    g_stGPUStatus.m_hWnd = nullptr;
    g_stGPUStatus.m_nFrameIndex = 0;
    g_stGPUStatus.m_emRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    g_stGPUStatus.m_emDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
    g_stGPUStatus.m_nRTVDescriptorSize = 0U;
    g_stGPUStatus.m_nDSVDescriptorSize = 0U;
    g_stGPUStatus.m_nSamplerDescriptorSize = 0u;
    g_stGPUStatus.m_nCBVSRVDescriptorSize = 0u;
    g_stGPUStatus.m_stViewPort = { 0.0f, 0.0f, static_cast<float>(g_stGPUStatus.m_iWndWidth), static_cast<float>(g_stGPUStatus.m_iWndHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
    g_stGPUStatus.m_stScissorRect = { 0, 0, static_cast<LONG>(g_stGPUStatus.m_iWndWidth), static_cast<LONG>(g_stGPUStatus.m_iWndHeight) };
    g_stGPUStatus.m_n64FenceValue = 0ui64;
    g_stGPUStatus.m_hEventFence = nullptr;

    g_stDefautHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    g_stDefautHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    g_stDefautHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    g_stDefautHeapProps.CreationNodeMask = 0;
    g_stDefautHeapProps.VisibleNodeMask = 0;

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

    g_stDepthStencilDesc.Format = g_stGPUStatus.m_emDepthStencilFormat;
    g_stDepthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    g_stDepthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

    g_stDepthOptimizedClearValue.Format = g_stGPUStatus.m_emDepthStencilFormat;
    g_stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    g_stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

    g_stDSTex2DDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    g_stDSTex2DDesc.Alignment = 0;
    g_stDSTex2DDesc.Width = g_stGPUStatus.m_iWndWidth;
    g_stDSTex2DDesc.Height = g_stGPUStatus.m_iWndHeight;
    g_stDSTex2DDesc.DepthOrArraySize = 1;
    g_stDSTex2DDesc.MipLevels = 0;
    g_stDSTex2DDesc.Format = g_stGPUStatus.m_emDepthStencilFormat;
    g_stDSTex2DDesc.SampleDesc.Count = 1;
    g_stDSTex2DDesc.SampleDesc.Quality = 0;
    g_stDSTex2DDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    g_stDSTex2DDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    g_stDSVHeapDesc.NumDescriptors = 1;
    g_stDSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    g_stDSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    stResStateTransBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    stResStateTransBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    stResStateTransBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    try
    {
        ::CoInitialize(nullptr);  //for WIC & COM

        // 0、得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
        {
            if ( 0 == ::GetModuleFileName(nullptr, g_pszAppPath, MAX_PATH) )
            {
                GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
            }

            WCHAR* lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
            if ( lastSlash )
            {//删除Exe文件名
                *(lastSlash) = _T('\0');
            }

            lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
            if ( lastSlash )
            {//删除x64路径
                *(lastSlash) = _T('\0');
            }

            lastSlash = _tcsrchr(g_pszAppPath, _T('\\'));
            if ( lastSlash )
            {//删除Debug 或 Release路径
                *(lastSlash + 1) = _T('\0');
            }
        }

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
            wcex.lpszClassName = GRS_WND_CLASS;
            RegisterClassEx(&wcex);

            g_stGPUStatus.m_dwWndStyle = WS_OVERLAPPEDWINDOW;

            RECT rtWnd = { 0, 0, g_stGPUStatus.m_iWndWidth, g_stGPUStatus.m_iWndHeight };
            AdjustWindowRect(&rtWnd, g_stGPUStatus.m_dwWndStyle, FALSE);

            // 计算窗口居中的屏幕坐标
            INT posX = (GetSystemMetrics(SM_CXSCREEN) - rtWnd.right - rtWnd.left) / 2;
            INT posY = (GetSystemMetrics(SM_CYSCREEN) - rtWnd.bottom - rtWnd.top) / 2;

            g_stGPUStatus.m_hWnd = CreateWindowW(GRS_WND_CLASS
                , GRS_WND_TITLE
                , g_stGPUStatus.m_dwWndStyle
                , posX
                , posY
                , rtWnd.right - rtWnd.left
                , rtWnd.bottom - rtWnd.top
                , nullptr
                , nullptr
                , hInstance
                , nullptr);

            if ( !g_stGPUStatus.m_hWnd )
            {
                GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        // 2、创建DXGI Factory对象
        {
            UINT nDXGIFactoryFlags = 0U;
#if defined(_DEBUG)
            // 打开显示子系统的调试支持
            ComPtr<ID3D12Debug> debugController;
            if ( SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))) )
            {
                debugController->EnableDebugLayer();
                // 打开附加的调试支持
                nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
#endif
            ComPtr<IDXGIFactory5> pIDXGIFactory5;
            GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);

            //获取IDXGIFactory6接口
            GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
        }

        // 3、枚举适配器创建设备
        {//选择NUMA架构的独显来创建3D设备对象,暂时先不支持集显了，当然你可以修改这些行为
            ComPtr<IDXGIAdapter1> pIAdapter1;
            GRS_THROW_IF_FAILED(pIDXGIFactory6->EnumAdapterByGpuPreference(0
                , DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                , IID_PPV_ARGS(&pIAdapter1)));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(pIAdapter1);

            // 创建D3D12.1的设备
            GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter1.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3D12Device4)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3D12Device4);


            DXGI_ADAPTER_DESC1 stAdapterDesc = {};
            TCHAR pszWndTitle[MAX_PATH] = {};
            GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc));
            ::GetWindowText(g_stGPUStatus.m_hWnd, pszWndTitle, MAX_PATH);
            StringCchPrintf(pszWndTitle
                , MAX_PATH
                , _T("%s(GPU[0]:%s)")
                , pszWndTitle
                , stAdapterDesc.Description);
            ::SetWindowText(g_stGPUStatus.m_hWnd, pszWndTitle);

            // 得到每个描述符元素的大小
            g_stGPUStatus.m_nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            g_stGPUStatus.m_nDSVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            g_stGPUStatus.m_nSamplerDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
            g_stGPUStatus.m_nCBVSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        }

        // 4、创建命令队列&命令列表
        {
            D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
            stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pIMainCMDQueue)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIMainCMDQueue);

            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT
                , IID_PPV_ARGS(&pIMainCMDAlloc)));

            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIMainCMDAlloc);

            // 创建图形命令列表
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(
                0
                , D3D12_COMMAND_LIST_TYPE_DIRECT
                , pIMainCMDAlloc.Get()
                , nullptr
                , IID_PPV_ARGS(&pIMainCMDList)));

            GRS_SET_D3D12_DEBUGNAME_COMPTR(pIMainCMDList);
        }

        // 5、创建交换链
        {
            DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
            stSwapChainDesc.BufferCount = g_nFrameBackBufCount;
            stSwapChainDesc.Width = g_stGPUStatus.m_iWndWidth;
            stSwapChainDesc.Height = g_stGPUStatus.m_iWndHeight;
            stSwapChainDesc.Format = g_stGPUStatus.m_emRenderTargetFormat;
            stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            stSwapChainDesc.SampleDesc.Count = 1;
            stSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            stSwapChainDesc.Scaling = DXGI_SCALING_NONE;

            ComPtr<IDXGISwapChain1> pISwapChain1;

            GRS_THROW_IF_FAILED(pIDXGIFactory6->CreateSwapChainForHwnd(
                pIMainCMDQueue.Get(),		// 交换链需要命令队列，Present命令要执行
                g_stGPUStatus.m_hWnd,
                &stSwapChainDesc,
                nullptr,
                nullptr,
                &pISwapChain1
            ));

            GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);
            GRS_THROW_IF_FAILED(pISwapChain1.As(&g_stGPUStatus.m_pISwapChain3));
            GRS_SET_DXGI_DEBUGNAME_COMPTR(g_stGPUStatus.m_pISwapChain3);

            //得到当前后缓冲区的序号，也就是下一个将要呈送显示的缓冲区的序号
            //注意此处使用了高版本的SwapChain接口的函数
            g_stGPUStatus.m_nFrameIndex = g_stGPUStatus.m_pISwapChain3->GetCurrentBackBufferIndex();

            //创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
            {
                D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
                stRTVHeapDesc.NumDescriptors = g_nFrameBackBufCount;
                stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

                GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(
                    &stRTVHeapDesc
                    , IID_PPV_ARGS(&g_stGPUStatus.m_pIRTVHeap)));

                GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stGPUStatus.m_pIRTVHeap);

            }

            //创建RTV的描述符
            {
                D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
                    = g_stGPUStatus.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                for ( UINT i = 0; i < g_nFrameBackBufCount; i++ )
                {
                    GRS_THROW_IF_FAILED(g_stGPUStatus.m_pISwapChain3->GetBuffer(i
                        , IID_PPV_ARGS(&g_stGPUStatus.m_pIARenderTargets[i])));

                    GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(g_stGPUStatus.m_pIARenderTargets, i);

                    pID3D12Device4->CreateRenderTargetView(g_stGPUStatus.m_pIARenderTargets[i].Get()
                        , nullptr
                        , stRTVHandle);

                    stRTVHandle.ptr += g_stGPUStatus.m_nRTVDescriptorSize;
                }
            }

            // 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
            // 注意将这个屏蔽Alt+Enter组合键的动作放到了这里，是因为直到这个时候DXGI Factory通过Create Swapchain动作才知道了HWND是哪个窗口
            // 因为组合键的处理在Windows中都是基于窗口过程的，没有窗口句柄就不知道是哪个窗口以及窗口过程，之前的屏蔽就没有意义了
            GRS_THROW_IF_FAILED(pIDXGIFactory6->MakeWindowAssociation(g_stGPUStatus.m_hWnd, DXGI_MWA_NO_ALT_ENTER));
        }

        // 6、创建深度缓冲及深度缓冲描述符堆
        {
            //使用隐式默认堆创建一个深度蜡板缓冲区，
            //因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stDefautHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stDSTex2DDesc
                , D3D12_RESOURCE_STATE_DEPTH_WRITE
                , &g_stDepthOptimizedClearValue
                , IID_PPV_ARGS(&g_stGPUStatus.m_pIDepthStencilBuffer)
            ));

            //==============================================================================================
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&g_stDSVHeapDesc, IID_PPV_ARGS(&g_stGPUStatus.m_pIDSVHeap)));

            pID3D12Device4->CreateDepthStencilView(g_stGPUStatus.m_pIDepthStencilBuffer.Get()
                , &g_stDepthStencilDesc
                , g_stGPUStatus.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
        }

        // 7、创建围栏
        {
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0
                , D3D12_FENCE_FLAG_NONE
                , IID_PPV_ARGS(&g_stGPUStatus.m_pIFence)));
            GRS_SET_D3D12_DEBUGNAME_COMPTR(g_stGPUStatus.m_pIFence);

            g_stGPUStatus.m_n64FenceValue = 1;

            // 创建一个Event同步对象，用于等待围栏事件通知
            g_stGPUStatus.m_hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if ( g_stGPUStatus.m_hEventFence == nullptr )
            {
                GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
            }
        }

        // 8、填充资源屏障结构
        {
            stBeginResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            stBeginResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            stBeginResBarrier.Transition.pResource = g_stGPUStatus.m_pIARenderTargets[g_stGPUStatus.m_nFrameIndex].Get();
            stBeginResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            stBeginResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            stBeginResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            stEneResBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            stEneResBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            stEneResBarrier.Transition.pResource = g_stGPUStatus.m_pIARenderTargets[g_stGPUStatus.m_nFrameIndex].Get();
            stEneResBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            stEneResBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            stEneResBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        }

        DWORD dwRet = 0;
        BOOL bExit = FALSE;
        D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle = g_stGPUStatus.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE stDSVHandle = g_stGPUStatus.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart();

        FLOAT fStartTime = (FLOAT)::GetTickCount64();
        FLOAT fCurrentTime = fStartTime;
        FLOAT fTimeInSeconds = (fCurrentTime - fStartTime) / 1000.0f;
        FLOAT fDeltaTime = fCurrentTime - fStartTime;

        // 先关闭和重置一下，使消息循环可以正常运转
        GRS_THROW_IF_FAILED(pIMainCMDList->Close());
        SetEvent(g_stGPUStatus.m_hEventFence);

        // 开始正式消息循环前才显示主窗口
        ShowWindow(g_stGPUStatus.m_hWnd, nCmdShow);
        UpdateWindow(g_stGPUStatus.m_hWnd);

        MSG msg = {};
        // 10、消息循环
        while ( !bExit )
        {
            dwRet = ::MsgWaitForMultipleObjects(1, &g_stGPUStatus.m_hEventFence, FALSE, INFINITE, QS_ALLINPUT);
            switch ( dwRet - WAIT_OBJECT_0 )
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

                fStartTime = fCurrentTime;
                //-----------------------------------------------------------------------------------------------------
                // OnRender()
                //----------------------------------------------------------------------------------------------------- 
                //命令分配器先Reset一下
                GRS_THROW_IF_FAILED(pIMainCMDAlloc->Reset());
                GRS_THROW_IF_FAILED(pIMainCMDList->Reset(pIMainCMDAlloc.Get(), nullptr));

                g_stGPUStatus.m_nFrameIndex = g_stGPUStatus.m_pISwapChain3->GetCurrentBackBufferIndex();

                pIMainCMDList->RSSetViewports(1, &g_stGPUStatus.m_stViewPort);
                pIMainCMDList->RSSetScissorRects(1, &g_stGPUStatus.m_stScissorRect);

                // 通过资源屏障判定后缓冲已经切换完毕可以开始渲染了
                stBeginResBarrier.Transition.pResource = g_stGPUStatus.m_pIARenderTargets[g_stGPUStatus.m_nFrameIndex].Get();
                pIMainCMDList->ResourceBarrier(1, &stBeginResBarrier);

                // 设置渲染目标
                stRTVHandle = g_stGPUStatus.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
                stRTVHandle.ptr += (size_t)g_stGPUStatus.m_nFrameIndex * g_stGPUStatus.m_nRTVDescriptorSize;
                pIMainCMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);

                // 继续记录命令，并真正开始新一帧的渲染
                pIMainCMDList->ClearRenderTargetView(stRTVHandle, faClearColor, 0, nullptr);
                pIMainCMDList->ClearDepthStencilView(g_stGPUStatus.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart()
                    , D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                //-----------------------------------------------------------------------------------------------------
                // Draw !
                //-----------------------------------------------------------------------------------------------------

                //-----------------------------------------------------------------------------------------------------
                //又一个资源屏障，用于确定渲染已经结束可以提交画面去显示了
                stEneResBarrier.Transition.pResource = g_stGPUStatus.m_pIARenderTargets[g_stGPUStatus.m_nFrameIndex].Get();
                pIMainCMDList->ResourceBarrier(1, &stEneResBarrier);

                //关闭命令列表，可以去执行了
                GRS_THROW_IF_FAILED(pIMainCMDList->Close());

                //执行命令列表
                ID3D12CommandList* ppCommandLists[] = { pIMainCMDList.Get() };
                pIMainCMDQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

                //提交画面
                GRS_THROW_IF_FAILED(g_stGPUStatus.m_pISwapChain3->Present(1, 0));

                //开始同步GPU与CPU的执行，先记录围栏标记值
                const UINT64 n64CurrentFenceValue = g_stGPUStatus.m_n64FenceValue;
                GRS_THROW_IF_FAILED(pIMainCMDQueue->Signal(g_stGPUStatus.m_pIFence.Get(), n64CurrentFenceValue));
                g_stGPUStatus.m_n64FenceValue++;
                GRS_THROW_IF_FAILED(g_stGPUStatus.m_pIFence->SetEventOnCompletion(n64CurrentFenceValue, g_stGPUStatus.m_hEventFence));
                //-----------------------------------------------------------------------------------------------------
            }
            break;
            case 1:
            {// 处理消息
                while ( ::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) )
                {
                    if ( WM_QUIT != msg.message )
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
    catch ( CAtlException& e )
    {//发生了COM异常
        e;
    }
    catch ( ... )
    {

    }
    return 0;
}

void OnSize(UINT width, UINT height, bool minimized)
{
    if ( (width == g_stGPUStatus.m_iWndWidth && height == g_stGPUStatus.m_iWndHeight)
        || minimized
        || (nullptr == g_stGPUStatus.m_pISwapChain3)
        || (0 == width)
        || (0 == height) )
    {
        return;
    }

    try
    {
        // 1、消息循环旁路
        if ( WAIT_OBJECT_0 != ::WaitForSingleObject(g_stGPUStatus.m_hEventFence, INFINITE) )
        {
            GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
        }

        // 2、释放获取的后缓冲对象接口
        for ( UINT i = 0; i < g_nFrameBackBufCount; i++ )
        {
            g_stGPUStatus.m_pIARenderTargets[i].Reset();

        }

        // 3、交换链重置大小
        DXGI_SWAP_CHAIN_DESC stSWDesc = {};
        g_stGPUStatus.m_pISwapChain3->GetDesc(&stSWDesc);

        HRESULT hr = g_stGPUStatus.m_pISwapChain3->ResizeBuffers(g_nFrameBackBufCount
            , width
            , height
            , stSWDesc.BufferDesc.Format
            , stSWDesc.Flags);

        GRS_THROW_IF_FAILED(hr);

        // 4、得到当前后缓冲索引
        g_stGPUStatus.m_nFrameIndex = g_stGPUStatus.m_pISwapChain3->GetCurrentBackBufferIndex();

        // 5、保存变换后的尺寸
        g_stGPUStatus.m_iWndWidth = width;
        g_stGPUStatus.m_iWndHeight = height;

        // 6、创建RTV的描述符
        ComPtr<ID3D12Device> pID3D12Device;
        ComPtr<ID3D12Device4> pID3D12Device4;
        GRS_THROW_IF_FAILED(g_stGPUStatus.m_pIRTVHeap->GetDevice(IID_PPV_ARGS(&pID3D12Device)));
        GRS_THROW_IF_FAILED(pID3D12Device.As(&pID3D12Device4));
        {
            D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
                = g_stGPUStatus.m_pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
            for ( UINT i = 0; i < g_nFrameBackBufCount; i++ )
            {
                GRS_THROW_IF_FAILED(g_stGPUStatus.m_pISwapChain3->GetBuffer(i
                    , IID_PPV_ARGS(&g_stGPUStatus.m_pIARenderTargets[i])));

                GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(g_stGPUStatus.m_pIARenderTargets, i);

                pID3D12Device4->CreateRenderTargetView(g_stGPUStatus.m_pIARenderTargets[i].Get()
                    , nullptr
                    , stRTVHandle);

                stRTVHandle.ptr += g_stGPUStatus.m_nRTVDescriptorSize;
            }
        }

        // 7、创建深度缓冲及深度缓冲描述符堆
        {
            g_stDSTex2DDesc.Width = g_stGPUStatus.m_iWndWidth;
            g_stDSTex2DDesc.Height = g_stGPUStatus.m_iWndHeight;

            // 释放深度缓冲先
            g_stGPUStatus.m_pIDepthStencilBuffer.Reset();

            //使用隐式默认堆创建一个深度蜡板缓冲区，
            //因为基本上深度缓冲区会一直被使用，重用的意义不大，所以直接使用隐式堆，图方便
            GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
                &g_stDefautHeapProps
                , D3D12_HEAP_FLAG_NONE
                , &g_stDSTex2DDesc
                , D3D12_RESOURCE_STATE_DEPTH_WRITE
                , &g_stDepthOptimizedClearValue
                , IID_PPV_ARGS(&g_stGPUStatus.m_pIDepthStencilBuffer)
            ));

            // 然后直接重建描述符
            pID3D12Device4->CreateDepthStencilView(g_stGPUStatus.m_pIDepthStencilBuffer.Get()
                , &g_stDepthStencilDesc
                , g_stGPUStatus.m_pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
        }

        // 8、更新视口大小和齐次包围盒大小
        g_stGPUStatus.m_stViewPort.TopLeftX = 0.0f;
        g_stGPUStatus.m_stViewPort.TopLeftY = 0.0f;
        g_stGPUStatus.m_stViewPort.Width = g_stGPUStatus.m_iWndWidth;
        g_stGPUStatus.m_stViewPort.Height = g_stGPUStatus.m_iWndHeight;

        g_stGPUStatus.m_stScissorRect.left = static_cast<LONG>(g_stGPUStatus.m_stViewPort.TopLeftX);
        g_stGPUStatus.m_stScissorRect.right = static_cast<LONG>(g_stGPUStatus.m_stViewPort.TopLeftX + g_stGPUStatus.m_stViewPort.Width);
        g_stGPUStatus.m_stScissorRect.top = static_cast<LONG>(g_stGPUStatus.m_stViewPort.TopLeftY);
        g_stGPUStatus.m_stScissorRect.bottom = static_cast<LONG>(g_stGPUStatus.m_stViewPort.TopLeftY + g_stGPUStatus.m_stViewPort.Height);

        // 9、设置下事件状态，从消息旁路返回主消息循环        
        SetEvent(g_stGPUStatus.m_hEventFence);
    }
    catch ( CAtlException& e )
    {//发生了COM异常
        e;
    }
    catch ( ... )
    {

    }

}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch ( message )
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    }
    break;
    case WM_KEYDOWN:
    {
        USHORT n16KeyCode = (wParam & 0xFF);
        if ( VK_ESCAPE == n16KeyCode )
        {// Esc键退出
            ::DestroyWindow(hWnd);
        }

        if ( VK_TAB == n16KeyCode )
        {// 按Tab键
        }

        if ( VK_SPACE == n16KeyCode )
        {// 按Space键

        }

        if ( VK_F3 == n16KeyCode )
        {// F3键

        }

        //-------------------------------------------------------------------------------
        // 基本摄像机位置移动操作
        float fDelta = 0.2f;
        XMVECTOR vDelta = { 0.0f,0.0f,fDelta,0.0f };
        // Z-轴
        if ( VK_UP == n16KeyCode || 'w' == n16KeyCode || 'W' == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorAdd(g_v4LookAt, vDelta);
        }

        if ( VK_DOWN == n16KeyCode || 's' == n16KeyCode || 'S' == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorSubtract(g_v4LookAt, vDelta);
        }

        // X-轴
        vDelta = { fDelta,0.0f,0.0f,0.0f };
        if ( VK_LEFT == n16KeyCode || 'a' == n16KeyCode || 'A' == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorAdd(g_v4LookAt, vDelta);
        }

        if ( VK_RIGHT == n16KeyCode || 'd' == n16KeyCode || 'D' == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorSubtract(g_v4LookAt, vDelta);
        }

        // Y-轴
        vDelta = { 0.0f,fDelta,0.0f,0.0f };
        if ( VK_PRIOR == n16KeyCode || 'r' == n16KeyCode || 'R' == n16KeyCode )
        {
            g_v4EyePos = XMVectorAdd(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorAdd(g_v4LookAt, vDelta);
        }

        if ( VK_NEXT == n16KeyCode || 'f' == n16KeyCode || 'F' == n16KeyCode )
        {
            g_v4EyePos = XMVectorSubtract(g_v4EyePos, vDelta);
            g_v4LookAt = XMVectorSubtract(g_v4LookAt, vDelta);
        }


        if ( VK_ADD == n16KeyCode || VK_OEM_PLUS == n16KeyCode )
        {// + 键

        }

        if ( VK_SUBTRACT == n16KeyCode || VK_OEM_MINUS == n16KeyCode )
        {// - 键
        }
    }
    break;
    case WM_SIZE:
    {
        ::GetClientRect(hWnd, &g_stGPUStatus.m_rtWndClient);
        OnSize(g_stGPUStatus.m_rtWndClient.right - g_stGPUStatus.m_rtWndClient.left
            , g_stGPUStatus.m_rtWndClient.bottom - g_stGPUStatus.m_rtWndClient.top
            , wParam == SIZE_MINIMIZED);
        // 下面这行注释掉了，不然Win10上面会报莫名其妙的UWP异常
        //return 0; // 不再让系统继续处理 
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
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}