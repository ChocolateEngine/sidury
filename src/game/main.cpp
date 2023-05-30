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

#include "tools/light_editor.h"

#include <SDL_system.h>
#include <SDL_hints.h>

#include <algorithm>


IGuiSystem*      gui          = nullptr;
IRender*         render       = nullptr;
IInputSystem*    input        = nullptr;
IAudioSystem*    audio        = nullptr;

static bool      gPaused      = true;
float            gFrameTime   = 0.f;
double           gCurTime     = 0.0;  // i could make this a size_t, and then just have it be every 1000 is 1 second

Entity           gLocalPlayer = ENT_INVALID;
ViewportCamera_t gView{};

extern ConVar r_nearz, r_farz, r_fov;

CONVAR( phys_friction, 10 );
CONVAR( dbg_global_axis, 1 );
CONVAR( dbg_global_axis_size, 15 );
CONVAR( r_render, 1 );

extern ConVar host_timescale;


CON_COMMAND( pause )
{
	gui->ShowConsole();
}


void Game_Shutdown()
{
	TEST_Shutdown();

	LightEditor_Shutdown();
	Skybox_Destroy();
	Phys_Shutdown();
	Net_Shutdown();
}


#ifdef _WIN32
  #define WM_PAINT 0x000F

void Game_WindowMessageHook( void* userdata, void* hWnd, unsigned int message, Uint64 wParam, Sint64 lParam )
{
	switch ( message )
	{
		case WM_PAINT:
		{
			// Log_Msg( "WM_PAINT\n" );
			break;
		}

		default:
		{
			// static size_t i = 0;
			// Log_MsgF( "erm %zd\n", i++ );
			break;
		}
	}
}
#endif


bool Game_Init()
{
	// Startup the Game Input System
	Input_Init();

	Game_UpdateProjection();

	if ( !Graphics_Init() )
		return false;

#ifdef _WIN32
	SDL_SetWindowsMessageHook( Game_WindowMessageHook, nullptr );
#endif

	srand( ( unsigned int )time( 0 ) );  // setup rand(  )

#if AUDIO_OPENAL
	hAudioMusic = audio->RegisterChannel( "Music" );
#endif

	Phys_Init();
	LightEditor_Init();
	Skybox_Init();
	Net_Init();

	entities = new EntityManager;
	entities->Init();

	players = entities->RegisterSystem<PlayerManager>();
	players->Init();

	gLocalPlayer = players->Create();

	// mark this as the local player
	auto& playerInfo = entities->GetComponent< CPlayerInfo >( gLocalPlayer );
	playerInfo.aIsLocalPlayer = true;

	players->Spawn( gLocalPlayer );

	Log_Msg( "Game Loaded!\n" );
	return true;
}


void CenterMouseOnScreen()
{
	int w, h;
	SDL_GetWindowSize( render->GetWindow(), &w, &h );
	SDL_WarpMouseInWindow( render->GetWindow(), w / 2, h / 2 );
}


bool Game_InMap()
{
	return MapManager_HasMap();
}


