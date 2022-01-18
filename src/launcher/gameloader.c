#include "core/platform.h"
#include <stdio.h>

#include <SDL2/SDL_loadso.h>

Module core = 0;
Module imgui = 0;
Module client = 0;


void unload_objects()
{
	if ( core )     SDL_UnloadObject( core );
	if ( imgui )    SDL_UnloadObject( imgui );
	if ( client )   SDL_UnloadObject( client );
}


int load_object( Module* mod, const char* path )
{
	char pathExt[512];
	snprintf(pathExt, 512, "%s%s", path, EXT_DLL );

	if ( *mod = SDL_LoadObject( pathExt ) )
		return 0;

	fprintf( stderr, "Failed to load %s: %s\n", pathExt, SDL_GetError(  ) );
	//sys_print_last_error( "Failed to load %s", path );

	unload_objects(  );

	return -1;
}


int main( int argc, char *argv[] )
{
	void ( *game_init )() = 0;
	void ( *core_init )( int argc, char *argv[], const char* gamePath ) = 0;

	if ( load_object( &core, "bin/core" ) == -1 )
		return -1;
	if ( load_object( &imgui, "bin/imgui" ) == -1 )
		return -1;
	if ( load_object( &client, "sidury/bin/client" ) == -1 )
		return -1;

	*( void** )( &core_init ) = SDL_LoadFunction( core, "core_init" );
	if ( !core_init )
	{
		fprintf( stderr, "Error: %s\n", SDL_GetError(  ) );
		unload_objects();
		return -1;
	}

	core_init( argc, argv, "sidury" );

	*( void** )( &game_init ) = SDL_LoadFunction( client, "game_init" );
	if ( !game_init )
	{
		fprintf( stderr, "Error: %s\n", SDL_GetError(  ) );
		unload_objects();
		return -1;
	}

	game_init();
	unload_objects();

	return 0;
}
