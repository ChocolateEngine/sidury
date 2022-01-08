#include "player.h"
#include "util.h"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <algorithm>
#include <cmath>


CONVAR( in_forward, 0 );
CONVAR( in_side, 0 );
CONVAR( in_duck, 0 );
CONVAR( in_sprint, 0 );
CONVAR( in_jump, 0 );

extern ConVarRef en_timescale;

CONVAR( sv_sprint_mult, 2.4 );
//CONVAR( sv_sprint_mult, 4 );  // Temp until physics works
CONVAR( sv_duck_mult, 0.5 );

constexpr float DEFAULT_SPEED = 250.f;

ConVar forward_speed( "sv_forward_speed", DEFAULT_SPEED );
ConVar side_speed( "sv_side_speed", DEFAULT_SPEED );  // 350.f
ConVar max_speed( "sv_max_speed", DEFAULT_SPEED );  // 320.f

ConVar stop_speed( "sv_stop_speed", 75 );
ConVar accel_speed( "sv_accel_speed", 10 );
ConVar accel_speed_air( "sv_accel_speed_air", 30 );
ConVar sv_friction( "sv_friction", 8 );  // 4.f
ConVar jump_force( "sv_jump_force", 250 );

CONVAR( phys_friction, 0.1 );

// lerp the friction maybe?
//CONVAR( sv_new_movement, 1 );
//CONVAR( sv_friction_new, 8 );  // 4.f

CONVAR( sv_gravity, 800 );
CONVAR( ground_pos, 225 );

// hack until i add config file support
#ifdef _MSC_VER
CONVAR( in_sensitivity, 0.025 );
#else
CONVAR( in_sensitivity, 0.1 );
#endif

CONVAR( cl_stepspeed, 200 );
CONVAR( cl_steptime, 0.25 );
CONVAR( cl_stepduration, 0.22 );

CONVAR( cl_view_height, 67 );  // 56
CONVAR( cl_view_height_duck, 36 );  // 24
CONVAR( cl_view_height_lerp, 15 );  // 0.015

CONVAR( cl_smooth_land, 1 );
CONVAR( cl_smooth_land_lerp, 30 );  // 0.015 // 150?
CONVAR( cl_smooth_land_scale, 600 );
CONVAR( cl_smooth_land_up_scale, 50 );
CONVAR( cl_smooth_land_view_scale, 0.05 );
CONVAR( cl_smooth_land_view_offset, 0.5 );

// multiplies the final velocity by this amount when setting the player position,
// a workaround for quake movement values not working correctly when lowered
#if !BULLET_PHYSICS
CONVAR( velocity_scale, 0.025 );
#else
CONVAR( velocity_scale, 1.0 );
CONVAR( velocity_scale2, 0.01 );
#endif

CONVAR( cl_thirdperson, 0 );
CONVAR( cl_cam_x, 0 );
CONVAR( cl_cam_y, 0 );
CONVAR( cl_cam_z, -2.5 );

constexpr float PLAYER_MASS = 200.f;

// TEMP
// #define SPAWN_POS 1085.69824, 260, 644.222046
//#define SPAWN_POS 1085.69824 * velocity_scale, 260 * velocity_scale, 644.222046 * velocity_scale
// #define SPAWN_POS 1085.69824 * velocity_scale, -46, 644.222046 * velocity_scale
// #define SPAWN_POS 10.8569824, -2.60, 6.44222046
// #define SPAWN_POS -26.5, -45, -8.2

//constexpr glm::vec3 SPAWN_POS = {0, 0, 0};
//constexpr glm::vec3 SPAWN_ANG = {0, 45, 0};

const glm::vec3 SPAWN_POS = {0, 0, 0};
const glm::vec3 SPAWN_ANG = {0, 45, 0};


CON_COMMAND( respawn )
{
	players->Respawn( game->aLocalPlayer );
}

CON_COMMAND( reset_velocity )
{
	auto& rigidBody = entities->GetComponent< CRigidBody >( game->aLocalPlayer );
	rigidBody.aVel = {0, 0, 0};
}


#define GET_KEY( key ) game->apInput->GetKeyState(key)

#define KEY_PRESSED( key ) game->apInput->KeyPressed(key)
#define KEY_RELEASED( key ) game->apInput->KeyReleased(key)
#define KEY_JUST_PRESSED( key ) game->apInput->KeyJustPressed(key)
#define KEY_JUST_RELEASED( key ) game->apInput->KeyJustReleased(key)


// glm::normalize doesn't return a float
// move to util.cpp?
float vec3_norm(glm::vec3& v)
{
	float length = sqrt(glm::dot(v, v));

	if (length)
		v *= 1/length;

	return length;
}

// ============================================================


PlayerManager* players = nullptr;

// HACKY HACKY HACK
#if BULLET_PHYSICS
PhysicsObject* apPhysObj = nullptr;
#endif

PlayerManager::PlayerManager()
{
}

PlayerManager::~PlayerManager()
{
}


void PlayerManager::Init(  )
{
	entities->RegisterComponent<CPlayerMoveData>();
	entities->RegisterComponent<CPlayerInfo>();
	entities->RegisterComponent<Model*>();
	entities->RegisterComponent<Model>();
	//entities->RegisterComponent<PhysicsObject*>();

	apMove = entities->RegisterSystem<PlayerMovement>();
}


