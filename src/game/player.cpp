#include "player.h"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <algorithm>



Player::Player():
	mX(0), mY(0),
	aFlags(0),
	maxSpeed(0),
	moveForward(0), moveSide(0),
	aVelocity(0, 0, 0),
	aMoveType(MoveType::Walk),
	aOrigin(0, 0, 0)
{
	aTransform = {};
	aDirection = {};
}

Player::~Player()
{
}


void Player::Spawn()
{
	Respawn();
}


void Player::Respawn()
{
	// HACK FOR RIVERHOUSE SPAWN POS
	//aTransform.pos = {114.112556, 1690.37122, -982.597900};
	aTransform.pos = {-43.8928490, 1706.54492, 364.839417};
	aTransform.rot = {0, 0, 0, 0};
	aVelocity = {0, 0, 0};
	aOrigin = {0, 0, 0};
	moveForward = 0.f;
	moveSide = 0.f;
	mX = 0.f;
	mY = 0.f;
}


void Player::Update( float dt )
{
	UpdateInputs();

	DetermineMoveType();

	switch ( aMoveType )
	{
		case MoveType::Walk:
			WalkMove();
			break;

		case MoveType::Fly:
			FlyMove();
			break;

		case MoveType::NoClip:
		default:
			NoClipMove();
			break;
	}

	UpdateView();
}


void Player::SetPos( const glm::vec3& origin )
{
	aTransform.pos = origin;
}

const glm::vec3& Player::GetPos(  )
{
	return aTransform.pos;
}


void Player::UpdateView(  )
{
	static glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	static glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
	static glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);

	glm::quat xRot = glm::angleAxis(glm::radians(mY), glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f)));
	glm::quat yRot = glm::angleAxis(glm::radians(mX), glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)));
	aTransform.rot = xRot * yRot;

	aDirection.Update( forward*yRot, up*xRot, right*yRot );

	g_pGame->SetViewMatrix( ToFirstPersonCameraTransformation( aTransform ) );
}


// will replace with convars later when implemented
#define FORWARD_SPEED 400.f
#define SIDE_SPEED 400.f  // 350.f


#define MAX_SPEED 400.f  // 320.f
#define STOP_SPEED 100.f
#define ACCEL_SPEED 10.f
#define FRICTION 8.f // 4.f


void Player::UpdateInputs(  )
{
	moveForward = 0.f;
	moveSide = 0.f;

	// blech
	const Uint8* state = SDL_GetKeyboardState( NULL );
	const float forwardSpeed = state[SDL_SCANCODE_LSHIFT] ? FORWARD_SPEED * 2.0f : FORWARD_SPEED;
	const float sideSpeed = state[SDL_SCANCODE_LSHIFT] ? SIDE_SPEED * 2.0f : SIDE_SPEED;
	maxSpeed = state[SDL_SCANCODE_LSHIFT] ? MAX_SPEED * 2.0f : MAX_SPEED;

	if ( state[SDL_SCANCODE_W] ) moveForward = forwardSpeed;
	if ( state[SDL_SCANCODE_S] ) moveForward += -forwardSpeed;
	if ( state[SDL_SCANCODE_A] ) moveSide = -sideSpeed;
	if ( state[SDL_SCANCODE_D] ) moveSide += sideSpeed;

	// TODO: move aMouseDelta to input system
	mX += g_pGame->aMouseDelta.x * 0.1f;
	mY -= g_pGame->aMouseDelta.y * 0.1f;

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
		if ( aMoveType == MoveType::NoClip )
		{
			aMoveType = MoveType::Walk;
		}
		else
		{
			aMoveType = MoveType::NoClip;
		}
	}

	wasNoClipButtonPressed = toggleNoClip;
}


void Player::UpdatePosition(  )
{
	SetPos( GetPos() + aVelocity * g_pGame->aFrameTime );
}


