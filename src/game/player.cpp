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

	Assert( flashlight );
	Assert( zoom );
	Assert( transform );
	Assert( health );

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

			// I hate this so muchdata:image/webp;base64,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
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
		audio->SetListenerTransform( transformView.aPos, transform->aAng );

#if AUDIO_OPENAL
		audio->SetListenerVelocity( rigidBody.aVel );
		audio->SetListenerOrient( camera->aForward, camera->aUp );
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
	if ( Handle stepSound = audio->LoadSound( soundName ) )
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

