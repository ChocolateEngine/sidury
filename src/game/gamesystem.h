#pragma once

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

constexpr glm::vec3 vec3_forward(1, 0, 0);
constexpr glm::vec3 vec3_right(0, 1, 0);
constexpr glm::vec3 vec3_up(0, 0, 1);

#include "physics.h"
#include "entity.h"


class GameSystem : public BaseSystem
{
public:
	typedef BaseSystem BaseClass;

	GameSystem (  );
	~GameSystem (  );

	virtual void Init(  );

	virtual void LoadModules(  );
	virtual void UnloadWorld(  );
	virtual void LoadWorld( const std::string& path, bool rotate );
	virtual void RegisterKeys(  );
	virtual void CreateEntities(  );

	virtual void InitConsoleCommands(  );
	virtual void Update( float frameTime );
	virtual void SetupModels( float frameTime );
	virtual void CheckPaused(  );
	virtual void ResetInputs(  );
	virtual void UpdateAudio(  );

	void SetViewMatrix( const glm::mat4& viewMatrix );

	virtual void HandleSDLEvent( SDL_Event* e );

	BaseGuiSystem* apGui = NULL;
	BaseGraphicsSystem* apGraphics = NULL;
	BaseInputSystem* apInput = NULL;
	BaseAudioSystem* apAudio = NULL;

	std::vector< Model* > aModels;

	Entity aLocalPlayer = ENT_INVALID;

#if !NO_BULLET_PHYSICS
	PhysicsEnvironment* apPhysEnv;
#endif

	float aFrameTime = 0.f;
	double aCurTime = 0.f;  // really should be size_t, but then that would be a little weird with using it

	View aView;

	// make a timescale option?
	bool aPaused;
};


extern GameSystem* game;
extern IMaterialSystem* materialsystem; // bruh
