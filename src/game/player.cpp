#include "main.h"
#include "player.h"
#include "util.h"
#include "skybox.h"
#include "inputsystem.h"

#include "igui.h"
#include "iinput.h"
#include "render/irender.h"
#include "graphics/graphics.h"

#include "cl_main.h"
#include "sv_main.h"
#include "game_shared.h"
#include "game_physics.h"
#include "mapmanager.h"
#include "entity_systems.h"
#include "testing.h"

#include "imgui/imgui.h"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <algorithm>
#include <cmath>

#include "ent_suit.h"


constexpr float DEFAULT_SPEED = 250.f;

CONVAR( sv_sprint_mult, 2.4, CVARF_DEF_SERVER_REPLICATED );
CONVAR( sv_duck_mult, 0.5, CVARF_DEF_SERVER_REPLICATED );

ConVar forward_speed( "sv_forward_speed", DEFAULT_SPEED, CVARF_DEF_SERVER_REPLICATED );
ConVar side_speed( "sv_side_speed", DEFAULT_SPEED, CVARF_DEF_SERVER_REPLICATED );  // 350.f
ConVar max_speed( "sv_max_speed", DEFAULT_SPEED, CVARF_DEF_SERVER_REPLICATED );    // 320.f

ConVar accel_speed( "sv_accel_speed", 10, CVARF_DEF_SERVER_REPLICATED );
ConVar accel_speed_air( "sv_accel_speed_air", 30, CVARF_DEF_SERVER_REPLICATED );
ConVar jump_force( "sv_jump_force", 250, CVARF_DEF_SERVER_REPLICATED );
ConVar stop_speed( "sv_stop_speed", 25, CVARF_DEF_SERVER_REPLICATED );

CONVAR( sv_friction, 8, CVARF_DEF_SERVER_REPLICATED );  // 4.f
CONVAR( sv_friction_enable, 1, CVARF_DEF_SERVER_REPLICATED );

CONVAR( phys_friction_player, 0.01, CVARF_DEF_SERVER_REPLICATED );
CONVAR( phys_player_offset, 40, CVARF_DEF_SERVER_REPLICATED );

// lerp the friction maybe?
//CONVAR( sv_new_movement, 1 );
//CONVAR( sv_friction_new, 8 );  // 4.f

CONVAR( sv_gravity, 800, CVARF_DEF_SERVER_REPLICATED );

CONVAR( cl_stepspeed, 200 );
CONVAR( cl_steptime, 0.25 );
CONVAR( cl_stepduration, 0.22 );

CONVAR( sv_view_height, 67, CVARF_DEF_SERVER_REPLICATED );  // 67
CONVAR( sv_view_height_duck, 36, CVARF_DEF_SERVER_REPLICATED );  // 36
CONVAR( sv_view_height_lerp, 15, CVARF_DEF_SERVER_REPLICATED );  // 0.015

CONVAR( player_model_scale, 40 );

CONVAR( cl_thirdperson, 0 );
CONVAR( cl_playermodel_enable, 1 );
CONVAR( cl_playermodel_shadow_local, 1 );
CONVAR( cl_playermodel_shadow, 0 );
CONVAR( cl_playermodel_cam_ang, 1 );
CONVAR( cl_cam_x, 0 );
CONVAR( cl_cam_y, 0 );
CONVAR( cl_cam_z, -150 );
CONVAR( cl_show_player_stats, 0 );

CONVAR( phys_dbg_player, 0 );

CONVAR( r_fov, 106.f, CVARF_ARCHIVE );
CONVAR( r_nearz, 1.f );
CONVAR( r_farz, 10000.f );

CONVAR( cl_zoom_fov, 40 );
CONVAR( cl_zoom_duration, 0.4 );

CONVAR( cl_duck_time, 0.4 );

CONVAR( sv_land_smoothing, 1 );
CONVAR( sv_land_max_speed, 1000 );
CONVAR( sv_land_vel_scale, 1 );      // 0.01
CONVAR( sv_land_power_scale, 100 );  // 0.01
CONVAR( sv_land_time_scale, 2 );

CONVAR( cl_bob_enabled, 1 );
CONVAR( cl_bob_magnitude, 2 );
CONVAR( cl_bob_freq, 4.5 );
CONVAR( cl_bob_speed_scale, 0.013 );
CONVAR( cl_bob_exit_lerp, 0.1 );
CONVAR( cl_bob_exit_threshold, 0.1 );
CONVAR( cl_bob_sound_threshold, 0.1 );
CONVAR( cl_bob_offset, 0.25 );
CONVAR( cl_bob_time_offset, -0.6 );
CONVAR( cl_bob_debug, 0 );

CONVAR( r_flashlight_brightness, 10.f );
CONVAR( r_flashlight_lock, 0.f );
CONVAR( r_flashlight_offset_x, -4.f );
CONVAR( r_flashlight_offset_y, -4.f );
CONVAR( r_flashlight_offset_z, -4.f );

extern ConVar   m_yaw, m_pitch;

extern Entity   gLocalPlayer;

constexpr float PLAYER_MASS = 200.f;


CONCMD_VA( respawn, CVARF( CL_EXEC ) )
{
	if ( CL_SendConVarIfClient( "respawn", args ) )
		return;

	Entity player = SV_GetCommandClientEntity();

	GetPlayers()->Respawn( player );
}


CONCMD_VA( reset_velocity, CVARF( CL_EXEC ) )
{
	if ( CL_SendConVarIfClient( "reset_velocity", args ) )
		return;

	Entity player  = SV_GetCommandClientEntity();
	auto rigidBody = GetRigidBody( player );

	if ( !rigidBody )
		return;

	rigidBody->aVel.Edit() = { 0, 0, 0 };
}


static void CmdSetPlayerMoveType( Entity sPlayer, PlayerMoveType sMoveType )
{
	// auto& move = GetEntitySystem()->GetComponent< CPlayerMoveData >( sPlayer );
	// SetMoveType( move, sMoveType );

	auto move = GetPlayerMoveData( sPlayer );

	if ( !move )
	{
		Log_Error( "Failed to find player move data component\n" );
		return;
	}

	if ( !GetPlayers()->SetCurrentPlayer( sPlayer ) )
		return;

	// Toggle between the desired move type and walking
	if ( move->aMoveType == sMoveType )
		GetPlayers()->apMove->SetMoveType( *move, PlayerMoveType::Walk );
	else
		GetPlayers()->apMove->SetMoveType( *move, sMoveType );
}


CONCMD_VA( noclip, CVARF( CL_EXEC ) )
{
	if ( CL_SendConVarIfClient( "noclip" ) )
		return;

	Entity player = SV_GetCommandClientEntity();

	if ( !player )
		return;

	CmdSetPlayerMoveType( player, PlayerMoveType::NoClip );
}


CONCMD_VA( fly, CVARF( CL_EXEC ) )
{
	// Forward to server if we are the client
	if ( CL_SendConVarIfClient( "fly" ) )
		return;

	Entity player = SV_GetCommandClientEntity();

	if ( !player )
		return;

	CmdSetPlayerMoveType( player, PlayerMoveType::Fly );
}


// glm::normalize doesn't return a float
// move to util.cpp?
float vec3_norm(glm::vec3& v)
{
	float length = sqrt(glm::dot(v, v));

	if (length)
		v *= 1/length;

	return length;
}


// ============================================================


#define CH_PLAYER_SV 0
#define CH_PLAYER_CL 1


static PlayerManager*      players[ 2 ]     = { 0, 0 };
static PlayerSpawnManager* playerSpawn[ 2 ] = { 0, 0 };


PlayerManager* GetPlayers()
{
	int i = Game_ProcessingClient() ? CH_PLAYER_CL : CH_PLAYER_SV;
	Assert( players[ i ] );
	return players[ i ];
}


PlayerManager::PlayerManager()
{
	apMove = new PlayerMovement;
}

PlayerManager::~PlayerManager()
{
	if ( apMove )
		delete apMove;

	apMove = nullptr;
}


CH_STRUCT_REGISTER_COMPONENT( CPlayerMoveData, playerMoveData, true, EEntComponentNetType_Both, false )
{
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_S32, EPlayerMoveType, aMoveType, moveType, false );
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_U8, unsigned char, aPlayerFlags, playerFlags, false );
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_U8, unsigned char, aPrevPlayerFlags, prevPlayerFlags, false );

	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aMaxSpeed, maxSpeed, false );

	// View Bobbing
	//CH_REGISTER_COMPONENT_VAR( EEntNetField_Float, float, aWalkTime, walkTime, true );
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aBobOffsetAmount, bobOffsetAmount, false );
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aPrevViewTilt, prevViewTilt, false );

	// Smooth Land
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aLandPower, landPower, false );
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aLandTime, landTime, false );

	// Smooth Duck
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aPrevViewHeight, prevViewHeight, false );
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aTargetViewHeight, targetViewHeight, false );
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aOutViewHeight, outViewHeight, false );
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aDuckDuration, duckDuration, false );
	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aDuckTime, duckTime, false );

	CH_REGISTER_COMPONENT_VAR2( EEntNetField_Float, float, aLastStepTime, lastStepTime, false );
}


