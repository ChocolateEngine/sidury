#include "main.h"
#include "core/systemmanager.h"
#include "core/asserts.h"

#include "iinput.h"
#include "igui.h"
#include "render/irender.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"
#include "imgui/ImGuizmo.h"

#include "util.h"
#include "game_physics.h"
#include "player.h"
#include "entity.h"
#include "graphics/graphics.h"
#include "mapmanager.h"
#include "inputsystem.h"
#include "skybox.h"
#include "testing.h"
#include "network/net_main.h"

#include "cl_main.h"
#include "sv_main.h"

#include "tools/light_editor.h"

#include <SDL_system.h>
#include <SDL_hints.h>

#include <algorithm>


IGuiSystem*      gui          = nullptr;
IRender*         render       = nullptr;
IInputSystem*    input        = nullptr;
IAudioSystem*    audio        = nullptr;

static bool      gPaused      = false;
float            gFrameTime   = 0.f;
double           gCurTime     = 0.0;  // i could make this a size_t, and then just have it be every 1000 is 1 second

Entity           gLocalPlayer = CH_ENT_INVALID;
ViewportCamera_t gView{};

extern bool      gRunning;
extern ConVar    r_nearz, r_farz, r_fov;
extern ConVar    host_timescale;

CONVAR( phys_friction, 10 );
CONVAR( dbg_global_axis, 1 );
CONVAR( dbg_global_axis_size, 15 );
CONVAR( r_render, 1 );


// CON_COMMAND( pause )
// {
// 	gui->ShowConsole();
// }


void Game_Shutdown()
{
	TEST_Shutdown();

	CL_Shutdown();
	SV_Shutdown();

	LightEditor_Shutdown();
	Skybox_Destroy();
	Phys_Shutdown();
	Net_Shutdown();
}


// disabled cause for some reason, it could jump to the WindowProc function mid frame and call this 
#define CH_LIVE_WINDOW_RESIZE 0


void WindowResizeCallback()
{
#if CH_LIVE_WINDOW_RESIZE
	PROF_SCOPE_NAMED( "WindowResizeCallback" );

	ImGui::NewFrame();
	ImGui_ImplSDL2_NewFrame();
	Graphics_NewFrame();

	Game_UpdateProjection();

	Graphics_Reset();

	if ( !( SDL_GetWindowFlags( render->GetWindow() ) & SDL_WINDOW_MINIMIZED ) && r_render )
	{
		gui->Update( 0.f );
		Graphics_Present();
	}
	else
	{
		ImGui::EndFrame();
	}
#endif
}


bool Game_Init()
{
	// Startup the Game Input System
	Input_Init();

	Game_UpdateProjection();

	if ( !Graphics_Init() )
		return false;

	Sys_SetResizeCallback( WindowResizeCallback );

	srand( ( unsigned int )time( 0 ) );  // setup rand(  )

#if AUDIO_OPENAL
	hAudioMusic = audio->RegisterChannel( "Music" );
#endif

	Phys_Init();
	LightEditor_Init(); // TODO: when tools are made, move this there

	// Init Entity Component Registry
	Ent_RegisterBaseComponents();
	PlayerManager::RegisterComponents();

	if ( !Net_Init() )
	{
		Log_Error( "Failed to init networking\n" );
		return false;
	}

	// TODO: is this actually even needed if we are only connecting to other servers?
	if ( !SV_Init() )
	{
		Log_Error( "Failed to init server\n" );
		return false;
	}

	if ( !CL_Init() )
	{
		Log_Error( "Failed to init client\n" );
		return false;
	}

#if 0
	entities = new EntityManager;
	GetEntitySystem()->Init();

	players = GetEntitySystem()->RegisterSystem<PlayerManager>();
	players->Init();

	gLocalPlayer = players->Create();

	// mark this as the local player
	auto& playerInfo = GetEntitySystem()->GetComponent< CPlayerInfo >( gLocalPlayer );
	playerInfo.aIsLocalPlayer = true;

	players->Spawn( gLocalPlayer );
#endif

	Log_Msg( "Game Loaded!\n" );
	return true;
}


void Game_Update( float frameTime )
{
	PROF_SCOPE();

	{
		PROF_SCOPE_NAMED( "Imgui New Frame" );
		ImGui::NewFrame();
		ImGui_ImplSDL2_NewFrame();

		ImGuizmo::BeginFrame();
		ImGuizmo::SetDrawlist();
	}

	Graphics_NewFrame();

	Game_UpdateGame( frameTime );

	// WORLD GLOBAL AXIS
	// if ( dbg_global_axis )
	// {
	// 	graphics->DrawLine( {0, 0, 0}, {dbg_global_axis_size.GetFloat(), 0, 0}, {1, 0, 0});
	// 	graphics->DrawLine( {0, 0, 0}, {0, dbg_global_axis_size.GetFloat(), 0}, {0, 1, 0} );
	// 	graphics->DrawLine( {0, 0, 0}, {0, 0, dbg_global_axis_size.GetFloat()}, {0, 0, 1} );
	// }

	LightEditor_Update();

	gui->Update( frameTime );

	if ( !( SDL_GetWindowFlags( render->GetWindow() ) & SDL_WINDOW_MINIMIZED ) && r_render )
	{
		Graphics_Present();
	}
	else
	{
		PROF_SCOPE_NAMED( "Imgui End Frame" );
		ImGui::EndFrame();
	}

	Con_Update();
}


