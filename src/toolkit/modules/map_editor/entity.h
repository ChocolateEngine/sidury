#pragma once

#include "types/transform.h"
#include "igraphics.h"
#include "physics/iphysics.h"


#include <unordered_set>


// using Entity = size_t;


struct Color3
{
	u8 r;
	u8 g;
	u8 b;
};


// not sure i really need much of a component system for an editor
struct Entity_t
{
	char*              apName;

	Transform          aTransform;

	// Rendering
	ChHandle_t         aModel;
	ChHandle_t         aRenderable;
	bool               aHidden;

	// Physics
	char*              apPhysicsModel;
	IPhysicsObject*    apPhysicsObject;

	// Lighting
	bool               aLightEnabled;
	Light_t*           apLight;
	ChHandle_t         aLightRenderable;

	// Audio

	// Color for Selecting with the cursor
	// IDEA: eventually you might need to select renderables based on material
	// so this would need to be an array of colors, with the same length as the material count on the current renderable
	u8                 aSelectColor[ 3 ];

	// This is used for selecting individual materials
	ChVector< Color3 > aMaterialColors;

	// List of Components with general data
};


bool                                                Entity_Init();
void                                                Entity_Shutdown();
void                                                Entity_Update();

ChHandle_t                                          Entity_Create();
void                                                Entity_Delete( ChHandle_t sHandle );

Entity_t*                                           Entity_GetData( ChHandle_t sHandle );
const std::vector< ChHandle_t >&                    Entity_GetHandleList();

void                                                Entity_SetName( ChHandle_t sHandle, const char* name );

void                                                Entity_SetEntityVisible( ChHandle_t sEntity, bool sVisible );
void                                                Entity_SetEntitiesVisible( ChHandle_t* sEntities, u32 sCount, bool sVisible );
void                                                Entity_SetEntitiesVisibleNoChild( ChHandle_t* sEntities, u32 sCount, bool sVisible );

// Do an update on these entities
void                                                Entity_SetEntitiesDirty( ChHandle_t* sEntities, u32 sCount );

// Get the highest level parent for this entity, returns self if not parented
ChHandle_t                                          Entity_GetRootParent( ChHandle_t sSelf );

// Recursively get all entities attached to this one (SLOW)
void                                                Entity_GetChildrenRecurse( ChHandle_t sEntity, ChVector< ChHandle_t >& srChildren );
void                                                Entity_GetChildrenRecurse( ChHandle_t sEntity, std::unordered_set< ChHandle_t >& srChildren );

// Get child entities attached to this one (SLOW)
void                                                Entity_GetChildren( ChHandle_t sEntity, ChVector< ChHandle_t >& srChildren );

// bool                          Entity_IsParented( ChHandle_t sEntity );
ChHandle_t                                          Entity_GetParent( ChHandle_t sEntity );
void                                                Entity_SetParent( ChHandle_t sEntity, ChHandle_t sParent );

// [ child ] = parent
const std::unordered_map< ChHandle_t, ChHandle_t >& Entity_GetParentMap();

// Returns a Model Matrix with parents applied in world space
void                                                Entity_GetWorldMatrix( glm::mat4& srMat, ChHandle_t sEntity );


