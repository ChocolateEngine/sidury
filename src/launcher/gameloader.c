#include "core/platform.h"
#include <stdio.h>

#include <SDL2/SDL_loadso.h>

Module core = 0;
Module imgui = 0;
Module engine = 0;


void unload_objects(  )
{
	if ( core )     SDL_UnloadObject( core );
	if ( imgui )    SDL_UnloadObject( imgui );
	if ( engine )   SDL_UnloadObject( engine );
}


int load_object( Module* mod, const char* path )
{
	char pathExt[512];
	snprintf(pathExt, 512, "%s%s", path, EXT_DLL );

	/*mod = SDL_LoadObject( pathExt );

	//if ( !mod )
	if ( !mod )
	{
		fprintf( stderr, "Failed to load %s: %s\n", pathExt, SDL_GetError(  ) );
		//sys_print_last_error( "Failed to load %s", path );
		return -1;
	}

	return 0;
	*/

	if ( *mod = SDL_LoadObject( pathExt ) )
		return 0;

	fprintf( stderr, "Failed to load %s: %s\n", pathExt, SDL_GetError(  ) );
	//sys_print_last_error( "Failed to load %s", path );

	unload_objects(  );

	return -1;
}

int main
	(  )
{
	void ( *engine_start )(  );
	void ( *init_console )(  );

#ifdef _WIN32
	// SetDllDirectoryA()
#endif

	if ( load_object( &core, "bin/core" ) == -1 )
		return -1;

	*( void** )( &init_console ) = SDL_LoadFunction( core, "init_console" );
	if ( !init_console )
	{
		fprintf( stderr, "Error: %s\n", SDL_GetError(  ) );
		SDL_UnloadObject( core );
		return -1;
	}

	init_console();

	if ( load_object( &imgui, "bin/imgui" ) == -1 )
		return -1;

	if ( load_object( &engine, "bin/engine" ) == -1 )
		return -1;

	*( void** )( &engine_start ) = SDL_LoadFunction( engine, "engine_start" );
	if ( !engine_start )
	{
		fprintf( stderr, "Error: %s\n", SDL_GetError(  ) );
		SDL_UnloadObject( engine );
		return -1;
	}

	engine_start(  );
	SDL_UnloadObject( engine );

	return 0;
}
