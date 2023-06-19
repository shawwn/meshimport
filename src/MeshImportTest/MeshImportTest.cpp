#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef WIN32
#include <direct.h>
#include <windows.h>
#endif

#include "MeshImport.h"


static const char *         lastSlash(const char *src) // last forward or backward slash character, null if none found.
{
	const char *ret = 0;

	const char *dot = strchr(src,'\\');
	if  ( dot == 0 )
		dot = strchr(src,'/');
	while ( dot )
	{
		ret = dot;
		dot = strchr(ret+1,'\\');
		if ( dot == 0 )
			dot = strchr(ret+1,'/');
	}
	return ret;
}

#pragma warning(disable:4996)

void demonstrateMeshExporting(NVSHARE::MeshImport *meshImportLibrary)
{
	NVSHARE::MeshSystemContainer *msc = meshImportLibrary->createMeshSystemContainer(); // create an empty mesh system container.
	NVSHARE::MeshImportInterface *mii = meshImportLibrary->getMeshImportInterface(msc); // get an importer interface for this mesh system container.

	mii->importAssetName("TestAsset","TestAssetInfo"); // specify the name of the asset.
	mii->importMesh("TestMesh","TestSkeleton");        // specify the name of hte mesh, and the skeleton associated with that mesh.
	mii->importMaterial("TestMaterial","TestMaterialInfo"); // specify the name of a material and any associated material meta data

    NVSHARE::MeshSkeleton skeleton;           	// declare a skeletaon.
    skeleton.mName = "TestSkeleton";			// set the name of the skeleton
    skeleton.mBoneCount = 1;					// The skeleton has just one bone
    NVSHARE::MeshBone bone;						// declare a single bone.
    bone.mName = "Bip01";                       // assign the name of the bone.
    skeleton.mBones = &bone;					// assign the bones pointer in the skeleton.
    mii->importSkeleton(skeleton);				// import the skeleton

	NVSHARE::MeshAnimation animation;   // declare a test animation.
	animation.mName = "TestAnimation";
	animation.mTrackCount = 1; // declare only a single track.
	animation.mFrameCount = 2; // declare two frames of animation.
	animation.mDuration   = (1.0f/60.0f)*2; // duration is 2 60th's of a second.
	animation.mDtime      = (1.0f/60.0f);   // frame delta time is 1/60th of a second (60 fps)

    NVSHARE::MeshAnimTrack track; // declare a single track of animation
    track.mName = "Bip01";    // assign the name of the track.
    track.mFrameCount = 2;    // two frames of animation data.
    track.mDuration   = (1.0f/60.0f)*2;
    track.mDtime      = (1.0f/60.0f);
	NVSHARE::MeshAnimPose poses[2]; // declare two frames of pose data.
	track.mPose       = &poses[0];
	NVSHARE::MeshAnimTrack *trackPtr = &track;

	animation.mTracks = &trackPtr; // assign the pointer to the single track of animation data.

	mii->importAnimation(animation);

    NVSHARE::MeshVertex v1,v2,v3;		// declare three vertices describing a single triangle

    v1.mPos[0] = 0;
    v1.mPos[1] = 0;
    v1.mPos[2] = 0;

    v2.mPos[0] = 1;
    v2.mPos[1] = 0;
    v2.mPos[2] = 0;

    v3.mPos[0] = 1;
    v3.mPos[1] = 1;
    v3.mPos[2] = 0;

	mii->importTriangle("TestMesh","TestMaterial",NVSHARE::MIVF_ALL | NVSHARE::MIVF_INTERP1,v1,v2,v3); // import the single triangle


    meshImportLibrary->gather(msc); // gather the contents of the mesh system container

    NVSHARE::MeshSystem *msexp = meshImportLibrary->getMeshSystem(msc); // get the mesh system data.
	NVSHARE::MeshSerialize data(NVSHARE::MSF_EZMESH);
	bool ok = meshImportLibrary->serializeMeshSystem(msexp,data); // serialize it in EZ-MESH
	if ( ok && data.mBaseData )
	{
		FILE *fph = fopen("../export.ezm", "wb");
		if ( fph )
		{
			fwrite(data.mBaseData, data.mBaseLen, 1, fph );
			fclose(fph);
		}
	}
	meshImportLibrary->releaseSerializeMemory(data);
	meshImportLibrary->releaseMeshSystemContainer(msc);
}

int main(int argc,const char **argv)
{
    char mesh[MAX_PATH] = {"ClothSim.ezm"};
    if (argc >= 1)
        strncpy(mesh, argv[1], sizeof(mesh));

    const char *dirname = nullptr;
    char prevcwd[MAX_PATH];
    getcwd(prevcwd, MAX_PATH);

    char strExePath [MAX_PATH];
#ifdef WIN32
    GetModuleFileNameA(NULL, strExePath, MAX_PATH);
#else
    strcpy(strExePath, argv[0]);
#endif

    char *slash = (char *)lastSlash(strExePath);
    if ( slash )
    {
        *slash = 0;
        dirname = strExePath;
    }
    assert(dirname != nullptr);
    printf("dirname=%s\n", dirname );

	NVSHARE::MeshImport *meshImportLibrary = NVSHARE::loadMeshImporters(dirname); // load the mesh import dll and associated importers

	if ( meshImportLibrary )
	{
		FILE *fph = fopen(mesh,"rb");  // read in an EZ-Mesh
		if ( !fph )
        {
            fprintf(stderr, "no such file %s\n", mesh);
            return 1;
        }
        else
		{
			fseek(fph,0L,SEEK_END);
			NxU32 dlen = ftell(fph);
			fseek(fph,0L,SEEK_SET);
			if ( dlen > 0 )
			{
				char *data = (char *)::malloc(dlen);
				fread(data,dlen,1,fph);
				NVSHARE::MeshSystemContainer *msc = meshImportLibrary->createMeshSystemContainer(mesh,data,dlen,nullptr);
				::free(data);
				if ( msc )
				{
					NVSHARE::MeshSystem *ms = meshImportLibrary->getMeshSystem(msc);
					fprintf(stderr, "%s has now been imported.  You can operate on the data now here.\n", mesh);


					demonstrateMeshExporting(meshImportLibrary);

					meshImportLibrary->releaseMeshSystemContainer(msc);
				}
				else
				{
					fprintf(stderr, "Failed to import %s\n", mesh);
				}
			}
			fclose(fph);
		}
	}
	else
	{
		printf("Failed to load MeshImport library\n");
	}
    return 0;
}