Entity PlayerManager::Create(  )
{
	// Add Components
	Entity player = entities->CreateEntity();

	entities->AddComponent< CPlayerMoveData >( player );
	entities->AddComponent< CPlayerInfo >( player );
	entities->AddComponent< CRigidBody >( player );
	Transform& transform = entities->AddComponent< Transform >( player );
	entities->AddComponent< CCamera >( player );

	//Model* model = new Model;
	//game->apGraphics->LoadModel( "materials/models/protogen_wip_22/protogen_wip_22.obj", "", model );
	//entities->AddComponent( player, model );

	Model* model = &entities->AddComponent< Model >( player );
	game->apGraphics->LoadModel( "materials/models/protogen_wip_22/protogen_wip_22.obj", "", model );

#if BULLET_PHYSICS
	PhysicsObjectInfo physInfo( ShapeType::Cylinder );
	physInfo.mass = PLAYER_MASS;
	physInfo.transform = transform;
	//physInfo.callbacks = true;
	//physInfo.collisionType = CollisionType::Kinematic;
	// physInfo.bounds = glm::vec3(14, 14, 32);  * velocity_scale.GetFloat();
	//physInfo.bounds = glm::vec3(14, 14, 32) * 0.025f;
	physInfo.bounds = glm::vec3(14, 14, 32);
	//physInfo.bounds = {14, 32, 14};  // bruh wtf why y up for this even though it's a btCylinderShapeZ

	// PhysicsObject* physObj = game->apPhysEnv->CreatePhysicsObject( physInfo );
	apPhysObj = game->apPhysEnv->CreatePhysicsObject( physInfo );
	apPhysObj->SetAlwaysActive( true );
	apPhysObj->SetContinuousCollisionEnabled( true );
	apPhysObj->SetWorldTransform( transform );
	apPhysObj->SetLinearVelocity( {0, 0, 0} );
	apPhysObj->SetAngularFactor( {0, 0, 0} );
	apPhysObj->SetSleepingThresholds( 0, 0 );
	apPhysObj->SetFriction( phys_friction.GetFloat() );

	//entities->AddComponent( player, physObj );
#endif

	aPlayerList.push_back( player );

	return player;
}


void PlayerManager::Spawn( Entity player )
{
	apMove->OnPlayerSpawn( player );

	Respawn( player );
}


void PlayerManager::Respawn( Entity player )
{
	auto& rigidBody = GetRigidBody( player );
	auto& transform = GetTransform( player );
	auto& camTransform = GetCamera( player ).aTransform;

	// HACK FOR RIVERHOUSE SPAWN POS
	transform.aPos = SPAWN_POS;
	transform.aAng = {0, SPAWN_ANG.y, 0};
	camTransform.aAng = SPAWN_ANG;
	rigidBody.aVel = {0, 0, 0};
	rigidBody.aAccel = {0, 0, 0};

	apMove->OnPlayerRespawn( player );
}


void PlayerManager::Update( float frameTime )
{
	for ( Entity player: aPlayerList )
	{
		if ( !game->aPaused )
		{
			DoMouseLook( player );
			apMove->MovePlayer( player );
		}

		auto& playerInfo = entities->GetComponent< CPlayerInfo >( player );

		if ( cl_thirdperson.GetBool() || !playerInfo.aIsLocalPlayer )
		{
			auto& model = entities->GetComponent< Model >( player );

			for ( auto& mesh: model.GetModelData().aMeshes )
				materialsystem->AddRenderable( mesh );
		}

		UpdateView( player );
	}
}


inline float DegreeConstrain( float num )
{
	num = std::fmod(num, 360.0f);
	return (num < 0.0f) ? num += 360.0f : num;
}


auto ClampAngles = []( Transform& transform, CCamera& camera )
{
	transform.aAng[YAW] = DegreeConstrain( transform.aAng[YAW] );
	camera.aTransform.aAng[YAW] = DegreeConstrain( camera.aTransform.aAng[YAW] );
	camera.aTransform.aAng[PITCH] = std::clamp( camera.aTransform.aAng[PITCH], -90.0f, 90.0f );
};


void PlayerManager::DoMouseLook( Entity player )
{
	auto& transform = GetTransform( player );
	auto& camera = GetCamera( player );

	const glm::vec2 mouse = in_sensitivity.GetFloat() * glm::vec2(game->apInput->GetMouseDelta());

	// transform.aAng[PITCH] = -mouse.y;
	camera.aTransform.aAng[PITCH] += mouse.y;
	camera.aTransform.aAng[YAW] += mouse.x;
	transform.aAng[YAW] += mouse.x;

	ClampAngles( transform, camera );
}


void PlayerManager::UpdateView( Entity player )
{
	auto& move = GetPlayerMoveData( player );
	auto& transform = GetTransform( player );
	auto& camera = GetCamera( player );

	ClampAngles( transform, camera );

	GetDirectionVectors( transform.ToViewMatrixZ(  ), move.aForward, move.aRight, move.aUp );

	/* Copy the player transformation, and apply the view offsets to it. */
	Transform transformView = transform;
	transformView.aPos += camera.aTransform.aPos * velocity_scale.GetFloat();
	transformView.aAng = camera.aTransform.aAng;
	//Transform transformView = transform;
	//transformView.aPos += move.aViewOffset * velocity_scale.GetFloat();
	//transformView.aAng += move.aViewAngOffset;

	if ( cl_thirdperson.GetBool() )
	{
		Transform thirdPerson = {};
		thirdPerson.aPos = {cl_cam_x, cl_cam_y, cl_cam_z};

		glm::mat4 viewMat = thirdPerson.ToMatrix( false ) * transformView.ToViewMatrixZ(  );

		game->SetViewMatrix( viewMat );
		GetDirectionVectors( viewMat, camera.aForward, camera.aRight, camera.aUp );
	}
	else
	{
		glm::mat4 viewMat = transformView.ToViewMatrixZ(  );

		game->SetViewMatrix( viewMat );
		GetDirectionVectors( viewMat, camera.aForward, camera.aRight, camera.aUp );
	}
}


// ============================================================


void PlayerMovement::OnPlayerSpawn( Entity player )
{
	auto& move = entities->GetComponent< CPlayerMoveData >( player );

	SetMoveType( move, PlayerMoveType::Walk );

	auto& camera = entities->GetComponent< CCamera >( player );
	camera.aTransform.aPos = {0, 0, cl_view_height};
}


void PlayerMovement::OnPlayerRespawn( Entity player )
{
	auto& move = entities->GetComponent< CPlayerMoveData >( player );

#if !NO_BULLET_PHYSICS
	auto& transform = entities->GetComponent< Transform >( player );
	//auto& physObj = entities->GetComponent< PhysicsObject* >( player );
	apPhysObj->SetWorldTransform( transform );
#endif

	// Init Smooth Duck
	move.aPrevViewHeight = GetViewHeight();
	move.aTargetViewHeight = GetViewHeight();
	move.aDuckLerpGoal = GetViewHeight();
	move.aDuckLerp = GetViewHeight();
	move.aPrevDuckLerp = GetViewHeight();
}


