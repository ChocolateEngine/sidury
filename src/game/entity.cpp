#include "main.h"
#include "game_shared.h"
#include "entity.h"
#include "world.h"
#include "util.h"
#include "player.h"  // TEMP - for CPlayerMoveData

#include "entity_systems.h"
#include "mapmanager.h"
#include "igui.h"

#include "graphics/graphics.h"

#include "game_physics.h"  // just for IPhysicsShape* and IPhysicsObject*


LOG_REGISTER_CHANNEL2( Entity, LogColor::DarkPurple );


EntitySystem*          cl_entities                                     = nullptr;
EntitySystem*          sv_entities                                     = nullptr;

extern Entity          gLocalPlayer;

CONVAR( ent_always_full_update, 0, "For debugging, always send a full update" );
CONVAR( ent_show_component_net_updates, 0, "Show Component Network Updates" );
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
// 	Assert( data );
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
	AssertMsg( spSystem, "Trying to register nullptr for a Entity Component Register Callback\n" );

	if ( !spSystem )
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
	aEntityCount = 0;
	aEntityPool.clear();
	aUsedEntities.clear();
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
	// Remove callback
	EntComp_RemoveRegisterCallback( this );

	// Mark all entities as destroyed
	for ( Entity entity : aUsedEntities )
	{
		aEntityFlags[ entity ] |= EEntityFlag_Destroyed;
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

	aEntityCount = 0;
	aEntityPool.clear();
	aUsedEntities.clear();
	aComponentPools.clear();
	aEntityIDConvert.clear();
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

	for ( auto& [ id, flags ] : aEntityFlags )
	{
		// Remove the created flag if it has that
		if ( flags & EEntityFlag_Created )
			aEntityFlags[ id ] &= ~EEntityFlag_Created;
	}
}


void EntitySystem::InitCreatedComponents()
{
	// Remove Components Queued for Deletion
	for ( auto& [ name, pool ] : aComponentPools )
	{
		pool->InitCreatedComponents();
	}
}


void EntitySystem::CreateComponentPools()
{
	// iterate through all registered components and create a component pool for them
	for ( auto& [ name, componentData ] : GetEntComponentRegistry().aComponentNames )
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
	CH_ASSERT_MSG( aEntityCount < CH_MAX_ENTITIES, "Hit Entity Limit!" );

	// Take an ID from the front of the queue
	// Entity id = aEntityPool.front();
	Entity id = aEntityPool.back();
	// aEntityPool.pop();
	aEntityPool.pop_back();
	++aEntityCount;

	// SANITY CHECK
	size_t findUsedIndex = vec_index( aUsedEntities, id );
	CH_ASSERT( findUsedIndex == SIZE_MAX );

	// Add this id to the used entity id list
	aUsedEntities.push_back( id );

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
	CH_ASSERT_MSG( sEntity < CH_MAX_ENTITIES, "Entity out of range" );

	aEntityFlags[ sEntity ] |= EEntityFlag_Destroyed;
	Log_DevF( gLC_Entity, 2, "%s - Marked Entity to be Destroyed: %zd\n", aIsClient ? "CLIENT" : "SERVER", sEntity );

	// Get all children attached to this entity
	std::set< Entity > children;
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
	for ( size_t i = 0; i < aUsedEntities.size(); )
	{
		Entity entity = aUsedEntities[ i ];

		// Check the entity's flags to see if it's marked as deleted
		if ( !( aEntityFlags[ entity ] & EEntityFlag_Destroyed ) )
		{
			i++;
			continue;
		}

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
		--aEntityCount;

		vec_remove( aUsedEntities, entity );

		aEntityFlags.erase( entity );

		Log_DevF( gLC_Entity, 2, "%s - Destroyed Entity %zd\n", aIsClient ? "CLIENT" : "SERVER", entity );
	}
}


Entity EntitySystem::GetEntityCount()
{
	return aEntityCount;
}


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


