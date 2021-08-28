#pragma once

#include "../../chocolate/inc/shared/system.h"
#include "../../chocolate/inc/core/graphics.h"
#include "../../chocolate/inc/types/renderertypes.h"


struct Camera
{
	Camera():
		forward(0, 0, 0),
		back(0, 0, 0),
		up(0, 0, 0),
		down(0, 0, 0),
		right(0, 0, 0),
		left(0, 0, 0)
	{
		transform = {};
	}

	glm::vec3 forward;
	glm::vec3 back;
	glm::vec3 up;
	glm::vec3 down;
	glm::vec3 right;
	glm::vec3 left;

	Transform transform;
};


class GameSystem : public BaseSystem
{
public:
	typedef BaseSystem BaseClass;

	GameSystem (  );
	~GameSystem (  );

	virtual void Init(  );
	virtual void Update( float frameTime );
	virtual void SetupModels( float frameTime );
	virtual void CheckPaused(  );
	virtual void ResetInputs(  );

	virtual void HandleSDLEvent( SDL_Event* e );

	BaseGuiSystem* apGui = NULL;
	BaseGraphicsSystem* apGraphics = NULL;

	glm::vec2 aMouseDelta;
	glm::vec2 aMousePos;

	float aFrameTime;

	Camera aCamera;
	View aView;

	// make a timescale option?
	bool aPaused;
};


extern GameSystem* g_pGame;

