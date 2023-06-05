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


const char* EntComp_NetTypeToStr( EEntComponentNetType sNetType )
{
	switch ( sNetType )
	{
		case EEntComponentNetType_None:
			return "None - This component is never networked";

		case EEntComponentNetType_Both:
			return "Both - This component is on both client and server";

		case EEntComponentNetType_Client:
			return "Client - This component is only on the Client";

		case EEntComponentNetType_Server:
			return "Server - This component is only on the Server";

		default:
			return "UNKNOWN";
	}
}


static const char* gEntVarTypeStr[ EEntComponentVarType_Count ] = {
	"INVALID",

	"bool",

	"float",
	"double",

#if 0
	"s8",
	"s16",
	"s32",
	"s64",

	"u8",
	"u16",
	"u32",
	"u64",
#else
	"char",
	"short",
	"int",
	"long long",

	"unsigned char",
	"unsigned short",
	"unsigned int",
	"unsigned long long",
#endif

	"glm::vec2",
	"glm::vec3",
	"glm::vec4",
};


const char* EntComp_VarTypeToStr( EEntComponentVarType sVarType )
{
	if ( sVarType < 0 || sVarType > EEntComponentVarType_Count )
		return gEntVarTypeStr[ EEntComponentVarType_Invalid ];

	return gEntVarTypeStr[ sVarType ];
}


std::string EntComp_GetStrValueOfVar( void* spData, EEntComponentVarType sVarType )
{
	switch ( sVarType )
	{
		default:
		{
			return "INVALID OR UNFINISHED";
		}
		case EEntComponentVarType_Invalid:
		{
			return "INVALID";
		}
		case EEntComponentVarType_Bool:
		{
			return *static_cast< bool* >( spData ) ? "TRUE" : "FALSE";
		}

		case EEntComponentVarType_Float:
		{
			return ToString( *static_cast< float* >( spData ) );
		}
		case EEntComponentVarType_Double:
		{
			return ToString( *static_cast< double* >( spData ) );
		}

		case EEntComponentVarType_S8:
		{
			s8 value = *static_cast< s8* >( spData );
			return vstring( "%c", value );
		}
		case EEntComponentVarType_S16:
		{
			s16 value = *static_cast< s16* >( spData );
			return vstring( "%d", value );
		}
		case EEntComponentVarType_S32:
		{
			s32 value = *static_cast< s32* >( spData );
			return vstring( "%d", value );
		}
		case EEntComponentVarType_S64:
		{
			s32 value = *static_cast< s32* >( spData );
			return vstring( "%lld", value );
		}

		case EEntComponentVarType_U8:
		{
			u8 value = *static_cast< u8* >( spData );
			return vstring( "%uc", value );
		}
		case EEntComponentVarType_U16:
		{
			u16 value = *static_cast< u16* >( spData );
			return vstring( "%ud", value );
		}
		case EEntComponentVarType_U32:
		{
			u32 value = *static_cast< u32* >( spData );
			return vstring( "%ud", value );
		}
		case EEntComponentVarType_U64:
		{
			u64 value = *static_cast< u64* >( spData );
			return vstring( "%zd", value );
		}

		case EEntComponentVarType_Vec2:
		{
			const glm::vec2* value = (const glm::vec2*)spData;
			return vstring( "(%.4f, %.4f)", value->x, value->y );
		}
		case EEntComponentVarType_Vec3:
		{
			return Vec2Str( *(const glm::vec3*)spData );
		}
		case EEntComponentVarType_Vec4:
		{
			const glm::vec4* value = (const glm::vec4*)spData;
			return vstring( "(%.4f, %.4f, %.4f, %.4f)", value->x, value->y, value->z, value->w );
		}
	}
}


std::string EntComp_GetStrValueOfVarOffset( size_t sOffset, void* spData, EEntComponentVarType sVarType )
{
	char* data = static_cast< char* >( spData );
	return EntComp_GetStrValueOfVar( data + sOffset, sVarType );
}


