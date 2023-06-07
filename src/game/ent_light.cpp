#include "game_shared.h"
#include "ent_light.h"
#include "graphics/graphics.h"
#include "graphics/lighting.h"


LightSystem::LightSystem()
{
}


LightSystem::~LightSystem()
{
}


void LightSystem::ComponentAdded( Entity sEntity )
{
	// light->aType will not be initialized yet smh

	// if ( Game_ProcessingServer() )
	// 	return;
	// 
	// auto light = Ent_GetComponent< CLight >( sEntity, "light" );
	// 
	// if ( light )
	// 	light->apLight = Graphics_CreateLight( light->aType );
}


void LightSystem::ComponentRemoved( Entity sEntity )
{
	if ( Game_ProcessingServer() )
		return;

	auto light = Ent_GetComponent< CLight >( sEntity, "light" );

	if ( light )
		Graphics_DestroyLight( light->apLight );
}


void LightSystem::Update()
{
	if ( Game_ProcessingServer() )
		return;

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
}


LightSystem* gLightEntSystems[ 2 ] = { 0, 0 };


LightSystem* GetLightEntSys()
{
	int i = Game_ProcessingClient() ? 1 : 0;
	Assert( gLightEntSystems[ i ] );
	return gLightEntSystems[ i ];
}

