#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <type_traits>
#include <bitset>
#include <queue>
#include <set>
#include <memory>
#include <array>

#include "types/transform.h"
#include "iaudio.h"

#include "iaudio.h"


// Forward Declarations
namespace capnp 
{
	class MessageReader;
	class MessageBuilder;
}


// lot of help from this page
// https://austinmorlan.com/posts/entity_component_system/


using Entity                            = size_t;
using ComponentType                     = uint8_t;

constexpr Entity        CH_MAX_ENTITIES = 8192;
constexpr Entity        CH_ENT_INVALID  = SIZE_MAX;

constexpr ComponentType MAX_COMPONENTS  = 64;

// uh, idk
using Signature = std::bitset<MAX_COMPONENTS>;

// NEW entity component system

// ====================================================================================================
// Entity Component Database
// 
// A shared system between client and server that stores info on each component
// this will not talk to anything, only things to talk to it
// ====================================================================================================


// prototype component stuff, would work nice for a map editor and networking
// look at datatable stuff
//
// CH_REGISTER_COMPONENT( transform, Transform, NetCompTransform, Net_WriteTransform, Net_ReadTransform )
//		CH_COMP_VEC3( aAng, "ang" )
//		CH_COMP_VEC3( aPos, "pos" )
//		CH_COMP_VEC3( aScale, "scale" )
// CH_END_COMPONENT()
//
// CH_REGISTER_COMPONENT( player_zoom, CPlayerZoom, NetCompPlayerZoom, Net_WritePlayerZoom, Net_ReadPlayerZoom )
//		CH_COMP_FLOAT( aZoom, "zoom" )
// CH_END_COMPONENT()
// 

// template< typename T >
// using FEntComp_ReadFunc = void( T& srData );
// 
// template< typename T >
// using FEntComp_WriteFunc = void( capnp::MessageBuilder& srMessage, const T& srData );

using FEntComp_ReadFunc  = void( capnp::MessageReader& srReader, void* spData );
using FEntComp_WriteFunc = void( capnp::MessageBuilder& srMessage, const void* spData );

using FEntComp_VarCopy   = void( void* spSrc, size_t sOffset, void* spOut );


enum EEntComponentVarType
{
	EEntComponentVarType_Invalid,

	EEntComponentVarType_Float,
	EEntComponentVarType_Double,

	EEntComponentVarType_S8,    // signed char
	EEntComponentVarType_S16,   // signed short
	EEntComponentVarType_S32,   // signed int
	
	EEntComponentVarType_U8,    // unsigned char
	EEntComponentVarType_U16,   // unsigned short
	EEntComponentVarType_U32,   // unsigned int

	EEntComponentVarType_Vec2,  // glm::vec2
	EEntComponentVarType_Vec3,  // glm::vec3
	EEntComponentVarType_Vec4,  // glm::vec4

	EEntComponentVarType_Count,
};


// Var Data for a component
struct EntComponentVarData_t
{
	EEntComponentVarType aType;
	const char*          apVarName;
	const char*          apName;
	// size_t         aOffset;
};


// Data for a specific component
// template< typename T >
struct EntComponentData_t
{
	const char*                                         apName;

	// [Var Offset] = Var Data
	std::unordered_map< size_t, EntComponentVarData_t > aVars;
	// std::vector< EntComponentVarData_t > aVars;

	FEntComp_ReadFunc*                                  apRead;
	FEntComp_WriteFunc*                                 apWrite;
};


#if 0
// Maybe just make a function pointer
class IEntComponentVarHandler
{
  public:
	virtual ~IEntComponentVarHandler() = default;

	virtual void Copy( void* spSrc, size_t sOffset, void* spOut ) = 0;
	// virtual void Write( void* spSrc, size_t sOffset, void* spOut ) = 0;
};


class EntComponentVarHandler_Vec3 : public IEntComponentVarHandler
{
  public:
	virtual ~EntComponentVarHandler_Vec3() = default;

	virtual inline void Copy( void* spSrc, size_t sOffset, void* spOut ) override
	{
		glm::vec3* src = static_cast< glm::vec3* >( (void*)( ( (char*)spSrc ) + sOffset ) );
		glm::vec3* out = static_cast< glm::vec3* >( (void*)( ( (char*)spOut ) + sOffset ) );

		memcpy( out, src, sizeof( glm::vec3 ) );
	}

