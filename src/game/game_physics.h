#pragma once

/*
* Game Implementation of Physics, mainly for helper stuff and loading physics
*/

#include "physics/iphysics.h"

// IDEA: make an actual physics object component thing here
//  and make a system to create a physics object for that component
//  that way, you don't need to make a physics object and then add that as a component


#define BULLET_PHYSICS 1


class GamePhysics
{
public:
	GamePhysics();
	~GamePhysics();

	void Init();
	void SetMaxVelocities( IPhysicsObject* spPhysObj );
};


extern IPhysicsEnvironment* physenv;

extern GamePhysics gamephys;

extern Ch_IPhysics* ch_physics;

