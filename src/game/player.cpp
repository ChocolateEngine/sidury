#include "player.h"
#include "../../chocolate/inc/shared/util.h"

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

extern ConVarRef e_timescale;

ConVar forward_speed( "sv_forward_speed", 300 );
ConVar side_speed( "sv_side_speed", 300 );  // 350.f

CONVAR( sv_sprint_mult, 2.0 );
CONVAR( sv_duck_mult, 0.5 );

ConVar max_speed( "sv_max_speed", 300 );  // 320.f
ConVar stop_speed( "sv_stop_speed", 100 );
ConVar accel_speed( "sv_accel_speed", 10 );
ConVar sv_friction( "sv_friction", 8 );  // 4.f
ConVar jump_force( "sv_jump_force", 250 );

// lerp the friction maybe?
//CONVAR( sv_new_movement, 1 );
//CONVAR( sv_friction_new, 8 );  // 4.f

CONVAR( sv_gravity, 800 );
CONVAR( ground_pos, 250 );

CONVAR( in_sensitivity, 0.1 );

CONVAR( cl_stepspeed, 200 );
CONVAR( cl_steptime, 0.25 );
CONVAR( cl_stepduration, 0.22 );

CONVAR( cl_view_height, 56 );
CONVAR( cl_view_height_duck, 24 );
CONVAR( cl_view_height_lerp, 0.015 );

CONVAR( cl_smooth_land, 1 );
CONVAR( cl_smooth_land_lerp, 0.015 );
CONVAR( cl_smooth_land_scale, 4000 );
CONVAR( cl_smooth_land_up_scale, 50 );

// temp
CONVAR( cl_lerp_scale, 1 );

// multiplies the final velocity by this amount when setting the player position,
// a workaround for quake movement values not working correctly when lowered
#if NO_BULLET_PHYSICS
CONVAR( velocity_scale, 0.025 );
#else
CONVAR( velocity_scale, 1.0 );
#endif

constexpr float PLAYER_MASS = 200.f;

// TEMP
// #define SPAWN_POS 1085.69824, 260, 644.222046
#define SPAWN_POS 1085.69824 * velocity_scale, 260 * velocity_scale, 644.222046 * velocity_scale
// #define SPAWN_POS 1085.69824 * velocity_scale, -46, 644.222046 * velocity_scale
// #define SPAWN_POS 10.8569824, -2.60, 6.44222046
// #define SPAWN_POS -26.5, -45, -8.2


