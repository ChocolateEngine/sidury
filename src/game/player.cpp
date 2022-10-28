#include "main.h"
#include "player.h"
#include "util.h"
#include "skybox.h"
#include "inputsystem.h"

#include "igui.h"
#include "iinput.h"
#include "render/irender.h"
#include "graphics/graphics.h"

#include "mapmanager.h"

#include <SDL2/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <algorithm>
#include <cmath>


CONVAR( in_forward, 0 );
CONVAR( in_side, 0 );
CONVAR( in_duck, 0 );
CONVAR( in_sprint, 0 );
CONVAR( in_jump, 0 );

extern ConVarRef en_timescale;

CONVAR( sv_sprint_mult, 2.4 );
//CONVAR( sv_sprint_mult, 4 );  // Temp until physics works
CONVAR( sv_duck_mult, 0.5 );

constexpr float DEFAULT_SPEED = 250.f;

ConVar forward_speed( "sv_forward_speed", DEFAULT_SPEED );
ConVar side_speed( "sv_side_speed", DEFAULT_SPEED );  // 350.f
ConVar max_speed( "sv_max_speed", DEFAULT_SPEED );  // 320.f

ConVar accel_speed( "sv_accel_speed", 10 );
ConVar accel_speed_air( "sv_accel_speed_air", 30 );
ConVar jump_force( "sv_jump_force", 250 );

#if BULLET_PHYSICS
ConVar stop_speed( "sv_stop_speed", 25 );
#else
ConVar stop_speed( "sv_stop_speed", 75 );
#endif

CONVAR( sv_friction, 8 );  // 4.f
CONVAR( sv_friction_enable, 1 );

CONVAR( phys_friction_player, 0.01 );
CONVAR( phys_player_offset, 40 );

// lerp the friction maybe?
//CONVAR( sv_new_movement, 1 );
//CONVAR( sv_friction_new, 8 );  // 4.f

CONVAR( sv_gravity, 800 );
CONVAR( ground_pos, 225 );

CONVAR( cl_stepspeed, 200 );
CONVAR( cl_steptime, 0.25 );
CONVAR( cl_stepduration, 0.22 );

CONVAR( cl_view_height, 67 );  // 67
CONVAR( cl_view_height_duck, 36 );  // 36
CONVAR( cl_view_height_lerp, 15 );  // 0.015

// multiplies the final velocity by this amount when setting the player position,
// a workaround for quake movement values not working correctly when lowered
#if !BULLET_PHYSICS
CONVAR( velocity_scale, 1 );
CONVAR( player_model_scale, 1 );
CONVAR( cl_cam_z, -2.5 );
#else
CONVAR( velocity_scale, 1.0 );
CONVAR( velocity_scale2, 0.01 );
CONVAR( player_model_scale, 25 );
CONVAR( cl_cam_z, -150 );
#endif

CONVAR( cl_thirdperson, 0 );
CONVAR( cl_playermodel_enable, 0 );
CONVAR( cl_cam_x, 0 );
CONVAR( cl_cam_y, 0 );
CONVAR( cl_show_player_stats, 0 );

CONVAR( phys_dbg_player, 0 );

CONVAR( r_fov, 106.f );
CONVAR( r_nearz, 1.f );
CONVAR( r_farz, 10000.f );

CONVAR( cl_zoom_fov, 40 );
CONVAR( cl_zoom_duration, 0.4 );

extern ConVar   m_yaw, m_pitch;

extern Entity   gLocalPlayer;

constexpr float PLAYER_MASS = 200.f;


CON_COMMAND( respawn )
{
	players->Respawn( gLocalPlayer );
}

CON_COMMAND( reset_velocity )
{
	auto& rigidBody = entities->GetComponent< CRigidBody >( gLocalPlayer );
	rigidBody.aVel = {0, 0, 0};
}


#define GET_KEY( key ) input->GetKeyState(key)

#define KEY_PRESSED( key ) input->KeyPressed(key)
#define KEY_RELEASED( key ) input->KeyReleased(key)
#define KEY_JUST_PRESSED( key ) input->KeyJustPressed(key)
#define KEY_JUST_RELEASED( key ) input->KeyJustReleased(key)


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


PlayerManager* players = nullptr;

// HACKY HACKY HACK
#if BULLET_PHYSICS
IPhysicsObject* apPhysObj = nullptr;
IPhysicsShape*  apPhysShape = nullptr;
#endif

PlayerManager::PlayerManager()
{
}

PlayerManager::~PlayerManager()
{
}


void PlayerManager::Init(  )
{
	entities->RegisterComponent<CPlayerMoveData>();
	entities->RegisterComponent<CPlayerInfo>();
	entities->RegisterComponent<CPlayerZoom>();
	entities->RegisterComponent<Model*>();
	entities->RegisterComponent<Model>();
	//entities->RegisterComponent<PhysicsObject*>();

	apMove = entities->RegisterSystem<PlayerMovement>();
}


