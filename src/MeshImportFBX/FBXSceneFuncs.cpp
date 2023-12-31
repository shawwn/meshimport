#include "UserMemAlloc.h"
#include "ImportFBX.h"

#include <string>

// Refer to US2085 and US2087 in Rally for details on the use of these defines
// for using frame 0 of animation instead of real bind pose when rebuilding
//#define __USE_FRAME_0__ // default to not use first frame

namespace NVSHARE
{

int sortPolygonMaterials( const void* a, const void* b );


// Process recursively each node of the scene. To avoid recomputing 
// uselessly the global positions, the global position of each 
// node is passed to it's children while browsing the node tree.
void MeshImportFBX::ProcessScene(FbxNode* subScene)
{
	FbxNode* pNode = subScene;
	if ( NULL == subScene )
		pNode = m_scene->GetRootNode();

	FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();
    if (lNodeAttribute)
    {
		if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
		{
			if ( pNode->Show.Get() && pNode->Visibility.Get() ) // import in if the Node is set to visible
				AddMeshNode(pNode);
		}
		else if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::eNurbs)
		{
			// Not supported yet. 
			// Should have been converted into a mesh in function ConvertNurbsAndPatch().
			//if ( pNode->Show.Get() && pNode->Visibility.Get() ) // import in if the Node is set to visible
		}
		else if (lNodeAttribute->GetAttributeType() == FbxNodeAttribute::ePatch)
		{
			// Not supported yet. 
			// Should have been converted into a mesh in function ConvertNurbsAndPatch().
			//if ( pNode->Show.Get() && pNode->Visibility.Get() ) // import in if the Node is set to visible
		}
    }

	// recurse
	int i, lCount = pNode->GetChildCount();
	for (i = 0; i < lCount; i++)
	{
	    ProcessScene(pNode->GetChild(i));
	}
}

struct VertexSkinInfo
{
	int mBone[MAX_BONES];
	float mWeight[MAX_BONES];

	VertexSkinInfo()
	{
		for(int i = 0; i < MAX_BONES; i++)
		{
			mBone[i] = 0;
			mWeight[i] = 0;
		}
	}
};