	// virtual inline void Write( void* spSrc, size_t sOffset, void* spOut ) override
	// {
	// 	glm::vec3* src = static_cast< glm::vec3* >( (void*)( ( (char*)spSrc ) + sOffset ) );
	// 	glm::vec3* out = static_cast< glm::vec3* >( (void*)( ( (char*)spOut ) + sOffset ) );
	// 
	// 	memcpy( out, src, sizeof( glm::vec3 ) );
	// }
};
#endif


struct EntComponentRegistry_t
{
	// [type hash of component] = Component Data
	std::unordered_map< size_t, EntComponentData_t > aComponents;

	// Component Name to Component Data
	std::unordered_map< std::string_view, EntComponentData_t* > aComponentNames;
};


extern EntComponentRegistry_t gEntComponentRegistry;

// void* EntComponentRegistry_Create( std::string_view sName );

void* EntComponentRegistry_GetVarHandler();


class IEntityComponentPool
{
  public:
	virtual ~IEntityComponentPool()                = default;

	// Called whenever an entity is destroyed on all Component Pools
	virtual void  EntityDestroyed( Entity entity ) = 0;

	// Adds This component to the entity
	virtual void* Create( Entity entity )          = 0;

	// Removes this component from the entity
	virtual void  Remove( Entity entity )          = 0;

	// Gets the data for this component
	virtual void* GetData( Entity entity )         = 0;
};


// AAAAAAAAAAAAAA THIS WONT WORK !!!!!
// when an entity system is created, it will need to make these component pools
// but they require a template argument for it's creation
// and the components are not registered in the entity system, so uh
template< typename T >
class EntityComponentPool : public IEntityComponentPool
{
  public:
	virtual ~EntityComponentPool()
	{
	}

	virtual void EntityDestroyed( Entity entity ) override
	{
		auto it = aMapEntityToComponent.find( entity );
		
		if ( it != aMapEntityToComponent.end() )
		{
			Remove( entity );
		}
	}

	// Adds This component to the entity
	virtual void* Create( Entity entity ) override
	{
		aMapComponentToEntity[ aCount ] = entity;
		aMapEntityToComponent[ entity ] = aCount;

		return &aComponents[ aCount++ ];
	}

	// Removes this component from the entity
	virtual void Remove( Entity entity ) override
	{
		auto it = aMapEntityToComponent.find( entity );

		if ( it == aMapEntityToComponent.end() )
		{
			Log_Error( "Failed to remove component from entity\n" );
		}

		size_t index = it->second;
		aMapComponentToEntity.erase( index );
		aMapEntityToComponent.erase( it );

		aCount--;
	}

	// Gets the data for this component
	virtual void* GetData( Entity entity ) override
	{
		auto it = aMapEntityToComponent.find( entity );

		if ( it != aMapEntityToComponent.end() )
		{
			size_t index = it->second;
			return &aComponents[ index ];
		}

		return nullptr;
	}

	// Map Component Index to Entity
	std::unordered_map< size_t, Entity > aMapComponentToEntity;

	// Map Entity to Component Index
	std::unordered_map< Entity, size_t > aMapEntityToComponent;

	// Memory Pool of Components
	// This is an std::array so that when a component is freed, it does not changes the index of each component
	std::array< T, CH_MAX_ENTITIES >     aComponents{};

	// Amount of Components we have allocated
	size_t                               aCount;
};


template< typename T >
inline void EntComp_RegisterComponent( const char* spName )
{
	size_t typeHash = typeid( T ).hash_code();
	auto   it       = gEntComponentRegistry.aComponents.find( typeHash );

	if ( it != gEntComponentRegistry.aComponents.end() )
	{
		Log_ErrorF( "Component already registered: \"%s\" - \"%s\"\n", typeid( T ).name(), spName );
		return;
	}

	EntComponentData_t& data                        = gEntComponentRegistry.aComponents[ typeHash ];
	data.apName                                     = spName;
	gEntComponentRegistry.aComponentNames[ spName ] = &data;
}


struct EntCompVarTypeToEnum_t
{
	size_t               aHashCode;
	EEntComponentVarType aType;
};



