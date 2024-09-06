#include "main.h"
#include "core/systemmanager.h"
#include "core/asserts.h"
#include "core/app_info.h"
#include "core/build_number.h"

#include "iinput.h"
#include "igui.h"
#include "irender3.h"
#include "physics/iphysics.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"

#include "util.h"

#include <SDL_system.h>
#include <SDL_hints.h>

#include <algorithm>


int                       gWidth          = Args_RegisterF( 1280, "Width of the main window", 2, "-width", "-w" );
int                       gHeight         = Args_RegisterF( 720, "Height of the main window", 2, "-height", "-h" );
static bool               gMaxWindow      = Args_Register( "Maximize the main window", "-max" );
static bool               gSingleWindow   = Args_Register( "Single Window Mode, all tools will be rendered in tabs on the main window", "-single-window" );


SDL_Window*               gpWindow        = nullptr;
void*                     gpSysWindow     = nullptr;
ChHandle_t                gGraphicsWindow = CH_INVALID_HANDLE;

Toolkit                   toolkit;

// std::vector< AppWindow >  gWindows;
std::vector< LoadedTool > gTools;

IGuiSystem*               gui                = nullptr;
IRender3*                 render             = nullptr;
IInputSystem*             input              = nullptr;
IAudioSystem*             audio              = nullptr;
Ch_IPhysics*              ch_physics         = nullptr;

//ITool*                    toolMapEditor     = nullptr;
//ITool*                    toolMatEditor     = nullptr;


//bool                      toolMapEditorOpen = false;
//bool                      toolMatEditorOpen = false;

static bool               gPaused            = false;
float                     gFrameTime         = 0.f;

// TODO: make gRealTime and gGameTime
// real time is unmodified time since engine launched, and game time is time affected by host_timescale and pausing
double                    gCurTime           = 0.0;  // i could make this a size_t, and then just have it be every 1000 is 1 second

extern bool               gRunning;

u32                       gMainViewportHandle   = UINT32_MAX;

int                       gMainMenuBarHeight    = 0.f;
static bool               gShowQuitConfirmation = false;

CONVAR_FLOAT_EXT( host_timescale );

CONVAR_FLOAT( r_nearz, 0.01, "Camera Near Z Plane" );
CONVAR_FLOAT( r_farz, 10000, "Camera Far Z Plane" );
CONVAR_FLOAT( r_fov, 106, "FOV" );


//void Util_DrawTextureInfo( TextureInfo_t& info )
//{
//	ImGui::Text( "Name: %s", info.aName.size ? info.aName.data : "UNNAMED" );
//
//	if ( info.aPath.size )
//		ImGui::Text( info.aPath.data );
//
//	ImGui::Text( "%d x %d - %.6f MB", info.aSize.x, info.aSize.y, Util_BytesToMB( info.aMemoryUsage ) );
//	ImGui::Text( "Format: TODO" );
//	ImGui::Text( "Mip Levels: TODO" );
//	ImGui::Text( "GPU Index: %d", info.aGpuIndex );
//	ImGui::Text( "Ref Count: %d", info.aRefCount );
//}


void DrawQuitConfirmation();


