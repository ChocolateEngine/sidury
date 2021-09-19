#include "physics.h"
#include "player.h"


#if !NO_BULLET_PHYSICS


PhysicsObject::PhysicsObject(  ):
	aPhysInfo( ShapeType::Invalid )
{
}


PhysicsObject::~PhysicsObject(  )
{
}


void PhysicsObject::getWorldTransform( btTransform& worldTransform ) const
{
	glm::vec3 globalPosition = {};
	glm::quat globalRotation = {};

	aTransform.GlobalDecomposed( &globalPosition, &globalRotation );

	worldTransform.setOrigin( toBt(globalPosition) );
	worldTransform.setRotation( toBt(globalRotation) );
}


void PhysicsObject::setWorldTransform( const btTransform& worldTransform )
{
	glm::vec3 globalPosition = fromBt( worldTransform.getOrigin() );
	glm::quat globalRotation = fromBt( worldTransform.getRotation() );

	// do for parenting?
	//glm::mat4 globalMatrix;
	//worldTransform.getOpenGLMatrix( (btScalar*)&globalMatrix[0] );

	aTransform.position = globalPosition;
	aTransform.rotation = globalRotation;
}


Transform& PhysicsObject::GetWorldTransform(  )
{
	return aTransform;
}


void PhysicsObject::SetWorldTransform( const Transform& transform )
{
	aTransform = transform;
	
	apRigidBody->getWorldTransform().setOrigin( toBt(aTransform.position) );

	apRigidBody->getMotionState()->setWorldTransform( toBt(aTransform) );

	apRigidBody->setLinearVelocity( btVector3(0, 0, 0) );
	apRigidBody->setAngularVelocity( btVector3(0, 0, 0) );
	apRigidBody->clearForces(  );
}


void PhysicsObject::Activate( bool active )
{
	apRigidBody->activate( active );
}


void PhysicsObject::SetAlwaysActive( bool alwaysActive )
{
	if ( alwaysActive )
		apRigidBody->setActivationState( DISABLE_DEACTIVATION );
	else
		apRigidBody->setActivationState( ACTIVE_TAG );
}


void PhysicsObject::SetCollisionEnabled( bool enable )
{
	if ( enable )
		apRigidBody->setCollisionFlags( apRigidBody->getCollisionFlags() & ~btCollisionObject::CF_NO_CONTACT_RESPONSE );
	else
		apRigidBody->setCollisionFlags( apRigidBody->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE );
}


void PhysicsObject::SetContinuousCollisionEnabled( bool enable )
{
	if ( enable )
	{
		apRigidBody->setCcdMotionThreshold( 1e-7 );
		apRigidBody->setCcdSweptSphereRadius( 0.5f );
	}
	else
	{
		// idk how to turn it off lmao
	}
}


void PhysicsObject::SetLinearVelocity( const glm::vec3& velocity )
{
	apRigidBody->setLinearVelocity( toBt(velocity) );
}


void PhysicsObject::SetAngularVelocity( const glm::vec3& velocity )
{
	apRigidBody->setAngularVelocity( toBt(velocity) );
}


const glm::vec3& PhysicsObject::GetLinearVelocity(  )
{
	return fromBt( apRigidBody->getLinearVelocity() );
}


const glm::vec3& PhysicsObject::GetAngularVelocity(  )
{
	return fromBt( apRigidBody->getAngularVelocity() );
}


void PhysicsObject::SetGravity( const glm::vec3& gravity )
{
	apRigidBody->setGravity( toBt(gravity) );
}


void PhysicsObject::SetGravity( float gravity )
{
	apRigidBody->setGravity( btVector3(0, gravity, 0) );
}


void PhysicsObject::ApplyForce( const glm::vec3& force )
{
	apRigidBody->applyCentralForce( toBt(force) );
}


void PhysicsObject::ApplyImpulse( const glm::vec3& impulse )
{
	// apRigidBody->applyCentralImpulse( toBt(impulse) );
	apRigidBody->setLinearVelocity( apRigidBody->getLinearVelocity() + toBt(impulse) );
}



// =========================================================


std::vector< btPersistentManifold* > manifolds;

inline void ReadContactPoint( const btManifoldPoint& from, ContactEvent::Contact* to )
{
	to->contactImpulse = from.getAppliedImpulse();
	to->contactDistance = from.getDistance();
	to->globalContactPosition = fromBt(from.m_positionWorldOnB);
	to->globalContactNormal = fromBt(from.m_normalWorldOnB);
}


