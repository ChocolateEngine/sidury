#include "main.h"
#include "core/systemmanager.h"
#include "core/asserts.h"
#include "core/app_info.h"
#include "core/build_number.h"

#include "iinput.h"
#include "igui.h"
#include "render/irender.h"
#include "igraphics.h"
#include "physics/iphysics.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl2.h"

#include "util.h"

#include <SDL_system.h>
#include <SDL_hints.h>

#include <algorithm>


int               gWidth          = Args_RegisterF( 1280, "Width of the main window", 2, "-width", "-w" );
int               gHeight         = Args_RegisterF( 720, "Height of the main window", 2, "-height", "-h" );
static bool       gMaxWindow      = Args_Register( "Maximize the main window", "-max" );


SDL_Window*       gpWindow        = nullptr;
void*             gpSysWindow     = nullptr;
ChHandle_t        gGraphicsWindow = CH_INVALID_HANDLE;


struct AppWindow
{
	SDL_Window*   window         = nullptr;
	void*         sysWindow      = nullptr;
	ChHandle_t    graphicsWindow = CH_INVALID_HANDLE;
	ImGuiContext* context        = nullptr;

	ChHandle_t    viewport       = CH_INVALID_HANDLE;
};


std::vector< AppWindow > gWindows;


IGuiSystem*       gui             = nullptr;
IRender*          render          = nullptr;
IInputSystem*     input           = nullptr;
IAudioSystem*     audio           = nullptr;
IGraphics*        graphics        = nullptr;
IRenderSystemOld* renderOld       = nullptr;
Ch_IPhysics*      ch_physics      = nullptr;

static bool       gPaused         = false;
float             gFrameTime      = 0.f;

// TODO: make gRealTime and gGameTime
// real time is unmodified time since engine launched, and game time is time affected by host_timescale and pausing
double            gCurTime        = 0.0;  // i could make this a size_t, and then just have it be every 1000 is 1 second

extern bool       gRunning;
extern ConVar     host_timescale;

u32               gMainViewportHandle   = UINT32_MAX;

int               gMainMenuBarHeight    = 0.f;
static bool       gShowQuitConfirmation = false;


CONVAR( r_nearz, 0.01 );
CONVAR( r_farz, 10000 );
CONVAR( r_fov, 106 );


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

			if ( ImGui::MenuItem( "Map Editor" ) )
			{
			}

			if ( ImGui::MenuItem( "Material Editor" ) )
			{
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

		ImGui::EndMainMenuBar();
	}

	auto yeah          = ImGui::GetItemRectSize();
	gMainMenuBarHeight = yeah.y;
}


// disabled cause for some reason, it could jump to the WindowProc function mid frame and call this 
#define CH_LIVE_WINDOW_RESIZE 1


void Window_Create()
{
	AppWindow& testWindow = gWindows.emplace_back();

	std::string windowName = vstring( "test window %d", gWindows.size() );

#ifdef _WIN32
	testWindow.sysWindow = Sys_CreateWindow( windowName.c_str(), 800, 600, false );

	if ( !testWindow.sysWindow )
	{
		Log_Error( "Failed to create test window\n" );
		return;
	}

	testWindow.window = SDL_CreateWindowFrom( testWindow.sysWindow );
#else
	int flags = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

	testWindow.window = SDL_CreateWindow( windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                             800, 600, flags );
#endif

	if ( !testWindow.window )
	{
		Log_Error( "Failed to create SDL2 Window\n" );
		return;
	}

	auto origContext          = ImGui::GetCurrentContext();
	testWindow.context        = ImGui::CreateContext();
	ImGui::SetCurrentContext( testWindow.context );

	testWindow.graphicsWindow = render->CreateWindow( testWindow.window, testWindow.sysWindow );

	if ( testWindow.graphicsWindow == CH_INVALID_HANDLE )
	{
		Log_Fatal( "Failed to Create GraphicsAPI Window\n" );
		ImGui::SetCurrentContext( origContext );
		return;
	}

	gui->StyleImGui();
	input->AddWindow( testWindow.window, testWindow.context );

	// Create the Main Viewport - TODO: use this more across the game code
	testWindow.viewport = graphics->CreateViewport();

	int width = 0, height = 0;
	render->GetSurfaceSize( testWindow.graphicsWindow, width, height );

	auto& io                   = ImGui::GetIO();
	io.DisplaySize.x           = width;
	io.DisplaySize.y           = height;

	ViewportShader_t* viewport = graphics->GetViewportData( testWindow.viewport );

	if ( !viewport )
	{
		ImGui::SetCurrentContext( origContext );
		return;
	}

	viewport->aNearZ      = r_nearz;
	viewport->aFarZ       = r_farz;
	viewport->aSize       = { width, height };
	viewport->aOffset     = { 0, 0 };
	viewport->aProjection = glm::mat4( 1.f );
	viewport->aView       = glm::mat4( 1.f );
	viewport->aProjView   = glm::mat4( 1.f );

	graphics->SetViewportUpdate( true );
	ImGui::SetCurrentContext( origContext );
}


