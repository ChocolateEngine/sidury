#pragma once

#define BULLET_PHYSICS 0

#ifndef BULLET_PHYSICS
#define BULLET_PHYSICS 0
#define NO_BULLET_PHYSICS 1  // get rid of this macro
#else
#define NO_BULLET_PHYSICS !BULLET_PHYSICS
#endif

#ifndef NO_BULLET_PHYSICS
#define NO_BULLET_PHYSICS 1
#endif

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

inline btQuaternion toBtRot(const glm::vec3& from)
{
	glm::quat q = AngToQuat( glm::radians(from) );
	//return btQuaternion(q.x, q.z, -q.y, q.w);
	return btQuaternion(q.x, q.z, -q.y, q.w);
	//return btQuaternion(q.x, -q.z, q.y, q.w);
}

inline glm::vec3 fromBtRot(const btQuaternion& from)
{
	//glm::quat q(from.getX(), -from.getZ(), from.getY(), from.getW());
	glm::quat q(from.getX(), from.getZ(), from.getY(), from.getW());
	//glm::quat q(from.getX(), from.getZ(), -from.getY(), from.getW());
	return glm::degrees( QuatToAng(q) );
}

inline Transform fromBt(const btTransform& from)
{
	Transform to;
	to.aPos = fromBt( from.getOrigin() );
	//to.aAng = glm::eulerAngles( fromBt( from.getRotation() ) );
	to.aAng = fromBtRot( from.getRotation() );
	return to;
}

inline btTransform toBt(const Transform& from)
{
	btTransform to;
	to.setOrigin( toBt(from.aPos) );
	///to.setRotation( toBt( AngToQuat( glm::radians(from.aAng) ) ) );
	to.setRotation( toBtRot( from.aAng ) );
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
	// ModelData*          modelData = nullptr;
	IMesh*              mesh = nullptr;

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
	void                  SetContinuousCollisionEnabled( bool enable );

	void                  SetScale( const glm::vec3& scale );

	void                  SetLinearVelocity( const glm::vec3& velocity );
	void                  SetAngularVelocity( const glm::vec3& velocity );
	glm::vec3             GetLinearVelocity(  );
	glm::vec3             GetAngularVelocity(  );
	void                  SetAngularFactor( const glm::vec3& ang );
	void                  SetAngularFactor( float factor );
	void                  SetSleepingThresholds( float min, float max );
	void                  SetFriction( float val );

	void                  SetGravity( const glm::vec3& gravity );
	void                  SetGravity( float gravity );  // convenience function

	void                  ApplyForce( const glm::vec3& force );
	void                  ApplyImpulse( const glm::vec3& impulse );

	int                   ContactTest();

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

public:
// private:
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
	glm::vec3                       GetGravity(  );

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

extern PhysicsEnvironment* physenv;

#endif
