#include "main.h"

#include "imgui/imgui_impl_sdl2.h"


AppWindow* g_app_window       = nullptr;
u16        g_app_window_count = 0;

// ch_handle_array_16_t< app_window_handle_t, AppWindow > g_app_windows;


void Window_Focus( AppWindow& window )
{
	input->SetWindowFocused( window.window );
}


u16 Window_Create( const char* windowName )
{
	AppWindow* new_data = ch_realloc( g_app_window, g_app_window_count + 1 );
	
	if ( !new_data )
	{
		Log_Error( "Failed to allocate memory for new window\n" );
		return UINT16_MAX;
	}
	
	g_app_window = new_data;
	AppWindow& appWindow = g_app_window[ g_app_window_count ];
	
	// zero it
	memset( &appWindow, 0, sizeof( AppWindow ) );

// 	AppWindow* appWindow = nullptr;
// 
// 	app_window_handle_t handle = g_app_windows.create( &appWindow );
// 
// 	if ( !handle )
// 	{
// 		Log_Error( "Failed to allocate memory for new window\n" );
// 		return UINT16_MAX;
// 	}

#ifdef _WIN32
	appWindow.nativeWindow = Sys_CreateWindow( windowName, 1280, 720, false );

	if ( !appWindow.nativeWindow )
	{
		Log_Error( "Failed to create native window\n" );
		return UINT16_MAX;
	}

	appWindow.window = SDL_CreateWindowFrom( appWindow.nativeWindow );
#else
	int flags         = SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;

	appWindow.window = SDL_CreateWindow( windowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                      1280, 720, flags );
#endif

	if ( !appWindow.window )
	{
		Log_Error( "Failed to create tool window\n" );
		return UINT16_MAX;
	}

	auto origContext   = ImGui::GetCurrentContext();
	appWindow.context = ImGui::CreateContext();
	ImGui::SetCurrentContext( appWindow.context );

	appWindow.graphicsWindow = render->window_create( appWindow.window, appWindow.nativeWindow );

	if ( appWindow.graphicsWindow == CH_INVALID_HANDLE )
	{
		Log_Fatal( "Failed to Create GraphicsAPI Window\n" );
		ImGui::SetCurrentContext( origContext );
		g_app_window_count++;
		Window_Close( g_app_window_count - 1 );
		return UINT16_MAX;
	}

	gui->StyleImGui();
	input->AddWindow( appWindow.window, appWindow.context );

	// Create the Main Viewport - TODO: use this more across the game code
	//appWindow.viewport = graphics->CreateViewport();

	glm::uvec2 surface_size = render->window_surface_size( appWindow.graphicsWindow );

	auto&      io           = ImGui::GetIO();
	io.DisplaySize.x        = surface_size.x;
	io.DisplaySize.y        = surface_size.y;

	//ViewportShader_t* viewport = graphics->GetViewportData( appWindow.viewport );
	//
	//if ( !viewport )
	//{
	//	ImGui::SetCurrentContext( origContext );
	//	Window_Close( *appWindow );
	//	return nullptr;
	//}
	//
	//viewport->aNearZ      = r_nearz;
	//viewport->aFarZ       = r_farz;
	//viewport->aSize       = { width, height };
	//viewport->aOffset     = { 0, 0 };
	//viewport->aProjection = glm::mat4( 1.f );
	//viewport->aView       = glm::mat4( 1.f );
	//viewport->aProjView   = glm::mat4( 1.f );
	//
	//graphics->SetViewportUpdate( true );

	ImGui::SetCurrentContext( origContext );

	Window_Focus( appWindow );

	return g_app_window_count++;
}


void Window_Close( u16 index )
{
//	AppWindow* window = g_app_windows.get( handle );
//
//	if ( !window )
//	{
//		Log_Error( "Invalid Window Handle\n" );
//		return;
//	}

//	if ( index > 0 )
//	{
//		Log_Error( "Can't close the main window through here\n" );
//		return;
//	}

	if ( index > g_app_window_count )
	{
		Log_Error( "Invalid Window Index\n" );
		return;
	}

	AppWindow&    window      = g_app_window[ index ];

	ImGuiContext* origContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext( window.context );

	input->RemoveWindow( window.window );

	//graphics->FreeViewport( window.viewport );
	render->window_free( window.graphicsWindow );
	ImGui_ImplSDL2_Shutdown();

	ImGui::DestroyContext( window.context );
	ImGui::SetCurrentContext( origContext );

	// shift all the data back
	for ( u16 i = index; i < g_app_window_count - 1; i++ )
	{
		g_app_window[ i ] = g_app_window[ i + 1 ];
	}

	g_app_window_count--;

//	g_app_windows.free( handle );
}


void Window_CloseAll()
{
	while ( g_app_window_count > 0 )
	{
		Window_Close( 0 );
	}
}


// TODO: this window will be used for Open and Save dialogs, not attached to any tools
// maybe change this to Window_RenderStart and Window_RenderEnd so we don't have to check if it's a tool every frame?
// TODO: when the new render system is here, we can change this to just Window_Present()
// also is more like Tool_RenderWindow()
void Window_Render( LoadedTool& tool, float frameTime, bool sResize )
{
	auto origContext = ImGui::GetCurrentContext();

	if ( tool.window == UINT16_MAX )
		return;

	AppWindow& window = g_app_window[ tool.window ];

	input->SetCurrentWindow( window.window );
	ImGui::SetCurrentContext( window.context );

	if ( sResize )
	{
		render->reset( window.graphicsWindow );

		glm::uvec2 surface_size = render->window_surface_size( window.graphicsWindow );

		auto&      io           = ImGui::GetIO();
		io.DisplaySize.x        = surface_size.x;
		io.DisplaySize.y        = surface_size.y;

		// ViewportShader_t* viewport = graphics->GetViewportData( window.viewport );
		// 
		// if ( !viewport )
		// 	return;
		// 
		// viewport->aNearZ      = r_nearz;
		// viewport->aFarZ       = r_farz;
		// viewport->aSize       = { width, height };
		// viewport->aOffset     = { 0, 0 };
		// viewport->aProjection = glm::mat4( 1.f );
		// viewport->aView       = glm::mat4( 1.f );
		// viewport->aProjView   = glm::mat4( 1.f );
		// 
		// graphics->SetViewportUpdate( true );
	}

	{
		PROF_SCOPE_NAMED( "Imgui New Frame" );
		ImGui::NewFrame();
		ImGui_ImplSDL2_NewFrame();
	}

	tool.tool->Render( frameTime, sResize, {} );

	ImGui::SetCurrentContext( origContext );
	input->SetCurrentWindow( gpWindow );
}


// change to Tool_Present()?
void Window_Present( LoadedTool& tool )
{
	if ( !tool.running )
		return;

	auto origContext = ImGui::GetCurrentContext();

	if ( tool.window != UINT16_MAX )
	{
		AppWindow& window = g_app_window[ tool.window ];
		ImGui::SetCurrentContext( window.context );
	}

	tool.tool->Present();
	ImGui::SetCurrentContext( origContext );
}


// change to Tool_PresentAll()?
void Window_PresentAll()
{
	for ( LoadedTool& tool : gTools )
	{
		Window_Present( tool );
	}
}