void MeshImportFBX::AddMeshNode(FbxNode* pNode)
{
	FbxVector4* positionValues = NULL;
	FbxLayerElementArrayTemplate<int>* materialIndices = NULL;
	//FbxLayerElementArrayTemplate<FbxVector4>* normalValues = NULL;
	//FbxLayerElementArrayTemplate<int>* normalIndices = NULL;
	FbxLayerElementArrayTemplate<FbxVector4>* tangentValues = NULL;
	FbxLayerElementArrayTemplate<int>* tangentIndices = NULL;
	FbxLayerElementArrayTemplate<FbxVector4>* binormalValues = NULL;
	FbxLayerElementArrayTemplate<int>* binormalIndices = NULL;
	FbxLayerElementArrayTemplate<FbxVector2>* uvValues = NULL;
	//FbxLayerElementArrayTemplate<int>* uvIndices = NULL;


	FbxMesh* lMesh = (FbxMesh*) pNode->GetNodeAttribute();

	const char* meshName = lMesh->GetName();
	const char* nodeName = pNode->GetName();

	if(lMesh->GetLayerCount() == 0)
		return;

	FbxLayer* lLayer0 = lMesh->GetLayer(0);

	int lPolygonCount = lMesh->GetPolygonCount();
	if( lPolygonCount == 0 )
		return;

	int lPolyVertexCount = lMesh->GetPolygonVertexCount();
	if (lPolyVertexCount == 0)
		return;

	int lVertexCount = lMesh->GetControlPointsCount();
	if (lVertexCount == 0)
		return;

	positionValues = lMesh->GetControlPoints();
	lMesh->GetMaterialIndices(&materialIndices);

	int materialIndexCount = materialIndices != NULL ? materialIndices->GetCount() : 1;

	//if(!lMesh->GetNormals(&normalValues))
	//{
	//	lMesh->ComputeVertexNormals();
	//	lMesh->GetNormals(&normalValues);
	//}
	//lMesh->GetNormalsIndices(&normalIndices);
	//FbxLayerElement::EMappingMode normalMapping = lLayer0->GetNormals()->GetMappingMode();

	// PH: tangents and binormals seem not to work, what a pity
	lMesh->GetTangents(&tangentValues);
	lMesh->GetTangentsIndices(&tangentIndices);
	FbxLayerElement::EMappingMode tangentMapping = tangentIndices ? FbxLayerElement::eByPolygonVertex : FbxLayerElement::eByControlPoint;

	lMesh->GetBinormals(&binormalValues);
	lMesh->GetBinormalsIndices(&binormalIndices);	
	FbxLayerElement::EMappingMode binormalMapping = binormalIndices ? FbxLayerElement::eByPolygonVertex : FbxLayerElement::eByControlPoint;

	lMesh->GetTextureUV(&uvValues, FbxLayerElement::eTextureDiffuse);
	//lMesh->GetTextureIndices(&uvIndices, FbxLayerElement::eTextureDiffuse);
	//FbxLayerElement::EMappingMode uvMapping = uvIndices ? FbxLayerElement::eByPolygonVertex : FbxLayerElement::eByControlPoint;
	// PH: This seems to work correctly, unless the eBY_CONBTROL_POINT option
	FbxLayerElement::EMappingMode uvMapping = FbxLayerElement::eByPolygonVertex;

	std::vector<VertexSkinInfo> meshSkinInfo(lVertexCount);

	mVertexFormat = 0;
	mVertexFormat |= NVSHARE::MIVF_POSITION;
	mVertexFormat |= NVSHARE::MIVF_BONE_WEIGHTING;
	//if(normalValues != NULL && normalValues->GetCount() > 0)
	mVertexFormat |= NVSHARE::MIVF_NORMAL;

	if (tangentValues != NULL && tangentValues->GetCount() > 0)
	{
		assert((tangentValues != NULL) == (binormalValues != NULL)); // we need both or none
		mVertexFormat |= NVSHARE::MIVF_TANGENT | NVSHARE::MIVF_BINORMAL;
	}

	if(uvValues != NULL && uvValues->GetCount() > 0)
		mVertexFormat |= NVSHARE::MIVF_TEXEL1;

	FbxAMatrix bindShapeXform;
	FbxAMatrix geometryXform = GetGeometry(pNode);
	FbxAMatrix shapeNodeXform = m_takeInfo ? pNode->EvaluateGlobalTransform(m_takeInfo->mLocalTimeSpan.GetStart()) : pNode->EvaluateGlobalTransform();

	bool isBindShapeSet = false;

	int lSkinCount= lMesh->GetDeformerCount(FbxDeformer::eSkin);
	for(int s = 0; s < lSkinCount; s++)
	{
		FbxSkin* skin = ((FbxSkin *)(lMesh->GetDeformer(0, FbxDeformer::eSkin)));

		int clusterCount = skin->GetClusterCount();
		if(clusterCount == 0)
			continue;

		for (int c=0; c < clusterCount; c++)
		{
			FbxCluster* cluster = skin->GetCluster(c);

			FbxNode* link = cluster->GetLink();
			if (!link)
				continue;

			const char* linkName = link->GetName();

			int  weightCount = cluster->GetControlPointIndicesCount();
			if (weightCount == 0)
				continue;

			int boneIndex = -1;
			for( int b=0; b < meshBones.size() && boneIndex == -1; b++)
			{
				if(meshNodes[b] == link)
					boneIndex = b;
			}
			assert(boneIndex >=0 && boneIndex < meshBones.size());


			if(!isBindShapeSet)
			{
				FbxAMatrix bindShapeBaseXform;
				FbxAMatrix bindLinkWorldMat;
				cluster->GetTransformLinkMatrix( bindLinkWorldMat );

				if ( cluster->GetLinkMode() == FbxCluster::eAdditive && cluster->GetAssociateModel() )
				{
					cluster->GetTransformAssociateModelMatrix(bindShapeBaseXform);
				}
				else
				{
					cluster->GetTransformMatrix( bindShapeBaseXform );
				}

				// multiply by the Geometry local deformation matrix
				// this matrix is Identity when FBX file is exported from Maya
				// but is not Identity when exported from Max, which what is causing
				// the mesh to not skin properly
				bindShapeBaseXform *= GetGeometry( lMesh->GetNode() );

				bindShapeXform = bindShapeBaseXform;//shapeNodeXform * bindShapeBaseXform * geometryXform;

#ifdef __USE_FRAME_0__ // skin to frame 0
				bindShapeXform *= pNode->GetGlobalFromCurrentTake( FbxTime( 0 ) );
#endif // __USE_FRAME_0__

				meshWorldBindShapeXforms[boneIndex] = bindShapeXform;

				isBindShapeSet = true;
			}

			int *weightIndices = cluster->GetControlPointIndices();
			double *weightValues = cluster->GetControlPointWeights();
			for( int w = 0; w < weightCount; w++)
			{
				int wIndex = weightIndices[w];
				double wValue = weightValues[w];

				VertexSkinInfo& vertexSkinInfo = meshSkinInfo[wIndex];

				//Assign the weight to the vertex
				for(int a = 0; a < MAX_BONES; a++ )
				{
					//Test to see if we want to insert here
					if( wValue <= vertexSkinInfo.mWeight[a])
						continue;//nope, try the next assignment

					//shift lower assignments to make room for the new value
					for(int shift = MAX_BONES-1; shift > a; shift-- )
					{
						vertexSkinInfo.mBone[shift]=vertexSkinInfo.mBone[shift-1];
						vertexSkinInfo.mWeight[shift]=vertexSkinInfo.mWeight[shift-1];
					}

					//insert new value
					vertexSkinInfo.mBone[a]=boneIndex;
					vertexSkinInfo.mWeight[a]=wValue;
					break;
				}
			}
		}
	}
	//scale weights to 1.0 total contribution
	for(int v = 0; v < meshSkinInfo.size(); v++ )
	{
		VertexSkinInfo& vertexSkinInfo = meshSkinInfo[v];

		float totalWeight = 0;
		for(int a = 0; a < MAX_BONES; a++ )
			totalWeight += vertexSkinInfo.mWeight[a];

		if ( totalWeight == 0 )
		{
			for(int a = 0; a < MAX_BONES; a++ )
			{
				vertexSkinInfo.mWeight[a] = (a == 0) ? 1 : 0;
			}
		}
		else
		{
			float scaleWeights = 1.0f/totalWeight;
			for(int a = 0; a < MAX_BONES; a++ )
				vertexSkinInfo.mWeight[a] *= scaleWeights;
		}
	}


	std::vector<PolygonMaterial> sortedPolygonMaterials(lPolygonCount);


	assert( materialIndexCount == lPolygonCount || materialIndexCount == 1 );
	if( lPolygonCount < materialIndexCount && materialIndexCount != 1 )
	{
		materialIndexCount = lPolygonCount;
	}

	for( int i = 0; i < lPolygonCount; ++i )
	{
		int idx = i < materialIndexCount ? i : 0;
		sortedPolygonMaterials[i].materialIndex = materialIndices ? materialIndices->GetAt(idx) : -1;
		sortedPolygonMaterials[i].polygonIndex = i;

	}
	qsort( &sortedPolygonMaterials[0], materialIndexCount, sizeof( PolygonMaterial ), sortPolygonMaterials );


	std::vector<int> partition;
	partition.reserve(lPolygonCount+1);
	partition.push_back(0);
	for( int i = 1; i < lPolygonCount; ++i )
	{
		if( !sortedPolygonMaterials[i].submeshesEqual( sortedPolygonMaterials[partition.back()] ) )
		{
			partition.push_back(i);
		}
	}
	partition.push_back(lPolygonCount);

	unsigned submeshCount = (unsigned)partition.size();
	int lNbMat = pNode->GetSrcObjectCount(FbxCriteria::ObjectType(FbxSurfaceMaterial::ClassId));
	FbxString materialNamePrefix = m_fileName + FbxString( "#" );

	std::vector<NVSHARE::MeshVertex> meshVertices(lPolyVertexCount);

	for( unsigned submeshIndex = 0; submeshIndex < submeshCount-1; ++submeshIndex )
	{
		PolygonMaterial* tmat = &sortedPolygonMaterials[partition[submeshIndex]];
		
		FbxSurfaceMaterial* lMaterial = NULL;
		if( tmat->materialIndex >= 0 && tmat->materialIndex < lNbMat )
		{
			lMaterial = FbxCast <FbxSurfaceMaterial>(pNode->GetSrcObject(FbxCriteria::ObjectType(FbxSurfaceMaterial::ClassId), tmat->materialIndex));
		}

		const int polygonNum = partition[submeshIndex+1]-partition[submeshIndex];

		// PH: Why on earth do we need a material prefix?
		//FbxString materialName = lMaterial ? (materialNamePrefix+lMaterial->GetName()) : FbxString("");
		FbxString materialName = lMaterial ? FbxString(lMaterial->GetName()) : FbxString("");
		std::string info;

		if ( lMaterial )
		{
			FbxProperty lProperty = lMaterial->FindProperty( FbxSurfaceMaterial::sDiffuse );
			if( !lProperty.IsValid( ) )
				continue;

			int lNbTex = lProperty.GetSrcObjectCount( FbxCriteria::ObjectType(FbxTexture::ClassId) );
			for (int lTextureIndex = 0; lTextureIndex < lNbTex; ++lTextureIndex )
			{
				FbxTexture * lTexture = FbxCast<FbxTexture>( lProperty.GetSrcObject( FbxCriteria::ObjectType(FbxTexture::ClassId), lTextureIndex ) );
				if ( !lTexture )
					continue;
				FbxString textureFileName = lTexture->GetName( );
				info = "diffuse=" + textureFileName;
				break;
			}
		}

		for (int lSortedPolygonIndex = partition[submeshIndex]; lSortedPolygonIndex < partition[submeshIndex+1]; lSortedPolygonIndex++)
		{
			const int lPolygonIndex = sortedPolygonMaterials[lSortedPolygonIndex].polygonIndex;
			const int lPolyVertexStart = lMesh->GetPolygonVertexIndex(lPolygonIndex);
			int lPolyVertexIndex = lPolyVertexStart;

			int lPolyLocalVertexCount = lMesh->GetPolygonSize(lPolygonIndex);

			for (int lPolyLocalVertexIndex=0; lPolyLocalVertexIndex < lPolyLocalVertexCount; lPolyLocalVertexIndex++, lPolyVertexIndex++)
			{
				const int lVertexIndex = lMesh->GetPolygonVertex(lPolygonIndex, lPolyLocalVertexIndex);

				NVSHARE::MeshVertex& meshVertex = meshVertices[lPolyVertexIndex];
				VertexSkinInfo& vertexSkinInfo = meshSkinInfo[lVertexIndex];
				FbxVector4& sourceVert = positionValues[lVertexIndex];

				FbxVector4 sourceNorm;
				lMesh->GetPolygonVertexNormal(lPolygonIndex, lPolyLocalVertexIndex, sourceNorm);
				sourceNorm[3] = 0;

				FbxVector4 sourceTangent;
				if (tangentValues != NULL)
				{
					if (tangentMapping == FbxLayerElement::eByPolygonVertex)
					{
						int tangentIndex = tangentIndices->GetAt(lPolygonIndex);
						sourceTangent = tangentValues->GetAt(tangentIndex);
					}
					else // FbxLayerElement::eByControlPoint
					{
						sourceTangent = tangentValues->GetAt(lVertexIndex);
					}
				}

				FbxVector4 sourceBinormal;
				if (binormalValues != NULL)
				{
					if (tangentMapping == FbxLayerElement::eByPolygonVertex)
					{
						int binormalIndex = binormalIndices->GetAt(lPolygonIndex);
						sourceBinormal = binormalValues->GetAt(binormalIndex);
					}
					else // FbxLayerElement::eByControlPoint
					{
						sourceBinormal = binormalValues->GetAt(lVertexIndex);
					}
				}

				FbxVector2 sourceUV;
				if(uvValues != NULL)
				{
					if (uvMapping == FbxLayerElement::eByPolygonVertex)
					{
						int uvIndex = lMesh->GetTextureUVIndex(lPolygonIndex, lPolyLocalVertexIndex);
						sourceUV = uvValues->GetAt(uvIndex);
					}
					else // FbxLayerElement::eByControlPoint
					{
						// PH: This does not seem to lead to good results, not sure why, using the other codepath from now on
						sourceUV = uvValues->GetAt(lVertexIndex);
					}
				}

				FbxVector4 accumVert;
				FbxVector4 accumNorm;
				FbxVector4 accumTangent;
				FbxVector4 accumBinormal;
				//#define ACCUMULATE_BIND_SHAPE
#ifdef ACCUMULATE_BIND_SHAPE
				for(int a = 0; a < MAX_BONES; a++ )
				{
					if(vertexSkinInfo.mWeight[a] == 0)
						break;

					FbxAMatrix& bindXform = meshWorldBindShapeXforms[vertexSkinInfo.mBone[a]];
					FbxAMatrix bindXformWithoutTranslation(bindXform);
					bindXformWithoutTranslation.SetT(FbxVector4(0.0f, 0.0f, 0.0f, 1.0f));

					float bindWeight = vertexSkinInfo.mWeight[a];

					FbxVector4 bindVert = bindXform.MultT(sourceVert);
					FbxVector4 bindNorm = bindXformWithoutTranslation.MultT(sourceNorm);
					FbxVector4 bindTangent;
					FbxVector4 bindBinormal;
					if (tangentValues != NULL)
					{
						bindTangent = bindXformWithoutTranslation.MultT(sourceTangent);
						bindBinormal = bindXformWithoutTranslation.MultT(sourceBinormal);
					}

					accumVert += bindVert * bindWeight;
					accumNorm += bindNorm * bindWeight;
					accumTangent += bindTangent * bindWeight;
					accumBinormal += bindBinormal * bindWeight;
				}

#else //!ACCUMULATE_BIND_SHAPE
				// this mesh has no skin/cluster, means just a static mesh
				// we have to set the bindshape transform correctly
				// because it is not set in code above
				if ( false == isBindShapeSet ) // no skin, no cluster
				{
					// in latest version of the sdk, 2011_Beta1
					// there is this GetEvaluator command to use
					// "pNode->GetScene()->GetEvaluator()->GetNodeGlobalTransform(pNode, pTime);"
					// but in the currently in used version, it is not there, so we have to use GetGlobalFromDefaultTake()

					bindShapeXform = lMesh->GetNode()->EvaluateGlobalTransform() * GetGeometry( lMesh->GetNode() ); // get bind pose
				}

				accumVert = bindShapeXform.MultT(sourceVert);

				FbxAMatrix bindShapeXformWithoutTranslation(bindShapeXform);
				bindShapeXformWithoutTranslation.SetT(FbxVector4(0.0f, 0.0f, 0.0f, 1.0f));
				accumNorm = bindShapeXformWithoutTranslation.MultT(sourceNorm);
				if (tangentValues != NULL)
				{
					accumTangent = bindShapeXformWithoutTranslation.MultT(sourceTangent);
					accumBinormal = bindShapeXformWithoutTranslation.MultT(sourceBinormal);
				}
#endif


				meshVertex.mPos[0] = accumVert[0];
				meshVertex.mPos[1] = accumVert[1];
				meshVertex.mPos[2] = accumVert[2];

				accumNorm[3] = 0.0f;
				accumNorm.Normalize();
				meshVertex.mNormal[0] = accumNorm[0];
				meshVertex.mNormal[1] = accumNorm[1];
				meshVertex.mNormal[2] = accumNorm[2];

				if (tangentValues != NULL)
				{
					accumTangent[3] = 0.0f;
					accumTangent.Normalize();
					accumBinormal[3] = 0.0f;
					accumBinormal.Normalize();

					meshVertex.mTangent[0] = accumTangent[0];
					meshVertex.mTangent[1] = accumTangent[1];
					meshVertex.mTangent[2] = accumTangent[2];

					meshVertex.mBiNormal[0] = accumBinormal[0];
					meshVertex.mBiNormal[1] = accumBinormal[1];
					meshVertex.mBiNormal[2] = accumBinormal[2];
				}

				meshVertex.mTexel1[0] = sourceUV[0];
				meshVertex.mTexel1[1] = sourceUV[1];

				meshVertex.mBone[0] = vertexSkinInfo.mBone[0];
				meshVertex.mBone[1] = vertexSkinInfo.mBone[1];
				meshVertex.mBone[2] = vertexSkinInfo.mBone[2];
				meshVertex.mBone[3] = vertexSkinInfo.mBone[3];

				meshVertex.mWeight[0] = vertexSkinInfo.mWeight[0];
				meshVertex.mWeight[1] = vertexSkinInfo.mWeight[1];
				meshVertex.mWeight[2] = vertexSkinInfo.mWeight[2];
				meshVertex.mWeight[3] = vertexSkinInfo.mWeight[3];
			}

			m_callback->importMaterial(materialName,info.c_str());

			for (int lPolyLocalVertexIndex=0; lPolyLocalVertexIndex < lPolyLocalVertexCount-2; lPolyLocalVertexIndex++)
			{
				m_callback->importTriangle(
					nodeName,
					materialName,
					mVertexFormat,
					meshVertices[lPolyVertexStart+0],
					meshVertices[lPolyVertexStart+lPolyLocalVertexIndex+1],
					meshVertices[lPolyVertexStart+lPolyLocalVertexIndex+2]);
			}
		}
	}
}


	