CH_STRUCT_REGISTER_COMPONENT( CPlayerSpawn, playerSpawn, true, EEntComponentNetType_Both, true )
{
	CH_REGISTER_COMPONENT_SYS2( PlayerSpawnManager, playerSpawn );
}


void PlayerManager::RegisterComponents()
{
	CH_REGISTER_COMPONENT( CPlayerInfo, playerInfo, true, EEntComponentNetType_Both, false );
	CH_REGISTER_COMPONENT_SYS( CPlayerInfo, PlayerManager, players );
	// CH_REGISTER_COMPONENT_VAR( CPlayerInfo, std::string, aName, name );
	CH_REGISTER_COMPONENT_VAR_EX( CPlayerInfo, EEntNetField_Entity, Entity, aCamera, camera, false );
	CH_REGISTER_COMPONENT_VAR_EX( CPlayerInfo, EEntNetField_Entity, Entity, aFlashlight, flashlight, false );
	CH_REGISTER_COMPONENT_VAR( CPlayerInfo, bool, aIsLocalPlayer, isLocalPlayer, false );  // don't mess with this

	CH_REGISTER_COMPONENT_RW( CPlayerZoom, playerZoom, false, false );
	CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aOrigFov, origFov, false );
	CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aNewFov, newFov, false );
	// CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aZoomChangeFov, zoomChangeFov );
	// CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aZoomTime, zoomTime );
	// CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aZoomDuration, zoomDuration );
	CH_REGISTER_COMPONENT_VAR( CPlayerZoom, bool, aWasZoomed, wasZoomed, false );

	// GetEntitySystem()->RegisterComponent< Model* >();
	// GetEntitySystem()->RegisterComponent< Model >();
	//GetEntitySystem()->RegisterComponent<PhysicsObject*>();
}


void PlayerManager::ComponentAdded( Entity sEntity, void* spData )
{
	Create( sEntity );
}


void PlayerManager::ComponentUpdated( Entity sEntity, void* spData )
{
	if ( Game_ProcessingServer() )
		return;

	CPlayerInfo* playerInfo = static_cast< CPlayerInfo* >( spData );

	if ( playerInfo->aCamera != CH_ENT_INVALID )
	{
		// Parent the Entities to the Player Entity
		GetEntitySystem()->ParentEntity( playerInfo->aCamera, sEntity );

		GetEntitySystem()->SetComponentPredicted( playerInfo->aCamera, "transform", true );
		GetEntitySystem()->SetComponentPredicted( playerInfo->aCamera, "direction", true );
		GetEntitySystem()->SetComponentPredicted( playerInfo->aCamera, "camera", true );
	}
}


bool PlayerManager::SetCurrentPlayer( Entity player )
{
	Assert( apMove );

	apMove->aPlayer = player;

	if ( Game_ProcessingClient() )
	{
		apMove->apUserCmd = &gClientUserCmd;
	}
	else
	{
		SV_Client_t* client = SV_GetClientFromEntity( player );
		if ( !client )
			return false;

		apMove->apUserCmd = &client->aUserCmd;
	}

	CPlayerInfo* playerInfo = GetPlayerInfo( player );

	if ( !playerInfo )
	{
		Log_Error( "playerInfo component not found on player entity\n" );
		return false;
	}

	Assert( playerInfo );
	Assert( playerInfo->aCamera );

	apMove->apCamTransform = GetTransform( playerInfo->aCamera );
	apMove->apCamDir       = GetComp_Direction( playerInfo->aCamera );
	apMove->apCamera       = GetCamera( playerInfo->aCamera );

	apMove->apDir          = Ent_GetComponent< CDirection >( player, "direction" );
	apMove->apRigidBody    = GetRigidBody( player );
	apMove->apTransform    = GetTransform( player );
	apMove->apPhysShape    = GetComp_PhysShapePtr( player );
	apMove->apPhysObjComp  = GetComp_PhysObject( player );
	apMove->apPhysObj      = apMove->apPhysObjComp->apObj;

	Assert( apMove->apDir );
	Assert( apMove->apRigidBody );
	Assert( apMove->apTransform );
	Assert( apMove->apPhysShape );
	Assert( apMove->apPhysObjComp );
	Assert( apMove->apPhysObj );

	Assert( apMove->apCamTransform );
	Assert( apMove->apCamDir );
	Assert( apMove->apCamera );

	return true;
}


void PlayerManager::Init()
{
	// apMove = GetEntitySystem()->RegisterSystem<PlayerMovement>();
}


void PlayerManager::Create( Entity player )
{
	GetEntitySystem()->SetAllowSavingToMap( player, false );

	CPlayerInfo* playerInfo = Ent_GetComponent< CPlayerInfo >( player, "playerInfo" );
	Assert( playerInfo );

	// Add Components to entity
	Ent_AddComponent( player, "playerMoveData" );
	Ent_AddComponent( player, "rigidBody" );
	Ent_AddComponent( player, "direction" );

	auto renderable   = Ent_AddComponent< CRenderable >( player, "renderable" );
	renderable->aPath = DEFAULT_PROTOGEN_PATH;

	auto flashlight   = Ent_AddComponent< CLight >( player, "light" );
	auto zoom         = Ent_AddComponent< CPlayerZoom >( player, "playerZoom" );
	auto transform    = Ent_AddComponent< CTransform >( player, "transform" );
	auto health       = Ent_AddComponent< CHealth >( player, "health" );
	auto suit         = Ent_AddComponent< CSuit >( player, "suit" );

	Assert( flashlight );
	Assert( zoom );
	Assert( transform );
	Assert( health );
	Assert( suit );

	health->aHealth = 100;

	if ( Game_ProcessingClient() )
	{
		playerInfo->aIsLocalPlayer = player == gLocalPlayer;

		Log_Msg( "Client Creating Local Player\n" );
	}
	else
	{
		SV_Client_t* client = SV_GetClientFromEntity( player );

		Assert( client );

		if ( !client )
			return;

		// Lets create local entities for the camera and the flashlight, so they have their own unique transform in local space
		// And we parent them to the player
		playerInfo->aCamera = GetEntitySystem()->CreateEntity();
		// playerInfo->aFlashlight = GetEntitySystem()->CreateEntity( true );

		// Parent the Entities to the Player Entity
		GetEntitySystem()->ParentEntity( playerInfo->aCamera, player );
		// GetEntitySystem()->ParentEntity( playerInfo->aFlashlight, player );

		Ent_AddComponent( playerInfo->aCamera, "transform" );
		Ent_AddComponent( playerInfo->aCamera, "direction" );
		Ent_AddComponent( playerInfo->aCamera, "camera" );

		Log_MsgF( "Server Creating Player Entity: \"%s\"\n", client->aName.c_str() );
	}

	zoom->aOrigFov             = r_fov.GetFloat();
	zoom->aNewFov              = r_fov.GetFloat();

	flashlight->aType          = ELightType_Cone;
	flashlight->aInnerFov      = 0.f;
	flashlight->aOuterFov      = 45.f;
	// flashlight->aColor    = { r_flashlight_brightness.GetFloat(), r_flashlight_brightness.GetFloat(), r_flashlight_brightness.GetFloat() };
	flashlight->aColor.Edit()  = { 1.f, 1.f, 1.f, r_flashlight_brightness.GetFloat() };

	auto compPhysShape        = Ent_AddComponent< CPhysShape >( player, "physShape" );
	compPhysShape->aShapeType = PhysShapeType::Cylinder;
	compPhysShape->aBounds    = glm::vec3( 72, 16, 1 );

	Phys_CreatePhysShapeComponent( compPhysShape );

	IPhysicsShape* physShape = compPhysShape->apShape;

	Assert( physShape );

	PhysicsObjectInfo physInfo;
	physInfo.aMotionType = PhysMotionType::Dynamic;
	physInfo.aPos = transform->aPos;
	physInfo.aAng = transform->aAng;

	physInfo.aCustomMass = true;
	physInfo.aMass = PLAYER_MASS;

	CPhysObject* physObj = Phys_CreateObject( player, physShape, physInfo );

	Assert( physObj );
}


void PlayerManager::Spawn( Entity player )
{
	apMove->OnPlayerSpawn( player );

	Respawn( player );
}


