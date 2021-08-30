#pragma once

#include "../../chocolate/inc/shared/system.h"
#include "../../chocolate/inc/core/graphics.h"
#include "../../chocolate/inc/types/renderertypes.h"


class GameSystem : public BaseSystem
{
public:
	typedef BaseSystem BaseClass;

	GameSystem (  );
	~GameSystem (  );

	virtual void Init(  );
	virtual void InitConsoleCommands(  );
	virtual void Update( float frameTime );
	virtual void SetupModels( float frameTime );
	virtual void CheckPaused(  );
	virtual void ResetInputs(  );

	void SetViewMatrix( const glm::mat4& viewMatrix );

	virtual void HandleSDLEvent( SDL_Event* e );

	BaseGuiSystem* apGui = NULL;
	BaseGraphicsSystem* apGraphics = NULL;

	std::vector< Model* > aModels;

	glm::vec2 aMouseDelta;
	glm::vec2 aMousePos;

	float aFrameTime;

	View aView;

	// make a timescale option?
	bool aPaused;
};


extern GameSystem* g_pGame;