Entity PlayerManager::Create(  )
{
	// Add Components
	Entity player = entities->CreateEntity();

	entities->AddComponent< CPlayerMoveData >( player );
	entities->AddComponent< CPlayerInfo >( player );
	entities->AddComponent< CRigidBody >( player );
	Transform& transform = entities->AddComponent< Transform >( player );
	entities->AddComponent< CCamera >( player );
	entities->AddComponent< CDirection >( player );
	entities->AddComponent< CPlayerZoom >( player );

	//Model* model = new Model;
	//graphics->LoadModel( "materials/models/protogen_wip_22/protogen_wip_22.obj", "", model );
	//entities->AddComponent( player, model );

	// Model *model = graphics->LoadModel( "materials/models/protogen_wip_25d/protogen_wip_25d.obj" );
	// entities->AddComponent< Model* >( player, model );

	PhysicsShapeInfo shapeInfo( PhysShapeType::Cylinder );
	shapeInfo.aBounds = glm::vec3(72, 16, 1);

	apPhysShape = physenv->CreateShape( shapeInfo );

	Assert( apPhysShape );

	PhysicsObjectInfo physInfo;
	physInfo.aMotionType = PhysMotionType::Dynamic;
	physInfo.aPos = transform.aPos;
	physInfo.aAng = transform.aAng;

	physInfo.aCustomMass = true;
	physInfo.aMass = PLAYER_MASS;

	apPhysObj = physenv->CreateObject( apPhysShape, physInfo );
	apPhysObj->SetAllowSleeping( false );
	apPhysObj->SetMotionQuality( PhysMotionQuality::LinearCast );
	apPhysObj->SetLinearVelocity( {0, 0, 0} );
	apPhysObj->SetAngularVelocity( {0, 0, 0} );

	gamephys.SetMaxVelocities( apPhysObj );

	apPhysObj->SetFriction( phys_friction_player );

	// Don't allow any rotation on this
	apPhysObj->SetInverseMass( 1.f / PLAYER_MASS );
	apPhysObj->SetInverseInertia( {0, 0, 0}, {1, 0, 0, 0} );

	// rotate 90 degrees
	apPhysObj->SetAng( {90, 0, 0} );

	aPlayerList.push_back( player );

	return player;
}


void PlayerManager::Spawn( Entity player )
{
	apMove->OnPlayerSpawn( player );

	Respawn( player );
}


void PlayerManager::Respawn( Entity player )
{
	auto& rigidBody = GetRigidBody( player );
	auto& transform = GetTransform( player );
	auto& camTransform = GetCamera( player ).aTransform;
	auto& zoom = GetPlayerZoom( player );

	transform.aPos = mapmanager->GetSpawnPos();
	transform.aAng = {0, mapmanager->GetSpawnAng().y, 0};
	camTransform.aAng = mapmanager->GetSpawnAng();
	rigidBody.aVel = {0, 0, 0};
	rigidBody.aAccel = {0, 0, 0};

	zoom.aOrigFov = r_fov;
	zoom.aNewFov = r_fov;

	apMove->OnPlayerRespawn( player );
}


void PlayerManager::Update( float frameTime )
{
	for ( Entity player: aPlayerList )
	{
		if ( !Game_IsPaused() )
		{
			DoMouseLook( player );
			apMove->MovePlayer( player );
		}

		auto& playerInfo = entities->GetComponent< CPlayerInfo >( player );

		if ( (cl_thirdperson.GetBool() && cl_playermodel_enable.GetBool()) || !playerInfo.aIsLocalPlayer )
		{
			ModelDraw_t* renderable = entities->GetComponent< ModelDraw_t* >( player );

			auto model = entities->GetComponent< HModel >( player );
			Transform transform = entities->GetComponent< Transform >( player );
			transform.aScale = glm::vec3(player_model_scale.GetFloat(), player_model_scale.GetFloat(), player_model_scale.GetFloat());

			renderable->aModelMatrix = transform.ToMatrix();
			renderable->aModel = model.handle;

			Graphics_DrawModel( renderable );
		}

		UpdateView( playerInfo, player );
	}
}


inline float DegreeConstrain( float num )
{
	num = std::fmod(num, 360.0f);
	return (num < 0.0f) ? num += 360.0f : num;
}


inline void ClampAngles( Transform& transform, CCamera& camera )
{
	transform.aAng[YAW] = DegreeConstrain( transform.aAng[YAW] );
	camera.aTransform.aAng[YAW] = DegreeConstrain( camera.aTransform.aAng[YAW] );
	camera.aTransform.aAng[PITCH] = std::clamp( camera.aTransform.aAng[PITCH], -90.0f, 90.0f );
};


