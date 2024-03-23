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


IGuiSystem*       gui          = nullptr;
IRender*          render       = nullptr;
IInputSystem*     input        = nullptr;
IAudioSystem*     audio        = nullptr;
IGraphics*        graphics     = nullptr;
IRenderSystemOld* renderOld    = nullptr;

static bool      gPaused      = false;
float            gFrameTime   = 0.f;

// TODO: make gRealTime and gGameTime
// real time is unmodified time since engine launched, and game time is time affected by host_timescale and pausing
double           gCurTime     = 0.0;  // i could make this a size_t, and then just have it be every 1000 is 1 second

extern bool      gRunning;
extern ConVar    r_nearz, r_farz, r_fov;
extern ConVar    host_timescale;

// blech
u32              gMainViewportHandle = UINT32_MAX;

CONVAR( phys_friction, 10 );
CONVAR( dbg_global_axis, 1 );
CONVAR( dbg_global_axis_size, 15 );
CONVAR( r_render, 1 );
CONVAR( ui_show_imgui_demo, 0 );

extern ConVar                   editor_gizmo_scale_enabled;
extern ConVar                   editor_gizmo_scale;


int                             gMainMenuBarHeight    = 0.f;
static bool                     gShowQuitConfirmation = false;
static bool                     gShowSettingsMenu     = false;


EditorData_t                    gEditorData{};

ChHandle_t                      gEditorContextIdx = CH_INVALID_EDITOR_CONTEXT;
ResourceList< EditorContext_t > gEditorContexts;


// CON_COMMAND( pause )
// {
// 	gui->ShowConsole();
// }


IPhysicsObject*                 reallyCoolObject = nullptr;


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