void PlayerMovement::MovePlayer( Entity player )
{
	aPlayer = player;
	apMove = &entities->GetComponent< CPlayerMoveData >( player );
	apRigidBody = &entities->GetComponent< CRigidBody >( player );
	apTransform = &entities->GetComponent< Transform >( player );
	apCamera = &entities->GetComponent< CCamera >( player );
	//apPhysObj = entities->GetComponent< PhysicsObject* >( player );

#if !NO_BULLET_PHYSICS
	// update velocity
	apRigidBody->aVel = apPhysObj->GetLinearVelocity();

	//apPhysObj->SetSleepingThresholds( 0, 0 );
	//apPhysObj->SetAngularFactor( 0 );
	apPhysObj->SetFriction( phys_friction.GetFloat() );
#endif

	UpdateInputs();

	// should be in WalkMove only, but i need this here when toggling noclip mid-duck
	DoSmoothDuck();

	DetermineMoveType();

	switch ( apMove->aMoveType )
	{
		case PlayerMoveType::Walk:    WalkMove();     break;
		case PlayerMoveType::Fly:     FlyMove();      break;
		case PlayerMoveType::NoClip:  NoClipMove();   break;
	}

	// CHANGE THIS IN THE FUTURE FOR NETWORKING
	if ( cl_thirdperson.GetBool() )
	{
		auto& model = entities->GetComponent< Model >( player );

		for ( auto& mesh: model.GetModelData().aMeshes )
		{
#if BULLET_PHYSICS
			//auto& physObj = entities->GetComponent< PhysicsObject* >( player );
			//mesh->aTransform = physObj->GetWorldTransform();
#else
			mesh->aTransform = *apTransform;
#endif
			mesh->aTransform.aAng[ROLL] += 90;
			mesh->aTransform.aAng[YAW] *= -1;
			mesh->aTransform.aAng[YAW] += 180;
		}
	}
}


float PlayerMovement::GetViewHeight(  )
{
	if ( KEY_PRESSED(SDL_SCANCODE_LCTRL) || in_duck )
		return cl_view_height_duck;

	return cl_view_height;
}


void PlayerMovement::DetermineMoveType(  )
{
	if ( KEY_JUST_PRESSED(SDL_SCANCODE_V) )
		SetMoveType( *apMove, apMove->aMoveType == PlayerMoveType::NoClip ? PlayerMoveType::Walk : PlayerMoveType::NoClip );

	if ( KEY_JUST_PRESSED(SDL_SCANCODE_B) )
		SetMoveType( *apMove, apMove->aMoveType == PlayerMoveType::Fly ? PlayerMoveType::Walk : PlayerMoveType::Fly );
}


void PlayerMovement::SetMoveType( CPlayerMoveData& move, PlayerMoveType type )
{
	move.aMoveType = type;

	switch (type)
	{
		case PlayerMoveType::NoClip:
		{
			EnableGravity( false );
			SetCollisionEnabled( false );
			break;
		}

		case PlayerMoveType::Fly:
		{
			EnableGravity( false );
			SetCollisionEnabled( true );
			break;
		}

		case PlayerMoveType::Walk:
		{
			EnableGravity( true );
			SetCollisionEnabled( true );
			break;
		}
	}
}


void PlayerMovement::SetCollisionEnabled( bool enable )
{
#if !NO_BULLET_PHYSICS
	apPhysObj->SetCollisionEnabled( enable );
#endif
}


void PlayerMovement::EnableGravity( bool enabled )
{
#if !NO_BULLET_PHYSICS
	apPhysObj->SetGravity( enabled ? game->apPhysEnv->GetGravity() : glm::vec3(0, 0, 0) );
#endif
}

// ============================================================


void PlayerMovement::DisplayPlayerStats( Entity player ) const
{
	auto& move = entities->GetComponent< CPlayerMoveData >( player );
	auto& rigidBody = entities->GetComponent< CRigidBody >( player );
	auto& transform = entities->GetComponent< Transform >( player );
	auto& camTransform = entities->GetComponent< CCamera >( player ).aTransform;

	glm::vec3 scaledVelocity = rigidBody.aVel * velocity_scale.GetFloat();
	float scaledSpeed = glm::length( glm::vec2(scaledVelocity.x, scaledVelocity.y) );
	float speed = glm::length( glm::vec2(rigidBody.aVel.x, rigidBody.aVel.y) );

	game->apGui->DebugMessage( 0, "Player Pos:    %s", Vec2Str(transform.aPos).c_str() );
	game->apGui->DebugMessage( 1, "Player Ang:    %s", Vec2Str(transform.aAng).c_str() );
	game->apGui->DebugMessage( 2, "Player Vel:    %s", Vec2Str(scaledVelocity).c_str() );
	game->apGui->DebugMessage( 3, "Player Speed:  %.4f (%.4f Unscaled)", scaledSpeed, speed );

	//game->apGui->DebugMessage( 5, "View Offset:   %.6f (%.6f Unscaled)", move.aViewOffset.z * velocity_scale, move.aViewOffset.z );
	//game->apGui->DebugMessage( 6, "Ang Offset:    %s", Vec2Str(move.aViewAngOffset).c_str() );

	game->apGui->DebugMessage( 5, "View Pos:      %s", Vec2Str(camTransform.aPos).c_str() );
	game->apGui->DebugMessage( 6, "View Ang:      %s", Vec2Str(camTransform.aAng).c_str() );
}


void PlayerMovement::SetPos( const glm::vec3& origin )
{
	apTransform->aPos = origin;
}

const glm::vec3& PlayerMovement::GetPos(  ) const
{
	return apTransform->aPos;
}

void PlayerMovement::SetAng( const glm::vec3& angles )
{
	apTransform->aAng = angles;
}