//void MeshImportFBX::AddSkeletonNode(FbxNode* pNode, FbxTime& pTime, FbxAMatrix& pGlobalPosition)
//{
//	FbxSkeleton* lSkeleton = (FbxSkeleton*) pNode->GetNodeAttribute();
//	FbxNode* pParentNode = pNode->GetParent();
//
//	FbxNodeAttribute* testAttr;
//	FbxNodeAttribute::EType testType;
//	if(pParentNode)
//	{
//		testAttr = pParentNode->GetNodeAttribute();
//		if(testAttr)
//			testType = testAttr->GetAttributeType();
//	}
//
//
//	FbxSkeleton::ESkeletonType skelType = lSkeleton->GetSkeletonType();
//    // We're only intersted if this is a limb node and if 
//    // the parent also has an attribute of type skeleton.
//	
//    if (
//		lSkeleton->GetSkeletonType() == FbxSkeleton::eLimbNode &&
//        pParentNode
//       )
//	{
//		
//		const char *pName = pNode->GetName();
//		const char *pParentName = pParentNode->GetName();
//
//		// Root Bone
//		bool isRoot = false;
//		if((!pParentNode->GetNodeAttribute()) || (!pParentNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton))
//		{
//			isRoot = true;
//			
//		}
//
//		const char * attrName;
//		const char * attrNodeName;
//		FbxNode* attrNode;
//		
//		NVSHARE::MeshBone* bone = new NVSHARE::MeshBone();
//
//		BoneRelation *bonePair = new BoneRelation();
//			
//		bonePair->bone = pName;
//		bonePair->parent = pParentName;
//
//		m_boneHierarchy.push_back(bonePair);
//		
//		bone->mName = pName;
//
//
//		fbxDouble3 r = pNode->LclRotation.Get();
//		fbxDouble3 p = pNode->LclTranslation.Get();
//		fbxDouble3 s = pNode->LclScaling.Get();
//	
//		FbxVector4 preRotate = pNode->PreRotation.Get();
//		FbxVector4 rot(r[0],r[1],r[2]);
//		FbxVector4 pos(p[0],p[1],p[2]);
//		FbxVector4 scl(s[0],s[1],s[2]);
//
//
//
//		FbxAMatrix lGlobalPosition(pos, rot + preRotate, scl);
//	
//	    addBoneInfo(pName, bone, lGlobalPosition, isRoot);
//
//	}
//}




