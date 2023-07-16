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
#include "game_rules.h"

class IGuiSystem;
class IRender;
class IInputSystem;
class IAudioSystem;
class ISteamSystem;

extern IGuiSystem*      gui;
extern IRender*         render;
extern IInputSystem*    input;
extern IAudioSystem*    audio;
extern ISteamSystem*    steam;
extern bool             gSteamLoaded;

extern ViewportCamera_t gView;
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


bool         Game_InMap();

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