const glm::vec3& PlayerMovement::GetAng(  ) const
{
	return apTransform->aAng;
}


void PlayerMovement::UpdateInputs(  )
{
	apRigidBody->aAccel = {0, 0, 0};

	float moveScale = 1.0f;

	apMove->aPrevPlayerFlags = apMove->aPlayerFlags;
	apMove->aPlayerFlags = PlyNone;

	if ( KEY_PRESSED(SDL_SCANCODE_LCTRL) || in_duck )
	{
		apMove->aPlayerFlags |= PlyInDuck;
		moveScale = sv_duck_mult;
	}

	else if ( KEY_PRESSED(SDL_SCANCODE_LSHIFT) || in_sprint )
	{
		apMove->aPlayerFlags |= PlyInSprint;
		moveScale = sv_sprint_mult;
	}

	const float forwardSpeed = forward_speed * moveScale;
	const float sideSpeed = side_speed * moveScale;
	apMove->aMaxSpeed = max_speed * moveScale;

	if ( KEY_PRESSED(SDL_SCANCODE_W) || in_forward.GetBool() ) apRigidBody->aAccel[W_FORWARD] = forwardSpeed;
	if ( KEY_PRESSED(SDL_SCANCODE_S) || in_forward == -1.f ) apRigidBody->aAccel[W_FORWARD] += -forwardSpeed;
	if ( KEY_PRESSED(SDL_SCANCODE_A) || in_side == -1.f ) apRigidBody->aAccel[W_RIGHT] = -sideSpeed;
	if ( KEY_PRESSED(SDL_SCANCODE_D) || in_side.GetBool() ) apRigidBody->aAccel[W_RIGHT] += sideSpeed;

	// HACK:
	// why is this so complicated???
	static bool wasJumpButtonPressed = false;
	bool jump = KEY_PRESSED(SDL_SCANCODE_SPACE) || in_jump;

	static bool jumped = false;

	if ( jump && IsOnGround() && !jumped )
	{
#if !NO_BULLET_PHYSICS
		apPhysObj->ApplyImpulse( {0, 0, jump_force} );
#endif
		apRigidBody->aVel[W_UP] += jump_force;
		jumped = true;
	}
	else
	{
		jumped = false;
	}

	wasJumpButtonPressed = jumped;
}


void PlayerMovement::UpdatePosition( Entity player )
{
#if BULLET_PHYSICS
	auto& transform = entities->GetComponent< Transform >( player );
	//auto& physObj = entities->GetComponent< PhysicsObject* >( player );

	//if ( aMoveType == MoveType::Fly )
		//aTransform = apPhysObj->GetWorldTransform();
	//transform.aPos = physObj->GetWorldTransform().aPos;
	transform.aPos = apPhysObj->GetWorldTransform().aPos;
	//transform.aAng = physObj->GetWorldTransform().aAng;  // hmm
#else
	SetPos( GetPos() + (apRigidBody->aVel * velocity_scale.GetFloat()) * game->aFrameTime );

	// blech
	//if ( IsOnGround() && aMoveType != MoveType::NoClip )
	//	aTransform.position.y = ground_pos.GetFloat() * velocity_scale.GetFloat();
#endif
}


void PlayerMovement::DoSmoothDuck(  )
{
	float viewHeightLerp = cl_view_height_lerp * game->aFrameTime;

	// NOTE: this doesn't work properly when jumping mid duck and landing
	// try and get this to round up a little faster while lerping to the target pos at the same speed?
	if ( IsOnGround() && apMove->aMoveType == PlayerMoveType::Walk )
	{
		if ( apMove->aTargetViewHeight != GetViewHeight() )
		{
			apMove->aPrevDuckLerp = apMove->aDuckLerp;
			apMove->aDuckLerp = GetViewHeight();
			apMove->aDuckLerpGoal = GetViewHeight();
		}

		// duckLerp = glm::lerp( prevDuckLerp, duckLerpGoal, viewHeightLerp );
		apMove->aDuckLerp = std::lerp( apMove->aPrevDuckLerp, apMove->aDuckLerpGoal, viewHeightLerp );
		apMove->aPrevDuckLerp = apMove->aDuckLerp;

		//duckLerp.y = Round( duckLerp.y );
		apMove->aDuckLerp = Round( apMove->aDuckLerp );
		// floating point inprecision smh my head
		//aViewOffset = glm::lerp( prevViewHeight, duckLerp, viewHeightLerp );
		apCamera->aTransform.aPos[W_UP] = std::lerp( apMove->aPrevViewHeight, apMove->aDuckLerp, viewHeightLerp );

		apMove->aTargetViewHeight = GetViewHeight();
	}
	else
	{
		//aViewOffset = glm::lerp( prevViewHeight, {0, targetViewHeight, 0}, viewHeightLerp );
		apCamera->aTransform.aPos[W_UP] = std::lerp( apMove->aPrevViewHeight, apMove->aTargetViewHeight, viewHeightLerp );
	}

	apMove->aPrevViewHeight = apCamera->aTransform.aPos[W_UP];
}


// temporarily using bullet directly until i abstract this
void PlayerMovement::DoRayCollision(  )
{
#if 0 // !NO_BULLET_PHYSICS
	// temp to avoid a crash intantly?
	if ( aVelocity.x == 0.f && aVelocity.y == 0.f && aVelocity.y == 0.f )
		return;

	btVector3 btFrom = toBt( GetPos() );
	btVector3 btTo = toBt( GetPos() * GetFrameTimeVelocity() );
	btCollisionWorld::ClosestRayResultCallback res( btFrom, btTo );

	game->apPhysEnv->apWorld->rayTest( btFrom, btTo, res );

	if ( res.hasHit() )
	{
		btScalar dist = res.m_hitPointWorld.distance2( btFrom );
		if ( dist < 300.f )  // 500.f, 3000.f
		{
			return;
		}
	}
	else
	{
		//UpdatePosition(  );
	}

	// SetPos( GetPos() + (aVelocity * velocity_scale.GetFloat()) * game->aFrameTime );
	// SetPos( fromBt( res.m_hitPointWorld ) );

#endif
}


