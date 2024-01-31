#include "entity.h"
#include "main.h"
#include "core/resource.h"

#define NAME_LEN 64


LOG_REGISTER_CHANNEL2( Entity, LogColor::DarkPurple );


static ResourceList< Entity_t >                      gEntityList;

// Entity Parents
// [ child ] = parent
static std::unordered_map< ChHandle_t, ChHandle_t >  gEntityParents;

//static std::unordered_map< glm::vec3, ChHandle_t > gColorToEntity;
//static std::unordered_map< ChHandle_t, glm::vec3 > gEntityToColor;


bool Entity_Init()
{
	return true;
}


void Entity_Shutdown()
{
	// TODO: delete all entities
}


void Entity_Update()
{
	// update renderables

	for ( ChHandle_t entityHandle : gEntityList.aHandles )
	{
		Entity_t* ent = nullptr;
		if ( !gEntityList.Get( entityHandle, &ent ) )
			continue;

		glm::mat4 worldMatrix;

		// BAD AND SLOW
		Entity_GetWorldMatrix( worldMatrix, entityHandle );

		// Update Light Position and Angle
		if ( ent->apLight )
		{
			ent->apLight->aEnabled = !ent->aHidden;
			ent->apLight->aPos     = Util_GetMatrixPosition( worldMatrix );
			ent->apLight->aRot     = Util_GetMatrixRotation( worldMatrix );

			// blech
			graphics->UpdateLight( ent->apLight );
		}

		if ( !ent->aRenderable )
			continue;

		Renderable_t* renderable = graphics->GetRenderableData( ent->aRenderable );
		//renderable->aVisible = !ent->aHidden;
		renderable->aModelMatrix = worldMatrix;

		graphics->UpdateRenderableAABB( ent->aRenderable );
	}
}


ChHandle_t Entity_Create()
{
	EditorContext_t* context = Editor_GetContext();

	if ( !context )
		return CH_INVALID_HANDLE;

	Entity_t* ent = nullptr;
	ChHandle_t entHandle = gEntityList.Create( &ent );

	if ( entHandle == CH_INVALID_HANDLE )
		return CH_INVALID_HANDLE;

	ent->aTransform.aScale.x = 1.f;
	ent->aTransform.aScale.y = 1.f;
	ent->aTransform.aScale.z = 1.f;

	context->aMap.aMapEntities.push_back( entHandle );

	ent->apName = realloc( ent->apName, NAME_LEN );
	if ( ent->apName == nullptr )
	{
		Log_Error( "Failed to allocate memory for entity name\n" );
		return;
	}

	strcpy( ent->apName, vstring( "Entity %zd", entHandle ).c_str() );

	return entHandle;
}


void Entity_Delete( ChHandle_t sHandle )
{
	// TODO: queue this for deletion, like in the game? or is that not needed here

	Entity_t* ent = nullptr;

	if ( !gEntityList.Get( sHandle, &ent ) )
	{
		Log_ErrorF( "Invalid Entity: %d", sHandle );
		return;
	}

	// Check if we have a renderable on this entity
	if ( ent->aRenderable != CH_INVALID_HANDLE )
	{
		graphics->FreeRenderable( ent->aRenderable );
	}

	// Check if we have a model on this entity
	if ( ent->aModel != CH_INVALID_HANDLE )
	{
		graphics->FreeModel( ent->aModel );
	}

	// Clear Parent and Children
	ChVector< ChHandle_t > children;
	Entity_GetChildrenRecurse( sHandle, children );

	// Mark all of them as destroyed
	for ( ChHandle_t child : children )
	{
		Entity_Delete( child );
		Log_DevF( gLC_Entity, 2, "Deleting Child Entity (parent %zd): %zd\n", sHandle, child );
	}

	// Search All Contexts for this entity

	u32              i   = 0;
	EditorContext_t* ctx = nullptr;
	for ( ; i < gEditorContexts.aHandles.size(); i++ )
	{
		if ( !gEditorContexts.Get( gEditorContexts.aHandles[ i ], &ctx ) )
			continue;

		u32 j = 0;
		for ( ; j < ctx->aMap.aMapEntities.size(); j++ )
		{
			if ( ctx->aMap.aMapEntities[ j ] == sHandle )
			{
				ctx->aMap.aMapEntities.erase( sHandle );
				break;
			}
		}

		if ( j != ctx->aMap.aMapEntities.size() )
			break;
	}

	gEntityList.Remove( sHandle );
}