// Get the highest level parent for this entity, returns self if not parented
Entity EntitySystem::GetRootParent( Entity sSelf )
{
	auto it = aEntityParents.find( sSelf );

	if ( it != aEntityParents.end() )
		return GetRootParent( it->second );

	return sSelf;
}


// Recursively get all entities attached to this one (SLOW)
void EntitySystem::GetChildrenRecurse( Entity sEntity, std::set< Entity >& srChildren )
{
	for ( Entity otherEntity : aUsedEntities )
	{
		Entity otherParent = GetParent( otherEntity );

		if ( otherParent == sEntity )
		{
			srChildren.emplace( otherEntity );
			GetChildrenRecurse( otherEntity, srChildren );
		}
	}
}


// Returns a Model Matrix with parents applied in world space IF we have a transform component
bool EntitySystem::GetWorldMatrix( glm::mat4& srMat, Entity sEntity )
{
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
	Transform final{};

	glm::mat4 matrix;
	if ( !GetWorldMatrix( matrix, sEntity ) )
		return final;

	final.aPos   = Util_GetMatrixPosition( matrix );
	final.aAng   = glm::degrees( Util_GetMatrixAngles( matrix ) );
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

		Entity entId = entityUpdate->id();

		if ( entityUpdate->destroyed() )
		{
			Entity entity = TranslateEntityID( entId, false );
			if ( entity != CH_ENT_INVALID )
				DeleteEntity( entity );
			else
				Log_Error( gLC_Entity, "Trying to delete entity not in translation list\n" );

			continue;
		}
		else
		{
			Entity entity = TranslateEntityID( entId, true );

			if ( !EntityExists( entity ) )
			{
				Log_Error( gLC_Entity, "wtf entity in translation list doesn't actually exist?\n" );
				continue;
			}
			else
			{
				// Check for an entity parent
				Entity parent = TranslateEntityID( entityUpdate->parent(), true );

				if ( parent == CH_ENT_INVALID )
					continue;

				ParentEntity( entity, parent );
			}
		}
	}
}


