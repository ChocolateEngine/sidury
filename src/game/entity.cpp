#include "entity.h"
#include "world.h"
#include "util.h"

#include "graphics/graphics.h"

#include "game_physics.h"  // just for IPhysicsShape* and IPhysicsObject*


EntityManager* entities = nullptr;

//ComponentFamily BaseComponent::aFamilyCounter = 0;


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