template< typename T, typename VAR_TYPE >
inline void EntComp_RegisterComponentVar( const char* spVarName, const char* spName, size_t sOffset )
{
	size_t typeHash = typeid( T ).hash_code();
	auto   it       = gEntComponentRegistry.aComponents.find( typeHash );

	if ( it == gEntComponentRegistry.aComponents.end() )
	{
		Log_ErrorF( "Component not registered, can't add var: \"%s\" - \"%s\"\n", typeid( T ).name() );
		return;
	}

	EntComponentData_t& data = it->second;
	auto                varFind = data.aVars.find( sOffset );

	if ( varFind != data.aVars.end() )
	{
		Log_ErrorF( "Component Var already registered: \"%s::%s\" - \"%s\"\n", typeid( T ).name(), typeid( VAR_TYPE ).name(), spName );
		return;
	}

	auto& varData     = data.aVars[ sOffset ];
	varData.apVarName = spVarName;
	varData.apName    = spName;

	// switch ( typeid( VAR_TYPE ).hash_code() )
	// {
	// 	case typeid( glm::vec2 ).hash_code():
	// 		varData.aType = EEntComponentVarType_Vec2;
	// 		break;
	// }
}


template< typename T >
// void EntComp_RegisterComponentReadWrite( FEntComp_ReadFunc< T >* spRead, FEntComp_WriteFunc< T >* spWrite )
inline void EntComp_RegisterComponentReadWrite( FEntComp_ReadFunc* spRead, FEntComp_WriteFunc* spWrite )
{
	size_t typeHash = typeid( T ).hash_code();
	auto   it       = gEntComponentRegistry.aComponents.find( typeHash );

	if ( it == gEntComponentRegistry.aComponents.end() )
	{
		Log_ErrorF( "Component not registered, can't add read and write function: \"%s\" - \"%s\"\n", typeid( T ).name() );
		return;
	}

	EntComponentData_t& data = it->second;
	data.apRead              = spRead;
	data.apWrite             = spWrite;
}


template< typename T >
inline void EntComp_RegisterVarHandler()
{
}


inline void EntComp_TestRegisterVarHandlers()
{

}


void Ent_RegisterBaseComponents();


// template< typename T >
// inline void EntComp_RegisterComponentRead( FEntComp_ReadFunc* spRead )
// {
// 	size_t typeHash = typeid( T ).hash_code();
// }
// 
// template< typename T >
// inline void EntComp_RegisterComponentWrite( FEntComp_WriteFunc* spWrite )
// {
// 	size_t typeHash = typeid( T ).hash_code();
// }


// ====================================================================================================
// Entity System
// ====================================================================================================


class EntitySystem
{
  public:
	static bool  CreateClient();
	static bool  CreateServer();

	static void  DestroyClient();
	static void  DestroyServer();

	bool         Init();
	void         Shutdown();

	void         CreateComponentPools();

	Entity       CreateEntity();
	void         DeleteEntity( Entity ent );
	void         DeleteEntityQueued( Entity ent );
	void         DeleteQueuedEntities();
	Entity       GetEntityCount();

	// kind of a hack, this just allocates the entity for the client
	// but what if a client only entity is using an ID that a newly created server entity is using?
	// curl up into a ball and die i guess
	Entity       CreateEntityFromServer( Entity desiredId );

	// Read and write from the network
	void         ReadEntityUpdates( capnp::MessageReader& srReader );
	void         WriteEntityUpdates( capnp::MessageBuilder& srBuilder );

	void         ReadComponents( Entity ent, capnp::MessageReader& srReader );
	void         WriteComponents( Entity ent, capnp::MessageBuilder& srBuilder );

	void         MarkComponentNetworked( Entity ent, const char* spName );

	// Add a component to an entity
	void*        AddComponent( Entity entity, const char* spName );

	// Get a component from an entity
	void*        GetComponent( Entity entity, const char* spName );


#if 0
	// Add a component to an entity
	template< typename T >
	T& AddComponent( Entity ent, const char* spName )
	{
		auto pool = GetComponentPool< T >();

		void* data = EntComponentRegistry_Create( spName );

		if ( data == nullptr )
		{
			Log_FatalF( "Failed to create component: \"%s\"\n", spName );
		}
	}

	// Add a component to an entity
	template< typename T >
	T& AddComponent( Entity ent )
	{
	}
	
	template< typename T >
	void AddComponent( Entity entity, T component )
	{
	}