void PlayerManager::DoMouseLook( Entity player )
{
	auto& transform = GetTransform( player );
	auto& camera = GetCamera( player );

	// const glm::vec2 mouse = in_sensitivity.GetFloat() * glm::vec2(input->GetMouseDelta());
	const glm::vec2 mouse = gameinput.GetMouseDelta();

	// transform.aAng[PITCH] = -mouse.y;
	camera.aTransform.aAng[PITCH] += mouse.y * m_pitch;
	camera.aTransform.aAng[YAW] += mouse.x * m_yaw;
	transform.aAng[YAW] += mouse.x * m_yaw;

	ClampAngles( transform, camera );
}


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


void CalcZoom( CCamera& camera, Entity player )
{
	auto& zoom = GetPlayerZoom( player );

	if ( zoom.aOrigFov != r_fov )
	{
		zoom.aZoomTime = 0.f;  // idk lol
		zoom.aOrigFov = r_fov;
	}

	if ( Game_IsPaused() )
	{
		camera.aFov = zoom.aNewFov;
		return;
	}

	float lerpTarget = 0.f;

	if ( KEY_PRESSED( SDL_SCANCODE_Z ) )
	{
		if ( KEY_JUST_PRESSED( SDL_SCANCODE_Z ) )
		{
			zoom.aZoomChangeFov = camera.aFov;
			
			// scale duration by how far zoomed in we are compared to the target zoom level
			zoom.aZoomDuration = Lerp_GetDurationIn( cl_zoom_fov, zoom.aOrigFov, camera.aFov, cl_zoom_duration );
			zoom.aZoomTime = 0.f;
		}

		lerpTarget = cl_zoom_fov;
	}
	else
	{
		if ( KEY_JUST_RELEASED( SDL_SCANCODE_Z ) )
		{
			zoom.aZoomChangeFov = camera.aFov;

			// scale duration by how far zoomed in we are compared to the target zoom level
			zoom.aZoomDuration = Lerp_GetDuration( cl_zoom_fov, zoom.aOrigFov, camera.aFov, cl_zoom_duration );
			zoom.aZoomTime = 0.f;
		}

		lerpTarget = zoom.aOrigFov;
	}

	zoom.aZoomTime += gFrameTime;

	if ( zoom.aZoomDuration >= zoom.aZoomTime )
	{
		float time = (zoom.aZoomTime / zoom.aZoomDuration);

		// smooth cosine lerp
		// float timeCurve = (( cos((zoomLerp * M_PI) - M_PI) ) + 1) * 0.5;
		float timeCurve = Math_EaseOutQuart( time );

		zoom.aNewFov = std::lerp( zoom.aZoomChangeFov, lerpTarget, timeCurve );
	}

	// scale mouse delta
	float fovScale = (zoom.aNewFov / zoom.aOrigFov);
	gameinput.SetMouseDeltaScale( {fovScale, fovScale} );

	camera.aFov = zoom.aNewFov;
}


void PlayerManager::UpdateView( CPlayerInfo& info, Entity player )
{
	auto& move = GetPlayerMoveData( player );
	auto& transform = GetTransform( player );
	auto& camera = GetCamera( player );
	auto& dir = GetDirection( player );
	auto& rigidBody = GetRigidBody( player );

	ClampAngles( transform, camera );

	GetDirectionVectors( transform.ToViewMatrixZ(  ), dir.aForward, dir.aRight, dir.aUp );

	// MOVE ME ELSEWHERE IDK, MAYBE WHEN AN HEV SUIT COMPONENT IS MADE
	CalcZoom( camera, player );

	/* Copy the player transformation, and apply the view offsets to it. */
	Transform transformView = transform;
	// transformView.aPos += (camera.aTransform.aPos + glm::vec3(1, 1, cl_view_height_offset)) * velocity_scale.GetFloat();
	transformView.aPos += camera.aTransform.aPos;
	transformView.aAng = camera.aTransform.aAng;
	//Transform transformView = transform;
	//transformView.aPos += move.aViewOffset * velocity_scale.GetFloat();
	//transformView.aAng += move.aViewAngOffset;

	if ( cl_thirdperson.GetBool() )
	{
		Transform thirdPerson = {
			.aPos = {cl_cam_x.GetFloat(), cl_cam_y.GetFloat(), cl_cam_z.GetFloat()}
		};

		// thirdPerson.aPos = {cl_cam_x.GetFloat(), cl_cam_y.GetFloat(), cl_cam_z.GetFloat()};

		if ( info.aIsLocalPlayer )
		{
			// audio->SetListenerTransform( thirdPerson.aPos, transformView.aAng );
		}

		glm::mat4 viewMat = thirdPerson.ToMatrix( false ) * transformView.ToViewMatrixZ(  );

		gViewInfo.aViewPos = thirdPerson.aPos;
		Game_SetView( viewMat );
		GetDirectionVectors( viewMat, camera.aForward, camera.aRight, camera.aUp );
	}
	else
	{
		if ( info.aIsLocalPlayer )
		{
			// wtf broken??
			// audio->SetListenerTransform( transformView.aPos, transformView.aAng );
		}

		glm::mat4 viewMat = transformView.ToViewMatrixZ();

		gViewInfo.aViewPos = transformView.aPos;
		Game_SetView( viewMat );
		GetDirectionVectors( viewMat, camera.aForward, camera.aRight, camera.aUp );
	}

	if ( info.aIsLocalPlayer )
	{
		// scale the nearz and farz
		gView.aFarZ  = r_farz;
		gView.aNearZ = r_nearz;
		gView.aFOV   = camera.aFov;
		Game_UpdateProjection();

		// i feel like there's gonna be a lot more here in the future...
		GetSkybox().SetAng( transformView.aAng );
		audio->SetListenerTransform( transformView.aPos, transform.aAng );

#if AUDIO_OPENAL
		audio->SetListenerVelocity( rigidBody.aVel );
		audio->SetListenerOrient( camera.aForward, camera.aUp );
#endif
	}
}


