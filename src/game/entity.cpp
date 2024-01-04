#include "main.h"
#include "game_shared.h"
#include "entity.h"
#include "world.h"
#include "util.h"
#include "player.h"  // TEMP - for CPlayerMoveData

#include "entity_systems.h"
#include "mapmanager.h"
#include "igui.h"

#include "game_physics.h"  // just for IPhysicsShape* and IPhysicsObject*


LOG_REGISTER_CHANNEL2( Entity, LogColor::DarkPurple );


EntitySystem* cl_entities = nullptr;
EntitySystem* sv_entities = nullptr;

extern Entity gLocalPlayer;

CONVAR( ent_show_translations, 0, "Show Entity ID Translations" );


// void* EntComponentRegistry_Create( std::string_view sName )
// {
// 	auto it = GetEntComponentRegistry().aComponentNames.find( sName );
// 
// 	if ( it == GetEntComponentRegistry().aComponentNames.end() )
// 	{
// 		return nullptr;
// 	}
// 
// 	EntComponentData_t* data = it->second;
// 	CH_ASSERT( data );
// 	return data->aCreate();
// }


EntCompVarTypeToEnum_t gEntCompVarToEnum[ EEntNetField_Count ] = {
	{ typeid( glm::vec3 ).hash_code(), EEntNetField_Vec3 },
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

	"Entity",
	"std::string",

	"glm::vec2",
	"glm::vec3",
	"glm::vec4",
	"glm::quat",

	"glm::vec3",
	"glm::vec4",

	"CUSTOM",
};


static_assert( CH_ARR_SIZE( gEntVarTypeStr ) == EEntNetField_Count );


const char* EntComp_VarTypeToStr( EEntNetField sVarType )
{
	if ( sVarType < 0 || sVarType > EEntNetField_Count )
		return gEntVarTypeStr[ EEntNetField_Invalid ];

	return gEntVarTypeStr[ sVarType ];
}


size_t EntComp_GetVarDirtyOffset( char* spData, EEntNetField sVarType )
{
	size_t offset = 0;
	switch ( sVarType )
	{
		default:
		case EEntNetField_Invalid:
			return 0;

		case EEntNetField_Bool:
			offset = sizeof( bool );
			break;

		case EEntNetField_Float:
			offset = sizeof( float );
			break;

		case EEntNetField_Double:
			offset = sizeof( double );
			break;


		case EEntNetField_S8:
			offset = sizeof( s8 );
			break;

		case EEntNetField_S16:
			offset = sizeof( s16 );
			break;

		case EEntNetField_S32:
			offset = sizeof( s32 );
			break;

		case EEntNetField_S64:
			offset = sizeof( s64 );
			break;


		case EEntNetField_U8:
			offset = sizeof( u8 );
			break;

		case EEntNetField_U16:
			offset = sizeof( u16 );
			break;

		case EEntNetField_U32:
			offset = sizeof( u32 );
			break;

		case EEntNetField_U64:
			offset = sizeof( u64 );
			break;


		case EEntNetField_Entity:
			offset = sizeof( Entity );
			break;

		case EEntNetField_StdString:
			offset = sizeof( std::string );
			break;

		case EEntNetField_Vec2:
			offset = sizeof( glm::vec2 );
			break;

		case EEntNetField_Color3:
		case EEntNetField_Vec3:
			offset = sizeof( glm::vec3 );
			break;

		case EEntNetField_Color4:
		case EEntNetField_Vec4:
			offset = sizeof( glm::vec4 );
			break;

		case EEntNetField_Quat:
			offset = sizeof( glm::quat );
			break;
	}

	return offset;
}


void EntComp_ResetVarDirty( char* spData, EEntNetField sVarType )
{
	size_t offset = EntComp_GetVarDirtyOffset( spData, sVarType );

	if ( offset )
	{
		bool* aIsDirty = reinterpret_cast< bool* >( spData + offset );
		*aIsDirty      = false;
	}
}


