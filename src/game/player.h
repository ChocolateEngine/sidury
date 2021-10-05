#pragma once

#include "gamesystem.h"
#include "util.h"
#include "../../chocolate/inc/types/transform.h"
#include "physics.h"


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


enum PlayerFlags_
{
	PlyNone = 0,
	PlyOnGround,
	PlyInSprint,
	PlyInDuck
};

typedef unsigned char PlayerFlags;

class Player
{
public:
	Player(  );
	~Player(  );

	enum class MoveType
	{
		Walk,
		Water,
		NoClip,
		Fly,
	};

	// =============================================================
	// General
	// =============================================================

	void                    Spawn(  );
	void                    Respawn(  );
	void                    Update( float dt );

	void                    UpdateInputs(  );
	void                    UpdateView(  );

	void                    DisplayPlayerStats(  );
	float                   GetViewHeight(  );

	void                    SetPos( const glm::vec3& origin );
	const glm::vec3&        GetPos(  ) const;

	void                    SetAng( const glm::vec3& angles );
	const glm::vec3&        GetAng(  ) const;

	void                    SetPosVel( const glm::vec3& origin );

	/* Returns velocity multiplied by frametime */
	glm::vec3               GetFrameTimeVelocity(  );

	// =============================================================
	// Movement
	// =============================================================

	void                    DetermineMoveType(  );
	void                    UpdatePosition(  );

	void                    PlayStepSound(  );

	bool                    IsOnGround(  );
	bool                    WasOnGround(  );
	void                    DoRayCollision(  );

	float                   GetMoveSpeed( glm::vec3 &wishDir, glm::vec3 &wishVel );
	float                   GetMaxSpeed(  );
	float                   GetMaxSpeedBase(  );
	float                   GetMaxSprintSpeed(  );
	float                   GetMaxDuckSpeed(  );

	void                    BaseFlyMove(  );
	void                    NoClipMove(  );
	void                    FlyMove(  );
	void                    WalkMove(  );

	void                    DoSmoothDuck(  );
	void                    DoSmoothLand( bool wasOnGround );
	void                    DoViewBob(  );
	void                    DoViewTilt(  );

	void                    AddFriction(  );
	void                    AddGravity(  );
	void                    Accelerate( float wishSpeed, glm::vec3 wishDir, bool inAir = false );

	void                    SetMoveType( MoveType type );
	void                    SetCollisionEnabled( bool enable );
	// void                    SetGravity( const glm::vec3& gravity );
	void                    EnableGravity( bool enabled );

	inline bool             IsInSprint(  )      { return aPlayerFlags & PlyInSprint; }
	inline bool             IsInDuck(  )        { return aPlayerFlags & PlyInDuck; }

	inline bool             WasInSprint(  )     { return aPrevPlayerFlags & PlyInSprint; }
	inline bool             WasInDuck(  )       { return aPrevPlayerFlags & PlyInDuck; }

	// =============================================================
	// Vars
	// =============================================================

	MoveType aMoveType = MoveType::Walk;

	glm::vec3 aOrigin = {};
	glm::vec3 aVelocity = {};
	glm::vec3 aMove = {};
	glm::vec3 aViewOffset = {};
	glm::vec3 aViewAngOffset = {};
	float aMaxSpeed = 0.f;

	PlayerFlags aPlayerFlags = PlyNone;
	PlayerFlags aPrevPlayerFlags = PlyNone;

	float mX, mY = 0.f;
	Transform aTransform = {};

	glm::vec3 aForward = {};
	glm::vec3 aUp = {};
	glm::vec3 aRight = {};
	
	double aLastStepTime = 0.f;
	AudioStream* apStepSound = nullptr; // uh

#if !NO_BULLET_PHYSICS
	PhysicsObject* apPhysObj;
#endif
};


