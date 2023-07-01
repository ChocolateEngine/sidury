#include "main.h"
#include "game_shared.h"
#include "entity.h"
#include "world.h"
#include "util.h"
#include "player.h"  // TEMP - for CPlayerMoveData

#include "entity.h"
#include "entity_systems.h"
#include "mapmanager.h"
#include "igui.h"

#include "graphics/graphics.h"

#include "game_physics.h"  // just for IPhysicsShape* and IPhysicsObject*


// ====================================================================================================
// Base Components
// 
// TODO:
//  - add an option to define what components and component variables get saved to a map/scene
//  - also a random thought - maybe you could use a map/scene to create a preset entity prefab to create?
//      like a preset player entity to spawn in when a player loads
// ====================================================================================================


void Ent_RegisterVarHandlers()
{
}


CH_STRUCT_REGISTER_COMPONENT( CRigidBody, rigidBody, true, EEntComponentNetType_Both )
{
	CH_REGISTER_COMPONENT_VAR2( EEntComponentVarType_Vec3, glm::vec3, aVel, vel );
	CH_REGISTER_COMPONENT_VAR2( EEntComponentVarType_Vec3, glm::vec3, aAccel, accel );
}


CH_STRUCT_REGISTER_COMPONENT( CDirection, direction, true, EEntComponentNetType_Both )
{
	CH_REGISTER_COMPONENT_VAR2( EEntComponentVarType_Vec3, glm::vec3, aForward, forward );
	CH_REGISTER_COMPONENT_VAR2( EEntComponentVarType_Vec3, glm::vec3, aUp, up );
	CH_REGISTER_COMPONENT_VAR2( EEntComponentVarType_Vec3, glm::vec3, aRight, right );
}


// TODO: use a "protocol system", so an internally created model path would be this:
// "internal://model_0"
// and a file on the disk to load will be this:
// "file://path/to/asset.glb"
CH_STRUCT_REGISTER_COMPONENT( CRenderable, renderable, true, EEntComponentNetType_Both )
{
	CH_REGISTER_COMPONENT_VAR2( EEntComponentVarType_StdString, std::string, aPath, path );
	CH_REGISTER_COMPONENT_VAR2( EEntComponentVarType_Bool, bool, aTestVis, testVis );
	CH_REGISTER_COMPONENT_VAR2( EEntComponentVarType_Bool, bool, aCastShadow, castShadow );
	CH_REGISTER_COMPONENT_VAR2( EEntComponentVarType_Bool, bool, aVisible, visible );
	
	CH_REGISTER_COMPONENT_SYS2( EntSys_Renderable, gEntSys_Renderable );
}