bool Game_InMap()
{
	return MapManager_HasMap();
}


void Game_UpdateGame( float frameTime )
{
	PROF_SCOPE();

	gFrameTime = frameTime * host_timescale;

	Game_HandleSystemEvents();

	Input_Update();

	Game_CheckPaused();

	if ( gPaused )
	{
		//ResetInputs(  );
		//players->Update( 0.f );
		//return;
		gFrameTime = 0.f;
	}

	gCurTime += gFrameTime;

	if ( gServerData.aActive )
		SV_Update( gFrameTime );

	// when do i call these lol
	CL_Update( gFrameTime );

	GetPlayers()->apMove->DisplayPlayerStats( gLocalPlayer );

	Game_SetupModels( gFrameTime );
	Game_ResetInputs();
	Game_UpdateAudio();
}


void Game_SetPaused( bool paused )
{
	gPaused = paused;
}


bool Game_IsPaused()
{
	return gPaused;
}


void Game_CheckPaused()
{
	// TODO: reenable this for when in single player, or we allow server pausing
#if 0
	bool wasPaused = gPaused;
	gPaused = gui->IsConsoleShown();

	if ( wasPaused != gPaused )
	{
		SDL_SetRelativeMouseMode( (SDL_bool)!gPaused );

		if ( gPaused )
		{
			CenterMouseOnScreen();
		}
	}

	audio->SetPaused( gPaused );
#endif
}


// will be used in the future for when updating bones and stuff
void Game_SetupModels( float frameTime )
{
	PROF_SCOPE();
	TEST_UpdateProtos( frameTime );
}


void Game_ResetInputs()
{
}


void Game_UpdateAudio()
{
	if ( gPaused )
		return;

	TEST_UpdateAudio();
}


void Game_HandleSystemEvents()
{
	PROF_SCOPE();

	static std::vector< SDL_Event >* events = input->GetEvents();

	for ( auto& e: *events )
	{
		switch (e.type)
		{
			// TODO: remove this and use pause concmd, can't at the moment as binds are only parsed when game is active, hmm
			case SDL_KEYDOWN:
			{
				if ( e.key.keysym.sym == SDLK_BACKQUOTE || e.key.keysym.sym == SDLK_ESCAPE )
					gui->ShowConsole();
			
				break;
			}

			case SDL_WINDOWEVENT:
			{
				switch (e.window.event)
				{
					case SDL_WINDOWEVENT_DISPLAY_CHANGED:
					{
						// static float prevDPI = 96.f;
						// 
						// float dpi, hdpi, vdpi;
						// int test = SDL_GetDisplayDPI( e.window.data1, &dpi, &hdpi, &vdpi );
						// 
						// ImGuiStyle& style = ImGui::GetStyle();
						// style.ScaleAllSizes( dpi / prevDPI );
						// 
						// prevDPI = dpi;
					}

					case SDL_WINDOWEVENT_SIZE_CHANGED:
					{
						// Log_Msg( "SDL_WINDOWEVENT_SIZE_CHANGED\n" );
						Game_UpdateProjection();
						Graphics_Reset();
						break;
					}
					case SDL_WINDOWEVENT_EXPOSED:
					{
						// Log_Msg( "SDL_WINDOWEVENT_EXPOSED\n" );
						// TODO: RESET RENDERER AND DRAW ONTO WINDOW WINDOW !!!
						break;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
	}
}


void Game_SetView( const glm::mat4& srViewMat )
{
	PROF_SCOPE();

	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	gView.aViewMat = srViewMat;
	gView.ComputeProjection( width, height );

	// um
	gViewInfo[ 0 ].aProjection = gView.aProjMat;
	gViewInfo[ 0 ].aView       = gView.aViewMat;
	gViewInfo[ 0 ].aNearZ      = gView.aNearZ;
	gViewInfo[ 0 ].aFarZ       = gView.aFarZ;

	Graphics_SetViewProjMatrix( gView.aProjViewMat );
	gViewInfoUpdate  = true;

	auto& io         = ImGui::GetIO();
	io.DisplaySize.x = width;
	io.DisplaySize.y = height;
}


void Game_UpdateProjection()
{
	PROF_SCOPE();

	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );
	gView.ComputeProjection( width, height );

	// um
	gViewInfo[ 0 ].aProjection = gView.aProjMat;
	gViewInfo[ 0 ].aView       = gView.aViewMat;
	gViewInfo[ 0 ].aNearZ      = gView.aNearZ;
	gViewInfo[ 0 ].aFarZ       = gView.aFarZ;

	Graphics_SetViewProjMatrix( gView.aProjViewMat );
	gViewInfoUpdate  = true;

	auto& io = ImGui::GetIO();
	io.DisplaySize.x = width;
	io.DisplaySize.y = height;
}

