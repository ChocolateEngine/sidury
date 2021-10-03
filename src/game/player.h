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


enum class StepType
{
	Dirt,
};


enum class StepSpeed
{
	Sneak,
	Walk,
	Run,
};


class Player;


#define VIEW_LERP_CLASS 0

#if VIEW_LERP_CLASS
class ViewLerp
{
public:
	ViewLerp( Player* player );
	~ViewLerp(  );

	glm::vec3 LerpView(  );

	Player* apPlayer;

	bool inLerp = false;
	float prevViewHeight = 0.f;

	glm::vec3 lerpGoal = {};
	glm::vec3 lerpOut = {};
	glm::vec3 lerpPrev = {};
};
#endif


class Player
{
public:
	Player(  );
	~Player(  );

	void Spawn(  );
	void Respawn(  );
	void Update( float dt );
	void DisplayPlayerStats(  );

	void UpdateInputs(  );
	void UpdateView(  );
	float GetViewHeight(  );

	void SetPos( const glm::vec3& origin );
	const glm::vec3& GetPos(  );

	void SetPosVel( const glm::vec3& origin );

	// returns velocity multiplied by frametime
	glm::vec3 GetFrameTimeVelocity(  );

	// ==================================
	// movement
	// ==================================
	void DetermineMoveType(  );
	void UpdatePosition(  );

	void PlayStepSound(  );

	bool IsOnGround(  );
	bool WasOnGround(  );
	void DoRayCollision(  );

	float GetMoveSpeed( glm::vec3 &wishDir, glm::vec3 &wishVel );
	void BaseFlyMove(  );
	void NoClipMove(  );
	void FlyMove(  );
	void WalkMove(  );

	void DoSmoothDuck(  );
	void DoSmoothLand( bool wasOnGround );

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
	float aMaxSpeed;

	bool aOnGround = false;
	bool aWasOnGround = false;

	float mX, mY;
	Direction aDirection;
	Transform aTransform;
	
	double aLastStepTime = 0.f;
	AudioStream* apStepSound = nullptr; // uh

#if !NO_BULLET_PHYSICS
	PhysicsObject* apPhysObj;
#endif
};


