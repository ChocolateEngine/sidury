#include <capnp/message.h>
#include <capnp/serialize-packed.h>

#include "game_shared.h"
#include "entity.h"
#include "world.h"
#include "util.h"
#include "player.h"  // TEMP - for CPlayerMoveData

#include "graphics/graphics.h"

#include "game_physics.h"  // just for IPhysicsShape* and IPhysicsObject*
#include "capnproto/sidury.capnp.h"


EntComponentRegistry_t gEntComponentRegistry;


// void* EntComponentRegistry_Create( std::string_view sName )
// {
// 	auto it = gEntComponentRegistry.aComponentNames.find( sName );
// 
// 	if ( it == gEntComponentRegistry.aComponentNames.end() )
// 	{
// 		return nullptr;
// 	}
// 
// 	EntComponentData_t* data = it->second;
// 	Assert( data );
// 	return data->aCreate();
// }


EntCompVarTypeToEnum_t gEntCompVarToEnum[ EEntComponentVarType_Count ] = {
	{ typeid( glm::vec3 ).hash_code(), EEntComponentVarType_Vec3 },
};



EntitySystem* cl_entities = nullptr;
EntitySystem* sv_entities = nullptr;


EntitySystem* GetEntitySystem()
{
	if ( Game_ProcessingClient() )
	{
		Assert( cl_entities );
		return cl_entities;
	}

	Assert( sv_entities );
	return sv_entities;
}


bool EntitySystem::CreateClient()
{
	Assert( !cl_entities );
	if ( cl_entities )
		return false;

	cl_entities = new EntitySystem;
	return cl_entities->Init();
}


bool EntitySystem::CreateServer()
{
	Assert( !sv_entities );
	if ( sv_entities )
		return false;

	sv_entities = new EntitySystem;
	return sv_entities->Init();
}


void EntitySystem::DestroyClient()
{
	if ( cl_entities )
	{
		// Make sure it's shut down
		cl_entities->Shutdown();
		delete cl_entities;
	}

	cl_entities = nullptr;
}


void EntitySystem::DestroyServer()
{
	if ( sv_entities )
	{
		// Make sure it's shut down
		sv_entities->Shutdown();
		delete sv_entities;
	}

	sv_entities = nullptr;
}


// -------------------------------------------------------------


bool EntitySystem::Init()
{
	// Initialize the queue with all possible entity IDs
	// for ( Entity entity = 0; entity < CH_MAX_ENTITIES; ++entity )
	// 	aEntityPool.push( entity );

	aEntityPool.resize( CH_MAX_ENTITIES );
	for ( Entity entity = 0; entity < CH_MAX_ENTITIES; ++entity )
		aEntityPool[ entity ] = entity;

	CreateComponentPools();

	return true;
}


void EntitySystem::Shutdown()
{
}


void EntitySystem::CreateComponentPools()
{
	// i can't create component pools without passing template in ffs
	Log_Error( " *** CREATE ENTITY COMPONENT POOLS *** \n" );
}


Entity EntitySystem::CreateEntity()
{
	Assert( aEntityCount < CH_MAX_ENTITIES && "Too many entities in existence." );

	// Take an ID from the front of the queue
	Entity id = aEntityPool.front();
	// aEntityPool.pop();
	aEntityPool.pop_back();
	++aEntityCount;

	return id;
}


void EntitySystem::DeleteEntity( Entity ent )
{
	Assert( ent < CH_MAX_ENTITIES && "Entity out of range." );

	// Invalidate the destroyed entity's signature
	// aSignatures[ ent ].reset();

	// Put the destroyed ID at the back of the queue
	// aEntityPool.push( ent );
	aEntityPool.push_back( ent );
	--aEntityCount;

	// Notify each component array that an entity has been destroyed
	// If it has a component for that entity, it will remove it
	// for ( auto const& pair : aComponentArrays )
	// {
	// 	auto const& component = pair.second;
	// 
	// 	component->EntityDestroyed( ent );
	// }

	// Erase a destroyed entity from all system lists
	// mEntities is a set so no check needed
	// for ( auto const& pair : aSystems )
	// {
	// 	auto const& system = pair.second;
	// 
	// 	system->aEntities.erase( ent );
	// }
}


Entity EntitySystem::GetEntityCount()
{
	return aEntityCount;
}


