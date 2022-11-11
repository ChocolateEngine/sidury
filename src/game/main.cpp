#include "main.h"
#include "core/systemmanager.h"
#include "core/asserts.h"

#include "iinput.h"
#include "igui.h"
#include "render/irender.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl.h"

#include "util.h"
#include "game_physics.h"
#include "player.h"
#include "entity.h"
#include "graphics/graphics.h"
#include "mapmanager.h"
#include "inputsystem.h"
#include "skybox.h"

#include "tools/light_editor.h"

#include <SDL_system.h>

#include <algorithm>


GameSystem*      game         = nullptr;

BaseGuiSystem*   gui          = nullptr;
IRender*         render       = nullptr;
BaseInputSystem* input        = nullptr;
BaseAudioSystem* audio        = nullptr;

static bool      gPaused      = false;
float            gFrameTime   = 0.f;
double           gCurTime     = 0.0;  // i could make this a size_t, and then just have it be every 1000 is 1 second

Entity           gLocalPlayer = ENT_INVALID;
ViewportCamera_t gView{};

// Audio Channels
Handle           hAudioMusic = InvalidHandle;

// only large near and farz for riverhouse and quake movement
// aView( 0, 0, 200, 200, r_nearz, r_farz, r_fov )

#define SPAWN_PROTOGEN 0

extern ConVar r_nearz, r_farz, r_fov;

CONVAR( phys_friction, 10 );
CONVAR( dbg_global_axis, 1 );
CONVAR( dbg_global_axis_size, 15 );

CONVAR( vrcmdl_scale, 40 );

extern ConVar en_timescale;

extern ConVar velocity_scale;


struct ModelPhysTest
{
	Model* mdl;
	std::vector<IPhysicsObject*> physObj;
};


const int physEntCount = 0;
ModelPhysTest* g_world = new ModelPhysTest{new Model, {}};

// GET RID OF THIS
std::vector< Entity > g_protos;
std::vector< Entity > g_otherEnts;
std::vector< Entity > g_staticEnts;
std::vector< ModelPhysTest* > g_physEnts;


void CreateProtogen( const std::string& path )
{
#if 1
	Entity proto = entities->CreateEntity();

	ModelDraw_t modelDraw{};
	modelDraw.aModel = Graphics_LoadModel( path );
	entities->AddComponent< ModelDraw_t >( proto, modelDraw );

	Transform& transform = entities->AddComponent< Transform >( proto );

	auto playerTransform = entities->GetComponent< Transform >( gLocalPlayer );

	transform.aPos   = playerTransform.aPos;
	transform.aScale = {vrcmdl_scale.GetFloat(), vrcmdl_scale.GetFloat(), vrcmdl_scale.GetFloat()};

	g_protos.push_back( proto );
	Graphics_AddModelDraw( &entities->GetComponent< ModelDraw_t >( proto ) );
#endif
}


void CreateModelEntity( const std::string& path )
{
#if 0
	Entity physEnt = entities->CreateEntity();

	Model* model   = Graphics_LoadModel( path );
	entities->AddComponent< Model* >( physEnt, model );
	Transform& transform = entities->AddComponent< Transform >( physEnt );

	Transform& playerTransform = entities->GetComponent< Transform >( game->aLocalPlayer );
	transform = playerTransform;

	g_staticEnts.push_back( physEnt );
#endif
}