void Game_Update( float frameTime )
{
	PROF_SCOPE();

	input->Update( frameTime );

	{
		PROF_SCOPE_NAMED( "Imgui New Frame" );
		ImGui::NewFrame();
		ImGui_ImplSDL2_NewFrame();
	}

	Game_UpdateGame( frameTime );

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


static ImVec2 ImVec2Mul( ImVec2 vec, float sScale )
{
	vec.x *= sScale;
	vec.y *= sScale;
	return vec;
}


static ImVec2 ImVec2MulMin( ImVec2 vec, float sScale, float sMin = 1.f )
{
	vec.x *= sScale;
	vec.y *= sScale;

	vec.x = glm::max( sMin, vec.x );
	vec.y = glm::max( sMin, vec.y );

	return vec;
}


static void ScaleImGui( float sScale )
{
	static ImGuiStyle baseStyle = ImGui::GetStyle();

	ImGuiStyle& style = ImGui::GetStyle();

	style.ChildRounding             = baseStyle.ChildRounding * sScale;
	style.WindowRounding            = baseStyle.WindowRounding * sScale;
	style.PopupRounding             = baseStyle.PopupRounding * sScale;
	style.FrameRounding             = baseStyle.FrameRounding * sScale;
	style.IndentSpacing             = baseStyle.IndentSpacing * sScale;
	style.ColumnsMinSpacing         = baseStyle.ColumnsMinSpacing * sScale;
	style.ScrollbarSize             = baseStyle.ScrollbarSize * sScale;
	style.ScrollbarRounding         = baseStyle.ScrollbarRounding * sScale;
	style.GrabMinSize               = baseStyle.GrabMinSize * sScale;
	style.GrabRounding              = baseStyle.GrabRounding * sScale;
	style.LogSliderDeadzone         = baseStyle.LogSliderDeadzone * sScale;
	style.TabRounding               = baseStyle.TabRounding * sScale;
	style.MouseCursorScale          = baseStyle.MouseCursorScale * sScale;
	style.TabMinWidthForCloseButton = ( baseStyle.TabMinWidthForCloseButton != FLT_MAX ) ? ( baseStyle.TabMinWidthForCloseButton * sScale ) : FLT_MAX;

	style.WindowPadding             = ImVec2Mul( baseStyle.WindowPadding, sScale );
	style.WindowMinSize             = ImVec2MulMin( baseStyle.WindowMinSize, sScale );
	style.FramePadding              = ImVec2Mul( baseStyle.FramePadding, sScale );
	style.ItemSpacing               = ImVec2Mul( baseStyle.ItemSpacing, sScale );
	style.ItemInnerSpacing          = ImVec2Mul( baseStyle.ItemInnerSpacing, sScale );
	style.CellPadding               = ImVec2Mul( baseStyle.CellPadding, sScale );
	style.TouchExtraPadding         = ImVec2Mul( baseStyle.TouchExtraPadding, sScale );
	style.DisplayWindowPadding      = ImVec2Mul( baseStyle.DisplayWindowPadding, sScale );
	style.DisplaySafeAreaPadding    = ImVec2Mul( baseStyle.DisplaySafeAreaPadding, sScale );
}


void Game_UpdateGame( float frameTime )
{
	PROF_SCOPE();

	gFrameTime = frameTime * host_timescale;

	Graphics_NewFrame();

	ImGuizmo::BeginFrame();
	ImGuizmo::SetDrawlist();

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

	MapManager_Update();

#if 0
	// what
	if ( ImGui::Begin( "What" ) )
	{
		ImGuiStyle& style = ImGui::GetStyle();

		static float imguiScale = 1.f;

		if ( ImGui::SliderFloat( "Scale", &imguiScale, 0.25f, 4.f, "%.4f", 1.f ) )
		{
			ScaleImGui( imguiScale );
		}

		// if ( ImGui::Button( "x 2.0" ) )
		// {
		// 	ScaleImGui( 2.0f );
		// }
		// 
		// else if ( ImGui::Button( "x 0.1" ) )
		// {
		// 	ScaleImGui( 1.1f );
		// }
		// 
		// else if ( ImGui::Button( "x 0.9" ) )
		// {
		// 	ScaleImGui( 0.9f );
		// }
	}

	ImGui::End();
#endif

	// WORLD GLOBAL AXIS
	// if ( dbg_global_axis )
	// {
	// 	graphics->DrawLine( {0, 0, 0}, {dbg_global_axis_size.GetFloat(), 0, 0}, {1, 0, 0});
	// 	graphics->DrawLine( {0, 0, 0}, {0, dbg_global_axis_size.GetFloat(), 0}, {0, 1, 0} );
	// 	graphics->DrawLine( {0, 0, 0}, {0, 0, dbg_global_axis_size.GetFloat()}, {0, 0, 1} );
	// }

	players->Update( gFrameTime );

	Phys_Simulate( physenv, gFrameTime );

	TEST_EntUpdate();

	// stupid
	for ( auto& player: players->aPlayerList )
	{
		players->apMove->UpdatePosition( player );
	}

	players->apMove->DisplayPlayerStats( gLocalPlayer );

	Game_SetupModels( gFrameTime );
	Game_ResetInputs();
	Game_UpdateAudio();

	if ( input->WindowHasFocus() && !gPaused )
	{
		CenterMouseOnScreen();
	}
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