Entity EntitySystem::CreateEntityFromServer( Entity desiredId )
{
	size_t index = vec_index( aEntityPool, desiredId );

	if ( index == SIZE_MAX )
	{
		Log_Error( "Entity ID from Server is already taken on Client\n" );
		return CH_ENT_INVALID;
	}

	vec_remove_index( aEntityPool, index );
	return desiredId;
}


// Read and write from the network
void EntitySystem::ReadEntityUpdates( capnp::MessageReader& srReader )
{
	// NetEntityUpdates
	// NetEntityUpdate
}


void EntitySystem::WriteEntityUpdates( capnp::MessageBuilder& srBuilder )
{
}


void EntitySystem::ReadComponents( Entity sEnt, capnp::MessageReader& srReader )
{
}


void EntitySystem::WriteComponents( Entity sEnt, capnp::MessageBuilder& srBuilder )
{
}


void EntitySystem::MarkComponentNetworked( Entity ent, const char* spName )
{
}


// Add a component to an entity
void* EntitySystem::AddComponent( Entity entity, const char* spName )
{
	auto pool = GetComponentPool( spName );

	if ( pool == nullptr )
	{
		Log_FatalF( "Failed to create component - no component pool found: \"%s\"\n", spName );
		return nullptr;
	}

	return pool->Create( entity );
}


// Get a component from an entity
void* EntitySystem::GetComponent( Entity entity, const char* spName )
{
	auto pool = GetComponentPool( spName );

	if ( pool == nullptr )
	{
		Log_FatalF( "Failed to get component - no component pool found: \"%s\"\n", spName );
		return nullptr;
	}

	return pool->GetData( entity );
}


// ====================================================================================================
// Base Components
// 
// TODO: Break these down into base functions for reading and writing certain types and structs
// And then have the Component Database or Entity System call those base functions
// Kind of like NetHelper_ReadVec3 and NetComp_WriteDirection
// ====================================================================================================


// TODO: try this instead for all these
// void TEMP_TransformRead( NetCompTransform::Reader& srReader, void* spData )
void TEMP_TransformRead( capnp::MessageReader& srReader, void* spData )
{
	Assert( spData );

	Transform* spTransform = static_cast< Transform* >( spData );
	auto       message     = srReader.getRoot< NetCompTransform >();

	NetHelper_ReadVec3( message.getPos(), spTransform->aPos );
	NetHelper_ReadVec3( message.getAng(), spTransform->aAng );
	NetHelper_ReadVec3( message.getScale(), spTransform->aScale );
}

void TEMP_TransformWrite( capnp::MessageBuilder& srMessage, const void* spData )
{
	Assert( spData );

	const Transform* spTransform = static_cast< const Transform* >( spData );
	auto             builder     = srMessage.initRoot< NetCompTransform >();

	// NetHelper_WriteVec3( &builder.initPos(), spTransform->aPos );
	// NetHelper_WriteVec3( &builder.initAng(), spTransform->aAng );
	// NetHelper_WriteVec3( &builder.initScale(), spTransform->aScale );
}


CH_COMPONENT_READ_DEF( TransformSmall )
{
	Assert( spData );
	TransformSmall* spTransform = static_cast< TransformSmall* >( spData );
	auto            message     = srReader.getRoot< NetCompTransformSmall >();

	NetHelper_ReadVec3( message.getPos(), spTransform->aPos );
	NetHelper_ReadVec3( message.getAng(), spTransform->aAng );
}

CH_COMPONENT_WRITE_DEF( TransformSmall )
{
	Assert( spData );

	const TransformSmall* spTransform = static_cast< const TransformSmall* >( spData );
	auto                  builder     = srMessage.initRoot< NetCompTransformSmall >();

	// NetHelper_WriteVec3( &builder.initPos(), spTransform->aPos );
	// NetHelper_WriteVec3( &builder.initAng(), spTransform->aAng );
}


CH_COMPONENT_READ_DEF( CRigidBody )
{
	Assert( spData );
	CRigidBody* spRigidBody = static_cast< CRigidBody* >( spData );
	auto        message     = srReader.getRoot< NetCompRigidBody >();

	NetHelper_ReadVec3( message.getVel(), spRigidBody->aVel );
	NetHelper_ReadVec3( message.getAccel(), spRigidBody->aAccel );
}

