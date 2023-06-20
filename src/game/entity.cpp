#include "main.h"
#include "game_shared.h"
#include "entity.h"
#include "world.h"
#include "util.h"
#include "player.h"  // TEMP - for CPlayerMoveData

#include "ent_light.h"
#include "igui.h"

#include "graphics/graphics.h"

#include "game_physics.h"  // just for IPhysicsShape* and IPhysicsObject*


LOG_REGISTER_CHANNEL2( Entity, LogColor::White );


EntComponentRegistry_t gEntComponentRegistry;

EntitySystem*          cl_entities                                     = nullptr;
EntitySystem*          sv_entities                                     = nullptr;


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


void EntComp_ResetVarDirty( char* spData, EEntComponentVarType sVarType )
{
	size_t offset = 0;
	switch ( sVarType )
	{
		default:
		case EEntComponentVarType_Invalid:
			return;

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
	aMapEntityToComponent.erase( sIndex );

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
	// for ( auto& [ index, state ] : aComponentStates )
	// for ( size_t i = 0; i < aComponentStates.size(); i++ )
	for ( auto it = aComponentStates.begin(); it != aComponentStates.end(); it++ )
	{
		if ( it->second == EEntityCreateState_Destroyed )
		{
			RemoveByIndex( it->first );
			continue;
		}

		if ( it->second == EEntityCreateState_Created )
		{
			it->second = EEntityCreateState_None;
		}
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


Entity EntitySystem::CreateEntityFromServer( Entity desiredId )
{
	AssertMsg( aEntityCount < CH_MAX_ENTITIES, "Hit Entity Limit!" );

	size_t index = vec_index( aEntityPool, desiredId );

	if ( index == SIZE_MAX )
	{
		Log_Error( gLC_Entity, "Entity ID from Server is already taken on Client\n" );
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
//void EntitySystem::ReadEntityUpdates( capnp::MessageReader& srReader )
//{
//}


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


// TODO: redo this by having it loop through component pools, and not entitys
// right now, it's doing a lot of entirely unnecessary checks
// we can avoid those if we loop through the pools instead
void EntitySystem::WriteComponentUpdates( flatbuffers::FlatBufferBuilder& srRootBuilder, bool sFullUpdate )
{
	// Make a list of component pools with a component that need updating
	std::vector< EntityComponentPool* >                 componentPools;
	std::vector< NetMsg_ComponentUpdateBuilder >        componentBuilders;
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

		std::vector< fb::Offset< NetMsg_ComponentUpdateData > > componentDataBuilt;

		auto                                                    regData = pool->GetRegistryData();

		std::vector< NetMsg_ComponentUpdateDataBuilder >        componentDataBuilders;

		// auto                                                             compNameOffset = srRootBuilder.CreateString( pool->apName );

		bool                                                    builtUpdateList = false;
		bool                                                    wroteData       = false;

		size_t compListI = 0;
		for ( auto& [ index, entity ] : pool->aMapComponentToEntity )
		{
			if ( pool->aComponentStates[ index ] == EEntityCreateState_Destroyed || (!regData->apWrite && !sFullUpdate) )
			{
				// Don't bother sending data if we're about to be destroyed or we have no write function
				// srRootBuilder.Finish( compDataBuilder.Finish() );
				compListI++;
				continue;
			}

			void*                 data = pool->GetData( entity );

			// Write Component Data
			fb::FlatBufferBuilder componentBuffer;

			if ( regData->apWrite )
				wroteData = regData->apWrite( componentBuffer, data, ent_always_full_update ? true : sFullUpdate );

			// If no data is written and this isn't a full update, don't send this component over
			if ( regData->apWrite && !sFullUpdate && !wroteData )
			{
				compListI++;
				continue;
			}

			fb::Offset< flatbuffers::Vector< u8 > > dataVector{};

			if ( wroteData )
			{
				auto size  = componentBuffer.GetSize();
				dataVector = srRootBuilder.CreateVector( componentBuffer.GetBufferPointer(), componentBuffer.GetSize() );
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
					gui->DebugMessage( "Sending Component Write Update to Clients: \"%s\" - %zd bytes", regData->apName, componentBuffer.GetSize() );
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

			if ( wroteData )
				compVector = srRootBuilder.CreateVector( componentDataBuilt.data(), componentDataBuilt.size() );

			auto                           compNameOffset = srRootBuilder.CreateString( pool->apName );

			NetMsg_ComponentUpdateBuilder& compUpdate = componentBuilders.emplace_back( srRootBuilder );
			compUpdate.add_name( compNameOffset );

			if ( wroteData )
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
		Log_GroupF( group, "   Has Write:       %s\n", regData->apWrite ? "TRUE" : "FALSE" );
		Log_GroupF( group, "   Has Read:        %s\n", regData->apRead ? "TRUE" : "FALSE" );
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


// ====================================================================================================
// Base Components
// 
// TODO: Break these down into base functions for reading and writing certain types and structs
// And then have the Component Database or Entity System call those base functions
// Kind of like NetHelper_ReadVec3 and NetComp_WriteDirection
// ====================================================================================================


// TODO: try this instead for all these
void TEMP_TransformRead( flatbuffers::Verifier& srVerifier, const uint8_t* spSerialized, void* spData )
{
	CTransform* spTransform = static_cast< CTransform* >( spData );
	auto        message     = flatbuffers::GetRoot< NetComp_Transform >( spSerialized );

	if ( !message || !message->Verify( srVerifier ) )
		return;

	NetHelper_ReadVec3( message->pos(), spTransform->aPos.Edit() );
	NetHelper_ReadVec3( message->ang(), spTransform->aAng.Edit() );
	NetHelper_ReadVec3( message->scale(), spTransform->aScale.Edit() );
}

bool TEMP_TransformWrite( flatbuffers::FlatBufferBuilder& srBuilder, const void* spData, bool sFullUpdate )
{
	const CTransform* spTransform = static_cast< const CTransform* >( spData );
	bool              isDirty     = sFullUpdate;

	isDirty |= spTransform->aPos.aIsDirty;
	isDirty |= spTransform->aAng.aIsDirty;
	isDirty |= spTransform->aScale.aIsDirty;

	if ( !isDirty )
		return false;

#if 0
	flatbuffers::Offset< Vec3 > posOffset{};
	flatbuffers::Offset< Vec3 > angOffset{};
	flatbuffers::Offset< Vec3 > scaleOffset{};

	if ( CH_VAR_DIRTY( spTransform->aPos ) )
	{
		Vec3Builder vecBuild( srBuilder );
		NetHelper_WriteVec3( vecBuild, spTransform->aPos );
		posOffset = vecBuild.Finish();
	}

	CH_NET_WRITE_VEC3( ang, spTransform->aAng );
	CH_NET_WRITE_VEC3( scale, spTransform->aScale );
#endif

	NetComp_TransformBuilder transformBuilder( srBuilder );

#if 1
	Vec3 posVec( spTransform->aPos.Get().x, spTransform->aPos.Get().y, spTransform->aPos.Get().z );
	transformBuilder.add_pos( &posVec );

	CH_NET_WRITE_VEC3( transformBuilder, ang, spTransform->aAng );
	CH_NET_WRITE_VEC3( transformBuilder, scale, spTransform->aScale );
#else
	if ( !posOffset.IsNull() )
		transformBuilder.add_pos( posOffset );

	CH_NET_WRITE_OFFSET( transformBuilder, ang );
	CH_NET_WRITE_OFFSET( transformBuilder, scale );
#endif

	auto builtTransform = transformBuilder.Finish();
	srBuilder.Finish( builtTransform );

	return true;
}


CH_COMPONENT_READ_DEF( CTransformSmall )
{
	/*CTransformSmall* spTransform = static_cast< CTransformSmall* >( spData );
	auto             message     = srReader.getRoot< NetCompTransformSmall >();

	if ( message.hasPos() )
		NetHelper_ReadVec3( message.getPos(), spTransform->aPos.Edit() );

	if ( message.hasAng() )
		NetHelper_ReadVec3( message.getAng(), spTransform->aAng.Edit() );*/
}

CH_COMPONENT_WRITE_DEF( CTransformSmall )
{
	return false;

	/*const CTransformSmall* spTransform = static_cast< const CTransformSmall* >( spData );
	bool                   isDirty     = sFullUpdate;

	isDirty |= spTransform->aPos.aIsDirty;
	isDirty |= spTransform->aAng.aIsDirty;

	if ( !isDirty )
		return false;

	auto builder     = srMessage.initRoot< NetCompTransformSmall >();

	if ( spTransform->aPos.aIsDirty || sFullUpdate )
	{
		auto pos = builder.initPos();
		NetHelper_WriteVec3( &pos, spTransform->aPos );
	}

	if ( spTransform->aAng.aIsDirty || sFullUpdate )
	{
		auto ang = builder.initAng();
		NetHelper_WriteVec3( &ang, spTransform->aAng );
	}

	return true;*/
}


CH_COMPONENT_READ_DEF( CRigidBody )
{
	CRigidBody* spRigidBody = static_cast< CRigidBody* >( spData );
	auto        message     = flatbuffers::GetRoot< NetComp_RigidBody >( spSerialized );

	if ( !message || !message->Verify( srVerifier ) )
		return;

	NetHelper_ReadVec3( message->vel(), spRigidBody->aVel.Edit() );
	NetHelper_ReadVec3( message->accel(), spRigidBody->aAccel.Edit() );
}

CH_COMPONENT_WRITE_DEF( CRigidBody )
{
	const CRigidBody* spRigidBody = static_cast< const CRigidBody* >( spData );
	bool              isDirty     = sFullUpdate;

	isDirty |= spRigidBody->aVel.aIsDirty;
	isDirty |= spRigidBody->aAccel.aIsDirty;

	if ( !isDirty )
		return false;

	flatbuffers::Offset< Vec3 > velOffset{};
	flatbuffers::Offset< Vec3 > accelOffset{};

	NetComp_RigidBodyBuilder compBuilder( srBuilder );

	CH_NET_WRITE_VEC3( compBuilder, vel, spRigidBody->aVel );
	CH_NET_WRITE_VEC3( compBuilder, accel, spRigidBody->aAccel );

	CH_NET_WRITE_OFFSET( compBuilder, vel );
	CH_NET_WRITE_OFFSET( compBuilder, accel );

	srBuilder.Finish( compBuilder.Finish() );

	return true;
}

// TEMP
void NetComp_ReadDirection( const NetComp_Direction* spReader, CDirection& srData )
{
	if ( spReader->forward() )
		NetHelper_ReadVec3( spReader->forward(), srData.aForward.Edit() );

	if ( spReader->up() )
		NetHelper_ReadVec3( spReader->up(), srData.aUp.Edit() );

	if ( spReader->right() )
		NetHelper_ReadVec3( spReader->right(), srData.aRight.Edit() );
}

// void NetComp_WriteDirection( flatbuffers::FlatBufferBuilder& srBuilder, NetComp_DirectionBuilder& builder, const CDirection& srData, bool sFullUpdate )
// {
// 	CH_NET_WRITE_VEC3( builder, forward, srData.aForward );
// 	CH_NET_WRITE_VEC3( builder, up, srData.aUp );
// 	CH_NET_WRITE_VEC3( builder, right, srData.aRight );
// }

CH_COMPONENT_READ_DEF( CDirection )
{
	CDirection* spDirection = static_cast< CDirection* >( spData );
	auto        message     = flatbuffers::GetRoot< NetComp_Direction >( spSerialized );

	if ( !message )
		return;

	bool valid = message->Verify( srVerifier );
	
	if ( valid )
		NetComp_ReadDirection( message, *spDirection );
}

CH_COMPONENT_WRITE_DEF( CDirection )
{
	const CDirection* spDirection = static_cast< const CDirection* >( spData );
	bool              isDirty     = sFullUpdate;

	isDirty |= spDirection->aForward.aIsDirty;
	isDirty |= spDirection->aRight.aIsDirty;
	isDirty |= spDirection->aUp.aIsDirty;

	if ( !isDirty )
		return false;

	flatbuffers::Offset< Vec3 > forwardOffset{};
	flatbuffers::Offset< Vec3 > upOffset{};
	flatbuffers::Offset< Vec3 > rightOffset{};

	NetComp_DirectionBuilder    dirBuilder( srBuilder );

	CH_NET_WRITE_VEC3( dirBuilder, forward, spDirection->aForward );
	CH_NET_WRITE_VEC3( dirBuilder, up, spDirection->aUp );
	CH_NET_WRITE_VEC3( dirBuilder, right, spDirection->aRight );

	CH_NET_WRITE_OFFSET( dirBuilder, forward );
	CH_NET_WRITE_OFFSET( dirBuilder, up );
	CH_NET_WRITE_OFFSET( dirBuilder, right );

	srBuilder.Finish( dirBuilder.Finish() );

	return true;
}


CH_COMPONENT_READ_DEF( CCamera )
{
	CCamera* spCamera = static_cast< CCamera* >( spData );
	auto     message  = flatbuffers::GetRoot< NetComp_Camera >( spSerialized );

	if ( !message || !message->Verify( srVerifier ) )
		return;

	if ( message->direction() )
		NetComp_ReadDirection( message->direction(), *spCamera );

	spCamera->aFov = message->fov();

	if ( message->transform() )
	{
		NetHelper_ReadVec3( message->transform()->pos(), spCamera->aTransform.Edit().aPos.Edit() );
		NetHelper_ReadVec3( message->transform()->ang(), spCamera->aTransform.Edit().aAng.Edit() );
	}
}

CH_COMPONENT_WRITE_DEF( CCamera )
{
	const CCamera* spCamera = static_cast< const CCamera* >( spData );
	bool           isDirty  = sFullUpdate;

	isDirty |= spCamera->aFov.aIsDirty;
	isDirty |= spCamera->aTransform.aIsDirty;
	isDirty |= spCamera->aForward.aIsDirty;
	isDirty |= spCamera->aRight.aIsDirty;
	isDirty |= spCamera->aUp.aIsDirty;

	if ( !isDirty )
		return false;

	flatbuffers::Offset< NetComp_Direction > dirOffset{};

	// this is stupid
	if ( spCamera->aForward.aIsDirty || spCamera->aRight.aIsDirty || spCamera->aUp.aIsDirty || sFullUpdate )
	{
		flatbuffers::Offset< Vec3 > forwardOffset{};
		flatbuffers::Offset< Vec3 > upOffset{};
		flatbuffers::Offset< Vec3 > rightOffset{};

		NetComp_DirectionBuilder    dirBuilder( srBuilder );

		CH_NET_WRITE_VEC3( dirBuilder, forward, spCamera->aForward );
		CH_NET_WRITE_VEC3( dirBuilder, up, spCamera->aUp );
		CH_NET_WRITE_VEC3( dirBuilder, right, spCamera->aRight );

		CH_NET_WRITE_OFFSET( dirBuilder, forward );
		CH_NET_WRITE_OFFSET( dirBuilder, up );
		CH_NET_WRITE_OFFSET( dirBuilder, right );

		dirOffset = dirBuilder.Finish();
	}

	NetComp_CameraBuilder compBuilder( srBuilder );

	if ( !dirOffset.IsNull() )
		compBuilder.add_direction( dirOffset );

	if ( spCamera->aFov.aIsDirty || sFullUpdate )
		compBuilder.add_fov( spCamera->aFov );

	srBuilder.Finish( compBuilder.Finish() );

	// if ( spCamera->aTransform.Get().aPos.aIsDirty || sFullUpdate )
	// {
	// 	Vec3::Builder pos = builder.getTransform().initPos();
	// 	pos.setX( spCamera->aTransform.Get().aPos.Get().x );
	// 	pos.setY( spCamera->aTransform.Get().aPos.Get().y );
	// 	pos.setZ( spCamera->aTransform.Get().aPos.Get().z );
	// }
	// 
	// if ( spCamera->aTransform.Get().aAng.aIsDirty || sFullUpdate )
	// {
	// 	Vec3::Builder ang = builder.getTransform().initAng();
	// 	ang.setX( spCamera->aTransform.Get().aAng.Get().x );
	// 	ang.setY( spCamera->aTransform.Get().aAng.Get().y );
	// 	ang.setZ( spCamera->aTransform.Get().aAng.Get().z );
	// }

	return true;
}


CH_COMPONENT_READ_DEF( CGravity )
{
	// CGravity* spGravity = static_cast< CGravity* >( spData );
	// auto      message  = srReader.getRoot< NetCompGravity >();
	// 
	// if ( message.hasForce() )
	// 	NetHelper_ReadVec3( message.getForce(), spGravity->aForce.Edit() );
}

CH_COMPONENT_WRITE_DEF( CGravity )
{
	return false;

	/*const CGravity* spGravity = static_cast< const CGravity* >( spData );
	bool            isDirty   = sFullUpdate;

	isDirty |= spGravity->aForce.aIsDirty;

	if ( !isDirty )
		return false;

	auto builder = srMessage.initRoot< NetCompGravity >();

	CH_NET_WRITE_VEC3( Force, spGravity->aForce );
	return true;*/
}


CH_COMPONENT_READ_DEF( CModelInfo )
{
	auto* spModelPath = static_cast< CModelInfo* >( spData );
	auto  message     = flatbuffers::GetRoot< NetComp_ModelInfo >( spSerialized );

	if ( !message || !message->Verify( srVerifier ) )
		return;

	if ( message->path() )
		spModelPath->aPath = message->path()->c_str();
}

CH_COMPONENT_WRITE_DEF( CModelInfo )
{
	const auto* spModelInfo = static_cast< const CModelInfo* >( spData );

	bool        isDirty     = sFullUpdate;
	isDirty |= spModelInfo->aPath.aIsDirty;

	if ( !isDirty )
		return false;

	auto                     path = srBuilder.CreateString( spModelInfo->aPath.Get() );

	NetComp_ModelInfoBuilder compBuilder( srBuilder );
	compBuilder.add_path( path );
	srBuilder.Finish( compBuilder.Finish() );

	return true;
}


CH_COMPONENT_READ_DEF( CLight )
{
	auto* spLight = static_cast< CLight* >( spData );
	auto  message = flatbuffers::GetRoot< NetComp_Light >( spSerialized );

	if ( !message || !message->Verify( srVerifier ) )
		return;

	NetHelper_ReadVec4( message->color(), spLight->aColor.Edit() );
	NetHelper_ReadVec3( message->pos(), spLight->aPos.Edit() );
	NetHelper_ReadVec3( message->ang(), spLight->aAng.Edit() );

	spLight->aType     = static_cast< ELightType >( message->type() );
	spLight->aInnerFov = message->innerFov();
	spLight->aOuterFov = message->outerFov();
	spLight->aRadius   = message->radius();
	spLight->aLength   = message->length();

	spLight->aShadow   = message->shadow();
	spLight->aEnabled  = message->enabled();
}


CH_COMPONENT_WRITE_DEF( CLight )
{
	return false;

	/*auto* spLight = static_cast< const CLight* >( spData );

	// Don't update anything else if the light isn't even enabled
	// Actually, this may cause issues for a multiplayer map editor
	// if ( !spLight->aEnabled && !spLight->aEnabled.aIsDirty )
	// 	return false;

	bool  isDirty = sFullUpdate;
	isDirty |= spLight->aColor.aIsDirty;
	isDirty |= spLight->aAng.aIsDirty;
	isDirty |= spLight->aPos.aIsDirty;
	isDirty |= spLight->aType.aIsDirty;
	isDirty |= spLight->aInnerFov.aIsDirty;
	isDirty |= spLight->aOuterFov.aIsDirty;
	isDirty |= spLight->aRadius.aIsDirty;
	isDirty |= spLight->aLength.aIsDirty;
	isDirty |= spLight->aShadow.aIsDirty;
	isDirty |= spLight->aEnabled.aIsDirty;

	if ( !isDirty )
		return false;

	auto  builder = srMessage.initRoot< NetCompLight >();

	CH_NET_WRITE_VEC4( Color, spLight->aColor );
	CH_NET_WRITE_VEC3( Pos, spLight->aPos );
	CH_NET_WRITE_VEC3( Ang, spLight->aAng );

	builder.setType( spLight->aType );
	builder.setInnerFov( spLight->aInnerFov );
	builder.setOuterFov( spLight->aOuterFov );
	builder.setRadius( spLight->aRadius );
	builder.setLength( spLight->aLength );

	builder.setShadow( spLight->aShadow );
	builder.setEnabled( spLight->aEnabled );

	return true;*/
}


void Ent_RegisterBaseComponents()
{
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
	EntComp_RegisterComponent< CTransform >( "transform", true, EEntComponentNetType_Both,
		[ & ]() { return new CTransform; }, [ & ]( void* spData ) { delete (CTransform*)spData; } );

	EntComp_RegisterComponentVar< CTransform, glm::vec3 >( "aPos", "pos", offsetof( CTransform, aPos ), typeid( CTransform::aPos ).hash_code() );
	EntComp_RegisterComponentVar< CTransform, glm::vec3 >( "aAng", "ang", offsetof( CTransform, aAng ), typeid( CTransform::aAng ).hash_code() );
	EntComp_RegisterComponentVar< CTransform, glm::vec3 >( "aScale", "scale", offsetof( CTransform, aScale ), typeid( CTransform::aScale ).hash_code() );
	EntComp_RegisterComponentReadWrite< CTransform >( TEMP_TransformRead, TEMP_TransformWrite );
	CH_REGISTER_COMPONENT_SYS( CTransform, EntSys_Transform, gEntSys_Transform );

	CH_REGISTER_COMPONENT_RW( CTransformSmall, transformSmall, true );
	CH_REGISTER_COMPONENT_VAR( CTransformSmall, glm::vec3, aPos, pos );
	CH_REGISTER_COMPONENT_VAR( CTransformSmall, glm::vec3, aAng, ang );

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
}

