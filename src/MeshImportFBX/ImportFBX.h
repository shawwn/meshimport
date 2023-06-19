#ifndef IMPORT_FBX_H
#define IMPORT_FBX_H

#pragma warning(disable:4265)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <fbxsdk.h>
#pragma clang diagnostic pop
#include <map>
#include <vector>
#include "UserMemAlloc.h"

#include "FBXReader.h"
#include "Texture.h"
#include "MeshImport.h"
#include "stringdict.h"
#include "fmi_math.h"

using namespace NVSHARE;

#pragma warning(disable: 4565)

#ifdef WIN32
#ifdef MESHIMPORTFBX_EXPORTS
#define FBX_DLL_API extern "C" __declspec(dllexport)
#else
#define FBX_DLL_API extern "C" __declspec(dllimport)
#endif
#else
#define FBX_DLL_API
#endif

namespace NVSHARE
{

#define MAX_BONES	4

#define DTIME_INTERVAL (1/30.0f)

struct PolygonMaterial
{
      	int materialIndex;
 //    	int diffuseIndex;
//	    int normalIndex;
	    int polygonIndex;

	    bool submeshesEqual( const PolygonMaterial& t )
	    {
		    return	materialIndex == t.materialIndex
//				&& diffuseIndex == t.diffuseIndex
//				&& normalIndex == t.normalIndex
            ;
	    }
};

// PH: Always use the same version as in the header. Everything else is just a pain
#define MESHIMPORTFBX_VERSION MESHIMPORT_VERSION  // version 0.01  increase this version number whenever an interface change occurs.



struct BoneRelation
{
	const char* bone;
	const char* parent;
};

struct ClusterBoneMap
{
public:
	ClusterBoneMap(): clusterID(-1),
		              meshBone(NULL),
					  pMesh(NULL),
					  globalPosition(),
					  boneDataInitialized(false),
					  clusterInfoInitialized(false),
					  clusterName(""),
					  isRoot(false),
					  boneName(""){}

	int clusterID;
	const char* clusterName;
	const char* boneName;
	FbxAMatrix bindPose;
	NVSHARE::MeshBone* meshBone;
	FbxMesh *pMesh;
	FbxAMatrix globalPosition;
	bool boneDataInitialized;
	bool clusterInfoInitialized;
	bool isRoot;

	bool IsIntialized() { return (boneDataInitialized && clusterInfoInitialized);}

};


class MeshImportFBX: public NVSHARE::MeshImporter, public Memalloc
{

public:
	MeshImportFBX();
	virtual ~MeshImportFBX();

	virtual const char * getExtension(int index);
	virtual const char * getDescription(int index);
	virtual bool importMesh(const char *fname,
		                    const void *data,unsigned int dlen,
							NVSHARE::MeshImportInterface *callback,
							const char *options,
							NVSHARE::MeshImportApplicationResource *appResource);


	void ProcessScene(FbxNode *subScene = NULL);

	void ImportMesh();


	void ImportSkeleton();
	bool importSkeletonRecursive(FbxNode* node, int parentBone, int& boneAllocator);
	bool ImportAnimation();

	//void AddSkeletonNode(FbxNode* pNode, FbxTime& pTime, FbxAMatrix& pParentGlobalPosition);
    void AddMeshNode(FbxNode* pNode);
	

	int	mNumBones;

    // Utility

	FbxAMatrix GetGlobalPosition(FbxNode* pNode, FbxTime& pTime, FbxPose* pPose, FbxAMatrix* pParentGlobalPosition = 0);
	FbxAMatrix GetGlobalPosition(FbxNode* pNode, FbxTime& pTime, FbxAMatrix& pParentGlobalPosition );
	FbxAMatrix GetPoseMatrix(FbxPose* pPose, int pNodeIndex);
	FbxAMatrix GetGeometry(FbxNode* pNode);
	