CONVAR( phys_ground_dist, 1200 );  // 200 // 1050

#if BULLET_PHYSICS
PhysicsObject* GetGroundObject( Entity player )
{
	btDispatcher *pDispatcher = physenv->apWorld->getDispatcher();

	//auto& physObj = entities->GetComponent< PhysicsObject* >( player );

	// Loop through the collision pair manifolds
	int numManifolds = pDispatcher->getNumManifolds();
	for (int i = 0; i < numManifolds; i++)
	{
		btPersistentManifold *pManifold = pDispatcher->getManifoldByIndexInternal(i);
		if (pManifold->getNumContacts() <= 0)
			continue;

		const btCollisionObject *objA = pManifold->getBody0();
		const btCollisionObject *objB = pManifold->getBody1();

		const btCollisionShape *shapeA = objA->getCollisionShape();
		const btCollisionShape *shapeB = objA->getCollisionShape();

		// Skip if one object is static/kinematic
		if (objA->isStaticOrKinematicObject() || objB->isStaticOrKinematicObject())
			continue;

		PhysicsObject *pPhysA = (PhysicsObject *)objA->getUserPointer();
		PhysicsObject *pPhysB = (PhysicsObject *)objB->getUserPointer();

		// Collision that involves us!
		if ( shapeA == apPhysObj->apCollisionShape || shapeB == apPhysObj->apCollisionShape )
		{
			int ourID = apPhysObj->apCollisionShape == shapeA ? 0 : 1;

			for (int i = 0; i < pManifold->getNumContacts(); i++)
			{
				btManifoldPoint &point = pManifold->getContactPoint(i);

				btVector3 norm = point.m_normalWorldOnB; // Normal worldspace A->B
				if (ourID == 1)
				{
					// Flip it because we're object B and we need norm B->A.
					norm *= -1;
				}

				// HACK: Guessing which way is up (as currently defined in our implementation y is up)
				// If the normal is up enough then assume it's some sort of ground
				if (norm.z() > 0.8)
				{
					return ourID == 0 ? pPhysB : pPhysA;
				}
			}
		}
	}

	return nullptr;
}
#endif


bool PlayerMovement::IsOnGround(  )
{
#if !NO_BULLET_PHYSICS
	btVector3 btFrom = toBt( GetPos() );
	//btVector3 btTo( GetPos().x, -10000.0, GetPos().y );
	btVector3 btTo( GetPos().x, GetPos().y, -10000.0 );
	btCollisionWorld::ClosestRayResultCallback res( btFrom, btTo );

	game->apPhysEnv->apWorld->rayTest( btFrom, btTo, res );

	if ( res.hasHit() )
	{
		btScalar dist = res.m_hitPointWorld.distance2( btFrom );
		if ( dist < phys_ground_dist )
			apMove->aPlayerFlags |= PlyOnGround;
		else
			apMove->aPlayerFlags &= ~PlyOnGround;
		
	}

	//return false;
#else
	// aOnGround = GetPos().y <= ground_pos.GetFloat() * velocity_scale.GetFloat();
	bool onGround = GetPos()[W_UP] <= ground_pos * velocity_scale;

	if ( onGround )
		apMove->aPlayerFlags |= PlyOnGround;
	else
		apMove->aPlayerFlags &= ~PlyOnGround;

#endif

	return apMove->aPlayerFlags & PlyOnGround;

	/*
	// maybe useful code
	// https://gamedev.stackexchange.com/questions/58012/detect-when-a-bullet-rigidbody-is-on-ground
	
	// Go through collisions
	int numManifolds = btDispatcher->getNumManifolds();
    for (int i = 0; i < numManifolds; i++)
    {
        btPersistentManifold* contactManifold = btWorld->getDispatcher()->getManifoldByIndexInternal(i);
        btCollisionObject* obA = const_cast<btCollisionObject*>(contactManifold->getBody0());
        btCollisionObject* obB = const_cast<btCollisionObject*>(contactManifold->getBody1());

        GameObject* gameObjA = static_cast<GameObject*>(obA->getUserPointer());
        GameObject* gameObjB = static_cast<GameObject*>(obB->getUserPointer());
		if (gameObjA->name == "Camera" || gameObjB->name == "Camera" have some way to check if one of the objects is the rigidbody you want ) {
		int numContacts = contactManifold->getNumContacts();
		for (int j = 0; j < numContacts; j++)
		{
			btManifoldPoint& pt = contactManifold->getContactPoint(j);
			if (pt.getDistance() < 0.f)
			{
				glm::vec3 normal;

				if (gameObjB->name == "Camera") //Check each object to see if it's the rigid body and determine the correct normal.
					normal = -pt.m_normalWorldOnB;
				else
					normal = pt.m_normalWorldOnB;

				// put the threshold here where 0.4f is
				if (normal.y > 0.4f ) {
					// The character controller is on the ground
				}
			}
		}
	}
	*/
}


bool PlayerMovement::WasOnGround()
{
	return apMove->aPrevPlayerFlags & PlyOnGround;
}

float PlayerMovement::GetMoveSpeed( glm::vec3 &wishdir, glm::vec3 &wishvel )
{
	wishdir = wishvel;
	float wishspeed = vec3_norm( wishdir );

	if ( wishspeed > apMove->aMaxSpeed )
	{
		wishvel = wishvel * apMove->aMaxSpeed/wishspeed;
		wishspeed = apMove->aMaxSpeed;
	}

	return wishspeed;
}

float PlayerMovement::GetMaxSpeed(  )
{
	return apMove->aMaxSpeed;
}

float PlayerMovement::GetMaxSpeedBase(  )
{
	return max_speed;
}

float PlayerMovement::GetMaxSprintSpeed(  )
{
	return GetMaxSpeedBase(  ) * sv_sprint_mult;
}

float PlayerMovement::GetMaxDuckSpeed(  )
{
	return GetMaxSpeedBase(  ) * sv_duck_mult;
}