CONCMD( ent_dump_registry )
{
	LogGroup group = Log_GroupBegin();

	Log_GroupF( group, "Entity Count: %zd\n", GetEntitySystem()->aEntityCount );
	Log_GroupF( group, "Registered Components: %zd\n", gEntComponentRegistry.aComponents.size() );

	for ( const auto& [ name, regData ] : gEntComponentRegistry.aComponentNames )
	{
		Assert( regData );

		if ( !regData )
			continue;

		Log_GroupF( group, "\nComponent: %s\n", regData->apName );
		Log_GroupF( group, "   Override Client: %s\n", regData->aOverrideClient ? "TRUE" : "FALSE" );
		Log_GroupF( group, "   Networking Type: %s\n", EntComp_NetTypeToStr( regData->aNetType ) );
		Log_GroupF( group, "   Has Write:       %s\n", regData->apWrite ? "TRUE" : "FALSE" );
		Log_GroupF( group, "   Has Read:        %s\n", regData->apRead ? "TRUE" : "FALSE" );
		Log_GroupF( group, "   Variable Count:  %zd\n", regData->aVars.size() );

		for ( const auto & [ offset, var ] : regData->aVars )
		{
			Log_GroupF( group, "       %s - %s\n", EntComp_VarTypeToStr( var.aType ), var.apName );
		}
	}

	Log_GroupEnd( group );
}


CONCMD( ent_dump )
{
	bool useServer = false;

	if ( Game_IsHosting() && Game_ProcessingClient() )
	{
		// We have access to the server's entity system, so if we want it, use that
		if ( args.size() > 0 && args[ 0 ] == "server" )
		{
			useServer = true;
			Game_SetClient( false );
		}
	}

	LogGroup group = Log_GroupBegin();

	Log_GroupF( group, "Entity Count: %zd\n", GetEntitySystem()->aEntityCount );

	Log_GroupF( group, "Registered Components: %zd\n", gEntComponentRegistry.aComponents.size() );

	for ( const auto& [ name, pool ] : GetEntitySystem()->aComponentPools )
	{
		Assert( pool );

		if ( !pool )
			continue;

		Log_GroupF( group, "Component Pool: %s - %zd Components in Pool\n", name.data(), pool->GetCount() );
	}

	Log_GroupF( group, "Components: %zd\n", GetEntitySystem()->aComponentPools.size() );

	for ( Entity i = 0; i < GetEntitySystem()->aEntityCount; i++ )
	{
		Entity entity = GetEntitySystem()->aUsedEntities[ i ];

		// this is the worst thing ever
		std::vector< IEntityComponentPool* > pools;

		// Find all component pools that contain this Entity
		// That means the Entity has the type of component that pool is for
		for ( auto& [ name, pool ] : GetEntitySystem()->aComponentPools )
		{
			if ( pool->Contains( entity ) )
				pools.push_back( pool );
		}

		Log_GroupF( group, "\nEntity %zd\n", entity );
		// Log_GroupF( group, "    Predicted: %s\n", GetEntitySystem() );

		for ( IEntityComponentPool* pool : pools )
		{
			auto data    = pool->GetData( entity );
			auto regData = pool->GetRegistryData();

			Log_GroupF( group, "\n    Component: %s\n", regData->apName );
			Log_GroupF( group,   "    Predicted: %s\n", pool->IsPredicted( entity ) ? "TRUE" : "FALSE" );

			for ( const auto& [ offset, var ] : regData->aVars )
			{
				Log_GroupF( group, "        %s = %s\n", var.apName, EntComp_GetStrValueOfVarOffset( offset, data, var.aType ).c_str() );
			}
		}
	}

	Log_GroupEnd( group );

	// Make sure to reset this
	if ( useServer )
	{
		Game_SetClient( true );
	}
}


// -------------------------------------------------------------


bool EntitySystem::Init()
{
	// Initialize the queue with all possible entity IDs
	// for ( Entity entity = 0; entity < CH_MAX_ENTITIES; ++entity )
	// 	aEntityPool.push( entity );

	aEntityPool.resize( CH_MAX_ENTITIES );
	for ( Entity entity = CH_MAX_ENTITIES - 1, index = 0; entity > 0; --entity, ++index )
		aEntityPool[ index ] = entity;

	CreateComponentPools();

	return true;
}


void EntitySystem::Shutdown()
{
}


