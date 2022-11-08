#pragma once

// Game Implementation of Physics, mainly for helper stuff and loading physics

#include "physics/iphysics.h"


// TODO: when physics materials are implemented, split up models by their physics material
void Phys_GetModelVerts( Handle sModel, PhysDataConvex_t& srData );
void Phys_GetModelTris( Handle sModel, std::vector< PhysTriangle_t >& srTris );
void Phys_GetModelInd( Handle sModel, PhysDataConcave_t& srData );


void Phys_Init();
void Phys_Shutdown();
void Phys_SetMaxVelocities( IPhysicsObject* spPhysObj );


extern IPhysicsEnvironment* physenv;
extern Ch_IPhysics* ch_physics;