std::string EntComp_GetStrValueOfVar( void* spData, EEntNetField sVarType )
{
	switch ( sVarType )
	{
		default:
		{
			return "INVALID OR UNFINISHED";
		}
		case EEntNetField_Invalid:
		{
			return "INVALID";
		}
		case EEntNetField_Bool:
		{
			return *static_cast< bool* >( spData ) ? "TRUE" : "FALSE";
		}

		case EEntNetField_Float:
		{
			return ToString( *static_cast< float* >( spData ) );
		}
		case EEntNetField_Double:
		{
			return ToString( *static_cast< double* >( spData ) );
		}

		case EEntNetField_S8:
		{
			s8 value = *static_cast< s8* >( spData );
			return vstring( "%c", value );
		}
		case EEntNetField_S16:
		{
			s16 value = *static_cast< s16* >( spData );
			return vstring( "%d", value );
		}
		case EEntNetField_S32:
		{
			s32 value = *static_cast< s32* >( spData );
			return vstring( "%d", value );
		}
		case EEntNetField_S64:
		{
			s64 value = *static_cast< s64* >( spData );
			return vstring( "%lld", value );
		}

		case EEntNetField_U8:
		{
			u8 value = *static_cast< u8* >( spData );
			return vstring( "%uc", value );
		}
		case EEntNetField_U16:
		{
			u16 value = *static_cast< u16* >( spData );
			return vstring( "%ud", value );
		}
		case EEntNetField_U32:
		{
			u32 value = *static_cast< u32* >( spData );
			return vstring( "%ud", value );
		}
		case EEntNetField_U64:
		{
			u64 value = *static_cast< u64* >( spData );
			return vstring( "%zd", value );
		}

		case EEntNetField_Entity:
		{
			Entity value = *static_cast< Entity* >( spData );
			return vstring( "%zd", value );
		}
		case EEntNetField_StdString:
		{
			return *(const std::string*)spData;
		}

		case EEntNetField_Vec2:
		{
			const glm::vec2* value = (const glm::vec2*)spData;
			return vstring( "(%.4f, %.4f)", value->x, value->y );
		}
		case EEntNetField_Color3:
		case EEntNetField_Vec3:
		{
			return Vec2Str( *(const glm::vec3*)spData );
		}
		case EEntNetField_Color4:
		case EEntNetField_Vec4:
		{
			const glm::vec4* value = (const glm::vec4*)spData;
			return vstring( "(%.4f, %.4f, %.4f, %.4f)", value->x, value->y, value->z, value->w );
		}

		case EEntNetField_Quat:
		{
			const glm::quat* value = (const glm::quat*)spData;
			return vstring( "(%.4f, %.4f, %.4f, %.4f)", value->x, value->y, value->z, value->w );
		}
	}
}


std::string EntComp_GetStrValueOfVarOffset( size_t sOffset, void* spData, EEntNetField sVarType )
{
	char* data = static_cast< char* >( spData );
	return EntComp_GetStrValueOfVar( data + sOffset, sVarType );
}


void EntComp_AddRegisterCallback( EntitySystem* spSystem )
{
	if ( CH_IF_ASSERT_MSG( spSystem, "Trying to register nullptr for a Entity Component Register Callback\n" ) )
	{
		Log_Warn( gLC_Entity, "Trying to register nullptr for a Entity Component Register Callback\n" );
		return;
	}

	GetEntComponentRegistry().aCallbacks.push_back( spSystem );
}


void EntComp_RemoveRegisterCallback( EntitySystem* spSystem )
{
	size_t index = vec_index( GetEntComponentRegistry().aCallbacks, spSystem );
	if ( index != SIZE_MAX )
	{
		GetEntComponentRegistry().aCallbacks.erase( GetEntComponentRegistry().aCallbacks.begin() + index );
	}
	else
	{
		Log_Warn( gLC_Entity, "Trying to remove component register callback that was isn't registered\n" );
	}
}


void EntComp_RunRegisterCallbacks( const char* spName )
{
	for ( auto system : GetEntComponentRegistry().aCallbacks )
	{
		system->CreateComponentPool( spName );
	}
}


// ===================================================================================
// Entity System
// ===================================================================================


EntitySystem* GetEntitySystem()
{
	if ( Game_ProcessingClient() )
	{
		CH_ASSERT( cl_entities );
		return cl_entities;
	}

	CH_ASSERT( sv_entities );
	return sv_entities;
}


bool EntitySystem::CreateClient()
{
	CH_ASSERT( !cl_entities );
	if ( cl_entities )
		return false;

	cl_entities            = new EntitySystem;
	cl_entities->aIsClient = true;

	return cl_entities->Init();
}


