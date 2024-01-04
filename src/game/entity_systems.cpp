#include "main.h"
#include "game_shared.h"
#include "entity_systems.h"
#include "igraphics.h"


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
	// 	// light->apLight = graphics->CreateLight( light->aType );
}


void LightSystem::ComponentRemoved( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;

	auto light = static_cast< CLight* >( spData );

	if ( light )
		graphics->DestroyLight( light->apLight );
}


static void UpdateLightData( Entity sEntity, CLight* spLight )
{
	PROF_SCOPE();

	if ( !spLight || !spLight->apLight )
		return;

	if ( spLight->aUseTransform )
	{
		glm::mat4 matrix;
		if ( GetEntitySystem()->GetWorldMatrix( matrix, sEntity ) )
		{
			spLight->apLight->aPos = Util_GetMatrixPosition( matrix );
			spLight->apLight->aAng = glm::degrees( Util_GetMatrixAngles( matrix ) );
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

	graphics->UpdateLight( spLight->apLight );
}


void LightSystem::ComponentUpdated( Entity sEntity, void* spData )
{
	PROF_SCOPE();

	if ( Game_ProcessingServer() )
		return;

	auto light = static_cast< CLight* >( spData );

	if ( !light )
		return;

	if ( !light->apLight )
	{
		light->apLight = graphics->CreateLight( light->aType );

		if ( !light->apLight )
			return;
	}

	// Light type switched, we need to recreate the light
	if ( light->aType != light->apLight->aType )
	{
		graphics->DestroyLight( light->apLight );
		light->apLight = nullptr;

		light->apLight = graphics->CreateLight( light->aType );

		if ( !light->apLight )
			return;
	}

	CH_ASSERT( light->apLight );

	UpdateLightData( sEntity, light );
}


void LightSystem::Update()
{
	PROF_SCOPE();

	if ( Game_ProcessingServer() )
		return;

#if 1
	for ( Entity entity : aEntities )
	{
		auto light = Ent_GetComponent< CLight >( entity, "light" );

		if ( !light )
			continue;

		CH_ASSERT( light->apLight );

		// this is awful
		UpdateLightData( entity, light );
	}
#endif
}


LightSystem* gLightEntSystems[ 2 ] = { 0, 0 };


LightSystem* GetLightEntSys()
{
	int i = Game_ProcessingClient() ? 1 : 0;
	CH_ASSERT( gLightEntSystems[ i ] );
	return gLightEntSystems[ i ];
}


// ------------------------------------------------------------


static bool UpdateModelHandle( CRenderable* modelInfo )
{
	PROF_SCOPE();

	if ( !modelInfo )
		return false;

	if ( !( modelInfo->aPath.aIsDirty || ( modelInfo->aModel == InvalidHandle && modelInfo->aPath.Get().size() ) ) )
		return false;

	if ( modelInfo->aModel != InvalidHandle )
	{
		std::string_view curModel = graphics->GetModelPath( modelInfo->aModel );
			
		if ( curModel != modelInfo->aPath.Get() )
		{
			graphics->FreeModel( modelInfo->aModel );
			modelInfo->aModel = InvalidHandle;
		}
	}

	if ( modelInfo->aModel == InvalidHandle )
		modelInfo->aModel = graphics->LoadModel( modelInfo->aPath );

	// Update Renderable if needed

	return true;
}


#if 0
void EntSys_ModelInfo::Update()
{
	for ( Entity entity : aEntities )
	{
		auto modelInfo = Ent_GetComponent< CModelInfo >( entity, "modelInfo" );

		CH_ASSERT( modelInfo );

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
#endif


// ------------------------------------------------------------


void EntSys_Transform::ComponentUpdated( Entity sEntity, void* spData )
{
	// THIS IS ONLY CALLED ON THE CLIENT THIS WON'T WORK
	// TODO: Check if we are parented to anything
}


void EntSys_Transform::Update()
{
	PROF_SCOPE();

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

			// graphics->DrawAxis( transform->aPos, transform->aAng, transform->aScale );
			graphics->DrawAxis( Util_GetMatrixPosition( matrix ), glm::degrees( Util_GetMatrixAngles( matrix ) ), Util_GetMatrixScale( matrix ) );
		}
	}
}


EntSys_Transform* gEntSys_Transform[ 2 ] = { 0, 0 };


// ------------------------------------------------------------


void EntSys_Renderable::ComponentAdded( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;
}


void EntSys_Renderable::ComponentRemoved( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;

	auto renderComp = static_cast< CRenderable* >( spData );

	// Auto Delete the renderable and free the model
	Renderable_t* renderData = graphics->GetRenderableData( renderComp->aRenderable );

	if ( !renderData )
		return;

	if ( renderData->aModel )
	{
		graphics->FreeModel( renderData->aModel );
	}

	graphics->FreeRenderable( renderComp->aRenderable );
}


void EntSys_Renderable::ComponentUpdated( Entity sEntity, void* spData )
{
	PROF_SCOPE();

	if ( Game_ProcessingServer() )
		return;

	// UpdateModelHandle( modelInfo );

	auto renderComp = static_cast< CRenderable* >( spData );

	Renderable_t* renderData = graphics->GetRenderableData( renderComp->aRenderable );

	if ( !renderData )
	{
		// no need to update the handle if we're creating it
		renderData = Ent_CreateRenderable( sEntity );
		graphics->UpdateRenderableAABB( renderComp->aRenderable );
		return;
	}

	renderData->aTestVis    = renderComp->aTestVis;
	renderData->aCastShadow = renderComp->aCastShadow;
	renderData->aVisible    = renderComp->aVisible;
	
	// Compare Handles
	if ( renderComp->aModel != renderData->aModel )
	{
		renderData->aModel = renderComp->aModel;
		graphics->UpdateRenderableAABB( renderComp->aRenderable );
	}
}


void EntSys_Renderable::Update()
{
	PROF_SCOPE();

	if ( Game_ProcessingServer() )
		return;

	for ( Entity entity : aEntities )
	{
		// TODO: check if any of the transforms are dirty, including the parents, unsure how that would work
		glm::mat4 matrix;
		if ( !GetEntitySystem()->GetWorldMatrix( matrix, entity ) )
			continue;

		auto renderComp = Ent_GetComponent< CRenderable >( entity, "renderable" );

		if ( !renderComp )
		{
			Log_Warn( "oops\n" );
			continue;
		}

		Renderable_t* renderData = graphics->GetRenderableData( renderComp->aRenderable );

		if ( !renderData )
		{
			Log_Warn( "oops2\n" );
			continue;
		}

		renderData->aModelMatrix = matrix;
		graphics->UpdateRenderableAABB( renderComp->aRenderable );
	}
}


EntSys_Renderable* gEntSys_Renderable[ 2 ] = { 0, 0 };


EntSys_Renderable* GetRenderableEntSys()
{
	int i = Game_ProcessingClient() ? 1 : 0;
	CH_ASSERT( gEntSys_Renderable[ i ] );
	return gEntSys_Renderable[ i ];
}

