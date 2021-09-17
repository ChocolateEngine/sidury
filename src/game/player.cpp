#include "player.h"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <algorithm>


ConVar forward_speed( "forward_speed", "400" );
ConVar side_speed( "side_speed", "400" );  // 350.f

ConVar max_speed( "max_speed", "400" );  // 320.f
ConVar stop_speed( "stop_speed", "100" );
ConVar accel_speed( "accel_speed", "10" );
ConVar sv_friction( "sv_friction", "8" );  // 4.f
ConVar jump_force( "jump_force", "500" );

ConVar sv_gravity( "sv_gravity", "800" );
// ConVar ground_pos( "ground_pos", "-48.8" );  // 250 for source_scale
ConVar ground_pos( "ground_pos", "250" );  // 250 for source_scale

// multiplies the final velocity by this amount when setting the player position,
// a workaround for quake movement values not working correctly when lowered
ConVar velocity_scale( "velocity_scale", "0.025" );

#define PLAYER_MASS 200.f

// TEMP
// #define SPAWN_POS 1085.69824, 260, 644.222046
#define SPAWN_POS 1085.69824 * velocity_scale.GetFloat(), 260 * velocity_scale.GetFloat(), 644.222046 * velocity_scale.GetFloat()
// #define SPAWN_POS 1085.69824 * velocity_scale.GetFloat(), -46, 644.222046 * velocity_scale.GetFloat()
// #define SPAWN_POS 10.8569824, -2.60, 6.44222046
// #define SPAWN_POS -26.5, -45, -8.2


CON_COMMAND( respawn )
{
	g_pGame->aLocalPlayer->Respawn();
}


Player::Player():
	mX(0), mY(0),
	aFlags(0),
	maxSpeed(0),
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

	aTransform.position = {SPAWN_POS};

#if !NO_BULLET_PHYSICS
	PhysicsObjectInfo physInfo( ShapeType::Box );
	physInfo.mass = PLAYER_MASS;
	physInfo.transform = aTransform;
	//physInfo.callbacks = true;
	//physInfo.collisionType = CollisionType::Kinematic;
	physInfo.bounds = {16, 72, 16};

	apPhysObj = g_pGame->apPhysEnv->CreatePhysicsObject( physInfo );
	apPhysObj->SetAlwaysActive( true );
	//apPhysObj->SetWorldTransform( aTransform );

	// enable Continuous Collision Detection
	//apPhysObj->aRigidBody.setCcdMotionThreshold( 1e-7 );
	//apPhysObj->aRigidBody.setCcdSweptSphereRadius( 0.5f );
#endif

	SetMoveType( MoveType::Walk );

	// won't change for now
	aViewOffset = {0, 56, 0};
}


void Player::Respawn()
{
	// HACK FOR RIVERHOUSE SPAWN POS
	aTransform.position = {SPAWN_POS};
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

	DetermineMoveType();

	switch ( aMoveType )
	{
		case MoveType::Walk:    WalkMove();     break;
		case MoveType::Fly:     FlyMove();      break;
		case MoveType::NoClip:  NoClipMove();   break;
	}

	UpdateView();
}


void Player::SetPos( const glm::vec3& origin )
{
	aTransform.position = origin;
}

const glm::vec3& Player::GetPos(  )
{
	return aTransform.position;
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
	static glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	static glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
	static glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);

	glm::quat xRot = glm::angleAxis(glm::radians(mY), glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f)));
	glm::quat yRot = glm::angleAxis(glm::radians(mX), glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)));
	aTransform.rotation = xRot * yRot;

	aDirection.Update( forward*yRot, up*xRot, right*yRot );

	Transform transform = aTransform;
	transform.position += aViewOffset * velocity_scale.GetFloat();

	g_pGame->SetViewMatrix( ToFirstPersonCameraTransformation( transform ) );
}


void Player::UpdateInputs(  )
{
	aMove = {0, 0, 0};

	// only way i can think of removing this is when i start to setup networking and prediction
	const Uint8* state = SDL_GetKeyboardState( NULL );

	float moveScale = 1.0f;

	if ( state[SDL_SCANCODE_LCTRL] )
		moveScale = .3;

	else if ( state[SDL_SCANCODE_LSHIFT] )
		moveScale = 2;

	const float forwardSpeed = forward_speed.GetFloat() * moveScale;
	const float sideSpeed = side_speed.GetFloat() * moveScale;
	maxSpeed = max_speed.GetFloat() * moveScale;

	if ( state[SDL_SCANCODE_W] ) aMove.x = forwardSpeed;
	if ( state[SDL_SCANCODE_S] ) aMove.x += -forwardSpeed;
	if ( state[SDL_SCANCODE_A] ) aMove.z = -sideSpeed;
	if ( state[SDL_SCANCODE_D] ) aMove.z += sideSpeed;

	// HACK:
	// why is this so complicated???
	static bool wasJumpButtonPressed = false;
	bool jump = false;

	if ( state[SDL_SCANCODE_SPACE] )
		jump = true; 

	static bool jumped = false;

	if ( jump && IsOnGround() && !jumped )
	{
#if !NO_BULLET_PHYSICS
		apPhysObj->ApplyImpulse( {0, jump_force.GetFloat(), 0} );
#else
		aVelocity.y += jump_force.GetFloat();
#endif
		jumped = true;
	}
	else
	{
		jumped = false;
	}

	wasJumpButtonPressed = jumped;

	mX += g_pGame->apInput->GetMouseDelta().x * 0.1f;
	mY -= g_pGame->apInput->GetMouseDelta().y * 0.1f;

	auto constrain = [](float num) -> float
	{
		num = std::fmod(num, 360.0f);
		return (num < 0.0f) ? num += 360.0f : num;
	};

	mX = constrain(mX);
	mY = std::clamp(mY, -90.0f, 90.0f);
}