void CreatePhysEntity( const std::string& path )
{
#if 1
	Entity physEnt = entities->CreateEntity();

	ModelDraw_t modelDraw{};
	modelDraw.aModel = Graphics_LoadModel( path );
	entities->AddComponent< ModelDraw_t >( physEnt, modelDraw );

	Graphics_AddModelDraw( &entities->GetComponent< ModelDraw_t >( physEnt ) );

	Transform& transform = entities->GetComponent< Transform >( gLocalPlayer );

	PhysicsShapeInfo shapeInfo( PhysShapeType::Convex );

	Phys_GetModelVerts( modelDraw.aModel, shapeInfo.aConvexData );

	IPhysicsShape* shape = physenv->CreateShape( shapeInfo );

	if ( shape == nullptr )
	{
		Log_ErrorF( "Failed to create physics shape for model: \"%s\"\n", path.c_str() );
		return;
	}

	PhysicsObjectInfo physInfo;
	physInfo.aPos = transform.aPos;  // NOTE: THIS IS THE CENTER OF MASS
	physInfo.aAng = transform.aAng;
	physInfo.aMass = 1.f;
	physInfo.aMotionType = PhysMotionType::Dynamic;
	physInfo.aStartActive = true;

	IPhysicsObject* phys = physenv->CreateObject( shape, physInfo );
	phys->SetFriction( phys_friction );

	Phys_SetMaxVelocities( phys );

	entities->AddComponent< IPhysicsShape * >( physEnt, shape );
	entities->AddComponent< IPhysicsObject * >( physEnt, phys );

	g_otherEnts.push_back( physEnt );
#endif
}

// #define DEFAULT_PROTOGEN_PATH "materials/models/protogen_wip_25d/protogen_wip_25d.obj"
#define DEFAULT_PROTOGEN_PATH "materials/models/protogen_wip_25d/protogen_25d.glb"

CON_COMMAND( create_proto )
{
	CreateProtogen( DEFAULT_PROTOGEN_PATH );
}

CON_COMMAND( create_gltf_proto )
{
	CreateProtogen( "materials/models/protogen_wip_25d/protogen_25d.glb" );
}

CON_COMMAND( create_look_entity )
{
	if ( args.size() )
		CreateProtogen( args[ 0 ] );
}

CON_COMMAND( delete_protos )
{
	for ( auto& proto : g_protos )
	{
		// Graphics_FreeModel( entities->GetComponent< Model* >( proto ) );
		Graphics_RemoveModelDraw( &entities->GetComponent< ModelDraw_t >( proto ) );
		entities->DeleteEntity( proto );
	}

	g_protos.clear();
}

CON_COMMAND( create_phys_test )
{
	CreatePhysEntity( "materials/models/riverhouse/riverhouse.obj" );
}

CON_COMMAND( create_phys_proto )
{
	CreatePhysEntity( "materials/models/protogen_wip_25d/protogen_wip_25d_big.obj" );
}


GameSystem::GameSystem()
{
	game = this;
}


GameSystem::~GameSystem(  )
{
	Skybox_Destroy();
	Phys_Shutdown();

	//if ( aLocalPlayer )
	//	delete aLocalPlayer;

	for ( int i = 0; i < physEntCount; i++ )
	{
		if ( g_physEnts[i] )
			delete g_physEnts[i];

		g_physEnts[i] = nullptr;
	}

	LightEditor_Shutdown();

	/*for ( int i = 0; i < aModels.size(); i++ )
	{
		if ( aModels[i] )
			delete aModels[i];
	}

	aModels.clear();*/

	//if ( g_proto )
	//	delete g_proto;
}



#ifdef _WIN32
  #define WM_PAINT 0x000F

void Game_WindowMessageHook( void* userdata, void* hWnd, unsigned int message, Uint64 wParam, Sint64 lParam )
{
	switch ( message )
	{
		case WM_PAINT:
		{
			Log_Msg( "WM_PAINT\n" );
			break;
		}

		default:
		{
			// static size_t i = 0;
			// Log_MsgF( "erm %zd\n", i++ );
			break;
		}
	}
}
#endif