Entity_t* Entity_GetData( ChHandle_t sHandle )
{
	Entity_t* ent = nullptr;

	if ( gEntityList.Get( sHandle, &ent ) )
	{
		return ent;
	}

	Log_ErrorF( "Invalid Entity: %d", sHandle );
	return nullptr;
}


const ChVector< ChHandle_t >& Entity_GetHandleList()
{
	return gEntityList.aHandles;
}


void Entity_SetEntityVisible( ChHandle_t sEntity, bool sVisible )
{
	Entity_t* ent = nullptr;

	if ( !gEntityList.Get( sEntity, &ent ) )
	{
		Log_ErrorF( "Invalid Entity: %d", sEntity );
		return;
	}

	ent->aHidden = !sVisible;

	if ( ent->apLight )
	{
		ent->apLight->aEnabled != sVisible;
		graphics->UpdateLight( ent->apLight );
	}

	// no renderable to hide
	if ( ent->aRenderable == CH_INVALID_HANDLE )
		return;

	Renderable_t* renderable = graphics->GetRenderableData( ent->aRenderable );
	
	if ( !renderable )
	{
		Log_ErrorF( gLC_Entity, "Entity Missing Renderable: %d", sEntity );
		ent->aRenderable = CH_INVALID_HANDLE;
		return;
	}

	renderable->aVisible = sVisible;
}


// Get the highest level parent for this entity, returns self if not parented
ChHandle_t Entity_GetRootParent( ChHandle_t sSelf )
{
	PROF_SCOPE();

	auto it = gEntityParents.find( sSelf );

	if ( it != gEntityParents.end() )
		return Entity_GetRootParent( it->second );

	return sSelf;
}


// Recursively get all entities attached to this one (SLOW)
void Entity_GetChildrenRecurse( ChHandle_t sEntity, ChVector< ChHandle_t >& srChildren )
{
	PROF_SCOPE();

	for ( auto& [ child, parent ] : gEntityParents )
	{
		if ( parent != sEntity )
			continue;

		srChildren.push_back( child );
		Entity_GetChildrenRecurse( child, srChildren );
		break;
	}
}


// Recursively get all entities attached to this one (SLOW)
void Entity_GetChildrenRecurse( ChHandle_t sEntity, std::unordered_set< ChHandle_t >& srChildren )
{
	PROF_SCOPE();

	for ( auto& [ child, parent ] : gEntityParents )
	{
		if ( parent != sEntity )
			continue;

		srChildren.emplace( child );
		Entity_GetChildrenRecurse( child, srChildren );
		break;
	}
}


// Get child entities attached to this one (SLOW)
void Entity_GetChildren( ChHandle_t sEntity, ChVector< ChHandle_t >& srChildren )
{
	PROF_SCOPE();

	for ( auto& [ child, parent ] : gEntityParents )
	{
		if ( parent != sEntity )
			continue;

		srChildren.push_back( child );
	}
}


bool Entity_IsParented( ChHandle_t sEntity )
{
	if ( sEntity == CH_INVALID_HANDLE )
		return false;

	auto it = gEntityParents.find( sEntity );
	if ( it == gEntityParents.end() )
		return false;

	return true;
}


ChHandle_t Entity_GetParent( ChHandle_t sEntity )
{
	if ( sEntity == CH_INVALID_HANDLE )
		return CH_INVALID_HANDLE;

	auto it = gEntityParents.find( sEntity );
	if ( it == gEntityParents.end() )
		return CH_INVALID_HANDLE;

	return it->second;
}


