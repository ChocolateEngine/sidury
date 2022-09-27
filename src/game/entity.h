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


// lot of help from this page
// https://austinmorlan.com/posts/entity_component_system/


using Entity = size_t;
using ComponentType = uint8_t;

constexpr Entity MAX_ENTITIES = 2048;  // not sure how much an ECS would use
constexpr Entity ENT_INVALID = SIZE_MAX;

constexpr ComponentType MAX_COMPONENTS = 32;

// uh, idk
using Signature = std::bitset<MAX_COMPONENTS>;


class System
{
public:
	std::set<Entity> aEntities;
};

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


extern EntityManager* entities;


// uh how will this work when networked? lol
static EntityManager& GetEntityManager()
{
	static EntityManager entityManager;
	return entityManager;
}


// ==========================================
// Convienence Macros

#define GET_COMPONENT( var, type, ent ) var = entities->GetComponent< type >( ent )

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


class CSound
{
public:
	Handle aStream = InvalidHandle;
};


// convinence
inline auto &GetDirection( Entity ent ) { return entities->GetComponent< CDirection >( ent ); }