bool EntitySystem::CreateServer()
{
	CH_ASSERT( !sv_entities );
	if ( sv_entities )
		return false;

	sv_entities            = new EntitySystem;
	sv_entities->aIsClient = false;

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
	PROF_SCOPE();

	aEntityPool.clear();
	aComponentPools.clear();
	aEntityIDConvert.clear();

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
	PROF_SCOPE();

	// Remove callback
	EntComp_RemoveRegisterCallback( this );

	// Mark all entities as destroyed
	for ( auto& [ entity, flags ] : aEntityFlags )
	{
		flags |= EEntityFlag_Destroyed;
	}

	// Destroy entities marked as destroyed
	DeleteQueuedEntities();

	// Tell all component pools every entity was destroyed
	// for ( Entity entity : aUsedEntities )
	// {
	// 	for ( auto& [ name, pool ] : aComponentPools )
	// 	{
	// 		pool->EntityDestroyed( entity );
	// 	}
	// }

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

	aEntityPool.clear();
	aComponentPools.clear();
	aEntityIDConvert.clear();
}


void EntitySystem::UpdateSystems()
{
	PROF_SCOPE();

	for ( auto& [ name, pool ] : aComponentPools )
	{
		if ( pool->apComponentSystem )
			pool->apComponentSystem->Update();
	}
}


void EntitySystem::UpdateStates()
{
	PROF_SCOPE();

	// Remove Components Queued for Deletion
	for ( auto& [ name, pool ] : aComponentPools )
	{
		pool->RemoveAllQueued();
	}

	DeleteQueuedEntities();

	for ( auto& [ id, flags ] : aEntityFlags )
	{
		// Remove the created flag if it has that
		if ( flags & EEntityFlag_Created )
			aEntityFlags[ id ] &= ~EEntityFlag_Created;
	}
}


void EntitySystem::InitCreatedComponents()
{
	PROF_SCOPE();

	// Remove Components Queued for Deletion
	for ( auto& [ name, pool ] : aComponentPools )
	{
		pool->InitCreatedComponents();
	}
}


void EntitySystem::CreateComponentPools()
{
	PROF_SCOPE();

	// iterate through all registered components and create a component pool for them
	for ( auto& [ name, componentData ] : GetEntComponentRegistry().aComponentNames )
	{
		CreateComponentPool( name.data() );
	}
}


void EntitySystem::CreateComponentPool( const char* spName )
{
	PROF_SCOPE();

	EntityComponentPool* pool = new EntityComponentPool;

	if ( !pool->Init( spName ) )
	{
		Log_ErrorF( gLC_Entity, "Failed to create component pool for \"%s\"", spName );
		delete pool;
		return;
	}
		
	aComponentPools[ spName ] = pool;

	// Create component system if it has one registered for it
	if ( !pool->apData->aFuncNewSystem )
		return;

	pool->apComponentSystem = pool->apData->aFuncNewSystem();

	if ( pool->apComponentSystem )
	{
		pool->apComponentSystem->apPool                                    = pool;
		aComponentSystems[ typeid( pool->apComponentSystem ).hash_code() ] = pool->apComponentSystem;
	}
	else
	{
		Log_ErrorF( gLC_Entity, "Failed to create component system for component \"%s\"\n", spName );
	}
}


IEntityComponentSystem* EntitySystem::GetComponentSystem( const char* spName )
{
	PROF_SCOPE();

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
	PROF_SCOPE();

	if ( CH_IF_ASSERT_MSG( GetEntityCount() < CH_MAX_ENTITIES, "Hit Entity Limit!" ) )
		return CH_ENT_INVALID;
	
	CH_ASSERT_MSG( GetEntityCount() + aEntityPool.size() == CH_MAX_ENTITIES, "Entity Count and Free Entities are out of sync!" );

	// Take an ID from the front of the queue
	// Entity id = aEntityPool.front();
	Entity id = aEntityPool.back();
	// aEntityPool.pop();
	aEntityPool.pop_back();

	// SANITY CHECK
	CH_ASSERT( !EntityExists( id ) );

	// Create Entity Flags for this and add the Created Flag to tit
	aEntityFlags[ id ] = EEntityFlag_Created;

	// If we want the entity to be local on the client or server, add that flag to it
	if ( sLocal )
		aEntityFlags[ id ] |= EEntityFlag_Local;

	Log_DevF( gLC_Entity, 2, "%s - Created Entity %zd\n", aIsClient ? "CLIENT" : "SERVER", id );

	return id;
}