void MeshImportFBX::ConvertNurbsAndPatchRecursive(FbxManager* pSdkManager,  FbxScene* pScene, FbxNode* pNode)
{
    FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();
    if (lNodeAttribute)
    {
		FbxNodeAttribute::EType attributeType = lNodeAttribute->GetAttributeType();
		if(attributeType == FbxNodeAttribute::eNurbsSurface)
		{
			FbxGeometryConverter lConverter(pSdkManager);
			lConverter.ConvertNurbsSurfaceToNurbsInPlace(pNode);
		}
	}
	
	lNodeAttribute = pNode->GetNodeAttribute();
	if(lNodeAttribute)
	{
		FbxNodeAttribute::EType attributeType = lNodeAttribute->GetAttributeType();
        if (attributeType == FbxNodeAttribute::eNurbs ||
            attributeType == FbxNodeAttribute::ePatch)
        {
			FbxGeometryConverter lConverter(pSdkManager);
            lConverter.Triangulate(lNodeAttribute, true);
        }
    }

    int i, lCount = pNode->GetChildCount();

    for (i = 0; i < lCount; i++)
    {
        ConvertNurbsAndPatchRecursive(pSdkManager, pScene, pNode->GetChild(i));
    }
}


void MeshImportFBX::ConvertNurbsAndPatch(FbxManager* pSdkManager, FbxScene* pScene)
{
    ConvertNurbsAndPatchRecursive(pSdkManager, pScene, pScene->GetRootNode());
}