// ============================================================


void PlayerMovement::OnPlayerSpawn( Entity player )
{
	auto& move = entities->GetComponent< CPlayerMoveData >( player );

	SetMoveType( move, PlayerMoveType::Walk );

	auto& camera = entities->GetComponent< CCamera >( player );
	camera.aTransform.aPos = {0, 0, cl_view_height.GetFloat()};
}


void PlayerMovement::OnPlayerRespawn( Entity player )
{
	auto& move = entities->GetComponent< CPlayerMoveData >( player );

#if BULLET_PHYSICS
	Transform transform = entities->GetComponent< Transform >( player );
	//auto& physObj = entities->GetComponent< PhysicsObject* >( player );
	transform.aPos.z += phys_player_offset;

	apPhysObj->SetPos( transform.aPos );
#endif

	// Init Smooth Duck
	move.aTargetViewHeight = GetViewHeight();
	move.aOutViewHeight = GetViewHeight();
}


void PlayerMovement::MovePlayer( Entity player )
{
	aPlayer = player;
	apMove = &entities->GetComponent< CPlayerMoveData >( player );
	apRigidBody = &entities->GetComponent< CRigidBody >( player );
	apTransform = &entities->GetComponent< Transform >( player );
	apCamera = &entities->GetComponent< CCamera >( player );
	apDir = &entities->GetComponent< CDirection >( player );
	//apPhysObj = entities->GetComponent< PhysicsObject* >( player );

#if BULLET_PHYSICS
	apPhysObj->SetAllowDebugDraw( phys_dbg_player );

	// update velocity
	apRigidBody->aVel = apPhysObj->GetLinearVelocity();

	//apPhysObj->SetSleepingThresholds( 0, 0 );
	//apPhysObj->SetAngularFactor( 0 );
	//apPhysObj->SetAngularVelocity( {0, 0, 0} );
	apPhysObj->SetFriction( phys_friction_player );
#endif

	UpdateInputs();

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
		auto model = entities->GetComponent< Model* >( player );

		model->aTransform.aAng[ROLL] += 90;
		model->aTransform.aAng[YAW] *= -1;
		model->aTransform.aAng[YAW] += 180;
	}
#endif
}


float PlayerMovement::GetViewHeight(  )
{
	if ( KEY_PRESSED(SDL_SCANCODE_LCTRL) || in_duck )
		return cl_view_height_duck;

	return cl_view_height;
}


void PlayerMovement::DetermineMoveType(  )
{
	if ( KEY_JUST_PRESSED(SDL_SCANCODE_V) )
		SetMoveType( *apMove, apMove->aMoveType == PlayerMoveType::NoClip ? PlayerMoveType::Walk : PlayerMoveType::NoClip );

	if ( KEY_JUST_PRESSED(SDL_SCANCODE_B) )
		SetMoveType( *apMove, apMove->aMoveType == PlayerMoveType::Fly ? PlayerMoveType::Walk : PlayerMoveType::Fly );
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
#if BULLET_PHYSICS
	apPhysObj->SetCollisionEnabled( enable );
#endif
}


void PlayerMovement::EnableGravity( bool enabled )
{
#if BULLET_PHYSICS
	// apPhysObj->SetGravity( enabled ? physenv->GetGravity() : glm::vec3(0, 0, 0) );
	apPhysObj->SetGravityEnabled( enabled );
#endif
}

// ============================================================


