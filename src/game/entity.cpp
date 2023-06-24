#include "main.h"
#include "game_shared.h"
#include "entity.h"
#include "world.h"
#include "util.h"
#include "player.h"  // TEMP - for CPlayerMoveData

#include "ent_light.h"
#include "mapmanager.h"
#include "igui.h"

#include "graphics/graphics.h"

#include "game_physics.h"  // just for IPhysicsShape* and IPhysicsObject*


LOG_REGISTER_CHANNEL2( Entity, LogColor::White );


EntComponentRegistry_t gEntComponentRegistry;

EntitySystem*          cl_entities                                     = nullptr;
EntitySystem*          sv_entities                                     = nullptr;

extern Entity          gLocalPlayer;

CONVAR( ent_always_full_update, 0, "For debugging, always send a full update" );
CONVAR( ent_show_component_net_updates, 0, "Show Component Network Updates" );


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


static const char* gEntVarTypeStr[] = {
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

	"std::string",

	"glm::vec2",
	"glm::vec3",
	"glm::vec4",
};


static_assert( ARR_SIZE( gEntVarTypeStr ) == EEntComponentVarType_Count );


const char* EntComp_VarTypeToStr( EEntComponentVarType sVarType )
{
	if ( sVarType < 0 || sVarType > EEntComponentVarType_Count )
		return gEntVarTypeStr[ EEntComponentVarType_Invalid ];

	return gEntVarTypeStr[ sVarType ];
}


size_t EntComp_GetVarDirtyOffset( char* spData, EEntComponentVarType sVarType )
{
	size_t offset = 0;
	switch ( sVarType )
	{
		default:
		case EEntComponentVarType_Invalid:
			return 0;

		case EEntComponentVarType_Bool:
			offset = sizeof( bool );
			break;

		case EEntComponentVarType_Float:
			offset = sizeof( float );
			break;

		case EEntComponentVarType_Double:
			offset = sizeof( double );
			break;


		case EEntComponentVarType_S8:
			offset = sizeof( s8 );
			break;

		case EEntComponentVarType_S16:
			offset = sizeof( s16 );
			break;

		case EEntComponentVarType_S32:
			offset = sizeof( s32 );
			break;

		case EEntComponentVarType_S64:
			offset = sizeof( s64 );
			break;


		case EEntComponentVarType_U8:
			offset = sizeof( u8 );
			break;

		case EEntComponentVarType_U16:
			offset = sizeof( u16 );
			break;

		case EEntComponentVarType_U32:
		{
			offset = sizeof( u32 );
			break;
		}
		case EEntComponentVarType_U64:
			offset = sizeof( u64 );
			break;


		case EEntComponentVarType_StdString:
			offset = sizeof( std::string );
			break;

		case EEntComponentVarType_Vec2:
			offset = sizeof( glm::vec2 );
			break;

		case EEntComponentVarType_Vec3:
			offset = sizeof( glm::vec3 );
			break;

		case EEntComponentVarType_Vec4:
			offset = sizeof( glm::vec4 );
			break;
	}

	return offset;
}