CONVAR( cl_step_sound_speed_vol, 0.001 );
CONVAR( cl_step_sound_speed_offset, 1 );
CONVAR( cl_step_sound_gravity_scale, 4 );
CONVAR( cl_step_sound_min_speed, 0.075 );
CONVAR( cl_step_sound, 1 );


void PlayerMovement::StopStepSound( bool force )
{
	if ( apMove->apStepSound && apMove->apStepSound->Valid() )
	{
		if ( game->aCurTime - apMove->aLastStepTime > cl_stepduration || force )
		{
			game->apAudio->FreeSound( &apMove->apStepSound );
			game->apGui->DebugMessage( 7, "Freed Step Sound" );
		}
	}
}


void PlayerMovement::PlayStepSound(  )
{
	if ( !cl_step_sound.GetBool() )
		return;

	//float vel = glm::length( glm::vec2(aVelocity.x, aVelocity.y) ); 
	float vel = glm::length( glm::vec3(apRigidBody->aVel.x, apRigidBody->aVel.y, apRigidBody->aVel.z * cl_step_sound_gravity_scale) ); 
	float speedFactor = glm::min( glm::log( vel * cl_step_sound_speed_vol + cl_step_sound_speed_offset ), 1.f );
	
	if ( speedFactor < cl_step_sound_min_speed )
		return;

	StopStepSound( true );

	char soundName[128];
	int soundIndex = ( rand(  ) / ( RAND_MAX / 40.0f ) ) + 1;
	snprintf(soundName, 128, "sound/footsteps/running_dirt_%s%d.ogg", soundIndex < 10 ? "0" : "", soundIndex);

	if ( apMove->apStepSound = game->apAudio->LoadSound(soundName) )
	{
		apMove->apStepSound->vol = speedFactor;

		game->apAudio->PlaySound( apMove->apStepSound );

		apMove->aLastStepTime = game->aCurTime;
	}
}


void PlayerMovement::BaseFlyMove(  )
{
	glm::vec3 wishvel(0,0,0);
	glm::vec3 wishdir(0,0,0);

	// forward and side movement
	for ( int i = 0; i < 3; i++ )
		wishvel[i] = apCamera->aForward[i]*apRigidBody->aAccel.x + apCamera->aRight[i]*apRigidBody->aAccel[W_RIGHT];

	float wishspeed = GetMoveSpeed( wishdir, wishvel );

	AddFriction(  );
	Accelerate( wishspeed, wishdir );
}


#define SET_VELOCITY() \
glm::vec3 __vel{}; \
for ( int i = 0; i < 3; i++ ) \
	__vel[i] = apCamera->aForward[i]*apRigidBody->aAccel.x + apCamera->aRight[i]*apRigidBody->aAccel[W_RIGHT]; \
apPhysObj->SetLinearVelocity( apPhysObj->GetLinearVelocity() + (__vel * velocity_scale2.GetFloat()) )


void PlayerMovement::NoClipMove(  )
{
	BaseFlyMove(  );

#if !NO_BULLET_PHYSICS
	SET_VELOCITY();
	// apPhysObj->SetLinearVelocity( apRigidBody->aVel );
#else
	UpdatePosition( aPlayer );
#endif
}


void PlayerMovement::FlyMove(  )
{
	BaseFlyMove(  );

#if !NO_BULLET_PHYSICS
	SET_VELOCITY();
	// apPhysObj->SetLinearVelocity( apRigidBody->aVel );
#else
	UpdatePosition( aPlayer );
#endif

	DoRayCollision(  );
}


void PlayerMovement::WalkMove(  )
{
	glm::vec3 wishvel = apMove->aForward*apRigidBody->aAccel.x + apMove->aRight*apRigidBody->aAccel[W_RIGHT];
	wishvel[W_UP] = 0.f;

	glm::vec3 wishdir(0,0,0);
	float wishspeed = GetMoveSpeed( wishdir, wishvel );

	bool onGround = IsOnGround();

	if ( onGround )
	{
		AddFriction(  );
		Accelerate( wishspeed, wishdir, false );
	}
	else
	{	// not on ground, so little effect on velocity
		Accelerate( wishspeed, wishvel, true );
		AddGravity(  );
	}

#if BULLET_PHYSICS
	SET_VELOCITY();
	// apPhysObj->SetLinearVelocity( apRigidBody->aVel );

	// uhhh
	apRigidBody->aVel = apPhysObj->GetLinearVelocity();

#else
	UpdatePosition( aPlayer );
#endif

	DoRayCollision(  );
	
	StopStepSound(  );

	if ( IsOnGround() && !onGround )
		PlayStepSound();

	// something is wrong with this here on bullet
#if !BULLET_PHYSICS
	DoSmoothLand( onGround );
	DoViewBob(  );
	DoViewTilt(  );
#endif

	if ( IsOnGround() )
		apRigidBody->aVel[W_UP] = 0;
}


void PlayerMovement::DoSmoothLand( bool wasOnGround )
{
	static glm::vec3 prevViewHeight = {};
	static glm::vec3 fallViewOffset = {};

	static float duckLerp = apRigidBody->aVel[W_UP] * velocity_scale;
	static float prevDuckLerp = apRigidBody->aVel[W_UP] * velocity_scale;

	float landLerp = cl_smooth_land_lerp * game->aFrameTime;
	float landScale = cl_smooth_land_scale * velocity_scale * game->aFrameTime;
	float landUpScale = cl_smooth_land_up_scale * velocity_scale;

	float viewHeightScale = glm::log( apMove->aPrevViewHeight * cl_smooth_land_view_scale + cl_smooth_land_view_offset );

	//game->apGui->DebugMessage(16, "smooth land view thing: %.4f", bruh );

	landScale *= viewHeightScale;

	if ( cl_smooth_land )
	{
		// NOTE: this doesn't work properly when jumping mid duck and landing
		// meh, works well enough with the current values for now
		if ( IsOnGround() && !wasOnGround )
		{
			duckLerp = apRigidBody->aVel[W_UP] * velocity_scale;
			prevDuckLerp = duckLerp;
		}

		// duckLerp = glm::lerp( prevDuckLerp, {0, 0, 0}, GetLandingLerp() );
		// duckLerp = glm::lerp( prevDuckLerp, -prevViewHeight, GetLandingLerp() );
		// acts like a trampoline, hmm
		duckLerp = std::lerp( prevDuckLerp, (-prevViewHeight[W_UP]) * landUpScale, landLerp );

		fallViewOffset[W_UP] += duckLerp * landScale;
		prevDuckLerp = duckLerp;
	}
	else
	{
		// lerp it back to 0 just in case
		fallViewOffset = glm::lerp( fallViewOffset, {0, 0, 0}, landLerp );
	}

	prevViewHeight = fallViewOffset;
	apCamera->aTransform.aPos += fallViewOffset;
}


