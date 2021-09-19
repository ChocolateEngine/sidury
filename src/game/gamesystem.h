#pragma once

#include "../../chocolate/inc/shared/system.h"
#include "../../chocolate/inc/shared/baseinput.h"
#include "../../chocolate/inc/core/graphics.h"
#include "../../chocolate/inc/types/renderertypes.h"

#include "physics.h"
//#include "player.h"

class Player;


class GameSystem : public BaseSystem
{
public:
	typedef BaseSystem BaseClass;

	GameSystem (  );
	~GameSystem (  );

	virtual void Init(  );

	virtual void LoadModules(  );
	virtual void LoadWorld(  );
	virtual void RegisterKeys(  );
	virtual void CreateEntities(  );

	virtual void InitConsoleCommands(  );
	virtual void Update( float frameTime );
	virtual void SetupModels( float frameTime );
	virtual void CheckPaused(  );
	virtual void ResetInputs(  );

	void SetViewMatrix( const glm::mat4& viewMatrix );

	virtual void HandleSDLEvent( SDL_Event* e );

	BaseGuiSystem* apGui = NULL;
	BaseGraphicsSystem* apGraphics = NULL;
	BaseInputSystem* apInput = NULL;

	std::vector< Model* > aModels;

	Player* aLocalPlayer = NULL;

#if !NO_BULLET_PHYSICS
	PhysicsEnvironment* apPhysEnv;
#endif

	float aFrameTime;

	View aView;

	// make a timescale option?
	bool aPaused;
};


extern GameSystem* g_pGame;