inline float ReadManifold( const btPersistentManifold* const& from, ContactEvent* to )
{
	float combinedImpulse = 0;
	to->contactCount = from->getNumContacts();

	for ( uint8_t i = 0; i < to->contactCount; i++ )
	{
		ReadContactPoint( from->getContactPoint(i), &to->contacts[i] );
		combinedImpulse += to->contacts[i].contactImpulse;
	}

	return combinedImpulse;
}


template <bool Colliding>
inline void contactCallback(btPersistentManifold* const& manifold)
{
	PhysicsObject* firstCollider = (PhysicsObject*)manifold->getBody0()->getUserPointer();
	PhysicsObject* secondCollider = (PhysicsObject*)manifold->getBody1()->getUserPointer();
	
	if (!firstCollider->aPhysInfo.callbacks && !secondCollider->aPhysInfo.callbacks)
		return;
	
	/*CollidingEvent collidingEvent;
	collidingEvent.firstEntity = firstCollider->self;
	collidingEvent.secondEntity = secondCollider->self;
	collidingEvent.colliding = Colliding;

	readManifold(manifold, &collidingEvent);
	
	eventsPtr->emit<CollidingEvent>(collidingEvent);*/
}


void TickCallback( btDynamicsWorld* world, btScalar timeStep )
{
	// process manifolds
	for ( uint32_t i = 0; i < g_pGame->apPhysEnv->apWorld->getDispatcher()->getNumManifolds(); i++ ) {
		const auto manifold = g_pGame->apPhysEnv->apWorld->getDispatcher()->getManifoldByIndexInternal(i);

		PhysicsObject* firstCollider = (PhysicsObject*)manifold->getBody0()->getUserPointer();
		PhysicsObject* secondCollider = (PhysicsObject*)manifold->getBody1()->getUserPointer();

		if ((!firstCollider->aPhysInfo.callbacks && !secondCollider->aPhysInfo.callbacks) ||
			(!manifold->getBody0()->isActive() && !manifold->getBody1()->isActive()))
			continue;

		ContactEvent contactEvent;
		contactEvent.first = firstCollider;
		contactEvent.second = secondCollider;

		ReadManifold( manifold, &contactEvent );

		float impulse = ReadManifold(manifold, &contactEvent);

		if ( impulse == 0 )
			continue;

		//eventsPtr->emit<ContactEvent>(contactEvent);
	}

	//eventsPtr->emit<PhysicsUpdateEvent>(PhysicsUpdateEvent{timeStep});
}


PhysicsEnvironment::PhysicsEnvironment(  )
{
}


PhysicsEnvironment::~PhysicsEnvironment(  )
{
}


void PhysicsEnvironment::Init(  )
{
	CreatePhysicsWorld(  );
}


void PhysicsEnvironment::Shutdown(  )
{
}


void PhysicsEnvironment::CreatePhysicsWorld(  )
{
	// collision configuration contains default setup for memory, collision setup
	apCollisionConfig = new btDefaultCollisionConfiguration();

	///use the default collision dispatcher. For parallel processing you can use a diffent dispatcher (see Extras/BulletMultiThreaded)
	apDispatcher = new btCollisionDispatcher(apCollisionConfig);

	apBroadphase = new btDbvtBroadphase();

	///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
	btSequentialImpulseConstraintSolver* sol = new btSequentialImpulseConstraintSolver;
	apSolver = sol;

	apWorld = new btDiscreteDynamicsWorld( apDispatcher, apBroadphase, apSolver, apCollisionConfig );

	apWorld->setGravity( btVector3(0, -800, 0) );

	apWorld->setInternalTickCallback(&TickCallback, 0);

	gContactStartedCallback = contactCallback<true>;
	gContactEndedCallback = contactCallback<false>;
}


void PhysicsEnvironment::Simulate(  )
{
	apWorld->stepSimulation( g_pGame->aFrameTime, 100, 1 / 240.0 );
}


