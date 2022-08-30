#include "GRS_Assimp_Loader.h"

#pragma comment(lib, "assimp-vc142-mtd.lib")

#define GRS_USEPRINTFA() CHAR pBuf[1024] = {};
#define GRS_TRACEA(...) \
    StringCchPrintfA(pBuf,1024,__VA_ARGS__);\
	OutputDebugStringA(pBuf);

static Assimp::Importer g_aiImporter;

BOOL LoadMesh(LPCSTR pszFileName, ST_GRS_MESH_DATA& stMeshData,UINT nFlags)
{
	stMeshData.m_nCurrentAnimIndex = 0;
	stMeshData.m_paiModel = g_aiImporter.ReadFile(pszFileName, nFlags);

	if ( nullptr == stMeshData.m_paiModel )
	{
		ATLTRACE("无法解析文件(%s)：%s (%d)\n"
			, pszFileName
			, g_aiImporter.GetErrorString()
			, ::GetLastError());
		return FALSE;
	}

	// 获取根节点的变换矩阵，其实就是 Module->World 的变换矩阵
	stMeshData.m_mxModel = XMMatrixTranspose(MXEqual(stMeshData.m_mxModel, stMeshData.m_paiModel->mRootNode->mTransformation));

	// 获取网格数量
	UINT nMeshCnt = stMeshData.m_paiModel->mNumMeshes;
	if ( 0 == nMeshCnt )
	{
		ATLTRACE("文件(%s)中没有网格数据！\n", pszFileName);
		return FALSE;
	}

	const aiMesh* paiSubMesh = nullptr;
	const aiVector3D	Zero3D(0.0f, 0.0f, 0.0f);
	UINT nNumBones = 0;
	UINT nNumVertices = 0;
	UINT nNumIndices = 0;

	stMeshData.m_arSubMeshInfo.SetCount(nMeshCnt);

	// 加载Vertex基本信息
	for (UINT i = 0; i < nMeshCnt; i++)
	{
		paiSubMesh = stMeshData.m_paiModel->mMeshes[i];

		stMeshData.m_arSubMeshInfo[i].m_nMaterialIndex	= paiSubMesh->mMaterialIndex;
		stMeshData.m_arSubMeshInfo[i].m_nNumIndices		= paiSubMesh->mNumFaces * GRS_INDICES_PER_FACE;
		stMeshData.m_arSubMeshInfo[i].m_nBaseVertex		= nNumVertices;
		stMeshData.m_arSubMeshInfo[i].m_nBaseIndex		= nNumIndices;

		stMeshData.m_arSubMeshInfo[i].m_bHasPosition	= paiSubMesh->HasPositions();
		stMeshData.m_arSubMeshInfo[i].m_bHasNormal		= paiSubMesh->HasNormals();
		stMeshData.m_arSubMeshInfo[i].m_bHasTexCoords	= paiSubMesh->HasTextureCoords(0);
		stMeshData.m_arSubMeshInfo[i].m_bHasTangent		= paiSubMesh->HasTangentsAndBitangents();

		// 当前Mesh的顶点数量和索引数量累加后，就是下个Mesh顶点和索引在整体缓冲中的索引开始位置
		nNumVertices	+= stMeshData.m_paiModel->mMeshes[i]->mNumVertices;
		nNumIndices		+= stMeshData.m_arSubMeshInfo[i].m_nNumIndices;		

		// 加载顶点常规数据
		for ( UINT j = 0; j < paiSubMesh->mNumVertices; j++ )
		{
			if ( stMeshData.m_arSubMeshInfo[i].m_bHasPosition )
			{
				stMeshData.m_arPositions.Add(XMFLOAT4(paiSubMesh->mVertices[j].x
					, paiSubMesh->mVertices[j].y
					, paiSubMesh->mVertices[j].z
					, 1.0f));
			}
			else
			{
				stMeshData.m_arPositions.Add(XMFLOAT4(0.0f,0.0f,0.0f,1.0f));
			}

			if( stMeshData.m_arSubMeshInfo[i].m_bHasNormal )
			{
				stMeshData.m_arNormals.Add(XMFLOAT4(paiSubMesh->mNormals[j].x
					, paiSubMesh->mNormals[j].y
					, paiSubMesh->mNormals[j].z
					, 0.0f));
			}
			else
			{
				stMeshData.m_arNormals.Add(XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
			}

			if ( stMeshData.m_arSubMeshInfo[i].m_bHasTangent )
			{
				stMeshData.m_arTangent.Add(XMFLOAT4( paiSubMesh->mTangents[j].x
				, paiSubMesh->mTangents[j].y
				, paiSubMesh->mTangents[j].z
					, 0.0f));
				stMeshData.m_arBitangent.Add(XMFLOAT4(paiSubMesh->mBitangents[j].x
					, paiSubMesh->mBitangents[j].y
					, paiSubMesh->mBitangents[j].z
					, 0.0f));
			}
			else
			{
				stMeshData.m_arTangent.Add(XMFLOAT4(0.0f,0.0f,0.0f, 0.0f));
				stMeshData.m_arBitangent.Add(XMFLOAT4(0.0f,0.0f,0.0f, 0.0f));
			}

			// 注意这个地方只考虑一个纹理的情况，其实最多可以有八个，可以再做个循环进行加载
			if ( stMeshData.m_arSubMeshInfo[i].m_bHasTexCoords )
			{
				stMeshData.m_arTexCoords.Add(XMFLOAT2(paiSubMesh->mTextureCoords[0][j].x
					, paiSubMesh->mTextureCoords[0][j].y));
			}
			else
			{
				stMeshData.m_arTexCoords.Add(XMFLOAT2(0.0f,0.0f));
			}
		}

		// 加载索引数据
		for (UINT j = 0; j < paiSubMesh->mNumFaces; j++)
		{
			const aiFace& Face = paiSubMesh->mFaces[j];
			// 已经通过导入标志强制为三角形网格了，每个面就三个索引
			ATLASSERT( Face.mNumIndices == GRS_INDICES_PER_FACE );

			for (UINT k = 0; k < Face.mNumIndices; k++)
			{
				stMeshData.m_arIndices.Add(Face.mIndices[k]);
			}
		}
	}

	stMeshData.m_arBoneIndices.SetCount(nNumVertices);

	UINT		VertexID = 0;
	FLOAT		Weight = 0.0f;
	UINT		nBoneIndex = 0;
	CStringA	strBoneName;
	aiMatrix4x4 mxBoneOffset;
	aiBone*		pBone = nullptr;

	// 加载骨骼数据
	for (UINT i = 0; i < nMeshCnt; i++)
	{
		paiSubMesh = stMeshData.m_paiModel->mMeshes[i];

		for (UINT j = 0; j < paiSubMesh->mNumBones; j++)
		{
			nBoneIndex = 0;
			pBone = paiSubMesh->mBones[j];
			strBoneName = pBone->mName.data;

			if ( nullptr == stMeshData.m_mapName2Bone.Lookup(strBoneName) )
			{
				// 新骨头索引
				nBoneIndex = nNumBones ++;
				stMeshData.m_arBoneDatas.SetCount(nNumBones);

				stMeshData.m_arBoneDatas[nBoneIndex].m_mxBoneOffset 
					= XMMatrixTranspose(MXEqual(stMeshData.m_arBoneDatas[nBoneIndex].m_mxBoneOffset, pBone->mOffsetMatrix));

				stMeshData.m_mapName2Bone.SetAt(strBoneName, nBoneIndex);
			}
			else
			{
				nBoneIndex = stMeshData.m_mapName2Bone[strBoneName];
			}

			for (UINT k = 0; k < pBone->mNumWeights; k++)
			{
				VertexID = stMeshData.m_arSubMeshInfo[i].m_nBaseVertex + pBone->mWeights[k].mVertexId;
				Weight = pBone->mWeights[k].mWeight;
				stMeshData.m_arBoneIndices[VertexID].AddBoneData(nBoneIndex, Weight);
			}
		}
	}

	// 获取材质数量
	UINT nMatCnt = stMeshData.m_paiModel->mNumMaterials;
	UINT nTextureIndex = 0;
	UINT nTmpIndex = 0;
	CStringA strTextureFileName;
	aiString aistrPath;
	for (UINT i = 0; i < stMeshData.m_paiModel->mNumMaterials; i++)
	{
		const aiMaterial* pMaterial = stMeshData.m_paiModel->mMaterials[i];
		if (pMaterial->GetTextureCount(aiTextureType_DIFFUSE) > 0)
		{
			if ( pMaterial->GetTexture(aiTextureType_DIFFUSE
				, 0, &aistrPath, nullptr, nullptr, nullptr, nullptr, nullptr)
				== AI_SUCCESS )
			{
				strTextureFileName = aistrPath.C_Str();
				nTmpIndex = 0;
				if ( !stMeshData.m_mapTextrueName2Index.Lookup( strTextureFileName , nTmpIndex ) )
				{
					stMeshData.m_mapTextrueName2Index.SetAt( strTextureFileName, nTextureIndex );
					nTmpIndex = nTextureIndex;
					++ nTextureIndex;
				}	
				stMeshData.m_mapTextureIndex2HeapIndex.SetAt( i, nTmpIndex );
			}
		}
	}

	return TRUE;
}

const aiNodeAnim* FindNodeAnim(const aiAnimation* pAnimation, const CStringA strNodeName)
{
	for (UINT i = 0; i < pAnimation->mNumChannels; i++)
	{
		if ( CStringA(pAnimation->mChannels[i]->mNodeName.data) == strNodeName)
		{
			return pAnimation->mChannels[i];
		}
	}

	return nullptr;
}

BOOL FindPosition(FLOAT AnimationTime, const aiNodeAnim* pNodeAnim, UINT& nPosIndex)
{
	nPosIndex = 0;
	if (!(pNodeAnim->mNumPositionKeys > 0))
	{
		return FALSE;
	}

	for ( UINT i = 0; i < pNodeAnim->mNumPositionKeys - 1; i++ )
	{
		// 严格判断时间Tick是否在两个关键帧之间
		if ( ( AnimationTime >= (FLOAT)pNodeAnim->mPositionKeys[i].mTime )
			&& ( AnimationTime < (FLOAT)pNodeAnim->mPositionKeys[i + 1].mTime) )
		{
			nPosIndex = i;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL FindRotation(FLOAT AnimationTime, const aiNodeAnim* pNodeAnim, UINT& nRotationIndex)
{
	nRotationIndex = 0;
	if (!(pNodeAnim->mNumRotationKeys > 0))
	{
		return FALSE;
	}

	for (UINT i = 0; i < pNodeAnim->mNumRotationKeys - 1; i++)
	{
		// 严格判断时间Tick是否在两个关键帧之间
		if ( (AnimationTime >= (FLOAT)pNodeAnim->mRotationKeys[i].mTime )
			&& (AnimationTime < (FLOAT)pNodeAnim->mRotationKeys[i + 1].mTime) )
		{
			nRotationIndex = i;
			return TRUE;
		}
	}

	return FALSE;
}

BOOL FindScaling(FLOAT AnimationTime, const aiNodeAnim* pNodeAnim, UINT& nScalingIndex)
{
	nScalingIndex = 0;
	if (!(pNodeAnim->mNumScalingKeys > 0))
	{
		return FALSE;
	}

	for (UINT i = 0; i < pNodeAnim->mNumScalingKeys - 1; i++)
	{
		// 严格判断时间Tick是否在两个关键帧之间
		if ( ( AnimationTime >= (FLOAT)pNodeAnim->mScalingKeys[i].mTime )
			&& ( AnimationTime < (FLOAT)pNodeAnim->mScalingKeys[i + 1].mTime) )
		{
			nScalingIndex = i;
			return TRUE;
		}
	}

	return FALSE;
}

void CalcInterpolatedPosition(XMVECTOR& mxOut, FLOAT AnimationTime, const aiNodeAnim* pNodeAnim)
{
	if (pNodeAnim->mNumPositionKeys == 1)
	{
		VectorEqual(mxOut, pNodeAnim->mPositionKeys[0].mValue);
		return;
	}

	UINT PositionIndex = 0;
	if (! FindPosition(AnimationTime, pNodeAnim, PositionIndex))
	{// 当前时间段内没有位移的变换，默认返回0.0位移
		mxOut = XMVectorSet(0.0f,0.0f,0.0f,0.0f);
		return;
	}

	UINT NextPositionIndex = (PositionIndex + 1);

	ATLASSERT(NextPositionIndex < pNodeAnim->mNumPositionKeys);
	FLOAT DeltaTime = (FLOAT)(pNodeAnim->mPositionKeys[NextPositionIndex].mTime - pNodeAnim->mPositionKeys[PositionIndex].mTime);
	FLOAT Factor = (AnimationTime - (FLOAT)pNodeAnim->mPositionKeys[PositionIndex].mTime) / DeltaTime;
	ATLASSERT(Factor >= 0.0f && Factor <= 1.0f);

	VectorLerp(mxOut
		, pNodeAnim->mPositionKeys[PositionIndex].mValue
		, pNodeAnim->mPositionKeys[NextPositionIndex].mValue
		, Factor);
}

void CalcInterpolatedRotation(XMVECTOR& mxOut, FLOAT AnimationTime, const aiNodeAnim* pNodeAnim)
{
	if (pNodeAnim->mNumRotationKeys == 1)
	{
		QuaternionEqual(mxOut, pNodeAnim->mRotationKeys[0].mValue);
		return;
	}

	UINT RotationIndex = 0;
	if (!FindRotation(AnimationTime, pNodeAnim, RotationIndex))
	{// 当前时间段内没有旋转变换，默认返回0.0旋转
		mxOut = XMVectorSet(0.0f,0.0f,0.0f,0.0f);
		return;
	}

	UINT NextRotationIndex = (RotationIndex + 1);
	ATLASSERT(NextRotationIndex < pNodeAnim->mNumRotationKeys);
	FLOAT DeltaTime = (FLOAT)(pNodeAnim->mRotationKeys[NextRotationIndex].mTime
		- pNodeAnim->mRotationKeys[RotationIndex].mTime);
	FLOAT Factor = (AnimationTime - (FLOAT)pNodeAnim->mRotationKeys[RotationIndex].mTime) / DeltaTime;
	ATLASSERT(Factor >= 0.0f && Factor <= 1.0f);

	QuaternionSlerp(mxOut
		, pNodeAnim->mRotationKeys[RotationIndex].mValue
		, pNodeAnim->mRotationKeys[NextRotationIndex].mValue
		, Factor);

	XMQuaternionNormalize(mxOut);
}

void CalcInterpolatedScaling(XMVECTOR& mxOut, FLOAT AnimationTime, const aiNodeAnim* pNodeAnim)
{
	if ( pNodeAnim->mNumScalingKeys == 1 )
	{
		VectorEqual(mxOut, pNodeAnim->mScalingKeys[0].mValue);
		return;
	}

	UINT ScalingIndex = 0;
	if (!FindScaling(AnimationTime, pNodeAnim, ScalingIndex))
	{// 当前时间帧没有缩放变换，返回 1.0缩放比例
		mxOut = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
		return;
	}

	UINT NextScalingIndex = (ScalingIndex + 1);
	ATLASSERT(NextScalingIndex < pNodeAnim->mNumScalingKeys);
	FLOAT DeltaTime = (FLOAT)(pNodeAnim->mScalingKeys[NextScalingIndex].mTime - pNodeAnim->mScalingKeys[ScalingIndex].mTime);
	FLOAT Factor = (AnimationTime - (FLOAT)pNodeAnim->mScalingKeys[ScalingIndex].mTime) / DeltaTime;
	ATLASSERT(Factor >= 0.0f && Factor <= 1.0f);

	VectorLerp(mxOut
		, pNodeAnim->mScalingKeys[ScalingIndex].mValue
		, pNodeAnim->mScalingKeys[NextScalingIndex].mValue
		, Factor);
}

void ReadNodeHeirarchy(ST_GRS_MESH_DATA& stMeshData
	, const aiAnimation* pAnimation
	, FLOAT AnimationTime
	, const aiNode* pNode
	, const XMMATRIX& mxParentTransform)
{
	
	XMMATRIX mxNodeTransformation = XMMatrixIdentity();
	MXEqual(mxNodeTransformation, pNode->mTransformation);
	mxNodeTransformation = XMMatrixTranspose(mxNodeTransformation);

	XMMATRIX mxThisTrans = XMMatrixTranspose(mxNodeTransformation);
	//mxThisTrans = XMMatrixTranspose(XMMatrixInverse(nullptr,mxNodeTransformation));

	CStringA strNodeName(pNode->mName.data);

	const aiNodeAnim* pNodeAnim = FindNodeAnim(pAnimation, strNodeName);

	if ( pNodeAnim )
	{
		// 缩放
		XMVECTOR vScaling = {};
		CalcInterpolatedScaling(vScaling, AnimationTime, pNodeAnim);
		XMMATRIX mxScaling = XMMatrixScalingFromVector(vScaling);

		// 四元数旋转
		XMVECTOR vRotationQ = {};
		CalcInterpolatedRotation(vRotationQ, AnimationTime, pNodeAnim);
		XMMATRIX mxRotationM = XMMatrixRotationQuaternion(vRotationQ);

		// 位移
		XMVECTOR vTranslation = {};
		CalcInterpolatedPosition(vTranslation, AnimationTime, pNodeAnim);
		XMMATRIX mxTranslationM = XMMatrixTranslationFromVector(vTranslation);

		// 骨骼动画中 最经典的 SQT 组合变换
		mxNodeTransformation = mxScaling * mxRotationM * mxTranslationM; // TranslationM* RotationM* ScalingM;
	}

	XMMATRIX mxGlobalTransformation = mxNodeTransformation *  mxParentTransform;

	UINT nBoneIndex = 0;
	if (stMeshData.m_mapName2Bone.Lookup(strNodeName, nBoneIndex))
	{
		stMeshData.m_arBoneDatas[nBoneIndex].m_mxFinalTransformation
			= stMeshData.m_arBoneDatas[nBoneIndex].m_mxBoneOffset
			/** mxThisTrans*/
			* mxGlobalTransformation 
			* stMeshData.m_mxModel;
	}

	for (UINT i = 0; i < pNode->mNumChildren; i++)
	{
		ReadNodeHeirarchy(stMeshData
			, pAnimation
			, AnimationTime
			, pNode->mChildren[i]
			, mxGlobalTransformation);
	}
}

VOID CalcAnimation(ST_GRS_MESH_DATA& stMeshData, FLOAT fTimeInSeconds, CGRSMatrixArray& arTransforms)
{
	XMMATRIX mxIdentity = XMMatrixIdentity();
	aiNode* pNode = stMeshData.m_paiModel->mRootNode;
	aiAnimation* pAnimation = stMeshData.m_paiModel->mAnimations[stMeshData.m_nCurrentAnimIndex];

	FLOAT TicksPerSecond = (FLOAT)(pAnimation->mTicksPerSecond != 0
		? pAnimation->mTicksPerSecond
		: 25.0f);

	FLOAT TimeInTicks = fTimeInSeconds * TicksPerSecond;
	FLOAT AnimationTime = fmod(TimeInTicks, (FLOAT)pAnimation->mDuration);

	ReadNodeHeirarchy(stMeshData, pAnimation, AnimationTime, pNode, mxIdentity);

	UINT nNumBones = (UINT)stMeshData.m_arBoneDatas.GetCount();

	for (UINT i = 0; i < nNumBones; i++)
	{
		arTransforms.Add(stMeshData.m_arBoneDatas[i].m_mxFinalTransformation);
	}
}