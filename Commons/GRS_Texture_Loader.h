#pragma once
#include <strsafe.h>
#include <wrl.h>
#include <atlconv.h>
#include <atltrace.h>
#include <atlexcept.h>

#include "GRS_WIC_Utility.h"

using namespace ATL;
using namespace Microsoft;
using namespace Microsoft::WRL;

// 加载纹理
__inline BOOL LoadTextureFromMem( ID3D12GraphicsCommandList* pCMDList
    , const BYTE* pbImageData
    , const size_t& szImageBufferSize
    , const DXGI_FORMAT emTextureFormat
    , const UINT 		nTextureW
    , const UINT 		nTextureH
    , const UINT 		nPicRowPitch
    , ID3D12Resource*& pITextureUpload
    , ID3D12Resource*& pITexture )
{
    BOOL bRet = TRUE;
    try
    {
        ComPtr<ID3D12Device> pID3D12Device;
        ComPtr<ID3D12GraphicsCommandList> pICMDList( pCMDList );

        GRS_THROW_IF_FAILED( pICMDList->GetDevice( IID_PPV_ARGS( &pID3D12Device ) ) );

        D3D12_HEAP_PROPERTIES stTextureHeapProp = {};
        stTextureHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC	stTextureDesc = {};

        stTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        stTextureDesc.MipLevels = 1;
        stTextureDesc.Format = emTextureFormat;
        stTextureDesc.Width = nTextureW;
        stTextureDesc.Height = nTextureH;
        stTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        stTextureDesc.DepthOrArraySize = 1;
        stTextureDesc.SampleDesc.Count = 1;
        stTextureDesc.SampleDesc.Quality = 0;

        GRS_THROW_IF_FAILED( pID3D12Device->CreateCommittedResource(
            &stTextureHeapProp
            , D3D12_HEAP_FLAG_NONE
            , &stTextureDesc				//可以使用CD3DX12_RESOURCE_DESC::Tex2D来简化结构体的初始化
            , D3D12_RESOURCE_STATE_COPY_DEST
            , nullptr
            , IID_PPV_ARGS( &pITexture ) ) );

        //获取需要的上传堆资源缓冲的大小，这个尺寸通常大于实际图片的尺寸
        D3D12_RESOURCE_DESC stDestDesc = pITexture->GetDesc();
        UINT64 n64UploadBufferSize = 0;
        pID3D12Device->GetCopyableFootprints( &stDestDesc, 0, 1, 0, nullptr, nullptr, nullptr, &n64UploadBufferSize );

        stTextureHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC stUploadTextureDesc = {};

        stUploadTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        stUploadTextureDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
        stUploadTextureDesc.Width = n64UploadBufferSize;
        stUploadTextureDesc.Height = 1;
        stUploadTextureDesc.DepthOrArraySize = 1;
        stUploadTextureDesc.MipLevels = 1;
        stUploadTextureDesc.Format = DXGI_FORMAT_UNKNOWN;
        stUploadTextureDesc.SampleDesc.Count = 1;
        stUploadTextureDesc.SampleDesc.Quality = 0;
        stUploadTextureDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        stUploadTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        GRS_THROW_IF_FAILED( pID3D12Device->CreateCommittedResource(
            &stTextureHeapProp
            , D3D12_HEAP_FLAG_NONE
            , &stUploadTextureDesc
            , D3D12_RESOURCE_STATE_GENERIC_READ
            , nullptr
            , IID_PPV_ARGS( &pITextureUpload ) ) );

        //获取向上传堆拷贝纹理数据的一些纹理转换尺寸信息
        //对于复杂的DDS纹理这是非常必要的过程
        UINT   nNumSubresources = 1u;  //我们只有一副图片，即子资源个数为1
        UINT   nTextureRowNum = 0u;
        UINT64 n64TextureRowSizes = 0u;
        UINT64 n64RequiredSize = 0u;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT	stTxtLayouts = {};

        pID3D12Device->GetCopyableFootprints( &stDestDesc
            , 0
            , nNumSubresources
            , 0
            , &stTxtLayouts
            , &nTextureRowNum
            , &n64TextureRowSizes
            , &n64RequiredSize );

        //因为上传堆实际就是CPU传递数据到GPU的中介
        //所以我们可以使用熟悉的Map方法将它先映射到CPU内存地址中
        //然后我们按行将数据复制到上传堆中
        //需要注意的是之所以按行拷贝是因为GPU资源的行大小
        //与实际图片的行大小是有差异的,二者的内存边界对齐要求是不一样的
        BYTE* pData = nullptr;
        GRS_THROW_IF_FAILED( pITextureUpload->Map( 0, NULL, reinterpret_cast<void**>( &pData ) ) );

        BYTE* pDestSlice = reinterpret_cast<BYTE*>( pData ) + stTxtLayouts.Offset;
        const BYTE* pSrcSlice = reinterpret_cast<const BYTE*>( pbImageData );
        for ( UINT y = 0; y < nTextureRowNum; ++y )
        {
            memcpy( pDestSlice + static_cast<SIZE_T>( stTxtLayouts.Footprint.RowPitch ) * y
                , pSrcSlice + static_cast<SIZE_T>( nPicRowPitch ) * y
                , nPicRowPitch );
        }
        //取消映射 对于易变的数据如每帧的变换矩阵等数据，可以撒懒不用Unmap了，
        //让它常驻内存,以提高整体性能，因为每次Map和Unmap是很耗时的操作
        //因为现在起码都是64位系统和应用了，地址空间是足够的，被长期占用不会影响什么
        pITextureUpload->Unmap( 0, NULL );

        ////释放图片数据，做一个干净的程序员
        //GRS_SAFE_FREE((VOID*)pbImageData);

        D3D12_TEXTURE_COPY_LOCATION stDstCopyLocation = {};
        stDstCopyLocation.pResource = pITexture;
        stDstCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        stDstCopyLocation.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION stSrcCopyLocation = {};
        stSrcCopyLocation.pResource = pITextureUpload;
        stSrcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        stSrcCopyLocation.PlacedFootprint = stTxtLayouts;

        pICMDList->CopyTextureRegion( &stDstCopyLocation, 0, 0, 0, &stSrcCopyLocation, nullptr );

        //设置一个资源屏障，同步并确认复制操作完成
        //直接使用结构体然后调用的形式
        D3D12_RESOURCE_BARRIER stResBar = {};
        stResBar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        stResBar.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        stResBar.Transition.pResource = pITexture;
        stResBar.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        stResBar.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        stResBar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        pICMDList->ResourceBarrier( 1, &stResBar );
    }
    catch ( CAtlException& e )
    {//发生了COM异常
        e;
        bRet = FALSE;
    }
    catch ( ... )
    {
        bRet = FALSE;
    }

    return bRet;
}