void Window_OnClose( AppWindow& window )
{
	graphics->FreeViewport( window.viewport );
	render->DestroyWindow( window.graphicsWindow );

	ImGuiContext* origContext = ImGui::GetCurrentContext();

	ImGui::SetCurrentContext( window.context );

	render->ShutdownImGui();
	ImGui_ImplSDL2_Shutdown();

	ImGui::DestroyContext( window.context );
	ImGui::SetCurrentContext( origContext );
}


void Window_Render( AppWindow& window, float frameTime, bool sResize )
{
	auto origContext = ImGui::GetCurrentContext();

	input->SetCurrentWindow( window.window );
	ImGui::SetCurrentContext( window.context );

	if ( sResize )
	{
		renderOld->Reset( window.graphicsWindow );

		int width = 0, height = 0;
		render->GetSurfaceSize( window.graphicsWindow, width, height );

		auto& io                   = ImGui::GetIO();
		io.DisplaySize.x           = width;
		io.DisplaySize.y           = height;

		ViewportShader_t* viewport = graphics->GetViewportData( window.viewport );

		if ( !viewport )
			return;

		viewport->aNearZ      = r_nearz;
		viewport->aFarZ       = r_farz;
		viewport->aSize       = { width, height };
		viewport->aOffset     = { 0, 0 };
		viewport->aProjection = glm::mat4( 1.f );
		viewport->aView       = glm::mat4( 1.f );
		viewport->aProjView   = glm::mat4( 1.f );

		graphics->SetViewportUpdate( true );
	}

	{
		PROF_SCOPE_NAMED( "Imgui New Frame" );
		ImGui::NewFrame();
		ImGui_ImplSDL2_NewFrame();
	}

	ImGui::Text( "cool" );

	renderOld->Present( window.graphicsWindow );
	ImGui::SetCurrentContext( origContext );
	input->SetCurrentWindow( gpWindow );
}


extern bool AssetBrowser_Init();
extern void AssetBrowser_Draw();


// return true if we should stop the UpdateLoop
void App_CloseMainWindow()
{
	// Close ALL Windows
	for ( u32 i = 0; i < gWindows.size(); i++ )
	{
		input->RemoveWindow( gWindows[ i ].window );
		Window_OnClose( gWindows[ i ] );
	}

	gWindows.clear();

	// lazy way to tell engine to quit
	Con_RunCommand( "quit" );
}


bool App_HandleEvents()
{
	std::vector< SDL_Event >* events = input->GetEvents();

	for ( SDL_Event& event : *events )
	{
		if ( event.type == SDL_QUIT )
		{
			App_CloseMainWindow();
			return true;
		}

		else if ( event.type == SDL_WINDOWEVENT )
		{
			if ( event.window.event == SDL_WINDOWEVENT_CLOSE )
			{
				SDL_Window* sdlWindow = SDL_GetWindowFromID( event.window.windowID );

				// Is this the main window?
				if ( sdlWindow == gpWindow )
				{
					App_CloseMainWindow();
					return true;
				}

				for ( u32 i = 0; i < gWindows.size(); i++ )
				{
					if ( gWindows[ i ].window != sdlWindow )
						continue;

					Window_OnClose( gWindows[ i ] );
					vec_remove_index( gWindows, i );
					break;
				}
			}
		}
	}

	return false;
}