void PlayerManager::Respawn( Entity player )
{
	CPlayerInfo* playerInfo = GetPlayerInfo( player );

	if ( !playerInfo )
	{
		Log_Error( "playerInfo component not found on player entity\n" );
		return;
	}

	auto         rigidBody  = GetRigidBody( player );
	auto         transform  = GetTransform( player );
	auto         zoom       = GetPlayerZoom( player );
	auto         physObjComp    = GetComp_PhysObject( player );

	Assert( playerInfo );
	Assert( playerInfo->aCamera );

	auto camTransform = GetTransform( playerInfo->aCamera );

	Assert( rigidBody );
	Assert( transform );
	Assert( camTransform );
	Assert( zoom );
	Assert( physObjComp );

	IPhysicsObject* physObj = physObjComp->apObj;

	Assert( physObj );

	Transform playerSpawnSpot = GetPlayerSpawn()->SelectSpawnTransform();

	transform->aPos               = playerSpawnSpot.aPos;
	transform->aAng.Edit()        = { 0, playerSpawnSpot.aAng.y, 0 };
	rigidBody->aVel.Edit()        = { 0, 0, 0 };
	rigidBody->aAccel.Edit()      = { 0, 0, 0 };

	camTransform->aAng.Edit()     = playerSpawnSpot.aAng;

	zoom->aOrigFov                = r_fov.GetFloat();
	zoom->aNewFov                 = r_fov.GetFloat();

	physObjComp->aTransformMode   = EPhysTransformMode_None;

	physObj->SetLinearVelocity( { 0, 0, 0 } );

	physObj->SetAllowSleeping( false );
	physObj->SetMotionQuality( PhysMotionQuality::LinearCast );
	physObj->SetLinearVelocity( { 0, 0, 0 } );
	physObj->SetAngularVelocity( { 0, 0, 0 } );

	Phys_SetMaxVelocities( physObj );

	physObj->SetFriction( phys_friction_player );

	// Don't allow any rotation on this
	physObj->SetInverseMass( 1.f / PLAYER_MASS );
	physObj->SetInverseInertia( { 0, 0, 0 }, { 1, 0, 0, 0 } );

	// rotate 90 degrees
	// physObj->SetAng( { 90, transform->aAng.Get().y, 0 } );
	physObj->SetAng( { 90, 0, 0 } );

	apMove->OnPlayerRespawn( player );
}


void Player_UpdateFlashlight( Entity player, bool sToggle )
{
	PROF_SCOPE();

	CPlayerInfo* playerInfo = GetPlayerInfo( player );

	if ( !playerInfo )
	{
		Log_Error( "playerInfo component not found on player entity\n" );
		return;
	}

	Assert( playerInfo->aCamera );

	CTransform* transform    = GetTransform( player );
	CLight*     flashlight   = Ent_GetComponent< CLight >( player, "light" );

	CTransform* camTransform = GetTransform( playerInfo->aCamera );
	auto        camDir       = Ent_GetComponent< CDirection >( playerInfo->aCamera, "direction" );

	Assert( transform );
	Assert( camTransform );
	Assert( camDir );
	Assert( flashlight );

	auto UpdateTransform = [ & ]()
	{
		// weird stuff to get the angle of the light correct
		flashlight->aAng.Edit().x = camTransform->aAng.Get().z;
		flashlight->aAng.Edit().y = -camTransform->aAng.Get().y;
		flashlight->aAng.Edit().z = -camTransform->aAng.Get().x + 90.f;

		flashlight->aPos          = transform->aPos.Get() + camTransform->aPos.Get();

		glm::vec3 offset( r_flashlight_offset_x.GetFloat(), r_flashlight_offset_y.GetFloat(), r_flashlight_offset_z.GetFloat() );

		flashlight->aPos += offset * camDir->aUp.Get();
	};

	// Toggle flashlight on or off
	if ( sToggle )
	{
		flashlight->aEnabled = !flashlight->aEnabled;

		if ( !flashlight->aEnabled )
		{
			// Graphics_UpdateLight( flashlight );
		}
		else
		{
			UpdateTransform();
		}
	}

	if ( flashlight->aEnabled )
	{
		// flashlight->aColor = { r_flashlight_brightness.GetFloat(), r_flashlight_brightness.GetFloat(), r_flashlight_brightness.GetFloat() };
		flashlight->aColor.Edit() = { 1.f, 1.f, 1.f, r_flashlight_brightness.GetFloat() };

		if ( !r_flashlight_lock.GetBool() )
		{
			UpdateTransform();
		}

		// Graphics_UpdateLight( flashlight );
	}
}


inline float DegreeConstrain( float num )
{
	num = std::fmod(num, 360.0f);
	return (num < 0.0f) ? num += 360.0f : num;
}


inline void ClampAngles( CTransform* spTransform )
{
	if ( !spTransform )
		return;

	spTransform->aAng.Edit()[ YAW ]   = DegreeConstrain( spTransform->aAng.Get()[ YAW ] );
	spTransform->aAng.Edit()[ PITCH ] = std::clamp( spTransform->aAng.Get()[ PITCH ], -90.0f, 90.0f );
};


void PlayerManager::Update( float frameTime )
{
	PROF_SCOPE();

	for ( Entity player: aEntities )
	{
		SV_Client_t* client = SV_GetClientFromEntity( player );
		if ( !client )
			continue;

		UserCmd_t& userCmd    = client->aUserCmd;

		auto playerInfo = GetPlayerInfo( player );
		// auto camera     = GetCamera( player );

		Assert( playerInfo );
		Assert( playerInfo->aCamera );

		if ( !Game_IsPaused() )
		{
			// Update Client UserCmd
			auto transform    = GetTransform( player );
			auto camTransform = GetTransform( playerInfo->aCamera );

			Assert( transform );
			Assert( camTransform );

			// transform.aAng[PITCH] = -mouse.y;
			camTransform->aAng.Edit()[ PITCH ] = userCmd.aAng[ PITCH ];
			camTransform->aAng.Edit()[ YAW ]   = userCmd.aAng[ YAW ];
			camTransform->aAng.Edit()[ ROLL ]  = userCmd.aAng[ ROLL ];

			transform->aAng.Set( { 0.f, DegreeConstrain( userCmd.aAng[ YAW ] ), 0.f } );

			ClampAngles( camTransform );

			apMove->MovePlayer( player, &userCmd );
			Player_UpdateFlashlight( player, userCmd.aFlashlight );
		}

		UpdateView( playerInfo, player );
	}
}


void PlayerManager::UpdateLocalPlayer()
{
	Assert( Game_ProcessingClient() );
	Assert( apMove );

	auto userCmd = gClientUserCmd;

	// We still need to do client updating of players actually, so

	for ( Entity player : aEntities )
	{
		auto playerInfo = GetPlayerInfo( player );
		Assert( playerInfo );
		Assert( playerInfo->aCamera );

		auto     playerMove = GetPlayerMoveData( player );
		auto     transform  = GetTransform( player );
		CLight*  flashlight = Ent_GetComponent< CLight >( player, "light" );

		auto     camTransform = GetTransform( playerInfo->aCamera );
		// auto     camera     = GetCamera( playerInfo->aCamera );

		Assert( playerMove );
		Assert( camTransform );
		//Assert( camera );
		Assert( transform );
		Assert( flashlight );

		apMove->SetPlayer( player );

		// TODO: prediction
		// apMove->MovePlayer( gLocalPlayer, &userCmd );
	
		// Player_UpdateFlashlight( player, &userCmd );
	
		// TEMP
		camTransform->aPos.Edit()[ W_UP ] = playerMove->aOutViewHeight;

		// bool wasOnGround = playerMove->aPrevPlayerFlags & PlyOnGround && !( playerMove->aPlayerFlags & PlyOnGround );
		bool wasOnGround = playerMove->aPrevPlayerFlags & PlyOnGround;

		if ( playerMove->aMoveType == PlayerMoveType::Walk )
		{
			apMove->DoSmoothLand( wasOnGround );
			apMove->DoViewBob();
			apMove->DoViewTilt();
		}

		UpdateView( playerInfo, player );

		// if ( ( cl_thirdperson.GetBool() && cl_playermodel_enable.GetBool() ) || !playerInfo->aIsLocalPlayer )
		if ( cl_playermodel_enable && ( cl_thirdperson.GetBool() || !playerInfo->aIsLocalPlayer ) )
		{
			auto renderComp = Ent_GetComponent< CRenderable >( player, "renderable" );

			// I hate this so much
			if ( renderComp->aRenderable == InvalidHandle )
			{
				if ( renderComp->aPath.Get().empty() )
					continue;

				if ( renderComp->aModel == InvalidHandle )
				{
					renderComp->aModel = Graphics_LoadModel( renderComp->aPath );
					if ( renderComp->aModel == InvalidHandle )
						continue;
				}

				renderComp->aRenderable = Graphics_CreateRenderable( renderComp->aModel );
				if ( renderComp->aRenderable == InvalidHandle )
					continue;
			}
			
			Renderable_t* renderData = Graphics_GetRenderableData( renderComp->aRenderable );

			if ( !renderData )
				continue;

			renderComp->aVisible = true;
			renderData->aVisible = true;

			if ( cl_playermodel_shadow )
				renderData->aCastShadow = cl_playermodel_shadow_local ? true : player != gLocalPlayer;
			else
				renderData->aCastShadow = false;

			float     scaleBase = player_model_scale.GetFloat();

			// This is to squish the model when the player crouches
			float     scaleMult = playerMove->aOutViewHeight / sv_view_height;
			
			glm::vec3 scale( scaleBase, scaleBase, scaleBase );
			scale.y *= scaleMult;

			// HACK HACK
			glm::vec3 ang{};

			if ( cl_playermodel_cam_ang )
			{
				ang = camTransform->aAng;
				//ang[ YAW ] *= -1;
				ang[ YAW ] += 180;
				ang[ ROLL ]  = ang[ PITCH ] + 90;
				ang[ PITCH ] = 0;
			}
			else
			{
				ang = transform->aAng;
				ang[ ROLL ] += 90;
				//ang[ YAW ] *= -1;
				ang[ YAW ] += 180;
			}

			// Util_ToMatrix( renderData->aModelMatrix, transform->aPos, ang, scale );

			// Is something wrong with my Util_ToMatrix function?
			// Do i have applying scale and rotation in the wrong order?
			// If so, why is everything else using it seemingly working fine so far?

			renderData->aModelMatrix = glm::translate( transform->aPos.Get() );

			renderData->aModelMatrix *= glm::eulerAngleYZX(
			  glm::radians( ang.x ),
			  glm::radians( ang.y ),
			  glm::radians( ang.z ) );

			renderData->aModelMatrix = glm::scale( renderData->aModelMatrix, scale );

			Graphics_UpdateRenderableAABB( renderComp->aRenderable );
		}
		else
		{
			// Make sure this thing is hidden
			auto renderComp = Ent_GetComponent< CRenderable >( player, "renderable" );
			if ( renderComp->aRenderable == InvalidHandle )
				continue;

			Renderable_t* renderData = Graphics_GetRenderableData( renderComp->aRenderable );
			if ( !renderData )
				continue;

			renderComp->aVisible = false;
			renderData->aVisible = false;
		}
	}
}