PhysicsObject* PhysicsEnvironment::CreatePhysicsObject( PhysicsObjectInfo& physInfo )
{
	btCollisionShape* collisionShape = nullptr;

	switch ( physInfo.shapeType )
	{
		case ShapeType::Sphere:
			collisionShape = new btSphereShape(physInfo.bounds.x * .5f);
			break;
		case ShapeType::Box:
			// phys->apCollisionShape = new btBoxShape(btVector3(halfExtents.x * .5f, halfExtents.y * .5f, halfExtents.y * .5f));
			collisionShape = new btBoxShape( toBt(physInfo.bounds) );
			break;
		case ShapeType::Plane:
			collisionShape = new btStaticPlaneShape(btVector3(0, 0, 1), 1);
			break;
		case ShapeType::Capsule:
			collisionShape = new btCapsuleShapeZ(physInfo.bounds.x * .5f, physInfo.bounds.y);
			break;
		case ShapeType::Cylinder:
			//phys->apCollisionShape = new btCylinderShapeZ(btVector3(shapeInfo.a * .5f, shapeInfo.a * .5f, shapeInfo.b * .5f));
			collisionShape = new btCylinderShapeZ( toBt(physInfo.bounds) );
			break;
		//case ShapeType::Cone:
		//	collisionShape = new btConeShapeZ(physInfo.bounds.a * .5f, physInfo.bounds.b);
		//	break;
		case ShapeType::Concave:
			collisionShape = LoadModelConCave( physInfo );
			break;
		case ShapeType::Convex:
			collisionShape = LoadModelConvex( physInfo );
			break;
	}

	if ( collisionShape == nullptr )
		return nullptr;

	PhysicsObject* phys = new PhysicsObject;
	phys->apCollisionShape = collisionShape;
	phys->aPhysInfo = physInfo;
	phys->aType = physInfo.shapeType;
	phys->apRigidBody = CreateRigidBody( phys, physInfo, phys->apCollisionShape );

	aCollisionShapes.push_back( phys->apCollisionShape );
	aPhysObjs.push_back( phys );

	if ( physInfo.shapeType == ShapeType::Concave )
	{
		phys->apRigidBody->setFriction( btScalar(0.9) );
	}

	phys->apCollisionShape->setUserPointer( phys );
	phys->apRigidBody->setUserPointer( phys );

	//phys->SetWorldTransform( physInfo.transform );

	return phys;
}


btRigidBody* PhysicsEnvironment::CreateRigidBody( PhysicsObject* physObj, PhysicsObjectInfo& physInfo, btCollisionShape* shape )
{
	if ( !shape || shape->getShapeType() == INVALID_SHAPE_PROXYTYPE )
		return NULL;

	//rigidbody is dynamic if and only if mass is non zero, otherwise static
	bool isDynamic = (physInfo.mass != 0.f);

	btVector3 localInertia(0, 0, 0);
	if (isDynamic)
		shape->calculateLocalInertia( physInfo.mass, localInertia );

	//using motionstate is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects

#define USE_MOTIONSTATE 1
#ifdef USE_MOTIONSTATE
	// btDefaultMotionState* motionState = new btDefaultMotionState(startTransform);
	btRigidBody::btRigidBodyConstructionInfo rigidBodyInfo(physInfo.mass, (btMotionState*)physObj, shape, localInertia);

	rigidBodyInfo.m_linearSleepingThreshold = 32;
	rigidBodyInfo.m_angularSleepingThreshold = 32;

	btRigidBody* body = new btRigidBody(rigidBodyInfo);
	//body->setContactProcessingThreshold(m_defaultContactProcessingThreshold);

	// crashes the physics world, wtf
	//body->setWorldTransform( toBt(physInfo.transform) );

	// might change this later?
	switch ( physInfo.collisionType )
	{
		case CollisionType::Trigger:
			body->setCollisionFlags( btCollisionObject::CollisionFlags::CF_NO_CONTACT_RESPONSE );
			break;
		case CollisionType::StaticTrigger:
			body->setCollisionFlags( btCollisionObject::CollisionFlags::CF_NO_CONTACT_RESPONSE | btCollisionObject::CollisionFlags::CF_STATIC_OBJECT );
			break;
		case CollisionType::Static:
			body->setCollisionFlags( btCollisionObject::CollisionFlags::CF_STATIC_OBJECT );
			break;
		case CollisionType::Kinematic:
			body->setCollisionFlags( btCollisionObject::CollisionFlags::CF_KINEMATIC_OBJECT );
			break;
	}

	if ( physInfo.callbacks )
		body->setCollisionFlags( body->getCollisionFlags() | btCollisionObject::CollisionFlags::CF_CUSTOM_MATERIAL_CALLBACK );

#else
	btRigidBody* body = new btRigidBody(mass, 0, shape, localInertia);
	body->setWorldTransform(startTransform);
#endif  //

	//body->setUserIndex(-1);
	apWorld->addRigidBody( body );
	return body;
}