CH_COMPONENT_WRITE_DEF( CRigidBody )
{
	Assert( spData );

	const CRigidBody* spRigidBody = static_cast< const CRigidBody* >( spData );
	auto              builder  = srMessage.initRoot< NetCompRigidBody >();

	// NetHelper_WriteVec3( &builder.initVel(), spRigidBody->aVel );
	// NetHelper_WriteVec3( &builder.initAccel(), spRigidBody->aAccel );
}

// TEMP, USE THESE PRIMARILY IN THE FUTURE
void NetComp_ReadDirection( const NetCompDirection::Reader& srReader, CDirection& srData )
{
	NetHelper_ReadVec3( srReader.getForward(), srData.aForward );
	NetHelper_ReadVec3( srReader.getUp(), srData.aUp );
	NetHelper_ReadVec3( srReader.getRight(), srData.aRight );
}

void NetComp_WriteDirection( NetCompDirection::Builder* spBuilder, const CDirection& srData )
{
	// NetHelper_WriteVec3( &spBuilder->initForward(), srData.aForward );
	// NetHelper_WriteVec3( &spBuilder->initUp(), srData.aUp );
	// NetHelper_WriteVec3( &spBuilder->initRight(), srData.aRight );
}

CH_COMPONENT_READ_DEF( CDirection )
{
	Assert( spData );

	CDirection* spDirection = static_cast< CDirection* >( spData );
	auto        message     = srReader.getRoot< NetCompDirection >();

	NetComp_ReadDirection( message, *spDirection );
}

CH_COMPONENT_WRITE_DEF( CDirection )
{
	Assert( spData );

	const CDirection* spDirection = static_cast< const CDirection* >( spData );
	auto              builder     = srMessage.initRoot< NetCompDirection >();

	NetComp_WriteDirection( &builder, *spDirection );
}


CH_COMPONENT_READ_DEF( CCamera )
{
	Assert( spData );

	CCamera* spCamera = static_cast< CCamera* >( spData );
	auto     message  = srReader.getRoot< NetCompCamera >();

	NetComp_ReadDirection( message.getDirection(), *spCamera );

	spCamera->aFov = message.getFov();

	NetHelper_ReadVec3( message.getTransform().getPos(), spCamera->aTransform.aPos );
	NetHelper_ReadVec3( message.getTransform().getAng(), spCamera->aTransform.aAng );
}

CH_COMPONENT_WRITE_DEF( CCamera )
{
	Assert( spData );

	const CCamera* spCamera = static_cast< const CCamera* >( spData );
	auto           builder  = srMessage.initRoot< NetCompCamera >();

	// NetComp_WriteDirection( &builder.initDirection(), *spCamera );

	builder.setFov( spCamera->aFov );

	Vec3::Builder pos = builder.getTransform().initPos();
	pos.setX( spCamera->aTransform.aPos.x );
	pos.setY( spCamera->aTransform.aPos.y );
	pos.setZ( spCamera->aTransform.aPos.z );

	Vec3::Builder ang = builder.getTransform().initAng();
	ang.setX( spCamera->aTransform.aAng.x );
	ang.setY( spCamera->aTransform.aAng.y );
	ang.setZ( spCamera->aTransform.aAng.z );
}


CH_COMPONENT_READ_DEF( CGravity )
{
	Assert( spData );

	CGravity* spGravity = static_cast< CGravity* >( spData );
	auto      message  = srReader.getRoot< NetCompGravity >();

	NetHelper_ReadVec3( message.getForce(), spGravity->aForce );
}

CH_COMPONENT_WRITE_DEF( CGravity )
{
	Assert( spData );

	const CGravity* spGravity = static_cast< const CGravity* >( spData );
	auto            builder   = srMessage.initRoot< NetCompGravity >();

	// NetHelper_WriteVec3( &builder.initForce(), spGravity->aForce );
}


CH_COMPONENT_READ_DEF( CModelPath )
{
	Assert( spData );

	auto* spModelPath = static_cast< CModelPath* >( spData );
	auto  message     = srReader.getRoot< NetCompModelPath >();

	spModelPath->aPath = message.getPath();
}

CH_COMPONENT_WRITE_DEF( CModelPath )
{
	Assert( spData );

	const auto* spModelPath = static_cast< const CModelPath* >( spData );
	auto        builder     = srMessage.initRoot< NetCompModelPath >();

	builder.setPath( spModelPath->aPath );
}