void PlayerManager::DoMouseLook( Entity player )
{
	Assert( Game_ProcessingClient() );

	CPlayerInfo* playerInfo = GetPlayerInfo( player );
	Assert( playerInfo );
	Assert( playerInfo->aCamera );

	if ( !playerInfo )
	{
		Log_Error( "playerInfo compoment not found when applying mouse look\n" );
		return;
	}

	CTransform*     camTransform = GetTransform( playerInfo->aCamera );
	CTransform*     transform    = GetTransform( player );

	const glm::vec2 mouse = Input_GetMouseDelta();

	// transform.aAng[PITCH] = -mouse.y;
	camTransform->aAng.Edit()[ PITCH ] += mouse.y * m_pitch;
	camTransform->aAng.Edit()[ YAW ] += mouse.x * m_yaw;
	transform->aAng.Edit()[ YAW ] += mouse.x * m_yaw;

	ClampAngles( camTransform );
	ClampAngles( transform );
}


// TODO: Move this to some generel math file or whatever

float Lerp_GetDuration( float max, float min, float current )
{
	return (current - min) / (max - min);
}

float Lerp_GetDuration( float max, float min, float current, float mult )
{
	return ((current - min) / (max - min)) * mult;
}

// inverted version of it
float Lerp_GetDurationIn( float max, float min, float current )
{
	return 1 - ((current - min) / (max - min));
}

float Lerp_GetDurationIn( float max, float min, float current, float mult )
{
	return (1 - ((current - min) / (max - min))) * mult;
}


float Math_EaseOutExpo( float x )
{
	return x == 1 ? 1 : 1 - pow( 2, -10 * x );
}

float Math_EaseOutQuart( float x )
{
	return 1 - pow( 1 - x, 4 );
}


void CalcZoom( CCamera* camera, Entity player )
{
	UserCmd_t* userCmd = nullptr;

	if ( Game_ProcessingClient() )
	{
		if ( player == gLocalPlayer )
			userCmd = &gClientUserCmd;
	}
	else
	{
		SV_Client_t* client = SV_GetClientFromEntity( player );

		if ( !client )
			return;

		userCmd = &client->aUserCmd;

		Assert( userCmd );
	}

	CPlayerZoom* zoom = GetPlayerZoom( player );

	Assert( zoom );

	// If we have a usercmd to process on either client or server, process it
	if ( userCmd )
	{
		if ( zoom->aOrigFov != r_fov.GetFloat() )
		{
			zoom->aZoomTime = 0.f;  // idk lol
			zoom->aOrigFov  = r_fov.GetFloat();
		}

		if ( Game_IsPaused() )
		{
			camera->aFov = zoom->aNewFov;
			return;
		}

		float lerpTarget = 0.f;

		if ( userCmd && userCmd->aButtons & EBtnInput_Zoom )
		{
			if ( !zoom->aWasZoomed )
			{
				zoom->aZoomChangeFov = camera->aFov;

				// scale duration by how far zoomed in we are compared to the target zoom level
				zoom->aZoomDuration  = Lerp_GetDurationIn( cl_zoom_fov, zoom->aOrigFov, camera->aFov, cl_zoom_duration );
				zoom->aZoomTime      = 0.f;
				zoom->aWasZoomed     = true;
			}

			lerpTarget = cl_zoom_fov;
		}
		else
		{
			if ( zoom->aWasZoomed || zoom->aZoomDuration == 0.f )
			{
				zoom->aZoomChangeFov = camera->aFov;

				// scale duration by how far zoomed in we are compared to the target zoom level
				zoom->aZoomDuration  = Lerp_GetDuration( cl_zoom_fov, zoom->aOrigFov, camera->aFov, cl_zoom_duration );
				zoom->aZoomTime      = 0.f;
				zoom->aWasZoomed     = false;
			}

			lerpTarget = zoom->aOrigFov;
		}

		zoom->aZoomTime += gFrameTime;

		if ( zoom->aZoomDuration >= zoom->aZoomTime )
		{
			float time = (zoom->aZoomTime / zoom->aZoomDuration);

			// smooth cosine lerp
			// float timeCurve = (( cos((zoomLerp * M_PI) - M_PI) ) + 1) * 0.5;
			float timeCurve = Math_EaseOutQuart( time );

			zoom->aNewFov = std::lerp( zoom->aZoomChangeFov, lerpTarget, timeCurve );
		}
	}

	if ( Game_ProcessingClient() && player == gLocalPlayer )
	{
		// scale mouse delta
		float fovScale = (zoom->aNewFov / zoom->aOrigFov);
		Input_SetMouseDeltaScale( { fovScale, fovScale } );
	}

	camera->aFov = zoom->aNewFov;
}


void PlayerManager::UpdateView( CPlayerInfo* info, Entity player )
{
	PROF_SCOPE();

	Assert( info );
	Assert( info->aCamera );

	CTransform* camTransform = GetTransform( info->aCamera );
	CTransform* transform    = GetTransform( player );

	auto        dir          = GetComp_Direction( player );

	auto        rigidBody    = GetRigidBody( player );
	auto        camera       = GetCamera( info->aCamera );
	auto        camDir       = GetComp_Direction( info->aCamera );

	Assert( transform );

	ClampAngles( camTransform );

	if ( transform->aPos.aIsDirty || transform->aAng.aIsDirty )
	{
		glm::mat4 viewMatrixZ;
		Util_ToViewMatrixZ( viewMatrixZ, transform->aPos, transform->aAng );
		Util_GetViewMatrixZDirection( viewMatrixZ, dir->aForward.Edit(), dir->aRight.Edit(), dir->aUp.Edit() );
	}

	// MOVE ME ELSEWHERE IDK, MAYBE WHEN AN HEV SUIT COMPONENT IS MADE
	CalcZoom( camera, player );

	// TEMP UNTIL GetWorldMatrix works properly
	CTransform transformView = *transform;
	transformView.aPos += camTransform->aPos;
	transformView.aAng = camTransform->aAng;

	// Transform transformViewTmp = GetEntitySystem()->GetWorldTransform( info->aCamera );
	// Transform transformView    = transformViewTmp;

	//transformView.aAng[ PITCH ] = transformViewTmp.aAng[ YAW ];
	//transformView.aAng[ YAW ]   = transformViewTmp.aAng[ PITCH ];

	//transformView.aAng.Edit()[ YAW ] *= -1;

	//Transform transformView = transform;
	//transformView.aAng += move.aViewAngOffset;

	if ( cl_thirdperson.GetBool() )
	{
		Transform thirdPerson = {
			.aPos = {cl_cam_x.GetFloat(), cl_cam_y.GetFloat(), cl_cam_z.GetFloat()}
		};

		// thirdPerson.aPos = {cl_cam_x.GetFloat(), cl_cam_y.GetFloat(), cl_cam_z.GetFloat()};

		glm::mat4 viewMatrixZ;
		Util_ToViewMatrixZ( viewMatrixZ, transformView.aPos, transformView.aAng );

		glm::mat4 viewMat = thirdPerson.ToMatrix( false ) * viewMatrixZ;

		if ( info->aIsLocalPlayer )
		{
			gViewInfo[ 0 ].aViewPos = thirdPerson.aPos;
			Game_SetView( viewMat );
			// audio->SetListenerTransform( thirdPerson.aPos, transformView.aAng );
		}

		Util_GetViewMatrixZDirection( viewMat, camDir->aForward.Edit(), camDir->aRight.Edit(), camDir->aUp.Edit() );
	}
	else
	{
		glm::mat4 viewMat;
		Util_ToViewMatrixZ( viewMat, transformView.aPos, transformView.aAng );

		if ( info->aIsLocalPlayer )
		{
			// wtf broken??
			// audio->SetListenerTransform( transformView.aPos, transformView.aAng );

			gViewInfo[ 0 ].aViewPos = transformView.aPos;
			Game_SetView( viewMat );
		}

		Util_GetViewMatrixZDirection( viewMat, camDir->aForward.Edit(), camDir->aRight.Edit(), camDir->aUp.Edit() );
	}

	// temp
	//Graphics_DrawAxis( transformView.aPos, transformView.aAng, { 40.f, 40.f, 40.f } );
	//Graphics_DrawAxis( transform->aPos, transform->aAng, { 40.f, 40.f, 40.f } );

	if ( info->aIsLocalPlayer )
	// if ( player == gLocalPlayer )
	{
		// scale the nearz and farz
		gView.aFarZ  = r_farz;
		gView.aNearZ = r_nearz;
		gView.aFOV   = camera->aFov;
		Game_UpdateProjection();

		// i feel like there's gonna be a lot more here in the future...
		Skybox_SetAng( transformView.aAng );

		glm::vec3 listenerAng{
			transformView.aAng.Get().x,
			transformView.aAng.Get().z,
			transformView.aAng.Get().y,
		};

		audio->SetListenerTransform( transformView.aPos, listenerAng );

#if AUDIO_OPENAL
		audio->SetListenerVelocity( rigidBody->aVel );
		// audio->SetListenerOrient( camDir->aForward, camDir->aUp );
#endif
	}
}