// TODO: redo this by having it loop through component pools, and not entitys
// right now, it's doing a lot of entirely unnecessary checks
// we can avoid those if we loop through the pools instead
void EntitySystem::WriteEntityUpdates( flatbuffers::FlatBufferBuilder& srBuilder, bool sSavingMap )
{
	Assert( aEntityCount == aUsedEntities.size() );

	std::vector< NetMsg_EntityUpdateBuilder > updateBuilderList;
	std::vector< flatbuffers::Offset< NetMsg_EntityUpdate > > updateOut;

	for ( size_t i = 0; i < aEntityCount; i++ )
	{
		Entity      entity = aUsedEntities[ i ];
		EEntityFlag flags  = aEntityFlags[ entity ];

		// Make sure this and all the parents are networked
		if ( !IsNetworked( entity ) )
			continue;

		// Make sure this entity is allowed to be saved to the map
		if ( sSavingMap && !CanSaveToMap( entity ) )
			continue;

		auto& update = updateBuilderList.emplace_back( srBuilder );

		update.add_id( entity );

		// Get Entity State
		if ( flags & EEntityFlag_Destroyed )
			update.add_destroyed( true );

		// doesn't matter if it returns CH_ENT_INVALID
		update.add_parent( GetParent( entity ) );

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
			case EEntNetField_Invalid:
				continue;

			case EEntNetField_Bool:
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

		Assert( var.aType != EEntNetField_Invalid );
		Assert( var.aType != EEntNetField_Bool );

		switch ( var.aType )
		{
			default:
				break;

			case EEntNetField_Float:
			{
				auto value = (float*)( data );
				*value     = vector[ i++ ].AsFloat();
				break;
			}
			case EEntNetField_Double:
			{
				auto value = (double*)( data );
				*value     = vector[ i++ ].AsDouble();
				break;
			}

			case EEntNetField_S8:
			{
				auto value = (s8*)( data );
				*value     = vector[ i++ ].AsInt8();
				break;
			}
			case EEntNetField_S16:
			{
				auto value = (s16*)( data );
				*value     = vector[ i++ ].AsInt16();
				break;
			}
			case EEntNetField_S32:
			{
				auto value = (s32*)( data );
				*value     = vector[ i++ ].AsInt32();
				break;
			}
			case EEntNetField_S64:
			{
				auto value = (s64*)( data );
				*value     = vector[ i++ ].AsInt64();
				break;
			}

			case EEntNetField_U8:
			{
				auto value = (u8*)( data );
				*value     = vector[ i++ ].AsUInt8();
				break;
			}
			case EEntNetField_U16:
			{
				auto value = (u16*)( data );
				*value     = vector[ i++ ].AsUInt16();
				break;
			}
			case EEntNetField_U32:
			{
				auto value = (u32*)( data );
				*value     = vector[ i++ ].AsUInt32();
				break;
			}
			case EEntNetField_U64:
			{
				auto value = (u64*)( data );
				*value     = vector[ i++ ].AsUInt64();
				break;
			}

			case EEntNetField_Entity:
			{
				auto value      = (Entity*)( data );
				auto recvEntity = (Entity)( vector[ i++ ].AsUInt64() );

				if ( recvEntity == CH_ENT_INVALID )
				{
					*value = CH_ENT_INVALID;
					break;
				}

				Entity convertEntity = GetEntitySystem()->TranslateEntityID( recvEntity );

				if ( convertEntity == CH_ENT_INVALID )
				{
					Log_Error( gLC_Entity, "Can't find Networked Entity ID\n" );
				}
				else
				{
					*value = convertEntity;
				}

				break;
			}

			case EEntNetField_StdString:
			{
				auto value = (std::string*)( data );
				*value     = vector[ i++ ].AsString().str();
				break;
			}

				// Will have a special case for these once i have each value in a vecX marked dirty

			case EEntNetField_Vec2:
			{
				auto value = (glm::vec2*)( data );
				value->x   = vector[ i++ ].AsFloat();
				value->y   = vector[ i++ ].AsFloat();
				break;
			}
			case EEntNetField_Color3:
			case EEntNetField_Vec3:
			{
				auto value = (glm::vec3*)( data );
				value->x   = vector[ i++ ].AsFloat();
				value->y   = vector[ i++ ].AsFloat();
				value->z   = vector[ i++ ].AsFloat();
				break;
			}
			case EEntNetField_Color4:
			case EEntNetField_Vec4:
			{
				auto value = (glm::vec4*)( data );
				value->x   = vector[ i++ ].AsFloat();
				value->y   = vector[ i++ ].AsFloat();
				value->z   = vector[ i++ ].AsFloat();
				value->w   = vector[ i++ ].AsFloat();
				break;
			}

			case EEntNetField_Quat:
			{
				auto value = (glm::quat*)( data );
				value->x   = vector[ i++ ].AsFloat();
				value->y   = vector[ i++ ].AsFloat();
				value->z   = vector[ i++ ].AsFloat();
				value->w   = vector[ i++ ].AsFloat();
				break;
			}
		}
	}
}


