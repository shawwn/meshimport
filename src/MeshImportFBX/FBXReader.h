#ifndef FBX_READER_H

#define FBX_READER_H


#include "ImportFBX.h"
#include "UserMemAlloc.h"

namespace NVSHARE
{

#define OUTPUT_TEXT_BUFFER_SIZE	1000


class FBXImporterReader : public FbxReader
{
public:


    FBXImporterReader(FbxManager &pFbxManager, int pID, FbxStatus& status);

    virtual ~FBXImporterReader();

    virtual void GetVersion(int& pMajor, int& pMinor, int& pRevision) const;
    virtual bool FileOpen(char* pFileName) override;
    virtual bool FileClose() override;
    virtual bool IsFileOpen() override;

    virtual bool GetReadOptions(bool pParseFileAsNeeded = true) override;
    virtual bool Read(FbxDocument* pDocument) override;

private:
    FILE *mFilePointer;
    FbxManager *mManager;
};

extern  FbxReader* CreateFBXImporterReader( FbxManager& pManager, FbxImporter& pImporter, int pSubID, int pPluginID );
extern void* GetFBXImporterReaderInfo( FbxReader::EInfoRequest pRequest, int pId );
extern void FillFBXImporterReaderIOSettings( FbxIOSettings& pIOS );

};

#endif
