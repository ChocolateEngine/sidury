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

// AAA
#include "graphics/graphics.h"

#include "iaudio.h"


// Forward Declarations
namespace capnp 
{
	class MessageReader;
	class MessageBuilder;
}

template< typename T >
struct ComponentNetVar;

class EntitySystem;
class IEntityComponentSystem;

using Entity                            = size_t;
using ComponentType                     = uint8_t;

constexpr Entity        CH_MAX_ENTITIES = 8192;
constexpr Entity        CH_ENT_INVALID  = SIZE_MAX;

constexpr ComponentType MAX_COMPONENTS  = 64;

#define COMPONENT_POOLS 1
#define COMPONENT_POOLS_TEMP 1

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

// Functions for creating and freeing components
using FEntComp_New       = std::function< void*() >;
using FEntComp_Free      = std::function< void( void* spData ) >;

// Functions for creating component systems
using FEntComp_NewSys    = std::function< IEntityComponentSystem*() >;

// Functions for serializing and deserializing Components with Cap'n Proto
using FEntComp_ReadFunc  = void( capnp::MessageReader& srReader, void* spData );
using FEntComp_WriteFunc = bool( capnp::MessageBuilder& srMessage, const void* spData, bool sFullUpdate );

// Callback Function for when a new component is registered at runtime
// Used for creating a new component pool for client and/or server entity system
using FEntComp_Register  = void( const char* spName );

using FEntComp_VarCopy   = void( void* spSrc, size_t sOffset, void* spOut );


enum EEntComponentVarType : u8
{
	EEntComponentVarType_Invalid,

	EEntComponentVarType_Bool,

	EEntComponentVarType_Float,
	EEntComponentVarType_Double,

	EEntComponentVarType_S8,    // signed char
	EEntComponentVarType_S16,   // signed short
	EEntComponentVarType_S32,   // signed int
	EEntComponentVarType_S64,   // signed long long
	
	EEntComponentVarType_U8,    // unsigned char
	EEntComponentVarType_U16,   // unsigned short
	EEntComponentVarType_U32,   // unsigned int
	EEntComponentVarType_U64,   // unsigned long long

	EEntComponentVarType_StdString,  // std::string

	EEntComponentVarType_Vec2,  // glm::vec2
	EEntComponentVarType_Vec3,  // glm::vec3
	EEntComponentVarType_Vec4,  // glm::vec4

	EEntComponentVarType_Count,
};


enum EEntComponentNetType
{
	// This component is never networked
	EEntComponentNetType_None,

	// This component is on both client and server
	EEntComponentNetType_Both,

	// This component is only on the client
	EEntComponentNetType_Client,

	// This component is only on the server
	EEntComponentNetType_Server,
};


enum EEntityCreateState
{
	// Entity still exists
	EEntityCreateState_None,

	// Entity was just created
	EEntityCreateState_Created,

	// Entity was destroyed and is queued for removal
	EEntityCreateState_Destroyed,
};


// Var Data for a component
struct EntComponentVarData_t
{
	EEntComponentVarType aType;
	bool                 aIsNetVar;
	size_t               aSize;
	const char*          apVarName;
	const char*          apName;
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
	
	bool                                                aOverrideClient;  // probably temporary until prediction
	EEntComponentNetType                                aNetType;

	FEntComp_New                                        aFuncNew;
	FEntComp_Free                                       aFuncFree;

	FEntComp_NewSys                                     aFuncNewSystem;
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
	std::unordered_map< size_t, EntComponentData_t >            aComponents;

	// Component Name to Component Data
	std::unordered_map< std::string_view, EntComponentData_t* > aComponentNames;

	// [type hash of var] = Var Type Enum
	std::unordered_map< size_t, EEntComponentVarType >          aVarTypes;

	// Callback Functions for when a new component is registered at runtime
	// Used for creating a new component pool for client and/or server entity system
	// std::vector< FEntComp_Register* >                           aCallbacks;
	std::vector< EntitySystem* >                                aCallbacks;
};


