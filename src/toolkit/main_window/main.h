#pragma once

#include "core/core.h"
#include "system.h"

#include "render/irender.h"

class IGuiSystem;
class IRender;
class IInputSystem;
class IAudioSystem;
class IGraphics;
class IRenderSystemOld;

extern IGuiSystem*       gui;
extern IRender*          render;
extern IInputSystem*     input;
extern IAudioSystem*     audio;
extern IGraphics*        graphics;
extern IRenderSystemOld* renderOld;

extern u32               gMainViewportHandle;
extern SDL_Window*       gpWindow;
extern ChHandle_t        gGraphicsWindow;

bool                     App_CreateMainWindow();
bool                     App_Init();

void                     UpdateLoop( float frameTime, bool sResize = false );
void                     UpdateProjection();
