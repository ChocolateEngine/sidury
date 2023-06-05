#pragma once

#include "util.h"
#include "types/transform.h"
#include "entity.h"
#include "game_physics.h"


struct UserCmd_t;


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
	bool            aIsLocalPlayer = false;  // only used on client, probably should split off from this
};


// =======================================================
// Systems - Logic that operates on the components


// This is really just the old Player class just jammed into one "system"
class PlayerMovement // : public ComponentSystem
{
  public:
	void             EnsureUserCmd( Entity player );

	void             OnPlayerSpawn( Entity player );
	void             OnPlayerRespawn( Entity player );

	void             MovePlayer( Entity player, UserCmd_t* spUserCmd );

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
	Entity           aPlayer     = CH_ENT_INVALID;
	UserCmd_t*       apUserCmd   = nullptr;

	CPlayerMoveData* apMove      = nullptr;
	CRigidBody*      apRigidBody = nullptr;
	Transform*       apTransform = nullptr;
	CCamera*         apCamera    = nullptr;
	CDirection*      apDir       = nullptr;

	// CPhysShape*      apPhysShape = nullptr;
	// CPhysObject*     apPhysObj   = nullptr;

	IPhysicsShape*   apPhysShape = nullptr;
	IPhysicsObject*  apPhysObj   = nullptr;

	// PhysCharacter* apPhys = nullptr;
	//PhysicsObject* apPhysObj = nullptr;
};


class PlayerManager // : public ComponentSystem
{
public:
	PlayerManager();
	~PlayerManager();

	static void             RegisterComponents();

	static void             CreateClient();
	static void             CreateServer();

	static void             DestroyClient();
	static void             DestroyServer();

	void                    Init();
	void                    Create( Entity player );
	void                    Spawn( Entity player );
	void                    Respawn( Entity player );
	void                    Update( float frameTime );  // ??
	void                    UpdateLocalPlayer();

	void                    UpdateView( CPlayerInfo* info, Entity player );
	void                    DoMouseLook( Entity player );

	std::vector< Entity > aPlayerList;
	
	PlayerMovement* apMove = nullptr;
};


PlayerManager* GetPlayers();


// convinence
inline auto GetPlayerMoveData( Entity ent ) { return Ent_GetComponent< CPlayerMoveData >( ent, "playerMoveData" ); }
inline auto GetPlayerZoom( Entity ent )     { return Ent_GetComponent< CPlayerZoom >(  ent, "playerZoom" ); }
inline auto GetPlayerInfo( Entity ent )     { return Ent_GetComponent< CPlayerInfo >( ent, "playerInfo" ); }
inline auto GetTransform( Entity ent )      { return Ent_GetComponent< Transform >( ent, "transform" ); }
inline auto GetCamera( Entity ent )         { return Ent_GetComponent< CCamera >( ent, "camera" ); }
inline auto GetRigidBody( Entity ent )      { return Ent_GetComponent< CRigidBody >( ent, "rigidBody" ); }
inline auto GetComp_Direction( Entity ent ) { return Ent_GetComponent< CDirection >( ent, "direction" ); }