struct EntCompVarTypeToEnum_t
{
	size_t               aHashCode;
	EEntComponentVarType aType;
};


extern EntComponentRegistry_t gEntComponentRegistry;

// void* EntComponentRegistry_Create( std::string_view sName );
// void* EntComponentRegistry_GetVarHandler();


class EntityComponentPool
{
  public:
	EntityComponentPool();
	~EntityComponentPool();

	// Called whenever an entity is destroyed on all Component Pools
	bool                                             Init( const char* spName );

	// Get Component Registry Data
	EntComponentData_t*                              GetRegistryData();

	// Does this pool contain a component for this entity?
	bool                                             Contains( Entity entity );

	// Called whenever an entity is destroyed on all Component Pools
	void                                             EntityDestroyed( Entity entity );

	// Adds This component to the entity
	void*                                            Create( Entity entity );

	// Removes this component from the entity
	void                                             Remove( Entity entity );

	// Removes this component by index
	void                                             RemoveByIndex( size_t sIndex );

	// Removes this component from the entity later
	void                                             RemoveQueued( Entity entity );

	// Removes components queued for deletion
	void                                             RemoveAllQueued();

	// Gets the data for this component
	void*                                            GetData( Entity entity );

	// Marks this component as predicted
	void                                             SetPredicted( Entity entity, bool sPredicted );

	// Is this component predicted for this Entity?
	bool                                             IsPredicted( Entity entity );

	// How Many Components are in this Pool?
	size_t                                           GetCount();

	// ------------------------------------------------------------------

	// Map Component Index to Entity
	std::unordered_map< size_t, Entity >             aMapComponentToEntity;

	// Map Entity to Component Index
	std::unordered_map< Entity, size_t >             aMapEntityToComponent;

	// Memory Pool of Components
	// This is an std::array so that when a component is freed, it does not changes the index of each component
	std::array< void*, CH_MAX_ENTITIES >             aComponents{};

	// Component States, will store if an entity is just created or deleted for one frame
	// Key is an index into aComponents
	std::unordered_map< size_t, EEntityCreateState > aComponentStates;

	// Entity's that are in here will have this component type predicted for it
	std::set< Entity >                               aPredicted{};

	// Amount of Components we have allocated
	size_t                                           aCount;

	// Component Name
	const char*                                      apName;

	// Component Creation and Free Func
	FEntComp_New                                     aFuncNew;
	FEntComp_Free                                    aFuncFree;

	EntComponentData_t*                              apData;

	// Component Systems that manage this component
	// NOTE: This may always just be one
	// std::set< IEntityComponentSystem* >  aComponentSystems;
	IEntityComponentSystem*                          apComponentSystem = nullptr;
};


const char* EntComp_NetTypeToStr( EEntComponentNetType sNetType );
const char* EntComp_VarTypeToStr( EEntComponentVarType sVarType );
std::string EntComp_GetStrValueOfVar( void* spData, EEntComponentVarType sVarType );
std::string EntComp_GetStrValueOfVarOffset( size_t sOffset, void* spData, EEntComponentVarType sVarType );

// void        EntComp_AddRegisterCallback( FEntComp_Register* spFunc );
// void        EntComp_RemoveRegisterCallback( FEntComp_Register* spFunc );
void        EntComp_AddRegisterCallback( EntitySystem* spSystem );
void        EntComp_RemoveRegisterCallback( EntitySystem* spSystem );
void        EntComp_RunRegisterCallbacks( const char* spName );

void        Ent_RegisterBaseComponents();


template< typename T >
inline void EntComp_RegisterComponent( const char* spName, bool sOverrideClient, EEntComponentNetType sNetType, FEntComp_New sFuncNew, FEntComp_Free sFuncFree )
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
	data.aOverrideClient                            = sOverrideClient;
	data.aNetType                                   = sNetType;
	data.aFuncNew                                   = sFuncNew;
	data.aFuncFree                                  = sFuncFree;

	gEntComponentRegistry.aComponentNames[ spName ] = &data;

	// Run Callbacks
	EntComp_RunRegisterCallbacks( spName );
}


