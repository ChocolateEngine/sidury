#include "main.h"
#include "core/systemmanager.h"
#include "core/asserts.h"

#include "iinput.h"
#include "igui.h"
#include "render/irender.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"

#include "util.h"
#include "game_physics.h"
#include "igraphics.h"
#include "mapmanager.h"
#include "inputsystem.h"
#include "skybox.h"
#include "editor_view.h"
#include "entity_editor.h"
#include "importer.h"

#include <SDL_system.h>
#include <SDL_hints.h>

#include <algorithm>


IGuiSystem*      gui          = nullptr;
IRender*         render       = nullptr;
IInputSystem*    input        = nullptr;
IAudioSystem*    audio        = nullptr;
IGraphics*       graphics     = nullptr;

static bool      gPaused      = false;
float            gFrameTime   = 0.f;

// TODO: make gRealTime and gGameTime
// real time is unmodified time since engine launched, and game time is time affected by host_timescale and pausing
double           gCurTime     = 0.0;  // i could make this a size_t, and then just have it be every 1000 is 1 second

extern bool      gRunning;
extern ConVar    r_nearz, r_farz, r_fov;
extern ConVar    host_timescale;

// blech
static u32       gMainViewportIndex = UINT32_MAX;

CONVAR( phys_friction, 10 );
CONVAR( dbg_global_axis, 1 );
CONVAR( dbg_global_axis_size, 15 );
CONVAR( r_render, 1 );


int                             gMainMenuBarHeight    = 0.f;
static bool                     gShowQuitConfirmation = false;


EditorData_t                    gEditorData{};

ChHandle_t                      gEditorContextIdx = CH_INVALID_EDITOR_CONTEXT;
ResourceList< EditorContext_t > gEditorContexts;


// CON_COMMAND( pause )
// {
// 	gui->ShowConsole();
// }


void Game_Shutdown()
{
	Log_Msg( "TODO: Save all open maps\n" );

	Resource_Shutdown();
	Importer_Shutdown();
	EntEditor_Shutdown();
	Skybox_Destroy();
	Phys_Shutdown();
}


void DrawQuitConfirmation();