CON_COMMAND( respawn )
{
	game->aLocalPlayer->Respawn();
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

#if VIEW_LERP_CLASS

ViewLerp::ViewLerp( Player* player )
{
	apPlayer = player;
}

ViewLerp::~ViewLerp()
{
}


glm::vec3 ViewLerp::LerpView()
{

}

#endif

// ============================================================


Player::Player():
	mX(0), mY(0),
	aFlags(0),
	aMaxSpeed(0),
	aMove(0, 0, 0),
	aVelocity(0, 0, 0),
	aMoveType(MoveType::Walk),
	aOrigin(0, 0, 0)
{
	aTransform = {};
	aDirection = {};

#if !NO_BULLET_PHYSICS
	apPhysObj = NULL;
#endif
}

Player::~Player()
{
}


void Player::Spawn()
{
	// should be below this code, but then the phys engine crashes
	Respawn();

	aTransform.aPos = {SPAWN_POS};

#if !NO_BULLET_PHYSICS
	PhysicsObjectInfo physInfo( ShapeType::Box );
	physInfo.mass = PLAYER_MASS;
	physInfo.transform = aTransform;
	//physInfo.callbacks = true;
	//physInfo.collisionType = CollisionType::Kinematic;
	physInfo.bounds = {16, 72, 16};

	apPhysObj = game->apPhysEnv->CreatePhysicsObject( physInfo );
	apPhysObj->SetAlwaysActive( true );
	apPhysObj->SetContinuousCollisionEnabled( true );
	apPhysObj->SetWorldTransform( aTransform );

	// enable Continuous Collision Detection
	//apPhysObj->aRigidBody.setCcdMotionThreshold( 1e-7 );
	//apPhysObj->aRigidBody.setCcdSweptSphereRadius( 0.5f );
#endif

	SetMoveType( MoveType::Walk );

	aViewOffset = {0, cl_view_height, 0};
}


void Player::Respawn()
{
	// HACK FOR RIVERHOUSE SPAWN POS
	aTransform.aPos = {SPAWN_POS};
	aTransform.rotation = {0, 0, 0, 0};
	aVelocity = {0, 0, 0};
	aOrigin = {0, 0, 0};
	aMove = {0, 0, 0};
	mX = 0.f;
	mY = 0.f;

#if !NO_BULLET_PHYSICS
	if ( apPhysObj )
		apPhysObj->SetWorldTransform( aTransform );
#endif
}


void Player::Update( float dt )
{
	UpdateInputs();

	// should be in WalkMove only, but i need this here when toggling noclip mid-duck
	DoSmoothDuck();

	DetermineMoveType();

	switch ( aMoveType )
	{
		case MoveType::Walk:    WalkMove();     break;
		case MoveType::Fly:     FlyMove();      break;
		case MoveType::NoClip:  NoClipMove();   break;
	}
}


void Player::DisplayPlayerStats(  )
{
	glm::vec3 scaledVelocity = aVelocity * velocity_scale.GetFloat();
	float scaledSpeed = glm::length( glm::vec2(scaledVelocity.x, scaledVelocity.z) );
	float speed = glm::length( glm::vec2(aVelocity.x, aVelocity.z) );

	game->apGui->DebugMessage( 0, "Player Pos:    %s", Vec2Str(aTransform.aPos).c_str() );
	// game->apGui->DebugMessage( 1, "Player Rot:    %s", Quat2Str(aTransform.rotation).c_str() );
	game->apGui->DebugMessage( 1, "Player Ang:    %s", Vec2Str(aTransform.aAng).c_str() );
	game->apGui->DebugMessage( 2, "Player Vel:    %s", Vec2Str(scaledVelocity).c_str() );

	game->apGui->DebugMessage( 4, "Player Speed:  %.4f (%.4f Unscaled)", scaledSpeed, speed );
	game->apGui->DebugMessage( 5, "View Offset:   %.6f (%.6f Unscaled)", aViewOffset.y * velocity_scale, aViewOffset.y );
	game->apGui->DebugMessage( 6, "Ang Offset:    %s", Vec2Str(aViewAngOffset).c_str() );
}


void Player::SetPos( const glm::vec3& origin )
{
	aTransform.aPos = origin;
}

const glm::vec3& Player::GetPos(  )
{
	return aTransform.aPos;
}

void Player::SetPosVel( const glm::vec3& origin )
{
	aTransform.aPos = GetPos() + (aVelocity * velocity_scale.GetFloat()) * game->aFrameTime;
}

glm::vec3 Player::GetFrameTimeVelocity(  )
{
	return aVelocity * game->aFrameTime;
}

/*void Player::SetAng( const glm::vec3& angle )
{
	aTransform.position = origin;
}

const glm::vec3& Player::GetAng(  )
{
	return aTransform.position;
}*/


void Player::UpdateView(  )
{
	float& pitch = aTransform.aAng[PITCH];
	float& yaw = aTransform.aAng[YAW];
	float& roll = aTransform.aAng[ROLL];

	pitch = -mY;
	yaw = mX;

	glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
	glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);

	// keeping the old way for now still
	glm::quat xRot = glm::angleAxis(glm::radians(mY), glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f)));
	glm::quat yRot = glm::angleAxis(glm::radians(mX), glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)));
	aTransform.rotation = xRot * yRot;

	aDirection.Update( forward*yRot, up*xRot, right*yRot );

	//game->apGui->DebugMessage( 8,  "Old Forward Direction:  %s", Vec2Str(aDirection.forward).c_str() );
	//game->apGui->DebugMessage( 9,  "Old Up Direction:       %s", Vec2Str(aDirection.up).c_str() );
	//game->apGui->DebugMessage( 10, "Old Right Direction:    %s", Vec2Str(aDirection.right).c_str() );

	/* Copy the player transformation, and apply the view offsets to it */
	Transform transform = aTransform;
	transform.aPos += aViewOffset * velocity_scale.GetFloat();
	transform.aAng += aViewAngOffset;

	glm::mat4 viewMatrix = transform.ToViewMatrix(  );

	game->SetViewMatrix( transform.ToViewMatrix(  ) );

	forward = -glm::normalize( glm::vec3(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2]) );
	up = glm::normalize( glm::vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]) );
	right = glm::normalize( glm::vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]) );

	// moves in the right direction, but the values look slightly different
	aDirection.Update( forward, up, right );

	// slightly offset from what it was before
	//game->apGui->DebugMessage( 12, "New Forward Direction:  %s", Vec2Str(forward).c_str() );
	//game->apGui->DebugMessage( 13, "New Up Direction:       %s", Vec2Str(up).c_str() );
	//game->apGui->DebugMessage( 14, "New Right Direction:    %s", Vec2Str(right).c_str() );
}