CONVAR( cl_bob_enabled, 1 );
CONVAR( cl_bob_magnitude, 2 );
CONVAR( cl_bob_freq, 4.5 );
CONVAR( cl_bob_speed_scale, 0.013 );
CONVAR( cl_bob_exit_lerp, 0.1 );
CONVAR( cl_bob_exit_threshold, 0.1 );
CONVAR( cl_bob_sound_threshold, 0.1 );


// TODO: doesn't smoothly transition out of viewbob still
// TODO: maybe make a minimum speed threshold and start lerping to 0
//  or some check if your movements changed compared to the previous frame
//  so if you start moving again faster than min speed, or movements change,
//  you restart the view bob, or take a new step right there
void PlayerMovement::DoViewBob(  )
{
	if ( !cl_bob_enabled )
		return;

	//static glm::vec3 prevMove = aMove;
	//static bool inExit = false;

	if ( !IsOnGround() /*|| aMove != prevMove || inExit*/ )
	{
		// lerp back to 0 to not snap view the offset (not good enough) and reset input
		apMove->aWalkTime = 0.f;
		apMove->aBobOffsetAmount = glm::mix( apMove->aBobOffsetAmount, 0.f, cl_bob_exit_lerp.GetFloat() );
		apCamera->aTransform.aPos[W_UP] += apMove->aBobOffsetAmount;
		//inExit = aBobOffsetAmount > 0.01;
		//prevMove = aMove;
		return;
	}

	static bool playedStepSound = false;

	float vel = glm::length( glm::vec2(apRigidBody->aVel.x, apRigidBody->aVel.y) ); 

	float speedFactor = glm::log( vel * cl_bob_speed_scale + 1 );

	// scale by speed
	apMove->aWalkTime += game->aFrameTime * speedFactor * cl_bob_freq;

	// never reaches 0 so do this to get it to 0
	static float sinMessFix = sin(cos(tan(cos(0))));
	
	// using this math function mess instead of abs(sin(x)) and lerping it, since it gives a similar result
	// mmmmmmm cpu cycles go brrrrrr
	apMove->aBobOffsetAmount = cl_bob_magnitude * speedFactor * (sin(cos(tan(cos(apMove->aWalkTime)))) - sinMessFix);

	// apMove->aBobOffsetAmount = cl_bob_magnitude * speedFactor * ( 0.6*(1-cos( 2*sin(apMove->aWalkTime) )) );

	// reset input
	if ( apMove->aBobOffsetAmount == 0.f )
		apMove->aWalkTime = 0.f;

	if ( apMove->aBobOffsetAmount <= cl_bob_sound_threshold )
	{
		if ( !playedStepSound )
		{
			PlayStepSound();
			playedStepSound = true;
		}
	}
	else
	{
		playedStepSound = false;
	}

	apCamera->aTransform.aPos[W_UP] += apMove->aBobOffsetAmount;

	game->apGui->DebugMessage( 8,  "Walk Time * Speed:  %.8f", apMove->aWalkTime );
	game->apGui->DebugMessage( 9,  "View Bob Offset:    %.4f", apMove->aBobOffsetAmount );
	game->apGui->DebugMessage( 10, "View Bob Speed:     %.6f", speedFactor );
}


CONVAR( cl_tilt, 0.8 );
CONVAR( cl_tilt_speed, 0.1 );
CONVAR( cl_tilt_threshold, 200 );

CONVAR( cl_tilt_type, 2 );
CONVAR( cl_tilt_lerp, 5 );
CONVAR( cl_tilt_lerp_new, 10 );
CONVAR( cl_tilt_speed_scale, 0.043 );
CONVAR( cl_tilt_scale, 0.2 );
CONVAR( cl_tilt_threshold_new, 12 );


void PlayerMovement::DoViewTilt(  )
{
	if ( cl_tilt_type == 0.f )
	{
		float side = glm::dot( apRigidBody->aVel, apMove->aRight );
		float sign = side < 0 ? -1 : 1;

		float speedFactor = glm::clamp( glm::max(0.f, side * sign - cl_tilt_threshold) / GetMaxSprintSpeed(), 0.f, 1.f );

		side = fabs(side);

		if (side < cl_tilt_speed.GetFloat())
			side = side * cl_tilt / cl_tilt_speed;
		else
			side = cl_tilt;

		/* Lerp the tilt angle by how fast your going. */
		float output = glm::mix( 0.f, side * sign, speedFactor );

		apCamera->aTransform.aAng[ROLL] = output;
		return;
	}

	// not too sure about this one, so im keeping the old one just in case

	static float prevTilt = 0.f;

	float output = glm::dot( apRigidBody->aVel, apMove->aRight );
	float side = output < 0 ? -1 : 1;

	if ( cl_tilt_type == 2.f )
	{
		float speedFactor = glm::max(0.f, glm::log( glm::max(0.f, (fabs(output) * cl_tilt_speed_scale + 1) - cl_tilt_threshold_new) ));

		/* Now Lerp the tilt angle with the previous angle to make a smoother transition. */
		output = glm::mix( prevTilt, speedFactor * side * cl_tilt_scale, cl_tilt_lerp_new * game->aFrameTime );
	}
	else // type 1.f
	{
		output = glm::clamp( glm::max(0.f, fabs(output) - cl_tilt_threshold) / GetMaxSprintSpeed(), 0.f, 1.f ) * side;

		/* Now Lerp the tilt angle with the previous angle to make a smoother transition. */
		output = glm::mix( prevTilt, output * cl_tilt, cl_tilt_lerp * game->aFrameTime );
	}

	apCamera->aTransform.aAng[ROLL] = output;

	prevTilt = output;
}