// ============================================================


void PlayerMovement::EnsureUserCmd( Entity player )
{
	if ( Game_ProcessingClient() )
	{
		apUserCmd = &gClientUserCmd;
	}
	else
	{
		apUserCmd           = nullptr;
		SV_Client_t* client = SV_GetClientFromEntity( player );

		if ( !client )
			return;

		apUserCmd = &client->aUserCmd;
	}

	Assert( apUserCmd );
}


void PlayerMovement::SetPlayer( Entity player )
{
	aPlayer     = player;

	CPlayerInfo* playerInfo = GetPlayerInfo( player );

	if ( !playerInfo )
	{
		Log_Error( "playerInfo component not found on player entity\n" );
		return;
	}

	Assert( playerInfo );
	Assert( playerInfo->aCamera );

	apCamTransform = GetTransform( playerInfo->aCamera );
	apCamDir       = GetComp_Direction( playerInfo->aCamera );
	apCamera       = GetCamera( playerInfo->aCamera );

	apMove         = GetPlayerMoveData( player );
	apRigidBody    = GetRigidBody( player );
	apTransform    = GetTransform( player );
	apDir          = Ent_GetComponent< CDirection >( player, "direction" );
	apPhysObjComp  = GetComp_PhysObject( player );
	apPhysObj      = apPhysObjComp->apObj;

	Assert( apCamTransform );
	Assert( apCamDir );
	Assert( apCamera );

	Assert( apMove );
	Assert( apRigidBody );
	Assert( apTransform );
	Assert( apDir );
	Assert( apPhysObjComp );
	Assert( apPhysObj );
}


void PlayerMovement::OnPlayerSpawn( Entity player )
{
	EnsureUserCmd( player );

	CPlayerInfo* playerInfo = GetPlayerInfo( player );
	Assert( playerInfo );
	Assert( playerInfo->aCamera );

	if ( !playerInfo )
	{
		Log_Error( "playerInfo compoment not found in OnPlayerSpawn\n" );
		return;
	}

	CTransform* camTransform = GetTransform( playerInfo->aCamera );
	auto        move         = GetPlayerMoveData( player );

	Assert( move );
	Assert( camTransform );

	SetMoveType( *move, PlayerMoveType::Walk );

	camTransform->aPos.Edit() = { 0, 0, sv_view_height.GetFloat() };
}


void PlayerMovement::OnPlayerRespawn( Entity player )
{
	EnsureUserCmd( player );

	auto move      = GetPlayerMoveData( player );
	auto transform = GetTransform( player );
	auto physObj   = GetComp_PhysObjectPtr( player );

	Assert( move );
	Assert( transform );
	Assert( physObj );

	//auto& physObj = GetEntitySystem()->GetComponent< PhysicsObject* >( player );
	transform->aPos.Edit().z += phys_player_offset;

	physObj->SetPos( transform->aPos );

	// Init Smooth Duck
	move->aTargetViewHeight = GetViewHeight();
	move->aOutViewHeight    = GetViewHeight();
}


void PlayerMovement::MovePlayer( Entity player, UserCmd_t* spUserCmd )
{
	SetPlayer( player );

	apUserCmd   = spUserCmd;

	apPhysObj->SetAllowDebugDraw( phys_dbg_player.GetBool() );

	// update velocity
	apRigidBody->aVel = apPhysObj->GetLinearVelocity();

	//apPhysObj->SetSleepingThresholds( 0, 0 );
	//apPhysObj->SetAngularFactor( 0 );
	//apPhysObj->SetAngularVelocity( {0, 0, 0} );
	apPhysObj->SetFriction( phys_friction_player );

	UpdateInputs();

	// Needed before smooth duck
	CalcOnGround();

	// should be in WalkMove only, but i need this here when toggling noclip mid-duck
	DoSmoothDuck();

	DetermineMoveType();

	switch ( apMove->aMoveType )
	{
		case PlayerMoveType::Walk:    WalkMove();     break;
		case PlayerMoveType::Fly:     FlyMove();      break;
		case PlayerMoveType::NoClip:  NoClipMove();   break;
	}
}


float PlayerMovement::GetViewHeight()
{
	if ( !apUserCmd )
		return sv_view_height;

	return ( apUserCmd->aButtons & EBtnInput_Duck ) ? sv_view_height_duck : sv_view_height;
}


void PlayerMovement::DetermineMoveType()
{
	// TODO: MULTIPLAYER
	// if ( gHostMoveType != apMove->aMoveType )
	// {
	// 	apMove->aMoveType = gHostMoveType;
	// 	SetMoveType( *apMove, gHostMoveType );
	// }
}


void PlayerMovement::SetMoveType( CPlayerMoveData& move, PlayerMoveType type )
{
	move.aMoveType = type;

	switch (type)
	{
		case PlayerMoveType::NoClip:
		{
			EnableGravity( false );
			SetCollisionEnabled( false );
			break;
		}

		case PlayerMoveType::Fly:
		{
			EnableGravity( false );
			SetCollisionEnabled( true );
			break;
		}

		case PlayerMoveType::Walk:
		{
			EnableGravity( true );
			SetCollisionEnabled( true );
			break;
		}
	}
}


void PlayerMovement::SetCollisionEnabled( bool enable )
{
	apPhysObjComp->aEnableCollision = enable;
	apPhysObj->SetCollisionEnabled( enable );
}


void PlayerMovement::EnableGravity( bool enabled )
{
	apPhysObjComp->aGravity = enabled;
	apPhysObj->SetGravityEnabled( enabled );
}

// ============================================================


void PlayerMovement::DisplayPlayerStats( Entity player ) const
{
	if ( !cl_show_player_stats )
		return;

	CPlayerInfo* playerInfo = GetPlayerInfo( player );
	Assert( playerInfo );

	if ( !playerInfo )
	{
		Log_Error( "playerInfo compoment not found in " CH_FUNC_NAME_CLASS "\n" );
		return;
	}

	Assert( playerInfo->aCamera );

	// auto& move = GetEntitySystem()->GetComponent< CPlayerMoveData >( player );
	auto rigidBody = GetRigidBody( player );
	auto transform = GetTransform( player );

	CTransform* camTransform = GetTransform( playerInfo->aCamera );
	CCamera*    camera       = GetCamera( playerInfo->aCamera );

	Assert( rigidBody );
	Assert( transform );
	Assert( camTransform );
	Assert( camera );

	float speed        = glm::length( glm::vec2( rigidBody->aVel.Get().x, rigidBody->aVel.Get().y ) );

	gui->DebugMessage( "Player Pos:    %s", Vec2Str(transform->aPos).c_str() );
	gui->DebugMessage( "Player Ang:    %s", Vec2Str(transform->aAng).c_str() );
	gui->DebugMessage( "Player Vel:    %s", Vec2Str(rigidBody->aVel).c_str() );
	gui->DebugMessage( "Player Speed:  %.4f", speed );

	gui->DebugMessage( "Camera FOV:    %.4f", camera->aFov.Get() );
	gui->DebugMessage( "Camera Pos:    %s", Vec2Str(camTransform->aPos.Get()).c_str() );
	gui->DebugMessage( "Camera Ang:    %s", Vec2Str(camTransform->aAng.Get()).c_str() );
}


void PlayerMovement::SetPos( const glm::vec3& origin )
{
	apTransform->aPos = origin;
}

const glm::vec3& PlayerMovement::GetPos() const
{
	return apTransform->aPos;
}

void PlayerMovement::SetAng( const glm::vec3& angles )
{
	apTransform->aAng = angles;
}

const glm::vec3& PlayerMovement::GetAng() const
{
	return apTransform->aAng;
}


void PlayerMovement::UpdateInputs()
{
	apRigidBody->aAccel.Set( { 0, 0, 0 } );

	float moveScale = 1.0f;

	apMove->aPrevPlayerFlags = apMove->aPlayerFlags;
	PlayerFlags newFlags     = PlyNone;

	if ( apUserCmd->aButtons & EBtnInput_Duck )
	{
		newFlags |= PlyInDuck;
		moveScale = sv_duck_mult;
	}

	else if ( apUserCmd->aButtons & EBtnInput_Sprint )
	{
		newFlags |= PlyInSprint;
		moveScale = sv_sprint_mult;
	}

	const float forwardSpeed = forward_speed * moveScale;
	const float sideSpeed = side_speed * moveScale;
	apMove->aMaxSpeed = max_speed * moveScale;

	if ( apUserCmd->aButtons & EBtnInput_Forward ) apRigidBody->aAccel.Edit()[ W_FORWARD ] = forwardSpeed;
	if ( apUserCmd->aButtons & EBtnInput_Back )    apRigidBody->aAccel.Edit()[ W_FORWARD ] += -forwardSpeed;
	if ( apUserCmd->aButtons & EBtnInput_Left )    apRigidBody->aAccel.Edit()[ W_RIGHT ] = -sideSpeed;
	if ( apUserCmd->aButtons & EBtnInput_Right )   apRigidBody->aAccel.Edit()[ W_RIGHT ] += sideSpeed;

	if ( CalcOnGround() && apUserCmd->aButtons & EBtnInput_Jump )
	{
		apRigidBody->aVel.Edit()[ W_UP ] = jump_force;
	}

	apMove->aPlayerFlags.Set( newFlags );
}


