#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef WIN32
#include <windows.h>
#include <windowsx.h>
#endif

#include "MeshImport.h"

#pragma warning(disable:4996)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

namespace NVSHARE
{

#ifdef WIN32

static void *getMeshBindingInterface(const char *dll,NxI32 version_number) // loads the tetra maker DLL and returns the interface pointer.
{
  void *ret = 0;

  UINT errorMode = 0;
  errorMode = SEM_FAILCRITICALERRORS;
  UINT oldErrorMode = SetErrorMode(errorMode);
  HMODULE module = LoadLibraryA(dll);
  SetErrorMode(oldErrorMode);
  if ( module )
  {
    void *proc = GetProcAddress(module,"getInterface");
    if ( proc )
    {
      typedef void * (__cdecl * NX_GetToolkit)(NxI32 version);
      ret = ((NX_GetToolkit)proc)(version_number);
    }
  }
  return ret;
}



#elif 1
#include <dlfcn.h>

static void *getMeshBindingInterface(const char *dll,NxI32 version_number) // loads the tetra maker DLL and returns the interface pointer.
{
    void *ret = nullptr;

    void* module = dlopen(dll, RTLD_NOW);
    if ( module )
    {
        void *proc = dlsym(module,"getInterface");
        if ( proc )
        {
            typedef void * (__cdecl * NX_GetToolkit)(NxI32 version);
            ret = ((NX_GetToolkit)proc)(version_number);
        }
    }
    return ret;
}
#endif

}; // end of namespace

#ifdef LINUX_GENERIC
#include <sys/types.h>
#include <sys/dir.h>
#include <fnmatch.h>
#endif

#define MAXNAME 512

#define MESHIMPORT_NVSHARE MESHIMPORT_##NVSHARE

namespace MESHIMPORT_NVSHARE
{

class FileFind
{
public:
  FileFind(const char *dirname,const char *spec)
  {
    if ( dirname && strlen(dirname) )
      snprintf(mSearchDir, sizeof(mSearchDir), "%s%c", dirname, pathsep);
    else
      strncpy(mSearchDir, "%s%c", sizeof(mSearchDir));
    strncpy(mSearchName, spec, sizeof(mSearchName));
   }


  bool FindFirst(char *name, size_t namelen)
  {
    bool ret=false;

    #ifdef WIN32
    char searchName[MAXNAME] = {};
    if ( strlen(mSearchDir) )
      snprintf(searchName,sizeof(searchName),"%s%c%s",mSearchDir,pathsep,mSearchName);
    else
      snprintf(searchName,sizeof(searchName),"%s",mSearchName);
    hFindNext = FindFirstFileA(searchName, &finddata);
    if ( hFindNext == INVALID_HANDLE_VALUE )
      ret =  false;
     else
     {
       bFound = 1; // have an initial file to check.
       ret =  FindNext(name, namelen);
     }
     #endif
     #ifdef LINUX_GENERIC
     mDir = opendir(mSearchDir);
     ret = FindNext(name, namelen);
    #endif
    return ret;
  }

  bool FindNext(char *name, size_t namelen)
  {
    bool ret = false;

    #ifdef WIN32
    while ( bFound )
    {
      bFound = FindNextFileA(hFindNext, &finddata);
      if ( bFound && (finddata.cFileName[0] != '.') && !(finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
      {
        strncpy(name,finddata.cFileName,namelen);
        ret = true;
        break;
      }
    }
    if ( !ret )
    {
      bFound = 0;
      FindClose(hFindNext);
    }
    #endif

    #ifdef LINUX_GENERIC

    if ( mDir )
    {
      while ( 1 )
      {

        struct direct *di = readdir( mDir );

        if ( !di )
        {
          closedir( mDir );
          mDir = nullptr;
          ret = false;
          break;
        }

        if ( strcmp(di->d_name,".") == 0 || strcmp(di->d_name,"..") == 0 )
        {
          // skip it!
        }
        else
        {
          bool found = !fnmatch(mSearchName, di->d_name, 0);
          if (!found)
          {
              // skip it!
              int x = 42;
          }
          else
          {
              snprintf(name, namelen, "%s%s", mSearchDir, di->d_name);
              ret = true;
              break;
          }
        }
      }
    }
    #endif
    return ret;
  }

private:
  char mSearchName[MAXNAME] = {};
#ifdef WIN32
  WIN32_FIND_DATAA finddata;
  HANDLE hFindNext;
  NxI32 bFound;
#endif
#ifdef LINUX_GENERIC
  char mSearchDir[MAXNAME] = {};
  char mSearchPattern[MAXNAME] = {};
  DIR      *mDir;
#endif
};

}; // end of namespace

namespace NVSHARE
{

	using namespace MESHIMPORT_NVSHARE;

static const char *lastSlash(const char *foo)
{
  const char *ret = foo;
  const char *last_slash = 0;

  while ( *foo )
  {
    if ( *foo == '\\' ) last_slash = foo;
    if ( *foo == '/' ) last_slash = foo;
    if ( *foo == ':' ) last_slash = foo;
    foo++;
  }
  if ( last_slash ) ret = last_slash+1;
  return ret;
}

NVSHARE::MeshImport * loadMeshImporters(const char * directory) // loads the mesh import library (dll) and all available importers from the same directory.
{
  NVSHARE::MeshImport *ret = nullptr;
#if defined(WIN32)
  const char * baseImporter = "MeshImport.dll";
#elif defined(__APPLE__)
  const char * baseImporter = "libMeshImport.dylib";
#else
  const char * baseImporter = "libMeshImport.so";
#endif

  char scratch[512];
  if ( directory && strlen(directory) )
  {
    snprintf(scratch,sizeof(scratch),"%s%c%s", directory, pathsep, baseImporter);
  }
  else
  {
    strcpy(scratch,baseImporter);
  }

  ret = (NVSHARE::MeshImport *)getMeshBindingInterface(scratch,MESHIMPORT_VERSION);

  if ( ret )
  {
#ifdef _M_IX86
      NVSHARE::FileFind ff(directory,"MeshImport*_x86.dll");
#elif defined(WIN32)
    NVSHARE::FileFind ff(directory,"MeshImport*_x64.dll");
#else
      NVSHARE::FileFind ff(directory,"libMeshImport*.so");
#endif
    char name[MAXNAME] = {};
    if ( ff.FindFirst(name, MAXNAME) )
    {
      do
      {
        const char *scan = lastSlash(name);
        if ( stricmp(scan,baseImporter) == 0 )
        {
          printf("Skipping '%s'\r\n", baseImporter);
        }
        else
        {
          printf("Loading '%s'\r\n", scan );
		  const char *fname;

		  if ( directory && strlen(directory) )
		  {
              snprintf(scratch,sizeof(scratch),"%s%c%s", directory, pathsep, scan);
			  fname = scratch;
		  }
		  else
		  {
			  fname = name;
		  }

          NVSHARE::MeshImporter *imp = (NVSHARE::MeshImporter *)getMeshBindingInterface(fname,MESHIMPORT_VERSION);
          if ( imp )
          {
            ret->addImporter(imp);
            printf("Added importer '%s'\r\n", name );
          }
        }
      } while ( ff.FindNext(name, MAXNAME) );
    }
  }
  return ret;
}

}; // end of namespace