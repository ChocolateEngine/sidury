#pragma once

#include "util.h"
#include "types/transform.h"
#include "entity.h"
#include "game_physics.h"


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
	NoClip,
	Fly,
	// Water,
	Count
};


// =======================================================
// Components

// TODO: Split up into more components, did this pretty lazily
struct CPlayerMoveData
{
	// General
	PlayerMoveType          aMoveType = PlayerMoveType::Walk;

	PlayerFlags             aPlayerFlags = PlyNone;
	PlayerFlags             aPrevPlayerFlags = PlyNone;

	float                   aMaxSpeed = 0.f;

	// View Bobbing
	float                   aWalkTime = 0.f;
	float                   aBobOffsetAmount = 0.f;

	// Smooth Duck
	float                   aPrevViewHeight = 0.f;
	float                   aTargetViewHeight = 0.f;
	float                   aOutViewHeight = 0.f;
	float                   aDuckDuration = 0.f;
	float                   aDuckTime = 0.f;

	// Sound Effects
	double                  aLastStepTime = 0.f;

	std::vector< Handle >   aStepSounds;
	std::vector< Handle >   aImpactSounds;

	// Physics

	// The maximum angle of slope that character can still walk on (radians and put in cos())
	float                   aMaxSlopeAngle = 45.f * (M_PI / 180.f);

	IPhysicsShape*          apPhysShape = nullptr;
	IPhysicsObject*         apPhysObj   = nullptr;
	
	IPhysicsObject*         apGroundObj = nullptr;
	glm::vec3               aGroundPosition{};
	glm::vec3               aGroundNormal{};
	// glm::vec3               aGroundVelocity{};
	// Handle                  aGroundMaterial;
};


struct CPlayerZoom
{
	float aOrigFov{};
	float aNewFov{};
	float aZoomChangeFov{};
	float aZoomTime{};
	float aZoomDuration{};
};


struct CPlayerInfo
{
	Entity          aEnt;
	std::string     aName = "";
	bool            aIsLocalPlayer = false;
};


// =======================================================
// Systems - Logic that operates on the components


// This is really just the old Player class just jammed into one "system"
class PlayerMovement : public System
{
  public:
	void             OnPlayerSpawn( Entity player );
	void             OnPlayerRespawn( Entity player );

	void             MovePlayer( Entity player );

	void             UpdateInputs();
	void             UpdatePosition( Entity player );

	void             DisplayPlayerStats( Entity player ) const;
	float            GetViewHeight();

	void             SetPos( const glm::vec3& origin );
	const glm::vec3& GetPos() const;

	void             SetAng( const glm::vec3& angles );
	const glm::vec3& GetAng() const;

	void             DetermineMoveType();

	// std::string             GetStepSound(  );
	Handle           GetStepSound();

	void             PlayStepSound();
	void             StopStepSound( bool force = false );  // Temp Hack for sound system

	// Sound for colliding onto stuff or landing on the ground
	void             PlayImpactSound();
	void             StopImpactSound();  // Temp Hack for sound system

	bool             CalcOnGround();
	bool             IsOnGround();
	bool             WasOnGround();

	float            GetMoveSpeed( glm::vec3& wishDir, glm::vec3& wishVel );
	float            GetMaxSpeed();
	float            GetMaxSpeedBase();
	float            GetMaxSprintSpeed();
	float            GetMaxDuckSpeed();

	void             BaseFlyMove();
	void             NoClipMove();
	void             FlyMove();
	void             WalkMove();

	void             WalkMovePostPhys();  // um

	void             DoSmoothDuck();
	void             DoSmoothLand( bool wasOnGround );
	void             DoViewBob();
	void             DoViewTilt();

	// TODO: remove these first 2 when physics finally works to a decent degree
	void             AddFriction();
	void             Accelerate( float wishSpeed, glm::vec3 wishDir, bool inAir = false );

	void             SetMoveType( CPlayerMoveData& move, PlayerMoveType type );
	void             SetCollisionEnabled( bool enable );
	void             SetGravity( const glm::vec3& gravity );
	void             EnableGravity( bool enabled );

	inline bool      IsInSprint() { return apMove ? apMove->aPlayerFlags & PlyInSprint : false; }
	inline bool      IsInDuck() { return apMove ? apMove->aPlayerFlags & PlyInDuck : false; }

	inline bool      WasInSprint() { return apMove ? apMove->aPrevPlayerFlags & PlyInSprint : false; }
	inline bool      WasInDuck() { return apMove ? apMove->aPrevPlayerFlags & PlyInDuck : false; }

	// store it for use in functions, save on GetComponent calls
	Entity           aPlayer     = ENT_INVALID;
	CPlayerMoveData* apMove      = nullptr;
	CRigidBody*      apRigidBody = nullptr;
	Transform*       apTransform = nullptr;
	CCamera*         apCamera    = nullptr;
	CDirection*      apDir       = nullptr;

	// PhysCharacter* apPhys = nullptr;
	//PhysicsObject* apPhysObj = nullptr;
};


class PlayerManager: public System
{
public:
	PlayerManager();
	~PlayerManager();

	void                    Init();
	Entity                  Create();
	void                    Spawn( Entity player );
	void                    Respawn( Entity player );
	void                    Update( float frameTime );  // ??

	void                    UpdateView( CPlayerInfo& info, Entity player );
	void                    DoMouseLook( Entity player );

	std::vector< Entity > aPlayerList;
	
	PlayerMovement* apMove = nullptr;
};


// class CL_PlayerManager: public System
// {
// public:
// 	CL_PlayerManager();
// 	~CL_PlayerManager();
// 
// 	void                    Init();
// 	Entity                  Create();
// 	void                    Spawn( Entity player );
// 	void                    Respawn( Entity player );
// 	void                    Update( float frameTime );  // ??
// 
// 	void                    UpdateView( CPlayerInfo& info, Entity player );
// 	void                    DoMouseLook( Entity player );
// 
// 	std::vector< Entity > aPlayerList;
// 	
// 	PlayerMovement* apMove = nullptr;
// };


// convinence
inline auto& GetPlayerMoveData( Entity ent )	    { return entities->GetComponent< CPlayerMoveData >( ent ); }
inline auto& GetPlayerZoom( Entity ent )            { return entities->GetComponent< CPlayerZoom >( ent ); }
inline auto& GetPlayerInfo( Entity ent )            { return entities->GetComponent< CPlayerInfo >( ent ); }
inline auto& GetTransform( Entity ent )             { return entities->GetComponent< Transform >( ent ); }
inline auto& GetCamera( Entity ent )                { return entities->GetComponent< CCamera >( ent ); }
inline auto& GetRigidBody( Entity ent )             { return entities->GetComponent< CRigidBody >( ent ); }
// inline HModel GetModel( Entity ent )                 { return entities->GetComponent< HModel >( ent ); }


extern PlayerManager* players;
// extern CL_PlayerManager* cl_players;
// extern PlayerManager*    sv_players;

