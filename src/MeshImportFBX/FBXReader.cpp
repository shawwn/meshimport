// Local definitions

#include "FBXReader.h"
#include "UserMemAlloc.h"

namespace NVSHARE
{

FBXImporterReader::FBXImporterReader(FbxManager &pFbxManager, int pID, FbxStatus &status):
FbxReader(pFbxManager, pID, status),
mFilePointer(NULL),
mManager(&pFbxManager)
{
}

FBXImporterReader::~FBXImporterReader()
{
    FileClose();
}

void FBXImporterReader::GetVersion(int& pMajor, int& pMinor, int& pRevision) const
{
    pMajor = 1;
    pMinor = 0;
    pRevision=0;
}

bool FBXImporterReader::FileOpen(char* pFileName)
{
    if(mFilePointer != NULL)
        FileClose();
    mFilePointer = fopen(pFileName, "r");
    if(mFilePointer)
        return false;
    return true;
}
bool FBXImporterReader::FileClose()
{
    if(mFilePointer!=NULL)
        fclose(mFilePointer);
    return true;
    
}
bool FBXImporterReader::IsFileOpen()
{
    if(mFilePointer != NULL)
        return true;
    return false;
}

bool FBXImporterReader::GetReadOptions(bool pParseFileAsNeeded)
{
    return true;
}

//Read the custom file and reconstruct node hierarchy.
bool FBXImporterReader::Read(FbxDocument* pDocument)
{
    if (!pDocument)
    {
        return false;
    }
    FbxScene*      lScene = FbxCast<FbxScene>(pDocument);
    bool            lIsAScene = (lScene != NULL);
    bool            lResult = false;

    if(lIsAScene)
    {
        FbxNode* lRootNode = lScene->GetRootNode();
        FbxNodeAttribute * lRootNodeAttribute = FbxNull::Create(mManager,"");
        lRootNode->SetNodeAttribute(lRootNodeAttribute);

        int lSize;
        char* lBuffer;    
        if(mFilePointer != NULL)
        {
            //To obtain file size
            fseek (mFilePointer , 0 , SEEK_END);
            lSize = ftell (mFilePointer);
            rewind (mFilePointer);

            //Read file content to a string.
            lBuffer = (char*) malloc (sizeof(char)*lSize);
            size_t lRead = fread(lBuffer, 1, lSize, mFilePointer);
            lBuffer[lRead]='\0';
            FbxString lString(lBuffer);

            //Parse the string to get name and relation of Nodes. 
            FbxString lSubString, lChildName, lParentName;
            FbxNode* lChildNode;
            FbxNode* lParentNode;
            FbxNodeAttribute* lChildAttribute;
            int lEndTokenCount = lString.GetTokenCount("\n");

            for (int i = 0; i < lEndTokenCount; i++)
            {
                lSubString = lString.GetToken(i, "\n");
                FbxString lNodeString;
                lChildName = lSubString.GetToken(0, "\"");
                lParentName = lSubString.GetToken(2, "\"");

                //Build node hierarchy.
                if(lParentName == "RootNode")
                {
                    lChildNode = FbxNode::Create(mManager,lChildName.Buffer());
                    lChildAttribute = FbxNull::Create(mManager,"");
                    lChildNode->SetNodeAttribute(lChildAttribute);

                    lRootNode->AddChild(lChildNode);
                }
                else
                {
                    lChildNode = FbxNode::Create(mManager,lChildName.Buffer());
                    lChildAttribute = FbxNull::Create(mManager,"");
                    lChildNode->SetNodeAttribute(lChildAttribute);

                    lParentNode = lRootNode->FindChild(lParentName.Buffer());
                    lParentNode->AddChild(lChildNode);
                }
            }
        }
        lResult = true;
    }    
    return lResult;
}


FbxReader* CreateFBXImporterReader( FbxManager& pManager, FbxImporter& pImporter, int pSubID, int pPluginID )
{
    FbxStatus status;
    return new FBXImporterReader( pManager, pPluginID, status );
}


// Get extension, description or version info about MyOwnReader
void* GetFBXImporterReaderInfo( FbxReader::EInfoRequest pRequest, int pId )
{
    static char const* sExt[] = 
    {
        0
    };

    static char const* sDesc[] = 
    {
        0
    };

    switch (pRequest)
    {
    case FbxReader::eInfoExtension:
        return sExt;
    case FbxReader::eInfoDescriptions:
        return sDesc;
    default:
        return 0;
    }
}

void FillFBXImporterReaderIOSettings( FbxIOSettings& pIOS )
{    
}

}; // end of namespace