class PlayerCollisionCheck : public PhysCollisionCollector
{
  public:
	explicit PlayerCollisionCheck( const glm::vec3& srGravity, const glm::vec3& srVelocity, CPlayerMoveData* moveData ) :
		aGravity( srGravity ),
		aVelocity( srVelocity ),
		apMove( moveData )
	{
		apMove->apGroundObj = nullptr;
		// apMove->aGroundPosition = sPhysResult.aContactPointOn2;
		// apMove->aGroundNormal = normal;
	}

	void AddResult( const PhysCollisionResult& sPhysResult ) override
	{
		glm::vec3 normal = -glm::normalize( sPhysResult.aPenetrationAxis );

		float dot = glm::dot( normal, aGravity );
		if ( dot < aBestDot ) // Find the hit that is most opposite to the gravity
		{
			apMove->apGroundObj = sPhysResult.apPhysObj2;
			// mGroundBodySubShapeID = inResult.mSubShapeID2;
			apMove->aGroundPosition = sPhysResult.aContactPointOn2;
			apMove->aGroundNormal = normal;

			aBestDot = dot;
		}
	}

	glm::vec3 GetDirection()
	{
		return aVelocity;
	}

	CPlayerMoveData*        apMove;

private:
	glm::vec3               aGravity;
	glm::vec3               aVelocity;
	float                   aBestDot = 0.f;
};


CONVAR( phys_player_max_sep_dist, 1 );

// Post Physics Simulation Update
void PlayerMovement::UpdatePosition( Entity player )
{
	apMove        = GetPlayerMoveData( player );
	apTransform   = GetTransform( player );
	apRigidBody   = GetRigidBody( player );
	apPhysObjComp = GetComp_PhysObject( player );
	apPhysObj     = apPhysObjComp->apObj;

	Assert( apMove );
	Assert( apTransform );
	Assert( apRigidBody );
	Assert( apPhysObjComp );
	Assert( apPhysObj );

	//auto& physObj = GetEntitySystem()->GetComponent< PhysicsObject* >( player );

	//if ( aMoveType == MoveType::Fly )
		//aTransform = apPhysObj->GetWorldTransform();
	//transform.aPos = physObj->GetWorldTransform().aPos;
	apTransform->aPos = apPhysObj->GetPos();
	apTransform->aPos.Edit().z -= phys_player_offset;

	if ( apMove->aMoveType != PlayerMoveType::NoClip )
	{
		PlayerCollisionCheck playerCollide( GetPhysEnv()->GetGravity(), apRigidBody->aVel, apMove );
		apPhysObj->CheckCollision( phys_player_max_sep_dist, &playerCollide );
	}

	// um
	if ( apMove->aMoveType == PlayerMoveType::Walk )
	{
		WalkMovePostPhys();
	}
}


float Math_EaseInOutCubic( float x )
{
	return x < 0.5 ? 4 * x * x * x : 1 - pow( -2 * x + 2, 3 ) / 2;
}


// VERY BUGGY STILL
void PlayerMovement::DoSmoothDuck()
{
	CPlayerInfo* playerInfo = GetPlayerInfo( aPlayer );
	Assert( playerInfo );
	Assert( playerInfo->aCamera );

	if ( !playerInfo )
	{
		Log_Error( "playerInfo compoment not found in OnPlayerSpawn\n" );
		return;
	}

	CTransform* camTransform = GetTransform( playerInfo->aCamera );

	if ( !camTransform )
	{
		Log_Error( "transform compoment not found on playerInfo->camera\n" );
		return;
	}

	if ( Game_ProcessingServer() )
	{
		if ( IsOnGround() && apMove->aMoveType == PlayerMoveType::Walk )
		{
			if ( apMove->aTargetViewHeight != GetViewHeight() )
			{
				apMove->aPrevViewHeight = apMove->aOutViewHeight;
				apMove->aTargetViewHeight = GetViewHeight();
				apMove->aDuckTime = 0.f;

				apMove->aDuckDuration = Lerp_GetDuration( sv_view_height, sv_view_height_duck, apMove->aPrevViewHeight );

				// this is stupid
				if ( apMove->aTargetViewHeight == sv_view_height.GetFloat() )
					apMove->aDuckDuration = 1 - apMove->aDuckDuration;

				apMove->aDuckDuration *= cl_duck_time;
			}
		}
		else if ( WasOnGround() )
		{
			apMove->aPrevViewHeight = apMove->aOutViewHeight;
			apMove->aDuckDuration = Lerp_GetDuration( sv_view_height, sv_view_height_duck, apMove->aPrevViewHeight, cl_duck_time );
			apMove->aDuckTime = 0.f;
		}

		apMove->aDuckTime += gFrameTime;

		if ( apMove->aDuckDuration >= apMove->aDuckTime )
		{
			float time = (apMove->aDuckTime / apMove->aDuckDuration);
			float timeCurve = Math_EaseOutQuart( time );

			apMove->aOutViewHeight = std::lerp( apMove->aPrevViewHeight, apMove->aTargetViewHeight, timeCurve );
		}
	}

	camTransform->aPos.Edit()[ W_UP ] = apMove->aOutViewHeight;
}


// The maximum angle of slope that character can still walk on
CONVAR( phys_player_max_slope_ang, 40, 0, "The maximum angle of slope that character can still walk on" );


bool PlayerMovement::CalcOnGround( bool sSetFlag )
{
	if ( apMove->aMoveType != PlayerMoveType::Walk )
		return false;

	// static float maxSlopeAngle = cos( apMove->aMaxSlopeAngle );
	
	// The maximum angle of slope that character can still walk on (radians and put in cos())
	float maxSlopeAngle = cos( phys_player_max_slope_ang * (M_PI / 180.f) );

	if ( apMove->apGroundObj == nullptr )
		return false;

	glm::vec3 up       = -glm::normalize( GetPhysEnv()->GetGravity() );

	bool      onGround = ( glm::dot( apMove->aGroundNormal, up ) > maxSlopeAngle );

	if ( sSetFlag )
	{
		if ( onGround )
			apMove->aPlayerFlags.Edit() |= PlyOnGround;
		else
			apMove->aPlayerFlags.Edit() &= ~PlyOnGround;
	}

	return onGround;
}


bool PlayerMovement::IsOnGround()
{
	return apMove->aPlayerFlags & PlyOnGround;
}


bool PlayerMovement::WasOnGround()
{
	return apMove->aPrevPlayerFlags & PlyOnGround;
}


float PlayerMovement::GetMoveSpeed( glm::vec3 &wishdir, glm::vec3 &wishvel )
{
	wishdir = wishvel;
	float wishspeed = vec3_norm( wishdir );

	if ( wishspeed > apMove->aMaxSpeed )
	{
		wishvel = wishvel * apMove->aMaxSpeed.Get() / wishspeed;
		wishspeed = apMove->aMaxSpeed;
	}

	return wishspeed;
}


float PlayerMovement::GetMaxSpeed()
{
	return apMove->aMaxSpeed;
}


float PlayerMovement::GetMaxSpeedBase()
{
	return max_speed;
}


float PlayerMovement::GetMaxSprintSpeed()
{
	return GetMaxSpeedBase() * sv_sprint_mult;
}


float PlayerMovement::GetMaxDuckSpeed()
{
	return GetMaxSpeedBase() * sv_duck_mult;
}


CONVAR( cl_step_sound_speed_vol, 0.001 );
CONVAR( cl_step_sound_speed_offset, 1 );
CONVAR( cl_step_sound_gravity_scale, 4 );
CONVAR( cl_step_sound_min_speed, 0.075 );
CONVAR( cl_step_sound, 1 );
CONVAR( cl_impact_sound, 1 );


// old system
/*std::string PlayerMovement::PreloadStepSounds()
{
	char soundName[128];
	int soundIndex = (rand() / (RAND_MAX / 40.0f)) + 1;
	snprintf( soundName, 128, "sound/footsteps/running_dirt_%s%d.ogg", soundIndex < 10 ? "0" : "", soundIndex );

	return soundName;
}*/


Handle PlayerMovement::GetStepSound()
{
	char soundName[128];
	int soundIndex = (rand() / (RAND_MAX / 40.0f)) + 1;
	snprintf( soundName, 128, "sound/footsteps/running_dirt_%s%d.ogg", soundIndex < 10 ? "0" : "", soundIndex );

	// audio system should auto free it i think?
	if ( Handle stepSound = audio->OpenSound( soundName ) )
		// return audio->PreloadSound( stepSound ) ? stepSound : nullptr;
		return stepSound;

	return InvalidHandle;
}