void EntitySystem::DeleteEntity( Entity sEntity )
{
	PROF_SCOPE();

	CH_ASSERT_MSG( sEntity < CH_MAX_ENTITIES, "Entity out of range" );

	CH_ASSERT_MSG( GetEntityCount() + aEntityPool.size() == CH_MAX_ENTITIES, "Entity Count and Free Entities are out of sync!" );

	aEntityFlags[ sEntity ] |= EEntityFlag_Destroyed;
	Log_DevF( gLC_Entity, 2, "%s - Marked Entity to be Destroyed: %zd\n", aIsClient ? "CLIENT" : "SERVER", sEntity );

	// Get all children attached to this entity
	ChVector< Entity > children;
	GetChildrenRecurse( sEntity, children );

	// Mark all of them as destroyed
	for ( Entity child : children )
	{
		aEntityFlags[ child ] |= EEntityFlag_Destroyed;
		Log_DevF( gLC_Entity, 2, "%s - Marked Child Entity to be Destroyed (parent %zd): %zd\n", aIsClient ? "CLIENT" : "SERVER", sEntity, child );
	}
}


void EntitySystem::DeleteQueuedEntities()
{
	PROF_SCOPE();

	ChVector< Entity > deleteEntities;

	// well this sucks
	for ( auto& [ entity, flags ] : aEntityFlags )
	{
		// Check the entity's flags to see if it's marked as deleted
		if ( flags & EEntityFlag_Destroyed )
			deleteEntities.push_back( entity );
	}

	for ( auto entity : deleteEntities )
	{
		// Tell each Component Pool that this entity was destroyed
		for ( auto& [ name, pool ] : aComponentPools )
		{
			pool->EntityDestroyed( entity );
		}

		// Remove this entity from the translation list if it's in it
		for ( auto it = aEntityIDConvert.begin(); it != aEntityIDConvert.end(); it++ )
		{
			if ( it->second == entity )
			{
				aEntityIDConvert.erase( it );
				break;
			}
		}

		// Put the destroyed ID at the back of the queue
		
		// SANITY CHECK
		size_t sanityCheckPool = vec_index( aEntityPool, entity );
		CH_ASSERT( sanityCheckPool == SIZE_MAX );  // can't be in pool already

		// aEntityPool.push( ent );
		// aEntityPool.push_back( ent );
		aEntityPool.insert( aEntityPool.begin(), entity );

		aEntityFlags.erase( entity );

		Log_DevF( gLC_Entity, 2, "%s - Destroyed Entity %zd\n", aIsClient ? "CLIENT" : "SERVER", entity );
	}
}


Entity EntitySystem::GetEntityCount()
{
	return aEntityFlags.size();
}


bool EntitySystem::EntityExists( Entity desiredId )
{
	PROF_SCOPE();

	auto it = aEntityFlags.find( desiredId );

	// if the entity does not have any flags, it exists
	return it != aEntityFlags.end();
}


void EntitySystem::ParentEntity( Entity sSelf, Entity sParent )
{
	if ( sSelf == CH_ENT_INVALID || sSelf == sParent )
		return;

	if ( sParent == CH_ENT_INVALID )
	{
		// Clear the parent
		aEntityFlags[ sSelf ] |= EEntityFlag_Parented;
		aEntityParents.erase( sSelf );
	}
	else
	{
		aEntityParents[ sSelf ] = sParent;
		aEntityFlags[ sSelf ] &= ~EEntityFlag_Parented;
	}
}


Entity EntitySystem::GetParent( Entity sSelf )
{
	auto it = aEntityParents.find( sSelf );

	if ( it != aEntityParents.end() )
		return it->second;

	return CH_ENT_INVALID;
}


bool EntitySystem::IsParented( Entity sSelf )
{
	auto it = aEntityFlags.find( sSelf );

	if ( it != aEntityFlags.end() )
		return it->second & EEntityFlag_Parented;

	return false;
}


// Get the highest level parent for this entity, returns self if not parented
Entity EntitySystem::GetRootParent( Entity sSelf )
{
	PROF_SCOPE();

	auto it = aEntityParents.find( sSelf );

	if ( it != aEntityParents.end() )
		return GetRootParent( it->second );

	return sSelf;
}


