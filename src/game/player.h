#pragma once

#include "gamesystem.h"
#include "util.h"


#define FL_NONE 0
#define FL_ONGROUND 1


struct Direction
{
	Direction():
		forward(0, 0, 0),
		back(0, 0, 0),
		up(0, 0, 0),
		down(0, 0, 0),
		right(0, 0, 0),
		left(0, 0, 0)
	{
	}

	void Update( const glm::vec3& forward, const glm::vec3& up, const glm::vec3& right)
	{
		this->forward = forward;
		this->up = up;
		this->right = right;

		back = -forward;
		down = -up;
		left = -right;
	}

	glm::vec3 forward;
	glm::vec3 back;
	glm::vec3 up;
	glm::vec3 down;
	glm::vec3 right;
	glm::vec3 left;
};


class Player
{
public:
	Player(  );
	~Player(  );

	void Spawn(  );
	void Respawn(  );
	void Update( float dt );

	void UpdateInputs(  );
	void UpdateView(  );

	void SetPos( const glm::vec3& origin );
	const glm::vec3& GetPos(  );

	// ==================================
	// quake movement
	// ==================================
	void DetermineMoveType(  );
	void UpdatePosition(  );

	void BaseFlyMove(  );
	void NoClipMove(  );
	void FlyMove(  );
	void WalkMove(  );

	void AddFriction(  );
	void Accelerate( float wishspeed, const glm::vec3 wishdir );
	//void AirAccelerate( float wishspeed, glm::vec3 wishveloc );
	void AddGravity(  );

	enum class MoveType
	{
		Walk,
		Water,
		NoClip,
		Fly,
	};

	MoveType aMoveType;

	int aFlags;

	glm::vec3 aOrigin;
	glm::vec3 aVelocity;

	float moveForward, moveSide;
	float maxSpeed;

	float mX, mY;
	Direction aDirection;
	Transform aTransform;
};