	inline void MatrixScale(FbxAMatrix& pMatrix, double pValue);
	inline void MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue);
	inline void MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix);
	
	const char* getFileName( const char* fullPath);

	bool Import( const char* filename, NVSHARE::MeshImportInterface* callBack );

	void outputMessage( const char* message );


	StringDict									meshStrings;
	NVSHARE::MeshSkeleton					meshSkeleton;
	std::vector<NVSHARE::MeshBone>			meshBones;
	std::vector<FbxNode*>						meshNodes;
	std::vector<FbxAMatrix>					meshWorldBindPoseXforms;
	std::vector<FbxAMatrix>					meshWorldBindShapeXforms;
	std::vector<FbxAMatrix>					meshWorldAnimXforms;
	NVSHARE::MeshAnimation					meshAnimation;
	std::vector<NVSHARE::MeshAnimTrack*>		meshTrackPtrs;
	std::vector<NVSHARE::MeshAnimTrack>		meshTracks;
	std::vector<NVSHARE::MeshAnimPose>		meshPoses;


	inline void GetBindPoseMatrix(FbxCluster *lCluster, FbxAMatrix& bindPose);
	inline void GetBindShapeMatrix( FbxCluster *lCluster, FbxAMatrix& bindShape);
	void ApplyVertexTransform(FbxMesh* pMesh, NVSHARE::MeshVertex *pVerts);
	
	void ConvertNurbsAndPatch(FbxManager* pSdk, FbxScene* pScene);

    void ConvertNurbsAndPatchRecursive(FbxManager* pSdk,
		                                      FbxScene* pScene, FbxNode* pNode);


    //void LoadTexture(FbxTexture* pTexture, FbxArray<VSTexture*>& pTextureArray);

	//void LoadSupportedTexturesRecursive(FbxNode *pNode, FbxArray<VSTexture*>& pTextureArray);

	//void LoadSupportedTextures(FbxScene *pScene, FbxArray<VSTexture*>& pTextureArray);

	bool getClusterByIndex(int j, ClusterBoneMap** pose)
	{
		
		bool ret = false;

		std::map<const char * , ClusterBoneMap*>::iterator it = m_boneClusterMap.begin();

		while(it != m_boneClusterMap.end())
		{
			ClusterBoneMap* tempPose = it->second;
			
			if(tempPose->clusterID == j)
			{
				*pose = tempPose;
				ret = true;
				break;
			}

			it++;
		}

		return ret;

	}


	bool getCluster(const char * name, ClusterBoneMap** cluster)
	{
		bool ret = false;

		std::map<const char *, ClusterBoneMap*>::iterator it = m_boneClusterMap.begin();

		while(it != m_boneClusterMap.end())
		{
			ClusterBoneMap* pose = it->second;
			if(0 == strcmp(pose->clusterName,name))
			{
				*cluster = pose;
				ret = true;
				break;
			}
			
			it++;
		}
		return ret;

	}
	bool getClusterByBone(const char* name, ClusterBoneMap** cluster)
	{
		bool ret = false;


		std::map<const char * , ClusterBoneMap*>::iterator it = m_boneClusterMap.begin();
			

		char clusterName[128];
		strcpy(clusterName, "Cluster ");
		strcat(clusterName, name);
		strcat(clusterName, "\0");

		while(it != m_boneClusterMap.end())
		{
			ClusterBoneMap* pose  = it->second;
			//@@DW Bad string termination???
			if(0 == strcmp(clusterName,it->first))
			{
				*cluster = pose;
				ret = true;
				break;

			}
			
			it++;
		}
		//find doesnt work??
		/*if((it = m_boneClusterMap.find(name)) != m_boneClusterMap.end())
		{
			cluster = it->second;
			ret = true;
		}*/

		
		return ret;

	}
	bool getBoneBindPose(const char *name, FbxAMatrix& bindPose)
	{
		bool ret = false;

	

		ClusterBoneMap* bone;

		if(getClusterByBone(name, &bone))
		{
			bindPose = bone->bindPose;
			ret = true;
		}

		return ret;
	}

	void AddClusterInfo(ClusterBoneMap* pCluster)
	{
		ClusterBoneMap *cluster;
		if(getCluster(pCluster->clusterName, &cluster))
		{
			cluster->clusterID = pCluster->clusterID;
			cluster->pMesh =  pCluster->pMesh;
			cluster->clusterName = pCluster->clusterName;
			cluster->clusterInfoInitialized = true;
			delete pCluster;

		} else
		{
			m_boneClusterMap.insert(m_boneClusterMap.begin(), std::pair<const char *, ClusterBoneMap*>(pCluster->clusterName, pCluster));
			pCluster->clusterInfoInitialized = true;
		}

		
	}

	int getNumBones()
	{
		return (int)m_boneClusterMap.size();
	}
	 bool getBone(const char *name, NVSHARE::MeshBone& bone)
	{
		bool ret = false;

		char clusterName[128];
		strcpy(clusterName, "Cluster ");
		strcat(clusterName, name);
		strcat(clusterName, "\0");

		std::map<const char *, ClusterBoneMap*>::iterator it;

		if((it = m_boneClusterMap.find(clusterName)) != m_boneClusterMap.end())
		{
			ClusterBoneMap* cluster = it->second;

			bone = *cluster->meshBone;
			ret = true;

		}

		return ret;

	}

	void updateBone(const char* name, NVSHARE::MeshBone *newBone, FbxAMatrix& pGlobalPosition)
	{
		ClusterBoneMap* pose;

		char clusterName[128];
		strcpy(clusterName, "Cluster ");
		strcat(clusterName, name);
		strcat(clusterName, "\0");

		if(getClusterByBone(clusterName, &pose))
		{
			if(pose->meshBone && (newBone != pose->meshBone))
				delete pose->meshBone;

			pose->meshBone = newBone;
			pose->globalPosition = pGlobalPosition;
		    

		}

	}
	
	//void addBoneInfo(const char *name, NVSHARE::MeshBone* newBone, FbxAMatrix& globalPos, bool isRoot)
	//{
	//	
	//	
	//	mVertexFormat |= MIVF_BONE_WEIGHTING;

	//	ClusterBoneMap* curPose;

	//	//@@DW Tidy this
	//	char clusterName[128];
	//	strcpy(clusterName, "Cluster ");
	//	strcat(clusterName, name);
	//	strcat(clusterName, "\0");

	//	char *newName = (char *)malloc(strlen(clusterName) + 1 * sizeof(char));

	//	strcpy(newName, clusterName);

	//	if(!getCluster(clusterName, &curPose))
	//	{
	//		curPose = new ClusterBoneMap();
	//		m_boneClusterMap.insert(m_boneClusterMap.begin(), pair<const char*, ClusterBoneMap*>(newName, curPose));
	//	} 
	//	
	//	
	//	curPose->meshBone = newBone;
	//	curPose->globalPosition = globalPos;
	//	curPose->boneName = name;
	//	curPose->boneDataInitialized = true;
	//	curPose->isRoot = isRoot;

	//	
	//}

	int mBoneCount;
	int mVertexCount;
	long mVertexFormat;
	
	
	std::map<const char * , ClusterBoneMap* >m_boneClusterMap; // Map to cluster ID
	std::vector<BoneRelation*>  m_boneHierarchy;

protected:

	NVSHARE::MeshImportInterface *m_callback;

	void Release();

	


	 
private:


	
	FbxString						m_fileName;
	FbxString						m_filePath;

	

	FbxManager*				m_sdkManager;
	FbxImporter*				m_importer;
	FbxScene*					m_scene;
	FbxTakeInfo*				m_takeInfo;
	FbxString*					m_takeName;

	FbxArray<FbxNode*>	m_cameraArray;
	FbxArray<FbxString*>	m_takeNameArray;
	FbxArray<FbxPose*>	m_poseArray;
	FbxArray<VSTexture*>	m_textureArray;
    FbxArray<FbxSurfaceMaterial*> m_MaterialArray;


	FbxTime						m_period;

	uint32_t			m_vertexCount;
	uint32_t			m_vertexUVCount;
	uint32_t			m_triangleCount;
	uint32_t			m_meshDataTypes;

};


//MeshImporter * createMeshImportFBX(void);
void           releaseMeshImportFBX(NVSHARE::MeshImporter *iface);


}; // end of namespace

#endif
