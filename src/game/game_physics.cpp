#include "game_physics.h"


IPhysicsEnvironment* physenv = nullptr;
Ch_IPhysics* ch_physics = nullptr;

GamePhysics gamephys;


CONVAR_CMD( phys_gravity, -800 )
{
	physenv->SetGravityZ( phys_gravity );
}


GamePhysics::GamePhysics()
{
}


GamePhysics::~GamePhysics()
{
	ch_physics->DestroyPhysEnv( physenv );
}


void GamePhysics::Init()
{
	GET_SYSTEM_ASSERT( ch_physics, Ch_IPhysics );
	physenv = ch_physics->CreatePhysEnv();
	physenv->Init(  );
	physenv->SetGravityZ( phys_gravity );
}


void GamePhysics::SetMaxVelocities( IPhysicsObject* spPhysObj )
{
	if ( !spPhysObj )
		return;

	spPhysObj->SetMaxLinearVelocity( 50000.f );
	spPhysObj->SetMaxAngularVelocity( 50000.f );
}