/*

void MeshImportFBX::LoadTexture(FbxTexture* pTexture, FbxArray<VSTexture*>& pTextureArray)
{
    // First find if the texture is already loaded
    int i, lCount = pTextureArray.GetCount();

    for (i = 0; i < lCount; i++)
    {
        if (pTextureArray[i]->mRefTexture == pTexture) return;
    }

    // Right now, only Targa textures are loaded by this sample
    FbxString lFileName = pTexture->GetFileName();

    if (lFileName.Right(3).Upper() == "TGA")
    {
        tga_image lTGAImage;

        if (tga_read(&lTGAImage, lFileName.Buffer()) == TGA_NOERR)
        {
            // Make sure the image is left to right
            if (tga_is_right_to_left(&lTGAImage)) tga_flip_horiz(&lTGAImage);

            // Make sure the image is bottom to top
            if (tga_is_top_to_bottom(&lTGAImage)) tga_flip_vert(&lTGAImage);

            // Make the image BGR 24
            tga_convert_depth(&lTGAImage, 24);

            VSTexture* lTexture = new VSTexture;

            lTexture->mW = lTGAImage.width;
            lTexture->mH = lTGAImage.height;
            lTexture->mRefTexture = pTexture;
            lTexture->mImageData  = new unsigned char[lTGAImage.width*lTGAImage.height*lTGAImage.pixel_depth/8];
            memcpy(lTexture->mImageData, lTGAImage.image_data, lTGAImage.width*lTGAImage.height*lTGAImage.pixel_depth/8);

            tga_free_buffers(&lTGAImage);

            pTextureArray.Add(lTexture);
        }
    }
}
*/

/*
void MeshImportFBX::LoadSupportedTexturesRecursive(FbxNode* pNode, FbxArray<VSTexture*>& pTextureArray)
{
    if (pNode)
    {
        int i, lCount;
        FbxNodeAttribute* lNodeAttribute = pNode->GetNodeAttribute();

        if (lNodeAttribute)
        {
            FbxLayerContainer* lLayerContainer = NULL;

            switch (lNodeAttribute->GetAttributeType())
            {
            case FbxNodeAttribute::eNURB:
                lLayerContainer = pNode->GetNurb();
                break;

            case FbxNodeAttribute::ePATCH:
                lLayerContainer = pNode->GetPatch();
                break;

            case FbxNodeAttribute::eMESH:
                lLayerContainer = pNode->GetMesh();
                break;
            }

            if (lLayerContainer){
                int lMaterialIndex;
                int lTextureIndex;
                FbxProperty lProperty;
                int lNbTex;
                FbxTexture* lTexture = NULL;
                FbxSurfaceMaterial *lMaterial = NULL;
                int lNbMat = pNode->GetSrcObjectCount(FbxSurfaceMaterial::ClassId);
                for (lMaterialIndex = 0; lMaterialIndex < lNbMat; lMaterialIndex++){
                    lMaterial = FbxCast <FbxSurfaceMaterial>(pNode->GetSrcObject(FbxSurfaceMaterial::ClassId, lMaterialIndex));
                    if(lMaterial){                                                          
                        lProperty = lMaterial->FindProperty(FbxSurfaceMaterial::sDiffuse);
                        if(lProperty.IsValid()){
                            lNbTex = lProperty.GetSrcObjectCount(FbxTexture::ClassId);
                            for (lTextureIndex = 0; lTextureIndex < lNbTex; lTextureIndex++){
                                lTexture = FbxCast <FbxTexture> (lProperty.GetSrcObject(FbxTexture::ClassId, lTextureIndex));
                                if(lTexture)
                                    LoadTexture(lTexture, pTextureArray);
                            }
                        }
                    }
                }
            } 

        }

        lCount = pNode->GetChildCount();

        for (i = 0; i < lCount; i++)
        {
            LoadSupportedTexturesRecursive(pNode->GetChild(i), pTextureArray);
        }
    }
*/

/*
void MeshImportFBX::LoadSupportedTextures(FbxScene* pScene, FbxArray<VSTexture*>& pTextureArray)
{
    pTextureArray.Clear();

    LoadSupportedTexturesRecursive(pScene->GetRootNode(), pTextureArray);
}*/


inline void MeshImportFBX::GetBindPoseMatrix(FbxCluster* lCluster, FbxAMatrix& bindPose)
{
	 // Get the link initial global position and the link current global position.
     lCluster->GetTransformLinkMatrix(bindPose);
}

inline void MeshImportFBX::GetBindShapeMatrix( FbxCluster* lCluster, FbxAMatrix& bindShape)
{   
			lCluster->GetTransformMatrix(bindShape);       
}


// Deform the vertex array with the links contained in the mesh.
void MeshImportFBX::ApplyVertexTransform(FbxMesh* pMesh, NVSHARE::MeshVertex* pVertexArray)
{
	
	int lVertexCount = pMesh->GetControlPointsCount();

	FbxAMatrix lVertexTransformMatrix;

	int* lBoneCount = new int[lVertexCount];
	memset(lBoneCount, 0, lVertexCount * sizeof(int));

    double* lBoneWeight = new double[lVertexCount];
    memset(lBoneWeight, 0, lVertexCount * sizeof(double));

    int lSkinCount=pMesh->GetDeformerCount(FbxDeformer::eSkin);
    int clusterCount = ((FbxSkin *)(pMesh->GetDeformer(0, FbxDeformer::eSkin)))->GetClusterCount();

	if(clusterCount == 0)
		return;


	for(int i = 0; i < lSkinCount; i ++)
	{
		for (int j=0; j< clusterCount; ++j)
		{
			

			FbxCluster* lCluster =((FbxSkin *) pMesh->GetDeformer(i, FbxDeformer::eSkin))->GetCluster(j);
			if (!lCluster->GetLink())
				continue;

				
			GetBindShapeMatrix(lCluster, lVertexTransformMatrix);

			char* clusterName = const_cast<char *>(lCluster->GetName());

			ClusterBoneMap* clusterMap = new ClusterBoneMap();
			clusterMap->clusterID = j;
			clusterMap->pMesh = pMesh;
			clusterMap->clusterName = clusterName;

			AddClusterInfo(clusterMap);

			 int k;
			 int lVertexIndexCount = lCluster->GetControlPointIndicesCount();

			 for (k = 0; k < lVertexIndexCount; ++k) 
			 {            
				 int lIndex = lCluster->GetControlPointIndices()[k];
				 double lWeight = lCluster->GetControlPointWeights()[k];

				 int count = lBoneCount[lIndex];

				 float* lSrcVertex = &pVertexArray[lIndex].mPos[0];
				 FbxVector4 srcVec((double)lSrcVertex[0],(double)lSrcVertex[1],(double)lSrcVertex[2]);
				 FbxVector4 dstVec;

				 dstVec = lVertexTransformMatrix.MultT(srcVec);
	           
				 pVertexArray[lIndex].mPos[0] = (float)dstVec.mData[0];
				 pVertexArray[lIndex].mPos[1] = (float)dstVec.mData[1];
				 pVertexArray[lIndex].mPos[2] = (float)dstVec.mData[2];

				 if((lWeight == 0.0) || (count >= MAX_BONES))
					  continue;

				  pVertexArray[lIndex].mBone[count] = j;
				  pVertexArray[lIndex].mWeight[count] = (float)lWeight;

				  lBoneCount[lIndex] += 1;

			 }
		}

		

	}



	delete [] lBoneWeight;
	delete [] lBoneCount;
}


