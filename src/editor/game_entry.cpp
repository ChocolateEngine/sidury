#include "../../src/game/main.h"

#include "core/profiler.h"

#include "iinput.h"
#include "render/irender.h"
#include "igraphics.h"
#include "igui.h"
#include "iaudio.h"
#include "physics/iphysics.h"

#include "imgui/imgui.h"

#include <chrono>
#include <vector>
#include <functional>

#if CH_USE_MIMALLOC
  #include "mimalloc-new-delete.h"
#endif

static bool gWaitForDebugger = Args_Register( "Upon Program Startup, Wait for the Debuger to attach", "-debugger" );
static bool gArgNoSteam      = Args_Register( "Don't try to load the steam abstraction", "-no-steam" );
static bool gRunning         = true;

CONVAR( host_max_frametime, 0.1 );
CONVAR( host_timescale, 1 );
CONVAR( host_fps_max, 300 );


CONCMD( exit )
{
	gRunning = false;
}

CONCMD( quit )
{
	gRunning = false;
}

#if CH_USE_MIMALLOC
CONCMD( mimalloc_print )
{
	// TODO: have this output to the logging system
	mi_collect( true );
	mi_stats_merge();
	mi_stats_print( nullptr );
}
#endif


extern IGuiSystem*   gui;
extern IRender*      render;
extern IInputSystem* input;
extern IAudioSystem* audio;
extern Ch_IPhysics*  ch_physics;
extern IGraphics*    graphics;


static AppModule_t gAppModules[] = 
{
	{ (ISystem**)&input,      "ch_input",     IINPUTSYSTEM_NAME, IINPUTSYSTEM_HASH },
	{ (ISystem**)&render,     "ch_render_vk", IRENDER_NAME, IRENDER_VER },  // TODO: rename to ch_render_vk
	{ (ISystem**)&audio,      "ch_aduio",     IADUIO_NAME, IADUIO_VER },
	{ (ISystem**)&ch_physics, "ch_physics",   IPHYSICS_NAME, IPHYSICS_HASH },
    { (ISystem**)&graphics,   "ch_graphics",  IGRAPHICS_NAME, IGRAPHICS_VER },
	{ (ISystem**)&gui,        "ch_gui",       IGUI_NAME, IGUI_HASH },
};


extern "C"
{
	void DLL_EXPORT game_init()
	{
		if ( gWaitForDebugger )
			sys_wait_for_debugger();

		IMGUI_CHECKVERSION();

#if CH_USE_MIMALLOC
		Log_DevF( 1, "Using mimalloc version %d\n", mi_version() );
#endif

		// Needs to be done before Renderer is loaded
		ImGui::CreateContext();

		// if ( gArgUseGL )
		// {
		// 	gAppModules[ 1 ].apModuleName = "ch_render_gl";
		// }

		// Load Modules and Initialize them in this order
		if ( !Mod_AddSystems( gAppModules, ARR_SIZE( gAppModules ) ) )
		{
			Log_Error( "Failed to Load Systems\n" );
			return;
		}

		Mod_InitSystems();

		if ( !Game_Init() )
		{
			Log_Error( "Failed to Start Game!\n" );
			Game_Shutdown();
			return;
		}

		Con_QueueCommandSilent( "exec ongameload", false );

		// ftl::TaskSchedulerInitOptions schedOptions;
		// schedOptions.Behavior = ftl::EmptyQueueBehavior::Sleep;
		// 
		// gTaskScheduler.Init( schedOptions );
		
		auto startTime = std::chrono::high_resolution_clock::now();

		// -------------------------------------------------------------------
		// Main Loop

		while ( gRunning )
		{
			PROF_SCOPE_NAMED( "Main Loop" );

			auto currentTime = std::chrono::high_resolution_clock::now();
			float time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();
	
			// don't let the time go too crazy, usually happens when in a breakpoint
			time = glm::min( time, host_max_frametime.GetFloat() );

			if ( host_fps_max.GetFloat() > 0.f )
			{
				float maxFps = glm::clamp( host_fps_max.GetFloat(), 10.f, 5000.f );

				// check if we still have more than 2ms till next frame and if so, wait for "1ms"
				float minFrameTime = 1.0f / maxFps;
				if ( (minFrameTime - time) > (2.0f/1000.f))
					sys_sleep( 1 );

				// framerate is above max
				if (time < minFrameTime)
					continue;
			}

			// ftl::TaskCounter taskCounter( &gTaskScheduler );

			input->Update( time );

			// may change from input update running the quit command
			if ( !gRunning )
				break;

			Game_Update( time );
			
			// Wait and help to execute unfinished tasks
			// gTaskScheduler.WaitForCounter( &taskCounter );

			startTime = currentTime;

#ifdef TRACY_ENABLE
			FrameMark;
#endif
		}

		Game_Shutdown();
	}
}

