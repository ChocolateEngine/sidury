#pragma once

#include "gamesystem.h"
#include "util.h"
#include "../../chocolate/inc/types/transform.h"
#include "physics.h"


#define FL_NONE 0
#define FL_ONGROUND 1


// should probably move into transform
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

	void SetPosVel( const glm::vec3& origin );

	// returns velocity multiplied by frametime
	const glm::vec3& GetFrameTimeVelocity(  );

	// ==================================
	// movement
	// ==================================
	void DetermineMoveType(  );
	void UpdatePosition(  );

	bool IsOnGround(  );
	void DoRayCollision(  );

	float GetMoveSpeed( glm::vec3 &wishDir, glm::vec3 &wishVel );
	void BaseFlyMove(  );
	void NoClipMove(  );
	void FlyMove(  );
	void WalkMove(  );

	void AddFriction(  );
	void AddGravity(  );
	void Accelerate( float wishSpeed, glm::vec3 wishDir, bool inAir = false );

	enum class MoveType
	{
		Walk,
		Water,
		NoClip,
		Fly,
	};

	void SetMoveType( MoveType type );
	void SetCollisionEnabled( bool enable );
	// void SetGravity( const glm::vec3& gravity );
	void EnableGravity( bool enabled );

	MoveType aMoveType;

	int aFlags;

	glm::vec3 aOrigin;
	glm::vec3 aVelocity;
	glm::vec3 aMove;
	glm::vec3 aViewOffset;
	float maxSpeed;

	float mX, mY;
	Direction aDirection;
	Transform aTransform;

#if !NO_BULLET_PHYSICS
	PhysicsObject* apPhysObj;
#endif
};