void Player::UpdateInputs(  )
{
	aMove = {0, 0, 0};

	float moveScale = 1.0f;

	aPrevPlayerFlags = aPlayerFlags;
	aPlayerFlags = PlyNone;

	if ( KEY_PRESSED(SDL_SCANCODE_LCTRL) || in_duck )
	{
		aPlayerFlags |= PlyInDuck;
		moveScale = sv_duck_mult;
	}

	else if ( KEY_PRESSED(SDL_SCANCODE_LSHIFT) || in_sprint )
	{
		aPlayerFlags |= PlyInSprint;
		moveScale = sv_sprint_mult;
	}

	const float forwardSpeed = forward_speed * moveScale;
	const float sideSpeed = side_speed * moveScale;
	aMaxSpeed = max_speed * moveScale;

	if ( KEY_PRESSED(SDL_SCANCODE_W) || in_forward.GetBool() ) aMove.x = forwardSpeed;
	if ( KEY_PRESSED(SDL_SCANCODE_S) || in_forward == -1.f ) aMove.x += -forwardSpeed;
	if ( KEY_PRESSED(SDL_SCANCODE_A) || in_side == -1.f ) aMove.z = -sideSpeed;
	if ( KEY_PRESSED(SDL_SCANCODE_D) || in_side.GetBool() ) aMove.z += sideSpeed;

	// HACK:
	// why is this so complicated???
	static bool wasJumpButtonPressed = false;
	bool jump = KEY_PRESSED(SDL_SCANCODE_SPACE) || in_jump;

	static bool jumped = false;

	if ( jump && IsOnGround() && !jumped )
	{
#if !NO_BULLET_PHYSICS
		apPhysObj->ApplyImpulse( {0, jump_force, 0} );
#endif
		aVelocity.y += jump_force;
		jumped = true;
	}
	else
	{
		jumped = false;
	}

	wasJumpButtonPressed = jumped;

	mX += in_sensitivity * game->apInput->GetMouseDelta().x;
	mY -= in_sensitivity * game->apInput->GetMouseDelta().y;

	auto constrain = [](float num) -> float
	{
		num = std::fmod(num, 360.0f);
		return (num < 0.0f) ? num += 360.0f : num;
	};

	mX = constrain(mX);
	mY = std::clamp(mY, -90.0f, 90.0f);
}


