#pragma once

#include "gamesystem.h"

class Player
{
public:
	Player(  );
	~Player(  );

	void Spawn(  );
	void Update( float dt );
	void OldMove(  );

	void UpdateInputs(  );
	void UpdateView(  );

	void SetPos( const glm::vec3& origin );
	const glm::vec3& GetPos(  );

	// ==================================
	// quake movement
	// ==================================
	void BaseFlyMove(  );
	void NoClipMove(  );
	void FlyMove(  );
	void WalkMove(  );
	void AddFriction(  );
	void Accelerate( float wishspeed, const glm::vec3 wishdir );
	//void AirAccelerate( float wishspeed, glm::vec3 wishveloc );
	void AddGravity(  );

	glm::vec3 aOrigin;
	glm::vec3 aVelocity;

	float moveForward, moveSide;

	float mX, mY;
	Camera &aCamera;
};


