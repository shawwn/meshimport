#ifndef MESH_IMPORT_BUILDER_H

#define MESH_IMPORT_BUILDER_H

#include "MeshImport.h"
#include "UserMemAlloc.h"

namespace NVSHARE
{

class KeyValueIni;

class MeshBuilder : public MeshSystem, public MeshImportInterface
{
public:
  virtual void gather(void) = 0;
  void scale(NxF32 s) { scale(s,s,s); }
  virtual void scale(NxF32 scaleX,NxF32 scaleY,NxF32 scaleZ) = 0;
  virtual void rotate(NxF32 rotX,NxF32 rotY,NxF32 rotZ) = 0;


};

MeshBuilder * createMeshBuilder(KeyValueIni *ini,
                                const char *meshName,
                                const void *data,
                                NxU32 dlen,
                                MeshImporter *mi,
                                const char *options,
                                MeshImportApplicationResource *appResource);

MeshBuilder * createMeshBuilder(MeshImportApplicationResource *appResource);
void          releaseMeshBuilder(MeshBuilder *m);

}; // end of namespace

#endif