void Entity_SetParent( ChHandle_t sEntity, ChHandle_t sParent )
{
	if ( sEntity == CH_INVALID_HANDLE || sEntity == sParent )
		return;

	// Make sure sParent isn't parented to sEntity
	auto it = gEntityParents.find( sParent );

	if ( it != gEntityParents.end() )
	{
		if ( it->second == sEntity )
		{
			Log_Error( gLC_Entity, "Trying to parent entity A to B, when B is already parented to A!!\n" );
			return;
		}
	}

	if ( sParent == CH_INVALID_HANDLE )
	{
		// Clear the parent
		gEntityParents.erase( sEntity );
	}
	else
	{
		gEntityParents[ sEntity ] = sParent;
	}
}


const std::unordered_map< ChHandle_t, ChHandle_t >& Entity_GetParentMap()
{
	return gEntityParents;
}


// Returns a Model Matrix with parents applied in world space
bool Entity_GetWorldMatrix( glm::mat4& srMat, ChHandle_t sEntity )
{
	PROF_SCOPE();

	ChHandle_t parent = Entity_GetParent( sEntity );
	glm::mat4  parentMat( 1.f );

	if ( parent != CH_INVALID_HANDLE )
	{
		// Get the world matrix recursively
		Entity_GetWorldMatrix( parentMat, parent );
	}

	Entity_t* entity = Entity_GetData( sEntity );

	// is this all the wrong order?

	// NOTE: THIS IS PROBABLY WRONG
	srMat = glm::translate( entity->aTransform.aPos );
	// srMat = glm::mat4( 1.f );

	glm::mat4 rotMat(1.f);

	// glm::vec3 temp = glm::radians( transform->aAng.Get() );
	// glm::quat rotQuat = temp;

// rotMat = glm::mat4_cast( rotQuat );

// srMat *= rotMat;
// srMat *= glm::eulerAngleYZX(
//   glm::radians(transform->aAng.Get().x ),
//   glm::radians(transform->aAng.Get().y ),
//   glm::radians(transform->aAng.Get().z ) );

	srMat *= glm::eulerAngleZYX(
		glm::radians( entity->aTransform.aAng[ ROLL ] ),
		glm::radians( entity->aTransform.aAng[ YAW ] ),
		glm::radians( entity->aTransform.aAng[ PITCH ] ) );

	// srMat *= glm::yawPitchRoll(
	//   glm::radians( transform->aAng.Get()[ PITCH ] ),
	//   glm::radians( transform->aAng.Get()[ YAW ] ),
	//   glm::radians( transform->aAng.Get()[ ROLL ] ) );

	srMat = glm::scale( srMat, entity->aTransform.aScale );

	srMat = parentMat * srMat;

	return true;
}


#if 0

ChHandle_t Entity_Create()
{
	EditorContext_t* context = Editor_GetContext();

	if ( !context )
		return CH_INVALID_HANDLE;

	Entity_t* ent = nullptr;
	return context->aEntities.Create( &ent );
}


void Entity_Delete( ChHandle_t sHandle )
{
	Entity_t*        ent = nullptr;

	// Search All Contexts for this entity

	u32              i   = 0;
	EditorContext_t* ctx = nullptr;
	for ( ; i < gEditorContexts.aHandles.size(); i++ )
	{
		if ( !gEditorContexts.Get( gEditorContexts.aHandles[ i ], &ctx ) )
			continue;

		if ( ctx->aEntities.Get( sHandle, &ent ) )
			break;
	}

	if ( i == gEditorContexts.aHandles.size() || ent == nullptr )
	{
		Log_ErrorF( "Invalid Entity: %d", sHandle );
		return;
	}

	// Check if we have a renderable on this entity
	if ( ent->aRenderable != CH_INVALID_HANDLE )
	{
		graphics->FreeRenderable( ent->aRenderable );
	}

	// Check if we have a model on this entity
	if ( ent->aModel != CH_INVALID_HANDLE )
	{
		graphics->FreeModel( ent->aModel );
	}

	ctx->aEntities.Remove( sHandle );
}

#endif