// Recursively get all entities attached to this one (SLOW)
void EntitySystem::GetChildrenRecurse( Entity sEntity, ChVector< Entity >& srChildren )
{
	PROF_SCOPE();

#pragma message( "could probably speed up GetChildrenRecurse() by instead iterating through aEntityParents, and checking the value for each one" )

	for ( auto& [ otherEntity, flags ] : aEntityFlags )
	{
		if ( !( flags & EEntityFlag_Parented ) )
			continue;

		Entity otherParent = GetParent( otherEntity );

		if ( otherParent == sEntity )
		{
			srChildren.push_back( otherEntity );
			GetChildrenRecurse( otherEntity, srChildren );

#pragma message( "CHECK IF THIS BREAKS THE CODE" )
			break;
		}
	}
}


// Returns a Model Matrix with parents applied in world space IF we have a transform component
bool EntitySystem::GetWorldMatrix( glm::mat4& srMat, Entity sEntity )
{
	PROF_SCOPE();

	// Entity    parent = IsParented( sEntity ) ? GetParent( sEntity ) : CH_ENT_INVALID;
	Entity    parent = GetParent( sEntity );
	glm::mat4 parentMat( 1.f );

	if ( parent != CH_ENT_INVALID )
	{
		// Get the world matrix recursively
		GetWorldMatrix( parentMat, parent );
	}

	// Check if we have a transform component
	auto transform = static_cast< CTransform* >( GetComponent( sEntity, "transform" ) );

	if ( !transform )
	{
		// Fallback to the parent world matrix
		srMat = parentMat;
		return ( parent != CH_ENT_INVALID );
	}

	// is this all the wrong order?

	// NOTE: THIS IS PROBABLY WRONG
	srMat = glm::translate( transform->aPos.Get() );
	// srMat = glm::mat4( 1.f );

	glm::mat4 rotMat( 1.f );
	#define ROT_ANG( axis ) glm::vec3( rotMat[ 0 ][ axis ], rotMat[ 1 ][ axis ], rotMat[ 2 ][ axis ] )

	// glm::vec3 temp = glm::radians( transform->aAng.Get() );
	// glm::quat rotQuat = temp;

	#undef ROT_ANG

	// rotMat = glm::mat4_cast( rotQuat );

	// srMat *= rotMat;
	// srMat *= glm::eulerAngleYZX(
	//   glm::radians(transform->aAng.Get().x ),
	//   glm::radians(transform->aAng.Get().y ),
	//   glm::radians(transform->aAng.Get().z ) );
	
	srMat *= glm::eulerAngleZYX(
	  glm::radians( transform->aAng.Get()[ ROLL ] ),
	  glm::radians( transform->aAng.Get()[ YAW ] ),
	  glm::radians( transform->aAng.Get()[ PITCH ] ) );
	
	// srMat *= glm::yawPitchRoll(
	//   glm::radians( transform->aAng.Get()[ PITCH ] ),
	//   glm::radians( transform->aAng.Get()[ YAW ] ),
	//   glm::radians( transform->aAng.Get()[ ROLL ] ) );

	srMat = glm::scale( srMat, transform->aScale.Get() );

	srMat = parentMat * srMat;

	return true;
}


Transform EntitySystem::GetWorldTransform( Entity sEntity )
{
	PROF_SCOPE();

	Transform final{};

	glm::mat4 matrix;
	if ( !GetWorldMatrix( matrix, sEntity ) )
		return final;

	final.aPos   = Util_GetMatrixPosition( matrix );
	final.aAng   = glm::degrees( Util_GetMatrixAngles( matrix ) );
	final.aScale = Util_GetMatrixScale( matrix );

	return final;
}


// Add a component to an entity
void* EntitySystem::AddComponent( Entity entity, std::string_view sName )
{
	PROF_SCOPE();

	auto pool = GetComponentPool( sName );

	if ( pool == nullptr )
	{
		Log_ErrorF( gLC_Entity, "Failed to create component - no component pool found: \"%s\"\n", sName.data() );
		return nullptr;
	}

	return pool->Create( entity );
}


// Does this entity have this component?
bool EntitySystem::HasComponent( Entity entity, std::string_view sName )
{
	PROF_SCOPE();

	auto pool = GetComponentPool( sName );

	if ( pool == nullptr )
	{
		Log_ErrorF( gLC_Entity, "Failed to get component - no component pool found: \"%s\"\n", sName.data() );
		return false;
	}

	return pool->Contains( entity );
}


// Get a component from an entity
void* EntitySystem::GetComponent( Entity entity, std::string_view sName )
{
	PROF_SCOPE();

	auto pool = GetComponentPool( sName );

	if ( pool == nullptr )
	{
		Log_ErrorF( gLC_Entity, "Failed to get component - no component pool found: \"%s\"\n", sName.data() );
		return nullptr;
	}

	return pool->GetData( entity );
}


