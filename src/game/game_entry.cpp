#include "../../src/game/gamesystem.h"
#include "core/platform.h"

#include <vector>

#include "shims.h"

extern GameSystem *game;

static bool gRunning = true;

CONVAR( s_max_frametime, 0.2 );
CONVAR( s_fps_max, 0 );

CON_COMMAND( quit )
{
	gRunning = false;
}

extern "C"
{
	void DLL_EXPORT game_init(  )
	{
		init_aduio_shims(  );
		init_graphics_shims(  );
		init_engine_shims(  );



		//




		InitInput(  );
		Init(  );//garfield
		AduioInit(  );
		console->RegisterConVars(  );







		
		
	        game = new GameSystem;
		game->Init(  );
		while ( gRunning )
		{
			static auto startTime = std::chrono::high_resolution_clock::now(  );

			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count(  );
	
			// don't let the time go too crazy, usually happens when in a breakpoint
			time = glm::min( time, s_max_frametime.GetFloat() );

			// rendering is actually half the framerate for some reason, odd
			if ( s_fps_max.GetFloat() > 0.f )
			{
				// HACK: fps is doubled for some reason, so multiple by 2
				float maxFps = glm::clamp( s_fps_max.GetFloat() * 2.f, 20.f, 5000.f );

				// check if we still have more than 2ms till next frame and if so, wait for "1ms"
				float minFrameTime = 1.0f / maxFps;
				if ( (minFrameTime - time) > (2.0f/1000.f))
					SDL_Delay(1);

				// framerate is above max
				if (time < minFrameTime)
					continue;
			}

			console->Update(  );
			InputUpdate( time );
			game->Update( time );
			AduioUpdate( time );
			DrawFrame( time );

			startTime = currentTime;
		}
	}
}
