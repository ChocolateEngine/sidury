#include <capnp/message.h>
#include <capnp/serialize-packed.h>

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
#include "capnproto/sidury.capnp.h"


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
		apComponentSystem->ComponentAdded( entity );
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

	// Remove it from systems
	// for ( auto system : aComponentSystems )
	if ( apComponentSystem )
	{
		apComponentSystem->ComponentRemoved( entity );
		vec_remove( apComponentSystem->aEntities, entity );
	}

	size_t index = it->second;
	aMapComponentToEntity.erase( index );
	aMapEntityToComponent.erase( it );

	aComponentStates.erase( index );

	void* data = aComponents[ index ];
	Assert( data );

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

	// Remove it from the system
	if ( apComponentSystem )
	{
		apComponentSystem->ComponentRemoved( entity );
		vec_remove( apComponentSystem->aEntities, entity );
	}

	aMapComponentToEntity.erase( it );
	aMapEntityToComponent.erase( sIndex );

	aComponentStates.erase( sIndex );

	void* data = aComponents[ sIndex ];
	Assert( data );

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


// TODO: redo this by having it loop through component pools, and not entitys
// right now, it's doing a lot of entirely unnecessary checks
// we can avoid those if we loop through the pools instead
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

		// Get Entity State
		update.setDestroyed( aEntityStates[ entity ] == EEntityCreateState_Destroyed );
	}
}


// Read and write from the network
void EntitySystem::ReadComponentUpdates( capnp::MessageReader& srReader )
{
}


