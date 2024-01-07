#pragma once

#include "core/core.h"
#include "system.h"

struct ViewportCamera_t;

// world xyz
constexpr int W_FORWARD = 0;
constexpr int W_RIGHT = 1;
constexpr int W_UP = 2;

//constexpr glm::vec3 vec3_forward(1, 0, 0);
//constexpr glm::vec3 vec3_right(0, 1, 0);
//constexpr glm::vec3 vec3_up(0, 0, 1);

const glm::vec3 vec3_forward(1, 0, 0);
const glm::vec3 vec3_right(0, 1, 0);
const glm::vec3 vec3_up(0, 0, 1);

#include "game_physics.h"
#include "entity.h"
#include "igraphics.h"
#include "mapmanager.h"

class IGuiSystem;
class IRender;
class IInputSystem;
class IAudioSystem;
class IGraphics;
class ISteamSystem;

extern IGuiSystem*      gui;
extern IRender*         render;
extern IInputSystem*    input;
extern IAudioSystem*    audio;
extern IGraphics*       graphics;
extern ISteamSystem*    steam;
extern bool             gSteamLoaded;

extern float            gFrameTime;
extern double           gCurTime;

constexpr int           CH_MAX_USERNAME_LEN = 256;

#define AUDIO_OPENAL 1

enum class GameState
{
	Menu,
	Loading,
	Running,
	Paused,
};


struct EditorView_t
{
	glm::mat4        aViewMat;
	glm::mat4        aProjMat;

	// projection matrix * view matrix
	glm::mat4        aProjViewMat;

	u32              aViewportIndex;

	// TODO: remove this from here, right now this is the same between ALL contexts
	glm::uvec2       aResolution;
	glm::uvec2       aOffset;

	glm::vec3        aPos{};
	glm::vec3        aAng{};

	glm::vec3        aForward{};
	glm::vec3        aRight{};
	glm::vec3        aUp{};
};


// multiple of these exist per map open
struct EditorContext_t
{
	// Viewport Data
	EditorView_t           aView;

	// Map Data
	SiduryMap              aMap;

	// Entities Selected - NOT IMPLEMENTED YET
	ChVector< ChHandle_t > aEntitiesSelected;
};


struct EditorData_t
{
	// Inputs
	glm::vec3 aMove{};
};


void         Game_SetPaused( bool paused );
bool         Game_IsPaused();

bool         Game_Init();
void         Game_Shutdown();

void         Game_Update( float frameTime );
void         Game_UpdateGame( float frameTime );  // epic name

void         Game_HandleSystemEvents();

void         Game_SetView( const glm::mat4& srViewMat );
void         Game_UpdateProjection();

inline float Game_GetCurTime()
{
	return gCurTime;
}


// -------------------------------------------------------------
// Editor Context

extern EditorData_t                   gEditorData;

extern ChHandle_t                     gEditorContextIdx;
// extern std::vector< EditorContext_t > gEditorContexts;
extern ResourceList< EditorContext_t > gEditorContexts;

constexpr u32                          CH_MAX_EDITOR_CONTEXTS    = 32;
constexpr ChHandle_t                   CH_INVALID_EDITOR_CONTEXT = CH_INVALID_HANDLE;

// return the context index, input a pointer address to the context if one was made
ChHandle_t                             Editor_CreateContext( EditorContext_t** spContext );
void                                   Editor_FreeContext( ChHandle_t sContext );

EditorContext_t*                       Editor_GetContext();
void                                   Editor_SetContext( ChHandle_t sContext );

// maybe do this?
// 
// void                                  Editor_SetCurrentContext( u32 sContextId );