bool WriteComponent( flexb::Builder& srBuilder, EntComponentData_t* spRegData, const void* spData, bool sFullUpdate, bool sSavingMap )
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
			// Make the var is allowed to be saved to the map
			if ( sSavingMap )
			{
				if ( !var.aSaveToMap )
				{
					srBuilder.Bool( false );
					return false;
				}
			}

			// We always write this if it's a full update
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
				srBuilder.Bool( false );
				break;

			case EEntNetField_Invalid:
				break;

			case EEntNetField_Bool:
			{
				auto value = *(bool*)( data );
				srBuilder.Bool( value );
				wroteData = true;
				break;
			}

			case EEntNetField_Float:
			{
				if ( IsVarDirty() )
				{
					auto value = *(float*)( data );
					srBuilder.Float( value );
				}
				
				break;
			}
			case EEntNetField_Double:
			{
				if ( IsVarDirty() )
				{
					auto value = *(double*)( data );
					srBuilder.Double( value );
				}
				
				break;
			}

			case EEntNetField_S8:
			{
				// FLEX BUFFERS STORES ALL INTS AND UINTS AS INT64 AND UINT64, WHAT A WASTE OF SPACE
				if ( IsVarDirty() )
				{
					auto value = *(s8*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntNetField_S16:
			{
				if ( IsVarDirty() )
				{
					auto value = *(s16*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntNetField_S32:
			{
				if ( IsVarDirty() )
				{
					auto value = *(s32*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntNetField_S64:
			{
				if ( IsVarDirty() )
				{
					auto value = *(s64*)( data );
					srBuilder.Add( value );
				}

				break;
			}

			case EEntNetField_U8:
			{
				// FLEX BUFFERS STORES ALL INTS AND UINTS AS INT64 AND UINT64, WHAT A WASTE OF SPACE
				if ( IsVarDirty() )
				{
					auto value = *(u8*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntNetField_U16:
			{
				if ( IsVarDirty() )
				{
					auto value = *(u16*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntNetField_U32:
			{
				if ( IsVarDirty() )
				{
					auto value = *(u32*)( data );
					srBuilder.Add( value );
				}

				break;
			}
			case EEntNetField_U64:
			{
				if ( IsVarDirty() )
				{
					auto value = *(u64*)( data );
					srBuilder.Add( value );
				}

				break;
			}

			case EEntNetField_Entity:
			{
				if ( IsVarDirty() )
				{
					auto value = *(Entity*)( data );
					srBuilder.Add( value );
				}

				break;
			}

			case EEntNetField_StdString:
			{
				if ( IsVarDirty() )
				{
					auto value = *(const std::string*)( data );
					srBuilder.Add( value );
				}

				break;
			}

			// Will have a special case for these once i have each value in a vecX marked dirty

			case EEntNetField_Vec2:
			{
				if ( IsVarDirty() )
				{
					const glm::vec2* value = (const glm::vec2*)( data );
					srBuilder.Add( value->x );
					srBuilder.Add( value->y );
				}

				break;
			}
			case EEntNetField_Color3:
			case EEntNetField_Vec3:
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
			case EEntNetField_Color4:
			case EEntNetField_Vec4:
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

			case EEntNetField_Quat:
			{
				if ( IsVarDirty() )
				{
					const glm::quat* value = (const glm::quat*)( data );
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


// TODO: redo this by having it loop through component pools, and not entitys
// right now, it's doing a lot of entirely unnecessary checks
// we can avoid those if we loop through the pools instead
void EntitySystem::WriteComponentUpdates( fb::FlatBufferBuilder& srRootBuilder, bool sFullUpdate, bool sSavingMap )
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
			EEntityFlag entFlags            = aEntityFlags[ entity ];
			EEntityFlag compFlags           = pool->aComponentFlags[ index ];

			bool        shouldSkipComponent = false;

			// check if the entity itself will be destroyed
			// shouldSkipComponent |= ( (entFlags & EEntityFlag_Destroyed) && !sFullUpdate );
			shouldSkipComponent |= entFlags & EEntityFlag_Destroyed;

			if ( sSavingMap )
			{
				shouldSkipComponent |= compFlags & EEntityFlag_DontSaveToMap;
				shouldSkipComponent |= !CanSaveToMap( entity );
				shouldSkipComponent |= !regData->aSaveToMap;
			}
			else
			{
				// check if the entity isn't networked
				shouldSkipComponent |= !IsNetworked( entity );
				shouldSkipComponent |= compFlags & EEntityFlag_Local;
			}

			// Have we determined we should skip this component?
			if ( shouldSkipComponent )
			{
				// Don't bother sending data if we're about to be destroyed or we have no write function
				compListI++;
				continue;
			}

			void*          data = pool->GetData( entity );

			// Write Component Data
			flexb::Builder flexBuilder;
			wroteData = WriteComponent( flexBuilder, regData, data, ent_always_full_update ? true : sFullUpdate, sSavingMap );

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
			if ( compFlags & EEntityFlag_Destroyed )
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
			compUpdate.add_hash( regData->aHash );

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

		for ( auto& [entity, compIndex] : pool->aMapEntityToComponent )
		{
			void* componentData = pool->aComponents[ compIndex.aIndex ];

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

		if ( componentUpdate->hash() != regData->aHash )
		{
			Log_ErrorF( "Hash of component \"%s\" differs from what we have (got %zd, expected %zd)\n",
				componentName, componentUpdate->hash(), regData->aHash );
			continue;
		}

		// Tell the Component System this entity's component updated
		IEntityComponentSystem* system = GetComponentSystem( regData->apName );

		for ( size_t c = 0; c < componentUpdate->components()->size(); c++ )
		{
			const NetMsg_ComponentUpdateData* componentUpdateData = componentUpdate->components()->Get( c );

			if ( !componentUpdateData )
				continue;

			// Is this entity in the translation system?
			Entity entity = TranslateEntityID( componentUpdateData->id(), false );
			if ( entity == CH_ENT_INVALID )
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

			// a bit of a hack and not implemented properly
			bool predicted = IsComponentPredicted( entity, componentName );
			if ( predicted )
				continue;

			if ( componentUpdateData->values() )
			{
				auto             values   = componentUpdateData->values();
				flexb::Reference flexRoot = flexb::GetRoot( values->data(), values->size() );

				ReadComponent( flexRoot, regData, componentData );
				// regData->apRead( componentVerifier, values->data(), componentData );

				Log_DevF( gLC_Entity, 3, "Parsed component data for entity \"%zd\" - \"%s\"\n", entity, componentName );

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
bool EntitySystem::IsNetworked( Entity entity )
{
	auto it = aEntityFlags.find( entity );
	if ( it == aEntityFlags.end() )
	{
		Log_Error( gLC_Entity, "Failed to get Entity Networked State - Entity not found\n" );
		return false;
	}

	// If we have the local flag, we aren't networked
	if ( it->second & EEntityFlag_Local )
		return false;

	// Check if we have a parent entity
	Entity parent = GetParent( entity );
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
	auto it = aEntityFlags.find( entity );
	if ( it == aEntityFlags.end() )
	{
		Log_Error( gLC_Entity, "Failed to get Entity SaveToMap State - Entity not found\n" );
		return false;
	}

	if ( it->second & EEntityFlag_DontSaveToMap )
		return false;

	// Check if our parent can be saved to a map
	Entity parent = GetParent( entity );
	if ( parent != CH_ENT_INVALID )
		return CanSaveToMap( parent );

	return true;
}


EntityComponentPool* EntitySystem::GetComponentPool( const char* spName )
{
	auto it = aComponentPools.find( spName );

	if ( it == aComponentPools.end() )
		Log_FatalF( gLC_Entity, "Component not registered before use: \"%s\"\n", spName );

	return it->second;
}


Entity EntitySystem::TranslateEntityID( Entity sEntity, bool sCreate )
{
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
			Log_DevF( gLC_Entity, 2, "Translating Entity ID %zd -> %zd\n", sEntity, it->second );

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

	Log_GroupF( group, "Entity Count: %zd\n", GetEntitySystem()->aEntityCount );
	Log_GroupF( group, "Registered Components: %zd\n", GetEntComponentRegistry().aComponents.size() );

	for ( const auto& [ name, regData ] : GetEntComponentRegistry().aComponentNames )
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

	Log_GroupF( group, "Registered Components: %zd\n", GetEntComponentRegistry().aComponents.size() );

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

