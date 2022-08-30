#pragma once

#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // 从 Windows 头中排除极少使用的资料
#include <windows.h>
#include <fstream>  //for ifstream
#include <DirectXMath.h>
#include <atlexcept.h>
#include "GRS_Mem.h"

using namespace std;
using namespace DirectX;
using namespace ATL;

struct ST_GRS_VERTEX
{
    XMFLOAT4 m_v4Position;	// Position
    XMFLOAT4 m_v4Normal;	// Normal
    XMFLOAT2 m_v2UV;		// Texcoord
};

__inline BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, UINT*& ppIndices)
{
    ifstream fin;
    char input;
    BOOL bRet = TRUE;
    try
    {
        fin.open(pszMeshFileName);
        if (fin.fail())
        {
            AtlThrowLastWin32();
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

        ppVertex = (ST_GRS_VERTEX*)GRS_CALLOC(nVertexCnt * sizeof(ST_GRS_VERTEX));
        ppIndices = (UINT*)GRS_CALLOC(nVertexCnt * sizeof(UINT));

        for (UINT i = 0; i < nVertexCnt; i++)
        {
            fin >> ppVertex[i].m_v4Position.x >> ppVertex[i].m_v4Position.y >> ppVertex[i].m_v4Position.z;
            ppVertex[i].m_v4Position.w = 1.0f;      //点的第4维坐标设为1
            fin >> ppVertex[i].m_v2UV.x >> ppVertex[i].m_v2UV.y;
            fin >> ppVertex[i].m_v4Normal.x >> ppVertex[i].m_v4Normal.y >> ppVertex[i].m_v4Normal.z;
            ppVertex[i].m_v4Normal.w = 0.0f;        //纯向量的第4维坐标设为0
            ppIndices[i] = i;
        }
    }
    catch (CAtlException& e)
    {
        e;
        bRet = FALSE;
    }
    return bRet;
}