// glm::normalize doesn't return a float
float VectorNormalize(glm::vec3& v)
{
	float bruh = glm::lxNorm(v, 2);

	float length = sqrt(glm::dot(v, v));

	if (length)
		v *= 1/length;

	return length;
}


void Player::DetermineMoveType(  )
{
	// will setup properly later, not sure if it will stay in here though
	// right now have a lazy way to toggle noclip
	static bool wasNoClipButtonPressed = false;
	bool toggleNoClip = false;

	const Uint8* state = SDL_GetKeyboardState( NULL );
	if ( state[SDL_SCANCODE_V] )
		toggleNoClip = true; 

	if ( toggleNoClip && !wasNoClipButtonPressed )
	{
		SetMoveType( aMoveType == MoveType::NoClip ? MoveType::Walk : MoveType::NoClip );
	}

	wasNoClipButtonPressed = toggleNoClip;
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
		}

		case MoveType::Fly:
		{
			EnableGravity( false );
			SetCollisionEnabled( true );
		}

		case MoveType::Walk:
		{
			EnableGravity( true );
			SetCollisionEnabled( true );
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
	apPhysObj->SetGravity( enabled ? g_pGame->apPhysEnv->GetGravity() : glm::vec3(0, 0, 0) );
#endif
}


void Player::UpdatePosition(  )
{
#if !NO_BULLET_PHYSICS
	aTransform = apPhysObj->GetWorldTransform();
#else
	SetPos( GetPos() + (aVelocity * velocity_scale.GetFloat()) * g_pGame->aFrameTime );

	// blech
	if ( IsOnGround() && aMoveType != MoveType::NoClip )
		aTransform.position.y = ground_pos.GetFloat() * velocity_scale.GetFloat();
#endif
}


bool Player::IsOnGround(  )
{
#if !NO_BULLET_PHYSICS
	btVector3 btFrom = toBt( GetPos() );
	btVector3 btTo( GetPos().x, -10000.0, GetPos().z );
	btCollisionWorld::ClosestRayResultCallback res( btFrom, btTo );

	g_pGame->apPhysEnv->apWorld->rayTest( btFrom, btTo, res );

	if ( res.hasHit() )
	{
		btScalar dist = res.m_hitPointWorld.distance2( btFrom );
		if ( dist < 300.f )  // 500.f, 3000.f
		{
			return true;
		}
		
	}

	return false;
#else
	return GetPos().y <= ground_pos.GetFloat() * velocity_scale.GetFloat();
#endif

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


float Player::GetMoveSpeed( glm::vec3 &wishdir, glm::vec3 &wishvel )
{
	wishdir = wishvel;
	float wishspeed = VectorNormalize( wishdir );

	if ( wishspeed > maxSpeed )
	{
		wishvel = wishvel * maxSpeed/wishspeed;
		wishspeed = maxSpeed;
	}

	return wishspeed;
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

	if ( IsOnGround() )
	{
		// blech
		aTransform.position.y = ground_pos.GetFloat() * velocity_scale.GetFloat();

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
}


void Player::AddFriction()
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
		friction = sv_friction.GetFloat();

	// apply friction
	float control = speed < stop_speed.GetFloat() ? stop_speed.GetFloat() : speed;
	float newspeed = glm::max( 0.f, speed - g_pGame->aFrameTime * control * friction );

	newspeed /= speed;
	aVelocity = vel * newspeed;
}


void Player::Accelerate( float wishSpeed, glm::vec3 wishDir, bool inAir )
{
	float baseWishSpeed = inAir ? glm::min( 30.f, VectorNormalize( wishDir ) ) : wishSpeed;

	float currentspeed = glm::dot( aVelocity, wishDir );
	float addspeed = baseWishSpeed - currentspeed;

	if ( addspeed <= 0.f )
		return;

	addspeed = glm::min( addspeed, accel_speed.GetFloat() * g_pGame->aFrameTime * wishSpeed );

	for ( int i = 0; i < 3; i++ )
		aVelocity[i] += addspeed * wishDir[i];
}


void Player::AddGravity(  )
{
#if !NO_BULLET_PHYSICS
	aVelocity.y = apPhysObj->GetLinearVelocity().y;
#else
	aVelocity.y -= sv_gravity.GetFloat() * g_pGame->aFrameTime;
#endif
}