void PlayerMovement::StopStepSound( bool force )
{
	/*if ( apMove->apStepSound && apMove->apStepSound->Valid() )
	{
		if ( game->aCurTime - apMove->aLastStepTime > cl_stepduration || force )
		{
			audio->FreeSound( apMove->apStepSound );
			//gui->DebugMessage( 7, "Freed Step Sound" );
		}
	}*/
}


void PlayerMovement::PlayStepSound()
{
	if ( !cl_step_sound.GetBool() )
		return;

	//float vel = glm::length( glm::vec2(aVelocity.x, aVelocity.y) ); 
	float vel         = glm::length( glm::vec3( apRigidBody->aVel.Get().x, apRigidBody->aVel.Get().y, apRigidBody->aVel.Get().z * cl_step_sound_gravity_scale ) ); 
	float speedFactor = glm::min( glm::log( vel * cl_step_sound_speed_vol + cl_step_sound_speed_offset ), 1.f );
	
	if ( speedFactor < cl_step_sound_min_speed )
		return;

	if ( gCurTime - apMove->aLastStepTime < cl_stepduration )
		return;

	StopStepSound( true );

	// if ( apMove->apStepSound = audio->LoadSound( GetStepSound().c_str() ) )
	if ( Handle stepSound = GetStepSound() )
	{
		audio->SetVolume( stepSound, speedFactor );
		audio->PlaySound( stepSound );

		// TODO: SPECTATING
		if ( aPlayer != gLocalPlayer )
		{
			audio->AddEffects( stepSound, AudioEffect_World );
			audio->SetEffectData( stepSound, EAudio_World_Pos, apTransform->aPos.Get() );
		}

		apMove->aLastStepTime = gCurTime;
	}
}


// really need to be able to play multiple impact sounds at a time
void PlayerMovement::StopImpactSound()
{
	/*if ( apMove->apImpactSound && apMove->apImpactSound->Valid() )
	{
		audio->FreeSound( apMove->apImpactSound );
	}*/
}


void PlayerMovement::PlayImpactSound()
{
	if ( !cl_impact_sound.GetBool() )
		return;

	//float vel = glm::length( glm::vec2(aVelocity.x, aVelocity.y) ); 
	float vel         = glm::length( glm::vec3( apRigidBody->aVel.Get().x, apRigidBody->aVel.Get().y, apRigidBody->aVel.Get().z * cl_step_sound_gravity_scale ) );
	float speedFactor = glm::min( glm::log( vel * cl_step_sound_speed_vol + cl_step_sound_speed_offset ), 1.f );

	if ( speedFactor < cl_step_sound_min_speed )
		return;

	StopImpactSound();

	/*if ( apMove->apImpactSound = audio->LoadSound(GetStepSound().c_str()) )
	{
		apMove->apImpactSound->vol = speedFactor;

		audio->PlaySound( apMove->apImpactSound );
	}*/

	if ( Handle impactSound = GetStepSound() )
	{
		audio->SetVolume( impactSound, speedFactor );
		audio->PlaySound( impactSound );
	}
}


void PlayerMovement::BaseFlyMove()
{
	glm::vec3 wishvel( 0, 0, 0 );
	glm::vec3 wishdir( 0, 0, 0 );

	// forward and side movement
	for ( int i = 0; i < 3; i++ )
		wishvel[ i ] = apCamDir->aForward.Get()[ i ] * apRigidBody->aAccel.Get().x + apCamDir->aRight.Get()[ i ] * apRigidBody->aAccel.Get()[ W_RIGHT ];

	float wishspeed = GetMoveSpeed( wishdir, wishvel );

	AddFriction();
	Accelerate( wishspeed, wishdir );
}


void PlayerMovement::NoClipMove()
{
	BaseFlyMove();
	apPhysObj->SetLinearVelocity( apRigidBody->aVel );
}


void PlayerMovement::FlyMove()
{
	BaseFlyMove();
	apPhysObj->SetLinearVelocity( apRigidBody->aVel );
}


CONVAR( cl_land_sound_threshold, 0.1 );


class PlayerStairsCheck : public PhysCollisionCollector
{
  public:
	explicit PlayerStairsCheck( const glm::vec3& srDir )
	{
		aDir = glm::normalize( srDir );
	}

	void AddResult( const PhysCollisionResult& srPhysResult ) override
	{
		glm::vec3 normal = -glm::normalize( srPhysResult.aPenetrationAxis );

		float     dot    = glm::dot( normal, aDir );
		if ( dot < aBestDot )  // Find the hit that is most opposite to the gravity
		{
			aBestResult = srPhysResult;
			aNormal     = normal;
			aBestDot    = dot;
		}
	}
	
	glm::vec3 GetDirection()
	{
		return aDir;
	}

	PhysCollisionResult aBestResult;
	glm::vec3           aNormal;

	glm::vec3           aDir;
	float               aBestDot = 0.f;
};


void PlayerMovement::WalkMove()
{
	glm::vec3 wishvel = apDir->aForward * apRigidBody->aAccel.Get().x + apDir->aRight * apRigidBody->aAccel.Get()[ W_RIGHT ];
	wishvel[W_UP] = 0.f;

	glm::vec3 wishdir(0,0,0);
	float wishspeed = GetMoveSpeed( wishdir, wishvel );

	bool wasOnGround = WasOnGround();
	bool onGround = CalcOnGround();

	if ( onGround )
	{
		// physics engine friction is kinda meh
		if ( sv_friction_enable )
			AddFriction();

		Accelerate( wishspeed, wishdir, false );
	}
	else
	{	// not on ground, so little effect on velocity
		Accelerate( wishspeed, wishvel, true );
	}

	// uhhh
	apPhysObj->SetLinearVelocity( apRigidBody->aVel );
	apRigidBody->aVel = apPhysObj->GetLinearVelocity();

	// -------------------------------------------------
	// Try checking for stairs

	// PlayerStairsCheck playerStairsForward( { apRigidBody->aVel.x, apRigidBody->aVel.y, 0 } );
	// apPhysObj->CheckCollision( phys_player_max_sep_dist, &playerStairsForward );

	// if ( playerStairsForward.aBestResult.apPhysObj2 )
	// {
	// 	Log_Msg( "FOUND STAIRS\n" );
	// }

	// -------------------------------------------------

	StopStepSound();

	if ( IsOnGround() && !onGround )
	{
	//	PlayImpactSound();

		//glm::vec2 vel( apRigidBody->aVel.x, apRigidBody->aVel.y );
		//if ( glm::length( vel ) < cl_land_sound_threshold )
		//	PlayStepSound();
	}

	// CalcOnGround();
	DoSmoothLand( wasOnGround );
	// DoViewBob();
	// DoViewTilt();

	wasOnGround = IsOnGround();
}


void PlayerMovement::WalkMovePostPhys()
{
	bool onGroundPrev = IsOnGround();
	bool onGround = CalcOnGround( false );

	if ( onGround && !onGroundPrev )
		PlayImpactSound();

	if ( onGround )
		apRigidBody->aVel.Edit()[ W_UP ] = 0.f;
}


void PlayerMovement::DoSmoothLand( bool wasOnGround )
{
	if ( Game_ProcessingServer() )
	{
		if ( sv_land_smoothing )
		{
			// NOTE: this doesn't work properly when jumping mid duck and landing
			// meh, works well enough with the current values for now
			if ( CalcOnGround() && !wasOnGround )
			// if ( CalcOnGround() && !WasOnGround() )
			{
				float baseLandVel  = abs( apRigidBody->aVel.Get()[ W_UP ] * sv_land_vel_scale.GetFloat() ) / sv_land_max_speed.GetFloat();
				float landVel      = std::clamp( baseLandVel * M_PI, 0.0, M_PI );

				apMove->aLandPower = ( -cos( landVel ) + 1 ) / 2;

				apMove->aLandTime  = 0.f;

				// This is shoddy and is only here to be funny
				auto health        = Ent_GetComponent< CHealth >( aPlayer, "health" );
				health->aHealth.Edit() -= ( landVel * 50 );
			}

			apMove->aLandTime += gFrameTime * sv_land_time_scale;
		}
		else
		{
			apMove->aLandPower = 0.f;
			apMove->aLandTime  = 0.f;
		}
	}

	if ( sv_land_smoothing && apMove->aMoveType == PlayerMoveType::Walk )
	{
		if ( apMove->aLandPower > 0.f )
			// apCamera->aTransform.aPos[W_UP] += (- landPower * sin(landTime / landPower / 2) / exp(landTime / landPower)) * cl_land_power_scale;
			apCamTransform->aPos.Edit()[ W_UP ] += ( -apMove->aLandPower * sin( apMove->aLandTime / apMove->aLandPower ) / exp( apMove->aLandTime / apMove->aLandPower ) ) * sv_land_power_scale;

	}
}