CH_COMPONENT_READ_DEF( CPlayerMoveData )
{
	Assert( spData );

	auto* spMoveData = static_cast< CPlayerMoveData* >( spData );
	auto  message    = srReader.getRoot< NetCompPlayerMoveData >();

	auto  moveType   = message.getMoveType();

	switch ( moveType )
	{
		case EPlayerMoveType::WALK:
			spMoveData->aMoveType = PlayerMoveType::Walk;

		default:
		case EPlayerMoveType::NO_CLIP:
			spMoveData->aMoveType = PlayerMoveType::NoClip;

		case EPlayerMoveType::FLY:
			spMoveData->aMoveType = PlayerMoveType::Fly;
	}

	spMoveData->aPlayerFlags      = message.getPlayerFlags();
	spMoveData->aPrevPlayerFlags  = message.getPrevPlayerFlags();
	spMoveData->aMaxSpeed         = message.getMaxSpeed();

	// Smooth Duck
	spMoveData->aPrevViewHeight   = message.getPrevViewHeight();
	spMoveData->aTargetViewHeight = message.getTargetViewHeight();
	spMoveData->aOutViewHeight    = message.getOutViewHeight();
	spMoveData->aDuckDuration     = message.getDuckDuration();
	spMoveData->aDuckTime         = message.getDuckTime();
}

CH_COMPONENT_WRITE_DEF( CPlayerMoveData )
{
	Assert( spData );

	auto* spMoveData = static_cast< const CPlayerMoveData* >( spData );
	auto  builder   = srMessage.initRoot< NetCompPlayerMoveData >();

	switch ( spMoveData->aMoveType )
	{
		case PlayerMoveType::Walk:
			builder.setMoveType( EPlayerMoveType::WALK );

		default:
		case PlayerMoveType::NoClip:
			builder.setMoveType( EPlayerMoveType::NO_CLIP );

		case PlayerMoveType::Fly:
			builder.setMoveType( EPlayerMoveType::FLY );
	}

	builder.setPlayerFlags( spMoveData->aPlayerFlags );
	builder.setPrevPlayerFlags( spMoveData->aPrevPlayerFlags );
	builder.setMaxSpeed( spMoveData->aMaxSpeed );

	// Smooth Duck
	builder.setPrevViewHeight( spMoveData->aPrevViewHeight );
	builder.setTargetViewHeight( spMoveData->aTargetViewHeight );
	builder.setOutViewHeight( spMoveData->aOutViewHeight );
	builder.setDuckDuration( spMoveData->aDuckDuration );
	builder.setDuckTime( spMoveData->aDuckTime );
}


CH_COMPONENT_READ_DEF( Light_t )
{
}


CH_COMPONENT_WRITE_DEF( Light_t )
{
}