	template< typename T >
	T& GetComponent( Entity ent )
	{
	}

	// template< typename T >
	// u8 GetComponentCount( Entity ent )
	// {
	// 	return 0;
	// }
	// 
	// inline u8 GetComponentCount( Entity ent, const char* spName )
	// {
	// 	return 0;
	// }

	template< typename T >
	EntityComponentPool< T >& GetComponentPool()
	{
		size_t typeHash = typeid( T ).hash_code();

		auto   it       = aComponentPools.find( typeHash );

		if ( it == aComponentPools.end() )
			Log_FatalF( "Component not registered before use: \"%s\"\n", typeid( T ).name() );

		return (EntityComponentPool< T >)it->second;
	}
#endif

	IEntityComponentPool* GetComponentPool( const char* spName )
	{
		auto it = aComponentPoolsStr.find( spName );

		if ( it == aComponentPoolsStr.end() )
			Log_FatalF( "Component not registered before use: \"%s\"\n", spName );

		return it->second;
	}

	// Queue of unused entity IDs
	// TODO: CHANGE BACK TO QUEUE
	// std::queue< Entity >                               aEntityPool{};
	std::vector< Entity >                                         aEntityPool{};

	// Entities queued to delete later
	std::vector< Entity >                                         aDeleteEntities{};

	// Array of signatures where the index corresponds to the entity ID
	// std::array< Signature, CH_MAX_ENTITIES > aSignatures{};

	// Total living entities - used to keep limits on how many exist
	Entity                                                        aEntityCount = 0;

	// Component Array - list of all of this type of component in existence
	std::unordered_map< size_t, IEntityComponentPool* >           aComponentPools;
	std::unordered_map< std::string_view, IEntityComponentPool* > aComponentPoolsStr;
};


EntitySystem* GetEntitySystem();


// ====================================================================================================
// OLD entity component system

// The one instance of virtual inheritance in the entire implementation.
// An interface is needed so that the ComponentManager (seen later)
// can tell a generic ComponentArray that an entity has been destroyed
// and that it needs to update its array mappings.

/*
The virtual inheritance of IComponentArray is unfortunate but, as far as I can tell, unavoidable.
As seen later, we'll have a list of every ComponentArray (one per component type),
and we need to notify all of them when an entity is destroyed so that it can remove the entity's data if it exists.
The only way to keep a list of multiple templated types is to keep a list
of their common interface so that we can call EntityDestroyed() on all of them.

Another method is to use events,
so that every ComponentArray can subscribe to an Entity Destroyed event and then respond accordingly.
This was my original approach but I decided to keep ComponentArrays relatively stupid.

Yet another method would be to use some fancy template magic and reflection,
but I wanted to keep it as simple as possibe for my own sanity.
The cost of calling the virtual function EntityDestroyed()
should be minimal because it isn't something that happens every single frame.
*/

#if 0

class System
{
public:
	std::set<Entity> aEntities;
};


class IComponentArray
{
public:
	virtual ~IComponentArray() = default;
	virtual void EntityDestroyed( Entity entity ) = 0;
};


template<typename T>
class ComponentArray : public IComponentArray
{
public:
	void InsertData( Entity entity, T component )
	{
		assert( aEntityToIndexMap.find(entity) == aEntityToIndexMap.end() && "Component added to same entity more than once." );

		// Put new entry at end and update the maps
		size_t newIndex = aSize;
		aEntityToIndexMap[entity] = newIndex;
		aIndexToEntityMap[newIndex] = entity;
		aComponentArray[newIndex] = component;
		++aSize;
	}

	void RemoveData( Entity entity )
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

	T& GetData( Entity entity )
	{
		auto it = aEntityToIndexMap.find( entity );

		if ( it == aEntityToIndexMap.end() )
			Log_FatalF( "Retrieving non-existent component: \"%s\"\n", typeid( T ).name() );

		// Return a reference to the entity's component
		return aComponentArray[it->second];


		// assert( aEntityToIndexMap.find(entity) != aEntityToIndexMap.end() && "Retrieving non-existent component." );
		// 
		// // Return a reference to the entity's component
		// return aComponentArray[aEntityToIndexMap[entity]];
	}