// TODO: doesn't smoothly transition out of viewbob still
// TODO: maybe make a minimum speed threshold and start lerping to 0
//  or some check if your movements changed compared to the previous frame
//  so if you start moving again faster than min speed, or movements change,
//  you restart the view bob, or take a new step right there
void PlayerMovement::DoViewBob()
{
	if ( !cl_bob_enabled )
		return;

	//static glm::vec3 prevMove = aMove;
	//static bool inExit = false;

	if ( !IsOnGround() /*|| aMove != prevMove || inExit*/ )
	{
		// lerp back to 0 to not snap view the offset (not good enough) and reset input
		apMove->aWalkTime        = 0.f;
		apMove->aBobOffsetAmount = glm::mix( apMove->aBobOffsetAmount.Get(), 0.f, cl_bob_exit_lerp.GetFloat() );
		apCamTransform->aPos.Edit()[ W_UP ] += apMove->aBobOffsetAmount;
		//inExit = aBobOffsetAmount > 0.01;
		//prevMove = aMove;
		return;
	}

	static bool playedStepSound = false;

	float       vel             = glm::length( glm::vec2( apRigidBody->aVel.Get().x, apRigidBody->aVel.Get().y ) ); 

	float speedFactor = glm::log( vel * cl_bob_speed_scale + 1 );

	// scale by speed
	apMove->aWalkTime += gFrameTime * speedFactor * cl_bob_freq;

	// never reaches 0 so do this to get it to 0
	// static float sinMessFix = sin(cos(tan(cos(0))));
	float sinMessFix = sin(cos(tan(cos(0)))) + cl_bob_offset;
	
	// using this math function mess instead of abs(sin(x)) and lerping it, since it gives a similar result
	// mmmmmmm cpu cycles go brrrrrr
	// apMove->aBobOffsetAmount = cl_bob_magnitude * speedFactor * (sin(cos(tan(cos(apMove->aWalkTime)))) - sinMessFix);
	apMove->aBobOffsetAmount = cl_bob_magnitude * speedFactor * (sin(cos(tan(cos(apMove->aWalkTime + cl_bob_time_offset)))) - sinMessFix);

	// apMove->aBobOffsetAmount = cl_bob_magnitude * speedFactor * ( 0.6*(1-cos( 2*sin(apMove->aWalkTime) )) );

	// reset input
	if ( apMove->aBobOffsetAmount == 0.f )
		apMove->aWalkTime = 0.f;

	// TODO: change this to be time based (and maybe a separate component? idk)
	if ( apMove->aBobOffsetAmount <= cl_bob_sound_threshold )
	{
		if ( !playedStepSound && vel > cl_land_sound_threshold )
		{
			PlayStepSound();
			playedStepSound = true;
		}
	}
	else
	{
		playedStepSound = false;
	}

	apCamTransform->aPos.Edit()[ W_UP ] += apMove->aBobOffsetAmount;
	
	if ( cl_bob_debug )
	{
		gui->DebugMessage( "Walk Time * Speed:  %.8f", apMove->aWalkTime.Get() );
		gui->DebugMessage( "View Bob Offset:    %.4f", apMove->aBobOffsetAmount.Get() );
		gui->DebugMessage( "View Bob Speed:     %.6f", speedFactor );
	}
}


CONVAR( cl_tilt, 1, CVARF_ARCHIVE );
CONVAR( cl_tilt_speed, 0.1 );
CONVAR( cl_tilt_threshold, 200 );

CONVAR( cl_tilt_type, 1 );
CONVAR( cl_tilt_lerp, 5 );
CONVAR( cl_tilt_lerp_new, 10 );
CONVAR( cl_tilt_speed_scale, 0.043 );
CONVAR( cl_tilt_scale, 0.2 );
CONVAR( cl_tilt_threshold_new, 12 );


void PlayerMovement::DoViewTilt()
{
	if ( cl_tilt == false )
		return;

	float        output   = glm::dot( apRigidBody->aVel.Get(), apDir->aRight.Get() );
	float side = output < 0 ? -1 : 1;

	if ( cl_tilt_type == 1.f )
	{
		float speedFactor = glm::max(0.f, glm::log( glm::max(0.f, (fabs(output) * cl_tilt_speed_scale + 1) - cl_tilt_threshold_new) ));

		/* Now Lerp the tilt angle with the previous angle to make a smoother transition. */
		output = glm::mix( apMove->aPrevViewTilt.Get(), speedFactor * side * cl_tilt_scale, cl_tilt_lerp_new * gFrameTime );
	}
	else // type 0
	{
		output = glm::clamp( glm::max(0.f, fabs(output) - cl_tilt_threshold) / GetMaxSprintSpeed(), 0.f, 1.f ) * side;

		/* Now Lerp the tilt angle with the previous angle to make a smoother transition. */
		output = glm::mix( apMove->aPrevViewTilt.Get(), output * cl_tilt, cl_tilt_lerp * gFrameTime );
	}

	apCamTransform->aAng.Edit()[ ROLL ] = output;
	apMove->aPrevViewTilt.Edit()                    = output;
}


CONVAR( sv_friction_idk, 16 );

CONVAR( sv_friction2, 800 );

/*CONVAR( sv_friction_scale, 0.005 );
CONVAR( sv_friction_scale2, 10 );
CONVAR( sv_friction_offset, 6 );
CONVAR( sv_friction_lerp, 5 );

CONVAR( sv_friction_stop_lerp, 2 );
CONVAR( sv_friction_scale3, 0.01 );
CONVAR( sv_friction_scaleeee, 1 );

CONVAR( sv_friction_power, 2 );*/


// TODO: make your own version of this that uses this to find the friction value:
// log(-playerSpeed) + 1
void PlayerMovement::AddFriction()
{
#if 0

	glm::vec3 vel = apRigidBody->aVel;

	float speed = vel.length();
	if ( !speed )
		return;

	vel /= sv_friction2;
	apRigidBody->aVel = vel;

#else
	glm::vec3	start(0, 0, 0), stop(0, 0, 0);
	float	friction;
	//trace_t	trace;

	glm::vec3 vel = apRigidBody->aVel;

	float speed = sqrt(vel[0]*vel[0] + vel[W_RIGHT]*vel[W_RIGHT] + vel[W_UP]*vel[W_UP]);
	if (!speed)
		return;

	float idk = sv_friction_idk;

	// if the leading edge is over a dropoff, increase friction
	start.x = stop.x = GetPos().x + vel.x / speed * idk;
	start[ W_RIGHT ] = stop[ W_RIGHT ] = GetPos()[ W_RIGHT ] + vel[ W_RIGHT ] / speed * idk;
	start[ W_UP ] = stop[ W_UP ] = GetPos()[ W_UP ] + vel[ W_UP ] / speed * idk;
	// start.z = GetPos().z + sv_player->v.mins.z;

	//trace = SV_Move (start, vec3_origin, vec3_origin, stop, true, sv_player);

	//if (trace.fraction == 1.0)
	//	friction = sv_friction.GetFloat() * sv_edgefriction.value;
	//else
		friction = sv_friction;

	// apply friction
	float control = speed < stop_speed ? stop_speed : speed;
	float newspeed = glm::max( 0.f, speed - gFrameTime * control * friction );

	newspeed /= speed;
	apRigidBody->aVel = vel * newspeed;
#endif
}


void PlayerMovement::Accelerate( float wishSpeed, glm::vec3 wishDir, bool inAir )
{
	//float baseWishSpeed = inAir ? glm::min( 30.f, vec3_norm( wishDir ) ) : wishSpeed;
	float baseWishSpeed = inAir ? glm::min( accel_speed_air.GetFloat(), vec3_norm( wishDir ) ) : wishSpeed;

	float currentspeed  = glm::dot( apRigidBody->aVel.Get(), wishDir );
	float addspeed      = baseWishSpeed - currentspeed;

	if ( addspeed <= 0.f )
		return;

	addspeed = glm::min( addspeed, accel_speed * gFrameTime * wishSpeed );

	for ( int i = 0; i < 3; i++ )
		apRigidBody->aVel.Edit()[ i ] += addspeed * wishDir[ i ];
}


Transform PlayerSpawnManager::SelectSpawnTransform()
{
	CH_ASSERT_MSG( Game_ProcessingServer(), "Tried to get playerSpawn transform on client!" );

	if ( Game_ProcessingClient() )
	{
		Log_Error( "Tried to get playerSpawn transform on client!\n" );
		return {};
	}

	if ( aEntities.empty() )
	{
		Log_Error( "No playerSpawn entities found, returning origin for spawn position\n" );
		return {};
	}

	Transform spot{};
	size_t    index = 0;

	// If we have more than one playerSpawn entity, pick a random one
	// TODO: maybe make some priority thing, or master spawn like in source engine? idk
	if ( aEntities.size() > 1 )
		index = RandomSizeT( 0, aEntities.size() - 1 );
	
	auto transform = Ent_GetComponent< CTransform >( aEntities[ index ], "transform" );

	if ( !transform )
	{
		Log_Error( "Entity with playerSpawn does not have a transform component, returning origin for spawn position\n" );
		return spot;
	}

	spot.aPos = transform->aPos.Get();
	spot.aAng = transform->aAng.Get();

	return spot;
}


PlayerSpawnManager* GetPlayerSpawn()
{
	CH_ASSERT_MSG( Game_ProcessingServer(), "Tried to get playerSpawn system on client!" );

	int i = Game_ProcessingClient() ? CH_PLAYER_CL : CH_PLAYER_SV;
	CH_ASSERT( playerSpawn[ i ] );
	return playerSpawn[ i ];
}