void EntitySystem::CreateComponentPools()
{
	// i can't create component pools without passing template in ffs
	// Log_Error( " *** CREATE ENTITY COMPONENT POOLS *** \n" );
#if COMPONENT_POOLS_TEMP
	
	// iterate through all registered components and create a component pool for them
	// NOTE: this will not account for components registered later on
	for ( auto& [ name, componentData ] : gEntComponentRegistry.aComponentNames )
	{
		EntityComponentPool* pool  = new EntityComponentPool( name.data() );
		aComponentPools[ name ] = pool;
	}
#endif
}


Entity EntitySystem::CreateEntity()
{
	AssertMsg( aEntityCount < CH_MAX_ENTITIES, "Hit Entity Limit!" );

	// Take an ID from the front of the queue
	// Entity id = aEntityPool.front();
	Entity id = aEntityPool.back();
	// aEntityPool.pop();
	aEntityPool.pop_back();
	++aEntityCount;

	aUsedEntities.push_back( id );

	return id;
}


void EntitySystem::DeleteEntity( Entity ent )
{
	AssertMsg( ent < CH_MAX_ENTITIES, "Entity out of range" );

	// Invalidate the destroyed entity's signature
	// aSignatures[ ent ].reset();

	// Put the destroyed ID at the back of the queue
	// aEntityPool.push( ent );
	// aEntityPool.push_back( ent );
	aEntityPool.insert( aEntityPool.begin(), ent );
	--aEntityCount;

	vec_remove( aUsedEntities, ent );

	// Tell each Component Pool that this entity was destroyed
	for ( auto& [ name, pool ] : aComponentPools )
	{
		pool->EntityDestroyed( ent );
	}

	// TODO: maybe have some IComponentSystem class that has it's own list of entities
	// so we can tell those that it was destroyed as well
}


Entity EntitySystem::GetEntityCount()
{
	return aEntityCount;
}


Entity EntitySystem::CreateEntityFromServer( Entity desiredId )
{
	AssertMsg( aEntityCount < CH_MAX_ENTITIES, "Hit Entity Limit!" );

	size_t index = vec_index( aEntityPool, desiredId );

	if ( index == SIZE_MAX )
	{
		Log_Error( "Entity ID from Server is already taken on Client\n" );
		return CH_ENT_INVALID;
	}

	vec_remove_index( aEntityPool, index );
	aUsedEntities.push_back( desiredId );
	++aEntityCount;

	return desiredId;
}


bool EntitySystem::EntityExists( Entity desiredId )
{
	size_t index = vec_index( aEntityPool, desiredId );

	// if the entity is not in the pool, it exists
	return ( index == SIZE_MAX );
}


// Read and write from the network
void EntitySystem::ReadEntityUpdates( capnp::MessageReader& srReader )
{
	// NetEntityUpdates
	// NetEntityUpdate

	// const char* spComponentName = componentRead.getName().cStr();
	// void*       componentData   = nullptr;
	// 
	// // if ( componentRead.getState() == NetMsgEntityUpdate::EState::DESTROYED )
	// // {
	// // 	GetEntitySystem()->RemoveComponent( entity, spComponentName );
	// // 	continue;
	// // }
	// 
	// componentData               = GetEntitySystem()->GetComponent( entity, spComponentName );
	// 
	// // else if ( componentRead.getState() == NetMsgEntityUpdate::EState::CREATED )
	// if ( !componentData )
	// {
	// 	// Create the component
	// 	componentData = GetEntitySystem()->AddComponent( entity, spComponentName );
	// 
	// 	if ( componentData == nullptr )
	// 	{
	// 		Log_ErrorF( "Failed to create component\n" );
	// 		continue;
	// 	}
	// }
	// 
	// capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)data.data(), data.size() ) );
	// NetMsgServerInfo::Reader      serverInfoMsg = reader.getRoot< NetMsgServerInfo >();
	// 
	// capnp::MallocMessageBuilder   compMessageBuilder;
	// regData->apWrite( compMessageBuilder, data );
	// auto array        = capnp::messageToFlatArray( compMessageBuilder );
	// 
	// auto valueBuilder = compBuilder.initValues( array.size() * sizeof( capnp::word ) );
	// // std::copy( array.begin(), array.end(), valueBuilder.begin() );
	// memcpy( valueBuilder.begin(), array.begin(), array.size() * sizeof( capnp::word ) );
	// 
	// componentRead.getValues();
}