void Player::BaseFlyMove(  )
{
	int			i = 0;
	glm::vec3	wishvel(0,0,0), wishdir(0,0,0);
	float		wishspeed = 0;

	// forward and side movement
	for (i=0 ; i<3 ; i++)
		wishvel[i] = aDirection.forward[i]*aDirection.up[1]*moveForward + aDirection.right[i]*moveSide;

	// vertical movement
	// why is this super slow when looking near 80 degrees down or up and higher?
	wishvel[1] = aDirection.up[2]*moveForward;

	wishdir = wishvel;

	wishspeed = VectorNormalize(wishdir);
	if (wishspeed > maxSpeed)
	{
		wishvel = wishvel * maxSpeed/wishspeed;
		wishspeed = maxSpeed;
	}

	AddFriction(  );
	Accelerate( wishspeed, wishdir );
}


void Player::NoClipMove(  )
{
	BaseFlyMove(  );
	UpdatePosition(  );
}


void Player::FlyMove(  )
{
	BaseFlyMove(  );
	UpdatePosition(  );
}


void Player::WalkMove(  )
{
	int			i = 0;
	glm::vec3	wishvel(0,0,0), wishdir(0,0,0);
	float		wishspeed = 0;

	for (i=0 ; i<3 ; i++)
		wishvel[i] = aDirection.forward[i]*moveForward + aDirection.right[i]*moveSide;

	//if ( (int)sv_player->v.movetype != MOVETYPE_WALK)
	//	wishvel[1] = umove;
	//else
		wishvel[1] = 0;

	wishdir = wishvel;

	wishspeed = VectorNormalize(wishdir);
	if (wishspeed > maxSpeed)
	{
		wishvel = wishvel * maxSpeed/wishspeed;
		wishspeed = maxSpeed;
	}

	// TEMP
	bool onground = true;

	if ( onground )
	{
		AddFriction(  );
		Accelerate( wishspeed, wishdir );
	}
	/*else
	{	// not on ground, so little effect on velocity
		SV_AirAccelerate (wishspeed, wishvel);
	}*/

	// just gonna put this here
	// AddGravity(  );

	UpdatePosition(  );
}


void Player::AddFriction()
{
	glm::vec3	vel(0, 0, 0);
	float	speed, newspeed, control;
	glm::vec3	start(0, 0, 0), stop(0, 0, 0);
	float	friction;
	//trace_t	trace;

	vel = aVelocity;

	speed = sqrt(vel[0]*vel[0] + vel[2]*vel[2]);
	if (!speed)
		return;

	// if the leading edge is over a dropoff, increase friction
	start[0] = stop[0] = GetPos().x + vel[0]/speed*16;
	//start[1] = stop[1] = GetPos().y + vel[1]/vspeed*16;
	// start[2] = aOrigin[2] + sv_player->v.mins[2];
	//start[1] = GetPos().y + -1.f;
	start[2] = stop[2] = GetPos().z + vel[2]/speed*16;
	stop[1] = start[1] - 34;

	//trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, sv_player);

	//if (trace.fraction == 1.0)
	//	friction = FRICTION*sv_edgefriction.value;
	//else
		friction = FRICTION;

	// apply friction
	control = speed < STOP_SPEED ? STOP_SPEED : speed;
	newspeed = speed - g_pGame->aFrameTime * control * friction;

	if (newspeed < 0)
		newspeed = 0;
	newspeed /= speed;

	aVelocity[0] = vel[0] * newspeed;
	aVelocity[1] = vel[1] * newspeed;
	aVelocity[2] = vel[2] * newspeed;
}


void Player::Accelerate( float wishspeed, const glm::vec3 wishdir )
{
	int			i;
	float		addspeed, accelspeed, currentspeed;

	currentspeed = glm::dot(aVelocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = ACCEL_SPEED * g_pGame->aFrameTime * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	for (i=0 ; i<3 ; i++)
		aVelocity[i] += accelspeed*wishdir[i];
}


#define GRAVITY 0.08f

void Player::AddGravity(  )
{
	float gravityScale = 1.0;

	aVelocity[1] -= gravityScale * GRAVITY * g_pGame->aFrameTime;
}


