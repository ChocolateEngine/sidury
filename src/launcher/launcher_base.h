#pragma once

#include "core/platform.h"

int  load_object( Module* mod, const char* path );
void unload_objects();
int  start( int argc, char* argv[], const char* spGameName, const char* spModuleName );

