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
#include "ent_light.h"
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

	if ( Game_GetCommandSource() != ECommandSource_Server )
		return;

	Entity player = SV_GetCommandClientEntity();

	GetPlayers()->Respawn( player );
}


CONCMD_VA( reset_velocity, CVARF( CL_EXEC ) )
{
	if ( CL_SendConVarIfClient( "reset_velocity", args ) )
		return;

	if ( Game_GetCommandSource() != ECommandSource_Server )
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


CH_COMPONENT_READ_DEF( CPlayerMoveData )
{
	//auto* spMoveData = static_cast< CPlayerMoveData* >( spData );
	//auto  message    = srReader.getRoot< NetCompPlayerMoveData >();
	//
	//auto  moveType   = message.getMoveType();
	//
	//switch ( moveType )
	//{
	//	default:
	//	case EPlayerMoveType::WALK:
	//		spMoveData->aMoveType = PlayerMoveType::Walk;
	//		break;
	//
	//	case EPlayerMoveType::NO_CLIP:
	//		spMoveData->aMoveType = PlayerMoveType::NoClip;
	//		break;
	//
	//	case EPlayerMoveType::FLY:
	//		spMoveData->aMoveType = PlayerMoveType::Fly;
	//		break;
	//}
	//
	//spMoveData->aPlayerFlags      = message.getPlayerFlags();
	//spMoveData->aPrevPlayerFlags  = message.getPrevPlayerFlags();
	//spMoveData->aMaxSpeed         = message.getMaxSpeed();
	//
	//// View Bobbing
	////spMoveData->aWalkTime         = message.getWalkTime();
	////spMoveData->aBobOffsetAmount  = message.getBobOffsetAmount();
	////spMoveData->aPrevViewTilt     = message.getPrevViewTilt();
	//
	//spMoveData->aLandPower        = message.getLandPower();
	//spMoveData->aLandTime         = message.getLandTime();
	//
	//// Smooth Duck
	//spMoveData->aPrevViewHeight   = message.getPrevViewHeight();
	//spMoveData->aTargetViewHeight = message.getTargetViewHeight();
	//spMoveData->aOutViewHeight    = message.getOutViewHeight();
	//spMoveData->aDuckDuration     = message.getDuckDuration();
	//spMoveData->aDuckTime         = message.getDuckTime();
}


CH_COMPONENT_WRITE_DEF( CPlayerMoveData )
{
	return false;

	/*auto* spMoveData = static_cast< const CPlayerMoveData* >( spData );
	bool  isDirty    = sFullUpdate;

	isDirty |= spMoveData->aMoveType.aIsDirty;
	isDirty |= spMoveData->aPlayerFlags.aIsDirty;
	isDirty |= spMoveData->aPrevPlayerFlags.aIsDirty;
	isDirty |= spMoveData->aMaxSpeed.aIsDirty;

	// isDirty |= spMoveData->aWalkTime.aIsDirty;
	// isDirty |= spMoveData->aBobOffsetAmount.aIsDirty;
	// isDirty |= spMoveData->aPrevViewTilt.aIsDirty;

	isDirty |= spMoveData->aLandPower.aIsDirty;
	isDirty |= spMoveData->aLandTime.aIsDirty;

	isDirty |= spMoveData->aPrevViewHeight.aIsDirty;
	isDirty |= spMoveData->aTargetViewHeight.aIsDirty;
	isDirty |= spMoveData->aOutViewHeight.aIsDirty;
	isDirty |= spMoveData->aDuckDuration.aIsDirty;
	isDirty |= spMoveData->aDuckTime.aIsDirty;

	if ( !isDirty )
		return false;

	auto builder = srMessage.initRoot< NetCompPlayerMoveData >();

	switch ( spMoveData->aMoveType )
	{
		default:
		case PlayerMoveType::Walk:
			builder.setMoveType( EPlayerMoveType::WALK );
			break;

		case PlayerMoveType::NoClip:
			builder.setMoveType( EPlayerMoveType::NO_CLIP );
			break;

		case PlayerMoveType::Fly:
			builder.setMoveType( EPlayerMoveType::FLY );
			break;
	}

	builder.setPlayerFlags( spMoveData->aPlayerFlags );
	builder.setPrevPlayerFlags( spMoveData->aPrevPlayerFlags );
	builder.setMaxSpeed( spMoveData->aMaxSpeed );

	//builder.setWalkTime( spMoveData->aWalkTime );
	//builder.setBobOffsetAmount( spMoveData->aBobOffsetAmount );
	//builder.setPrevViewTilt( spMoveData->aPrevViewTilt );

	builder.setLandPower( spMoveData->aLandPower );
	builder.setLandTime( spMoveData->aLandTime );

	// Smooth Duck
	builder.setPrevViewHeight( spMoveData->aPrevViewHeight );
	builder.setTargetViewHeight( spMoveData->aTargetViewHeight );
	builder.setOutViewHeight( spMoveData->aOutViewHeight );
	builder.setDuckDuration( spMoveData->aDuckDuration );
	builder.setDuckTime( spMoveData->aDuckTime );

	return true;*/
}


CH_COMPONENT_READ_DEF( CPlayerZoom )
{
	// auto* spZoom    = static_cast< CPlayerZoom* >( spData );
	// auto  message   = srReader.getRoot< NetCompPlayerZoom >();
	// 
	// spZoom->aOrigFov = message.getOrigFov();
	// spZoom->aNewFov = message.getNewFov();
}


CH_COMPONENT_WRITE_DEF( CPlayerZoom )
{
	return false;

	// auto* spZoom  = static_cast< const CPlayerZoom* >( spData );
	// bool  isDirty = sFullUpdate;
	// 
	// isDirty |= spZoom->aNewFov.aIsDirty;
	// 
	// if ( !isDirty )
	// 	return false;
	// 
	// auto builder = srMessage.initRoot< NetCompPlayerZoom >();
	// 
	// builder.setOrigFov( spZoom->aOrigFov );
	// builder.setNewFov( spZoom->aNewFov );
	// 
	// return true;
}


// ============================================================


#define CH_PLAYER_SV 0
#define CH_PLAYER_CL 1


static PlayerManager* players[ 2 ] = { 0, 0 };


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


void PlayerManager::RegisterComponents()
{
	CH_REGISTER_COMPONENT_RW( CPlayerMoveData, playerMoveData, true );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, PlayerMoveType, aMoveType, moveType );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, PlayerFlags, aPlayerFlags, playerFlags );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, PlayerFlags, aPrevPlayerFlags, prevPlayerFlags );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aMaxSpeed, maxSpeed );

	// View Bobbing
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aWalkTime, walkTime );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aBobOffsetAmount, bobOffsetAmount );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aPrevViewTilt, prevViewTilt );

	// Smooth Land
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aLandPower, landPower );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aLandTime, landTime );

	// Smooth Duck
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aPrevViewHeight, prevViewHeight );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aTargetViewHeight, targetViewHeight );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aOutViewHeight, outViewHeight );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aDuckDuration, duckDuration );
	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aDuckTime, duckTime );

	CH_REGISTER_COMPONENT_VAR( CPlayerMoveData, float, aLastStepTime, lastStepTime );


	CH_REGISTER_COMPONENT( CPlayerInfo, playerInfo, true, EEntComponentNetType_Both );
	CH_REGISTER_COMPONENT_SYS( CPlayerInfo, PlayerManager, players );
	// CH_REGISTER_COMPONENT_VAR( CPlayerInfo, std::string, aName, name );
	CH_REGISTER_COMPONENT_VAR( CPlayerInfo, bool, aIsLocalPlayer, isLocalPlayer );  // don't mess with this

	CH_REGISTER_COMPONENT_RW( CPlayerZoom, playerZoom, true );
	CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aOrigFov, origFov );
	CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aNewFov, newFov );
	CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aZoomChangeFov, zoomChangeFov );
	CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aZoomTime, zoomTime );
	CH_REGISTER_COMPONENT_VAR( CPlayerZoom, float, aZoomDuration, zoomDuration );
	CH_REGISTER_COMPONENT_VAR( CPlayerZoom, bool, aWasZoomed, wasZoomed );

	// what the fuck
	// CH_REGISTER_COMPONENT( Model, model, true, EEntComponentNetType_Both );
	CH_REGISTER_COMPONENT( CRenderable_t, renderable, false, EEntComponentNetType_Client );
	CH_REGISTER_COMPONENT_SYS( CRenderable_t, EntSys_Renderable, gEntSys_Renderable );
	// CH_REGISTER_COMPONENT( Renderable_t, renderable, false, EEntComponentNetType_Client );

	// GetEntitySystem()->RegisterComponent< Model* >();
	// GetEntitySystem()->RegisterComponent< Model >();
	//GetEntitySystem()->RegisterComponent<PhysicsObject*>();
}