void Main_DrawGraphicsSettings()
{
#if 0
	auto windowSize = ImGui::GetWindowSize();
	windowSize.x -= 60;
	windowSize.y -= 60;

	//ImGui::SetNextWindowContentSize( windowSize );

	if ( !ImGui::BeginChild( "Graphics Settings", {}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border ) )
	{
		ImGui::EndChild();
		return;
	}

	static ConVarData_t* r_msaa          = Con_GetConVarData( "r_msaa" );
	static ConVarData_t* r_msaa_samples  = Con_GetConVarData( "r_msaa_samples" );
	static ConVarData_t* r_msaa_textures = Con_GetConVarData( "r_msaa_textures" );
	static ConVarData_t* r_fov           = Con_GetConVarData( "r_fov" );

	bool                 msaa            = *r_msaa->aBool.apData;
	int                  msaa_samples    = *r_msaa_samples->aInt.apData;
	bool                 msaa_textures   = *r_msaa_textures->aBool.apData;
	float                fov             = *r_fov->aFloat.apData;

	if ( ImGui::SliderFloat( "FOV", &fov, 0.1f, 179.9f ) )
	{
		std::string    fovStr = ToString( fov );
		ch_string_auto fovCmd = ch_str_join( "r_fov ", 6, fovStr.data(), (s64)fovStr.size() );

		Con_QueueCommandSilent( fovCmd.data, fovCmd.size );
	}

	std::string msaaPreview = msaa ? ToString( msaa_samples ) + "X" : "Off";
	if ( ImGui::BeginCombo( "MSAA", msaaPreview.c_str() ) )
	{
		if ( ImGui::Selectable( "Off", !msaa ) )
		{
			Con_QueueCommandSilent( "r_msaa 0" );
		}

		int maxSamples = render->GetMaxMSAASamples();

		// TODO: check what your graphics card actually supports
		if ( maxSamples >= 2 && ImGui::Selectable( "2X", msaa && msaa_samples == 2 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 2" );
		}

		if ( maxSamples >= 4 && ImGui::Selectable( "4X", msaa && msaa_samples == 4 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 4" );
		}

		if ( maxSamples >= 8 && ImGui::Selectable( "8X", msaa && msaa_samples == 8 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 8" );
		}

		if ( maxSamples >= 16 && ImGui::Selectable( "16X", msaa && msaa_samples == 16 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 16" );
		}

		if ( maxSamples >= 32 && ImGui::Selectable( "32X", msaa && msaa_samples == 32 ) )
		{
			Con_QueueCommandSilent( "r_msaa 1; r_msaa_samples 32" );
		}

		if ( maxSamples >= 64 && ImGui::Selectable( "64X", msaa && msaa_samples == 64 ) )
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
#endif
}


void Main_DrawSettingsMenu()
{
	if ( !ImGui::BeginChild( "Settings Menu" ) )
	{
		ImGui::EndChild();
		return;
	}

	if ( ImGui::BeginTabBar( "settings tabs" ) )
	{
		if ( ImGui::BeginTabItem( "Graphics" ) )
		{
			Main_DrawGraphicsSettings();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Other" ) )
		{
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

	ImGui::EndChild();
}


LoadedTool* App_GetTool( const char* tool )
{
	if ( tool == nullptr )
		return nullptr;

	for ( u32 i = 0; i < gTools.size(); i++ )
	{
		if ( ch_str_equals( gTools[ i ].interface, tool ) )
			return &gTools[ i ];
	}

	Log_ErrorF( "Failed to find tool \"%s\"\n", tool );
	return nullptr;
}


AppWindow* App_GetToolWindow( const char* toolInterface )
{
	LoadedTool* tool = App_GetTool( toolInterface );

	if ( tool && tool->window != UINT16_MAX )
	{
		if ( tool->window < g_app_window_count )
			return &g_app_window[ tool->window ];
		else
			Log_ErrorF( "Tool Window Index out of bounds!\n" );
	}
	else
	{
		Log_ErrorF( "Failed to find window for tool!\n" );
	}

	Log_ErrorF( "Failed to find tool to get window from!\n" );
	return nullptr;
}


void App_LaunchTool( const char* toolInterface )
{
	if ( !toolInterface )
	{
		Log_ErrorF( "Tool not Loaded!\n" );
		return;
	}

	LoadedTool* tool = App_GetTool( toolInterface );

	if ( tool->running )
	{
		if ( tool->window != UINT16_MAX )
			Window_Focus( g_app_window[ tool->window ] );
		else
			Log_ErrorF( "TODO: FOCUS TOOL TAB!\n" );

		return;
	}

	// Launch this tool instead
	const char* windowName = tool->tool->GetName();
	AppWindow*  window     = nullptr;
	u16         window_id  = UINT16_MAX;

	if ( !gSingleWindow )
	{
		window_id = Window_Create( windowName );

		if ( window_id == UINT16_MAX )
		{
			Log_ErrorF( "Failed to open tool window: \"%s\"\n", windowName );
			return;
		}
	}

	ToolLaunchData launchData{};
	launchData.toolkit = &toolkit;

	if ( gSingleWindow )
	{
		launchData.window         = gpWindow;
		launchData.graphicsWindow = gGraphicsWindow;
	}
	else
	{
		launchData.window         = window->window;
		launchData.graphicsWindow = window->graphicsWindow;
	}

	if ( !tool->tool->Launch( launchData ) )
	{
		Log_ErrorF( "Failed to launch tool: \"%s\"\n", windowName );

		App_CloseTool( tool );
		return;
	}

	tool->window  = window_id;
	tool->running = true;
}


void App_CloseTool( LoadedTool* tool )
{
	if ( !tool )
		return;

	tool->tool->Close();
	tool->running   = false;

	u16 windowIndex = tool->window;

	if ( windowIndex != UINT16_MAX )
	{
		Window_Close( windowIndex );

		// shift the indexes of the windows down if they're above the index of the current window
		for ( u32 i = 0; i < gTools.size(); i++ )
		{
			if ( gTools[ i ].window == UINT16_MAX )
				continue;

			if ( gTools[ i ].window > windowIndex )
				gTools[ i ].window--;  // will be shifted down in Window_Close()
		}
	}

	tool->window = UINT16_MAX;
}


void Main_DrawMenuBar()
{
	if ( gShowQuitConfirmation )
	{
		DrawQuitConfirmation();
	}

	if ( ImGui::BeginMainMenuBar() )
	{
		if ( ImGui::BeginMenu( "Load Tool" ) )
		{
			if ( ImGui::MenuItem( "Game" ) )
			{
			}

			ImGui::Separator();

			for ( auto& tool : gTools )
			{
				if ( ImGui::MenuItem( tool.tool->GetName(), nullptr, tool.running ) )
					App_LaunchTool( tool.interface );
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
			ImGui::EndMenu();
		}

		ImGui::Separator();

		ImGui::EndMainMenuBar();
	}

	auto yeah          = ImGui::GetItemRectSize();
	gMainMenuBarHeight = yeah.y;
}


#define CH_LIVE_WINDOW_RESIZE 1


// return true if we should stop the UpdateLoop
void App_CloseMainWindow()
{
	// Close ALL Windows and Tools
	for ( u32 i = 0; i < gTools.size(); i++ )
	{
		if ( gTools[ i ].running )
		{
			gTools[ i ].tool->Close();
		}
	}

	gTools.clear();

	Window_CloseAll();

	AssetBrowser_Close();

	// lazy way to tell engine to quit
	Con_RunCommand( "quit" );
}


u16 App_GetWindowIndex( SDL_Window* sdl_window )
{
	for ( u32 i = 0; i < g_app_window_count; i++ )
	{
		if ( g_app_window[ i ].window == sdl_window )
			return i;
	}

	return UINT16_MAX;
}


u16 App_GetWindowIndexFromID( u32 window_id )
{
	SDL_Window* sdl_window = SDL_GetWindowFromID( window_id );

	if ( !sdl_window )
		return UINT16_MAX;

	return App_GetWindowIndex( sdl_window );
}


bool App_HandleEvents()
{
	std::vector< SDL_Event >* events = input->GetEvents();

	for ( SDL_Event& event : *events )
	{
		switch ( event.type )
		{
			case SDL_QUIT:
			{
				App_CloseMainWindow();
				return true;
			}
			case SDL_WINDOWEVENT:
			{
				SDL_Window* sdl_window = SDL_GetWindowFromID( event.window.windowID );

				switch ( event.window.event )
				{
					case SDL_WINDOWEVENT_CLOSE:
					{
						// Is this the main window?
						if ( sdl_window == gpWindow )
						{
							App_CloseMainWindow();
							return true;
						}

						u16 windowIndex = App_GetWindowIndex( sdl_window );

						if ( windowIndex == UINT16_MAX )
							break;

						for ( u32 i = 0; i < gTools.size(); i++ )
						{
							if ( gTools[ i ].window != windowIndex )
								continue;

							App_CloseTool( &gTools[ i ] );
							break;
						}

						break;
					}
					case SDL_WINDOWEVENT_SIZE_CHANGED:
					{
						u16 windowIndex = App_GetWindowIndex( sdl_window );

						if ( windowIndex == UINT16_MAX )
							break;

						App_UpdateImGuiDisplaySize( g_app_window[ windowIndex ] );

						render->reset( g_app_window[ windowIndex ].graphicsWindow );
						
						Log_Msg( "SDL Window Size Changed\n" );
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

	return false;
}


void RenderMainWindow( float frameTime, bool sResize )
{
	PROF_SCOPE();

	Main_DrawMenuBar();

	// hack
	static bool showConsole     = false;
	static bool wasConsoleShown = false;

	// set position
	glm::uvec2 surface_size = render->window_surface_size( gGraphicsWindow );

	// ImGui::SetNextWindowSizeConstraints( { (float)width, (float)( (surface_size.y) - gMainMenuBarHeight ) }, { (float)width, (float)( (surface_size.y) - gMainMenuBarHeight ) } );
	//if ( showConsole )
	//	ImGui::SetNextWindowSizeConstraints( { (float)width, 64 }, { (float)width, 64 } );
	//else

	static bool inToolTab = false;

	// value... fresh from my ass
	// float       titleBarHeight = 16.f;
	// float       titleBarHeight = 10.f;
	// float       titleBarHeight = 28.f;
	float       titleBarHeight = 31.f;

	if ( inToolTab )
	{
		// ImGui::SetNextWindowSizeConstraints( { (float)width, (float)( gMainMenuBarHeight + titleBarHeight ) }, { (float)width, (float)( gMainMenuBarHeight + titleBarHeight ) } );
		ImGui::SetNextWindowSizeConstraints( { (float)surface_size.x, (float)( titleBarHeight ) }, { (float)surface_size.x, (float)( titleBarHeight ) } );
	}
	else
	{
		ImGui::SetNextWindowSizeConstraints( { (float)surface_size.x, (float)( ( surface_size.y ) - gMainMenuBarHeight ) }, { (float)surface_size.x, (float)( ( surface_size.y ) - gMainMenuBarHeight ) } );
	}

	ImGui::SetNextWindowPos( { 0, (float)gMainMenuBarHeight } );

	if ( ImGui::Begin( "##Asset Browser", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollWithMouse ) )
	{
		if ( ImGui::BeginTabBar( "Toolkit Tabs" ) )
		{
			inToolTab = false;

			if ( ImGui::BeginTabItem( "Asset List" ) )
			{
				AssetBrowser_Draw();
				ImGui::EndTabItem();
			}

			if ( ImGui::BeginTabItem( "Console" ) )
			{
				showConsole = true;
				gui->DrawConsole( wasConsoleShown, true );
				ImGui::EndTabItem();
			}
			else
			{
				showConsole = false;
			}

			if ( ImGui::BeginTabItem( "Resource Usage" ) )
			{
				ResourceUsage_Draw();
				ImGui::EndTabItem();
			}

			if ( ImGui::BeginTabItem( "Settings" ) )
			{
				Main_DrawSettingsMenu();
				ImGui::EndTabItem();
			}

			// Draw the tabs for the main tools
			for ( auto& tool : gTools )
			{
				if ( !tool.running )
					continue;

				if ( tool.window )
					continue;

				if ( ImGui::BeginTabItem( tool.tool->GetName() ) )
				{
					inToolTab = true;
					tool.tool->Render( frameTime, sResize, { 0, titleBarHeight } );
					ImGui::EndTabItem();
				}
			}

			ImGui::EndTabBar();
		}

		if ( wasConsoleShown != showConsole )
			wasConsoleShown = showConsole;

		ImGui::End();
	}

	if ( sResize )
		UpdateProjection();
}


void UpdateLoop( float frameTime, bool sResize )
{
	PROF_SCOPE();

	// dump fps to console for now

	// calc fps from frametime
	static float fps = 0.f;
	fps = 1.f / frameTime;

	// only print every 60 frames
	static u32 frameCount = 0;
	frameCount++;

	if ( frameCount % 60 == 0 )
	{
		// float to string c
		char fps_str[ 16 ];
		gcvt( fps, 4, fps_str );

	// 	ch_print( "FPS: " );
		ch_print( fps_str );
		ch_print( "\n" );
	}

	{
		PROF_SCOPE_NAMED( "Imgui New Frame" );
		ImGui::NewFrame();
		ImGui_ImplSDL2_NewFrame();
	}

	if ( App_HandleEvents() )
		return;

	render->new_frame();

	if ( sResize )
		render->reset( gGraphicsWindow );

	// Run Tools
	if ( !sResize )
	{
		for ( u32 i = 0; i < gTools.size(); i++ )
		{
			if ( gTools[ i ].running )
				gTools[ i ].tool->Update( frameTime );
		}
	}

	input->SetCurrentWindow( gpWindow );

	if ( !( SDL_GetWindowFlags( gpWindow ) & SDL_WINDOW_MINIMIZED ) )
	{
		RenderMainWindow( frameTime, sResize );
	}
	else
	{
		PROF_SCOPE_NAMED( "Imgui End Frame" );
		ImGui::EndFrame();
	}

	if ( !sResize )
	{
		// Draw other windows
		for ( auto& tool: gTools )
		{
			if ( !tool.running )
				continue;

			if ( tool.window )
				Window_Render( tool, frameTime, sResize );
		}
	}

	ImGui::Render();

	if ( sResize )
		return;

//	render->PrePresent();
//	render->present( gGraphicsWindow, &gMainViewportHandle, 1 );
	render->present( gGraphicsWindow );
	Window_PresentAll();

	Con_Update();
}


void WindowResizeCallback( void* hwnd )
{
#if CH_LIVE_WINDOW_RESIZE
	if ( hwnd == gpSysWindow )
	{
		UpdateLoop( 0.f, true );

		// render->PrePresent();
		// render->present( gGraphicsWindow, &gMainViewportHandle, 1 );
		render->present( gGraphicsWindow );
		return;
	}

	for ( LoadedTool& tool: gTools )
	{
		if ( tool.window == UINT16_MAX )
			continue;

		if ( g_app_window[ tool.window ].nativeWindow == hwnd )
		{
			Window_Render( tool, 0.f, true );
			Window_Present( tool );
			break;
		}
	}
#endif
}


bool App_CreateMainWindow()
{
	// Create Main Window
	std::string windowName;

	windowName = ( Core_GetAppInfo().apWindowTitle ) ? Core_GetAppInfo().apWindowTitle : "Chocolate Engine";
	windowName += vstring( " - Build %zd - Compiled On - %s %s", Core_GetBuildNumber(), Core_GetBuildDate(), Core_GetBuildTime() );

#ifdef _WIN32
	gpSysWindow = Sys_CreateWindow( windowName.c_str(), gWidth, gHeight, gMaxWindow );

	if ( !gpSysWindow )
	{
		Log_Error( "Failed to create toolkit window\n" );
		return false;
	}

	gpWindow = SDL_CreateWindowFrom( gpSysWindow );
#else
	int flags = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

	if ( gMaxWindow )
		flags |= SDL_WINDOW_MAXIMIZED;

	gpWindow = SDL_CreateWindow( windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                             gWidth, gHeight, flags );
#endif

	if ( !gpWindow )
	{
		Log_Error( "Failed to create SDL2 Window\n" );
		return false;
	}

	render->set_main_surface( gpWindow, gpSysWindow );

	input->AddWindow( gpWindow, ImGui::GetCurrentContext() );

	// add it to the window list
	g_app_window_count       = 1;
	g_app_window             = ch_malloc< AppWindow >( g_app_window_count );

	AppWindow& appWindow     = g_app_window[ 0 ];
	appWindow.window         = gpWindow;
	appWindow.context        = ImGui::GetCurrentContext();
	appWindow.graphicsWindow = gGraphicsWindow;

#ifdef _WIN32
	appWindow.nativeWindow = gpSysWindow;
#endif

	return true;
}


bool App_Init()
{
	gGraphicsWindow = render->window_create( gpWindow, gpSysWindow );

	if ( gGraphicsWindow == CH_INVALID_HANDLE )
	{
		Log_Fatal( "Failed to Create GraphicsAPI Window\n" );
		return false;
	}

	// lol
	g_app_window[ 0 ].graphicsWindow = gGraphicsWindow;

	gui->StyleImGui();

	// Create the Main Viewport - TODO: use this more across the game code
//	gMainViewportHandle = graphics->CreateViewport();

	UpdateProjection();

#ifdef _WIN32
	Sys_SetResizeCallback( WindowResizeCallback );
#endif /* _WIN32  */

	AssetBrowser_Init();

	Log_Msg( "Toolkit Loaded!\n" );
	return true;
}


static void DrawQuitConfirmation()
{
	glm::uvec2 surface_size = render->window_surface_size( gGraphicsWindow );

	ImGui::SetNextWindowSize( { (float)surface_size.x, (float)surface_size.y } );
	ImGui::SetNextWindowPos({0.f, 0.f});

	if (ImGui::Begin("FullScreen Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav))
	{
		ImGui::SetNextWindowFocus();

		ImGui::SetNextWindowSize({ 250, 60 });

		ImGui::SetNextWindowPos( { ( surface_size.x / 2.f ) - 125.f, ( surface_size.y / 2.f ) - 30.f } );

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


void App_UpdateImGuiDisplaySize( AppWindow& app_window )
{
	ImGuiContext* context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext( app_window.context );

	glm::uvec2 surface_size = render->window_surface_size( app_window.graphicsWindow );

	auto&      io           = ImGui::GetIO();
	io.DisplaySize.x        = surface_size.x;
	io.DisplaySize.y        = surface_size.y;

	// restore original context
	ImGui::SetCurrentContext( context );
}


void UpdateProjection()
{
	PROF_SCOPE();

	glm::uvec2 surface_size = render->window_surface_size( gGraphicsWindow );

	auto&      io           = ImGui::GetIO();
	io.DisplaySize.x        = surface_size.x;
	io.DisplaySize.y        = surface_size.y;

//	ViewportShader_t* viewport  = graphics->GetViewportData( gMainViewportHandle );
//
//	if ( !viewport )
//		return;
//
//	// HACK HACK HACK
//	// if ( !gSingleWindow )
//	{
//		viewport->aNearZ      = r_nearz;
//		viewport->aFarZ       = r_farz;
//		viewport->aSize       = { width, height };
//		viewport->aOffset     = { 0, 0 };
//		viewport->aProjection = glm::mat4( 1.f );
//		viewport->aView       = glm::mat4( 1.f );
//		viewport->aProjView   = glm::mat4( 1.f );
//	}
//
//	graphics->SetViewportUpdate( true );
}


void Toolkit::OpenAsset( const char* spToolInterface, const char* spPath )
{
	LoadedTool* tool = App_GetTool( spToolInterface );

	if ( tool == nullptr )
	{
		Log_ErrorF( "Failed to find tool \"%s\" to open asset with: \"%s\"\n", spToolInterface, spPath );
		return;
	}

	if ( !tool->running )
		App_LaunchTool( spToolInterface );

	if ( !tool->running )
	{
		Log_ErrorF( "Failed to launch tool \"%s\" to open asset with: \"%s\"\n", spToolInterface, spPath );
		return;
	}

	if ( !tool->tool->OpenAsset( spPath ) )
	{
		Log_ErrorF( "Failed to open asset in tool \"%s\": \"%s\"\n", spToolInterface, spPath );
		return;
	}

	if ( tool->window != UINT16_MAX )
		Window_Focus( g_app_window[ tool->window ] );
}

