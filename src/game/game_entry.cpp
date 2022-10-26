#include "../../src/game/main.h"

#include "core/profiler.h"
#include "ch_iengine.h"

#include "imgui/imgui.h"

#include <chrono>
#include <vector>
#include <functional>

GameSystem *gamesystem = new GameSystem;
Ch_IEngine *engine     = 0;

CONVAR( en_max_frametime, 0.1 );
CONVAR( en_timescale, 1 );
CONVAR( en_fps_max, 0 );

extern "C"
{
	void DLL_EXPORT game_init()
	{
		if ( Args_Find( "-debugger" ) )
			sys_wait_for_debugger();
		
		Module pHandle;
		Ch_IEngine *( *cengine_get )() = 0;

		std::string path = FileSys_FindFile( "engine" EXT_DLL );

		if ( path == "" )
		{
			Log_Fatal( "Couldn't find engine" EXT_DLL "!\n" );
			return;
		}

		if ( !( pHandle = SDL_LoadObject( path.c_str() ) ) )
		{
			Log_FatalF( "Failed to load engine: %s!\n", SDL_GetError() );
			return;
		}
		*( void** )( &cengine_get ) = SDL_LoadFunction( pHandle, "cengine_get" );
		if ( !cengine_get )
		{
			Log_FatalF( "Failed to load engine's entry point: %s!\n", SDL_GetError() );
			return;
		}

		ImGui::CreateContext();

		engine = cengine_get();

		// Load Modules and Initialize them in this order
		engine->Init({
			"input",
			"ch_graphics",
			"ch_gui",
			"aduio",
			"ch_physics",
		});

		gamesystem->Init();

		Con_QueueCommandSilent( "exec ongameload", false );

		// ftl::TaskSchedulerInitOptions schedOptions;
		// schedOptions.Behavior = ftl::EmptyQueueBehavior::Sleep;
		// 
		// gTaskScheduler.Init( schedOptions );
		
		auto startTime = std::chrono::high_resolution_clock::now();

		while ( engine->IsActive() )
		{
			// ZoneScoped
			PROF_SCOPE();

			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
	
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

			// ftl::TaskCounter taskCounter( &gTaskScheduler );

			gamesystem->Update( time );
			// Con_Update();
			
			// Wait and help to execute unfinished tasks
			// gTaskScheduler.WaitForCounter( &taskCounter );

			startTime = currentTime;

			profile_end_frame();
		}

		delete engine;

		SDL_UnloadObject( pHandle );
	}
}