void UpdateLoop( float frameTime, bool sResize )
{
	PROF_SCOPE();

	{
		PROF_SCOPE_NAMED( "Imgui New Frame" );
		ImGui::NewFrame();
		ImGui_ImplSDL2_NewFrame();
	}

	if ( App_HandleEvents() )
		return;

	renderOld->NewFrame();

	if ( sResize )
		renderOld->Reset( gGraphicsWindow );

	input->SetCurrentWindow( gpWindow );

	if ( !( SDL_GetWindowFlags( gpWindow ) & SDL_WINDOW_MINIMIZED ) )
	{
		Main_DrawMenuBar();

		// hack
		static bool showConsole     = false;
		static bool wasConsoleShown = false;

		// set position
		int         width, height;
		render->GetSurfaceSize( gGraphicsWindow, width, height );

		// ImGui::SetNextWindowSizeConstraints( { (float)width, (float)( (height) - gMainMenuBarHeight ) }, { (float)width, (float)( (height) - gMainMenuBarHeight ) } );
		//if ( showConsole )
		//	ImGui::SetNextWindowSizeConstraints( { (float)width, 64 }, { (float)width, 64 } );
		//else
			ImGui::SetNextWindowSizeConstraints( { (float)width, (float)( (height)-gMainMenuBarHeight ) }, { (float)width, (float)( (height)-gMainMenuBarHeight ) } );

		if ( ImGui::Begin( "##Asset Browser", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize ) )
		{
			if ( ImGui::BeginTabBar( "Toolkit Tabs" ) )
			{
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
					ImGui::Text( "TODO" );

					if ( ImGui::Button( "TEST WINDOW" ) )
						Window_Create();

					ImGui::EndTabItem();
				}

				if ( ImGui::BeginTabItem( "Settings" ) )
				{
					Main_DrawSettingsMenu();
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}

			if ( wasConsoleShown != showConsole )
				wasConsoleShown = showConsole;

			ImGui::End();
		}

		// if ( showConsole )
		// 	gui->Update( 0.f );

		if ( sResize )
			UpdateProjection();

		renderOld->Present( gGraphicsWindow );
	}
	else
	{
		PROF_SCOPE_NAMED( "Imgui End Frame" );
		ImGui::EndFrame();
	}

	if ( !sResize )
	{
		for ( auto& testWindow : gWindows )
		{
			Window_Render( testWindow, frameTime, sResize );
		}
	}

	if ( sResize )
		return;

	Con_Update();
}


void WindowResizeCallback( void* hwnd )
{
#if CH_LIVE_WINDOW_RESIZE
	if ( hwnd == gpSysWindow )
	{
		UpdateLoop( 0.f, true );
		return;
	}

	for ( AppWindow& window : gWindows )
	{
		if ( window.sysWindow != hwnd )
			continue;

		Window_Render( window, 0.f, true );
		break;
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

	render->SetMainSurface( gpWindow, gpSysWindow );

	input->AddWindow( gpWindow, ImGui::GetCurrentContext() );

	return true;
}


bool App_Init()
{
	gGraphicsWindow = render->CreateWindow( gpWindow, gpSysWindow );

	if ( gGraphicsWindow == CH_INVALID_HANDLE )
	{
		Log_Fatal( "Failed to Create GraphicsAPI Window\n" );
		return false;
	}

	gui->StyleImGui();

	// Create the Main Viewport - TODO: use this more across the game code
	gMainViewportHandle = graphics->CreateViewport();

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
	int width = 0, height = 0;
	render->GetSurfaceSize( gGraphicsWindow, width, height );

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


void UpdateProjection()
{
	PROF_SCOPE();

	int width = 0, height = 0;
	render->GetSurfaceSize( gGraphicsWindow, width, height );

	auto& io          = ImGui::GetIO();
	io.DisplaySize.x  = width;
	io.DisplaySize.y  = height;

	ViewportShader_t* viewport  = graphics->GetViewportData( gMainViewportHandle );

	if ( !viewport )
		return;

	viewport->aNearZ      = r_nearz;
	viewport->aFarZ       = r_farz;
	viewport->aSize       = { width, height };
	viewport->aOffset     = { 0, 0 };
	viewport->aProjection = glm::mat4( 1.f );
	viewport->aView       = glm::mat4( 1.f );
	viewport->aProjView   = glm::mat4( 1.f );

	graphics->SetViewportUpdate( true );
}