void PlayerManager::ComponentAdded( Entity sEntity, void* spData )
{
	Create( sEntity );
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

	apMove->apDir       = Ent_GetComponent< CDirection >( player, "direction" );
	apMove->apRigidBody = GetRigidBody( player );
	apMove->apTransform = GetTransform( player );
	apMove->apCamera    = GetCamera( player );
	apMove->apPhysShape = GetComp_PhysShapePtr( player );
	apMove->apPhysObj   = GetComp_PhysObjectPtr( player );

	Assert( apMove->apDir );
	Assert( apMove->apRigidBody );
	Assert( apMove->apTransform );
	Assert( apMove->apCamera );
	Assert( apMove->apPhysShape );
	Assert( apMove->apPhysObj );

	return true;
}


void PlayerManager::Init()
{
	// apMove = GetEntitySystem()->RegisterSystem<PlayerMovement>();
}


void PlayerManager::Create( Entity player )
{
	// Add Components to entity
	GetEntitySystem()->AddComponent( player, "playerMoveData" );
	GetEntitySystem()->AddComponent( player, "playerZoom" );

	GetEntitySystem()->AddComponent( player, "rigidBody" );
	GetEntitySystem()->AddComponent( player, "camera" );
	GetEntitySystem()->AddComponent( player, "direction" );

	auto modelInfo     = Ent_AddComponent< CModelInfo >( player, "modelInfo" );
	modelInfo->aPath   = DEFAULT_PROTOGEN_PATH;

	CLight* flashlight = static_cast< CLight* >( GetEntitySystem()->AddComponent( player, "light" ) );

	Assert( flashlight );

	if ( Game_ProcessingClient() )
	{
		auto playerInfo            = GetPlayerInfo( player );
		playerInfo->aIsLocalPlayer = player == gLocalPlayer;

		auto renderComp = Ent_AddComponent< CRenderable_t >( player, "renderable" );
		
		Assert( renderComp );
		//renderComp->aHandle = Graphics_CreateRenderable();

		Log_Msg( "Client Creating Local Player\n" );
	}
	else
	{
		SV_Client_t* client = SV_GetClientFromEntity( player );

		Assert( client );

		if ( !client )
			return;

		// Setting the player name here crashes the game, amazing
		auto playerInfo = GetPlayerInfo( player );
		// playerInfo->aName = client->aName;
		// playerInfo->aName = "bruh";

		// This here is done just so the server can manage the light
		// I really feel like im doing this wrong lol

		Log_MsgF( "Server Creating Player Entity: \"%s\"\n", client->aName.c_str() );
	}

	flashlight->aType          = ELightType_Cone;
	flashlight->aInnerFov      = 0.f;
	flashlight->aOuterFov      = 45.f;
	// flashlight->aColor    = { r_flashlight_brightness.GetFloat(), r_flashlight_brightness.GetFloat(), r_flashlight_brightness.GetFloat() };
	flashlight->aColor.Edit()  = { 1.f, 1.f, 1.f, r_flashlight_brightness.GetFloat() };

	Transform*       transform = (Transform*)GetEntitySystem()->AddComponent( player, "transform" );

	//Model* model = new Model;
	//graphics->LoadModel( "materials/models/protogen_wip_22/protogen_wip_22.obj", "", model );
	//GetEntitySystem()->AddComponent( player, model );

	// Model *model = graphics->LoadModel( "materials/models/protogen_wip_25d/protogen_wip_25d.obj" );
	// GetEntitySystem()->AddComponent< Model* >( player, model );

	PhysicsShapeInfo shapeInfo( PhysShapeType::Cylinder );
	shapeInfo.aBounds        = glm::vec3( 72, 16, 1 );

	IPhysicsShape* physShape = Phys_CreateShape( player, shapeInfo );

	Assert( physShape );

	PhysicsObjectInfo physInfo;
	physInfo.aMotionType = PhysMotionType::Dynamic;
	physInfo.aPos = transform->aPos;
	physInfo.aAng = transform->aAng;

	physInfo.aCustomMass = true;
	physInfo.aMass = PLAYER_MASS;

	IPhysicsObject* physObj = Phys_CreateObject( player, physShape, physInfo );

	Assert( physObj );

	physObj->SetAllowSleeping( false );
	physObj->SetMotionQuality( PhysMotionQuality::LinearCast );
	physObj->SetLinearVelocity( {0, 0, 0} );
	physObj->SetAngularVelocity( {0, 0, 0} );

	Phys_SetMaxVelocities( physObj );

	physObj->SetFriction( phys_friction_player );

	// Don't allow any rotation on this
	physObj->SetInverseMass( 1.f / PLAYER_MASS );
	physObj->SetInverseInertia( {0, 0, 0}, {1, 0, 0, 0} );

	// rotate 90 degrees
	physObj->SetAng( { 90, 0, 0 } );
}