// Scale all the elements of a matrix.
inline void MeshImportFBX::MatrixScale(FbxAMatrix& pMatrix, double pValue)
{
    int i,j;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            pMatrix[i][j] *= pValue;
        }
    }
}

// Add a value to all the elements in the diagonal of the matrix.
inline void MeshImportFBX::MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue)
{
    pMatrix[0][0] += pValue;
    pMatrix[1][1] += pValue;
    pMatrix[2][2] += pValue;
    pMatrix[3][3] += pValue;
}

// Sum two matrices element by element.
inline void MeshImportFBX::MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix)
{
    int i,j;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            pDstMatrix[i][j] += pSrcMatrix[i][j];
        }
    }
}

  

bool MeshImportFBX::ImportAnimation()
{
	FbxArray<FbxString*> takeNames;
	m_scene->FillAnimStackNameArray(takeNames);
	int takeCount = takeNames.GetCount();
	if(takeCount == 0)
		return true;

	m_takeName = NULL;
	m_takeInfo = NULL;
	for(int t = 0; t < takeCount && m_takeInfo == NULL; t++ )
	{
		m_takeName = takeNames[t];
		m_scene->SetCurrentAnimationStack( m_scene->FindMember<FbxAnimStack>(m_takeName->Buffer()) );
		m_takeInfo =  m_scene->GetTakeInfo( *m_takeName );
	}

	FbxTime start = 0;
	FbxTime stop = 0;

    if (m_takeInfo)
    {
        start = m_takeInfo->mLocalTimeSpan.GetStart();
        stop = m_takeInfo->mLocalTimeSpan.GetStop();
    }
    
	if ( start < 0 || stop < 0 ) // local data time is not reliable
    {
		// get global time
		FbxTimeSpan lTimeLine;
		m_scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLine);

        start = lTimeLine.GetStart();
        stop  = lTimeLine.GetStop();
    }

	FbxTimeSpan span(start, stop);
	float dTime = 0.02f;// Magic timestep from clothing tool NX_TIME_STEP

	double dDurationSecond = span.GetDuration().GetSecondDouble();
	if (dDurationSecond > 7200.0f) // more than two hours
	{
		char message[OUTPUT_TEXT_BUFFER_SIZE+1] = "";
		message[OUTPUT_TEXT_BUFFER_SIZE] = '\0';
		sprintf_s(message, OUTPUT_TEXT_BUFFER_SIZE, "Time span of animation take %s is too long.", m_takeName->Buffer());
		outputMessage( message );
		return false;
	}
	int numFrames = (int)(dDurationSecond / dTime + 1);
	float duration = (float)(numFrames - 1)* dTime;

	meshWorldAnimXforms.clear();
	meshWorldAnimXforms.resize(meshBones.size());
	meshTrackPtrs.clear();
	meshTrackPtrs.resize(meshBones.size() );
	meshTracks.clear();
	meshTracks.resize(meshBones.size() );
	meshPoses.clear();
	meshPoses.resize(meshBones.size() * numFrames);

	meshAnimation.mName = m_takeName->Buffer();
	meshAnimation.mTracks = &meshTrackPtrs[0];
	meshAnimation.mTrackCount = (int)meshBones.size();
	meshAnimation.mFrameCount = numFrames;
	meshAnimation.mDuration = duration;
	meshAnimation.mDtime = dTime * 200.0f;//compensate for fricken magic number in SkeletalAnim.cpp

	for(int b=0;b<meshBones.size();b++)
	{
		FbxNode* node = meshNodes[b];
		const NVSHARE::MeshBone& bone = meshBones[b];
		NVSHARE::MeshAnimTrack& track = meshTracks[b];
		meshTrackPtrs[b] = &track;

		track.SetName(bone.GetName());
		track.mPose = &meshPoses[numFrames*b];
		track.mFrameCount = numFrames;
		track.mDuration = duration;
		track.mDtime = dTime * 200.0f;//compensate for fricken magic number in SkeletalAnim.cpp
	}

	FbxTime keyTime = start;
	FbxTime stepTime;
	stepTime.SetSecondDouble(dTime);

	for(int f=0;f<numFrames; f++, keyTime+=stepTime)
	{
		//Get Globals
		for(int b=0;b<meshBones.size();b++)
		{
			FbxNode* node = meshNodes[b];
			meshWorldAnimXforms[b] = node->EvaluateGlobalTransform( keyTime );
		}

		//Convert to locals
		for(int b=0;b<meshBones.size();b++)
		{
			const NVSHARE::MeshBone& bone = meshBones[b];
			NVSHARE::MeshAnimTrack& track = meshTracks[b];
		
			FbxAMatrix localAnimXform;
			FbxNode* node = m_scene->GetNode( b );
			if(bone.mParentIndex == -1)
			{
				localAnimXform = meshWorldAnimXforms[b];
				//localAnimXform = node->GetGlobalFromCurrentTake( FbxTime( 0 ) ).Inverse() * meshWorldAnimXforms[b];
			}
			else
			{
				localAnimXform = meshWorldAnimXforms[bone.mParentIndex].Inverse() * meshWorldAnimXforms[b];
				//localAnimXform = (m_scene->GetNode(bone.mParentIndex)->GetGlobalFromCurrentTake( FbxTime( 0 ) ).Inverse() *
				//				  node->GetGlobalFromCurrentTake( FbxTime( 0 ) ) )*
				//				  meshWorldAnimXforms[bone.mParentIndex].Inverse() * meshWorldAnimXforms[b]; 
			}
			//localAnimXform.SetIdentity();

			float localAnimXformF[16];
			for(int x = 0; x < 16; x++ )
			{
				localAnimXformF[x] = ((double*)localAnimXform)[x];
			}

			float t[3];
			float r[4];
			float s[3];
			fmi_decomposeTransform(localAnimXformF, t, r, s);

			track.SetPose(f, t, r, s);
		}
	}

	m_callback->importAnimation(meshAnimation);
	return true;
}