void Main_DrawGraphicsSettings()
{
	auto windowSize = ImGui::GetWindowSize();
	windowSize.x -= 60;
	windowSize.y -= 60;

	//ImGui::SetNextWindowContentSize( windowSize );

	if ( !ImGui::BeginChild( "Graphics Settings", {}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border ) )
	{
		ImGui::EndChild();
		return;
	}

	static ConVarRef r_msaa( "r_msaa" );
	static ConVarRef r_msaa_samples( "r_msaa_samples" );
	static ConVarRef r_msaa_textures( "r_msaa_textures" );
	static ConVarRef r_fov( "r_fov" );

	bool             msaa_textures = r_msaa_textures.GetBool();
	float            fov           = r_fov.GetFloat();

	if ( ImGui::SliderFloat( "FOV", &fov, 0.1f, 179.9f ) )
	{
		std::string fovStr = ToString( fov );
		Con_QueueCommandSilent( "r_fov " + fovStr );
	}

	std::string msaaPreview = r_msaa.GetBool() ? ToString( r_msaa_samples ) + "X" : "Off";
	if ( ImGui::BeginCombo( "MSAA", msaaPreview.c_str() ) )
	{
		if ( ImGui::Selectable( "Off", !r_msaa.GetBool() ) )
		{
			Con_QueueCommandSilent( "r_msaa 0" );
		}

		int maxSamples = render->GetMaxMSAASamples();

		// TODO: check what your graphics card actually supports
		if ( maxSamples >= 2 && ImGui::Selectable( "2X", r_msaa.GetBool() && r_msaa_samples.GetFloat() == 2 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 2" );
		}

		if ( maxSamples >= 4 && ImGui::Selectable( "4X", r_msaa.GetBool() && r_msaa_samples.GetFloat() == 4 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 4" );
		}

		if ( maxSamples >= 8 && ImGui::Selectable( "8X", r_msaa.GetBool() && r_msaa_samples.GetFloat() == 8 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 8" );
		}

		if ( maxSamples >= 16 && ImGui::Selectable( "16X", r_msaa.GetBool() && r_msaa_samples.GetFloat() == 16 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 16" );
		}

		if ( maxSamples >= 32 && ImGui::Selectable( "32X", r_msaa.GetBool() && r_msaa_samples.GetFloat() == 32 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 32" );
		}

		if ( maxSamples >= 64 && ImGui::Selectable( "64X", r_msaa.GetBool() && r_msaa_samples.GetFloat() == 64 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 64" );
		}

		ImGui::EndCombo();
	}

	if ( ImGui::Checkbox( "MSAA Textures/Sample Shading - Very Expensive", &msaa_textures ) )
	{
		if ( msaa_textures )
			Con_QueueCommandSilent( "r_msaa_textures 1" );
		else
			Con_QueueCommandSilent( "r_msaa_textures 0" );
	}

	//ImGui::SetWindowSize( windowSize );

	ImGui::EndChild();
}


void Main_DrawInputSettings()
{
	if ( !ImGui::BeginChild( "Input", {}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border ) )
	{
		ImGui::EndChild();
		return;
	}

	if ( ImGui::BeginTable( "Bindings", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable ) )
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex( 0 );

		ImGui::Text( "Keys" );

		ImGui::TableSetColumnIndex( 1 );
		ImGui::Text( "Binding" );

		for ( u16 binding = 0; binding < EBinding_Count; binding++ )
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 );

			const char*         bindingStr = Input_BindingToStr( (EBinding)binding );
			const ButtonList_t* buttonList = Input_GetKeyBinding( (EBinding)binding );

			if ( buttonList )
			{
				ImGui::Text( Input_ButtonListToStr( buttonList ).c_str() );
			}

			ImGui::TableSetColumnIndex( 1 );
			ImGui::Text( bindingStr );
		}

		ImGui::EndTable();
	}

	ImGui::EndChild();
}


void Main_DrawSettingsMenu()
{
	if ( !gShowSettingsMenu )
		return;

	ImGui::SetNextWindowSizeConstraints( { 200, 200 }, {4000, 4000} );

	if ( !ImGui::Begin( "Settings Menu" ) )
	{
		ImGui::End();
		return;
	}

	if ( ImGui::BeginTabBar( "settings tabs" ) )
	{
		if ( ImGui::BeginTabItem( "Input" ) )
		{
			Main_DrawInputSettings();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Graphics" ) )
		{
			Main_DrawGraphicsSettings();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Other" ) )
		{
			bool scaleEnabled = editor_gizmo_scale_enabled.GetBool();
			if ( ImGui::Checkbox( "Gizmo Scaling", &scaleEnabled ) )
			{
				if ( scaleEnabled )
					Con_QueueCommandSilent( "editor_gizmo_scale_enabled 1" );
				else
					Con_QueueCommandSilent( "editor_gizmo_scale_enabled 0" );
			}

			float scale = editor_gizmo_scale.GetFloat();
			if ( ImGui::DragFloat( "Gizmo Scale", &scale, 0.0005, 0.f, 0.1f, "%.6f" ) )
			{
				editor_gizmo_scale.SetValue( scale );
			}

			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImVec2 windowSize = ImGui::GetWindowSize();

	ImGui::SetCursorPosY( windowSize.y - 28 );

	if ( ImGui::Button( "Save" ) )
	{
		Con_Archive();
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Close" ) )
	{
		gShowSettingsMenu = false;
	}

	ImGui::End();
}


void Main_DrawMenuBar()
{
	if ( gShowQuitConfirmation )
	{
		DrawQuitConfirmation();
	}

	Main_DrawSettingsMenu();

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
			if ( ImGui::MenuItem( "Settings" ) )
			{
				gShowSettingsMenu = true;
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


void UpdateLoop( float frameTime, bool sResize )
{
	PROF_SCOPE();

	{
		PROF_SCOPE_NAMED( "Imgui New Frame" );
		ImGui::NewFrame();
		ImGui_ImplSDL2_NewFrame();
	}

	if ( ui_show_imgui_demo )
		ImGui::ShowDemoWindow();

	renderOld->NewFrame();

	if ( !sResize )
		Game_UpdateGame( frameTime );
	else
		renderOld->Reset();

	Phys_Simulate( GetPhysEnv(), frameTime );

	gui->Update( frameTime );

	if ( !sResize )
		Entity_Update();

	EntEditor_Update( frameTime );

	if ( reallyCoolObject )
	{
		glm::vec3 pos = reallyCoolObject->GetPos();
		glm::vec3 ang = reallyCoolObject->GetAng();

		graphics->DrawAxis( pos, ang, { 1, 1, 1 } );
	}

	if ( !( SDL_GetWindowFlags( render->GetWindow() ) & SDL_WINDOW_MINIMIZED ) && r_render )
	{
		Main_DrawMenuBar();
		EntEditor_DrawUI();

		if ( sResize )
			Game_UpdateProjection();

		renderOld->Present();
	}
	else
	{
		PROF_SCOPE_NAMED( "Imgui End Frame" );
		ImGui::EndFrame();
	}

	if ( sResize )
		return;

	Con_Update();

	// Resource_Update();
}


void WindowResizeCallback()
{
#if CH_LIVE_WINDOW_RESIZE
	UpdateLoop( 0.f, true );
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
	gMainViewportHandle = graphics->CreateViewport();

	Game_UpdateProjection();
#ifdef _WIN32
	Sys_SetResizeCallback( WindowResizeCallback );
#endif /* _WIN32  */

	srand( ( unsigned int )time( 0 ) );  // setup rand(  )

#if AUDIO_OPENAL
	// hAudioMusic = audio->RegisterChannel( "Music" );
#endif

	Phys_Init();

	Skybox_Init();
	EntEditor_Init();

	renderOld->EnableSelection( true, gMainViewportHandle );

	// TODO, mess with ImGui WantSaveIniSettings

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
	UpdateLoop( frameTime, false );

	// PROF_SCOPE();
	// 
	// {
	// 	PROF_SCOPE_NAMED( "Imgui New Frame" );
	// 	ImGui::NewFrame();
	// 	ImGui_ImplSDL2_NewFrame();
	// }
	// 
	// ImGui::ShowDemoWindow();
	// 
	// graphics->NewFrame();
	// 
	// Game_UpdateGame( frameTime );
	// 
	// gui->Update( frameTime );
	// 
	// Entity_Update();
	// EntEditor_Update( frameTime );
	// 
	// if ( !( SDL_GetWindowFlags( render->GetWindow() ) & SDL_WINDOW_MINIMIZED ) && r_render )
	// {
	// 	Main_DrawMenuBar();
	// 	EntEditor_DrawUI();
	// 	graphics->Present();
	// }
	// else
	// {
	// 	PROF_SCOPE_NAMED( "Imgui End Frame" );
	// 	ImGui::EndFrame();
	// }
	// 
	// Con_Update();
	// 
	// Resource_Update();
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
						renderOld->Reset();
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
	extern glm::vec2  gEntityListSize;
	extern glm::ivec2 gAssetBrowserSize;

	int               offsetX   = gEntityListSize.x;
	int               offsetY   = gMainMenuBarHeight;

	int               newWidth  = width - offsetX;
	int               newHeight = height - offsetY - gAssetBrowserSize.y;

	glm::mat4         projMat   = Util_ComputeProjection( newWidth, newHeight, r_nearz, r_farz, r_fov );

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
	ViewportShader_t* viewport  = graphics->GetViewportData( gMainViewportHandle );

	if ( !viewport )
		return;

	// only 1 viewport can be seen right now
	EditorContext_t* context = Editor_GetContext();

	viewport->aNearZ      = r_nearz;
	viewport->aFarZ       = r_farz;
	viewport->aSize       = { newWidth, newHeight };
	viewport->aOffset     = { offsetX, offsetY };
	viewport->aProjection = projMat;

	if ( context )
	{
		viewport->aView = context->aView.aViewMat;
	}
	else
	{
		viewport->aView = glm::mat4( 1.f );
	}

	viewport->aProjView = projMat * viewport->aView;

	// Update Context View Data
	if ( context )
	{
		context->aView.aResolution.x = newWidth;
		context->aView.aResolution.y = newHeight;
		context->aView.aOffset.x     = offsetX;
		context->aView.aOffset.y     = offsetY;
		context->aView.aProjMat      = projMat;
		context->aView.aProjViewMat  = viewport->aProjView;
	}
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
	Skybox_SetMaterial( context->aMap.aSkybox );

	// TODO: once multiple windows for the 3d view are supported, we will change focus of them here
}


// ------------------------------------------------------------------------


Ray Util_GetRayFromScreenSpace( glm::ivec2 mousePos, glm::vec3 origin, u32 viewportIndex )
{
	ViewportShader_t* viewport = graphics->GetViewportData( viewportIndex );

	if ( viewport == nullptr )
	{
		Log_ErrorF( "Invalid Viewport Index for \"%s\" func - %d", CH_FUNC_NAME, viewportIndex );
		return {};
	}

	// Apply viewport offsets to mouse pos
	mousePos.x -= viewport->aOffset.x;
	mousePos.y -= viewport->aOffset.y;

	Ray ray
	{
		.origin = origin,
		.dir = Util_GetRayFromScreenSpace( mousePos, viewport->aProjView, viewport->aSize ),
	};

	return ray;
}


// ------------------------------------------------------------------------
// TESTING


void CreatePhysObjectTest( const std::string& path )
{
	EditorContext_t* ctx = Editor_GetContext();
	
	if ( !ctx )
		return;

	IPhysicsShape* shape = GetPhysEnv()->LoadShape( path, PhysShapeType::StaticCompound );

	if ( !shape )
		return;

	PhysicsObjectInfo settings{};
	settings.aMotionType   = PhysMotionType::Kinematic;
	settings.aPos          = ctx->aView.aPos;
	settings.aAng          = ctx->aView.aAng;
	settings.aStartActive  = true;
	settings.aCustomMass   = true;
	settings.aMass         = 10.f;

	IPhysicsObject* object = GetPhysEnv()->CreateObject( shape, settings );

	reallyCoolObject       = object;
	return;
}


CONCMD( create_phys_object )
{
	if ( args.empty() )
		return;

	std::string path = args[ 0 ];
	CreatePhysObjectTest( path );
}


CONCMD( create_phys_object_chair )
{
	CreatePhysObjectTest( "riverhouse/controlroom_chair001a_physics.obj" );
}
