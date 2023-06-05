#pragma once

// Game Implementation of Physics, mainly for helper stuff and loading physics

#include "physics/iphysics.h"
#include "entity.h"

using Entity = size_t;

// Physics Shape Component
struct CPhysShape
{
	IPhysicsShape* apShape;
};


struct CPhysObject
{
	IPhysicsObject* apObj;
};


IPhysicsEnvironment* GetPhysEnv();

// TODO: when physics materials are implemented, split up models by their physics material
void                 Phys_GetModelVerts( Handle sModel, PhysDataConvex_t& srData );
void                 Phys_GetModelTris( Handle sModel, std::vector< PhysTriangle_t >& srTris );
void                 Phys_GetModelInd( Handle sModel, PhysDataConcave_t& srData );

void                 Phys_Init();
void                 Phys_Shutdown();

void                 Phys_CreateEnv( bool sClient );
void                 Phys_DestroyEnv( bool sClient );

// Simulate This Physics Environment
void                 Phys_Simulate( IPhysicsEnvironment* spPhysEnv, float sFrameTime );
void                 Phys_SetMaxVelocities( IPhysicsObject* spPhysObj );

// Helper functions for creating physics shapes/objects in the physics engine and adding a component wrapper to the entity
IPhysicsShape*       Phys_CreateShape( Entity sEntity, PhysicsShapeInfo& srShapeInfo );
IPhysicsObject*      Phys_CreateObject( Entity sEntity, PhysicsObjectInfo& srObjectInfo );
IPhysicsObject*      Phys_CreateObject( Entity sEntity, IPhysicsShape* spShape, PhysicsObjectInfo& srObjectInfo );

// Networking
// void                 Phys_NetworkRead();
// void                 Phys_NetworkWrite();

extern Ch_IPhysics*  ch_physics;

// Helper functions for getting the wrapper physics components
inline auto GetComp_PhysShape( Entity ent )  { return Ent_GetComponent< CPhysShape >( ent, "physShape" ); }
inline auto GetComp_PhysObject( Entity ent ) { return Ent_GetComponent< CPhysObject >( ent, "physObject" ); }

inline IPhysicsShape* GetComp_PhysShapePtr( Entity ent )
{
	if ( auto physShape = GetComp_PhysShape( ent ) )
		return physShape->apShape;

	return nullptr;
}

inline IPhysicsObject* GetComp_PhysObjectPtr( Entity ent )
{
	if ( auto physObject = GetComp_PhysObject( ent ) )
		return physObject->apObj;

	return nullptr;
}