	void EntityDestroyed( Entity entity ) override
	{
		if ( aEntityToIndexMap.find(entity) != aEntityToIndexMap.end() )
		{
			// Remove the entity's component if it existed
			RemoveData(entity);
		}
	}

private:
	// The packed array of components (of generic type T),
	// set to a specified maximum amount, matching the maximum number
	// of entities allowed to exist simultaneously, so that each entity
	// has a unique spot.
	std::array<T, MAX_ENTITIES> aComponentArray{};

	// Map from an entity ID to an array index.
	std::unordered_map<Entity, size_t> aEntityToIndexMap{};

	// Map from an array index to an entity ID.
	std::unordered_map<size_t, Entity> aIndexToEntityMap{};

	// Total size of valid entries in the array.
	size_t aSize = 0;

	// Hash of data type
	// size_t aTypeHash = 0;
};


class EntityManager
{
public:
	int                         Init();

	Entity                      CreateEntity();
	void                        DeleteEntity( Entity ent );

	void                        SetSignature( Entity ent, Signature sig );
	Signature                   GetSignature( Entity ent );
	void                        EntitySignatureChanged( Entity ent, Signature entSig );

	template<typename T>
	void RegisterComponent()
	{
		size_t typeHash = typeid(T).hash_code();

		assert(aComponentTypes.find(typeHash) == aComponentTypes.end() && "Registering component type more than once.");

		// Add this component type to the component type map
		aComponentTypes.insert({typeHash, aNextComponentType});

		// Create a ComponentArray pointer and add it to the component arrays map
		aComponentArrays.insert({typeHash, new ComponentArray<T>});

		// Increment the value so that the next component registered will be different
		++aNextComponentType;
	}

	template<typename T>
	ComponentType GetComponentType()
	{
		size_t typeHash = typeid(T).hash_code();

		auto it = aComponentTypes.find( typeHash );

		if ( it == aComponentTypes.end() )
			Log_FatalF( "Component not registered before use: \"%s\"\n", typeid(T).name() );

		// Return this component's type - used for creating signatures
		return it->second;


		// size_t typeHash = typeid(T).hash_code();
		// 
		// assert(aComponentTypes.find(typeHash) != aComponentTypes.end() && "Component not registered before use.");
		// 
		// return aComponentTypes[typeHash];
	}

	template<typename T>
	void AddComponent( Entity entity, T component )
	{
		// Add a component to the array for an entity
		GetComponentArray<T>()->InsertData( entity, component );

		auto signature = GetSignature( entity );
		signature.set( GetComponentType<T>(), true );

		SetSignature( entity, signature );
		EntitySignatureChanged( entity, signature );
	}

	template<typename T>
	T& AddComponent( Entity entity )
	{
		// Add a component to the array for an entity
		GetComponentArray<T>()->InsertData( entity, T{} );

		auto signature = GetSignature( entity );
		signature.set( GetComponentType<T>(), true );

		SetSignature( entity, signature );
		EntitySignatureChanged( entity, signature );

		return GetComponentArray<T>()->GetData( entity );
	}

	template<typename T>
	void RemoveComponent( Entity entity )
	{
		// Remove a component from the array for an entity
		GetComponentArray<T>()->RemoveData( entity );

		auto signature = GetSignature( entity );
		signature.set( GetComponentType<T>(), false );

		SetSignature( entity, signature );
		EntitySignatureChanged( entity, signature );
	}

	template<typename T>
	T& GetComponent( Entity entity )
	{
		// Get a reference to a component from the array for an entity
		return GetComponentArray<T>()->GetData( entity );
	}

	/*
	
	Maintain a record of registered systems and their signatures.
	When a system is registered, it’s added to a map with the same typeid(T).name() trick used for the components.
	That same key is used for a map of system pointers as well.

	As with components, this approach requires a call to RegisterSystem() for every additional system type added to the game.

	Each system needs to have a signature set for it so that the manager can add appropriate entities to each systems’s list of entities.
	When an entity’s signature has changed (due to components being added or removed),
	then the system’s list of entities that it’s tracking needs to be updated.

	If an entity that the system is tracking is destroyed, then it also needs to update its list.

	*/

	template<typename T>
	T* RegisterSystem()
	{
		size_t typeHash = typeid(T).hash_code();

		assert(aSystems.find(typeHash) == aSystems.end() && "Registering system more than once.");

		// Create a pointer to the system and return it so it can be used externally
		auto system = new T;
		aSystems.insert({typeHash, system});
		return system;
	}