// Remove a component from an entity
void EntitySystem::RemoveComponent( Entity entity, std::string_view sName )
{
	PROF_SCOPE();

	auto pool = GetComponentPool( sName );

	if ( pool == nullptr )
	{
		Log_ErrorF( gLC_Entity, "Failed to remove component - no component pool found: \"%s\"\n", sName.data() );
		return;
	}

	pool->RemoveQueued( entity );
}


// Sets Prediction on this component
void EntitySystem::SetComponentPredicted( Entity entity, std::string_view sName, bool sPredicted )
{
	if ( !aIsClient )
	{
		// The server does not need to know if it's predicted, the client will have special handling for this
		Log_ErrorF( gLC_Entity, "Tried to mark entity component as predicted on server - \"%s\"\n", sName.data() );
		return;
	}

	auto pool = GetComponentPool( sName );

	if ( pool == nullptr )
	{
		Log_ErrorF( gLC_Entity, "Failed to set component prediction - no component pool found: \"%s\"\n", sName.data() );
		return;
	}

	pool->SetPredicted( entity, sPredicted );
}


// Is this component predicted for this Entity?
bool EntitySystem::IsComponentPredicted( Entity entity, std::string_view sName )
{
	if ( !aIsClient )
	{
		// The server does not need to know if it's predicted, the client will have special handling for this
		Log_ErrorF( gLC_Entity, "Tried to find a predicted entity component on server - \"%s\"\n", sName.data() );
		return false;
	}

	auto pool = GetComponentPool( sName );

	if ( pool == nullptr )
	{
		Log_FatalF( gLC_Entity, "Failed to get component prediction - no component pool found: \"%s\"\n", sName.data() );
		return false;
	}

	return pool->IsPredicted( entity );
}


// Enables/Disables Networking on this Entity
void EntitySystem::SetNetworked( Entity entity, bool sNetworked )
{
	auto it = aEntityFlags.find( entity );
	if ( it == aEntityFlags.end() )
	{
		Log_Error( gLC_Entity, "Failed to set Entity Networked State - Entity not found\n" );
		return;
	}

	if ( sNetworked )
		it->second |= EEntityFlag_Local;
	else
		it->second &= ~EEntityFlag_Local;
}


// Is this Entity Networked?
bool EntitySystem::IsNetworked( Entity sEntity )
{
	PROF_SCOPE();

	auto it = aEntityFlags.find( sEntity );
	if ( it == aEntityFlags.end() )
	{
		Log_Error( gLC_Entity, "Failed to get Entity Networked State - Entity not found\n" );
		return false;
	}

	// If we have the local flag, we aren't networked
	if ( it->second & EEntityFlag_Local )
		return false;

	// Check if we have a parent entity
	if ( !( it->second & EEntityFlag_Parented ) )
		return true;

	Entity parent = GetParent( sEntity );
	CH_ASSERT( parent != CH_ENT_INVALID );
	if ( parent == CH_ENT_INVALID )
		return true;

	// We make sure the parents are also networked before networking this one
	return IsNetworked( parent );
}


bool EntitySystem::IsNetworked( Entity sEntity, EEntityFlag sFlags )
{
	PROF_SCOPE();

	// If we have the local flag, we aren't networked
	if ( sFlags & EEntityFlag_Local )
		return false;

	// Check if we have a parent entity
	if ( !( sFlags & EEntityFlag_Parented ) )
		return true;

	Entity parent = GetParent( sEntity );
	CH_ASSERT( parent != CH_ENT_INVALID );
	if ( parent == CH_ENT_INVALID )
		return true;

	// We make sure the parents are also networked before networking this one
	return IsNetworked( parent );
}


void EntitySystem::SetAllowSavingToMap( Entity entity, bool sSaveToMap )
{
	auto it = aEntityFlags.find( entity );
	if ( it == aEntityFlags.end() )
	{
		Log_Error( gLC_Entity, "Failed to get Entity SaveToMap State - Entity not found\n" );
		return;
	}

	if ( sSaveToMap )
		it->second &= ~EEntityFlag_DontSaveToMap;
	else
		it->second |= EEntityFlag_DontSaveToMap;
}