// TODO: probably rethink this so it's a list of components on their own, not a list of entities to update everything on
// though we should have some sort of system to detect whether it's dirty or not
void EntitySystem::WriteEntityUpdates( capnp::MessageBuilder& srBuilder )
{
	Assert( aEntityCount == aUsedEntities.size() );

	auto root       = srBuilder.initRoot< NetMsgEntityUpdates >();
	auto updateList = root.initUpdateList( aEntityCount );

	for ( size_t i = 0; i < aEntityCount; i++ )
	{
		Entity entity = aUsedEntities[ i ];
		auto   update = updateList[ i ];

		update.setId( entity );

		// this is the worst thing ever
		std::vector< IEntityComponentPool* > pools;

		// Find all component pools that contain this Entity
		// That means the Entity has the type of component that pool is for
		for ( auto& [ name, pool ] : aComponentPools )
		{
			if ( pool->Contains( entity ) )
				pools.push_back( pool );
		}

		// Now init components with the correct size
		auto componentsList = update.initComponents( pools.size() );

		for ( size_t i = 0; i < pools.size(); i++ )
		{
			auto compBuilder = componentsList[ i ];
			auto pool        = pools[ i ];

			auto data        = pool->GetData( entity );
			auto regData     = pool->GetRegistryData();

			compBuilder.setName( regData->apName );

			if ( !regData->apWrite )
			{
				compBuilder.initValues( 0 );
				continue;
			}

			capnp::MallocMessageBuilder compMessageBuilder;
			regData->apWrite( compMessageBuilder, data );
			auto array = capnp::messageToFlatArray( compMessageBuilder );

			auto valueBuilder = compBuilder.initValues( array.size() * sizeof( capnp::word ) );
			// std::copy( array.begin(), array.end(), valueBuilder.begin() );
			memcpy( valueBuilder.begin(), array.begin(), array.size() * sizeof( capnp::word ) );
		}
	}
}


void EntitySystem::ReadComponents( Entity sEnt, capnp::MessageReader& srReader )
{
}


void EntitySystem::WriteComponents( Entity sEnt, capnp::MessageBuilder& srBuilder )
{
}


// void EntitySystem::MarkComponentNetworked( Entity ent, const char* spName )
// {
// }


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