template< typename T >
inline void EntComp_RegisterComponentSystem( FEntComp_NewSys sFuncNewSys )
{
	size_t typeHash = typeid( T ).hash_code();

	auto   it       = gEntComponentRegistry.aComponents.find( typeHash );

	if ( it == gEntComponentRegistry.aComponents.end() )
	{
		Log_ErrorF( "Component not registered, can't set system creation function: \"%s\"\n", typeid( T ).name() );
		return;
	}

	EntComponentData_t& data = it->second;
	data.aFuncNewSystem      = sFuncNewSys;
}


template< typename T, typename VAR_TYPE >
inline void EntComp_RegisterComponentVar( const char* spVarName, const char* spName, size_t sOffset, size_t sVarHash )
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

	auto& varData       = data.aVars[ sOffset ];
	varData.apVarName   = spVarName;
	varData.apName      = spName;
	varData.aSize       = sizeof( VAR_TYPE );

	// Assert( sVarHash == typeid( ComponentNetVar< VAR_TYPE > ).hash_code() );

	if ( sVarHash != typeid( ComponentNetVar< VAR_TYPE > ).hash_code() )
	{
		varData.aIsNetVar = false;
		// Log_ErrorF( "Not Registering Component Var, is not a ComponentNetVar Type: \"%s\" - \"%s\"\n", typeid( VAR_TYPE ).name(), spVarName );
		// return;
	}
	else
	{
		varData.aIsNetVar = true;
	}

	// Get Var Type
	size_t varTypeHash = typeid( VAR_TYPE ).hash_code();
	auto   findEnum    = gEntComponentRegistry.aVarTypes.find( varTypeHash );

	if ( findEnum == gEntComponentRegistry.aVarTypes.end() )
	{
		Log_ErrorF( "Component Var Type not registered in EEntComponentVarType: \"%s\"\n", typeid( VAR_TYPE ).name() );
		return;
	}

	varData.aType = findEnum->second;
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
// Entity Component System Interface
// stores a list of entities with that component type
// ====================================================================================================


class IEntityComponentSystem
{
  public:
	virtual ~IEntityComponentSystem()                        = default;

	// Called when the component is added to this entity
	virtual void          ComponentAdded( Entity sEntity )   = 0;

	// Called when the component is removed from this entity
	virtual void          ComponentRemoved( Entity sEntity ) = 0;

	// Called when the component data has been updated
	virtual void          ComponentUpdated( Entity sEntity ) = 0;

	virtual void          Update() {};

	std::vector< Entity > aEntities;
};


// ====================================================================================================
// Entity System
// ====================================================================================================


class EntitySystem
{
  public:
	static bool                                                   CreateClient();
	static bool                                                   CreateServer();

	static void                                                   DestroyClient();
	static void                                                   DestroyServer();

	bool                                                          Init();
	void                                                          Shutdown();

	void                                                          UpdateSystems();

	// Update Entity and Component States, and Delete Queued Entities
	void                                                          UpdateStates();

	void                                                          CreateComponentPools();
	void                                                          CreateComponentPool( const char* spName );

	// Get a system for managing this component 
	IEntityComponentSystem*                                       GetComponentSystem( const char* spName );

	// Entity                                                        CreateEntityNetworked();

	Entity                                                        CreateEntity();
	void                                                          DeleteEntity( Entity ent );
	void                                                          DeleteEntityQueued( Entity ent );
	void                                                          DeleteQueuedEntities();
	Entity                                                        GetEntityCount();

	// kind of a hack, this just allocates the entity for the client
	// but what if a client only entity is using an ID that a newly created server entity is using?
	// curl up into a ball and die i guess
	Entity                                                        CreateEntityFromServer( Entity desiredId );
	bool                                                          EntityExists( Entity desiredId );