bool GameSystem::Init()
{
	Game_RegisterKeys();
	Game_UpdateProjection();

	if ( !Graphics_Init() )
		return false;

#ifdef _WIN32
	SDL_SetWindowsMessageHook( Game_WindowMessageHook, nullptr );
#endif

	srand( ( unsigned int )time( 0 ) );  // setup rand(  )

#if AUDIO_OPENAL
	hAudioMusic = audio->RegisterChannel( "Music" );
#endif

	Phys_Init();
	LightEditor_Init();
	Skybox_Init();

	entities = new EntityManager;
	entities->Init();

	// voxelworld->Init(  );

	players = entities->RegisterSystem<PlayerManager>();
	players->Init();

	gLocalPlayer = players->Create();

	// mark this as the local player
	auto& playerInfo = entities->GetComponent< CPlayerInfo >( gLocalPlayer );
	playerInfo.aIsLocalPlayer = true;

	players->Spawn( gLocalPlayer );

	Log_Msg( "Game Loaded!\n" );
	return true;
}


void Game_RegisterKeys()
{
	input->RegisterKey( SDL_SCANCODE_W );
	input->RegisterKey( SDL_SCANCODE_S );
	input->RegisterKey( SDL_SCANCODE_A );
	input->RegisterKey( SDL_SCANCODE_D );

	input->RegisterKey( SDL_SCANCODE_LCTRL );
	input->RegisterKey( SDL_SCANCODE_LSHIFT );
	input->RegisterKey( SDL_SCANCODE_SPACE );
	
	input->RegisterKey( SDL_SCANCODE_V ); // noclip
	input->RegisterKey( SDL_SCANCODE_B ); // flight

	input->RegisterKey( SDL_SCANCODE_Z );  // zoom button

	// Debugging Keys

	input->RegisterKey( SDL_SCANCODE_G ); // play a test sound at current position in world
	input->RegisterKey( SDL_SCANCODE_H ); // stop all test sounds

	input->RegisterKey( SDL_SCANCODE_E ); // create protogen
	input->RegisterKey( SDL_SCANCODE_R ); // create protogen hold down key

	input->RegisterKey( SDL_SCANCODE_T ); // create a point light
	input->RegisterKey( SDL_SCANCODE_Y ); // create a cone light

	gameinput.Init();
}


void CenterMouseOnScreen()
{
	int w, h;
	SDL_GetWindowSize( render->GetWindow(), &w, &h );
	SDL_WarpMouseInWindow( render->GetWindow(), w / 2, h / 2 );
}


bool Game_InMap()
{
	return MapManager_HasMap();
}


// testing
std::vector<Handle> streams {};
Model* g_streamModel = nullptr;


ConVar snd_cube_scale("snd_cube_scale", "0.05");
extern ConVar velocity_scale;


CONVAR( r_render, 1 );


void GameSystem::Update( float frameTime )
{
	input->Update( frameTime );

	ImGui::NewFrame();
	ImGui_ImplSDL2_NewFrame();

	Game_Update( frameTime );

	LightEditor_Update();

	gui->Update( frameTime );

	if ( !( SDL_GetWindowFlags( render->GetWindow() ) & SDL_WINDOW_MINIMIZED ) && r_render )
	{
		Graphics_Present();
	}
	else
	{
		ImGui::EndFrame();
	}

	Con_Update();
}


void EntUpdate()
{
	// blech
	for ( auto &ent : g_otherEnts )
	{
		ModelDraw_t& model = entities->GetComponent< ModelDraw_t >( ent );

		if ( model.aModel == InvalidHandle )
			continue;

		// Model *physObjList = &entities->GetComponent< Model >( ent );

		// Transform& transform = entities->GetComponent< Transform >( ent );
		IPhysicsObject* phys = entities->GetComponent< IPhysicsObject* >( ent );

		if ( phys )
		{
			phys->SetFriction( phys_friction );
			Util_ToMatrix( model.aModelMatrix, phys->GetPos(), phys->GetAng() );
		}
	}
}