// 加载纹理
__inline BOOL LoadTextureFromFile(
    LPCWSTR pszTextureFile
    , ID3D12GraphicsCommandList* pCMDList
    , ID3D12Resource*& pITextureUpload
    , ID3D12Resource*& pITexture )
{
    BOOL bRet = TRUE;
    try
    {
        BYTE*       pbImageData = nullptr;
        size_t		szImageBufferSize = 0;
        DXGI_FORMAT emTextureFormat = DXGI_FORMAT_UNKNOWN;
        UINT 		nTextureW = 0;
        UINT 		nTextureH = 0;
        UINT 		nPicRowPitch = 0;

        bRet = WICLoadImageFromFile( pszTextureFile
            , emTextureFormat
            , nTextureW
            , nTextureH
            , nPicRowPitch
            , pbImageData
            , szImageBufferSize );

        if ( bRet )
        {
            bRet = LoadTextureFromMem( pCMDList
                , pbImageData
                , szImageBufferSize
                , emTextureFormat
                , nTextureW
                , nTextureH
                , nPicRowPitch
                , pITextureUpload
                , pITexture
            );

            GRS_SAFE_FREE( pbImageData );
        }
    }
    catch ( CAtlException& e )
    {//发生了COM异常
        e;
        bRet = FALSE;
    }
    catch ( ... )
    {
        bRet = FALSE;
    }
    return bRet;
}