#pragma once

struct Model;
class IPhysicsObject;


struct ModelPhysTest
{
	Model*                         mdl;
	std::vector< IPhysicsObject* > physObj;
};


void TEST_Init();
void TEST_Shutdown();
void TEST_EntUpdate();

void TEST_CL_UpdateProtos( float frameTime );
void TEST_SV_UpdateProtos( float frameTime );

void TEST_UpdateAudio();

