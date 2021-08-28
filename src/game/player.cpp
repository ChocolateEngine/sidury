#include "player.h"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <algorithm>


Player::Player():
	aCamera(g_pGame->aCamera),
	aVelocity(0, 0, 0),
	aOrigin(0, 0, 0)
{
}

Player::~Player()
{
}


void Player::Spawn()
{
}


void Player::Update( float dt )
{
	UpdateInputs();

	NoClipMove();

	UpdateView();
}


void Player::SetPos( const glm::vec3& origin )
{
	aCamera.transform.position = origin;
}

const glm::vec3& Player::GetPos(  )
{
	return aCamera.transform.position;
}


void Player::UpdateView(  )
{
	static glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	static glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
	static glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);

	glm::quat xRot = glm::angleAxis(glm::radians(mY), glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f)));
	glm::quat yRot = glm::angleAxis(glm::radians(mX), glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)));
	aCamera.transform.rotation = xRot * yRot;

	// hmm
	g_pGame->aView.viewMatrix = ToFirstPersonCameraTransformation(aCamera.transform);

	aCamera.forward = forward * yRot;
	aCamera.up = up * xRot;
	aCamera.right = right * yRot;

	aCamera.back = -aCamera.forward;
	aCamera.down = -aCamera.up;
	aCamera.left = -aCamera.right;

	// hmmmm
	g_pGame->apGraphics->SetView( g_pGame->aView );
}


void Player::UpdateInputs(  )
{
	moveForward = 0.f;
	moveSide = 0.f;

	const Uint8* state = SDL_GetKeyboardState( NULL );
	const float speed = state[SDL_SCANCODE_LSHIFT] ? 2.0f : 1.0f;

	if ( state[SDL_SCANCODE_W] ) moveForward = speed;
	if ( state[SDL_SCANCODE_S] ) moveForward += -speed;
	if ( state[SDL_SCANCODE_A] ) moveSide = -speed;
	if ( state[SDL_SCANCODE_D] ) moveSide += speed;

	// TODO: move aMouseDelta to input system
	mX += g_pGame->aMouseDelta.x * 0.1f;
	mY -= g_pGame->aMouseDelta.y * 0.1f;

	auto constrain = [](float num) -> float
	{
		num = std::fmod(num, 360.0f);
		return (num < 0.0f) ? num += 360.0f : num;
	};

	mX = constrain(mX);
	mY = std::clamp(mY, -80.0f, 80.0f);
}


void Player::OldMove()
{
	const Uint8* state = SDL_GetKeyboardState( NULL );

	const float speed = state[SDL_SCANCODE_LSHIFT] ? 12.f : 6.f;

	glm::vec3 move = {};

	if ( state[SDL_SCANCODE_W] ) move += aCamera.forward;
	if ( state[SDL_SCANCODE_S] ) move += aCamera.back;
	if ( state[SDL_SCANCODE_A] ) move += aCamera.left;
	if ( state[SDL_SCANCODE_D] ) move += aCamera.right;

	if ( state[SDL_SCANCODE_SPACE] ) move += aCamera.up;
	if ( state[SDL_SCANCODE_LCTRL] ) move += aCamera.down;

	glm::normalize( move );
	aCamera.transform.position += move * speed * g_pGame->aFrameTime;

	/*if (input->IsPressed("Jump"))
		camera.transform.position += glm::vec3(0.0f, 1.0f, 0.0f) * speed * dt;
	else if (input->IsPressed("Crouch"))
		camera.transform.position += glm::vec3(0.0f, -1.0f, 0.0f) * speed * dt;*/
}


// glm::normalize doesn't return a float
float VectorNormalize(glm::vec3& v)
{
	float length = sqrt(glm::dot(v, v));

	if (length)
		v *= 1/length;

	return length;
}


// wtf does this even do
void VectorMA( const glm::vec3& start, float scale, const glm::vec3& direction, glm::vec3& dest )
{
	dest = start + direction * scale;
}


// will replace with convars later when implemented
#define MAX_SPEED 4.0f
#define STOP_SPEED 1.5f
#define ACCEL_SPEED 6.f
#define FRICTION 3.f
#define FLY_SPEED 8.f


void Player::BaseFlyMove()
{
	float factor = FLY_SPEED;

	glm::vec3 wishvel(0, 0, 0);
	glm::vec3 wishdir(0, 0, 0);
	float wishspeed = 0;
	float maxspeed = MAX_SPEED * factor;

	float fmove = moveForward * factor;
	float smove = moveSide * factor;

	VectorNormalize(aCamera.forward);  // Normalize remainder of vectors
	VectorNormalize(aCamera.right);    // 

	for (int i=0 ; i<3 ; i++)       // Determine x and y parts of velocity (slow down forward velocity by up vector)
		wishvel[i] = aCamera.forward[i]*aCamera.up[1]*fmove + aCamera.right[i]*smove;

	wishvel[1] = aCamera.up[2]*fmove;

	wishdir = wishvel;   // Determine maginitude of speed of move
	wishspeed = VectorNormalize(wishdir);

	// Clamp to max speed
	if (wishspeed > maxspeed )
	{
		wishvel *= maxspeed/wishspeed;
		wishspeed = maxspeed;
	}

	Accelerate( wishspeed, wishdir );

	float spd = glm::length( aVelocity );
	if (spd < 0.01f)
	{
		aVelocity.x = 0;
		aVelocity.y = 0;
		aVelocity.z = 0;
		return;
	}

	// Bleed off some speed, but if we have less than the bleed
	//  threshhold, bleed the theshold amount.
	float control = (spd < maxspeed/4.0) ? maxspeed/4.0 : spd;

	float friction = FRICTION * 1.0;

	// Add the amount to the drop amount.
	float drop = control * friction * g_pGame->aFrameTime;

	// scale the velocity
	float newspeed = spd - drop;
	if (newspeed < 0)
		newspeed = 0;

	// Determine proportion of old speed we are using.
	newspeed /= spd;
	aVelocity *= newspeed;
}


void Player::NoClipMove(  )
{
	BaseFlyMove(  );

	glm::vec3 out;
	VectorMA( GetPos(), g_pGame->aFrameTime, aVelocity, out );
	SetPos( out );
}


void Player::FlyMove(  )
{
	BaseFlyMove(  );

	// TODO: test for collision
	glm::vec3 out;
	VectorMA( GetPos(), g_pGame->aFrameTime, aVelocity, out );
	SetPos( out );
}


void Player::WalkMove(  )
{
	int			i = 0;
	glm::vec3	wishvel(0,0,0), wishdir(0,0,0);
	float		wishspeed = 0;

	//if ( state[SDL_SCANCODE_SPACE] ) umove += speed;
	//if ( state[SDL_SCANCODE_LCTRL] ) umove -= speed;

	for (i=0 ; i<3 ; i++)
		wishvel[i] = aCamera.forward[i]*moveForward + aCamera.right[i]*moveSide;

	//if ( (int)sv_player->v.movetype != MOVETYPE_WALK)
	//	wishvel[1] = umove;
	//else
		wishvel[1] = 0;

	wishdir = wishvel;

	wishspeed = VectorNormalize(wishdir);
	if (wishspeed > MAX_SPEED)
	{
		wishvel = wishvel * MAX_SPEED/wishspeed;
		wishspeed = MAX_SPEED;
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

	SetPos( GetPos() + aVelocity );
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


#define GRAVITY 2.f

void Player::AddGravity(  )
{
	float gravityScale = 1.0;

	aVelocity[1] -= gravityScale * GRAVITY * g_pGame->aFrameTime;
}


