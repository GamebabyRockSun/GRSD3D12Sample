#pragma once
#include <strsafe.h>
#include <wrl.h> //添加WTL支持 方便使用COM
#include <atlbase.h>
#include <atlcoll.h>
#include <atlchecked.h>
#include <atlstr.h>
#include <atlconv.h>

#include <d3d12.h> //for d3d12
#include <DirectXMath.h>
using namespace DirectX;
using namespace Microsoft::WRL;

#include "GRS_Assimp_Loader.h"

struct ST_GRS_MESH_DATA_MULTI_SLOT
{
    ST_GRS_MESH_DATA                        m_stMeshData;

    UINT                                    m_nMaxSlot;
    CGRSStringArrayA                        m_arSemanticName;
    CAtlArray< D3D12_INPUT_ELEMENT_DESC >   m_arInputElementDesc;

    D3D12_INDEX_BUFFER_VIEW                 m_stIBV;
    ComPtr<ID3D12Resource>                  m_pIIB;

    CAtlArray<D3D12_VERTEX_BUFFER_VIEW>     m_arVBV;
    CAtlArray<ComPtr<ID3D12Resource>>       m_arIVB;

    CAtlArray<ComPtr<ID3D12Resource>>       m_ppITexture;   
};

BOOL LoadMesh_DX12( LPCSTR pszFileName
    , ID3D12Device4* pID3D12Device4
    , ST_GRS_MESH_DATA_MULTI_SLOT& stMeshBufferData
    , UINT nFlags = ASSIMP_LOAD_FLAGS_DEFAULT);