void Game_Update( float frameTime )
{
	gFrameTime = frameTime * en_timescale;

	Graphics_NewFrame();
	Game_HandleSystemEvents();

	gameinput.Update();

	Game_CheckPaused();

	if ( gPaused )
	{
		//ResetInputs(  );
		//players->Update( 0.f );
		//return;
		gFrameTime = 0.f;
	}

	gCurTime += gFrameTime;

	MapManager_Update();

	// WORLD GLOBAL AXIS
	// if ( dbg_global_axis )
	// {
	// 	graphics->DrawLine( {0, 0, 0}, {dbg_global_axis_size.GetFloat(), 0, 0}, {1, 0, 0});
	// 	graphics->DrawLine( {0, 0, 0}, {0, dbg_global_axis_size.GetFloat(), 0}, {0, 1, 0} );
	// 	graphics->DrawLine( {0, 0, 0}, {0, 0, dbg_global_axis_size.GetFloat()}, {0, 0, 1} );
	// }

	players->Update( gFrameTime );

	physenv->Simulate( gFrameTime );

	EntUpdate();

	// stupid
	for ( auto& player: players->aPlayerList )
	{
		players->apMove->UpdatePosition( player );
	}

	players->apMove->DisplayPlayerStats( gLocalPlayer );

	Game_SetupModels( gFrameTime );
	Game_ResetInputs();
	Game_UpdateAudio();

	if ( input->WindowHasFocus() && !gPaused )
	{
		CenterMouseOnScreen();
	}
}


void Game_SetPaused( bool paused )
{
	gPaused = paused;
}


bool Game_IsPaused()
{
	return gPaused;
}


void Game_CheckPaused()
{
	bool wasPaused = gPaused;
	gPaused = gui->IsConsoleShown();

	if ( wasPaused != gPaused )
	{
		SDL_SetRelativeMouseMode( (SDL_bool)!gPaused );

		if ( gPaused )
		{
			CenterMouseOnScreen();
		}
	}

	audio->SetPaused( gPaused );
}


// from vkquake
glm::vec3 Util_VectorToAngles( const glm::vec3& forward )
{
	glm::vec3 angles;

	if (forward.x == 0.f && forward.y == 0.f )
	{
		// either vertically up or down
		angles[PITCH] = (forward.z > 0) ? -90 : 90;
		angles[YAW] = 0;
		angles[ROLL] = 0;
	}
	else
	{
		angles[PITCH] = -atan2(forward.z, sqrt( glm::dot(forward, forward) ));
		angles[YAW] = atan2(forward.y, forward.x);
		angles[ROLL] = 0;

		angles = glm::degrees( angles );
	}
	
	return angles;
}


glm::vec3 Util_VectorToAngles( const glm::vec3& forward, const glm::vec3& up )
{
	glm::vec3 angles;

	if (forward.x == 0 && forward.y == 0)
	{
		// either vertically up or down
		if (forward[2] > 0)
		{
			angles[PITCH] = -90;
			angles[YAW] = glm::degrees( ::atan2(-up[1], -up[0]) );
		}
		else
		{
			angles[PITCH] = 90;
			angles[YAW] = glm::degrees( atan2(up[1], up[0]) );
		}
		angles[ROLL] = 0;
	}
	else
	{
		angles[PITCH] = -atan2(forward.z, sqrt( glm::dot(forward, forward) ));
		angles[YAW] = atan2(forward.y, forward.x);

		float cp = cos(angles[PITCH]), sp = sin(angles[PITCH]);
		float cy = cos(angles[YAW]), sy = sin(angles[YAW]);
		glm::vec3 tleft, tup;
		tleft.x = -sy;
		tleft.y = cy;
		tleft.z = 0;
		tup.x = sp*cy;
		tup.y = sp*sy;
		tup.z = cp;
		angles[ROLL] = -atan2( glm::dot(up, tleft), glm::dot(up, tup) );

		angles = glm::degrees( angles );
	}
	
	return angles;
}


extern ConVar cl_view_height;

CONVAR( proto_look, 1 );
CONVAR( proto_follow, 0 );
CONVAR( proto_follow_speed, 400 );


struct ProtoLookData_t
{
	std::vector< Entity > aProtos;
	Transform* aPlayerTransform;
};