// TODO: redo this by having it loop through component pools, and not entitys
// right now, it's doing a lot of entirely unnecessary checks
// we can avoid those if we loop through the pools instead
void EntitySystem::WriteComponentUpdates( capnp::MessageBuilder& srBuilder, bool sFullUpdate )
{
	auto                                root = srBuilder.initRoot< NetMsgComponentUpdates >();

	// Make a list of component pools with a component that need updating
	std::vector< EntityComponentPool* > componentPools;
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

	auto   updateList = root.initUpdateList( componentPools.size() );

	size_t i          = 0;

	for ( auto pool : componentPools )
	{
		NetMsgComponentUpdate::Builder compUpdate = updateList[ i ];

		// If there are no components in existence, don't even bother to send anything here
		if ( !pool->aMapComponentToEntity.size() )
			continue;

		compUpdate.setName( pool->apName );

		auto   regData   = pool->GetRegistryData();
		auto   compList  = compUpdate.initComponents( pool->aMapComponentToEntity.size() );

		size_t compListI = 0;
		for ( auto& [ index, entity ] : pool->aMapComponentToEntity )
		{
			NetMsgComponentUpdate::Component::Builder compBuilder = compList[ compListI ];

			compBuilder.setId( entity );

			// Set Destroyed
			compBuilder.setDestroyed( pool->aComponentStates[ index ] == EEntityCreateState_Destroyed );
			
			if ( pool->aComponentStates[ index ] == EEntityCreateState_Destroyed || !regData->apWrite )
			{
				// Don't bother sending data if we're about to be destroyed or we have no write function
				compListI++;
				continue;
			}

			auto data = pool->GetData( entity );

			capnp::MallocMessageBuilder compMessageBuilder;
			if ( regData->apWrite( compMessageBuilder, data, ent_always_full_update ? true : sFullUpdate ) )
			{
				NetOutputStream outputStream;
				capnp::writePackedMessage( outputStream, compMessageBuilder );

				if ( Game_IsClient() && ent_show_component_net_updates )
				{
					gui->DebugMessage( "Sending Component Write Update to Clients: \"%s\" - %zd bytes", regData->apName, outputStream.aBuffer.size_bytes() );
				}

				auto valueBuilder = compBuilder.initValues( outputStream.aBuffer.size_bytes() );
				// std::copy( array.begin(), array.end(), valueBuilder.begin() );
				memcpy( valueBuilder.begin(), outputStream.aBuffer.begin(), outputStream.aBuffer.size_bytes() );
			}
			else
			{
				// compBuilder.initValues( 0 );
			}

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

		i++;
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


#define CH_NET_WRITE_VEC2( varFunc, var ) \
	if ( var.aIsDirty || sFullUpdate ) { \
		auto builderVar = builder.init##varFunc(); \
		NetHelper_WriteVec2( &builderVar, var ); \
	}

#define CH_NET_WRITE_VEC3( varFunc, var ) \
	if ( var.aIsDirty || sFullUpdate ) { \
		auto builderVar = builder.init##varFunc(); \
		NetHelper_WriteVec3( &builderVar, var ); \
	}

#define CH_NET_WRITE_VEC4( varFunc, var ) \
	if ( var.aIsDirty || sFullUpdate ) { \
		auto builderVar = builder.init##varFunc(); \
		NetHelper_WriteVec4( &builderVar, var ); \
	}


#define CH_NET_WRITE_VEC3_PTR( varFunc, var ) \
	if ( var.aIsDirty || sFullUpdate ) { \
		auto builderVar = builder->init##varFunc(); \
		NetHelper_WriteVec3( &builderVar, var ); \
	}


#define CH_VAR_DIRTY( var ) var.aIsDirty || sFullUpdate


// TODO: try this instead for all these
void TEMP_TransformRead( capnp::MessageReader& srReader, void* spData )
{
	CTransform* spTransform = static_cast< CTransform* >( spData );
	auto       message     = srReader.getRoot< NetCompTransform >();

	if ( message.hasPos() )
		NetHelper_ReadVec3( message.getPos(), spTransform->aPos.Edit() );

	if ( message.hasAng() )
		NetHelper_ReadVec3( message.getAng(), spTransform->aAng.Edit() );

	if ( message.hasScale() )
		NetHelper_ReadVec3( message.getScale(), spTransform->aScale.Edit() );
}

bool TEMP_TransformWrite( capnp::MessageBuilder& srMessage, const void* spData, bool sFullUpdate )
{
	const CTransform* spTransform = static_cast< const CTransform* >( spData );
	bool              isDirty     = sFullUpdate;

	isDirty |= spTransform->aPos.aIsDirty;
	isDirty |= spTransform->aAng.aIsDirty;
	isDirty |= spTransform->aScale.aIsDirty;

	if ( !isDirty )
		return false;

	auto builder = srMessage.initRoot< NetCompTransform >();

	if ( CH_VAR_DIRTY( spTransform->aPos ) )
	{
		auto pos = builder.initPos();
		NetHelper_WriteVec3( &pos, spTransform->aPos );
	}

	CH_NET_WRITE_VEC3( Ang, spTransform->aAng );
	CH_NET_WRITE_VEC3( Scale, spTransform->aScale );

	// if ( spTransform->aAng.aIsDirty )
	// {
	// 	auto ang = builder.initAng();
	// 	NetHelper_WriteVec3( &ang, spTransform->aAng );
	// }
	// 
	// if ( spTransform->aScale.aIsDirty )
	// {
	// 	auto scale = builder.initScale();
	// 	NetHelper_WriteVec3( &scale, spTransform->aScale );
	// }

	return true;
}


CH_COMPONENT_READ_DEF( CTransformSmall )
{
	CTransformSmall* spTransform = static_cast< CTransformSmall* >( spData );
	auto             message     = srReader.getRoot< NetCompTransformSmall >();

	if ( message.hasPos() )
		NetHelper_ReadVec3( message.getPos(), spTransform->aPos.Edit() );

	if ( message.hasAng() )
		NetHelper_ReadVec3( message.getAng(), spTransform->aAng.Edit() );
}

CH_COMPONENT_WRITE_DEF( CTransformSmall )
{
	const CTransformSmall* spTransform = static_cast< const CTransformSmall* >( spData );
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

	return true;
}


CH_COMPONENT_READ_DEF( CRigidBody )
{
	CRigidBody* spRigidBody = static_cast< CRigidBody* >( spData );
	auto        message     = srReader.getRoot< NetCompRigidBody >();

	if ( message.hasVel() )
		NetHelper_ReadVec3( message.getVel(), spRigidBody->aVel.Edit() );

	if ( message.hasAccel() )
		NetHelper_ReadVec3( message.getAccel(), spRigidBody->aAccel.Edit() );
}

CH_COMPONENT_WRITE_DEF( CRigidBody )
{
	const CRigidBody* spRigidBody = static_cast< const CRigidBody* >( spData );
	bool              isDirty     = sFullUpdate;

	isDirty |= spRigidBody->aVel.aIsDirty;
	isDirty |= spRigidBody->aAccel.aIsDirty;

	if ( !isDirty )
		return false;

	auto builder  = srMessage.initRoot< NetCompRigidBody >();

	CH_NET_WRITE_VEC3( Vel, spRigidBody->aVel );
	CH_NET_WRITE_VEC3( Accel, spRigidBody->aAccel );

	return true;
}

// TEMP, USE THESE PRIMARILY IN THE FUTURE
void NetComp_ReadDirection( const NetCompDirection::Reader& srReader, CDirection& srData )
{
	if ( srReader.hasForward() )
		NetHelper_ReadVec3( srReader.getForward(), srData.aForward.Edit() );

	if ( srReader.hasUp() )
		NetHelper_ReadVec3( srReader.getUp(), srData.aUp.Edit() );

	if ( srReader.hasRight() )
		NetHelper_ReadVec3( srReader.getRight(), srData.aRight.Edit() );
}

void NetComp_WriteDirection( NetCompDirection::Builder* builder, const CDirection& srData, bool sFullUpdate )
{
	CH_NET_WRITE_VEC3_PTR( Forward, srData.aForward );
	CH_NET_WRITE_VEC3_PTR( Up, srData.aUp );
	CH_NET_WRITE_VEC3_PTR( Right, srData.aRight );
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
	bool              isDirty     = sFullUpdate;

	isDirty |= spDirection->aForward.aIsDirty;
	isDirty |= spDirection->aRight.aIsDirty;
	isDirty |= spDirection->aUp.aIsDirty;

	if ( !isDirty )
		return false;

	auto builder = srMessage.initRoot< NetCompDirection >();
	NetComp_WriteDirection( &builder, *spDirection, sFullUpdate );

	return true;
}


CH_COMPONENT_READ_DEF( CCamera )
{
	CCamera* spCamera = static_cast< CCamera* >( spData );
	auto     message  = srReader.getRoot< NetCompCamera >();

	if ( message.hasDirection() )
		NetComp_ReadDirection( message.getDirection(), *spCamera );

	spCamera->aFov = message.getFov();

	if ( message.hasTransform() )
	{
		if ( message.getTransform().hasPos() )
			NetHelper_ReadVec3( message.getTransform().getPos(), spCamera->aTransform.Edit().aPos.Edit() );

		if ( message.getTransform().hasAng() )
			NetHelper_ReadVec3( message.getTransform().getAng(), spCamera->aTransform.Edit().aAng.Edit() );
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

	auto builder  = srMessage.initRoot< NetCompCamera >();

	if ( spCamera->aForward.aIsDirty || spCamera->aRight.aIsDirty || spCamera->aUp.aIsDirty || sFullUpdate )
	{
		auto dir = builder.initDirection();
		NetComp_WriteDirection( &dir, *spCamera, sFullUpdate );
	}

	// if ( spCamera->aFov.aIsDirty )
		builder.setFov( spCamera->aFov );

	if ( spCamera->aTransform.Get().aPos.aIsDirty || sFullUpdate )
	{
		Vec3::Builder pos = builder.getTransform().initPos();
		pos.setX( spCamera->aTransform.Get().aPos.Get().x );
		pos.setY( spCamera->aTransform.Get().aPos.Get().y );
		pos.setZ( spCamera->aTransform.Get().aPos.Get().z );
	}

	if ( spCamera->aTransform.Get().aAng.aIsDirty || sFullUpdate )
	{
		Vec3::Builder ang = builder.getTransform().initAng();
		ang.setX( spCamera->aTransform.Get().aAng.Get().x );
		ang.setY( spCamera->aTransform.Get().aAng.Get().y );
		ang.setZ( spCamera->aTransform.Get().aAng.Get().z );
	}

	return true;
}


CH_COMPONENT_READ_DEF( CGravity )
{
	CGravity* spGravity = static_cast< CGravity* >( spData );
	auto      message  = srReader.getRoot< NetCompGravity >();

	if ( message.hasForce() )
		NetHelper_ReadVec3( message.getForce(), spGravity->aForce.Edit() );
}

CH_COMPONENT_WRITE_DEF( CGravity )
{
	const CGravity* spGravity = static_cast< const CGravity* >( spData );
	bool            isDirty   = sFullUpdate;

	isDirty |= spGravity->aForce.aIsDirty;

	if ( !isDirty )
		return false;

	auto builder = srMessage.initRoot< NetCompGravity >();

	CH_NET_WRITE_VEC3( Force, spGravity->aForce );
	return true;
}


CH_COMPONENT_READ_DEF( CModelInfo )
{
	auto* spModelPath = static_cast< CModelInfo* >( spData );
	auto  message     = srReader.getRoot< NetCompModelPath >();

	spModelPath->aPath = message.getPath();
}

CH_COMPONENT_WRITE_DEF( CModelInfo )
{
	const auto* spModelInfo = static_cast< const CModelInfo* >( spData );

	bool        isDirty     = sFullUpdate;
	isDirty |= spModelInfo->aPath.aIsDirty;

	if ( !isDirty )
		return false;

	auto builder = srMessage.initRoot< NetCompModelPath >();

	builder.setPath( spModelInfo->aPath.Get() );

	return true;
}


CH_COMPONENT_READ_DEF( CLight )
{
	auto* spLight = static_cast< CLight* >( spData );
	auto  message = srReader.getRoot< NetCompLight >();

	if ( message.hasColor() )
		NetHelper_ReadVec4( message.getColor(), spLight->aColor.Edit() );

	if ( message.hasPos() )
		NetHelper_ReadVec3( message.getPos(), spLight->aPos.Edit() );

	if ( message.hasAng() )
		NetHelper_ReadVec3( message.getAng(), spLight->aAng.Edit() );

	spLight->aType     = static_cast< ELightType >( message.getType() );
	spLight->aInnerFov = message.getInnerFov();
	spLight->aOuterFov = message.getOuterFov();
	spLight->aRadius   = message.getRadius();
	spLight->aLength   = message.getLength();

	spLight->aShadow   = message.getShadow();
	spLight->aEnabled  = message.getEnabled();

	//ADJIAWDI)JDAIW)D 
	// Graphics_UpdateLight( spLight );
}


CH_COMPONENT_WRITE_DEF( CLight )
{
	auto* spLight = static_cast< const CLight* >( spData );

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

	return true;
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