void PhysicsEnvironment::DeleteRigidBody( btRigidBody* body )
{
	apWorld->removeRigidBody( body );
	btMotionState* ms = body->getMotionState();
	delete body;
	delete ms;
}


btBvhTriangleMeshShape* PhysicsEnvironment::LoadModelConCave( PhysicsObjectInfo& physInfo )
{
	if ( physInfo.modelData == nullptr )
		return nullptr;

	ModelData& model = *physInfo.modelData;

	btTransform transform;
	transform.setIdentity();
	transform.setOrigin( toBt(model.aTransform.position) );

	btAlignedObjectArray<btVector3> convertedVerts;
	convertedVerts.reserve( model.aVertexCount );
	for (int i = 0; i < model.aVertexCount; i++)
	{
		convertedVerts.push_back(btVector3(
			model.apVertices[i].pos[0] * model.aTransform.scale.x,
			model.apVertices[i].pos[1] * model.aTransform.scale.y,
			model.apVertices[i].pos[2] * model.aTransform.scale.z));
	}

	btTriangleMesh* meshInterface = new btTriangleMesh();
	aMeshInterfaces.push_back( meshInterface );

	for (int i = 0; i < model.aIndexCount / 3; i++)
	{
		glm::vec3 v0 = model.apVertices[ model.apIndices[i * 3]     ].pos;
		glm::vec3 v1 = model.apVertices[ model.apIndices[i * 3 + 1] ].pos;
		glm::vec3 v2 = model.apVertices[ model.apIndices[i * 3 + 2] ].pos;

		meshInterface->addTriangle(btVector3(v0[0], v0[1], v0[2]),
									btVector3(v1[0], v1[1], v1[2]),
									btVector3(v2[0], v2[1], v2[2]));
	}

	btBvhTriangleMeshShape* trimesh = new btBvhTriangleMeshShape( meshInterface, true, true );
	aCollisionShapes.push_back( trimesh );

	return trimesh;
}


btConvexHullShape* PhysicsEnvironment::LoadModelConvex( PhysicsObjectInfo& physInfo )
{
	if ( physInfo.modelData == nullptr )
		return nullptr;

	ModelData& model = *physInfo.modelData;

	//btConvexHullShape* shape = new btConvexHullShape((const btScalar*)(&(v->pos[0])), modelData.aVertexCount, sizeof(vertex_3d_t));
	btConvexHullShape* shape = new btConvexHullShape((const btScalar*)(&(model.apVertices->pos[0])), model.aVertexCount, sizeof(vertex_3d_t));
	aCollisionShapes.push_back( shape );

	if ( physInfo.optimizeConvex )
		shape->optimizeConvexHull();

	//if (m_options & ComputePolyhedralFeatures)
	//if ( true )
		//shape->initializePolyhedralFeatures();

	//shape->setMargin(0.001);

	return shape;
}


void PhysicsEnvironment::SetGravity( const glm::vec3& gravity )
{
	apWorld->setGravity( toBt(gravity) );
}


void PhysicsEnvironment::SetGravity( float gravity )
{
	apWorld->setGravity( btVector3(0, gravity, 0) );
}


const glm::vec3& PhysicsEnvironment::GetGravity(  )
{
	return fromBt( apWorld->getGravity() );
}


// UNFINISHED !!!!
void PhysicsEnvironment::RayTest( const glm::vec3& from, const glm::vec3& to, std::vector< RayHit* >& hits )
{
	btVector3 btFrom( toBt(from) );
	btVector3 btTo( toBt(to) );
	btCollisionWorld::AllHitsRayResultCallback res( btFrom, btTo );

	// res.m_flags |= btTriangleRaycastCallback::kF_KeepUnflippedNormal;
	// res.m_flags |= btTriangleRaycastCallback::kF_UseSubSimplexConvexCastRaytest;

	apWorld->rayTest( btFrom, btTo, res );

	hits.reserve( res.m_collisionObjects.size() );

	for ( uint32_t i = 0; i < res.m_collisionObjects.size(); i++ )
	{
		// PhysicsObject* physObj = (PhysicsObject*)res.m_collisionObjects[i]->getUserPointer();
		// RayHit* hit = new RayHit;

		//hits.push_back( hit );
	}
}


#endif // !NO_BULLET_PHYSICS