bool EntitySystem::CanSaveToMap( Entity entity )
{
	PROF_SCOPE();

	auto it = aEntityFlags.find( entity );
	if ( it == aEntityFlags.end() )
	{
		Log_Error( gLC_Entity, "Failed to get Entity SaveToMap State - Entity not found\n" );
		return false;
	}

	if ( it->second & EEntityFlag_DontSaveToMap )
		return false;

	if ( it->second & EEntityFlag_Parented )
	{
		// Check if our parent can be saved to a map
		Entity parent = GetParent( entity );
		if ( parent != CH_ENT_INVALID )
			return CanSaveToMap( parent );
	}

	return true;
}


EntityComponentPool* EntitySystem::GetComponentPool( std::string_view spName )
{
	PROF_SCOPE();

	auto it = aComponentPools.find( spName );

	if ( it == aComponentPools.end() )
		Log_FatalF( gLC_Entity, "Component not registered before use: \"%s\"\n", spName.data() );

	return it->second;
}


Entity EntitySystem::TranslateEntityID( Entity sEntity, bool sCreate )
{
	PROF_SCOPE();

	if ( sEntity == CH_ENT_INVALID )
		return CH_ENT_INVALID;

	auto it = aEntityIDConvert.find( sEntity );
	if ( it != aEntityIDConvert.end() )
	{
		// Make sure it actually exists
		if ( !EntityExists( it->second ) )
		{
			Log_ErrorF( gLC_Entity, "Failed to find entity while translating Entity ID: %zd -> %zd\n", sEntity, it->second );
			// remove it from the list
			aEntityIDConvert.erase( it );
			return CH_ENT_INVALID;
		}

		if ( ent_show_translations.GetBool() )
			Log_DevF( gLC_Entity, 3, "Translating Entity ID %zd -> %zd\n", sEntity, it->second );

		return it->second;
	}

	if ( !sCreate )
	{
		Log_ErrorF( gLC_Entity, "Entity not in translation list: %zd\n", sEntity );
		return CH_ENT_INVALID;
	}

	Entity entity = CreateEntity();

	if ( entity == CH_ENT_INVALID )
	{
		Log_Warn( gLC_Entity, "Failed to create networked entity\n" );
		return CH_ENT_INVALID;
	}

	Log_DevF( gLC_Entity, 2, "Added Translation of Entity ID %zd -> %zd\n", sEntity, entity );
	aEntityIDConvert[ sEntity ] = entity;
	return entity;
}


// ===================================================================================
// Console Commands
// ===================================================================================


CONCMD( ent_dump_registry )
{
	LogGroup group = Log_GroupBegin( gLC_Entity );

	Log_GroupF( group, "Entity Count: %zd\n", GetEntitySystem()->GetEntityCount() );
	Log_GroupF( group, "Registered Components: %zd\n", GetEntComponentRegistry().aComponents.size() );

	for ( const auto& [ name, regData ] : GetEntComponentRegistry().aComponentNames )
	{
		CH_ASSERT( regData );

		if ( !regData )
			continue;

		Log_GroupF( group, "\nComponent: %s\n", regData->apName );
		Log_GroupF( group, "   Override Client: %s\n", ( regData->aFlags & ECompRegFlag_DontOverrideClient ) ? "TRUE" : "FALSE" );
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

	Log_GroupF( group, "Entity Count: %zd\n", GetEntitySystem()->GetEntityCount() );

	Log_GroupF( group, "Registered Components: %zd\n", GetEntComponentRegistry().aComponents.size() );

	for ( const auto& [ name, pool ] : GetEntitySystem()->aComponentPools )
	{
		CH_ASSERT( pool );

		if ( !pool )
			continue;

		Log_GroupF( group, "Component Pool: %s - %zd Components in Pool\n", name.data(), pool->GetCount() );
	}

	Log_GroupF( group, "Components: %zd\n", GetEntitySystem()->aComponentPools.size() );

	for ( auto& [ entity, flags ] : GetEntitySystem()->aEntityFlags )
	{
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
			if ( var.aType == EEntNetField_StdString )
			{
				// We have to iterate through all components and get the amount of memory used by each std::string
				// compOtherSize += 0;
			}
		}

		compSize = regData->aSize * pool->GetCount();
		curSize += compSize + compOtherSize;

		Log_GroupF( group, "Component Pool \"%s\": %zd bytes\n", pool->apName, curSize );
		Log_GroupF( group, "    %zd bytes per component * %zd components\n\n", regData->aSize, pool->GetCount() );

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

