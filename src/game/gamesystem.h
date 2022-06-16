#pragma once

#include "core/core.h"
#include "system.h"
#include "iinput.h"
#include "iaudio.h"
#include "igui.h"
#include "graphics/igraphics.h"
#include "graphics/renderertypes.h"

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


extern BaseGuiSystem* gui;
extern BaseGraphicsSystem* graphics;
extern BaseInputSystem* input;
extern BaseAudioSystem* audio;


enum class GameState
{
	Menu,
	Running,
};


class GameSystem : public BaseSystem
{
public:
	typedef BaseSystem BaseClass;

	GameSystem (  );
	~GameSystem (  );

	void Init(  );

	void LoadModules(  );
	void RegisterKeys(  );

	bool InMap();

	void Update( float frameTime );
	void GameUpdate( float frameTime );

	void SetupModels( float frameTime );
	void CheckPaused(  );
	void ResetInputs(  );
	void UpdateAudio(  );

	void SetViewMatrix( const glm::mat4& viewMatrix );

	void HandleSystemEvents();

	std::vector< Model* > aModels;

	Entity aLocalPlayer = ENT_INVALID;

	float aFrameTime = 0.f;
	double aCurTime = 0.f;  // really should be size_t, but then that would be a little weird with using it

	View aView;

	// make a timescale option?
	bool aPaused;
};


extern GameSystem* game;
extern IMaterialSystem* materialsystem; // bruh