CONVAR( sv_friction_idk, 16 );
/*CONVAR( sv_friction_scale, 0.005 );
CONVAR( sv_friction_scale2, 10 );
CONVAR( sv_friction_offset, 6 );
CONVAR( sv_friction_lerp, 5 );

CONVAR( sv_friction_stop_lerp, 2 );
CONVAR( sv_friction_scale3, 0.01 );
CONVAR( sv_friction_scaleeee, 1 );

CONVAR( sv_friction_power, 2 );*/


// TODO: make your own version of this that uses this to find the friction value:
// log(-playerSpeed) + 1
void PlayerMovement::AddFriction(  )
{
	glm::vec3	start(0, 0, 0), stop(0, 0, 0);
	float	friction;
	//trace_t	trace;

	glm::vec3 vel = apRigidBody->aVel;

	float speed = sqrt(vel[0]*vel[0] + vel[W_RIGHT]*vel[W_RIGHT] + vel[W_UP]*vel[W_UP]);
	if (!speed)
		return;

	float idk = sv_friction_idk;

	// if the leading edge is over a dropoff, increase friction
#if 0

	auto falloff = [&]( float vel ) -> float
	{
		float in = (vel/speed*idk) * sv_friction_scale + sv_friction_offset;
		return std::lerp( in, 0, sv_friction_lerp * game->aFrameTime );
	};

	start.x = falloff( vel.x );
	start.y = falloff( vel.y );
	start.z = falloff( vel.z );

	/*start.x = glm::log( vel.x + sv_friction_offset );
	start.y = glm::log( vel.y + sv_friction_offset );
	start.z = glm::log( vel.z + sv_friction_offset );*/

	//start *= sv_friction_scale.GetFloat();

	// apRigidBody->aVel = vel * start * game->aFrameTime;
	apRigidBody->aVel = start * game->aFrameTime;

#elif 0
	/*start.x = glm::log( vel.x ) + sv_friction_offset.GetFloat();
	start.y = glm::log( vel.y ) + sv_friction_offset.GetFloat();
	start.z = glm::log( vel.z ) + sv_friction_offset.GetFloat();*/

	auto falloff = [&]( float vel ) -> float
	{
		if ( vel == 0.f )
			return 0.f;

		float dir = vel > 0.f ? 1.f : -1.f;

		// float in = (vel/speed*idk);
		float in = vel;
		return glm::pow( sv_friction_power.GetFloat(), in * sv_friction_scale + sv_friction_offset ) * dir;
		//return glm::log( fabs(in * sv_friction_scale + sv_friction_offset) ) * dir;
	};


	start.x = falloff( vel.x );
	start.y = falloff( vel.y );
	start.z = falloff( vel.z );

	/*start.x = glm::log( vel.x + sv_friction_offset );
	start.y = glm::log( vel.y + sv_friction_offset );
	start.z = glm::log( vel.z + sv_friction_offset );*/

	start *= game->aFrameTime;

	//apRigidBody->aVel = start * game->aFrameTime;
	apRigidBody->aVel *= start * sv_friction_scale2.GetFloat();

#elif 0
	start.x = GetPos().x + vel.x / speed*idk;
	start[W_RIGHT] = GetPos()[W_RIGHT] + vel[W_RIGHT] / speed*idk;
	start[W_UP] = GetPos()[W_UP] + vel[W_UP] / speed*idk;

	friction = sv_friction_scale3;

	// apply friction
	// float control = speed < stop_speed ? stop_speed : speed;
	float control = std::lerp( speed, 0, sv_friction_stop_lerp * game->aFrameTime );

	// float newspeed = glm::max( 0.f, speed - game->aFrameTime * control * sv_friction );
	float newspeed = glm::min( 1.f, glm::max( 0.f, control * friction ) );

	//newspeed /= speed;
	apRigidBody->aVel = vel * newspeed;
#else
	start.x = stop.x = GetPos().x + vel.x / speed*idk;
	start[W_RIGHT] = stop[W_RIGHT] = GetPos()[W_RIGHT] + vel[W_RIGHT] / speed*idk;
	start[W_UP] = stop[W_UP] = GetPos()[W_UP] + vel[W_UP] / speed*idk;
	// start.z = GetPos().z + sv_player->v.mins.z;

	//trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, sv_player);

	//if (trace.fraction == 1.0)
	//	friction = sv_friction.GetFloat() * sv_edgefriction.value;
	//else
		friction = sv_friction;

	// apply friction
	float control = speed < stop_speed ? stop_speed : speed;
	float newspeed = glm::max( 0.f, speed - game->aFrameTime * control * friction );

	newspeed /= speed;
	apRigidBody->aVel = vel * newspeed;
#endif
}


void PlayerMovement::Accelerate( float wishSpeed, glm::vec3 wishDir, bool inAir )
{
	//float baseWishSpeed = inAir ? glm::min( 30.f, vec3_norm( wishDir ) ) : wishSpeed;
	float baseWishSpeed = inAir ? glm::min( accel_speed_air.GetFloat(), vec3_norm( wishDir ) ) : wishSpeed;

	float currentspeed = glm::dot( apRigidBody->aVel, wishDir );
	float addspeed = baseWishSpeed - currentspeed;

	if ( addspeed <= 0.f )
		return;

	addspeed = glm::min( addspeed, accel_speed * game->aFrameTime * wishSpeed );

	for ( int i = 0; i < 3; i++ )
		apRigidBody->aVel[i] += addspeed * wishDir[i];
}


void PlayerMovement::AddGravity(  )
{
#if !NO_BULLET_PHYSICS
	//apRigidBody->aVel[W_UP] = apPhysObj->GetLinearVelocity()[W_UP];
#else
	apRigidBody->aVel[W_UP] -= sv_gravity * game->aFrameTime;
#endif
}