	template<typename T>
	void SetSystemSignature( Signature signature )
	{
		size_t typeHash = typeid(T).hash_code();

		assert(aSystems.find(typeHash) != aSystems.end() && "System used before registered.");

		// Set the signature for this system
		aSystemSignatures.insert({typeHash, signature});
	}

private:
	// Queue of unused entity IDs
	std::queue<Entity> aEntityPool{};

	// Array of signatures where the index corresponds to the entity ID
	std::array<Signature, MAX_ENTITIES> aSignatures{};

	// Total living entities - used to keep limits on how many exist
	Entity aEntityCount = 0;

private:
	// Map from type hash value to a component type
	std::unordered_map<size_t, ComponentType> aComponentTypes{};

	// Map from type hash value to a component array
	std::unordered_map<size_t, IComponentArray*> aComponentArrays{};

	// The component type to be assigned to the next registered component - starting at 0
	ComponentType aNextComponentType = 0;

	// Convenience function to get the statically casted pointer to the ComponentArray of type T.
	template<typename T>
	ComponentArray<T>* GetComponentArray()
	{
		size_t typeHash = typeid(T).hash_code();

		auto it = aComponentArrays.find( typeHash );

		if ( it == aComponentArrays.end() )
			Log_FatalF( "Component not registered before use: \"%s\"\n", typeid(T).name() );

		return (ComponentArray<T>*)it->second;
	}

private:
	// Map from system type string pointer to a signature
	std::unordered_map<size_t, Signature> aSystemSignatures{};

	// Map from system type string pointer to a system pointer
	std::unordered_map<size_t, System*> aSystems{};
};

#endif


// ==========================================
// Some Base Types here for now


struct CRigidBody
{
	glm::vec3 aVel = {};
	glm::vec3 aAccel = {};
};


struct CGravity
{
	glm::vec3 aForce = {};
};


// Direction Vectors
struct CDirection
{
	glm::vec3 aForward = {};
	glm::vec3 aUp = {};
	glm::vec3 aRight = {};
};


struct CCamera: public CDirection
{
	TransformSmall aTransform = {};
	float          aFov = 90.f;
};


struct CSound
{
	Handle aStream = InvalidHandle;
};


struct CRenderable_t
{
	Handle aHandle;
};


struct CModelPath
{
	std::string aPath;
};


// convinence
// inline auto &GetDirection( Entity ent ) { return GetEntitySystem()->GetComponent< CDirection >( ent ); }


// Helper Macros
#define CH_REGISTER_COMPONENT( type, name ) \
  EntComp_RegisterComponent< type >( #name )

#define CH_REGISTER_COMPONENT_VAR( type, varType, varName, varStr ) \
  EntComp_RegisterComponentVar< type, varType >( #varName, #varStr, offsetof( type, varName ) )

#define CH_REGISTER_COMPONENT_RW_EX( type, read, write ) \
  EntComp_RegisterComponentReadWrite< type >( read, write )

#define CH_COMPONENT_READ_DEF( type ) \
  static void __EntCompFunc_Read_##type( capnp::MessageReader& srReader, void* spData )

#define CH_COMPONENT_WRITE_DEF( type ) \
  static void __EntCompFunc_Write_##type( capnp::MessageBuilder& srMessage, const void* spData )

#define CH_COMPONENT_RW( type )    __EntCompFunc_Read_##type, __EntCompFunc_Write_##type
#define CH_COMPONENT_READ( type )  __EntCompFunc_Read_##type
#define CH_COMPONENT_WRITE( type ) __EntCompFunc_Write_##type

#define CH_REGISTER_COMPONENT_RW( type, name ) \
  CH_REGISTER_COMPONENT( type, name );  \
  EntComp_RegisterComponentReadWrite< type >( CH_COMPONENT_RW( type ) )


// Helper Macros for Registering Standard Var Types

#define CH_REGISTER_COMP_VAR_VEC3( type, varName, varStr ) \
  EntComp_RegisterComponentVar< type, glm::vec3 >( #varName, #varStr, offsetof( type, varName ) )

#define CH_REGISTER_COMPT_VAR_FL( type, varName, varStr ) \
  EntComp_RegisterComponentVar< type, float >( #varName, #varStr, offsetof( type, varName ) )

