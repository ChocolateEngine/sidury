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


class GameSystem : public BaseSystem
{
public:
	GameSystem();
	~GameSystem();

	bool Init() override;
	void Update( float frameTime ) override;
};

extern GameSystem* game;

void               Game_RegisterKeys();

bool               Game_InMap();

void               Game_SetPaused( bool paused );
bool               Game_IsPaused();
void               Game_CheckPaused();

void               Game_Update( float frameTime );

void               Game_SetupModels( float frameTime );
void               Game_ResetInputs();
void               Game_UpdateAudio();

void               Game_HandleSystemEvents();

void               Game_SetView( const glm::mat4& srViewMat );
void               Game_UpdateProjection();

// TODO: MOVE THESE TO CORE !!!
// void               Util_GetDirectionVectors( const glm::vec3& srAngles, glm::vec3* spForward, glm::vec3* spRight = nullptr, glm::vec3* spUp = nullptr );
extern glm::vec3          Util_VectorToAngles( const glm::vec3& forward );
glm::vec3          Util_VectorToAngles( const glm::vec3& srForward, const glm::vec3& srUp );