void PlayerMovement::DisplayPlayerStats( Entity player ) const
{
	if ( !cl_show_player_stats )
		return;

	auto& move = entities->GetComponent< CPlayerMoveData >( player );
	auto& rigidBody = entities->GetComponent< CRigidBody >( player );
	auto& transform = entities->GetComponent< Transform >( player );
	auto& camera = entities->GetComponent< CCamera >( player );
	auto& camTransform = camera.aTransform;

	float speed = glm::length( glm::vec2(rigidBody.aVel.x, rigidBody.aVel.y) );

	gui->DebugMessage( "Player Pos:    %s", Vec2Str(transform.aPos).c_str() );
	gui->DebugMessage( "Player Ang:    %s", Vec2Str(transform.aAng).c_str() );
	gui->DebugMessage( "Player Vel:    %s", Vec2Str(rigidBody.aVel).c_str() );
	gui->DebugMessage( "Player Speed:  %.4f", speed );

	gui->DebugMessage( "Camera FOV:    %.4f", camera.aFov );
	gui->DebugMessage( "Camera Pos:    %s", Vec2Str(camTransform.aPos).c_str() );
	gui->DebugMessage( "Camera Ang:    %s", Vec2Str(camTransform.aAng).c_str() );
}


void PlayerMovement::SetPos( const glm::vec3& origin )
{
	apTransform->aPos = origin;
}

const glm::vec3& PlayerMovement::GetPos(  ) const
{
	return apTransform->aPos;
}

void PlayerMovement::SetAng( const glm::vec3& angles )
{
	apTransform->aAng = angles;
}

const glm::vec3& PlayerMovement::GetAng(  ) const
{
	return apTransform->aAng;
}


void PlayerMovement::UpdateInputs(  )
{
	apRigidBody->aAccel = {0, 0, 0};

	float moveScale = 1.0f;

	apMove->aPrevPlayerFlags = apMove->aPlayerFlags;
	apMove->aPlayerFlags = PlyNone;

	if ( KEY_PRESSED(SDL_SCANCODE_LCTRL) || in_duck )
	{
		apMove->aPlayerFlags |= PlyInDuck;
		moveScale = sv_duck_mult;
	}

	else if ( KEY_PRESSED(SDL_SCANCODE_LSHIFT) || in_sprint )
	{
		apMove->aPlayerFlags |= PlyInSprint;
		moveScale = sv_sprint_mult;
	}

	const float forwardSpeed = forward_speed * moveScale;
	const float sideSpeed = side_speed * moveScale;
	apMove->aMaxSpeed = max_speed * moveScale;

	if ( KEY_PRESSED(SDL_SCANCODE_W) || in_forward.GetBool() )  apRigidBody->aAccel[W_FORWARD] = forwardSpeed;
	if ( KEY_PRESSED(SDL_SCANCODE_S) || in_forward == -1.f )    apRigidBody->aAccel[W_FORWARD] += -forwardSpeed;
	if ( KEY_PRESSED(SDL_SCANCODE_A) || in_side == -1.f )       apRigidBody->aAccel[W_RIGHT] = -sideSpeed;
	if ( KEY_PRESSED(SDL_SCANCODE_D) || in_side.GetBool() )     apRigidBody->aAccel[W_RIGHT] += sideSpeed;

	// kind of a hack
	// this feels really stupid
	static bool wasJumpButtonPressed = false;
	bool jump = KEY_PRESSED(SDL_SCANCODE_SPACE) || in_jump;

	if ( CalcOnGround() )
	{
		if ( jump && !wasJumpButtonPressed )
		{
			apRigidBody->aVel[W_UP] = jump_force;
			wasJumpButtonPressed = true;
			// Log_Msg( "New Velocity After Jumping: %s", Vec2Str( apRigidBody->aVel ).c_str() );
		}
	}
	else
	{
		wasJumpButtonPressed = false;
	}
}


class PlayerCollisionCheck : public PhysCollisionCollector
{
public:
	explicit PlayerCollisionCheck( const glm::vec3& srGravity, CPlayerMoveData* moveData ) :
		aGravity( srGravity ),
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

	CPlayerMoveData*        apMove;

private:
	glm::vec3               aGravity;
	float                   aBestDot = 0.f;
};


CONVAR( phys_player_max_sep_dist, 1 );

// Post Physics Simulation Update
void PlayerMovement::UpdatePosition( Entity player )
{
#if BULLET_PHYSICS
	apMove = &entities->GetComponent< CPlayerMoveData >( player );
	apTransform = &entities->GetComponent< Transform >( player );
	apRigidBody = &entities->GetComponent< CRigidBody >( player );
	//auto& physObj = entities->GetComponent< PhysicsObject* >( player );

	//if ( aMoveType == MoveType::Fly )
		//aTransform = apPhysObj->GetWorldTransform();
	//transform.aPos = physObj->GetWorldTransform().aPos;
	apTransform->aPos = apPhysObj->GetPos();
	apTransform->aPos.z -= phys_player_offset;

	if ( apMove->aMoveType != PlayerMoveType::NoClip )
	{
		PlayerCollisionCheck playerCollide( physenv->GetGravity(), apMove );
		apPhysObj->CheckCollision( phys_player_max_sep_dist, &playerCollide );
	}

	// um
	if ( apMove->aMoveType == PlayerMoveType::Walk )
	{
		WalkMovePostPhys();
	}

#else
	SetPos( GetPos() + (apRigidBody->aVel * velocity_scale.GetFloat()) * game->aFrameTime );

	// blech
	//if ( IsOnGround() && aMoveType != MoveType::NoClip )
	//	aTransform.position.y = ground_pos.GetFloat() * velocity_scale.GetFloat();
#endif
}


