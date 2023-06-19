#ifndef LINUX_COMPAT_H
#define LINUX_COMPAT_H

//=============================================================================
// Misc Windows-to-Linux compatibility
//=============================================================================
#ifndef WIN32
#include <stdio.h>
#include <stdarg.h>
#define _vsnprintf vsnprintf
#define sprintf_s snprintf

#include <math.h>
#define _finite isfinite

#include <unistd.h>
#define _chdir chdir
#endif
//=============================================================================

//=============================================================================
// Strings
//=============================================================================
#ifndef WIN32
#include "utf8.h"

#define _stricmp stricmp
#define stricmp strcasecmp
#define strlwr utf8lwr
#endif
//=============================================================================

//=============================================================================
// Filesystem Paths
//=============================================================================

/*====================
  MAX_PATH
  ====================*/
#ifndef WIN32
#include <limits.h>
#define MAX_PATH PATH_MAX
#endif

/*====================
  pathsep
  A C++ character equivalent to Python's os.path.sep value.
  ====================*/
#ifdef WIN32
static const char pathsep = '\\';
#else
static const char pathsep = '/';
#endif
//=============================================================================

#endif