#pragma once

#include <strsafe.h>
#include <atlbase.h>
#include <atlcoll.h>
#include <atlchecked.h>
#include <atlstr.h>
#include <atlconv.h>

#include <DirectXMath.h>
using namespace DirectX;

#include "assimp/Importer.hpp"      // 导入器在该头文件中定义
#include "assimp/scene.h"           // 读取到的模型数据都放在scene中
#include "assimp/postprocess.h"

// 导入文件时预处理的标志
// aiProcess_LimitBoneWeights
// aiProcess_OptimizeMeshes
// aiProcess_MakeLeftHanded
// aiProcess_ConvertToLeftHanded
// aiProcess_MakeLeftHanded
// aiProcess_ConvertToLeftHanded
//| aiProcess_MakeLeftHanded\

#define ASSIMP_LOAD_FLAGS (aiProcess_Triangulate\
 | aiProcess_GenSmoothNormals\
 | aiProcess_GenBoundingBoxes\
 | aiProcess_JoinIdenticalVertices\
 | aiProcess_FlipUVs\
 | aiProcess_ConvertToLeftHanded\
 | aiProcess_LimitBoneWeights)			

// 每个表面的索引数，（根据上面的导入标志，导入的模型应该都是严格的三角形网格）
#define GRS_INDICES_PER_FACE 3

// 定义每个骨骼数据中子项的个数（根据上面的导入标志，现在严格限定每个顶点被最多4根骨头影响）
#define GRS_BONE_DATACNT 4

// 定义最大可支持的骨头数量
#define GRS_MAX_BONES 256


struct ST_GRS_VERTEX_BONE
{
public:
	UINT32  m_nBonesIDs[GRS_BONE_DATACNT];
	FLOAT	m_fWeights[GRS_BONE_DATACNT];
public:
	VOID AddBoneData(UINT nBoneID, FLOAT fWeight)
	{
		for (UINT32 i = 0; i < GRS_BONE_DATACNT; i++)
		{
			if ( m_fWeights[i] == 0.0 )
			{
				m_nBonesIDs[i] = nBoneID;
				m_fWeights[i] = fWeight;
				break;
			}
		}
	}
};

// 基本骨骼动画中每根骨头相对于模型空间的位移矩阵
// 其中BoneOffset是位移
// FinalTransformation用于解算最终动画时保存真实的每根骨头的变换
struct ST_GRS_BONE_DATA
{
	XMMATRIX m_mxBoneOffset;
	XMMATRIX m_mxFinalTransformation;
};

typedef CAtlArray<XMFLOAT4>						CGRSARPositions;
typedef CAtlArray<XMFLOAT4>						CGRSARNormals;
typedef CAtlArray<XMFLOAT2>						CGRSARTexCoords;
typedef CAtlArray<UINT>								CGRSARIndices;
typedef CAtlArray<ST_GRS_VERTEX_BONE>	CGRSARVertexBones;
typedef CAtlArray<ST_GRS_BONE_DATA>		CGRSARBoneDatas;
typedef CAtlArray<CStringA>						CGRSARTTextureName;
typedef CAtlArray<XMMATRIX>					CGRSARMatrix;
typedef CAtlMap<UINT, CStringA>				CGRSMapUINT2String;
typedef CAtlMap<CStringA, UINT>				CGRSMapString2UINT;
typedef CAtlMap<UINT, UINT>						CGRSMapUINT2UINT;
// 模型中子网格的顶点偏移等信息
struct ST_GRS_SUBMESH_DATA
{
	UINT m_nNumIndices;
	UINT m_nBaseVertex;
	UINT m_nBaseIndex;
	UINT m_nMaterialIndex;
};

typedef CAtlArray<ST_GRS_SUBMESH_DATA> CGRSSubMesh;

const UINT			g_ncSlotCnt = 4;		// 用4个插槽上传顶点数据
struct ST_GRS_MESH_DATA
{
	const aiScene*		m_paiModel;
	CStringA			m_strFileName;
	XMMATRIX			m_mxModel;

	CGRSSubMesh			m_arSubMeshInfo;

	CGRSARPositions		m_arPositions;
	CGRSARNormals		m_arNormals;
	CGRSARTexCoords		m_arTexCoords;
	CGRSARVertexBones	m_arBoneIndices;
	CGRSARIndices		m_arIndices;

	CGRSMapString2UINT	m_mapTextrueName2Index;
	CGRSMapUINT2UINT	m_mapTextureIndex2HeapIndex;

	CGRSARBoneDatas		m_arBoneDatas;
	CGRSMapString2UINT	m_mapName2Bone;			//名称->骨骼的索引
	CGRSMapString2UINT	m_mapAnimName2Index;	//名称->动画的索引

	UINT				m_nCurrentAnimIndex;	// 当前播放的动画序列索引（当前动作）
};

__inline const XMMATRIX& MXEqual(XMMATRIX& mxDX, const aiMatrix4x4& mxAI)
{
	mxDX = XMMatrixSet(mxAI.a1, mxAI.a2, mxAI.a3, mxAI.a4,
		mxAI.b1, mxAI.b2, mxAI.b3, mxAI.b4,
		mxAI.c1, mxAI.c2, mxAI.c3, mxAI.c4,
		mxAI.d1, mxAI.d2, mxAI.d3, mxAI.d4);
	return mxDX;
}

__inline const XMVECTOR& VectorEqual(XMVECTOR& vDX, const aiVector3D& vAI)
{
	vDX = XMVectorSet(vAI.x, vAI.y, vAI.z, 0);
	return vDX;
}

__inline const XMVECTOR& QuaternionEqual(XMVECTOR& qDX, const aiQuaternion& qAI)
{
	qDX = XMVectorSet(qAI.x, qAI.y, qAI.z, qAI.w);
	return qDX;
}

__inline VOID VectorLerp(XMVECTOR& vOut, aiVector3D& aivStart, aiVector3D& aivEnd, FLOAT t)
{
	XMVECTOR vStart = XMVectorSet(aivStart.x, aivStart.y, aivStart.z, 0);
	XMVECTOR vEnd = XMVectorSet(aivEnd.x, aivEnd.y, aivEnd.z, 0);
	vOut = XMVectorLerp(vStart, vEnd, t);
}

__inline VOID QuaternionSlerp(XMVECTOR& vOut, aiQuaternion& qStart, aiQuaternion& qEnd, FLOAT t)
{
	//DirectXMath四元数函数使用XMVECTOR 4 - vector来表示四元数，其中X、Y和Z分量是矢量部分，W分量是标量部分。

	XMVECTOR qdxStart;
	XMVECTOR qdxEnd;
	QuaternionEqual(qdxStart, qStart);
	QuaternionEqual(qdxEnd, qEnd);

	vOut = XMQuaternionSlerp(qdxStart, qdxEnd, t);
}

BOOL LoadMesh(LPCSTR pszFileName, ST_GRS_MESH_DATA& stMeshData);
VOID CalcAnimation(ST_GRS_MESH_DATA& stMeshData, FLOAT fTimeInSeconds, CGRSARMatrix& arTransforms);