	// Read and write from the network
	void                                                          ReadEntityUpdates( capnp::MessageReader& srReader );
	void                                                          WriteEntityUpdates( capnp::MessageBuilder& srBuilder );

	void                                                          ReadComponentUpdates( capnp::MessageReader& srReader );
	void                                                          WriteComponentUpdates( capnp::MessageBuilder& srBuilder, bool sFullUpdate );

	// void                                                          SetComponentNetworked( Entity ent, const char* spName );

	// Add a component to an entity
	void*                                                         AddComponent( Entity entity, const char* spName );

	// Does this entity have this component?
	bool                                                          HasComponent( Entity entity, const char* spName );

	// Get a component from an entity
	void*                                                         GetComponent( Entity entity, const char* spName );

	// Remove a component from an entity
	void                                                          RemoveComponent( Entity entity, const char* spName );

	// Sets Prediction on this component
	void                                                          SetComponentPredicted( Entity entity, const char* spName, bool sPredicted );

	// Is this component predicted for this Entity?
	bool                                                          IsComponentPredicted( Entity entity, const char* spName );

	// Get the Component Pool for this Component
	EntityComponentPool*                                          GetComponentPool( const char* spName );

	// Gets a component system by type_hash()
	template< typename T >
	inline T* GetSystem()
	{
		size_t hash = typeid( T ).hash_code();
		auto   it   = aComponentSystems.find( hash );

		if ( it != aComponentSystems.end() )
		{
			static_cast< T* >( it->second );
		}

		Log_ErrorF( "Failed to find Component System \"%s\"\n", typeid( T ).name() );
		return nullptr;
	}

	// TEMP DEBUG
	bool                                                          aIsClient = false;

	// Queue of unused entity IDs
	// TODO: CHANGE BACK TO QUEUE
	// std::queue< Entity >                               aEntityPool{};
	std::vector< Entity >                                         aEntityPool{};

	// Entities queued to delete later
	std::set< Entity >                                            aDeleteEntities{};

	// Entity ID's in use
	std::vector< Entity >                                         aUsedEntities{};

	// Array of signatures where the index corresponds to the entity ID
	// std::array< Signature, CH_MAX_ENTITIES > aSignatures{};

	// Total living entities - used to keep limits on how many exist
	Entity                                                        aEntityCount = 0;

	// Component Array - list of all of this type of component in existence
	std::unordered_map< std::string_view, EntityComponentPool* >  aComponentPools;

	// All Component Systems, key is the type_hash() of the system
	// NOTE: it's a bit strange to have them be stored here and one in each component pool
	std::unordered_map< size_t, IEntityComponentSystem* >         aComponentSystems;

	// Entity States, will store if an entity is just created or deleted for one frame
	std::unordered_map< Entity, EEntityCreateState >              aEntityStates;
};


EntitySystem* GetEntitySystem();

inline void*  Ent_AddComponent( Entity sEnt, const char* spName )
{
	return GetEntitySystem()->AddComponent( sEnt, spName );
}

template< typename T >
inline T* Ent_AddComponent( Entity sEnt, const char* spName )
{
	return static_cast< T* >( GetEntitySystem()->AddComponent( sEnt, spName ) );
}


inline void* Ent_GetComponent( Entity sEnt, const char* spName )
{
	return GetEntitySystem()->GetComponent( sEnt, spName );
}

// Gets a component and static cast's it to the desired type
template< typename T >
inline T* Ent_GetComponent( Entity sEnt, const char* spName )
{
	return static_cast< T* >( GetEntitySystem()->GetComponent( sEnt, spName ) );
}

// Gets a component system by type_hash()
template< typename T >
inline T* Ent_GetSystem()
{
	return static_cast< T* >( GetEntitySystem()->GetSystem< T >() );
}


// ==========================================
// Network Var
// ==========================================


template< typename T >
struct ComponentNetVar
{
	T    aValue{};
	bool aIsDirty = true;