CONVAR( cl_duck_time, 0.4 );


float Math_EaseInOutCubic( float x )
{
	return x < 0.5 ? 4 * x * x * x : 1 - pow( -2 * x + 2, 3 ) / 2;
}



// VERY BUGGY STILL
void PlayerMovement::DoSmoothDuck()
{
	if ( IsOnGround() && apMove->aMoveType == PlayerMoveType::Walk )
	{
		if ( apMove->aTargetViewHeight != GetViewHeight() )
		{
			apMove->aPrevViewHeight = apMove->aOutViewHeight;
			apMove->aTargetViewHeight = GetViewHeight();
			apMove->aDuckTime = 0.f;

			apMove->aDuckDuration = Lerp_GetDuration( cl_view_height, cl_view_height_duck, apMove->aPrevViewHeight );

			// this is stupid
			if ( apMove->aTargetViewHeight == cl_view_height.GetFloat() )
				apMove->aDuckDuration = 1 - apMove->aDuckDuration;

			apMove->aDuckDuration *= cl_duck_time;
		}
	}
	else if ( WasOnGround() )
	{
		apMove->aPrevViewHeight = apMove->aOutViewHeight;
		apMove->aDuckDuration = Lerp_GetDuration( cl_view_height, cl_view_height_duck, apMove->aPrevViewHeight, cl_duck_time );
		apMove->aDuckTime = 0.f;
	}

	apMove->aDuckTime += gFrameTime;

	if ( apMove->aDuckDuration >= apMove->aDuckTime )
	{
		float time = (apMove->aDuckTime / apMove->aDuckDuration);
		float timeCurve = Math_EaseOutQuart( time );

		apMove->aOutViewHeight = std::lerp( apMove->aPrevViewHeight, apMove->aTargetViewHeight, timeCurve );
	}

	apCamera->aTransform.aPos[W_UP] = apMove->aOutViewHeight;
}


// temporarily using bullet directly until i abstract this
void PlayerMovement::DoRayCollision(  )
{
}


CONVAR( phys_player_max_slope_ang, 40 );


bool PlayerMovement::CalcOnGround()
{
	// static float maxSlopeAngle = cos( apMove->aMaxSlopeAngle );
	float maxSlopeAngle = cos( phys_player_max_slope_ang * (M_PI / 180.f) );

	if ( apMove->apGroundObj == nullptr )
		return false;

	glm::vec3 up = -glm::normalize( physenv->GetGravity() );

	if ( glm::dot(apMove->aGroundNormal, up) > maxSlopeAngle )
		apMove->aPlayerFlags |= PlyOnGround;
	else
		apMove->aPlayerFlags &= ~PlyOnGround;

#if 0
	// aOnGround = GetPos().y <= ground_pos.GetFloat() * velocity_scale.GetFloat();
	bool onGround = GetPos()[W_UP] <= ground_pos * velocity_scale;

	if ( onGround )
		apMove->aPlayerFlags |= PlyOnGround;
	else
		apMove->aPlayerFlags &= ~PlyOnGround;
#endif

	return apMove->aPlayerFlags & PlyOnGround;
}


bool PlayerMovement::IsOnGround(  )
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
		wishvel = wishvel * apMove->aMaxSpeed/wishspeed;
		wishspeed = apMove->aMaxSpeed;
	}

	return wishspeed;
}

float PlayerMovement::GetMaxSpeed(  )
{
	return apMove->aMaxSpeed;
}

float PlayerMovement::GetMaxSpeedBase(  )
{
	return max_speed;
}

float PlayerMovement::GetMaxSprintSpeed(  )
{
	return GetMaxSpeedBase(  ) * sv_sprint_mult;
}