bool MeshImportFBX::importSkeletonRecursive(
	FbxNode* node,
	int parentBone, 
	int& boneAllocator)
{
	bool result = true;

	// PH: unfortunately we must add all nodes, not just skeleton bones. Some other nodes can contain
	//     keyframed animation and objects might be bound to them as well...
	int boneIndex = boneAllocator;
	boneAllocator++;

	//reserved so this is efficient
	meshBones.resize(boneIndex+1);
	meshNodes.resize(boneIndex+1);

	meshNodes[boneIndex] = node;

	const char* nodeName = meshStrings.Get(node->GetName()).Get();
	NVSHARE::MeshBone& milBone = meshBones[boneIndex];
	milBone.SetName(nodeName);
	milBone.mParentIndex = parentBone;

	int childCount = node->GetChildCount();
	for(int n = 0; n < childCount; n++)
	{
		FbxNode* child = node->GetChild(n);

		result = result && importSkeletonRecursive(child, boneIndex, boneAllocator);
	}

	return result;
}

void MeshImportFBX::ImportSkeleton()
{
	bool result = true;

	meshSkeleton.SetName("skeleton");

	// PH: We must clear these arrays to get them re-initialized. Otherwise we will have side effects from previous calls to Import!!
	meshNodes.clear();
	meshNodes.reserve(m_scene->GetNodeCount());
	meshBones.clear();
	meshBones.reserve(m_scene->GetNodeCount());

	int boneAllocator = 0;
	result = importSkeletonRecursive(m_scene->GetRootNode(), -1, boneAllocator);

	meshSkeleton.SetBones((int)meshBones.size(), &meshBones[0]);
	meshWorldBindPoseXforms.clear();
	meshWorldBindPoseXforms.resize(meshBones.size());
	meshWorldBindShapeXforms.clear();
	meshWorldBindShapeXforms.resize(meshBones.size());

#if 0
	// PH: We need to initialize them with the correct scale at least.
	// PH: Doesn't work at all :( since it does not just depend on scale, I need full bind pose for all bones!
	for( int n = 0; n < meshBones.size(); n++)
	{
		FbxAMatrix defaultTake = meshNodes[n]->GetGlobalFromDefaultTake();
		defaultTake.SetT(FbxVector4(0.0f, 0.0f, 0.0f, 1.0f));
		FbxVector4 test(1.0f, 0.0f, 0.0f, 0.0f); // we better have uniform scaling!
		test = defaultTake.MultT(test);
		meshWorldBindPoseXforms[n].SetIdentity();
		meshWorldBindPoseXforms[n] *= test.Length();
	}
#endif

#if 0
	// PH: initially fill them with the default pose (maybe similar to bind pose)
	// PH: unfortunately this is not working: It will put the skeleton into the first frame of the animation, not the t-pose. Then of
	//     course the animation is completely broken
	for( int n = 0; n < meshBones.size(); n++)
	{
		meshWorldBindPoseXforms[n] = meshNodes[n]->GetGlobalFromDefaultTake(FbxNode::eSOURCE_SET);
		//meshWorldBindPoseXforms[n] = meshNodes[n]->GetGlobalFromDefaultTake(FbxNode::eDESTINATION_SET);
	}
#endif

#if 1
	std::vector<bool> boneInitialized(meshBones.size());
	std::vector<bool> boneImportant(meshBones.size());

	// PH: This still seems to be leading to the best results
	//     But some bones don't get a bind pose if they are not referenced by the mesh.
	int numNode = m_scene->GetNodeCount();
	for( int n = 0; n < numNode; n++)
	{
		FbxNode* node = m_scene->GetNode(n);
	    FbxNodeAttribute* nodeAttribute = node->GetNodeAttribute();
		if (!nodeAttribute)
			continue;

		if (nodeAttribute->GetAttributeType() != FbxNodeAttribute::eMesh)
			continue;
    
		FbxMesh* mesh = (FbxMesh*) nodeAttribute;
	
		int lSkinCount=mesh->GetDeformerCount(FbxDeformer::eSkin);
		if(lSkinCount == 0)
			continue;

		for(int s = 0; s < lSkinCount; s++)
		{
			FbxSkin* skin = ((FbxSkin *)(mesh->GetDeformer(0, FbxDeformer::eSkin)));

			int clusterCount = skin->GetClusterCount();
			if(clusterCount == 0)
				continue;

			for (int c=0; c < clusterCount; c++)
			{
				FbxCluster* cluster = skin->GetCluster(c);

				FbxNode* link = cluster->GetLink();
				if (!link)
					continue;

				const char* linkName = link->GetName();

				bool boneFound = false;
				int b;
				for(b = 0; b < meshBones.size(); b++)
				{
					if(link == meshNodes[b])//0==strcmp(linkName, meshBones[b].GetName()))
					{
						boneFound = true;
						break;
					}
				}
				if(!boneFound)
					continue;

				// how to handle bone bound to > 1 skin?
				// this fixes the batman model from being displayed in a garbled state
				// this is due to the fact that there are bones which are attached to more one skin which.
				// In the batman case, the order in which the skin is processed when importing makes this fix works
				// because the offending bones got their "correct" transformation first, before being overwritten by a second "wrong" transformation
				// so this fix just skips a bone if it has been initialized.
				// this might break other models if the order of the skin being process is different
				if ( boneInitialized[b] == true )
				{
					//OutputDebugString( "test" );
					continue;
				}

				cluster->GetTransformLinkMatrix(meshWorldBindPoseXforms[b]);

#ifdef __USE_FRAME_0__ // make frame 0 are the bind pose
				meshWorldBindPoseXforms[b] *= node->GetGlobalFromCurrentTake( FbxTime( 0 ) );
#endif // __USE_FRAME_0__

				boneInitialized[b] = true;
				boneImportant[b] = true;
				//DH:  Removed because we need to update it later anyways if it is bound to two different meshes
				//cluster->GetTransformMatrix(meshWorldBindShapeXforms[b]);
			}
		}
	}
#endif

#if 0
	// PH: this will generate the list of bones that should be initialized
	//     put a breakpoint where the comment says 'parent should be initialized' and dump the list of 'parent's
	for (int b = 0; b < meshBones.size(); b++)
	{
		if (!boneImportant[b])
			continue;

		int parent = meshBones[b].mParentIndex;
		while (parent > 0)
		{
			if (!boneInitialized[parent])
			{
				// parent should be initialized!
				boneInitialized[parent] = true;
			}
			parent = meshBones[parent].mParentIndex;
		}
	}
#endif

	for(int b = 0; b < meshBones.size(); b++)
	{
		NVSHARE::MeshBone& bone = meshBones[b];
		FbxAMatrix& worldBindPose = meshWorldBindPoseXforms[b];
		FbxAMatrix localBindPose;

		FbxNode* node = m_scene->GetNode(b);

		if(bone.mParentIndex == -1)
		{
			localBindPose = worldBindPose;
			//localBindPose *= node->GetGlobalFromCurrentTake( FbxTime( 0 ) );
		}
		else
		{
			FbxAMatrix& parentWorldBindPose = meshWorldBindPoseXforms[bone.mParentIndex];

			localBindPose = parentWorldBindPose.Inverse() * worldBindPose;
			//localBindPose *= (m_scene->GetNode(bone.mParentIndex)->GetGlobalFromCurrentTake( FbxTime( 0 ) ).Inverse() * node->GetGlobalFromCurrentTake( FbxTime( 0 ) ) );
		}

		float localBindPoseF[16];
		for(int x = 0; x < 16; x++ )
		{
			//localBindPoseF[x] = ((double*)localBindPose)[x];
			// PH: This looks somewhat nicer...
			localBindPoseF[x] = localBindPose.Get(x / 4, x % 4);
		}

		float t[3];
		float r[4];
		float s[3];
		fmi_decomposeTransform(localBindPoseF, t, r, s);

		bone.mPosition[0] = t[0];
		bone.mPosition[1] = t[1];
		bone.mPosition[2] = t[2];
		bone.mOrientation[0] = r[0];
		bone.mOrientation[1] = r[1];
		bone.mOrientation[2] = r[2];
		bone.mOrientation[3] = r[3];
		bone.mScale[0] = s[0];
		bone.mScale[1] = s[1];
		bone.mScale[2] = s[2];
	}

	m_callback->importSkeleton(meshSkeleton);
}