	ComponentNetVar() :
		aIsDirty( true ), aValue()
	{
	}

	template< typename VAR_TYPE = int >
	ComponentNetVar( VAR_TYPE var ) :
		aIsDirty( true ), aValue( var )
	{
	}

	~ComponentNetVar()
	{
	}

	const T& Set( const T* spValue )
	{
		// if ( aValue != *spValue )
		if ( memcmp( &aValue, spValue, sizeof( T ) ) != 0 )
		{
			aIsDirty = true;
			aValue   = *spValue;
		}

		return aValue;
	}

	const T& Set( const T& srValue )
	{
		if ( memcmp( &aValue, &srValue, sizeof( T ) ) != 0 )
		{
			aIsDirty = true;
			aValue   = srValue;
		}

		return aValue;
	}

	// const T& Set( const T& srValue )
	// {
	// 	if ( aValue != srValue )
	// 	{
	// 		aIsDirty = true;
	// 		aValue   = srValue;
	// 	}
	// }

	const T& Get() const
	{
		return aValue;
	}

	T& Edit()
	{
		aIsDirty = true;
		return aValue;
	}

	// -------------------------------------------------
	// Operators

	operator const T&() const
	{
		return aValue;
	}

	// T* operator->() const
	// {
	// 	return &aValue;
	// }

	// T* operator()() const
	// {
	// 	return &aValue;
	// }

	const T& operator=( const T* spValue )
	{
		return Set( spValue );
	}

	// const T& operator=( const T& srValue )
	// {
	// 	return Set( srValue );
	// }

	bool operator==( const T& srValue ) const
	{
		return aValue == srValue;
	}

	bool operator!=( const T& srValue ) const
	{
		return aValue != srValue;
	}

	const T& operator+=( const T* spValue )
	{
		aIsDirty = true;
		aValue += *spValue;
		return aValue;
	}

	const T& operator+=( const T& srValue )
	{
		aIsDirty = true;
		aValue += srValue;
		return aValue;
	}

	const T& operator*=( const T* spValue )
	{
		aIsDirty = true;
		aValue *= *spValue;
		return aValue;
	}

	const T& operator*=( const T& srValue )
	{
		aIsDirty = true;
		aValue *= srValue;
		return aValue;
	}

	T operator*( const T& srValue ) const
	{
		return aValue * srValue;
	}

	T operator/( const T& srValue ) const
	{
		return aValue / srValue;
	}

	T operator+( const T& srValue ) const
	{
		return aValue + srValue;
	}

	T operator-( const T& srValue ) const
	{
		return aValue - srValue;
	}

	template< typename OTHER_TYPE = int >
	T operator*( const OTHER_TYPE& srValue ) const
	{
		return aValue * srValue;
	}

#if 0
	template< typename OTHER_TYPE = int >
	T operator/( const OTHER_TYPE& srValue ) const
	{
		return aValue / srValue;
	}

	template< typename OTHER_TYPE = int >
	T operator+( const OTHER_TYPE& srValue ) const
	{
		return aValue + srValue;
	}

	template< typename OTHER_TYPE = int >
	T operator-( const OTHER_TYPE& srValue ) const
	{
		return aValue - srValue;
	}

	// Index into the buffer
	template< typename INDEX_TYPE = int >
	T& operator[]( INDEX_TYPE sIndex )
	{
		return aValue[ sIndex ];
	}
#endif

	// -------------------------------------------------
	// C++ class essentials

	// copying
	void assign( const ComponentNetVar& other )
	{
		Set( &other.aValue );
	}

	// moving
	void assign( ComponentNetVar&& other )
	{
		Set( &other.aValue );

		// swap the values of this one with that one
		// std::swap( aIsDirty, other.aIsDirty );
		// std::swap( aValue, other.aValue );
	}

#if 1
	ComponentNetVar& operator=( const ComponentNetVar& other )
	{
		assign( other );
		return *this;
	}