#if 0
void TaskUpdateProtoLook( ftl::TaskScheduler *taskScheduler, void *arg )
{
	float protoScale = vrcmdl_scale;

	ProtoLookData_t* lookData = (ProtoLookData_t*)arg;
	
	// TODO: maybe make this into some kind of "look at player" component? idk lol
	// also could thread this as a test
	for ( auto& proto : lookData->aProtos )
	{
		DefaultRenderable* renderable = (DefaultRenderable*)entities->GetComponent< RenderableHandle_t >( proto );
		auto& protoTransform = entities->GetComponent< Transform >( proto );

		bool matrixChanged = false;

		if ( proto_look.GetBool() )
		{
			matrixChanged = true;

			glm::vec3 forward{}, right{}, up{};
			//AngleToVectors( protoTransform.aAng, forward, right, up );
			AngleToVectors( lookData->aPlayerTransform->aAng, forward, right, up );

			glm::vec3 protoView = protoTransform.aPos;
			//protoView.z += cl_view_height;

			glm::vec3 direction = (protoView - lookData->aPlayerTransform->aPos);
			// glm::vec3 rotationAxis = VectorToAngles( direction );
			glm::vec3 rotationAxis = VectorToAngles( direction, up );

			protoTransform.aAng = rotationAxis;
			protoTransform.aAng[PITCH] = 0.f;
			protoTransform.aAng[YAW] -= 90.f;
			protoTransform.aAng[ROLL] = (-rotationAxis[PITCH]) + 90.f;
			//protoTransform.aAng[ROLL] = 90.f;
		}

		if ( protoTransform.aScale.x != protoScale )
		{
			// protoTransform.aScale = {protoScale, protoScale, protoScale};
			protoTransform.aScale = glm::vec3( protoScale );
			matrixChanged = true;
		}

		if ( matrixChanged )
			renderable->aMatrix = protoTransform.ToMatrix();
	}
}
#endif


CONVAR( r_proto_line_dist, 32.f );
CONVAR( r_proto_line_dist2, 32.f );