// Does this entity have this component?
bool EntitySystem::HasComponent( Entity entity, const char* spName )
{
	return GetComponent( entity, spName ) != nullptr;
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


// Remove a component from an entity
void EntitySystem::RemoveComponent( Entity entity, const char* spName )
{
	auto pool = GetComponentPool( spName );

	if ( pool == nullptr )
	{
		Log_FatalF( "Failed to remove component - no component pool found: \"%s\"\n", spName );
		return;
	}

	pool->Remove( entity );
}


// Sets Prediction on this component
void EntitySystem::SetComponentPredicted( Entity entity, const char* spName, bool sPredicted )
{
	if ( this == sv_entities )
	{
		// The server does not need to know if it's predicted, the client will have special handling for this
		Log_ErrorF( "Tried to mark entity component as predicted on server - \"%s\"\n", spName );
		return;
	}

	auto pool = GetComponentPool( spName );

	if ( pool == nullptr )
	{
		Log_FatalF( "Failed to set component prediction - no component pool found: \"%s\"\n", spName );
		return;
	}

	pool->SetPredicted( entity, sPredicted );
}


// Is this component predicted for this Entity?
bool EntitySystem::IsComponentPredicted( Entity entity, const char* spName )
{
	if ( this == sv_entities )
	{
		// The server does not need to know if it's predicted, the client will have special handling for this
		Log_ErrorF( "Tried to find a predicted entity component on server - \"%s\"\n", spName );
		return false;
	}

	auto pool = GetComponentPool( spName );

	if ( pool == nullptr )
	{
		Log_FatalF( "Failed to get component prediction - no component pool found: \"%s\"\n", spName );
		return false;
	}

	return pool->IsPredicted( entity );
}


IEntityComponentPool* EntitySystem::GetComponentPool( const char* spName )
{
	auto it = aComponentPools.find( spName );

	if ( it == aComponentPools.end() )
		Log_FatalF( "Component not registered before use: \"%s\"\n", spName );

	return it->second;
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
	Transform* spTransform = static_cast< Transform* >( spData );
	auto       message     = srReader.getRoot< NetCompTransform >();

	NetHelper_ReadVec3( message.getPos(), spTransform->aPos );
	NetHelper_ReadVec3( message.getAng(), spTransform->aAng );
	NetHelper_ReadVec3( message.getScale(), spTransform->aScale );
}

void TEMP_TransformWrite( capnp::MessageBuilder& srMessage, const void* spData )
{
	const Transform* spTransform = static_cast< const Transform* >( spData );
	auto             builder     = srMessage.initRoot< NetCompTransform >();

	auto             pos         = builder.initPos();
	NetHelper_WriteVec3( &pos, spTransform->aPos );

	auto ang = builder.initAng();
	NetHelper_WriteVec3( &ang, spTransform->aAng );

	auto scale = builder.initScale();
	NetHelper_WriteVec3( &scale, spTransform->aScale );
}


CH_COMPONENT_READ_DEF( TransformSmall )
{
	TransformSmall* spTransform = static_cast< TransformSmall* >( spData );
	auto            message     = srReader.getRoot< NetCompTransformSmall >();

	NetHelper_ReadVec3( message.getPos(), spTransform->aPos );
	NetHelper_ReadVec3( message.getAng(), spTransform->aAng );
}

CH_COMPONENT_WRITE_DEF( TransformSmall )
{
	const TransformSmall* spTransform = static_cast< const TransformSmall* >( spData );
	auto                  builder     = srMessage.initRoot< NetCompTransformSmall >();

	// NetHelper_WriteVec3( &builder.initPos(), spTransform->aPos );
	// NetHelper_WriteVec3( &builder.initAng(), spTransform->aAng );
}


CH_COMPONENT_READ_DEF( CRigidBody )
{
	CRigidBody* spRigidBody = static_cast< CRigidBody* >( spData );
	auto        message     = srReader.getRoot< NetCompRigidBody >();

	NetHelper_ReadVec3( message.getVel(), spRigidBody->aVel );
	NetHelper_ReadVec3( message.getAccel(), spRigidBody->aAccel );
}

CH_COMPONENT_WRITE_DEF( CRigidBody )
{
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
	CDirection* spDirection = static_cast< CDirection* >( spData );
	auto        message     = srReader.getRoot< NetCompDirection >();

	NetComp_ReadDirection( message, *spDirection );
}

CH_COMPONENT_WRITE_DEF( CDirection )
{
	const CDirection* spDirection = static_cast< const CDirection* >( spData );
	auto              builder     = srMessage.initRoot< NetCompDirection >();

	NetComp_WriteDirection( &builder, *spDirection );
}


CH_COMPONENT_READ_DEF( CCamera )
{
	CCamera* spCamera = static_cast< CCamera* >( spData );
	auto     message  = srReader.getRoot< NetCompCamera >();

	NetComp_ReadDirection( message.getDirection(), *spCamera );

	spCamera->aFov = message.getFov();

	NetHelper_ReadVec3( message.getTransform().getPos(), spCamera->aTransform.aPos );
	NetHelper_ReadVec3( message.getTransform().getAng(), spCamera->aTransform.aAng );
}

CH_COMPONENT_WRITE_DEF( CCamera )
{
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
	CGravity* spGravity = static_cast< CGravity* >( spData );
	auto      message  = srReader.getRoot< NetCompGravity >();

	NetHelper_ReadVec3( message.getForce(), spGravity->aForce );
}

CH_COMPONENT_WRITE_DEF( CGravity )
{
	const CGravity* spGravity = static_cast< const CGravity* >( spData );
	auto            builder   = srMessage.initRoot< NetCompGravity >();

	// NetHelper_WriteVec3( &builder.initForce(), spGravity->aForce );
}


CH_COMPONENT_READ_DEF( CModelPath )
{
	auto* spModelPath = static_cast< CModelPath* >( spData );
	auto  message     = srReader.getRoot< NetCompModelPath >();

	spModelPath->aPath = message.getPath();
}

CH_COMPONENT_WRITE_DEF( CModelPath )
{
	const auto* spModelPath = static_cast< const CModelPath* >( spData );
	auto        builder     = srMessage.initRoot< NetCompModelPath >();

	builder.setPath( spModelPath->aPath );
}


CH_COMPONENT_READ_DEF( CPlayerMoveData )
{
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
	auto* spLight = static_cast< Light_t* >( spData );
	auto  message = srReader.getRoot< NetCompLight >();

	NetHelper_ReadVec4( message.getColor(), spLight->aColor );
	NetHelper_ReadVec3( message.getPos(), spLight->aPos );
	NetHelper_ReadVec3( message.getAng(), spLight->aAng );

	spLight->aType     = static_cast< ELightType >( message.getType() );
	spLight->aInnerFov = message.getInnerFov();
	spLight->aOuterFov = message.getOuterFov();
	spLight->aRadius   = message.getRadius();
	spLight->aLength   = message.getLength();

	spLight->aShadow   = message.getShadow();
	spLight->aEnabled  = message.getEnabled();

	//ADJIAWDI)JDAIW)D 
	Graphics_UpdateLight( spLight );
}


CH_COMPONENT_WRITE_DEF( Light_t )
{
	auto* spLight = static_cast< const Light_t* >( spData );
	auto  builder = srMessage.initRoot< NetCompLight >();

	auto  color   = builder.initColor();
	NetHelper_WriteVec4( &color, spLight->aColor );

	auto pos = builder.initPos();
	NetHelper_WriteVec3( &pos, spLight->aPos );

	auto ang = builder.initAng();
	NetHelper_WriteVec3( &ang, spLight->aAng );

	builder.setType( spLight->aType );
	builder.setInnerFov( spLight->aInnerFov );
	builder.setOuterFov( spLight->aOuterFov );
	builder.setRadius( spLight->aRadius );
	builder.setLength( spLight->aLength );

	builder.setShadow( spLight->aShadow );
	builder.setEnabled( spLight->aEnabled );
}


void Ent_RegisterBaseComponents()
{
	// Setup Types
	gEntComponentRegistry.aVarTypes[ typeid( bool ).hash_code() ]      = EEntComponentVarType_Bool;
	gEntComponentRegistry.aVarTypes[ typeid( float ).hash_code() ]     = EEntComponentVarType_Float;
	gEntComponentRegistry.aVarTypes[ typeid( double ).hash_code() ]    = EEntComponentVarType_Double;

	gEntComponentRegistry.aVarTypes[ typeid( s8 ).hash_code() ]        = EEntComponentVarType_S8;
	gEntComponentRegistry.aVarTypes[ typeid( s16 ).hash_code() ]       = EEntComponentVarType_S16;
	gEntComponentRegistry.aVarTypes[ typeid( s32 ).hash_code() ]       = EEntComponentVarType_S32;
	gEntComponentRegistry.aVarTypes[ typeid( s64 ).hash_code() ]       = EEntComponentVarType_S64;

	gEntComponentRegistry.aVarTypes[ typeid( u8 ).hash_code() ]        = EEntComponentVarType_U8;
	gEntComponentRegistry.aVarTypes[ typeid( u16 ).hash_code() ]       = EEntComponentVarType_U16;
	gEntComponentRegistry.aVarTypes[ typeid( u32 ).hash_code() ]       = EEntComponentVarType_U32;
	gEntComponentRegistry.aVarTypes[ typeid( u64 ).hash_code() ]       = EEntComponentVarType_U64;

	gEntComponentRegistry.aVarTypes[ typeid( glm::vec2 ).hash_code() ] = EEntComponentVarType_Vec2;
	gEntComponentRegistry.aVarTypes[ typeid( glm::vec3 ).hash_code() ] = EEntComponentVarType_Vec3;
	gEntComponentRegistry.aVarTypes[ typeid( glm::vec4 ).hash_code() ] = EEntComponentVarType_Vec4;

	// Now Register Base Components
	EntComp_RegisterComponent< Transform >( "transform", true, EEntComponentNetType_Both,
		[ & ]() { return new Transform; }, [ & ]( void* spData ) { delete (Transform*)spData; } );

	EntComp_RegisterComponentVar< Transform, glm::vec3 >( "aPos", "pos", offsetof( Transform, aPos ) );
	EntComp_RegisterComponentVar< Transform, glm::vec3 >( "aAng", "ang", offsetof( Transform, aAng ) );
	EntComp_RegisterComponentVar< Transform, glm::vec3 >( "aScale", "scale", offsetof( Transform, aScale ) );
	EntComp_RegisterComponentReadWrite< Transform >( TEMP_TransformRead, TEMP_TransformWrite );

	CH_REGISTER_COMPONENT_RW( TransformSmall, transformSmall, true );
	CH_REGISTER_COMPONENT_VAR( TransformSmall, glm::vec3, aPos, pos );
	CH_REGISTER_COMPONENT_VAR( TransformSmall, glm::vec3, aAng, ang );

	CH_REGISTER_COMPONENT_RW( CRigidBody, rigidBody, true );
	CH_REGISTER_COMPONENT_VAR( CRigidBody, glm::vec3, aVel, vel );
	CH_REGISTER_COMPONENT_VAR( CRigidBody, glm::vec3, aAccel, accel );

	CH_REGISTER_COMPONENT_RW( CDirection, direction, true );
	CH_REGISTER_COMPONENT_VAR( CDirection, glm::vec3, aForward, forward );
	CH_REGISTER_COMPONENT_VAR( CDirection, glm::vec3, aUp, up );
	// CH_REGISTER_COMPONENT_VAR( CDirection, glm::vec3, aRight, right );
	CH_REGISTER_COMP_VAR_VEC3( CDirection, aRight, right );

	CH_REGISTER_COMPONENT_RW( CGravity, gravity, true );
	CH_REGISTER_COMP_VAR_VEC3( CGravity, aForce, force );

	// might be a bit weird
	// HACK HACK: DONT OVERRIDE CLIENT VALUE, IT WILL NEVER BE UPDATED
	CH_REGISTER_COMPONENT_RW( CCamera, camera, false );
	CH_REGISTER_COMPONENT_VAR( CCamera, float, aFov, fov );
	CH_REGISTER_COMPONENT_VAR( CCamera, glm::vec3, aForward, forward );
	CH_REGISTER_COMPONENT_VAR( CCamera, glm::vec3, aUp, up );
	CH_REGISTER_COMPONENT_VAR( CCamera, glm::vec3, aRight, right );
	EntComp_RegisterComponentVar< CCamera, glm::vec3 >( "aPos", "pos", offsetof( CCamera, aTransform.aPos ) );
	EntComp_RegisterComponentVar< CCamera, glm::vec3 >( "aAng", "ang", offsetof( CCamera, aTransform.aAng ) );

	CH_REGISTER_COMPONENT_RW( CModelPath, modelPath, true );
	// CH_REGISTER_COMPONENT_VAR( CModelPath, std::string, aPath, path );

	// Probably should be in graphics?
	CH_REGISTER_COMPONENT_RW( Light_t, light, true );
	CH_REGISTER_COMPONENT_VAR( Light_t, ELightType, aType, type );
	CH_REGISTER_COMPONENT_VAR( Light_t, glm::vec4, aColor, color );

	// TODO: these 2 should not be here
    // it should be attached to it's own entity that can be parented
    // and that entity needs to contain the transform (or transform small) component
	CH_REGISTER_COMPONENT_VAR( Light_t, glm::vec3, aPos, pos );
	CH_REGISTER_COMPONENT_VAR( Light_t, glm::vec3, aAng, ang );

	CH_REGISTER_COMPONENT_VAR( Light_t, float, aInnerFov, innerFov );
	CH_REGISTER_COMPONENT_VAR( Light_t, float, aOuterFov, outerFov );
	CH_REGISTER_COMPONENT_VAR( Light_t, float, aRadius, radius );
	CH_REGISTER_COMPONENT_VAR( Light_t, float, aLength, length );
	CH_REGISTER_COMPONENT_VAR( Light_t, bool, aShadow, shadow );
	CH_REGISTER_COMPONENT_VAR( Light_t, bool, aEnabled, enabled );

	// TODO: SHOULD NOT BE HERE !!!!!
	CH_REGISTER_COMPONENT_RW( CPlayerMoveData, playerMoveData, true );
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