	ComponentNetVar& operator=( ComponentNetVar&& other )
	{
		assign( std::move( other ) );
		return *this;
	}
#endif

	// copying
	ComponentNetVar( const ComponentNetVar& other )
	{
		assign( other );
	}

	// moving
	ComponentNetVar( ComponentNetVar&& other )
	{
		assign( std::move( other ) );
	}
};


// ==========================================
// Some Base Types here for now


struct CTransform
{
	ComponentNetVar< glm::vec3 > aPos   = {};
	ComponentNetVar< glm::vec3 > aAng   = {};
	ComponentNetVar< glm::vec3 > aScale = {};

	// -------------------------------------------------
	// C++ class essentials

	CTransform()
	{
	}

	~CTransform()
	{
	}

	// copying
	void                         assign( const CTransform& other )
	{
		aPos   = std::move( other.aPos );
		aAng   = std::move( other.aAng );
		aScale = std::move( other.aScale );
	}

	// moving
	void assign( CTransform&& other )
	{
		// swap the values of this one with that one
		std::swap( aPos, other.aPos );
		std::swap( aAng, other.aAng );
		std::swap( aScale, other.aScale );
	}

	CTransform& operator=( const CTransform& other )
	{
		assign( other );
		return *this;
	}

	CTransform& operator=( CTransform&& other )
	{
		assign( std::move( other ) );
		return *this;
	}

	// copying
	CTransform( const CTransform& other )
	{
		assign( other );
	}

	// moving
	CTransform( CTransform&& other ) noexcept
	{
		assign( std::move( other ) );
	}
};


struct CTransformSmall
{
	ComponentNetVar< glm::vec3 > aPos = {};
	ComponentNetVar< glm::vec3 > aAng = {};

	// -------------------------------------------------
	// C++ class essentials

	CTransformSmall()
	{
	}

	~CTransformSmall()
	{
	}

	// copying
	void                         assign( const CTransformSmall& other )
	{
		aPos   = std::move( other.aPos );
		aAng   = std::move( other.aAng );
	}

	// moving
	void assign( CTransformSmall&& other )
	{
		// swap the values of this one with that one
		std::swap( aPos, other.aPos );
		std::swap( aAng, other.aAng );
	}

	CTransformSmall& operator=( const CTransformSmall& other )
	{
		assign( other );
		return *this;
	}

	CTransformSmall& operator=( CTransformSmall&& other )
	{
		assign( std::move( other ) );
		return *this;
	}

	// copying
	CTransformSmall( const CTransformSmall& other )
	{
		assign( other );
	}

	// moving
	CTransformSmall( CTransformSmall&& other ) noexcept
	{
		assign( std::move( other ) );
	}
};


struct CRigidBody
{
	ComponentNetVar< glm::vec3 > aVel = {};
	ComponentNetVar< glm::vec3 > aAccel = {};
};


struct CGravity
{
	// glm::vec3 aForce = {};
	ComponentNetVar< glm::vec3 > aForce = {};
};


// Direction Vectors
struct CDirection
{
	ComponentNetVar< glm::vec3 > aForward = {};
	ComponentNetVar< glm::vec3 > aUp = {};
	ComponentNetVar< glm::vec3 > aRight = {};
};


struct CCamera: public CDirection
{
	ComponentNetVar< CTransformSmall > aTransform = {};
	ComponentNetVar< float >           aFov       = 90.f;
};


struct CSound
{
	ComponentNetVar< Handle > aStream = InvalidHandle;
};


struct CRenderable_t
{
	ComponentNetVar< Handle > aHandle = InvalidHandle;
};


struct CModelInfo
{
	// Path to model to load
	ComponentNetVar< std::string > aPath;

	// TODO: store animations, attachments, etc. here
};


