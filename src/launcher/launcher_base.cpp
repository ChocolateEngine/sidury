#include "launcher_base.h"
#include "util.h"
#include <stdio.h>
#include <string>

#if CH_USE_MIMALLOC
  #include "mimalloc-new-delete.h"
#endif

#ifdef _WIN32
  #include <Windows.h>
  #include <direct.h>
#endif

#ifdef __unix__
  #include <string.h>
  #include <unistd.h>
  #include <dlfcn.h>	
#endif /* __unix __  */

Module core = 0;
Module imgui = 0;
Module client = 0;


#ifdef _WIN32
Module sys_load_library( const char* path )
{
	return (Module)LoadLibrary( path );
}

void sys_close_library( Module mod )
{
	FreeLibrary( (HMODULE)mod );
}

void* sys_load_func( Module mod, const char* name )
{
	return GetProcAddress( (HMODULE)mod, name );
}

const char* sys_get_error()
{
	DWORD errorID = GetLastError();

	if ( errorID == 0 )
		return "";  // No error message

	LPSTR strErrorMessage = NULL;

	FormatMessage(
	  FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_ALLOCATE_BUFFER,
	  NULL,
	  errorID,
	  0,
	  (LPSTR)&strErrorMessage,
	  0,
	  NULL );

	//std::string message;
	//message.resize(512);
	//snprintf( message.data(), 512, "Win32 API Error %d: %s", errorID, strErrorMessage );

	static char message[ 512 ];
	memset( message, 512, 0 );
	snprintf( message, 512, "Win32 API Error %ud: %s", errorID, strErrorMessage );

	// Free the Win32 string buffer.
	LocalFree( strErrorMessage );

	return message;
}

#elif __unix__

Module sys_load_library( const char* path )
{
	return (Module)dlopen( path, RTLD_LAZY );
}

void sys_close_library( Module mod )
{
    if ( !mod )
        return;

    dlclose( mod );
}

void* sys_load_func( Module mod, const char* path )
{
    return dlsym( mod, path );
}

const char* sys_get_error()
{
    return dlerror();
}

#else
	#error "No Library Loading Code"
#endif


void unload_objects()
{
	if ( core ) sys_close_library( core );
	if ( imgui ) sys_close_library( imgui );
	if ( client ) sys_close_library( client );
}


int load_object( Module* mod, const char* path )
{
	if ( *mod = sys_load_library( path ) )
		return 0;

	fprintf( stderr, "Failed to load %s: %s\n", path, sys_get_error() );
	unload_objects();

	return -1;
}

#if CH_USE_MIMALLOC
// ensure mimalloc is loaded
struct ForceMiMalloc_t
{
	ForceMiMalloc_t()
	{
		mi_version();

  #if _DEBUG
		mi_option_enable( mi_option_show_errors );
		mi_option_enable( mi_option_show_stats );
		mi_option_enable( mi_option_verbose );
  #endif
	}
};

static ForceMiMalloc_t forceMiMalloc;
#endif


// This adds a DLL search path, so i can store all dll's in the bin/win64 folder, instead of having dependency dlls in the root folder
void set_search_directory()
{
#ifdef _WIN32
	char* cwd = getcwd( 0, 0 );

	char  path[ 512 ] = {};
	strcat( path, cwd );
	strcat( path, CH_PATH_SEP_STR "bin" CH_PATH_SEP_STR CH_PLAT_FOLDER );

	auto ret = SetDllDirectory( path );
#endif
}


int start( int argc, char *argv[], const char* spGameName, const char* spModuleName )
{
	set_search_directory();

	int  ( *app_init )()                                                = 0;
	void ( *core_init )( int argc, char* argv[], const char* gamePath ) = 0;
	void ( *core_exit )( bool writeArchive )                            = 0;

	//if ( load_object( &sdl2, "bin/" CH_PLAT_FOLDER "/SDL2" EXT_DLL ) == -1 )
	//	return -1;
	if ( load_object( &core, "bin/" CH_PLAT_FOLDER "/ch_core" EXT_DLL ) == -1 )
		return -1;
	if ( load_object( &imgui, "bin/" CH_PLAT_FOLDER "/imgui" EXT_DLL ) == -1 )
		return -1;

	*(void**)( &core_init ) = sys_load_func( core, "core_init" );
	if ( !core_init )
	{
		fprintf( stderr, "Error: %s\n", sys_get_error() );
		unload_objects();
		return -1;
	}

	*(void**)( &core_exit ) = sys_load_func( core, "core_exit" );
	if ( !core_exit )
	{
		fprintf( stderr, "Error: %s\n", sys_get_error() );
		unload_objects();
		return -1;
	}

	// MUST LOAD THIS FIRST TO REGISTER LAUNCH ARGUMENTS
	core_init( argc, argv, spGameName );

	char name[ 512 ] = {};

	// TODO: remove path change in core_init()
#if _WIN32
	strcat( name, ".." CH_PATH_SEP_STR ".." CH_PATH_SEP_STR );
#else
	strcat( name, ".." CH_PATH_SEP_STR );
#endif

	strcat( name, spGameName );
	strcat( name, CH_PATH_SEP_STR "bin" CH_PATH_SEP_STR CH_PLAT_FOLDER CH_PATH_SEP_STR );
	strcat( name, spModuleName );
	strcat( name, EXT_DLL );

	if ( load_object( &client, name ) == -1 )
		return -1;

	// if ( load_object( &client, "bin/" CH_PLAT_FOLDER "/client" EXT_DLL ) == -1 )
	// 	return -1;

	*(void**)( &app_init ) = sys_load_func( client, "app_init" );
	if ( !app_init )
	{
		fprintf( stderr, "Error: %s\n", sys_get_error() );
		unload_objects();
		return -1;
	}

	int appRet = app_init();
	core_exit( appRet == 0 );

	unload_objects();

	return 0;
}
