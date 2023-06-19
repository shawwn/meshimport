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

#ifdef WIN32
    #ifdef MESHIMPORT_EXPORTS
#define MESHIMPORT_API __declspec(dllexport)
#else
#define MESHIMPORT_API __declspec(dllimport)
#endif
#else
#define MESHIMPORT_API
#endif

extern "C"
{
MESHIMPORT_API NVSHARE::MeshImport * getInterface(NxI32 version_number);
};

static void *getMeshBindingInterface(const char *dll,NxI32 version_number,bool dynamic = true) // loads the tetra maker DLL and returns the interface pointer.
{
    if (dynamic)
    {
        void *ret = 0;

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
    return getInterface(version_number);
}
#endif

}; // end of namespace

#ifdef LINUX_GENERIC
#include <sys/types.h>
#include <sys/dir.h>
#endif

#define MAXNAME 512

#define MESHIMPORT_NVSHARE MESHIMPORT_##NVSHARE

#ifdef LINUX_GENERIC
#include <glob.h>
#include <fnmatch.h>
#include <vector>
#include <string>
namespace MESHIMPORT_NVSHARE
{

std::vector<std::string> glob(const std::string& pattern) {
    glob_t glob_result = {0}; // zero initialize

    // do the glob operation
    int return_value = ::glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);

    if(return_value != 0) throw std::runtime_error(std::strerror(errno));

    // collect all the filenames into a std::vector<std::string>
    // using the vector constructor that takes two iterators
    std::vector<std::string> filenames(
        glob_result.gl_pathv, glob_result.gl_pathv + glob_result.gl_pathc);

    // cleanup
    globfree(&glob_result);

    // done
    return filenames;
}
}
#endif

namespace MESHIMPORT_NVSHARE
{

class FileFind
{
public:
  FileFind(const char *dirname,const char *spec)
  {
    if ( dirname && strlen(dirname) )
      snprintf(mSearchName,sizeof(mSearchName),"%s%c%s",dirname,pathsep,spec);
    else
      snprintf(mSearchName,sizeof(mSearchName),"%s",spec);
#ifdef LINUX_GENERIC
    snprintf(mSearchPattern,sizeof(mSearchPattern),"%s",spec);
#endif
   }

  ~FileFind(void)
  {
  }


  bool FindFirst(char *name)
  {
    bool ret=false;

    #ifdef WIN32
    hFindNext = FindFirstFileA(mSearchName, &finddata);
    if ( hFindNext == INVALID_HANDLE_VALUE )
      ret =  false;
     else
     {
       bFound = 1; // have an initial file to check.
       ret =  FindNext(name);
     }
     #endif
     #ifdef LINUX_GENERIC
     mDir = opendir(".");
     ret = FindNext(name);
    #endif
    return ret;
  }

  bool FindNext(char *name)
  {
    bool ret = false;

    #ifdef WIN32
    while ( bFound )
    {
      bFound = FindNextFileA(hFindNext, &finddata);
      if ( bFound && (finddata.cFileName[0] != '.') && !(finddata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
      {
        strncpy(name,finddata.cFileName,MAXNAME);
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
          mDir = 0;
          ret = false;
          break;
        }

        if ( strcmp(di->d_name,".") == 0 || strcmp(di->d_name,"..") == 0 )
        {
          // skip it!
        }
        else
        {
          bool found = !fnmatch(mSearchPattern, di->d_name, FNM_PATHNAME);
          if (!found)
          {
              // skip it!
              int x = 42;
          }
          else
          {
              strncpy(name,di->d_name,MAXNAME);
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
  char mSearchName[MAXNAME];
#ifdef WIN32
  WIN32_FIND_DATAA finddata;
  HANDLE hFindNext;
  NxI32 bFound;
#endif
#ifdef LINUX_GENERIC
  char mSearchPattern[MAXNAME];
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
  NVSHARE::MeshImport *ret = 0;
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

#if defined(WIN32)
  ret = (NVSHARE::MeshImport *)getMeshBindingInterface(scratch,MESHIMPORT_VERSION);
#else
  ret = (NVSHARE::MeshImport *)getMeshBindingInterface(nullptr,MESHIMPORT_VERSION);
#endif

  if ( ret )
  {
#ifdef _M_IX86
      NVSHARE::FileFind ff(directory,"MeshImport*_x86.dll");
#elif defined(WIN32)
    NVSHARE::FileFind ff(directory,"MeshImport*_x64.dll");
#else
      NVSHARE::FileFind ff(directory,"libMeshImport*.so");
#endif
    char name[MAXNAME];
    if ( ff.FindFirst(name) )
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
      } while ( ff.FindNext(name) );
    }
  }
  return ret;
}

}; // end of namespace