void Player::DoSmoothDuck(  )
{
	// TODO: make a view offset lerp handler class or some shit, idk
	// that way we don't duplicate code, and have it work for multiplayer

	static float targetViewHeight = GetViewHeight();
	static glm::vec3 prevViewHeight = {0, GetViewHeight(), 0};

	//static glm::vec3 duckLerpGoal = prevViewHeight;
	//static glm::vec3 duckLerp = prevViewHeight;
	//static glm::vec3 prevDuckLerp = prevViewHeight;

	static float duckLerpGoal = targetViewHeight;
	static float duckLerp = targetViewHeight;
	static float prevDuckLerp = targetViewHeight;

	float viewHeightLerp = cl_view_height_lerp * cl_lerp_scale * e_timescale;

	// NOTE: this doesn't work properly when jumping mid duck and landing
	// try and get this to round up a little faster while lerping to the target pos at the same speed?
	if ( IsOnGround() && aMoveType == MoveType::Walk )
	{
		if ( targetViewHeight != GetViewHeight() )
		{
			prevDuckLerp = duckLerp;
			//duckLerp = {0, GetViewHeight(), 0};
			//duckLerpGoal = {0, GetViewHeight(), 0};
			duckLerp = GetViewHeight();
			duckLerpGoal = GetViewHeight();
		}

		// duckLerp = glm::lerp( prevDuckLerp, duckLerpGoal, viewHeightLerp );
		duckLerp = std::lerp( prevDuckLerp, duckLerpGoal, viewHeightLerp );
		prevDuckLerp = duckLerp;

		//duckLerp.y = Round( duckLerp.y );
		duckLerp = Round( duckLerp );
		// floating point inprecision smh my head
		//aViewOffset = glm::lerp( prevViewHeight, duckLerp, viewHeightLerp );
		aViewOffset.y = std::lerp( prevViewHeight.y, duckLerp, viewHeightLerp );

		targetViewHeight = GetViewHeight();
	}
	else
	{
		//aViewOffset = glm::lerp( prevViewHeight, {0, targetViewHeight, 0}, viewHeightLerp );
		aViewOffset.y = std::lerp( prevViewHeight.y, targetViewHeight, viewHeightLerp );
	}

	prevViewHeight = aViewOffset;
}


float Player::GetViewHeight(  )
{
	if ( KEY_PRESSED(SDL_SCANCODE_LCTRL) || in_duck )
		return cl_view_height_duck;

	return cl_view_height;
}


void Player::DetermineMoveType(  )
{
	if ( KEY_JUST_PRESSED(SDL_SCANCODE_V) )
		SetMoveType( aMoveType == MoveType::NoClip ? MoveType::Walk : MoveType::NoClip );

	if ( KEY_JUST_PRESSED(SDL_SCANCODE_B) )
		SetMoveType( aMoveType == MoveType::Fly ? MoveType::Walk : MoveType::Fly );
}


void Player::SetMoveType( MoveType type )
{
	aMoveType = type;

	switch (type)
	{
		case MoveType::NoClip:
		{
			EnableGravity( false );
			SetCollisionEnabled( false );
			break;
		}

		case MoveType::Fly:
		{
			EnableGravity( false );
			SetCollisionEnabled( true );
			break;
		}

		case MoveType::Walk:
		{
			EnableGravity( true );
			SetCollisionEnabled( true );
			break;
		}
	}
}


void Player::SetCollisionEnabled( bool enable )
{
#if !NO_BULLET_PHYSICS
	apPhysObj->SetCollisionEnabled( enable );
#endif
}


void Player::EnableGravity( bool enabled )
{
#if !NO_BULLET_PHYSICS
	apPhysObj->SetGravity( enabled ? game->apPhysEnv->GetGravity() : glm::vec3(0, 0, 0) );
#endif
}


void Player::UpdatePosition(  )
{
#if !NO_BULLET_PHYSICS
	//if ( aMoveType == MoveType::Fly )
		aTransform = apPhysObj->GetWorldTransform();
#else
	SetPos( GetPos() + (aVelocity * velocity_scale.GetFloat()) * game->aFrameTime );

	// blech
	//if ( IsOnGround() && aMoveType != MoveType::NoClip )
	//	aTransform.position.y = ground_pos.GetFloat() * velocity_scale.GetFloat();
#endif
}