void Main_DrawMenuBar()
{
	if ( gShowQuitConfirmation )
	{
		DrawQuitConfirmation();
	}

	if ( ImGui::BeginMainMenuBar() )
	{
		if ( ImGui::BeginMenu( "File" ) )
		{
			if ( ImGui::MenuItem( "New" ) )
			{
				Editor_CreateContext( nullptr );
			}

			if ( ImGui::MenuItem( "Load" ) )
			{
			}

			if ( ImGui::MenuItem( "Save" ) )
			{
			}

			if ( ImGui::MenuItem( "Close" ) )
			{
			}

			ImGui::Separator();

			if ( ImGui::BeginMenu( "Open Recent" ) )
			{
				ImGui::EndMenu();
			}

			ImGui::Separator();

			if ( ImGui::MenuItem( "Quit" ) )
			{
				gShowQuitConfirmation = true;
			}

			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "Edit" ) )
		{
			if ( ImGui::MenuItem( "Bindings" ) )
			{
			}

			ImGui::EndMenu();
		}

		if ( ImGui::BeginMenu( "View" ) )
		{
			if ( ImGui::MenuItem( "Entity Editor", nullptr, true ) )
			{
			}

			ImGui::EndMenu();
		}

		// I would like tabs eventually
		if ( ImGui::BeginMenu( "Map List" ) )
		{
			for ( u32 i = 0; i < gEditorContexts.aHandles.size(); i++ )
			{
				EditorContext_t* context = nullptr;
				if ( !gEditorContexts.Get( gEditorContexts.aHandles[ i ], &context ) )
					continue;

				ImGui::PushID( i );
				bool selected = gEditorContexts.aHandles[ i ] == gEditorContextIdx;
				if ( ImGui::MenuItem( context->aMap.aMapPath.empty() ? "Unsaved Map" : context->aMap.aMapPath.c_str(), nullptr, &selected ) )
				{
					Editor_SetContext( gEditorContexts.aHandles[ i ] );
				}
				ImGui::PopID();
			}

			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	auto yeah          = ImGui::GetItemRectSize();
	gMainMenuBarHeight = yeah.y;
}


// disabled cause for some reason, it could jump to the WindowProc function mid frame and call this 
#define CH_LIVE_WINDOW_RESIZE 1


void WindowResizeCallback()
{
#if CH_LIVE_WINDOW_RESIZE
	PROF_SCOPE_NAMED( "WindowResizeCallback" );

	ImGui::NewFrame();
	ImGui_ImplSDL2_NewFrame();
	graphics->NewFrame();

	graphics->Reset();

	EntEditor_Update( 0.f );

	if ( !( SDL_GetWindowFlags( render->GetWindow() ) & SDL_WINDOW_MINIMIZED ) && r_render )
	{
		gui->Update( 0.f );
		Main_DrawMenuBar();
		EntEditor_DrawUI();

		Game_UpdateProjection();

		graphics->Present();
	}
	else
	{
		ImGui::EndFrame();
	}
#endif
}


bool Game_Init()
{
	//gpEditorContext = &gEditorContexts.emplace_back();

	// Startup the Game Input System
	Input_Init();

	if ( !Importer_Init() )
	{
		Log_ErrorF( "Failed to init Importer\n" );
		return false;
	}

	// Create the Main Viewport - TODO: use this more across the game code
	gMainViewportIndex = graphics->CreateViewport();

	Game_UpdateProjection();

	Sys_SetResizeCallback( WindowResizeCallback );

	srand( ( unsigned int )time( 0 ) );  // setup rand(  )

#if AUDIO_OPENAL
	// hAudioMusic = audio->RegisterChannel( "Music" );
#endif

	Phys_Init();

	EntEditor_Init();

	Log_Msg( "Editor Loaded!\n" );
	return true;
}


static bool        gInSkyboxSelect   = false;
static std::string gSkyboxSelectPath = "";


// return true if model selected
std::string DrawSkyboxSelectionWindow()
{
	if (!ImGui::Begin("Model Selection Window"))
	{
		ImGui::End();
		return "";
	}

	if (ImGui::Button("Close"))
	{
		gInSkyboxSelect = false;
		gSkyboxSelectPath.clear();
	}

	auto fileList = FileSys_ScanDir(gSkyboxSelectPath, ReadDir_AbsPaths);

	for (std::string_view file : fileList)
	{
		bool isDir = FileSys_IsDir(file.data(), true);

		if (!isDir)
		{
			bool model = file.ends_with(".cmt");

			if (!model)
				continue;
		}

		if (ImGui::Selectable(file.data()))
		{
			if (isDir)
			{
				gSkyboxSelectPath = FileSys_CleanPath(file);
			}
			else
			{
				ImGui::End();
				return file.data();
			}
		}
	}

	ImGui::End();

	return "";
}


void Game_Update( float frameTime )
{
	PROF_SCOPE();

	{
		PROF_SCOPE_NAMED( "Imgui New Frame" );
		ImGui::NewFrame();
		ImGui_ImplSDL2_NewFrame();
	}

	graphics->NewFrame();

	Game_UpdateGame( frameTime );

	gui->Update( frameTime );

	Entity_Update();
	EntEditor_Update( frameTime );

	if ( !( SDL_GetWindowFlags( render->GetWindow() ) & SDL_WINDOW_MINIMIZED ) && r_render )
	{
		EntEditor_DrawUI();
		graphics->Present();
	}
	else
	{
		PROF_SCOPE_NAMED( "Imgui End Frame" );
		ImGui::EndFrame();
	}

	Con_Update();

	Resource_Update();
}


static void DrawQuitConfirmation()
{
	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	ImGui::SetNextWindowSize({ (float)width, (float)height });
	ImGui::SetNextWindowPos({0.f, 0.f});

	if (ImGui::Begin("FullScreen Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav))
	{
		ImGui::SetNextWindowFocus();

		ImGui::SetNextWindowSize({ 250, 60 });

		ImGui::SetNextWindowPos( { (width / 2.f) - 125.f, (height / 2.f) - 30.f } );

		if ( !ImGui::Begin( "Quit Editor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove ) )
		{
			ImGui::End();
			return;
		}

		ImGui::Text( "Are you sure you want to quit?" );

		ImGui::Separator();

		if ( ImGui::Button( "Yes", {50, 0} ) )
		{
			Con_QueueCommand( "quit" );
		}

		ImGui::SameLine( 190 );

		if ( ImGui::Button( "No", {50, 0} ) )
		{
			gShowQuitConfirmation = false;
		}

		ImGui::End();
	}

	ImGui::End();
}


void Game_UpdateGame( float frameTime )
{
	PROF_SCOPE();

	gFrameTime = frameTime * host_timescale;

	Game_HandleSystemEvents();

	Input_Update();

	if ( gPaused )
	{
		gFrameTime = 0.f;
	}

	gCurTime += gFrameTime;

	EditorView_Update();

	Main_DrawMenuBar();
}


void Game_SetPaused( bool paused )
{
	gPaused = paused;
}


bool Game_IsPaused()
{
	return gPaused;
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
						graphics->Reset();
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


// TODO: this will not work for multiple camera viewports
void Game_SetView( const glm::mat4& srViewMat )
{
	if ( EditorContext_t* context = Editor_GetContext() )
		context->aView.aViewMat = srViewMat;
}


// TODO: this will not work for multiple camera viewports
void Game_UpdateProjection()
{
	PROF_SCOPE();

	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	auto& io          = ImGui::GetIO();
	io.DisplaySize.x  = width;
	io.DisplaySize.y  = height;

	// HACK
	extern glm::vec2 gEntityListSize;

	int              offsetX = gEntityListSize.x;
	int              offsetY = gMainMenuBarHeight;

	int              newWidth  = width - offsetX;
	int              newHeight = height - offsetY;

	glm::mat4        projMat   = Util_ComputeProjection( newWidth, height, r_nearz, r_farz, r_fov );

	// update each viewport for each context
#if 0
	for ( u32 i = 0; i < gEditorContexts.aHandles.size(); i++ )
	{
		EditorContext_t* context = nullptr;
		if ( !gEditorContexts.Get( gEditorContexts.aHandles[ i ], &context ) )
			continue;

		ViewportShader_t* viewport   = graphics->GetViewportData( context->aView.aViewportIndex );
		
		context->aView.aResolution.x = width;
		context->aView.aResolution.x = height;
		context->aView.aProjMat      = projMat;
		context->aView.aProjViewMat  = projMat * context->aView.aViewMat;

		if ( !viewport )
			continue;

		viewport->aProjView   = context->aView.aProjViewMat;
		viewport->aProjection = context->aView.aProjMat;
		viewport->aView       = context->aView.aViewMat;

		viewport->aNearZ      = r_nearz;
		viewport->aFarZ       = r_farz;
		viewport->aSize       = { width, height };
	}
#else
	// only 1 viewport can be seen right now
	EditorContext_t* context = Editor_GetContext();
	if ( !context )
		return;

	ViewportShader_t* viewport   = graphics->GetViewportData( 0 );

	context->aView.aResolution.x = newWidth;
	context->aView.aResolution.y = newHeight;
	context->aView.aOffset.x     = offsetX;
	context->aView.aOffset.y     = offsetY;
	context->aView.aProjMat      = projMat;
	context->aView.aProjViewMat  = projMat * context->aView.aViewMat;

	if ( !viewport )
		return;

	viewport->aProjView   = context->aView.aProjViewMat;
	viewport->aProjection = context->aView.aProjMat;
	viewport->aView       = context->aView.aViewMat;

	viewport->aNearZ      = r_nearz;
	viewport->aFarZ       = r_farz;
	viewport->aSize       = { newWidth, height };
	viewport->aOffset     = { offsetX, offsetY };
#endif

	graphics->SetViewportUpdate( true );
}


// ----------------------------------------------------------------------------


ChHandle_t Editor_CreateContext( EditorContext_t** spContext )
{
	if ( gEditorContexts.size() >= CH_MAX_EDITOR_CONTEXTS )
	{
		Log_ErrorF( "Max Editor Contexts Hit: Max of %zd\n", CH_MAX_EDITOR_CONTEXTS );
		return CH_INVALID_EDITOR_CONTEXT;
	}

	EditorContext_t* context = nullptr;
	ChHandle_t       handle  = gEditorContexts.Create( &context );

	if ( handle == CH_INVALID_HANDLE )
		return CH_INVALID_HANDLE;

	Editor_SetContext( handle );

	if ( spContext )
		*spContext = context;

	// lazy
	Game_UpdateProjection();

	return gEditorContextIdx;
}


void Editor_FreeContext( ChHandle_t sContext )
{
	gEditorContexts.Remove( sContext );
}


EditorContext_t* Editor_GetContext()
{
	if ( gEditorContextIdx == CH_INVALID_EDITOR_CONTEXT )
		return nullptr;

	EditorContext_t* context = nullptr;
	if ( !gEditorContexts.Get( gEditorContextIdx, &context ) )
		return nullptr;

	return context;
}


void Editor_SetContext( ChHandle_t sContext )
{
	EditorContext_t* context = nullptr;
	if ( !gEditorContexts.Get( sContext, &context ) )
	{
		Log_ErrorF( "Tried to set invalid editor context handle: %zd\n", sContext );
		return;
	}

	ChHandle_t       lastContextIdx = gEditorContextIdx;
	EditorContext_t* lastContext    = nullptr;
	gEditorContextIdx               = sContext;

	// Hide Entities of Last Context (TODO: change this if you implement some quick hide or vis group system like hammer has)
	if ( gEditorContexts.Get( lastContextIdx, &lastContext ) )
	{
		for ( ChHandle_t ent : lastContext->aMap.aMapEntities )
		{
			Entity_SetEntityVisible( ent, false );
		}
	}

	// Show Entities in New Context
	for ( ChHandle_t ent : context->aMap.aMapEntities )
	{
		Entity_SetEntityVisible( ent, true );
	}

	// Set Skybox Material
	if ( context->aMap.aMapInfo )
		Skybox_SetMaterial( context->aMap.aMapInfo->skybox );
	else
		Skybox_SetMaterial( "" );

	// TODO: once multiple windows for the 3d view are supported, we will change focus of them here
}