float PlayerMovement::GetMaxDuckSpeed(  )
{
	return GetMaxSpeedBase(  ) * sv_duck_mult;
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


void PlayerMovement::PlayStepSound(  )
{
	if ( !cl_step_sound.GetBool() )
		return;

	//float vel = glm::length( glm::vec2(aVelocity.x, aVelocity.y) ); 
	float vel = glm::length( glm::vec3(apRigidBody->aVel.x, apRigidBody->aVel.y, apRigidBody->aVel.z * cl_step_sound_gravity_scale) ); 
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
	float vel = glm::length( glm::vec3( apRigidBody->aVel.x, apRigidBody->aVel.y, apRigidBody->aVel.z * cl_step_sound_gravity_scale ) );
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


void PlayerMovement::BaseFlyMove(  )
{
	glm::vec3 wishvel(0,0,0);
	glm::vec3 wishdir(0,0,0);

	// forward and side movement
	for ( int i = 0; i < 3; i++ )
		wishvel[i] = apCamera->aForward[i]*apRigidBody->aAccel.x + apCamera->aRight[i]*apRigidBody->aAccel[W_RIGHT];

	float wishspeed = GetMoveSpeed( wishdir, wishvel );

	AddFriction(  );
	Accelerate( wishspeed, wishdir );
}


#define SET_VELOCITY() \
glm::vec3 __vel{}; \
for ( int i = 0; i < 3; i++ ) \
	__vel[i] = apCamera->aForward[i]*apRigidBody->aAccel.x + apCamera->aRight[i]*apRigidBody->aAccel[W_RIGHT]; \
apPhysObj->SetLinearVelocity( apPhysObj->GetLinearVelocity() + (__vel * velocity_scale2.GetFloat()) )


void PlayerMovement::NoClipMove(  )
{
	BaseFlyMove(  );

#if BULLET_PHYSICS
	//SET_VELOCITY();
	apPhysObj->SetLinearVelocity( apRigidBody->aVel );
#else
	UpdatePosition( aPlayer );
#endif
}


void PlayerMovement::FlyMove(  )
{
	BaseFlyMove(  );

#if BULLET_PHYSICS
	//SET_VELOCITY();
	apPhysObj->SetLinearVelocity( apRigidBody->aVel );
#else
	UpdatePosition( aPlayer );
#endif

	DoRayCollision(  );
}


CONVAR( cl_land_sound_threshold, 0.1 );


void PlayerMovement::WalkMove(  )
{
	glm::vec3 wishvel = apDir->aForward*apRigidBody->aAccel.x + apDir->aRight*apRigidBody->aAccel[W_RIGHT];
	wishvel[W_UP] = 0.f;

	glm::vec3 wishdir(0,0,0);
	float wishspeed = GetMoveSpeed( wishdir, wishvel );

	// man
	static bool wasOnGround = IsOnGround();
	bool onGround = CalcOnGround();

	if ( onGround )
	{
#if 1 // !BULLET_PHYSICS
		// bullet friction is kinda meh
		if ( sv_friction_enable )
			AddFriction(  );
#endif
		Accelerate( wishspeed, wishdir, false );
	}
	else
	{	// not on ground, so little effect on velocity
		Accelerate( wishspeed, wishvel, true );

#if !BULLET_PHYSICS
		// Apply Gravity
		apRigidBody->aVel[W_UP] -= sv_gravity * game->aFrameTime;
#endif
	}

#if BULLET_PHYSICS
	//SET_VELOCITY();
	apPhysObj->SetLinearVelocity( apRigidBody->aVel );

	// uhhh
	apRigidBody->aVel = apPhysObj->GetLinearVelocity();

#else
	UpdatePosition( aPlayer );
#endif

	DoRayCollision(  );
	
	StopStepSound(  );

	if ( IsOnGround() && !onGround )
	{
	//	PlayImpactSound();

		//glm::vec2 vel( apRigidBody->aVel.x, apRigidBody->aVel.y );
		//if ( glm::length( vel ) < cl_land_sound_threshold )
		//	PlayStepSound();
	}

	// something is wrong with this here on bullet
#if 1 // !BULLET_PHYSICS
	DoSmoothLand( wasOnGround );
#endif

	DoViewBob(  );
	DoViewTilt(  );

	// if ( IsOnGround() )
	//	apRigidBody->aVel[W_UP] = 0;

	wasOnGround = IsOnGround();
}


void PlayerMovement::WalkMovePostPhys(  )
{
	bool onGroundPrev = IsOnGround();
	bool onGround = CalcOnGround();

	// uhhh
	// apRigidBody->aVel = apPhysObj->GetLinearVelocity();

	if ( onGround && !onGroundPrev )
		PlayImpactSound();

	if ( onGround )
		apRigidBody->aVel[W_UP] = 0.f;
}


CONVAR( cl_land_smoothing, 1 );
CONVAR( cl_land_max_speed, 1000 );
CONVAR( cl_land_power, 1 ); // 2
CONVAR( cl_land_vel_scale, 1 ); // 0.01
CONVAR( cl_land_power_scale, 100 ); // 0.01
CONVAR( cl_land_timevar, 2 );

void PlayerMovement::DoSmoothLand( bool wasOnGround )
{
	static float landPower = 0.f, landTime = 0.f;

    if ( cl_land_smoothing )
    {
        // NOTE: this doesn't work properly when jumping mid duck and landing
        // meh, works well enough with the current values for now
        if ( CalcOnGround() && !wasOnGround )
        // if ( CalcOnGround() && !WasOnGround() )
        {
			float baseLandVel = abs(apRigidBody->aVel[W_UP] * cl_land_vel_scale.GetFloat()) / cl_land_max_speed.GetFloat();
			float landVel = std::clamp( baseLandVel * M_PI, 0.0, M_PI );

			landPower = (-cos( landVel ) + 1) / 2;

			landTime = 0.f;
        }

		landTime += gFrameTime * cl_land_timevar;

		if ( landPower > 0.f )
			// apCamera->aTransform.aPos[W_UP] += (- landPower * sin(landTime / landPower / 2) / exp(landTime / landPower)) * cl_land_power_scale;
			apCamera->aTransform.aPos[W_UP] += (- landPower * sin(landTime / landPower) / exp(landTime / landPower)) * cl_land_power_scale;
    }
    else
    {
		landPower = 0.f;
        landTime = 0.f;
    }
}


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


// TODO: doesn't smoothly transition out of viewbob still
// TODO: maybe make a minimum speed threshold and start lerping to 0
//  or some check if your movements changed compared to the previous frame
//  so if you start moving again faster than min speed, or movements change,
//  you restart the view bob, or take a new step right there
void PlayerMovement::DoViewBob(  )
{
	if ( !cl_bob_enabled )
		return;

	//static glm::vec3 prevMove = aMove;
	//static bool inExit = false;

	if ( !IsOnGround() /*|| aMove != prevMove || inExit*/ )
	{
		// lerp back to 0 to not snap view the offset (not good enough) and reset input
		apMove->aWalkTime = 0.f;
		apMove->aBobOffsetAmount = glm::mix( apMove->aBobOffsetAmount, 0.f, cl_bob_exit_lerp.GetFloat() );
		apCamera->aTransform.aPos[W_UP] += apMove->aBobOffsetAmount;
		//inExit = aBobOffsetAmount > 0.01;
		//prevMove = aMove;
		return;
	}

	static bool playedStepSound = false;

	float vel = glm::length( glm::vec2(apRigidBody->aVel.x, apRigidBody->aVel.y) ); 

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

	apCamera->aTransform.aPos[W_UP] += apMove->aBobOffsetAmount;
	
	if ( cl_bob_debug )
	{
		gui->DebugMessage( "Walk Time * Speed:  %.8f", apMove->aWalkTime );
		gui->DebugMessage( "View Bob Offset:    %.4f", apMove->aBobOffsetAmount );
		gui->DebugMessage( "View Bob Speed:     %.6f", speedFactor );
	}
}


CONVAR( cl_tilt, 1 );
CONVAR( cl_tilt_speed, 0.1 );
CONVAR( cl_tilt_threshold, 200 );

CONVAR( cl_tilt_type, 1 );
CONVAR( cl_tilt_lerp, 5 );
CONVAR( cl_tilt_lerp_new, 10 );
CONVAR( cl_tilt_speed_scale, 0.043 );
CONVAR( cl_tilt_scale, 0.2 );
CONVAR( cl_tilt_threshold_new, 12 );


void PlayerMovement::DoViewTilt(  )
{
	if ( cl_tilt == false )
		return;

	static float prevTilt = 0.f;

	float output = glm::dot( apRigidBody->aVel, apDir->aRight );
	float side = output < 0 ? -1 : 1;

	if ( cl_tilt_type == 1.f )
	{
		float speedFactor = glm::max(0.f, glm::log( glm::max(0.f, (fabs(output) * cl_tilt_speed_scale + 1) - cl_tilt_threshold_new) ));

		/* Now Lerp the tilt angle with the previous angle to make a smoother transition. */
		output = glm::mix( prevTilt, speedFactor * side * cl_tilt_scale, cl_tilt_lerp_new * gFrameTime );
	}
	else // type 0
	{
		output = glm::clamp( glm::max(0.f, fabs(output) - cl_tilt_threshold) / GetMaxSprintSpeed(), 0.f, 1.f ) * side;

		/* Now Lerp the tilt angle with the previous angle to make a smoother transition. */
		output = glm::mix( prevTilt, output * cl_tilt, cl_tilt_lerp * gFrameTime );
	}

	apCamera->aTransform.aAng[ROLL] = output;

	prevTilt = output;
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
void PlayerMovement::AddFriction(  )
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
	start.x = stop.x = GetPos().x + vel.x / speed*idk;
	start[W_RIGHT] = stop[W_RIGHT] = GetPos()[W_RIGHT] + vel[W_RIGHT] / speed*idk;
	start[W_UP] = stop[W_UP] = GetPos()[W_UP] + vel[W_UP] / speed*idk;
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

	float currentspeed = glm::dot( apRigidBody->aVel, wishDir );
	float addspeed = baseWishSpeed - currentspeed;

	if ( addspeed <= 0.f )
		return;

	addspeed = glm::min( addspeed, accel_speed * gFrameTime * wishSpeed );

	for ( int i = 0; i < 3; i++ )
		apRigidBody->aVel[i] += addspeed * wishDir[i];
}


void PlayerMovement::AddGravity(  )
{
#if !BULLET_PHYSICS
	apRigidBody->aVel[W_UP] -= sv_gravity * game->aFrameTime;
#endif
}

