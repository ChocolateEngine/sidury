#pragma once

#include "../../chocolate/inc/shared/system.h"
#include "../../chocolate/inc/core/graphics.h"
#include "../../chocolate/inc/types/renderertypes.h"


struct Camera
{
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
	virtual void Update( float dt );
	virtual void SetupModels( float dt );
	virtual void CheckPaused(  );
	virtual void UpdateCamera(  );
	virtual void InputCamera( float dt );

	virtual void HandleSDLEvent( SDL_Event* e );

	BaseGuiSystem* apGui = NULL;
	BaseGraphicsSystem* apGraphics = NULL;

	glm::vec2 aMouseDelta;
	glm::vec2 aMousePos;

	float mX, mY;

	Camera aCamera;
	View aView;

	// make a timescale option?
	bool aPaused;
};
