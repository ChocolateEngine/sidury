#pragma once

#define NO_BULLET_PHYSICS 1

#if !NO_BULLET_PHYSICS

#include <glm/glm.hpp>
//#include <glm/gtx/hash.hpp>
#include <glm/gtx/quaternion.hpp>
//#include <glm/gtx/transform.hpp>

#include "../../chocolate/inc/types/transform.h"
#include "../../chocolate/inc/types/renderertypes.h"
#include "../../chocolate/inc/types/modeldata.h"

#include <LinearMath/btMotionState.h>
#include <LinearMath/btTransform.h>
#include <btBulletDynamicsCommon.h>
#include <BulletSoftBody/btSoftRigidDynamicsWorld.h>


inline glm::quat fromBt(const btQuaternion& from) {
	return glm::quat(from.w(), from.x(), from.y(), from.z());
}

inline glm::vec3 fromBt(const btVector3& from) {
	return glm::vec3(from.x(), from.y(), from.z());
}

inline btQuaternion toBt(const glm::quat& from) {
	return btQuaternion(from.x, from.y, from.z, from.w);
}

inline btVector3 toBt(const glm::vec3& from) {
	return btVector3(from.x, from.y, from.z);
}

inline Transform fromBt(const btTransform& from)
{
	Transform to;
	to.position = fromBt( from.getOrigin() );
	to.rotation = fromBt( from.getRotation() );
	return to;
}

inline btTransform toBt(const Transform& from)
{
	btTransform to;
	to.setOrigin( toBt(from.position) );
	to.setRotation( toBt(from.rotation) );
	return to;
}


enum class ShapeType
{
	Invalid = 0,
	Plane,
	Box,
	Cylinder,
	Sphere,
	Capsule,
	Concave,
	Convex
};


enum class CollisionType
{
	Solid,
	Trigger,
	Static,
	StaticTrigger,
	Kinematic
};


struct PhysicsObjectInfo
{
	PhysicsObjectInfo( ShapeType shapeType )
	{
		this->shapeType = shapeType;
	}

	// Only used for when making a Convex or a Concave collision mesh
	ModelData*          modelData = nullptr;

	// Set a starting position/rotation/scale
	Transform           transform = {};

	// Only used if not a model
	glm::vec3           bounds = {0, 0, 0};

	// Must be set for every shape (put in a constructor maybe?)
	ShapeType           shapeType = ShapeType::Invalid;

	// May be subject to change
	CollisionType       collisionType = CollisionType::Solid;

	// A mass of 0.0 will not have localInertia calculated on creation
	float               mass = 0.f;

	// does nothing yet
	bool                callbacks = false;

	// Tell the physics engine to optimize this convex collision mesh
	bool                optimizeConvex = true;
};


// abstraction for physics objects, or bullet collision shapes
struct PhysicsObject: public btMotionState
{
	PhysicsObject(  );
	~PhysicsObject(  );

	//bool                  Valid(  );

	Transform&            GetWorldTransform(  );
	void                  SetWorldTransform( const Transform& transform );

	void                  Activate( bool active );
	void                  SetAlwaysActive( bool alwaysActive );
	void                  SetCollisionEnabled( bool enable );

	void                  SetLinearVelocity( const glm::vec3& velocity );
	void                  SetAngularVelocity( const glm::vec3& velocity );
	const glm::vec3&      GetLinearVelocity(  );
	const glm::vec3&      GetAngularVelocity(  );

	void                  SetGravity( const glm::vec3& gravity );
	void                  SetGravity( float gravity );  // convenience function

	void                  ApplyForce( const glm::vec3& force );
	void                  ApplyImpulse( const glm::vec3& impulse );

	//void                GetAabb( const Transform& t, glm::vec3& aabbMin, glm::vec3& aabbMax );
	//void                GetBoundingSphere( glm::vec3& center, float& radius );
	//float               GetContactBreakingThreshold( float defaultContactThresholdFactor );

	// calculateTemporalAabb calculates the enclosing aabb for the moving object over interval [0..timeStep)
	// result is conservative
	//void                CalculateTemporalAabb( const Transform& curTrans, const glm::vec3& linvel, const glm::vec3& angvel, float timeStep, glm::vec3& temporalAabbMin, glm::vec3& temporalAabbMax );

	ShapeType         aType = ShapeType::Invalid;
	Transform         aTransform = {};
	PhysicsObjectInfo aPhysInfo;

protected:
	void getWorldTransform(btTransform& worldTransform) const final;
	void setWorldTransform(const btTransform& worldTransform) final;

private:
	btCollisionShape* apCollisionShape = nullptr;
	//btRigidBody&      aRigidBody;
	btRigidBody*       apRigidBody = nullptr;

	friend class PhysicsEnvironment;

};


struct ContactEvent
{
	struct Contact
	{
		glm::vec3 globalContactPosition;
		glm::vec3 globalContactNormal;
		float contactDistance = 0.f;
		float contactImpulse = 0.f;
	};

	PhysicsObject* first = nullptr;
	PhysicsObject* second = nullptr;

	ContactEvent::Contact contacts[4] = {};
	uint8_t contactCount = 0;
};


struct RayHit
{
	struct Contact
	{
		glm::vec3 globalContactPosition;
		glm::vec3 globalContactNormal;
		float contactDistance = 0.f;
		float contactImpulse = 0.f;
	};

	PhysicsObject* physObj = nullptr;

	ContactEvent::Contact contacts[4] = {};
	uint8_t contactCount = 0;
};


class PhysicsEnvironment
{
public:
	PhysicsEnvironment(  );
	~PhysicsEnvironment(  );

	void                            Init(  );
	void                            Shutdown(  );
	void                            Simulate(  );

	PhysicsObject*                  CreatePhysicsObject( PhysicsObjectInfo& physInfo );
	void                            DeletePhysicsObject( PhysicsObject* body );

	void                            SetGravity( const glm::vec3& gravity );
	void                            SetGravity( float gravity );  // convenience function
	const glm::vec3&                GetGravity(  );

	void                            RayTest( const glm::vec3& from, const glm::vec3& to, std::vector< RayHit* >& hits );

	std::vector< PhysicsObject* >   aPhysObjs;

private:
	void                            CreatePhysicsWorld(  );

	btBvhTriangleMeshShape*         LoadModelConCave( PhysicsObjectInfo& physInfo );
	btConvexHullShape*              LoadModelConvex( PhysicsObjectInfo& physInfo );

	btRigidBody*                    CreateRigidBody( PhysicsObject* physObj, PhysicsObjectInfo& physInfo, btCollisionShape* shape );
	void                            DeleteRigidBody( btRigidBody* body );

	btAlignedObjectArray< btCollisionShape* > aCollisionShapes;
	mutable btAlignedObjectArray< btTriangleMesh* > aMeshInterfaces;

	btBroadphaseInterface* apBroadphase;
	btCollisionDispatcher* apDispatcher;
	btConstraintSolver* apSolver;
	btDefaultCollisionConfiguration* apCollisionConfig;

	// temporary
public:
	btDiscreteDynamicsWorld* apWorld;
};

#endif