void EntComp_ResetVarDirty( char* spData, EEntComponentVarType sVarType )
{
	size_t offset = EntComp_GetVarDirtyOffset( spData, sVarType );

	if ( offset )
	{
		bool* aIsDirty = reinterpret_cast< bool* >( spData + offset );
		*aIsDirty      = false;
	}
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
			s64 value = *static_cast< s64* >( spData );
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

		case EEntComponentVarType_StdString:
		{
			return *(const std::string*)spData;
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


void EntComp_AddRegisterCallback( EntitySystem* spSystem )
{
	AssertMsg( spSystem, "Trying to register nullptr for a Entity Component Register Callback\n" );

	if ( !spSystem )
	{
		Log_Warn( gLC_Entity, "Trying to register nullptr for a Entity Component Register Callback\n" );
		return;
	}

	gEntComponentRegistry.aCallbacks.push_back( spSystem );
}


void EntComp_RemoveRegisterCallback( EntitySystem* spSystem )
{
	size_t index = vec_index( gEntComponentRegistry.aCallbacks, spSystem );
	if ( index != SIZE_MAX )
	{
		gEntComponentRegistry.aCallbacks.erase( gEntComponentRegistry.aCallbacks.begin() + index );
	}
	else
	{
		Log_Warn( gLC_Entity, "Trying to remove component register callback that was isn't registered\n" );
	}
}


void EntComp_RunRegisterCallbacks( const char* spName )
{
	for ( auto system : gEntComponentRegistry.aCallbacks )
	{
		system->CreateComponentPool( spName );
	}
}


// ===================================================================================
// Entity Component Pool
// ===================================================================================


EntityComponentPool::EntityComponentPool()
{
	aCount = 0;
}


EntityComponentPool::~EntityComponentPool()
{
	while ( aMapEntityToComponent.size() )
	{
		auto it = aMapEntityToComponent.begin();
		Remove( it->first );
	}
}


bool EntityComponentPool::Init( const char* spName )
{
	apName  = spName;

	// Get Creation and Free functions
	auto it = gEntComponentRegistry.aComponentNames.find( apName );

	if ( it == gEntComponentRegistry.aComponentNames.end() )
	{
		Log_FatalF( gLC_Entity, "Component not registered: \"%s\"\n", apName );
		return false;
	}

	Assert( it->second->aFuncNew );
	Assert( it->second->aFuncFree );

	aFuncNew  = it->second->aFuncNew;
	aFuncFree = it->second->aFuncFree;

	apData    = it->second;

	return true;
}


// Get Component Registry Data
EntComponentData_t* EntityComponentPool::GetRegistryData()
{
	return gEntComponentRegistry.aComponentNames[ apName ];
}


// Does this pool contain a component for this entity?
bool EntityComponentPool::Contains( Entity entity )
{
	auto it = aMapEntityToComponent.find( entity );
	return ( it != aMapEntityToComponent.end() );
}


void EntityComponentPool::EntityDestroyed( Entity entity )
{
	auto it = aMapEntityToComponent.find( entity );

	if ( it != aMapEntityToComponent.end() )
	{
		Remove( entity );
	}
}


// Adds This component to the entity
void* EntityComponentPool::Create( Entity entity )
{
	// Is this a client or server component pool?
	// Make sure this component can be created on it

	// Check if the component already exists
	auto it = aMapEntityToComponent.find( entity );

	if ( it != aMapEntityToComponent.end() )
	{
		Log_ErrorF( gLC_Entity, "Component already exists on entity - \"%s\"\n", apName );
		return aComponents[ it->second ];
	}

	aMapComponentToEntity[ aCount ] = entity;
	aMapEntityToComponent[ entity ] = aCount;

	void* data                      = aFuncNew();

	aComponentStates[ aCount ]      = EEntityCreateState_Created;

	aComponents[ aCount++ ]         = data;

	// Add it to systems
	// for ( auto system : aComponentSystems )
	if ( apComponentSystem )
	{
		apComponentSystem->aEntities.push_back( entity );
		apComponentSystem->ComponentAdded( entity, data );
	}

	// TODO: use malloc and use that pointer in the constructor for this
	// use placement new
	// https://stackoverflow.com/questions/2494471/c-is-it-possible-to-call-a-constructor-directly-without-new

	return data;
}


// Removes this component from the entity
void EntityComponentPool::Remove( Entity entity )
{
	auto it = aMapEntityToComponent.find( entity );

	if ( it == aMapEntityToComponent.end() )
	{
		Log_ErrorF( gLC_Entity, "Failed to remove component from entity - \"%s\"\n", apName );
		return;
	}

	size_t index = it->second;
	void* data = aComponents[ index ];
	Assert( data );

	// Remove it from systems
	// for ( auto system : aComponentSystems )
	if ( apComponentSystem )
	{
		apComponentSystem->ComponentRemoved( entity, data );
		vec_remove( apComponentSystem->aEntities, entity );
	}

	aMapComponentToEntity.erase( index );
	aMapEntityToComponent.erase( it );

	aComponentStates.erase( index );

	aFuncFree( data );

	aCount--;
}


// Removes this component by index
void EntityComponentPool::RemoveByIndex( size_t sIndex )
{
	auto it = aMapComponentToEntity.find( sIndex );

	if ( it == aMapComponentToEntity.end() )
	{
		Log_ErrorF( gLC_Entity, "Failed to remove component from entity - \"%s\"\n", apName );
		return;
	}

	Entity entity = it->second;

	void*  data   = aComponents[ sIndex ];
	Assert( data );

	// Remove it from the system
	if ( apComponentSystem )
	{
		apComponentSystem->ComponentRemoved( entity, data );
		vec_remove( apComponentSystem->aEntities, entity );
	}

	aMapComponentToEntity.erase( it );
	aMapEntityToComponent.erase( entity );

	aComponentStates.erase( sIndex );

	aFuncFree( data );

	aCount--;
}


// Removes this component from the entity later
void EntityComponentPool::RemoveQueued( Entity entity )
{
	auto it = aMapEntityToComponent.find( entity );

	if ( it == aMapEntityToComponent.end() )
	{
		Log_ErrorF( gLC_Entity, "Failed to remove component from entity - \"%s\"\n", apName );
		return;
	}

	size_t index = it->second;

	// Mark Component as Destroyed
	aComponentStates[ index ] = EEntityCreateState_Destroyed;
}


// Removes components queued for deletion
void EntityComponentPool::RemoveAllQueued()
{
	std::vector< size_t > toRemove;

	// for ( auto& [ index, state ] : aComponentStates )
	for ( auto it = aComponentStates.begin(); it != aComponentStates.end(); it++ )
	{
		if ( it->second == EEntityCreateState_Destroyed )
		{
			toRemove.push_back( it->first );
			continue;
		}

		if ( it->second == EEntityCreateState_Created )
		{
			it->second = EEntityCreateState_None;
		}
	}

	for ( size_t entity : toRemove )
	{
		RemoveByIndex( entity );
	}
}


// Gets the data for this component
void* EntityComponentPool::GetData( Entity entity )
{
	auto it = aMapEntityToComponent.find( entity );

	if ( it != aMapEntityToComponent.end() )
	{
		size_t index = it->second;
		// return &aComponents[ index ];
		return aComponents[ index ];
	}

	return nullptr;
}


// Marks this component as predicted
void EntityComponentPool::SetPredicted( Entity entity, bool sPredicted )
{
	auto it = aMapEntityToComponent.find( entity );

	if ( it == aMapEntityToComponent.end() )
	{
		Log_ErrorF( gLC_Entity, "Failed to mark Entity Component as predicted, Entity does not have this component - \"%s\"\n", apName );
		return;
	}

	if ( sPredicted )
	{
		// We want this component predicted, add it to the prediction set
		aPredicted.emplace( entity );
	}
	else
	{
		// We don't want this component predicted, remove it from the prediction set if it exists
		auto find = aPredicted.find( entity );
		if ( find != aPredicted.end() )
			aPredicted.erase( find );
	}
}


bool EntityComponentPool::IsPredicted( Entity entity )
{
	// If the entity is in this set, then this component is predicted
	auto it = aPredicted.find( entity );
	return ( it != aPredicted.end() );
}


// How Many Components are in this Pool?
size_t EntityComponentPool::GetCount()
{
	return aCount;
}


// ===================================================================================
// Entity System
// ===================================================================================


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

	cl_entities            = new EntitySystem;
	cl_entities->aIsClient = true;

	return cl_entities->Init();
}


bool EntitySystem::CreateServer()
{
	Assert( !sv_entities );
	if ( sv_entities )
		return false;

	sv_entities            = new EntitySystem;
	cl_entities->aIsClient = false;

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


bool EntitySystem::Init()
{
	aEntityCount = 0;
	aUsedEntities.clear();
	aComponentPools.clear();

	// Initialize the queue with all possible entity IDs
	// for ( Entity entity = 0; entity < CH_MAX_ENTITIES; ++entity )
	// 	aEntityPool.push( entity );

	aEntityPool.resize( CH_MAX_ENTITIES );
	for ( Entity entity = CH_MAX_ENTITIES - 1, index = 0; entity > 0; --entity, ++index )
		aEntityPool[ index ] = entity;

	CreateComponentPools();

	// Add callback
	EntComp_AddRegisterCallback( this );

	return true;
}


void EntitySystem::Shutdown()
{
	// Remove callback
	EntComp_RemoveRegisterCallback( this );

	// Tell all component pools every entity was destroyed
	for ( Entity entity : aUsedEntities )
	{
		for ( auto& [ name, pool ] : aComponentPools )
		{
			pool->EntityDestroyed( entity );
		}
	}

	// Free component pools
	for ( auto& [ name, pool ] : aComponentPools )
	{
		if ( pool->apComponentSystem )
		{
			aComponentSystems.erase( typeid( pool->apComponentSystem ).hash_code() );
			delete pool->apComponentSystem;
			pool->apComponentSystem = nullptr;
		}

		delete pool;
	}

	aEntityCount = 0;
	aUsedEntities.clear();
	aComponentPools.clear();
}


void EntitySystem::UpdateSystems()
{
	for ( auto& [ name, pool ] : aComponentPools )
	{
		if ( pool->apComponentSystem )
			pool->apComponentSystem->Update();
	}
}


void EntitySystem::UpdateStates()
{
	// Remove Components Queued for Deletion
	for ( auto& [ name, pool ] : aComponentPools )
	{
		pool->RemoveAllQueued();
	}

	DeleteQueuedEntities();

	for ( auto& [ id, state ] : aEntityStates )
	{
		if ( state == EEntityCreateState_Created )
			aEntityStates[ id ] = EEntityCreateState_None;
	}
}


void EntitySystem::CreateComponentPools()
{
	// iterate through all registered components and create a component pool for them
	for ( auto& [ name, componentData ] : gEntComponentRegistry.aComponentNames )
	{
		CreateComponentPool( name.data() );
	}
}


void EntitySystem::CreateComponentPool( const char* spName )
{
	EntityComponentPool* pool = new EntityComponentPool;

	if ( !pool->Init( spName ) )
	{
		Log_ErrorF( gLC_Entity, "Failed to create component pool for \"%s\"", spName );
		delete pool;
		return;
	}
		
	aComponentPools[ spName ] = pool;

	// Create component system if it has one registered for it
	if ( pool->apData->aFuncNewSystem )
	{
		pool->apComponentSystem                                            = pool->apData->aFuncNewSystem();
		aComponentSystems[ typeid( pool->apComponentSystem ).hash_code() ] = pool->apComponentSystem;
	}
}


IEntityComponentSystem* EntitySystem::GetComponentSystem( const char* spName )
{
	EntityComponentPool* pool = GetComponentPool( spName );

	if ( pool == nullptr )
	{
		Log_ErrorF( gLC_Entity, "Failed to get component system - no component pool found: \"%s\"\n", spName );
		return nullptr;
	}

	return pool->apComponentSystem;
}


Entity EntitySystem::CreateEntity( bool sLocal )
{
	AssertMsg( aEntityCount < CH_MAX_ENTITIES, "Hit Entity Limit!" );

	// Take an ID from the front of the queue
	// Entity id = aEntityPool.front();
	Entity id = aEntityPool.back();
	// aEntityPool.pop();
	aEntityPool.pop_back();
	++aEntityCount;

	aUsedEntities.push_back( id );

	aEntityStates[ id ] = EEntityCreateState_Created;

	return id;
}


void EntitySystem::DeleteEntity( Entity ent )
{
	AssertMsg( ent < CH_MAX_ENTITIES, "Entity out of range" );

	aDeleteEntities.emplace( ent );
	aEntityStates[ ent ] = EEntityCreateState_Destroyed;
}


void EntitySystem::DeleteQueuedEntities()
{
	for ( Entity entity : aDeleteEntities )
	{
		// Invalidate the destroyed entity's signature
		// aSignatures[ ent ].reset();

		// Put the destroyed ID at the back of the queue
		// aEntityPool.push( ent );
		// aEntityPool.push_back( ent );
		aEntityPool.insert( aEntityPool.begin(), entity );
		--aEntityCount;

		vec_remove( aUsedEntities, entity );

		// Tell each Component Pool that this entity was destroyed
		for ( auto& [ name, pool ] : aComponentPools )
		{
			pool->EntityDestroyed( entity );
		}

		aEntityStates.erase( entity );
	}

	aDeleteEntities.clear();
}


Entity EntitySystem::GetEntityCount()
{
	return aEntityCount;
}


//Entity EntitySystem::CreateEntityFromServer( Entity desiredId )
//{
//	AssertMsg( aEntityCount < CH_MAX_ENTITIES, "Hit Entity Limit!" );
//
//	size_t index = vec_index( aEntityPool, desiredId );
//
//	if ( index == SIZE_MAX )
//	{
//		Log_Error( gLC_Entity, "Entity ID from Server is already taken on Client\n" );
//		return CH_ENT_INVALID;
//	}
//
//	vec_remove_index( aEntityPool, index );
//	aUsedEntities.push_back( desiredId );
//	++aEntityCount;
//
//	return desiredId;
//}


bool EntitySystem::EntityExists( Entity desiredId )
{
	size_t index = vec_index( aEntityPool, desiredId );

	// if the entity is not in the pool, it exists
	return ( index == SIZE_MAX );
}


void EntitySystem::ParentEntity( Entity sSelf, Entity sParent )
{
	if ( sSelf == sParent )
		return;

	if ( sParent == CH_ENT_INVALID )
	{
		// Clear the parent
		aEntityParents.erase( sSelf );
	}
	else
	{
		aEntityParents[ sSelf ] = sParent;
	}
}


Entity EntitySystem::GetParent( Entity sSelf )
{
	auto it = aEntityParents.find( sSelf );

	if ( it != aEntityParents.end() )
		return it->second;

	return CH_ENT_INVALID;
}


// Returns a Model Matrix with parents applied in world space IF we have a transform component
bool EntitySystem::GetWorldMatrix( glm::mat4& srMat, Entity sEntity )
{
	// Check if we have a transform component first
	auto transform = Ent_GetComponent< CTransform >( sEntity, "transform" );

	if ( !transform )
	{
		srMat = glm::mat4( 1.f );
		return false;
	}

	Entity    parent = GetParent( sEntity );
	glm::mat4 parentMat( 1.f );

	if ( parent )
	{
		// Get the world matrix recursively
		GetWorldMatrix( parentMat, parent );
	}

	// NOTE: THIS IS PROBABLY WRONG
	srMat = glm::translate( transform->aPos.Get() );

	srMat *= glm::eulerAngleYZX(
	  glm::radians( transform->aAng.Get().x ),
	  glm::radians( transform->aAng.Get().y ),
	  glm::radians( transform->aAng.Get().z ) );

	srMat = glm::scale( srMat, transform->aScale.Get() );
	srMat = parentMat * srMat;

	return true;
}


Transform EntitySystem::GetWorldTransform( Entity sEntity )
{
	Transform final{};

	glm::mat4 matrix;
	if ( !GetWorldMatrix( matrix, sEntity ) )
		return final;

	final.aPos   = Util_GetMatrixPosition( matrix );
	final.aAng   = Util_GetMatrixAngles( matrix );
	final.aScale = Util_GetMatrixScale( matrix );

	return final;
}


// Read and write from the network
void EntitySystem::ReadEntityUpdates( const NetMsg_EntityUpdates* spMsg )
{
	auto entityUpdateList = spMsg->update_list();

	if ( !entityUpdateList )
		return;

	for ( size_t i = 0; i < entityUpdateList->size(); i++ )
	{
		const NetMsg_EntityUpdate* entityUpdate = entityUpdateList->Get( i );

		if ( !entityUpdate )
			continue;

		// NetMsg_EntityUpdate

		Entity entId  = entityUpdate->id();
		Entity entity = CH_ENT_INVALID;

		if ( entityUpdate->destroyed() )
		{
			GetEntitySystem()->DeleteEntity( entId );
			continue;
		}
		else
		{
			// Is this entity in the translation system?
			auto it = aEntityIDConvert.find( entId );

			// It's not, add it
			if ( it == aEntityIDConvert.end() )
			{
				entity = GetEntitySystem()->CreateEntity();

				if ( entity == CH_ENT_INVALID )
				{
					Log_Warn( gLC_Entity, "Failed to create client entity\n" );
					continue;
				}

				aEntityIDConvert[ entId ] = entity;
			}
			else if ( !GetEntitySystem()->EntityExists( it->second ) )
			{
				Log_Error( gLC_Entity, "wtf entity in translation list doesn't actually exist?\n" );
			}
		}
	}
}


// TODO: redo this by having it loop through component pools, and not entitys
// right now, it's doing a lot of entirely unnecessary checks
// we can avoid those if we loop through the pools instead
void EntitySystem::WriteEntityUpdates( flatbuffers::FlatBufferBuilder& srBuilder )
{
	Assert( aEntityCount == aUsedEntities.size() );

	std::vector< NetMsg_EntityUpdateBuilder > updateBuilderList;
	std::vector< flatbuffers::Offset< NetMsg_EntityUpdate > > updateOut;

	for ( size_t i = 0; i < aEntityCount; i++ )
	{
		Entity entity = aUsedEntities[ i ];
		auto&  update = updateBuilderList.emplace_back( srBuilder );

		update.add_id( entity );

		// Get Entity State
		if ( aEntityStates[ entity ] == EEntityCreateState_Destroyed )
			update.add_destroyed( true );

		updateOut.push_back( update.Finish() );
		// srBuilder.Finish( update.Finish() );
	}

	auto                        vec = srBuilder.CreateVector( updateOut );

	NetMsg_EntityUpdatesBuilder updatesBuilder( srBuilder );
	updatesBuilder.add_update_list( vec );

	srBuilder.Finish( updatesBuilder.Finish() );
}


// Read and write from the network
//void EntitySystem::ReadComponentUpdates( capnp::MessageReader& srReader )
//{
//}


#if USE_FLEXBUFFERS
void ReadComponent( flexb::Reference& spSrc, EntComponentData_t* spRegData, void* spData )
{
	// Get the vector i guess
	auto   vector    = spSrc.AsVector();
	size_t i         = 0;
	size_t curOffset = 0;

	for ( const auto& [ offset, var ] : spRegData->aVars )
	{
		if ( !var.aIsNetVar )
		{
			curOffset += var.aSize + offset;
			continue;
		}

		// size_t offset = EntComp_GetVarDirtyOffset( (char*)spData, var.aType );
		void* data = ( (char*)spData ) + offset;
		// curOffset += var.aSize + offset;

		// Check these first
		switch ( var.aType )
		{
			case EEntComponentVarType_Invalid:
				continue;

			case EEntComponentVarType_Bool:
			{
				bool* value = static_cast< bool* >( data );
				*value      = vector[ i++ ].AsBool();
				continue;
			}
		}

		// Now, check if we wrote data for this var
		bool wroteVar = vector[ i++ ].AsBool();

		if ( !wroteVar )
			continue;

		Assert( var.aType != EEntComponentVarType_Invalid );
		Assert( var.aType != EEntComponentVarType_Bool );

		switch ( var.aType )
		{
			default:
				break;

			case EEntComponentVarType_Float:
			{
				auto value = (float*)( data );
				*value     = vector[ i++ ].AsFloat();
				break;
			}
			case EEntComponentVarType_Double:
			{
				auto value = (double*)( data );
				*value     = vector[ i++ ].AsDouble();
				break;
			}

			case EEntComponentVarType_S8:
			{
				auto value = (s8*)( data );
				*value     = vector[ i++ ].AsInt8();
				break;
			}
			case EEntComponentVarType_S16:
			{
				auto value = (s16*)( data );
				*value     = vector[ i++ ].AsInt16();
				break;
			}
			case EEntComponentVarType_S32:
			{
				auto value = (s32*)( data );
				*value     = vector[ i++ ].AsInt32();
				break;
			}
			case EEntComponentVarType_S64:
			{
				auto value = (s64*)( data );
				*value     = vector[ i++ ].AsInt64();
				break;
			}

			case EEntComponentVarType_U8:
			{
				auto value = (u8*)( data );
				*value     = vector[ i++ ].AsUInt8();
				break;
			}
			case EEntComponentVarType_U16:
			{
				auto value = (u16*)( data );
				*value     = vector[ i++ ].AsUInt16();
				break;
			}
			case EEntComponentVarType_U32:
			{
				auto value = (u32*)( data );
				*value     = vector[ i++ ].AsUInt32();
				break;
			}
			case EEntComponentVarType_U64:
			{
				auto value = (u64*)( data );
				*value     = vector[ i++ ].AsUInt64();
				break;
			}

			case EEntComponentVarType_StdString:
			{
				auto value = (std::string*)( data );
				*value     = vector[ i++ ].AsString().str();
				break;
			}

				// Will have a special case for these once i have each value in a vecX marked dirty

			case EEntComponentVarType_Vec2:
			{
				auto value = (glm::vec2*)( data );
				value->x   = vector[ i++ ].AsFloat();
				value->y   = vector[ i++ ].AsFloat();
				break;
			}
			case EEntComponentVarType_Vec3:
			{
				auto value = (glm::vec3*)( data );
				value->x   = vector[ i++ ].AsFloat();
				value->y   = vector[ i++ ].AsFloat();
				value->z   = vector[ i++ ].AsFloat();
				break;
			}
			case EEntComponentVarType_Vec4:
			{
				auto value = (glm::vec4*)( data );
				value->x   = vector[ i++ ].AsFloat();
				value->y   = vector[ i++ ].AsFloat();
				value->z   = vector[ i++ ].AsFloat();
				value->w   = vector[ i++ ].AsFloat();
				break;
			}
		}
	}
}


bool WriteComponent( flexb::Builder& srBuilder, EntComponentData_t* spRegData, const void* spData, bool sFullUpdate )
{
	bool   wroteData = false;
	size_t curOffset = 0;
	size_t flexVec   = srBuilder.StartVector();

	for ( const auto& [ offset, var ] : spRegData->aVars )
	{
		if ( !var.aIsNetVar )
			continue;

		// size_t offset = EntComp_GetVarDirtyOffset( (char*)spData, var.aType );

		auto IsVarDirty = [ & ]()
		{
			if ( sFullUpdate )
			{
				wroteData = true;
				srBuilder.Bool( true );
				return true;
			}

			char* dataChar = (char*)spData;
			bool isDirty  = *reinterpret_cast< bool* >( dataChar + var.aSize + offset );
			
			wroteData |= isDirty;
			srBuilder.Bool( isDirty );
			return isDirty;
		};

		void* data = ( (char*)spData ) + offset;

		switch ( var.aType )
		{
			default:
			case EEntComponentVarType_Invalid:
				break;

			case EEntComponentVarType_Bool:
			{
				auto value = *(bool*)( data );
				srBuilder.Bool( value );
				wroteData = true;
				break;
			}

			case EEntComponentVarType_Float:
			{
				if ( IsVarDirty() )
				{
					auto value = *(float*)( data );
					srBuilder.Float( value );
				}
				
				break;
			}
			case EEntComponentVarType_Double:
			{
				if ( IsVarDirty() )
				{
					auto value = *(double*)( data );
					srBuilder.Double( value );
				}
				
				break;
			}

			case EEntComponentVarType_S8:
			{
				// FLEX BUFFERS STORES ALL INTS AND UINTS AS INT64 AND UINT64, WHAT A WASTE OF SPACE
				if ( IsVarDirty() )
				{
					auto value = *(s8*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntComponentVarType_S16:
			{
				if ( IsVarDirty() )
				{
					auto value = *(s16*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntComponentVarType_S32:
			{
				if ( IsVarDirty() )
				{
					auto value = *(s32*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntComponentVarType_S64:
			{
				if ( IsVarDirty() )
				{
					auto value = *(s64*)( data );
					srBuilder.Add( value );
				}

				break;
			}

			case EEntComponentVarType_U8:
			{
				// FLEX BUFFERS STORES ALL INTS AND UINTS AS INT64 AND UINT64, WHAT A WASTE OF SPACE
				if ( IsVarDirty() )
				{
					auto value = *(u8*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntComponentVarType_U16:
			{
				if ( IsVarDirty() )
				{
					auto value = *(u16*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntComponentVarType_U32:
			{
				if ( IsVarDirty() )
				{
					auto value = *(u32*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntComponentVarType_U64:
			{
				if ( IsVarDirty() )
				{
					auto value = *(u64*)( data );
					srBuilder.Add( value );
				}

				break;
			}

			case EEntComponentVarType_StdString:
			{
				if ( IsVarDirty() )
				{
					auto value = *(const std::string*)( data );
					srBuilder.Add( value );
				}

				break;
			}

			// Will have a special case for these once i have each value in a vecX marked dirty

			case EEntComponentVarType_Vec2:
			{
				if ( IsVarDirty() )
				{
					const glm::vec2* value = (const glm::vec2*)( data );
					srBuilder.Add( value->x );
					srBuilder.Add( value->y );
				}

				break;
			}
			case EEntComponentVarType_Vec3:
			{
				if ( IsVarDirty() )
				{
					const glm::vec3* value = (const glm::vec3*)( data );
					srBuilder.Add( value->x );
					srBuilder.Add( value->y );
					srBuilder.Add( value->z );
				}

				break;
			}
			case EEntComponentVarType_Vec4:
			{
				if ( IsVarDirty() )
				{
					const glm::vec4* value = (const glm::vec4*)( data );
					srBuilder.Add( value->x );
					srBuilder.Add( value->y );
					srBuilder.Add( value->z );
					srBuilder.Add( value->w );
				}

				break;
			}
		}
	}

	srBuilder.EndVector( flexVec, false, false );
	return wroteData;
}
#endif


// TODO: redo this by having it loop through component pools, and not entitys
// right now, it's doing a lot of entirely unnecessary checks
// we can avoid those if we loop through the pools instead
void EntitySystem::WriteComponentUpdates( fb::FlatBufferBuilder& srRootBuilder, bool sFullUpdate )
{
	PROF_SCOPE();

	// Make a list of component pools with a component that need updating
	std::vector< EntityComponentPool* >                 componentPools;
	std::vector< fb::Offset< NetMsg_ComponentUpdate > > componentsBuilt;

	for ( auto& [ name, pool ] : aComponentPools )
	{
		// If there are no components in existence, don't even bother to send anything here
		if ( !pool->aMapComponentToEntity.size() )
			continue;

		componentPools.push_back( pool );
	} 

	if ( Game_IsClient() && ent_show_component_net_updates )
	{
		gui->DebugMessage( "Sending \"%zd\" Component Types", componentPools.size() );
	}

	size_t i = 0;

	for ( auto pool : componentPools )
	{
		// If there are no components in existence, don't even bother to send anything here
		if ( !pool->aMapComponentToEntity.size() )
			continue;

		EntComponentData_t* regData = pool->GetRegistryData();

		if ( regData->aNetType != EEntComponentNetType_Both && regData->aNetType != EEntComponentNetType_Server )
			continue;

		std::vector< fb::Offset< NetMsg_ComponentUpdateData > > componentDataBuilt;

		std::vector< NetMsg_ComponentUpdateDataBuilder >        componentDataBuilders;

		// auto                                                             compNameOffset = srRootBuilder.CreateString( pool->apName );

		bool                                                    builtUpdateList = false;
		bool                                                    wroteData       = false;

		size_t compListI = 0;
		for ( auto& [ index, entity ] : pool->aMapComponentToEntity )
		{
			if ( pool->aComponentStates[ index ] == EEntityCreateState_Destroyed && !sFullUpdate )
			{
				// Don't bother sending data if we're about to be destroyed or we have no write function
				// srRootBuilder.Finish( compDataBuilder.Finish() );
				compListI++;
				continue;
			}

			void*                 data = pool->GetData( entity );

			// Write Component Data
			flexb::Builder flexBuilder;
			wroteData = WriteComponent( flexBuilder, regData, data, ent_always_full_update ? true : sFullUpdate );

			// if ( !sFullUpdate && !wroteData )
			// {
			// 	compListI++;
			// 	continue;
			// }

			fb::Offset< flatbuffers::Vector< u8 > > dataVector{};

			if ( wroteData )
			{
				flexBuilder.Finish();
				dataVector = srRootBuilder.CreateVector( flexBuilder.GetBuffer().data(), flexBuilder.GetSize() );
			}

			// Now after creating the data vector, we can make the update data builder

			NetMsg_ComponentUpdateDataBuilder& compDataBuilder = componentDataBuilders.emplace_back( srRootBuilder );

			compDataBuilder.add_id( entity );

			// Set Destroyed
			if ( pool->aComponentStates[ index ] == EEntityCreateState_Destroyed )
				compDataBuilder.add_destroyed( true );

			if ( wroteData )
			{
				if ( Game_IsClient() && ent_show_component_net_updates )
				{
					// gui->DebugMessage( "Sending Component Write Update to Clients: \"%s\" - %zd bytes", regData->apName, outputStream.aBuffer.size_bytes() );
					gui->DebugMessage( "Sending Component Write Update to Clients: \"%s\" - %zd bytes", regData->apName, flexBuilder.GetSize() );
				}

				compDataBuilder.add_values( dataVector );
			}

			builtUpdateList = true;
			componentDataBuilt.push_back( compDataBuilder.Finish() );

			// Reset Component Var Dirty Values
			if ( !ent_always_full_update )
			{
				for ( const auto& [ offset, var ] : regData->aVars )
				{
					if ( !var.aIsNetVar )
						continue;

					char* dataChar = static_cast< char* >( data );
					// EntComp_ResetVarDirty( dataChar + offset, var.aType );

					bool* aIsDirty = reinterpret_cast< bool* >( dataChar + var.aSize + offset );
					*aIsDirty      = false;
				}
			}

			compListI++;
		}

		// srRootBuilder.Finish();

		if ( componentDataBuilt.size() && builtUpdateList )
		{
			// oh my god
			fb::Offset< fb::Vector< fb::Offset< NetMsg_ComponentUpdateData > > > compVector{};

			//if ( wroteData )
				compVector = srRootBuilder.CreateVector( componentDataBuilt.data(), componentDataBuilt.size() );

			auto                           compNameOffset = srRootBuilder.CreateString( pool->apName );

			NetMsg_ComponentUpdateBuilder compUpdate( srRootBuilder );
			compUpdate.add_name( compNameOffset );

			//if ( wroteData )
				compUpdate.add_components( compVector );

			componentsBuilt.push_back( compUpdate.Finish() );

			if ( Game_IsClient() && ent_show_component_net_updates )
			{
				gui->DebugMessage( "Size of Component Write Update for \"%s\": %zd bytes", pool->apName, srRootBuilder.GetSize() );
			}
		}
		else
		{
			// NetMsg_ComponentUpdateBuilder& compUpdate = componentBuilders.emplace_back( srRootBuilder );
			// componentsBuilt.push_back( compUpdate.Finish() );
		}

		// if ( Game_IsClient() && ent_show_component_net_updates )
		// {
		// 	gui->DebugMessage( "Full Component Write Update: \"%s\" - %zd bytes", regData->apName, componentDataBuilt.size() * sizeof( flatbuffers::Offset< NetMsg_ComponentUpdateData > ) );
		// }

		i++;
	}

	auto                           updateListOut = srRootBuilder.CreateVector( componentsBuilt.data(), componentsBuilt.size() );

	NetMsg_ComponentUpdatesBuilder root( srRootBuilder );
	root.add_update_list( updateListOut );

	srRootBuilder.Finish( root.Finish() );

	if ( Game_IsClient() && ent_show_component_net_updates )
	{
		gui->DebugMessage( "Total Size of Component Write Update: %zd bytes", srRootBuilder.GetSize() );
	}
}


void EntitySystem::ReadComponentUpdates( const NetMsg_ComponentUpdates* spReader )
{
	PROF_SCOPE();

	// First, reset all dirty variables
	for ( auto& [ name, pool ] : GetEntitySystem()->aComponentPools )
	{
		EntComponentData_t* regData = pool->GetRegistryData();

		for ( auto [entity, compIndex] : pool->aMapEntityToComponent )
		{
			void* componentData = pool->aComponents[ compIndex ];

			// Reset Component Var Dirty Values
			for ( const auto& [ offset, var ] : regData->aVars )
			{
				if ( !var.aIsNetVar )
					continue;

				char* dataChar = static_cast< char* >( componentData );
				bool* isDirty  = reinterpret_cast< bool* >( dataChar + var.aSize + offset );
				*isDirty       = false;
			}
		}
	}

	auto componentUpdateList = spReader->update_list();

	if ( !componentUpdateList )
		return;

	for ( size_t i = 0; i < componentUpdateList->size(); i++ )
	{
		const NetMsg_ComponentUpdate* componentUpdate = componentUpdateList->Get( i );

		if ( !componentUpdate )
			continue;

		if ( !componentUpdate->name() )
			continue;

		// This shouldn't even be networked if we don't have any components
		if ( !componentUpdate->components() )
			continue;

		const char*          componentName = componentUpdate->name()->string_view().data();

		EntityComponentPool* pool          = GetComponentPool( componentName );

		AssertMsg( pool, "Failed to find component pool" );

		if ( !pool )
		{
			Log_ErrorF( "Failed to find component pool for component: \"%s\"\n", componentName );
			continue;
		}

		EntComponentData_t* regData = pool->GetRegistryData();

		AssertMsg( regData, "Failed to find component registry data" );

		// Tell the Component System this entity's component updated
		IEntityComponentSystem* system = GetComponentSystem( regData->apName );

		for ( size_t c = 0; c < componentUpdate->components()->size(); c++ )
		{
			const NetMsg_ComponentUpdateData* componentUpdateData = componentUpdate->components()->Get( c );

			if ( !componentUpdateData )
				continue;

			// Get the Entity and Make sure it exists
			Entity entId  = componentUpdateData->id();

			// Is this entity in the translation system?
			auto   it     = aEntityIDConvert.find( entId );
			if ( it == aEntityIDConvert.end() )
			{
				Log_Error( gLC_Entity, "Failed to find entity while updating components from server\n" );
				continue;
			}

			Entity entity = it->second;

			if ( !EntityExists( entity ) )
			{
				Log_Error( gLC_Entity, "Failed to find entity while updating components from server\n" );
				continue;
			}

			// Check the component state, do we need to remove it, or add it to the entity?
			if ( componentUpdateData->destroyed() )
			{
				// We can just remove the component right now, no need to queue it,
				// as this is before all client game processing
				pool->Remove( entity );
				continue;
			}

			void* componentData = GetComponent( entity, componentName );

			if ( !componentData )
			{
				// Create the component
				componentData = AddComponent( entity, componentName );

				if ( componentData == nullptr )
				{
					Log_ErrorF( "Failed to create component\n" );
					continue;
				}
			}

			// Now, update component data
			// NOTE: i could try to check if it's predicted here and get rid of aOverrideClient
			if ( !regData->aOverrideClient && entity == gLocalPlayer )
				continue;

			if ( componentUpdateData->values() )
			{
				auto             values   = componentUpdateData->values();
				flexb::Reference flexRoot = flexb::GetRoot( values->data(), values->size() );

				ReadComponent( flexRoot, regData, componentData );
				// regData->apRead( componentVerifier, values->data(), componentData );

				Log_DevF( gLC_Entity, 2, "Parsed component data for entity \"%zd\" - \"%s\"\n", entity, componentName );

				if ( system )
				{
					system->ComponentUpdated( entity, componentData );
				}
			}
		}
	}
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
		Log_FatalF( gLC_Entity, "Failed to create component - no component pool found: \"%s\"\n", spName );
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
		Log_FatalF( gLC_Entity, "Failed to get component - no component pool found: \"%s\"\n", spName );
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
		Log_FatalF( gLC_Entity, "Failed to remove component - no component pool found: \"%s\"\n", spName );
		return;
	}

	pool->RemoveQueued( entity );
}


// Sets Prediction on this component
void EntitySystem::SetComponentPredicted( Entity entity, const char* spName, bool sPredicted )
{
	if ( this == sv_entities )
	{
		// The server does not need to know if it's predicted, the client will have special handling for this
		Log_ErrorF( gLC_Entity, "Tried to mark entity component as predicted on server - \"%s\"\n", spName );
		return;
	}

	auto pool = GetComponentPool( spName );

	if ( pool == nullptr )
	{
		Log_FatalF( gLC_Entity, "Failed to set component prediction - no component pool found: \"%s\"\n", spName );
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
		Log_ErrorF( gLC_Entity, "Tried to find a predicted entity component on server - \"%s\"\n", spName );
		return false;
	}

	auto pool = GetComponentPool( spName );

	if ( pool == nullptr )
	{
		Log_FatalF( gLC_Entity, "Failed to get component prediction - no component pool found: \"%s\"\n", spName );
		return false;
	}

	return pool->IsPredicted( entity );
}


EntityComponentPool* EntitySystem::GetComponentPool( const char* spName )
{
	auto it = aComponentPools.find( spName );

	if ( it == aComponentPools.end() )
		Log_FatalF( gLC_Entity, "Component not registered before use: \"%s\"\n", spName );

	return it->second;
}


// ===================================================================================
// Console Commands
// ===================================================================================


CONCMD( ent_dump_registry )
{
	LogGroup group = Log_GroupBegin( gLC_Entity );

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
		Log_GroupF( group, "   Variable Count:  %zd\n", regData->aVars.size() );

		for ( const auto& [ offset, var ] : regData->aVars )
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

	LogGroup group = Log_GroupBegin( gLC_Entity );

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
		Entity                              entity = GetEntitySystem()->aUsedEntities[ i ];

		// this is the worst thing ever
		std::vector< EntityComponentPool* > pools;

		// Find all component pools that contain this Entity
		// That means the Entity has the type of component that pool is for
		for ( auto& [ name, pool ] : GetEntitySystem()->aComponentPools )
		{
			if ( pool->Contains( entity ) )
				pools.push_back( pool );
		}

		Log_GroupF( group, "\nEntity %zd\n", entity );
		// Log_GroupF( group, "    Predicted: %s\n", GetEntitySystem() );

		for ( EntityComponentPool* pool : pools )
		{
			auto data    = pool->GetData( entity );
			auto regData = pool->GetRegistryData();

			Log_GroupF( group, "\n    Component: %s\n", regData->apName );
			Log_GroupF( group, "    Size: %zd\n", regData->aSize );
			Log_GroupF( group, "    Predicted: %s\n", pool->IsPredicted( entity ) ? "TRUE" : "FALSE" );

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


// MOVE TO util.h
template< typename KEY, typename VALUE >
inline size_t Util_SizeOfUnordredMap( const std::unordered_map< KEY, VALUE >& srMap )
{
	return ( srMap.size() * ( sizeof( std::unordered_map< KEY, VALUE >::value_type ) + sizeof( void* ) ) +  // data list
	         srMap.bucket_count() * ( sizeof( void* ) + sizeof( size_t ) ) )                                // bucket index
	       * 1.5;                                                                                           // estimated allocation overheads
}


CONCMD( ent_mem )
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

	LogGroup group = Log_GroupBegin( gLC_Entity );

	size_t componentPoolMapSize = Util_SizeOfUnordredMap( GetEntitySystem()->aComponentPools );
	size_t componentPoolSize    = 0;

	for ( auto& [name, pool] : GetEntitySystem()->aComponentPools )
	{
		size_t              curSize       = sizeof( EntityComponentPool );
		size_t              compSize      = 0;
		size_t              compOtherSize = 0;

		EntComponentData_t* regData  = pool->GetRegistryData();

		for ( const auto& [ offset, var ] : regData->aVars )
		{
			// Check for special case std::string
			if ( var.aType == EEntComponentVarType_StdString )
			{
				// We have to iterate through all components and get the amount of memory used by each std::string
				// compOtherSize += 0;
			}
		}

		compSize = regData->aSize * pool->aCount;
		curSize += compSize + compOtherSize;

		Log_GroupF( group, "Component Pool \"%s\": %zd bytes\n", pool->apName, curSize );
		Log_GroupF( group, "    %zd bytes per component * %zd components\n\n", regData->aSize, pool->aCount );

		componentPoolSize += curSize;
	}

	Log_GroupF( group, "Component Pool Base Size: %zd bytes\n", sizeof( EntityComponentPool ) );
	Log_GroupF( group, "Component Pool Map Size: %zd bytes\n", componentPoolMapSize );
	Log_GroupF( group, "Component Pools: %zd bytes (%zd pools)\n", componentPoolSize, GetEntitySystem()->aComponentPools.size() );

	Log_GroupEnd( group );

	// Make sure to reset this
	if ( useServer )
	{
		Game_SetClient( true );
	}
}


// ====================================================================================================
// Base Components
// ====================================================================================================


void Ent_RegisterVarHandlers()
{
}


void Ent_RegisterBaseComponents()
{
	Ent_RegisterVarHandlers();

	// Setup Types
	gEntComponentRegistry.aVarTypes[ typeid( bool ).hash_code() ]        = EEntComponentVarType_Bool;
	gEntComponentRegistry.aVarTypes[ typeid( float ).hash_code() ]       = EEntComponentVarType_Float;
	gEntComponentRegistry.aVarTypes[ typeid( double ).hash_code() ]      = EEntComponentVarType_Double;

	gEntComponentRegistry.aVarTypes[ typeid( s8 ).hash_code() ]          = EEntComponentVarType_S8;
	gEntComponentRegistry.aVarTypes[ typeid( s16 ).hash_code() ]         = EEntComponentVarType_S16;
	gEntComponentRegistry.aVarTypes[ typeid( s32 ).hash_code() ]         = EEntComponentVarType_S32;
	gEntComponentRegistry.aVarTypes[ typeid( s64 ).hash_code() ]         = EEntComponentVarType_S64;

	gEntComponentRegistry.aVarTypes[ typeid( u8 ).hash_code() ]          = EEntComponentVarType_U8;
	gEntComponentRegistry.aVarTypes[ typeid( u16 ).hash_code() ]         = EEntComponentVarType_U16;
	gEntComponentRegistry.aVarTypes[ typeid( u32 ).hash_code() ]         = EEntComponentVarType_U32;
	gEntComponentRegistry.aVarTypes[ typeid( u64 ).hash_code() ]         = EEntComponentVarType_U64;

	gEntComponentRegistry.aVarTypes[ typeid( std::string ).hash_code() ] = EEntComponentVarType_StdString;

	gEntComponentRegistry.aVarTypes[ typeid( glm::vec2 ).hash_code() ]   = EEntComponentVarType_Vec2;
	gEntComponentRegistry.aVarTypes[ typeid( glm::vec3 ).hash_code() ]   = EEntComponentVarType_Vec3;
	gEntComponentRegistry.aVarTypes[ typeid( glm::vec4 ).hash_code() ]   = EEntComponentVarType_Vec4;

	// Now Register Base Components
	EntComp_RegisterComponent< CTransform >(
	  "transform", true, EEntComponentNetType_Both,
	  [ & ]()
	  {
		  auto transform           = new CTransform;
		  transform->aScale.Edit() = { 1.f, 1.f, 1.f };
		  return transform;
	  },
	  [ & ]( void* spData )
	  { delete (CTransform*)spData; } );

	EntComp_RegisterComponentVar< CTransform, glm::vec3 >( "aPos", "pos", offsetof( CTransform, aPos ), typeid( CTransform::aPos ).hash_code() );
	EntComp_RegisterComponentVar< CTransform, glm::vec3 >( "aAng", "ang", offsetof( CTransform, aAng ), typeid( CTransform::aAng ).hash_code() );
	EntComp_RegisterComponentVar< CTransform, glm::vec3 >( "aScale", "scale", offsetof( CTransform, aScale ), typeid( CTransform::aScale ).hash_code() );
	// EntComp_RegisterComponentReadWrite< CTransform >( TEMP_TransformRead, TEMP_TransformWrite );
	CH_REGISTER_COMPONENT_SYS( CTransform, EntSys_Transform, gEntSys_Transform );

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
	EntComp_RegisterComponentVar< CCamera, glm::vec3 >( "aPos", "pos", offsetof( CCamera, aTransform.aValue.aPos ), typeid( CCamera::aTransform.aValue.aPos ).hash_code() );
	EntComp_RegisterComponentVar< CCamera, glm::vec3 >( "aAng", "ang", offsetof( CCamera, aTransform.aValue.aAng ), typeid( CCamera::aTransform.aValue.aAng ).hash_code() );

	CH_REGISTER_COMPONENT_RW( CModelInfo, modelInfo, true );
	CH_REGISTER_COMPONENT_SYS( CModelInfo, EntSys_ModelInfo, gEntSys_ModelInfo );
	CH_REGISTER_COMPONENT_VAR( CModelInfo, std::string, aPath, path );

	// CH_REGISTER_COMPONENT( Model, model, true, EEntComponentNetType_Both );
	CH_REGISTER_COMPONENT( CRenderable_t, renderable, false, EEntComponentNetType_Client );
	CH_REGISTER_COMPONENT_SYS( CRenderable_t, EntSys_Renderable, gEntSys_Renderable );
	// CH_REGISTER_COMPONENT( Renderable_t, renderable, false, EEntComponentNetType_Client );

	CH_REGISTER_COMPONENT( CMap, map, true, EEntComponentNetType_Both );

	// TODO: Merge CModelInfo and CRenderable_t into this instead
	// would make your life easier
	// only difficulty is figuring out how to network the model paths...
	// since i would kind of like to be able to create models in the engine that use this,
	// so there would be no path
	// unless you used a "protocol system", so an internally created model path would be this:
	// "internal://model_0"
	// and a file on the disk to load will be this:
	// "file://path/to/asset.glb"
	CH_REGISTER_COMPONENT( CAutoRenderable, autoRenderable, true, EEntComponentNetType_Both );
	CH_REGISTER_COMPONENT_VAR( CAutoRenderable, bool, aTestVis, testVis );
	CH_REGISTER_COMPONENT_VAR( CAutoRenderable, bool, aCastShadow, castShadow );
	CH_REGISTER_COMPONENT_VAR( CAutoRenderable, bool, aVisible, visible );
	CH_REGISTER_COMPONENT_SYS( CAutoRenderable, EntSys_AutoRenderable, gEntSys_AutoRenderable );

	CH_REGISTER_COMPONENT( CPhysInfo, physInfo, true, EEntComponentNetType_Both );
	CH_REGISTER_COMPONENT_SYS( CPhysInfo, EntSys_PhysInfo, gEntSys_PhysInfo );

	// Probably should be in graphics?
	CH_REGISTER_COMPONENT_RW( CLight, light, true );
	CH_REGISTER_COMPONENT_SYS( CLight, LightSystem, gLightEntSystems );
	CH_REGISTER_COMPONENT_VAR( CLight, ELightType, aType, type );
	CH_REGISTER_COMPONENT_VAR( CLight, glm::vec4, aColor, color );

	// TODO: these 2 should not be here
    // it should be attached to it's own entity that can be parented
    // and that entity needs to contain the transform (or transform small) component
	CH_REGISTER_COMPONENT_VAR( CLight, glm::vec3, aPos, pos );
	CH_REGISTER_COMPONENT_VAR( CLight, glm::vec3, aAng, ang );

	CH_REGISTER_COMPONENT_VAR( CLight, float, aInnerFov, innerFov );
	CH_REGISTER_COMPONENT_VAR( CLight, float, aOuterFov, outerFov );
	CH_REGISTER_COMPONENT_VAR( CLight, float, aRadius, radius );
	CH_REGISTER_COMPONENT_VAR( CLight, float, aLength, length );
	CH_REGISTER_COMPONENT_VAR( CLight, bool, aShadow, shadow );
	CH_REGISTER_COMPONENT_VAR( CLight, bool, aEnabled, enabled );
	CH_REGISTER_COMPONENT_VAR( CLight, bool, aUseTransform, useTransform );
}

