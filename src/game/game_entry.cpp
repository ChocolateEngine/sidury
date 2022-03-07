#include "../../src/game/gamesystem.h"
/* Horrendous.  */
#include "../src/engine/engine.h"

#include <vector>
#include <functional>

GameSystem *gamesystem = new GameSystem;
Engine     *engine     = 0;

CONVAR( en_max_frametime, 0.1 );
CONVAR( en_timescale, 1 );
CONVAR( en_fps_max, 0 );

extern "C"
{
	void DLL_EXPORT game_init()
	{
		if ( cmdline->Find( "-debugger" ) )
			sys_wait_for_debugger();

		Module pHandle;
		Engine *( *cengine_get )() = 0;

		std::string path = filesys->FindFile( "engine" EXT_DLL );

		if ( path == "" ) {
			LogError( "Couldn't find %s\n", path.c_str() );
		}

		if ( !( pHandle = SDL_LoadObject( path.c_str() ) ) ) {
			LogError( "Failed to load engine: %s!\n", SDL_GetError() );
			return;
		}
		*( void** )( &cengine_get ) = SDL_LoadFunction( pHandle, "cengine_get" );
		if ( !cengine_get ) {
			LogError( "Failed to load engine's entry point: %s!\n", SDL_GetError() );
			return;
		}

		engine = cengine_get();
		engine->Init();
		gamesystem->Init();

		console->Add( "exec ongameload" );

		auto startTime = std::chrono::high_resolution_clock::now();

		while ( engine->aActive )
		{
			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count(  );
	
			// don't let the time go too crazy, usually happens when in a breakpoint
			time = glm::min( time, en_max_frametime.GetFloat() );

			if ( en_fps_max.GetFloat() > 0.f )
			{
				float maxFps = glm::clamp( en_fps_max.GetFloat(), 10.f, 5000.f );

				// check if we still have more than 2ms till next frame and if so, wait for "1ms"
				float minFrameTime = 1.0f / maxFps;
				if ( (minFrameTime - time) > (2.0f/1000.f))
					SDL_Delay(1);

				// framerate is above max
				if (time < minFrameTime)
					continue;
			}

			gamesystem->Update( time );
			console->Update();

			startTime = currentTime;
		}

		delete engine;

		SDL_UnloadObject( pHandle );
	}
}
