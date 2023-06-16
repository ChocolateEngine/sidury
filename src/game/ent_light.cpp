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

	Assert( light->apLight );

	if ( light->apLight )
	{
		light->apLight->aType     = light->aType;
		light->apLight->aColor    = light->aColor;
		light->apLight->aPos      = light->aPos;
		light->apLight->aAng      = light->aAng;
		light->apLight->aInnerFov = light->aInnerFov;
		light->apLight->aOuterFov = light->aOuterFov;
		light->apLight->aRadius   = light->aRadius;
		light->apLight->aLength   = light->aLength;
		light->apLight->aShadow   = light->aShadow;
		light->apLight->aEnabled  = light->aEnabled;

		Graphics_UpdateLight( light->apLight );
	}
}


void LightSystem::Update()
{
	if ( Game_ProcessingServer() )
		return;

#if 0
	for ( Entity entity : aEntities )
	{
		auto light = Ent_GetComponent< CLight >( entity, "light" );

		if ( !light )
			continue;

		Assert( light->apLight );
		
		if ( light->apLight )
		{
			// Very inefficient every frame, once you have a way of doing more efficent component updates, you have to make use of that here
			memcpy( light->apLight, light, sizeof( Light_t ) );
			Graphics_UpdateLight( light->apLight );
		}
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
}


void EntSys_ModelInfo::ComponentRemoved( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;
}


void EntSys_ModelInfo::ComponentUpdated( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;
}


void EntSys_ModelInfo::Update()
{
	if ( Game_ProcessingServer() )
		return;
}


EntSys_ModelInfo* gEntSys_ModelInfo[ 2 ] = { 0, 0 };


// ------------------------------------------------------------


void EntSys_Transform::Update()
{
	if ( Game_ProcessingServer() )
		return;

	if ( r_debug_draw_transforms )
	{
		for ( Entity entity : aEntities )
		{
			auto transform = Ent_GetComponent< CTransform >( entity, "transform" );

			if ( !transform )
				continue;

			Graphics_DrawAxis( transform->aPos, transform->aAng, transform->aScale );
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

