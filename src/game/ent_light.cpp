#include "game_shared.h"
#include "ent_light.h"
#include "graphics/graphics.h"
#include "graphics/lighting.h"


CONVAR( r_debug_draw_transforms, 0 );


void LightSystem::ComponentAdded( Entity sEntity, void* spData )
{
	// light->aType will not be initialized yet smh

	// if ( Game_ProcessingServer() )
	// 	return;
	// 
	// auto light = Ent_GetComponent< CLight >( sEntity, "light" );
	// 
	// if ( light )
	// 	light->apLight = nullptr;
	// 	// light->apLight = Graphics_CreateLight( light->aType );
}


void LightSystem::ComponentRemoved( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;

	auto light = static_cast< CLight* >( spData );

	if ( light )
		Graphics_DestroyLight( light->apLight );
}


static void UpdateLightData( Entity sEntity, CLight* spLight )
{
	if ( !spLight || !spLight->apLight )
		return;

	if ( spLight->aUseTransform )
	{
		glm::mat4 matrix;
		if ( GetEntitySystem()->GetWorldMatrix( matrix, sEntity ) )
		{
			spLight->apLight->aPos = Util_GetMatrixPosition( matrix );
			spLight->apLight->aAng = Util_GetMatrixAngles( matrix );
		}
	}
	else
	{
		spLight->apLight->aPos = spLight->aPos;
		spLight->apLight->aAng = spLight->aAng;
	}

	spLight->apLight->aType     = spLight->aType;
	spLight->apLight->aColor    = spLight->aColor;
	spLight->apLight->aInnerFov = spLight->aInnerFov;
	spLight->apLight->aOuterFov = spLight->aOuterFov;
	spLight->apLight->aRadius   = spLight->aRadius;
	spLight->apLight->aLength   = spLight->aLength;
	spLight->apLight->aShadow   = spLight->aShadow;
	spLight->apLight->aEnabled  = spLight->aEnabled;

	Graphics_UpdateLight( spLight->apLight );
}


void LightSystem::ComponentUpdated( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;

	auto light = static_cast< CLight* >( spData );

	if ( !light )
		return;

	if ( !light->apLight )
	{
		light->apLight = Graphics_CreateLight( light->aType );

		if ( !light->apLight )
			return;
	}

	// Light type switched, we need to recreate the light
	if ( light->aType != light->apLight->aType )
	{
		Graphics_DestroyLight( light->apLight );
		light->apLight = nullptr;

		light->apLight = Graphics_CreateLight( light->aType );

		if ( !light->apLight )
			return;
	}

	Assert( light->apLight );

	UpdateLightData( sEntity, light );
}


void LightSystem::Update()
{
	if ( Game_ProcessingServer() )
		return;

#if 1
	for ( Entity entity : aEntities )
	{
		auto light = Ent_GetComponent< CLight >( entity, "light" );

		if ( !light )
			continue;

		Assert( light->apLight );

		// this is awful
		UpdateLightData( entity, light );
	}
#endif
}


LightSystem* gLightEntSystems[ 2 ] = { 0, 0 };


LightSystem* GetLightEntSys()
{
	int i = Game_ProcessingClient() ? 1 : 0;
	Assert( gLightEntSystems[ i ] );
	return gLightEntSystems[ i ];
}


// ------------------------------------------------------------


void EntSys_ModelInfo::ComponentAdded( Entity sEntity, void* spData )
{
	// darn, modelInfo->aPath will not be initialized, bruh this sucks
	// i will have to do it in the Update function

	// A potential solution to this, though not guaranteed,
	// is to have AddComponent have a 3rd parameter containing data to initialize the component with
	// though sometimes components could be added here without that data, so it's unreliable
	
	// HACK: PASS THROUGH TO AUTO RENDERABLE
	void* autoRenderable = Ent_GetComponent( sEntity, "autoRenderable" );

	if ( autoRenderable )
		GetAutoRenderableSys()->ComponentAdded( sEntity, autoRenderable );
}


void EntSys_ModelInfo::ComponentRemoved( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;

	// HACK: PASS THROUGH TO AUTO RENDERABLE
	void* autoRenderable = Ent_GetComponent( sEntity, "autoRenderable" );

	if ( autoRenderable )
		GetAutoRenderableSys()->ComponentRemoved( sEntity, autoRenderable );
}


static bool UpdateModelHandle( CModelInfo* modelInfo )
{
	if ( !modelInfo )
		return false;

	if ( modelInfo->aPath.aIsDirty || ( modelInfo->aModel == InvalidHandle && modelInfo->aPath.Get().size() ) )
	{
		if ( modelInfo->aModel != InvalidHandle )
		{
			Graphics_FreeModel( modelInfo->aModel );
			modelInfo->aModel = InvalidHandle;
		}

		modelInfo->aModel = Graphics_LoadModel( modelInfo->aPath );
		return true;
	}

	return false;
}


void EntSys_ModelInfo::ComponentUpdated( Entity sEntity, void* spData )
{
	auto modelInfo = static_cast< CModelInfo* >( spData );

	if ( !modelInfo )
		return;

	UpdateModelHandle( modelInfo );

	if ( Game_ProcessingServer() )
		return;

	// HACK: PASS THROUGH TO AUTO RENDERABLE
	void* autoRenderable = Ent_GetComponent( sEntity, "autoRenderable" );

	if ( autoRenderable )
		GetAutoRenderableSys()->ComponentUpdated( sEntity, autoRenderable );
}


