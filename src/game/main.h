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

class BaseGuiSystem;
class IRender;
class BaseInputSystem;
class BaseAudioSystem;

extern BaseGuiSystem*   gui;
extern IRender*         render;
extern BaseInputSystem* input;
extern BaseAudioSystem* audio;

extern ViewportCamera_t gView;
extern float            gFrameTime;
extern double           gCurTime;

enum class GameState
{
	Menu,
	Loading,
	Running,
	Paused,
};


void               Game_RegisterKeys();

bool               Game_InMap();

void               Game_SetPaused( bool paused );
bool               Game_IsPaused();
void               Game_CheckPaused();

bool               Game_Init();
void               Game_Shutdown();

void               Game_Update( float frameTime );
void               Game_UpdateGame( float frameTime );  // epic name

void               Game_SetupModels( float frameTime );
void               Game_ResetInputs();
void               Game_UpdateAudio();

void               Game_HandleSystemEvents();

void               Game_SetView( const glm::mat4& srViewMat );
void               Game_UpdateProjection();

