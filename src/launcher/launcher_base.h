#pragma once

#include "core/platform.h"


#ifdef _WIN32
constexpr char CH_PATH_SEP = '\\';
#define        CH_PATH_SEP_STR "\\"

#elif __unix__
constexpr char CH_PATH_SEP = '/';
#define        CH_PATH_SEP_STR "/"
#endif


int  load_object( Module* mod, const char* path );
void unload_objects();
int  start( int argc, char* argv[], const char* spGameName, const char* spModuleName );

