#include "../../src/game/gamesystem.h"
#include "core/platform.h"
/* Horrendous.  */
#include "../src/engine/engine.h"
#include "core/filesystem.h"

#include <vector>
#include <functional>

GameSystem *gamesystem = new GameSystem;
Engine     *engine     = 0;

CONVAR( en_max_frametime, 0.2 );
CONVAR( en_timescale, 1 );
CONVAR( en_fps_max, 0 );

extern "C"
{
	void DLL_EXPORT game_init()
	{
	        Module pHandle;
		Engine *( *cengine_get )() = 0;

		std::string path = filesys->FindFile( std::string( "engine" ) + EXT_DLL );

		if ( path == "" ) {
			fprintf( stderr, "Couldn't find %s\n", path.c_str() );
		}

		if ( !( pHandle = SDL_LoadObject( path.c_str() ) ) ) {
		        fprintf( stderr, "Failed to load engine: %s!\n", SDL_GetError() );
			return;
		}
		*( void** )( &cengine_get ) = SDL_LoadFunction( pHandle, "cengine_get" );
		if ( !cengine_get ) {
			fprintf( stderr, "Failed to load engine's entry point: %s!\n", SDL_GetError() );
			return;
		}

		engine = cengine_get();
		engine->Init();
		gamesystem->Init();

	        while ( engine->aActive ) {
			static auto startTime = std::chrono::high_resolution_clock::now(  );

			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count(  );
	
			// don't let the time go too crazy, usually happens when in a breakpoint
			time = glm::min( time, en_max_frametime.GetFloat() );

			// rendering is actually half the framerate for some reason, odd
			if ( en_fps_max.GetFloat() > 0.f )
			{
				// HACK: fps is doubled for some reason, so multiple by 2
				float maxFps = glm::clamp( en_fps_max.GetFloat() * 2.f, 20.f, 5000.f );

				// check if we still have more than 2ms till next frame and if so, wait for "1ms"
				float minFrameTime = 1.0f / maxFps;
				if ( (minFrameTime - time) > (2.0f/1000.f))
					SDL_Delay(1);

				// framerate is above max
				if (time < minFrameTime)
					return;
			}

			gamesystem->Update( time );

			engine->Update( time );

			startTime = currentTime;
		}
		delete engine;

		SDL_UnloadObject( pHandle );
	}
}
