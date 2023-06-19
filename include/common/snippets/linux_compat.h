#pragma once

#ifdef WIN32
static const char pathsep = '\\';
#else
static const char pathsep = '/';
#include "utf8.h"
#define _stricmp stricmp
#define stricmp strcasecmp
#define _vsnprintf vsnprintf
#define strlwr utf8lwr
#define _finite isfinite
#include <limits.h>
#define MAX_PATH PATH_MAX
#endif
