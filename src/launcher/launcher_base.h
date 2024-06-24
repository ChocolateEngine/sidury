#pragma once

#include "core/platform.h"


#ifdef _WIN32
constexpr char    CH_PATH_SEP = '\\';
#define           CH_PATH_SEP_STR "\\"

constexpr wchar_t CH_UPATH_SEP = L'\\';
#define           CH_UPATH_SEP_STR L"\\"

#elif __unix__
constexpr char    CH_PATH_SEP = '/';
#define           CH_PATH_SEP_STR "/"

constexpr char    CH_UPATH_SEP = '/';
#define           CH_UPATH_SEP_STR "/"
#endif


int  load_object( Module* mod, const char* path );
void unload_objects();
int  start( int argc, char* argv[], const char* spGameName, const char* spModuleName );