void PlayerManager::Spawn( Entity player )
{
	apMove->OnPlayerSpawn( player );

	Respawn( player );
}


void PlayerManager::Respawn( Entity player )
{
	auto rigidBody    = GetRigidBody( player );
	auto transform    = GetTransform( player );
	auto camera       = GetCamera( player );
	auto zoom         = GetPlayerZoom( player );
	auto physObj      = GetComp_PhysObjectPtr( player );

	Assert( rigidBody );
	Assert( transform );
	Assert( camera );
	Assert( zoom );
	Assert( physObj );

	transform->aPos                = MapManager_GetSpawnPos();
	transform->aAng.Edit()         = { 0, MapManager_GetSpawnAng().y, 0 };
	camera->aTransform.Edit().aAng = MapManager_GetSpawnAng();
	rigidBody->aVel.Edit()         = { 0, 0, 0 };
	rigidBody->aAccel.Edit()       = { 0, 0, 0 };

	zoom->aOrigFov                 = r_fov.GetFloat();
	zoom->aNewFov                  = r_fov.GetFloat();

	physObj->SetLinearVelocity( { 0, 0, 0 } );

	apMove->OnPlayerRespawn( player );
}


void Player_UpdateFlashlight( Entity player, bool sToggle )
{
	PROF_SCOPE();

	CTransform* transform  = GetTransform( player );
	CCamera*    camera     = GetCamera( player );
	CLight*     flashlight = Ent_GetComponent< CLight >( player, "light" );

	Assert( transform );
	Assert( camera );
	Assert( flashlight );

	auto UpdateTransform = [ & ]()
	{
		// weird stuff to get the angle of the light correct
		flashlight->aAng.Edit().x = camera->aTransform.Get().aAng.Get().z;
		flashlight->aAng.Edit().y = -camera->aTransform.Get().aAng.Get().y;
		flashlight->aAng.Edit().z = -camera->aTransform.Get().aAng.Get().x + 90.f;

		flashlight->aPos   = transform->aPos.Get() + camera->aTransform.Get().aPos.Get();

		glm::vec3 offset( r_flashlight_offset_x.GetFloat(), r_flashlight_offset_y.GetFloat(), r_flashlight_offset_z.GetFloat() );

		flashlight->aPos += offset * camera->aUp.Get();
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


inline void ClampAngles( CTransform& transform, CCamera& camera )
{
	camera.aTransform.Edit().aAng.Edit()[ YAW ]   = DegreeConstrain( camera.aTransform.Get().aAng.Get()[ YAW ] );
	camera.aTransform.Edit().aAng.Edit()[ PITCH ] = std::clamp( camera.aTransform.Get().aAng.Get()[ PITCH ], -90.0f, 90.0f );
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

		if ( !Game_IsPaused() )
		{
			// Update Client UserCmd
			auto            transform = GetTransform( player );
			auto            camera    = GetCamera( player );

			Assert( transform );
			Assert( camera );

			// transform.aAng[PITCH] = -mouse.y;
			camera->aTransform.Edit().aAng.Edit()[ PITCH ] = userCmd.aAng[ PITCH ];
			camera->aTransform.Edit().aAng.Edit()[ YAW ]   = userCmd.aAng[ YAW ];
			camera->aTransform.Edit().aAng.Edit()[ ROLL ]  = userCmd.aAng[ ROLL ];

			transform->aAng.Set( { 0.f, DegreeConstrain( userCmd.aAng[ YAW ] ), 0.f } );

			ClampAngles( *transform, *camera );

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

	if ( !Game_IsPaused() )
	{
		if ( !CL_IsMenuShown() )
			DoMouseLook( gLocalPlayer );
	}

	auto userCmd = gClientUserCmd;

	if ( !Game_IsPaused() )
	{
		if ( input->WindowHasFocus() && !CL_IsMenuShown() )
			DoMouseLook( gLocalPlayer );
	}

	// We still need to do client updating of players actually, so

	for ( Entity player : aEntities )
	{
		auto     playerMove = GetPlayerMoveData( player );
		auto     playerInfo = GetPlayerInfo( player );
		auto     camera     = GetCamera( player );
		auto     transform  = GetTransform( player );
		CLight*  flashlight = Ent_GetComponent< CLight >( player, "light" );

		Assert( playerMove );
		Assert( playerInfo );
		Assert( camera );
		Assert( transform );
		Assert( flashlight );

		apMove->SetPlayer( player );

		// TODO: prediction
		// apMove->MovePlayer( gLocalPlayer, &userCmd );
	
		// Player_UpdateFlashlight( player, &userCmd );
	
		// TEMP
		camera->aTransform.Edit().aPos.Edit()[ W_UP ] = playerMove->aOutViewHeight;

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
			auto renderComp = Ent_GetComponent< CRenderable_t >( player, "renderable" );

			// I hate this so much
			if ( renderComp->aHandle == InvalidHandle )
			{
				auto modelInfo = Ent_GetComponent< CModelInfo >( player, "modelInfo" );
				if ( !modelInfo )
				{
					// Log_Msg( "Failed to get modelInfo, Turning off playermodels for now\n" );
					// cl_playermodel_enable.SetValue( 0 );
					continue;
				}

				Handle model = Graphics_LoadModel( modelInfo->aPath );
				if ( model == InvalidHandle )
					continue;

				renderComp->aHandle = Graphics_CreateRenderable( model );
				if ( renderComp->aHandle == InvalidHandle )
					continue;
			}
			
			Renderable_t* renderData = Graphics_GetRenderableData( renderComp->aHandle );

			if ( !renderData )
				continue;

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
				ang = camera->aTransform.Get().aAng;
				ang[ YAW ] *= -1;
				ang[ YAW ] += 180;
				ang[ ROLL ]  = ang[ PITCH ] + 90;
				ang[ PITCH ] = 0;
			}
			else
			{
				ang = transform->aAng;
				ang[ ROLL ] += 90;
				ang[ YAW ] *= -1;
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

			Graphics_UpdateRenderableAABB( renderComp->aHandle );
		}
		else
		{
			// Make sure this thing is hidden
			auto renderComp = Ent_GetComponent< CRenderable_t >( player, "renderable" );
			if ( renderComp->aHandle == InvalidHandle )
				continue;

			Renderable_t* renderData = Graphics_GetRenderableData( renderComp->aHandle );
			if ( !renderData )
				continue;

			renderData->aVisible = false;
		}
	}
}


void PlayerManager::DoMouseLook( Entity player )
{
	Assert( Game_ProcessingClient() );

	auto transform = GetTransform( player );
	auto camera = GetCamera( player );

	const glm::vec2 mouse = Input_GetMouseDelta();

	// transform.aAng[PITCH] = -mouse.y;
	camera->aTransform.Edit().aAng.Edit()[PITCH] += mouse.y * m_pitch;
	camera->aTransform.Edit().aAng.Edit()[YAW] += mouse.x * m_yaw;
	transform->aAng.Edit()[ YAW ] += mouse.x * m_yaw;

	ClampAngles( *transform, *camera );
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
			if ( zoom->aWasZoomed )
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

	// auto& move = GetPlayerMoveData( player );
	auto transform = GetTransform( player );
	auto camera    = GetCamera( player );
	auto dir       = GetComp_Direction( player );
	auto rigidBody = GetRigidBody( player );

	Assert( transform );

	ClampAngles( *transform, *camera );

	if ( transform->aPos.aIsDirty || transform->aAng.aIsDirty )
	{
		glm::mat4 viewMatrixZ;
		Util_ToViewMatrixZ( viewMatrixZ, transform->aPos, transform->aAng );
		Util_GetViewMatrixZDirection( viewMatrixZ, dir->aForward.Edit(), dir->aRight.Edit(), dir->aUp.Edit() );
	}

	// MOVE ME ELSEWHERE IDK, MAYBE WHEN AN HEV SUIT COMPONENT IS MADE
	CalcZoom( camera, player );

	/* Copy the player transformation, and apply the view offsets to it. */
	CTransform transformView = *transform;
	transformView.aPos += camera->aTransform.Get().aPos;
	transformView.aAng = camera->aTransform.Get().aAng;
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

		Util_GetViewMatrixZDirection( viewMat, camera->aForward.Edit(), camera->aRight.Edit(), camera->aUp.Edit() );
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

		Util_GetViewMatrixZDirection( viewMat, camera->aForward.Edit(), camera->aRight.Edit(), camera->aUp.Edit() );
	}

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

	apMove      = GetPlayerMoveData( player );
	apRigidBody = GetRigidBody( player );
	apTransform = GetTransform( player );
	apCamera    = GetCamera( player );
	apDir       = Ent_GetComponent< CDirection >( player, "direction" );
	apPhysObj   = GetComp_PhysObjectPtr( player );

	Assert( apMove );
	Assert( apRigidBody );
	Assert( apTransform );
	Assert( apCamera );
	Assert( apDir );
	Assert( apPhysObj );
}


void PlayerMovement::OnPlayerSpawn( Entity player )
{
	EnsureUserCmd( player );

	auto move   = GetPlayerMoveData( player );
	auto camera = GetCamera(player );

	Assert( move );
	Assert( camera );

	// SetMoveType( *move, PlayerMoveType::Walk );
	SetMoveType( *move, PlayerMoveType::NoClip );

	camera->aTransform.Edit().aPos.Edit() = { 0, 0, sv_view_height.GetFloat() };
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

	// CHANGE THIS IN THE FUTURE FOR NETWORKING
#if 0
	if ( cl_thirdperson.GetBool() && cl_playermodel_enable.GetBool() )
	{
		auto model = GetEntitySystem()->GetComponent< Model* >( player );

		model->aTransform.aAng[ROLL] += 90;
		model->aTransform.aAng[YAW] *= -1;
		model->aTransform.aAng[YAW] += 180;
	}
#endif
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
	apPhysObj->SetCollisionEnabled( enable );
}


void PlayerMovement::EnableGravity( bool enabled )
{
	apPhysObj->SetGravityEnabled( enabled );
}

// ============================================================


void PlayerMovement::DisplayPlayerStats( Entity player ) const
{
	if ( !cl_show_player_stats )
		return;

	// auto& move = GetEntitySystem()->GetComponent< CPlayerMoveData >( player );
	auto rigidBody = GetRigidBody( player );
	auto transform = GetTransform( player );
	auto camera    = GetCamera( player );

	Assert( rigidBody );
	Assert( transform );
	Assert( camera );

	auto  camTransform = camera->aTransform;

	float speed        = glm::length( glm::vec2( rigidBody->aVel.Get().x, rigidBody->aVel.Get().y ) );

	gui->DebugMessage( "Player Pos:    %s", Vec2Str(transform->aPos).c_str() );
	gui->DebugMessage( "Player Ang:    %s", Vec2Str(transform->aAng).c_str() );
	gui->DebugMessage( "Player Vel:    %s", Vec2Str(rigidBody->aVel).c_str() );
	gui->DebugMessage( "Player Speed:  %.4f", speed );

	gui->DebugMessage( "Camera FOV:    %.4f", camera->aFov.Get() );
	gui->DebugMessage( "Camera Pos:    %s", Vec2Str(camTransform.Get().aPos).c_str() );
	gui->DebugMessage( "Camera Ang:    %s", Vec2Str(camTransform.Get().aAng).c_str() );
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
	apMove      = GetPlayerMoveData( player );
	apTransform = GetTransform( player );
	apRigidBody = GetRigidBody( player );
	apPhysObj   = GetComp_PhysObjectPtr( player );

	Assert( apMove );
	Assert( apTransform );
	Assert( apRigidBody );
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

	apCamera->aTransform.Edit().aPos.Edit()[ W_UP ] = apMove->aOutViewHeight;
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
		wishvel[ i ] = apCamera->aForward.Get()[ i ] * apRigidBody->aAccel.Get().x + apCamera->aRight.Get()[ i ] * apRigidBody->aAccel.Get()[ W_RIGHT ];

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
			apCamera->aTransform.Edit().aPos.Edit()[ W_UP ] += ( -apMove->aLandPower * sin( apMove->aLandTime / apMove->aLandPower ) / exp( apMove->aLandTime / apMove->aLandPower ) ) * sv_land_power_scale;

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
		apCamera->aTransform.Edit().aPos.Edit()[ W_UP ] += apMove->aBobOffsetAmount;
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

	apCamera->aTransform.Edit().aPos.Edit()[ W_UP ] += apMove->aBobOffsetAmount;
	
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

	apCamera->aTransform.Edit().aAng.Edit()[ ROLL ] = output;
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

