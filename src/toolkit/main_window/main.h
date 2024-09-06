#pragma once

#include "core/core.h"
#include "system.h"
#include "itool.h"

#include "iinput.h"
#include "irender3.h"
#include "igui.h"
#include "physics/iphysics.h"

#include "imgui/imgui.h"

class IGuiSystem;
class IRender3;
class IInputSystem;
class IAudioSystem;
class ImGuiContext;


struct AppWindow
{
	SDL_Window*   window         = nullptr;
	void*         nativeWindow   = nullptr;
	ChHandle_t    graphicsWindow = CH_INVALID_HANDLE;
	ImGuiContext* context        = nullptr;
};


struct LoadedTool
{
	const char* interface          = nullptr;
	ITool*      tool               = nullptr;
	u16         window             = UINT16_MAX;
	bool        running            = false;    // MOVE TO SEPARATE ARRAY
	bool        renderInMainWindow = false;    // MOVE TO SEPARATE ARRAY - if false, this tool has it's own window, if true, it's stored in a tab in the main window
};


class Toolkit : public IToolkit
{
   public:
	// Open an Asset in a Tool
	// - spToolInterface is the name of the module interface
	// - spPath is the path to the asset to open
	void OpenAsset( const char* spToolInterface, const char* spPath ) override;
};


extern IGuiSystem*               gui;
extern IRender3*                 render;
extern IInputSystem*             input;
extern IAudioSystem*             audio;

extern Toolkit                   toolkit;

extern u32                       gMainViewportHandle;
extern SDL_Window*               gpWindow;
extern ChHandle_t                gGraphicsWindow;
extern std::vector< LoadedTool > gTools;

extern AppWindow*                g_app_window;
extern u16                       g_app_window_count;

CONVAR_FLOAT_EXT( r_nearz );
CONVAR_FLOAT_EXT( r_farz );
CONVAR_FLOAT_EXT( r_fov );

// void                             Util_DrawTextureInfo( TextureInfo_t& info );

LoadedTool*                      App_GetTool( const char* tool );
void                             App_CloseTool( LoadedTool* tool );
bool                             App_CreateMainWindow();
bool                             App_Init();

u16                              App_GetWindowIndex( SDL_Window* sdl_window );
u16                              App_GetWindowIndexFromID( u32 window_id );

void                             UpdateLoop( float frameTime, bool sResize = false );
void                             App_UpdateImGuiDisplaySize( AppWindow& app_window );
void                             UpdateProjection();

// returns an index to the window
u16                              Window_Create( const char* windowName );
void                             Window_Close( u16 index );
void                             Window_CloseAll();
void                             Window_Focus( AppWindow& window );
void                             Window_Render( LoadedTool& tool, float frameTime, bool sResize );
void                             Window_Present( LoadedTool& window );
void                             Window_PresentAll();

bool                             AssetBrowser_Init();
void                             AssetBrowser_Close();
void                             AssetBrowser_Draw();

void                             ResourceUsage_Draw();