void Ent_RegisterBaseComponents()
{
	EntComp_RegisterComponent< Transform >( "transform" );
	EntComp_RegisterComponentVar< Transform, glm::vec3 >( "aPos", "pos", offsetof( Transform, aPos ) );
	EntComp_RegisterComponentVar< Transform, glm::vec3 >( "aAng", "ang", offsetof( Transform, aAng ) );
	EntComp_RegisterComponentVar< Transform, glm::vec3 >( "aScale", "scale", offsetof( Transform, aScale ) );
	EntComp_RegisterComponentReadWrite< Transform >( TEMP_TransformRead, TEMP_TransformWrite );

	CH_REGISTER_COMPONENT_RW( TransformSmall, transformSmall );
	CH_REGISTER_COMPONENT_VAR( TransformSmall, glm::vec3, aPos, pos );
	CH_REGISTER_COMPONENT_VAR( TransformSmall, glm::vec3, aAng, ang );

	CH_REGISTER_COMPONENT_RW( CRigidBody, rigidBody );
	CH_REGISTER_COMPONENT_VAR( CRigidBody, glm::vec3, aVel, vel );
	CH_REGISTER_COMPONENT_VAR( CRigidBody, glm::vec3, aAccel, accel );

	CH_REGISTER_COMPONENT_RW( CDirection, direction );
	CH_REGISTER_COMPONENT_VAR( CDirection, glm::vec3, aForward, forward );
	CH_REGISTER_COMPONENT_VAR( CDirection, glm::vec3, aUp, up );
	// CH_REGISTER_COMPONENT_VAR( CDirection, glm::vec3, aRight, right );
	CH_REGISTER_COMP_VAR_VEC3( CDirection, aRight, right );

	CH_REGISTER_COMPONENT_RW( CGravity, gravity );
	CH_REGISTER_COMP_VAR_VEC3( CGravity, aForce, force );

	// might be a bit weird
	CH_REGISTER_COMPONENT_RW( CCamera, camera );
	CH_REGISTER_COMPONENT_VAR( CCamera, float, aFov, fov );
	CH_REGISTER_COMPONENT_VAR( CCamera, glm::vec3, aForward, forward );
	CH_REGISTER_COMPONENT_VAR( CCamera, glm::vec3, aUp, up );
	CH_REGISTER_COMPONENT_VAR( CCamera, glm::vec3, aRight, right );
	EntComp_RegisterComponentVar< CCamera, glm::vec3 >( "aPos", "pos", offsetof( CCamera, aTransform.aPos ) );
	EntComp_RegisterComponentVar< CCamera, glm::vec3 >( "aAng", "ang", offsetof( CCamera, aTransform.aAng ) );

	CH_REGISTER_COMPONENT_RW( CModelPath, model_path );
	CH_REGISTER_COMPONENT_VAR( CModelPath, std::string, aPath, path );

	// Probably should be in graphics?
	CH_REGISTER_COMPONENT_RW( Light_t, light );
	// CH_REGISTER_COMPONENT_VAR( Light_t, int, aMoveType, moveType );

	// TODO: SHOULD NOT BE HERE !!!!!
	CH_REGISTER_COMPONENT_RW( CPlayerMoveData, playerMoveData );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, int, aMoveType, moveType );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, PlayerFlags, aPlayerFlags, playerFlags );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, PlayerFlags, aPrevPlayerFlags, prevPlayerFlags );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aMaxSpeed, maxSpeed );

	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aPrevViewHeight, prevViewHeight );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aTargetViewHeight, targetViewHeight );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aOutViewHeight, outViewHeight );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aDuckDuration, duckDuration );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aDuckTime, duckTime );
}


// maybe some static component registering
// gEC stands for "global entity component"
// static gEC_RigidBody = Entity_RegisterComponent< CRigidBody >();
// 
// Each component has it's own memory pool
// Now for getting components, this could happen:
// 
// CRigidBody* rigidBody = nullptr;
// bool found = Entity_GetComponent( player, gEC_RigidBody, &rigidBody );
// 
// Handle compHandle = Entity_AddComponent( player, gEC_RigidBody, rigidBody );
// 
// data you can write to is stored in rigidBody as an output pointer
// Handle compHandle = Entity_CreateComponent( player, gEC_RigidBody, &rigidBody );
// 
// Still no such thing as multiple components
// Maybe what you can do instead is multiple entities parented to one? idk
// 
// Or if you still want it, do this
// size_t count = Entity_GetComponentCount( player, gEC_RigidBody );
// bool found   = Entity_GetComponents( player, gEC_RigidBody, &rigidBody, count );
// 
// -----------------------------------------------------------
// 
// But what about for an editor?
// It needs to know the data in the components so it can modify them
// Maybe do some weird thing with static initilization
// Or go the C route, and just have some functions and structs for defining component data types
// 
// there has to be some way to manage these data types
// like something to manage vec3's, mat4's, etc.
// or maybe just only allow certain types internally
// probably do that, yeah
// ChVector's will be funny though (no std::vector allowed)
// 
// use the typeinfo to determine what to do with this and how it shows in the editor
// Entity_RegisterComponentVar( offset, size, name, typeinfo );
//

#if 0

int EntityManager::Init()
{
	// Initialize the queue with all possible entity IDs
	for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
		aEntityPool.push(entity);

	// Register Base Components
	RegisterComponent< Transform >();
	RegisterComponent< TransformSmall >();
	
	// uhhhhh
	RegisterComponent< Handle >();
	RegisterComponent< HModel >();
	RegisterComponent< CRenderable_t >();

	RegisterComponent< Light_t* >();

	//RegisterComponent< ModelData >();  // doesn't like this
	RegisterComponent< CRigidBody >();
	RegisterComponent< CGravity >();
	RegisterComponent< CCamera >();
	RegisterComponent< CSound >();
	RegisterComponent< CDirection >();

#if 1
	RegisterComponent< IPhysicsShape* >();
	RegisterComponent< IPhysicsObject* >();

	// HACK HACK HACK
	// so this ECS doesn't support multiple of the same component on this,
	// so i'll just allow a vector of these physics objects for now
	// RegisterComponent< std::vector<PhysicsObject*> >();
#endif

	return 0;
}


