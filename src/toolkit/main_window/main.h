#pragma once

#include "core/core.h"
#include "system.h"
#include "itool.h"

#include "iinput.h"
#include "render/irender.h"
#include "igui.h"
#include "igraphics.h"
#include "physics/iphysics.h"

#include "imgui/imgui.h"

class IGuiSystem;
class IRender;
class IInputSystem;
class IAudioSystem;
class IGraphics;
class IRenderSystemOld;
class ImGuiContext;


struct AppWindow
{
	SDL_Window*   window         = nullptr;
	void*         sysWindow      = nullptr;
	ChHandle_t    graphicsWindow = CH_INVALID_HANDLE;
	ImGuiContext* context        = nullptr;

	ChHandle_t    viewport       = CH_INVALID_HANDLE;

	ITool*        tool           = nullptr;
};


extern IGuiSystem*              gui;
extern IRender*                 render;
extern IInputSystem*            input;
extern IAudioSystem*            audio;
extern IGraphics*               graphics;
extern IRenderSystemOld*        renderOld;

extern ITool*                   toolMapEditor;
extern ITool*                   toolMatEditor;

extern bool                     toolMapEditorOpen;
extern bool                     toolMatEditorOpen;

extern u32                      gMainViewportHandle;
extern SDL_Window*              gpWindow;
extern ChHandle_t               gGraphicsWindow;
extern std::vector< AppWindow > gWindows;

extern ConVar                   r_nearz;
extern ConVar                   r_farz;
extern ConVar                   r_fov;


bool                            App_CreateMainWindow();
bool                            App_Init();

void                            UpdateLoop( float frameTime, bool sResize = false );
void                            UpdateProjection();

AppWindow*                      Window_Create( const char* windowName );
void                            Window_OnClose( AppWindow& window );
void                            Window_Focus( AppWindow* window );
void                            Window_Render( AppWindow& window, float frameTime, bool sResize );

void                            Tool_Focus( ITool* tool );
void                            Tool_OpenAsset( ITool* tool, bool& toolIsOpen, const std::string& path );