// temporarily using bullet directly until i abstract this
void Player::DoRayCollision(  )
{
#if !NO_BULLET_PHYSICS
	// temp to avoid a crash intantly?
	if ( aVelocity.x == 0.f && aVelocity.y == 0.f && aVelocity.z == 0.f )
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


bool Player::IsOnGround(  )
{
#if !NO_BULLET_PHYSICS
	btVector3 btFrom = toBt( GetPos() );
	btVector3 btTo( GetPos().x, -10000.0, GetPos().z );
	btCollisionWorld::ClosestRayResultCallback res( btFrom, btTo );

	game->apPhysEnv->apWorld->rayTest( btFrom, btTo, res );

	if ( res.hasHit() )
	{
		btScalar dist = res.m_hitPointWorld.distance2( btFrom );
		if ( dist < 300.f )  // 500.f, 3000.f
		{
			aOnGround = true;
			return true;
		}
		
	}

	return false;
#else
	// aOnGround = GetPos().y <= ground_pos.GetFloat() * velocity_scale.GetFloat();
	bool onGround = GetPos().y <= ground_pos * velocity_scale;

	if ( onGround )
		aPlayerFlags |= PlyOnGround;
	else
		aPlayerFlags &= ~PlyOnGround;

#endif

	return aPlayerFlags & PlyOnGround;

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


bool Player::WasOnGround()
{
	return aPrevPlayerFlags & PlyOnGround;
}

float Player::GetMoveSpeed( glm::vec3 &wishdir, glm::vec3 &wishvel )
{
	wishdir = wishvel;
	float wishspeed = vec3_norm( wishdir );

	if ( wishspeed > aMaxSpeed )
	{
		wishvel = wishvel * aMaxSpeed/wishspeed;
		wishspeed = aMaxSpeed;
	}

	return wishspeed;
}

float Player::GetMaxSpeed(  )
{
	return aMaxSpeed;
}

float Player::GetMaxSpeedBase(  )
{
	return max_speed;
}

float Player::GetMaxSprintSpeed(  )
{
	return GetMaxSpeedBase(  ) * sv_sprint_mult;
}

float Player::GetMaxDuckSpeed(  )
{
	return GetMaxSpeedBase(  ) * sv_duck_mult;
}


void Player::PlayStepSound(  )
{
	auto FreeSound = [ & ]( bool force = false )
	{
		if ( apStepSound && apStepSound->Valid() )
		{
			if ( game->aCurTime - aLastStepTime > cl_stepduration || force )
			{
				game->apAudio->FreeSound( &apStepSound );
				game->apGui->DebugMessage( 7, "Freed Step Sound" );
			}
		}
	};

	//glm::vec2 groundVel = {aVelocity.x, aVelocity.z};
	float speed = glm::length(aVelocity);

	if ( speed < cl_stepspeed )
	{
		FreeSound();
		return;
	}
	
	FreeSound();

	if ( !IsOnGround() )
		return;

	if ( game->aCurTime - aLastStepTime < cl_steptime )
		return;

	FreeSound();

	char soundName[128];
	// this sound just breaks it right now and idk why
	//int soundIndex = ( rand(  ) / ( RAND_MAX / 40.0f ) ) + 1;
	//snprintf(soundName, 128, "sound/footsteps/running_dirt_%s%d.ogg", soundIndex < 10 ? "0" : "", soundIndex);
	snprintf(soundName, 128, "sound/robots_cropped.ogg");

	if ( game->apAudio->LoadSound( soundName, &apStepSound ) )
	{
		apStepSound->vol = 0.5f;
		apStepSound->inWorld = false;

		game->apAudio->PlaySound( apStepSound );

		game->apGui->DebugMessage( 8, "Played Step Sound" );
		game->apGui->DebugMessage( 9, "Step Sound Time: %.4f", game->aCurTime - aLastStepTime );

		aLastStepTime = game->aCurTime;
	}
}


void Player::BaseFlyMove(  )
{
	glm::vec3 wishvel(0,0,0);
	glm::vec3 wishdir(0,0,0);

	// forward and side movement
	for ( int i = 0; i < 3; i++ )
		wishvel[i] = aDirection.forward[i]*aDirection.up[1]*aMove.x + aDirection.right[i]*aMove.z;

	// vertical movement
	// why is this super slow when looking near 80 degrees down or up and higher and when not sprinting?
	wishvel[1] = aDirection.up[2]*aMove.x;

	float wishspeed = GetMoveSpeed( wishdir, wishvel );

	AddFriction(  );
	Accelerate( wishspeed, wishdir );
}


void Player::NoClipMove(  )
{
	BaseFlyMove(  );

#if !NO_BULLET_PHYSICS
	apPhysObj->SetLinearVelocity( aVelocity );
#else
	UpdatePosition(  );
#endif
}


void Player::FlyMove(  )
{
	BaseFlyMove(  );

#if !NO_BULLET_PHYSICS
	apPhysObj->SetLinearVelocity( aVelocity );
#else
	UpdatePosition(  );
#endif

	DoRayCollision(  );
}


void Player::WalkMove(  )
{
	glm::vec3 wishvel = aDirection.forward*aMove.x + aDirection.right*aMove.z;

	//if ( (int)sv_player->v.movetype != MOVETYPE_WALK)
	//	wishvel[1] = aMove.y;
	//else
		wishvel[1] = 0;

	glm::vec3 wishdir(0,0,0);
	float wishspeed = GetMoveSpeed( wishdir, wishvel );

	bool onGround = IsOnGround();

	if ( onGround )
	{
		// blech
#if NO_BULLET_PHYSICS
		//aTransform.position.y = ground_pos.GetFloat() * velocity_scale.GetFloat();
#endif

		AddFriction(  );
		Accelerate( wishspeed, wishdir, false );
	}
	else
	{	// not on ground, so little effect on velocity
		Accelerate( wishspeed, wishvel, true );
		AddGravity(  );
	}

#if !NO_BULLET_PHYSICS
	apPhysObj->SetLinearVelocity( aVelocity );
#else
	UpdatePosition(  );
#endif

	DoRayCollision(  );

	PlayStepSound(  );

	DoSmoothLand( onGround );
	DoViewBob(  );
	DoViewTilt(  );

	if ( IsOnGround() )
		aVelocity.y = 0;
}


void Player::DoSmoothLand( bool wasOnGround )
{
	static glm::vec3 prevViewHeight = {};
	static glm::vec3 fallViewOffset = {};

	static float duckLerp = aVelocity.y * velocity_scale;
	static float prevDuckLerp = aVelocity.y * velocity_scale;

	float landLerp = cl_smooth_land_lerp * cl_lerp_scale * e_timescale;
	float landScale = cl_smooth_land_scale * velocity_scale;
	float landUpScale = cl_smooth_land_up_scale * velocity_scale;

	if ( cl_smooth_land )
	{
		// NOTE: this doesn't work properly when jumping mid duck and landing
		// meh, works well enough with the current values for now
		if ( IsOnGround() && !wasOnGround )
		{
			duckLerp = aVelocity.y * velocity_scale;
			prevDuckLerp = duckLerp;
		}

		// duckLerp = glm::lerp( prevDuckLerp, {0, 0, 0}, GetLandingLerp() );
		// duckLerp = glm::lerp( prevDuckLerp, -prevViewHeight, GetLandingLerp() );
		// acts like a trampoline, hmm
		duckLerp = std::lerp( prevDuckLerp, (-prevViewHeight.y) * landUpScale, landLerp );

		fallViewOffset.y += duckLerp / landScale;
		prevDuckLerp = duckLerp;
	}
	else
	{
		// lerp it back to 0 just in case
		fallViewOffset = glm::lerp( fallViewOffset, {0, 0, 0}, landLerp );
	}

	prevViewHeight = fallViewOffset;
	aViewOffset += fallViewOffset;
}


CONVAR( cl_bob_enabled, 1 );
CONVAR( cl_bob_magnitude, 8 );
CONVAR( cl_bob_freq, 1.65 );
CONVAR( cl_bob_time_scale, 4 );
//CONVAR( cl_bob_freq_mult, 10 );

// obsolete now?
CONVAR( cl_bob_lerp_step, 0.75 );
CONVAR( cl_bob_lerp, 0.5 );
CONVAR( cl_bob_lerp_offset, 0 );
CONVAR( cl_bob_lerp_threshold, 0.5 );


// comment hell right now because it's still not what i want yet
// plus i need to add a small amount of "noise" with rand() to make it less artificial feeling
void Player::DoViewBob(  )
{
	if ( !cl_bob_enabled || !IsOnGround() )
		return;

	static float timeSinceStartedWalking = game->aCurTime;

	float curVel = glm::length( glm::vec2(aVelocity.x, aVelocity.z) ); 

	if ( curVel == 0.f )
		timeSinceStartedWalking = game->aCurTime;

	static float timeSinceLastStep = game->aCurTime;

	float input = game->aCurTime - timeSinceStartedWalking;
	// input += game->aFrameTime;
	// float input = game->aCurTime - timeSinceLastStep;

	// (-(glm::sin( sineInput ) ** 4) ) + 1 // 1 is whatever the max value can be so the lowest value goes to 0

	float speedFactor = glm::clamp( curVel / GetMaxSprintSpeed(), 0.f, 1.f );

	// when velocity changes, this gets offset a lot
	// float sineInput = freq * cl_bob_freq_mult * input;

	// oscilate between -1 and 1 in this math function?
	// then we can just stretch it based on the speedFactor, or just normal velocity?
	// -cos(x * M_PI)

	// or go from to 0 to 1 here?
	// abs(sin(x * M_PI))
	
	// or go from -0.5 to 0.5, where both is multiplied by speedFactor
	// abs(cos(x * M_PI * 0.5f * speedFactor))

	float sineInput = cl_bob_time_scale * input;

	//sineInput *= M_PI * 0.5f * speedFactor;

	// float freq = curVel * cl_bob_freq;
	// float freq = glm::mod(curVel, 2.f) * cl_bob_freq;
	// float freq = speedFactor * cl_bob_freq2;
	float freq = speedFactor;
	//float freq = glm::sin(speedFactor * cl_bob_freq2);

	// IDEA subtract from time when changing speed somehow so that way we don't blast through it?

	// sineInput = glm::sin(sineInput * freq) * freq;
	// sineInput = glm::sin(sineInput) * freq;
	sineInput = glm::sin(sineInput * cl_bob_freq) * freq;
	// sineInput *= freq;
	// sineInput = glm::sin(sineInput) * glm::sin(freq) * freq;

	// float output = glm::abs( cl_bob_magnitude * sin(sineInput) );

	// float output = glm::abs( cl_bob_magnitude * cos(sineInput) ) - cl_bob_magnitude;
	//float output = glm::abs( cl_bob_magnitude * sin(sineInput) );

	// never reaches 0 so do this to get it to 0
	static float sinMessFix = sin(cos(tan(cos(0))));
	//static float sinMessFix = cos(tan(cos(0)));
	
	// using this math function mess instead of abs(sin(x)) and lerping it, since it gives a similar result
	// though it may be too smooth now? idk
	float output = cl_bob_magnitude * (sin(cos(tan(cos(sineInput)))) - sinMessFix);
	//float output = cl_bob_magnitude * (cos(tan(cos(sineInput))) - sinMessFix);

	// cos(tan(sin(x))) or cos(tan(cos(x)))
	// sin(cos(tan(cos(x)))) * 1.2

	// log(x + 0.1) + 1

	// adjust the height of the view bob based on speed
	// output = freq * output;

	static float stepTime = input;

	if ( output == 0.f )
	{
		stepTime = input;
		timeSinceLastStep = input;
	}

	// when nearing 0, lerp it back up so it has a smooth transition in going back up
	if ( cl_bob_lerp_step > output && curVel != 0.f )
	{
		// lerp the viewbob up a little bit
		//output = glm::mix( output, cl_bob_lerp_step.GetFloat(), 0.5 );

		// magic number corrects what the lerp is at rest
		//output += cl_bob_lerp_offset - 0.3750;
	}

	// is there a way i can have this freq "slow down" the faster it gets? like -(x**2)?
	
	// AAAA this just makes the viewbob go exponentially faster the faster we move, i want the opposite !!!
	//float bobSpeedDecrease = glm::clamp(glm::pow(curVel * 0.05f, 2.f), 0.f, 9999.f);
	//float freq = bobSpeedDecrease * cl_bob_freq;

	// float freq = glm::clamp( (curVel / aMaxSpeed) * cl_bob_freq, 0.f, cl_bob_freq_max.GetFloat() );
	// float freq = glm::clamp( curVel * cl_bob_freq, 0.f, cl_bob_freq_max.GetFloat() );

	aViewOffset.y += output;

	//prevViewBob = output;

	game->apGui->DebugMessage( 8, "Walk Time: %.8f", input );
	game->apGui->DebugMessage( 9, "View Bob Offset: %.4f", output );
	//game->apGui->DebugMessage( 21, "View Bob Freq: %.4f", freq );
	game->apGui->DebugMessage( 10, "Sine Input: %.6f", sineInput );
	//game->apGui->DebugMessage( 23, "Sine Input No Time: %.6f", (freq*cl_bob_freq_mult) );
}


CONVAR( cl_tilt, 0.6 );
CONVAR( cl_tilt_speed, 0.1 );


void Player::DoViewTilt(  )
{
	if ( cl_tilt == 0.f )
		return;

	float side = glm::dot(aVelocity, aDirection.right);
	float sign = side < 0 ? -1 : 1;
	float curVel = side * sign; 

	side = fabs(side);

	if (side < cl_tilt_speed.GetFloat())
		side = side * cl_tilt / cl_tilt_speed;
	else
		side = cl_tilt;

	float speedFactor = glm::clamp( curVel / GetMaxSprintSpeed(), 0.f, 1.f );

	/* Lerp the tilt angle by how fast your going */
	float output = glm::mix( 0.f, side * sign, speedFactor );

	aViewAngOffset = {0, 0, output};
}


void Player::AddFriction(  )
{
	glm::vec3	start(0, 0, 0), stop(0, 0, 0);
	float	friction;
	//trace_t	trace;

	glm::vec3 vel = aVelocity;

	float speed = sqrt(vel[0]*vel[0] + vel[2]*vel[2]);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start.x = stop.x = GetPos().x + vel.x / speed*16.f;
	start.z = stop.z = GetPos().z + vel.z / speed*16.f;

	// start.y = GetPos().y + sv_player->v.mins.y;
	start.y = stop.y = GetPos().y + vel.y / speed*16.f;
	stop[1] = start[1] - 34;

	//trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, sv_player);

	//if (trace.fraction == 1.0)
	//	friction = sv_friction.GetFloat() * sv_edgefriction.value;
	//else
		friction = sv_friction;

	// apply friction
	float control = speed < stop_speed ? stop_speed : speed;
	float newspeed = glm::max( 0.f, speed - game->aFrameTime * control * friction );

	newspeed /= speed;
	aVelocity = vel * newspeed;
}


void Player::Accelerate( float wishSpeed, glm::vec3 wishDir, bool inAir )
{
	float baseWishSpeed = inAir ? glm::min( 30.f, vec3_norm( wishDir ) ) : wishSpeed;

	float currentspeed = glm::dot( aVelocity, wishDir );
	float addspeed = baseWishSpeed - currentspeed;

	if ( addspeed <= 0.f )
		return;

	addspeed = glm::min( addspeed, accel_speed * game->aFrameTime * wishSpeed );

	for ( int i = 0; i < 3; i++ )
		aVelocity[i] += addspeed * wishDir[i];
}


void Player::AddGravity(  )
{
#if !NO_BULLET_PHYSICS
	aVelocity.y = apPhysObj->GetLinearVelocity().y;
#else
	aVelocity.y -= sv_gravity * game->aFrameTime;
#endif
}