void EntSys_ModelInfo::Update()
{
	for ( Entity entity : aEntities )
	{
		auto modelInfo = Ent_GetComponent< CModelInfo >( entity, "modelInfo" );

		Assert( modelInfo );

		if ( !modelInfo )
			continue;

		bool handleUpdated = UpdateModelHandle( modelInfo );

		if ( handleUpdated )
		{
			// HACK: PASS THROUGH TO AUTO RENDERABLE
			void* autoRenderable = Ent_GetComponent( entity, "autoRenderable" );

			if ( autoRenderable )
				GetAutoRenderableSys()->ComponentUpdated( entity, autoRenderable );
		}
	}

	if ( Game_ProcessingServer() )
		return;
}


EntSys_ModelInfo* gEntSys_ModelInfo[ 2 ] = { 0, 0 };


// ------------------------------------------------------------


void EntSys_Transform::ComponentUpdated( Entity sEntity, void* spData )
{
	// THIS IS ONLY CALLED ON THE CLIENT THIS WON'T WORK
	// TODO: Check if we are parented to anything
}


void EntSys_Transform::Update()
{
	if ( Game_ProcessingServer() )
		return;

	if ( r_debug_draw_transforms )
	{
		for ( Entity entity : aEntities )
		{
			// auto transform = Ent_GetComponent< CTransform >( entity, "transform" );

			// We have to draw them in world space
			glm::mat4 matrix;
			if ( !GetEntitySystem()->GetWorldMatrix( matrix, entity ) )
				continue;

			// Graphics_DrawAxis( transform->aPos, transform->aAng, transform->aScale );
			Graphics_DrawAxis( Util_GetMatrixPosition( matrix ), Util_GetMatrixAngles( matrix ), Util_GetMatrixScale( matrix ) );
		}
	}
}


EntSys_Transform* gEntSys_Transform[ 2 ] = { 0, 0 };


// ------------------------------------------------------------


void EntSys_Renderable::ComponentRemoved( Entity sEntity, void* spData )
{
	// This shouldn't even be on the server
	if ( Game_ProcessingServer() )
		return;

	auto          renderComp = static_cast< CRenderable_t* >( spData );

	// Auto Delete the renderable and free the model
	Renderable_t* renderData = Graphics_GetRenderableData( renderComp->aHandle );

	if ( !renderData )
		return;

	if ( renderData->aModel )
	{
		Graphics_FreeModel( renderData->aModel );
	}

	Graphics_FreeRenderable( renderComp->aHandle );
}


EntSys_Renderable* gEntSys_Renderable[ 2 ] = { 0, 0 };


// ------------------------------------------------------------


void EntSys_AutoRenderable::ComponentAdded( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;

	Ent_AddComponent( sEntity, "renderable" );
	Renderable_t* renderable = Ent_CreateRenderable( sEntity );
}


void EntSys_AutoRenderable::ComponentRemoved( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;

	GetEntitySystem()->RemoveComponent( sEntity, "renderable" );
}


void EntSys_AutoRenderable::ComponentUpdated( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;

	auto autoRenderable = static_cast< CAutoRenderable* >( spData );

	// Get Model Info and Renderable
	auto modelInfo      = Ent_GetComponent< CModelInfo >( sEntity, "modelInfo" );

	// warn that this needs model info?
	if ( !modelInfo )
		return;

	auto renderComp = Ent_GetComponent< CRenderable_t >( sEntity, "renderable" );

	if ( !renderComp )
	{
		Log_Warn( "oops\n" );
		return;
	}

	Renderable_t* renderData = Graphics_GetRenderableData( renderComp->aHandle );

	if ( !renderData )
	{
		// no need to update the handle if we're creating it
		renderData = Ent_CreateRenderable( sEntity );
		Graphics_UpdateRenderableAABB( renderComp->aHandle );
		return;
	}

	renderData->aTestVis    = autoRenderable->aTestVis;
	renderData->aCastShadow = autoRenderable->aCastShadow;
	renderData->aVisible    = autoRenderable->aVisible;
	
	// Compare Handles
	if ( modelInfo->aModel != renderData->aModel )
	{
		renderData->aModel = modelInfo->aModel;
		Graphics_UpdateRenderableAABB( renderComp->aHandle );
	}
}


void EntSys_AutoRenderable::Update()
{
	if ( Game_ProcessingServer() )
		return;

	for ( Entity entity : aEntities )
	{
		// TODO: check if any of the transforms are dirty, including the parents, unsure how that would work
		glm::mat4 matrix;
		if ( !GetEntitySystem()->GetWorldMatrix( matrix, entity ) )
			continue;

		auto renderComp = Ent_GetComponent< CRenderable_t >( entity, "renderable" );

		if ( !renderComp )
		{
			Log_Warn( "oops\n" );
			continue;
		}

		Renderable_t* renderData = Graphics_GetRenderableData( renderComp->aHandle );

		if ( !renderData )
		{
			Log_Warn( "oops2\n" );
			continue;
		}

		renderData->aModelMatrix = matrix;
		Graphics_UpdateRenderableAABB( renderComp->aHandle );
	}
}


EntSys_AutoRenderable* gEntSys_AutoRenderable[ 2 ] = { 0, 0 };


EntSys_AutoRenderable* GetAutoRenderableSys()
{
	int i = Game_ProcessingClient() ? 1 : 0;
	Assert( gEntSys_AutoRenderable[ i ] );
	return gEntSys_AutoRenderable[ i ];
}


// ------------------------------------------------------------


void EntSys_PhysInfo::ComponentAdded( Entity sEntity, void* spData )
{
}


void EntSys_PhysInfo::ComponentRemoved( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;
}


void EntSys_PhysInfo::ComponentUpdated( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;
}


void EntSys_PhysInfo::Update()
{
	if ( Game_ProcessingServer() )
		return;
}


EntSys_PhysInfo* gEntSys_PhysInfo[ 2 ] = { 0, 0 };