// will be used in the future for when updating bones and stuff
void Game_SetupModels( float frameTime )
{
	auto& playerTransform = entities->GetComponent< Transform >( gLocalPlayer );
	auto& camTransform = entities->GetComponent< CCamera >( gLocalPlayer ).aTransform;

	if ( !gPaused )
	{
		if ( input->KeyJustPressed( SDL_SCANCODE_E ) || input->KeyPressed( SDL_SCANCODE_R ) )
		{
			CreateProtogen( DEFAULT_PROTOGEN_PATH );
		}
	}

	//transform.aPos += camTransform.aPos;
	//transform.aAng += camTransform.aAng;

	// ?????
	float protoScale = vrcmdl_scale;

	// TODO: maybe make this into some kind of "look at player" component? idk lol
	// also could thread this as a test
#if 1
	for ( auto& proto: g_protos )
	{
		ModelDraw_t& modelDraw      = entities->GetComponent< ModelDraw_t >( proto );
		auto&        protoTransform = entities->GetComponent< Transform >( proto );

		bool         matrixChanged  = false;

		// TESTING: BROKEN
		if ( proto_look == 2.f )
		{
			glm::vec3 forward, right, up;
			//AngleToVectors( protoTransform.aAng, forward, right, up );
			// Util_GetDirectionVectors( playerTransform.aAng, &forward, &right, &up );
			Util_GetMatrixDirection( playerTransform.ToMatrix( false ), &forward, &right, &up );

			// Graphics_DrawLine( protoTransform.aPos, protoTransform.aPos + ( forward * r_proto_line_dist2.GetFloat() ), { 1.f, 0.f, 0.f } );
			// Graphics_DrawLine( protoTransform.aPos, protoTransform.aPos + ( right * r_proto_line_dist2.GetFloat() ), { 0.f, 1.f, 0.f } );
			// Graphics_DrawLine( protoTransform.aPos, protoTransform.aPos + ( up * r_proto_line_dist2.GetFloat() ), { 0.f, 0.f, 1.f } );

			glm::vec3 protoView    = protoTransform.aPos;
			//protoView.z += cl_view_height;

			glm::vec3 direction    = ( protoView - playerTransform.aPos );
			// glm::vec3 rotationAxis = Util_VectorToAngles( direction );
			glm::vec3 rotationAxis = Util_VectorToAngles( direction, up );

			glm::mat4 protoViewMat = glm::lookAt( protoView, playerTransform.aPos, up );

			glm::vec3 vForward, vRight, vUp;
			Util_GetViewMatrixZDirection( protoViewMat, vForward, vRight, vUp );

			glm::quat protoQuat   = protoViewMat;

			glm::mat4 modelMatrix = glm::translate( protoTransform.aPos );

			modelMatrix           = glm::scale( modelMatrix, glm::vec3( protoScale ) );

			modelMatrix *= glm::toMat4( protoQuat );

			modelDraw.aModelMatrix = modelMatrix;
		}
		else if ( proto_look.GetBool() )
		{
			matrixChanged = true;

			glm::vec3 forward, right, up;
			Util_GetDirectionVectors( playerTransform.aAng, &forward, &right, &up );

			glm::vec3 protoView          = protoTransform.aPos;
			//protoView.z += cl_view_height;

			glm::vec3 direction          = ( protoView - playerTransform.aPos );
			glm::vec3 rotationAxis       = Util_VectorToAngles( direction, up );

			protoTransform.aAng          = rotationAxis;
			protoTransform.aAng[ PITCH ] = 0.f;
			protoTransform.aAng[ YAW ] -= 90.f;
			protoTransform.aAng[ ROLL ] = ( -rotationAxis[ PITCH ] ) + 90.f;
		}

		if ( protoTransform.aScale.x != protoScale )
		{
			protoTransform.aScale = glm::vec3( protoScale );
			matrixChanged = true;
		}

		if ( proto_follow.GetBool() && !gPaused )
		{
			matrixChanged = true;

			glm::vec3 modelUp;
			Util_GetMatrixDirection( modelDraw.aModelMatrix, nullptr, nullptr, &modelUp );

			protoTransform.aPos = protoTransform.aPos + ( modelUp * proto_follow_speed.GetFloat() * gFrameTime );
		}

		if ( matrixChanged )
		{
		 	modelDraw.aModelMatrix = protoTransform.ToMatrix();
			Graphics_UpdateModelAABB( &modelDraw );
		}

		// TEMP
		{
			glm::vec3 modelForward, modelRight, modelUp;
			Util_GetMatrixDirection( modelDraw.aModelMatrix, &modelForward, &modelRight, &modelUp );
			Graphics_DrawLine( protoTransform.aPos, protoTransform.aPos + ( modelForward * r_proto_line_dist.GetFloat() ), { 1.f, 0.f, 0.f } );
			Graphics_DrawLine( protoTransform.aPos, protoTransform.aPos + ( modelRight * r_proto_line_dist.GetFloat() ), { 0.f, 1.f, 0.f } );
			Graphics_DrawLine( protoTransform.aPos, protoTransform.aPos + ( modelUp * r_proto_line_dist.GetFloat() ), { 0.f, 0.f, 1.f } );
		}
	}
#endif
}


void Game_ResetInputs(  )
{
}


CONVAR( snd_test_vol, 0.25 );

#if AUDIO_OPENAL
// idk what these values should really be tbh
// will require a lot of fine tuning tbh
CONVAR_CMD( snd_doppler_scale, 0.2 )
{
	audio->SetDopplerScale( snd_doppler_scale );
}

CONVAR_CMD( snd_sound_speed, 6000 )
{
	audio->SetSoundSpeed( snd_sound_speed );
}
#endif