void Ent_RegisterBaseComponents()
{
	Ent_RegisterVarHandlers();

	// Setup Types, only used for registering variables without specifing the VarType
	gEntComponentRegistry.aVarTypes[ typeid( bool ).hash_code() ]        = EEntComponentVarType_Bool;
	gEntComponentRegistry.aVarTypes[ typeid( float ).hash_code() ]       = EEntComponentVarType_Float;
	gEntComponentRegistry.aVarTypes[ typeid( double ).hash_code() ]      = EEntComponentVarType_Double;

	gEntComponentRegistry.aVarTypes[ typeid( s8 ).hash_code() ]          = EEntComponentVarType_S8;
	gEntComponentRegistry.aVarTypes[ typeid( s16 ).hash_code() ]         = EEntComponentVarType_S16;
	gEntComponentRegistry.aVarTypes[ typeid( s32 ).hash_code() ]         = EEntComponentVarType_S32;
	gEntComponentRegistry.aVarTypes[ typeid( s64 ).hash_code() ]         = EEntComponentVarType_S64;

	gEntComponentRegistry.aVarTypes[ typeid( u8 ).hash_code() ]          = EEntComponentVarType_U8;
	gEntComponentRegistry.aVarTypes[ typeid( u16 ).hash_code() ]         = EEntComponentVarType_U16;
	gEntComponentRegistry.aVarTypes[ typeid( u32 ).hash_code() ]         = EEntComponentVarType_U32;
	gEntComponentRegistry.aVarTypes[ typeid( u64 ).hash_code() ]         = EEntComponentVarType_U64;

	// probably overrides type have of u64, hmmm
	// gEntComponentRegistry.aVarTypes[ typeid( Entity ).hash_code() ]      = EEntComponentVarType_Entity;
	gEntComponentRegistry.aVarTypes[ typeid( std::string ).hash_code() ] = EEntComponentVarType_StdString;

	gEntComponentRegistry.aVarTypes[ typeid( glm::vec2 ).hash_code() ]   = EEntComponentVarType_Vec2;
	gEntComponentRegistry.aVarTypes[ typeid( glm::vec3 ).hash_code() ]   = EEntComponentVarType_Vec3;
	gEntComponentRegistry.aVarTypes[ typeid( glm::vec4 ).hash_code() ]   = EEntComponentVarType_Vec4;

	// Now Register Base Components
	EntComp_RegisterComponent< CTransform >(
	  "transform", true, EEntComponentNetType_Both,
	  [ & ]()
	  {
		  auto transform           = new CTransform;
		  transform->aScale.Edit() = { 1.f, 1.f, 1.f };
		  return transform;
	  },
	  [ & ]( void* spData )
	  { delete (CTransform*)spData; } );

	EntComp_RegisterComponentVar< CTransform, glm::vec3 >( "pos", offsetof( CTransform, aPos ), typeid( CTransform::aPos ).hash_code() );
	EntComp_RegisterComponentVar< CTransform, glm::vec3 >( "ang", offsetof( CTransform, aAng ), typeid( CTransform::aAng ).hash_code() );
	EntComp_RegisterComponentVar< CTransform, glm::vec3 >( "scale", offsetof( CTransform, aScale ), typeid( CTransform::aScale ).hash_code() );
	CH_REGISTER_COMPONENT_SYS( CTransform, EntSys_Transform, gEntSys_Transform );

	// CH_REGISTER_COMPONENT_RW( CRigidBody, rigidBody, true );
	// CH_REGISTER_COMPONENT_VAR( CRigidBody, glm::vec3, aVel, vel );
	// CH_REGISTER_COMPONENT_VAR( CRigidBody, glm::vec3, aAccel, accel );

	// CH_REGISTER_COMPONENT_RW( CDirection, direction, true );
	// CH_REGISTER_COMPONENT_VAR( CDirection, glm::vec3, aForward, forward );
	// CH_REGISTER_COMPONENT_VAR( CDirection, glm::vec3, aUp, up );
	// // CH_REGISTER_COMPONENT_VAR( CDirection, glm::vec3, aRight, right );
	// CH_REGISTER_COMP_VAR_VEC3( CDirection, aRight, right );

	// CH_REGISTER_COMPONENT_RW( CGravity, gravity, true );
	// CH_REGISTER_COMP_VAR_VEC3( CGravity, aForce, force );

	// might be a bit weird
	// HACK HACK: DONT OVERRIDE CLIENT VALUE, IT WILL NEVER BE UPDATED
	CH_REGISTER_COMPONENT_RW( CCamera, camera, false );
	CH_REGISTER_COMPONENT_VAR( CCamera, float, aFov, fov );
	
	CH_REGISTER_COMPONENT( CMap, map, true, EEntComponentNetType_Both );

	// Probably should be in graphics?
	CH_REGISTER_COMPONENT_RW( CLight, light, true );
	CH_REGISTER_COMPONENT_SYS( CLight, LightSystem, gLightEntSystems );
	CH_REGISTER_COMPONENT_VAR( CLight, ELightType, aType, type );
	CH_REGISTER_COMPONENT_VAR( CLight, glm::vec4, aColor, color );

	// TODO: these 2 should not be here
    // it should be attached to it's own entity that can be parented
    // and that entity needs to contain the transform (or transform small) component
	CH_REGISTER_COMPONENT_VAR( CLight, glm::vec3, aPos, pos );
	CH_REGISTER_COMPONENT_VAR( CLight, glm::vec3, aAng, ang );

	CH_REGISTER_COMPONENT_VAR( CLight, float, aInnerFov, innerFov );
	CH_REGISTER_COMPONENT_VAR( CLight, float, aOuterFov, outerFov );
	CH_REGISTER_COMPONENT_VAR( CLight, float, aRadius, radius );
	CH_REGISTER_COMPONENT_VAR( CLight, float, aLength, length );
	CH_REGISTER_COMPONENT_VAR( CLight, bool, aShadow, shadow );
	CH_REGISTER_COMPONENT_VAR( CLight, bool, aEnabled, enabled );
	CH_REGISTER_COMPONENT_VAR( CLight, bool, aUseTransform, useTransform );
}


// Helper Functions
Handle Ent_GetRenderableHandle( Entity sEntity )
{
	auto renderComp = Ent_GetComponent< CRenderable >( sEntity, "renderable" );

	if ( !renderComp )
	{
		Log_Error( "Failed to get renderable component\n" );
		return InvalidHandle;
	}

	return renderComp->aRenderable;
}


Renderable_t* Ent_GetRenderable( Entity sEntity )
{
	auto renderComp = Ent_GetComponent< CRenderable >( sEntity, "renderable" );

	if ( !renderComp )
	{
		Log_Error( "Failed to get renderable component\n" );
		return nullptr;
	}

	return Graphics_GetRenderableData( renderComp->aRenderable );
}


// Requires the entity to have renderable component with a model path set
Renderable_t* Ent_CreateRenderable( Entity sEntity )
{
	auto renderComp = Ent_GetComponent< CRenderable >( sEntity, "renderable" );

	if ( !renderComp )
	{
		Log_Error( "Failed to get renderable component\n" );
		return nullptr;
	}

	if ( renderComp->aRenderable == InvalidHandle )
	{
		if ( renderComp->aModel == InvalidHandle )
		{
			renderComp->aModel = Graphics_LoadModel( renderComp->aPath );
			if ( renderComp->aModel == InvalidHandle )
			{
				Log_Error( "Failed to load model for renderable\n" );
				return nullptr;
			}
		}

		renderComp->aRenderable = Graphics_CreateRenderable( renderComp->aModel );
		if ( renderComp->aRenderable == InvalidHandle )
		{
			Log_Error( "Failed to create renderable\n" );
			return nullptr;
		}
	}

	return Graphics_GetRenderableData( renderComp->aRenderable );
}