int sortPolygonMaterials( const void* a, const void* b )
{
	const PolygonMaterial& tmA = *(const PolygonMaterial*)a;
	const PolygonMaterial& tmB = *(const PolygonMaterial*)b;

	int diff = tmA.materialIndex - tmB.materialIndex;
	if( diff )
	{
		return diff;
	}

	return tmA.polygonIndex - tmB.polygonIndex;
}

// Get the geometry deformation local to a node. It is never inherited by the
// children.
FbxAMatrix MeshImportFBX::GetGeometry(FbxNode* pNode) {
    FbxVector4 lT, lR, lS;
    FbxAMatrix lGeometry;

    lT = pNode->GetGeometricTranslation(FbxNode::eSourcePivot);
    lR = pNode->GetGeometricRotation(FbxNode::eSourcePivot);
    lS = pNode->GetGeometricScaling(FbxNode::eSourcePivot);

    lGeometry.SetT(lT);
    lGeometry.SetR(lR);
    lGeometry.SetS(lS);

    return lGeometry;
}



// Get the global position.
// Do not take in account the geometric transform.
FbxAMatrix MeshImportFBX::GetGlobalPosition(FbxNode* pNode, FbxTime& pTime, FbxAMatrix& pParentGlobalPosition)
{
    // Ideally this function would use parent global position and local position to
    // compute the global position.
    // Unfortunately the equation 
    //    lGlobalPosition = pParentGlobalPosition * lLocalPosition
    // does not hold when inheritance type is other than "Parent" (RSrs). To compute
    // the parent rotation and scaling is tricky in the RrSs and Rrs cases.
    // This is why GetGlobalFromCurrentTake() is used: it always computes the right
    // global position.

    return pNode->EvaluateGlobalTransform(pTime);
}


// Get the global position of the node for the current pose.
// If the specified node is not part of the pose, get its
// global position at the current time.
FbxAMatrix MeshImportFBX::GetGlobalPosition(FbxNode* pNode, FbxTime& pTime, FbxPose* pPose, FbxAMatrix* pParentGlobalPosition)
{
    FbxAMatrix lGlobalPosition;
    bool        lPositionFound = false;

    if (pPose)
    {
        int lNodeIndex = pPose->Find(pNode);

        if (lNodeIndex > -1)
        {
            // The bind pose is always a global matrix.
            // If we have a rest pose, we need to check if it is
            // stored in global or local space.
            if (pPose->IsBindPose() || !pPose->IsLocalMatrix(lNodeIndex))
            {
                lGlobalPosition = GetPoseMatrix(pPose, lNodeIndex);
            }
            else
            {
                // We have a local matrix, we need to convert it to
                // a global space matrix.
                FbxAMatrix lParentGlobalPosition;

                if (pParentGlobalPosition)
                {
                    lParentGlobalPosition = *pParentGlobalPosition;
                }
                else
                {
                    if (pNode->GetParent())
                    {
                        lParentGlobalPosition = GetGlobalPosition(pNode->GetParent(), pTime, pPose);
                    }
                }

                FbxAMatrix lLocalPosition = GetPoseMatrix(pPose, lNodeIndex);
                lGlobalPosition = lParentGlobalPosition * lLocalPosition;
            }

            lPositionFound = true;
        }
    }

    if (!lPositionFound)
    {
        // There is no pose entry for that node, get the current global position instead
        lGlobalPosition = GetGlobalPosition(pNode, pTime, *pParentGlobalPosition);
    }

    return lGlobalPosition;
}

// Get the matrix of the given pose
FbxAMatrix MeshImportFBX::GetPoseMatrix(FbxPose* pPose, int pNodeIndex)
{
    FbxAMatrix lPoseMatrix;
    FbxMatrix lMatrix = pPose->GetMatrix(pNodeIndex);

    memcpy((double*)lPoseMatrix, (double*)lMatrix, sizeof(lMatrix.mData));

    return lPoseMatrix;
}

}; // end of namespace