void Game_UpdateAudio(  )
{
	if ( gPaused )
		return;

	auto& transform = entities->GetComponent< Transform >( gLocalPlayer );

	if ( input->KeyJustPressed( SDL_SCANCODE_H ) )
	{
		for ( Handle stream: streams )
		{
			if ( audio->IsValid( stream ) )
			{
				audio->FreeSound( stream );
			}
		}

		streams.clear();
	}

	if ( input->KeyJustPressed( SDL_SCANCODE_G ) )
	{
		Handle stream = streams.emplace_back( audio->LoadSound( "sound/endymion_mono.ogg" ) );

		/*if ( audio->IsValid( stream ) )
		{
			audio->FreeSound( stream );
		}*/
		// test sound
		//else if ( stream = audio->LoadSound("sound/rain2.ogg") )  
		if ( stream )  
		//else if ( stream = audio->LoadSound("sound/endymion_mono.ogg") )  
		//else if ( stream = audio->LoadSound("sound/endymion2.wav") )  
		//else if ( stream = audio->LoadSound("sound/endymion_mono.wav") )  
		//else if ( stream = audio->LoadSound("sound/robots_cropped.ogg") )  
		{
			audio->SetVolume( stream, snd_test_vol );

#if AUDIO_OPENAL
			audio->AddEffect( stream, AudioEffect_Loop );  // on by default
#endif

			// play it where the player currently is
			// audio->AddEffect( stream, AudioEffect_World );
			// audio->SetEffectData( stream, Audio_World_Pos, transform.aPos );

			audio->PlaySound( stream );

			/*if ( g_streamModel == nullptr )
			{
				g_streamModel = graphics->LoadModel( "materials/models/cube.obj" );
				//aModels.push_back( g_streamModel );
			}

			g_streamModel->SetPos( transform.aPos );*/
		}
	}

	for ( Handle stream: streams )
	{
		if ( audio->IsValid( stream ) )
		{
			audio->SetVolume( stream, snd_test_vol );
		}
	}
}


void Game_HandleSystemEvents()
{
	static std::vector< SDL_Event >* events = input->GetEvents();

	for ( auto& e: *events )
	{
		switch (e.type)
		{
			case SDL_WINDOWEVENT:
			{
				switch (e.window.event)
				{
					case SDL_WINDOWEVENT_SIZE_CHANGED:
					{
						// Log_Msg( "SDL_WINDOWEVENT_SIZE_CHANGED\n" );
						Game_UpdateProjection();
						Graphics_Reset();
						break;
					}
					case SDL_WINDOWEVENT_EXPOSED:
					{
						// Log_Msg( "SDL_WINDOWEVENT_EXPOSED\n" );
						// TODO: RESET RENDERER AND DRAW ONTO WINDOW WINDOW !!!
						break;
					}
				}
				break;
			}

			default:
			{
				break;
			}
		}
	}
}


void Game_SetView( const glm::mat4& srViewMat )
{
	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	gView.aViewMat = srViewMat;
	gView.ComputeProjection( width, height );

	// um
	gViewInfo[ 0 ].aProjection = gView.aProjMat;
	gViewInfo[ 0 ].aView       = gView.aViewMat;
	gViewInfo[ 0 ].aNearZ      = gView.aNearZ;
	gViewInfo[ 0 ].aFarZ       = gView.aFarZ;

	Graphics_SetViewProjMatrix( gView.aProjViewMat );
	gViewInfoUpdate  = true;

	auto& io         = ImGui::GetIO();
	io.DisplaySize.x = width;
	io.DisplaySize.y = height;
}


void Game_UpdateProjection()
{
	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );
	gView.ComputeProjection( width, height );

	// um
	gViewInfo[ 0 ].aProjection = gView.aProjMat;
	gViewInfo[ 0 ].aView       = gView.aViewMat;
	gViewInfo[ 0 ].aNearZ      = gView.aNearZ;
	gViewInfo[ 0 ].aFarZ       = gView.aFarZ;

	Graphics_SetViewProjMatrix( gView.aProjViewMat );
	gViewInfoUpdate  = true;

	auto& io = ImGui::GetIO();
	io.DisplaySize.x = width;
	io.DisplaySize.y = height;
}


