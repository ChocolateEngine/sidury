#pragma once

#include "gamesystem.h"
#include "util.h"
#include "types/transform.h"
#include "entity.h"
#include "physics.h"


enum class SurfaceType
{
	Dirt,
};


enum PlayerFlags_
{
	PlyNone = (1 << 0),
	PlyOnGround = (1 << 1),
	PlyInSprint = (1 << 2),
	PlyInDuck = (1 << 3),
};

typedef unsigned char PlayerFlags;


enum class PlayerMoveType
{
	Walk,
	Water,
	NoClip,
	Fly,
};


// =======================================================
// Components

// TODO: Split up into more components, did this pretty lazily
struct CPlayerMoveData
{
	PlayerMoveType aMoveType = PlayerMoveType::Walk;

	PlayerFlags aPlayerFlags = PlyNone;
	PlayerFlags aPrevPlayerFlags = PlyNone;

	float aMaxSpeed = 0.f;

	// Direction Vectors
	glm::vec3 aForward = {};
	glm::vec3 aUp = {};
	glm::vec3 aRight = {};

	// View Bobbing
	float aWalkTime = 0.f;
	float aBobOffsetAmount = 0.f;

	// Smooth Duck
	float aPrevViewHeight = 0.f;
	float aTargetViewHeight = 0.f;
	float aDuckLerpGoal = 0.f;
	float aDuckLerp = 0.f;
	float aPrevDuckLerp = 0.f;

	// Step Sound
	double aLastStepTime = 0.f;
	AudioStream* apStepSound = nullptr;
};


struct CPlayerInfo
{
	Entity aEnt;
	std::string aName = "";
	bool aIsLocalPlayer = false;
};


// =======================================================
// Systems - Logic that operates on the components


// This is really just the old Player class just jammed into one "system"
class PlayerMovement: public System
{
public:
	void                    OnPlayerSpawn( Entity player );
	void                    OnPlayerRespawn( Entity player );

	void                    MovePlayer( Entity player );

	void                    UpdateInputs(  );
	void                    UpdatePosition( Entity player );

	void                    DisplayPlayerStats( Entity player ) const;
	float                   GetViewHeight(  );

	void                    SetPos( const glm::vec3& origin );
	const glm::vec3&        GetPos(  ) const;

	void                    SetAng( const glm::vec3& angles );
	const glm::vec3&        GetAng(  ) const;

	void                    DetermineMoveType(  );

	void                    PlayStepSound(  );
	void                    StopStepSound( bool force = false );  // Temp Hack for sound system

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

	void                    SetMoveType( CPlayerMoveData& move, PlayerMoveType type );
	void                    SetCollisionEnabled( bool enable );
	void                    SetGravity( const glm::vec3& gravity );
	void                    EnableGravity( bool enabled );

	inline bool             IsInSprint(  )      { return apMove ? apMove->aPlayerFlags & PlyInSprint : false; }
	inline bool             IsInDuck(  )        { return apMove ? apMove->aPlayerFlags & PlyInDuck : false; }

	inline bool             WasInSprint(  )     { return apMove ? apMove->aPrevPlayerFlags & PlyInSprint : false; }
	inline bool             WasInDuck(  )       { return apMove ? apMove->aPrevPlayerFlags & PlyInDuck : false; }

	// store it for use in functions, save on GetComponent calls
	Entity aPlayer = ENT_INVALID;
	CPlayerMoveData* apMove = nullptr;
	CRigidBody* apRigidBody = nullptr;
	Transform* apTransform = nullptr;
	CCamera* apCamera = nullptr;
	//PhysicsObject* apPhysObj = nullptr;
};


class PlayerManager: public System
{
public:
	PlayerManager(  );
	~PlayerManager(  );

	void                    Init(  );
	Entity                  Create(  );
	void                    Spawn( Entity player );
	void                    Respawn( Entity player );
	void                    Update( float frameTime );  // ??

	void                    UpdateView( Entity player );
	void                    DoMouseLook( Entity player );

	std::vector< Entity > aPlayerList;
	
	PlayerMovement* apMove = nullptr;
};


// convinence
inline auto& GetPlayerMoveData( Entity ent )	    { return entities->GetComponent< CPlayerMoveData >( ent ); }
inline auto& GetPlayerInfo( Entity ent )            { return entities->GetComponent< CPlayerInfo >( ent ); }
inline auto& GetTransform( Entity ent )             { return entities->GetComponent< Transform >( ent ); }
inline auto& GetCamera( Entity ent )                { return entities->GetComponent< CCamera >( ent ); }
inline auto& GetRigidBody( Entity ent )             { return entities->GetComponent< CRigidBody >( ent ); }
inline auto& GetModel( Entity ent )                 { return entities->GetComponent< Model >( ent ); }


extern PlayerManager* players;

