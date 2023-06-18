#include "../../src/game/main.h"

#include "core/profiler.h"

#include "iinput.h"
#include "render/irender.h"
#include "igui.h"
#include "iaudio.h"
#include "physics/iphysics.h"
#include "steam.h"

#include "imgui/imgui.h"

#include <chrono>
#include <vector>
#include <functional>

#if CH_USE_MIMALLOC
  #include "mimalloc-new-delete.h"
#endif

static bool gWaitForDebugger = Args_Register( "Upon Program Startup, Wait for the Debuger to attach", "-debugger" );
static bool gArgNoSteam = Args_Register( "Don't try to load the steam abstraction", "-no-steam" );
static bool gRunning   = true;

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


static AppModules_t gAppModules[] = 
{
	{ (void**)&input,      "ch_input",    IINPUTSYSTEM_NAME, IINPUTSYSTEM_HASH },
	{ (void**)&render,     "ch_graphics", IRENDER_NAME, IRENDER_VER },
	{ (void**)&gui,        "ch_gui",      IGUI_NAME, IGUI_HASH },
	{ (void**)&audio,      "ch_aduio",    IADUIO_NAME, IADUIO_HASH },
	{ (void**)&ch_physics, "ch_physics",  IPHYSICS_NAME, IPHYSICS_HASH },
};


// We load this separately because all modules loaded through Mod_AddSystems() are required and shuts down on failure,
// and this is not required.
static void LoadSteamAbstraction()
{
	if ( !Mod_Load( "ch_steam" ) )
	{
		Log_Error( "Failed to load module: ch_steam\n" );
		return;
	}

	// add system we want from module
	void* system = Mod_GetInterface( ISTEAM_NAME, ISTEAM_VER );
	if ( system == nullptr )
	{
		Log_ErrorF( "Failed to load system from module: ch_steam - %s\n", ISTEAM_NAME );
		return;
	}

	steam = static_cast< ISteamSystem* >( system );
	Mod_AddLoadedSystem( steam );
}


extern "C"
{
	void DLL_EXPORT game_init()
	{
		if ( gWaitForDebugger )
			sys_wait_for_debugger();

#if CH_USE_MIMALLOC
		Log_DevF( 1, "Using mimalloc version %d\n", mi_version() );
#endif

		// Needs to be done before Renderer is loaded
		ImGui::CreateContext();

		// Load Modules and Initialize them in this order
		if ( !Mod_AddSystems( gAppModules, ARR_SIZE( gAppModules ) ) )
		{
			Log_Error( "Failed to Load Systems\n" );
			return;
		}

		if ( !gArgNoSteam )
			LoadSteamAbstraction();

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
			// ZoneScoped
			PROF_SCOPE();

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
			
			profile_end_frame();
		}

		Game_Shutdown();
	}
}