Entity EntityManager::CreateEntity()
{
	assert(aEntityCount < MAX_ENTITIES && "Too many entities in existence.");

	// Take an ID from the front of the queue
	Entity id = aEntityPool.front();
	aEntityPool.pop();
	++aEntityCount;

	return id;
}


void EntityManager::DeleteEntity( Entity ent )
{
	assert(ent < MAX_ENTITIES && "Entity out of range.");

	// Invalidate the destroyed entity's signature
	aSignatures[ent].reset();

	// Put the destroyed ID at the back of the queue
	aEntityPool.push(ent);
	--aEntityCount;

	// Notify each component array that an entity has been destroyed
	// If it has a component for that entity, it will remove it
	for (auto const& pair : aComponentArrays)
	{
		auto const& component = pair.second;

		component->EntityDestroyed( ent );
	}

	// Erase a destroyed entity from all system lists
	// mEntities is a set so no check needed
	for (auto const& pair : aSystems)
	{
		auto const& system = pair.second;

		system->aEntities.erase( ent );
	}
}


void EntityManager::SetSignature( Entity ent, Signature sig )
{
	assert(ent < MAX_ENTITIES && "Entity out of range.");

	// Put this entity's signature into the array
	aSignatures[ent] = sig;
}


Signature EntityManager::GetSignature( Entity ent )
{
	assert(ent < MAX_ENTITIES && "Entity out of range.");

	// Get this entity's signature from the array
	return aSignatures[ent];
}


void EntityManager::EntitySignatureChanged( Entity ent, Signature entSig )
{
	// Notify each system that an entity's signature changed
	for (auto const& pair : aSystems)
	{
		auto const& type = pair.first;
		auto const& system = pair.second;
		auto const& systemSignature = aSystemSignatures[type];

		// Entity signature matches system signature - insert into set
		if ((entSig & systemSignature) == systemSignature)
		{
			system->aEntities.insert(ent);
		}
		// Entity signature does not match system signature - erase from set
		else
		{
			system->aEntities.erase(ent);
		}
	}
}


#if 0
template<typename T>
void ComponentArray<T>::InsertData( Entity entity, T component )
{
	assert( aEntityToIndexMap.find(entity) == aEntityToIndexMap.end() && "Component added to same entity more than once." );

	// Put new entry at end and update the maps
	size_t newIndex = aSize;
	aEntityToIndexMap[entity] = newIndex;
	aIndexToEntityMap[newIndex] = entity;
	aComponentArray[newIndex] = component;
	++aSize;
}


template<typename T>
void ComponentArray<T>::RemoveData( Entity entity )
{
	assert( aEntityToIndexMap.find(entity) != aEntityToIndexMap.end() && "Removing non-existent component." );

	// Copy element at end into deleted element's place to maintain density
	size_t indexOfRemovedEntity = aEntityToIndexMap[entity];
	size_t indexOfLastElement = aSize - 1;
	aComponentArray[indexOfRemovedEntity] = aComponentArray[indexOfLastElement];

	// Update map to point to moved spot
	Entity entityOfLastElement = aIndexToEntityMap[indexOfLastElement];
	aEntityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
	aIndexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

	aEntityToIndexMap.erase(entity);
	aIndexToEntityMap.erase(indexOfLastElement);

	--aSize;
}


template<typename T>
T& ComponentArray<T>::GetData( Entity entity )
{
	assert( aEntityToIndexMap.find(entity) != aEntityToIndexMap.end() && "Retrieving non-existent component." );

	// Return a reference to the entity's component
	return aComponentArray[aEntityToIndexMap[entity]];
}


template<typename T>
void ComponentArray<T>::EntityDestroyed( Entity entity )
{
	if ( aEntityToIndexMap.find(entity) != aEntityToIndexMap.end() )
	{
		// Remove the entity's component if it existed
		RemoveData(entity);
	}
}
#endif

#endif