// wrapper for lights
struct CLight
{
	ComponentNetVar< ELightType > aType = ELightType_Directional;
	ComponentNetVar< glm::vec4 >  aColor{};
	ComponentNetVar< glm::vec3 >  aPos{};
	ComponentNetVar< glm::vec3 >  aAng{};
	ComponentNetVar< float >      aInnerFov = 45.f;
	ComponentNetVar< float >      aOuterFov = 45.f;
	ComponentNetVar< float >      aRadius   = 0.f;
	ComponentNetVar< float >      aLength   = 0.f;
	ComponentNetVar< bool >       aShadow   = true;
	ComponentNetVar< bool >       aEnabled  = true;

	Light_t*                      apLight   = nullptr;  // var not registered
};


// convinence
// inline auto &GetDirection( Entity ent ) { return GetEntitySystem()->GetComponent< CDirection >( ent ); }


// Helper Macros
#define CH_REGISTER_COMPONENT( type, name, overrideClient, netType ) \
  EntComp_RegisterComponent< type >( #name, overrideClient, netType, [ & ]() { return new type; }, [ & ]( void* spData ) { delete (type*)spData; } )

#define CH_REGISTER_COMPONENT_SYS( type, systemClass, systemVar ) \
  EntComp_RegisterComponentSystem< type >( [ & ]() { \
		if ( Game_ProcessingClient() ) { \
			systemVar[ 1 ] = new systemClass; \
			return systemVar[ 1 ]; \
		} \
		else { \
			systemVar[ 0 ] = new systemClass; \
			return systemVar[ 0 ]; \
		} \
	} )

#define CH_REGISTER_COMPONENT_NEWFREE( type, name, overrideClient, netType, newFunc, freeFunc ) \
  EntComp_RegisterComponent< type >( #name, overrideClient, netType, newFunc, freeFunc )

#define CH_REGISTER_COMPONENT_VAR( type, varType, varName, varStr ) \
  EntComp_RegisterComponentVar< type, varType >( #varName, #varStr, offsetof( type, varName ), typeid( type::varName ).hash_code() )

// #define CH_REGISTER_COMPONENT_RW_EX( type, read, write ) \
//   EntComp_RegisterComponentReadWrite< type >( read, write )

#define CH_COMPONENT_READ_DEF( type ) \
  static void __EntCompFunc_Read_##type( capnp::MessageReader& srReader, void* spData )

#define CH_COMPONENT_WRITE_DEF( type ) \
  static bool __EntCompFunc_Write_##type( capnp::MessageBuilder& srMessage, const void* spData, bool sFullUpdate )

#define CH_COMPONENT_RW( type )    __EntCompFunc_Read_##type, __EntCompFunc_Write_##type
#define CH_COMPONENT_READ( type )  __EntCompFunc_Read_##type
#define CH_COMPONENT_WRITE( type ) __EntCompFunc_Write_##type

#define CH_REGISTER_COMPONENT_RW_EX( type, name, overrideClient, netType ) \
  CH_REGISTER_COMPONENT( type, name, overrideClient, netType );  \
  EntComp_RegisterComponentReadWrite< type >( CH_COMPONENT_RW( type ) )

#define CH_REGISTER_COMPONENT_RW( type, name, overrideClient ) \
  CH_REGISTER_COMPONENT_RW_EX( type, name, overrideClient, EEntComponentNetType_Both );

#define CH_REGISTER_COMPONENT_RW_CL( type, name, overrideClient ) \
  CH_REGISTER_COMPONENT_RW( type, name, overrideClient, EEntComponentNetType_Client )

#define CH_REGISTER_COMPONENT_RW_SV( type, name, overrideClient ) \
  CH_REGISTER_COMPONENT_RW( type, name, overrideClient, EEntComponentNetType_Server )


// Helper Macros for Registering Standard Var Types

#define CH_REGISTER_COMP_VAR_VEC3( type, varName, varStr ) \
  CH_REGISTER_COMPONENT_VAR( type, glm::vec3, varName, varStr )

#define CH_REGISTER_COMPT_VAR_FL( type, varName, varStr ) \
  CH_REGISTER_COMPONENT_VAR( type, float, varName, varStr )

