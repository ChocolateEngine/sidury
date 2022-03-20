#include "gamesystem.h"
#include "core/systemmanager.h"
#include "util.h"
#include "physics.h"
#include "player.h"
#include "entity.h"
#include "terrain/terrain.h"
#include "graphics/sprite.h"
#include "mapmanager.h"
#include <algorithm>


GameSystem* game = nullptr;

BaseGuiSystem* gui = nullptr;
BaseGraphicsSystem* graphics = nullptr;
BaseInputSystem* input = nullptr;
BaseAudioSystem* audio = nullptr;

IMaterialSystem* materialsystem = nullptr;

#define SPAWN_PROTOGEN 0

void CenterMouseOnScreen(  )
{
	int w, h;
	SDL_GetWindowSize( graphics->GetWindow(), &w, &h );
	SDL_WarpMouseInWindow( graphics->GetWindow(), w/2, h/2 );
}


CONVAR( r_fov, 100.f );
CONVAR( r_nearz, 1.f );
CONVAR( r_farz, 10000.f );

CONVAR( phys_rot_x, 90 );
CONVAR( phys_rot_y, 0 );
CONVAR( phys_rot_z, 0 );
CONVAR( phys_friction, 10 );
CONVAR( dbg_global_axis, 1 );
CONVAR( dbg_global_axis_size, 15 );

CONVAR( vrcmdl_scale, 40 );

extern ConVar en_timescale;

extern ConVar velocity_scale;


struct ModelPhysTest
{
	Model* mdl;
#if BULLET_PHYSICS
	std::vector<PhysicsObject*> physObj;
#else
	int* physObj;
#endif
};


const int physEntCount = 0;
ModelPhysTest* g_world = new ModelPhysTest{new Model, {}};
std::vector< Entity > g_protos;
std::vector< Entity > g_otherEnts;
std::vector< ModelPhysTest* > g_physEnts;


void CreateProtogen()
{
	Entity proto = entities->CreateEntity();

	Model *model = graphics->LoadModel( "materials/models/protogen_wip_25d/protogen_wip_25d.obj" );
	entities->AddComponent< Model* >( proto, model );

	auto transform = entities->GetComponent< Transform >( game->aLocalPlayer );

	model->SetPos( transform.aPos );
	model->SetScale( {vrcmdl_scale, vrcmdl_scale, vrcmdl_scale} );

	g_protos.push_back( proto );
}


void CreatePhysEntity( const std::string& path )
{
	Entity physEnt = entities->CreateEntity();

	Model* model = graphics->LoadModel( path );
	entities->AddComponent< Model* >( physEnt, model );

	Transform transform = entities->GetComponent< Transform >( game->aLocalPlayer );
	transform.aAng = {};
	transform.aScale = {1, 1, 1};

	PhysicsObjectInfo physInfo( ShapeType::Convex );
	physInfo.vertices = model->aVertices;
	physInfo.indices = model->aIndices;
	physInfo.mass = 40.f;
	// physInfo.collisionType = CollisionType::Kinematic;
	physInfo.transform = transform;  // doesn't even work? bruh

	PhysicsObject* phys = physenv->CreatePhysicsObject( physInfo );
	phys->SetWorldTransform( transform );
	phys->SetAlwaysActive( true );
	phys->SetContinuousCollisionEnabled( true );
	phys->SetSleepingThresholds( 0, 0 );
	phys->SetFriction( phys_friction );

	entities->AddComponent< PhysicsObject * >( physEnt, phys );

	model->SetPos( transform.aPos );

	g_otherEnts.push_back( physEnt );
}


CON_COMMAND( create_proto )
{
	CreateProtogen();
}

CON_COMMAND( create_phys_test )
{
	CreatePhysEntity( "materials/models/riverhouse/riverhouse.obj" );
}

CON_COMMAND( create_phys_proto )
{
	CreatePhysEntity( "materials/models/protogen_wip_25d/protogen_wip_25d_big.obj" );
}

CON_COMMAND( delete_protos )
{
	for ( auto& proto : g_protos )
	{
		graphics->FreeModel( entities->GetComponent< Model* >( proto ) );
		entities->DeleteEntity( proto );
	}
	g_protos.clear();
}


GameSystem::GameSystem(  ):
	aPaused( true ),
	aFrameTime(0.f),
	// only large near and farz for riverhouse and quake movement
	// aView( 0, 0, 200, 200, 1, 10000, 90 )
	aView( 0, 0, 200, 200, r_nearz, r_farz, r_fov )
{
	game = this;
}


GameSystem::~GameSystem(  )
{
#if BULLET_PHYSICS
	if ( physenv )
		delete physenv;
#endif

	//if ( aLocalPlayer )
	//	delete aLocalPlayer;

	for ( int i = 0; i < physEntCount; i++ )
	{
		if ( g_physEnts[i] )
			delete g_physEnts[i];

		g_physEnts[i] = nullptr;
	}

	/*for ( int i = 0; i < aModels.size(); i++ )
	{
		if ( aModels[i] )
			delete aModels[i];
	}

	aModels.clear();*/

	//if ( g_proto )
	//	delete g_proto;
}


void GameSystem::Init(  )
{
	LoadModules(  );
	RegisterKeys(  );

	srand( ( unsigned int )time( 0 ) );  // setup rand(  )

	entities = new EntityManager;
	entities->Init();

	// voxelworld->Init(  );

	players = entities->RegisterSystem<PlayerManager>();
	players->Init();

	aLocalPlayer = players->Create();

	// mark this as the local player
	auto& playerInfo = entities->GetComponent< CPlayerInfo >( aLocalPlayer );
	playerInfo.aIsLocalPlayer = true;

	players->Spawn( aLocalPlayer );

	LogMsg( "Game Loaded!\n" );
}


void GameSystem::RegisterKeys(  )
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

	input->RegisterKey( SDL_SCANCODE_G ); // play a test sound at current position in world
	input->RegisterKey( SDL_SCANCODE_H ); // stop all test sounds

	input->RegisterKey( SDL_SCANCODE_E ); // create protogen
	input->RegisterKey( SDL_SCANCODE_R ); // create protogen hold down key

	//input->RegisterKey( SDL_SCANCODE_G ); // create a sprite
}


void GameSystem::LoadModules(  )
{
	GET_SYSTEM_CHECK( gui, BaseGuiSystem );
	GET_SYSTEM_CHECK( graphics, BaseGraphicsSystem );
	GET_SYSTEM_CHECK( input, BaseInputSystem );
	GET_SYSTEM_CHECK( audio, BaseAudioSystem );

	graphics->GetWindowSize( &aView.width, &aView.height );
	aView.ComputeProjection();

#if BULLET_PHYSICS
	physenv = new PhysicsEnvironment;
	physenv->Init(  );
#endif

	// stupid
	materialsystem = graphics->GetMaterialSystem();

	mapmanager = new MapManager;
}


bool GameSystem::InMap()
{
	return mapmanager->apMap != nullptr;
}


// testing
std::vector<Handle> streams {};
Model* g_streamModel = nullptr;


ConVar snd_cube_scale("snd_cube_scale", "0.05");
extern ConVar velocity_scale;


// TODO: figure out a better way to do this, god
void GameSystem::Update( float frameTime )
{
	input->Update( frameTime );
	gui->StartFrame();

	GameUpdate( frameTime );

	gui->Update( frameTime );
	graphics->Update( frameTime );  // updates gui internally
}


void GameSystem::GameUpdate( float frameTime )
{
	// move to engine?
	aFrameTime = frameTime * en_timescale;

	// scale the nearz and farz
	aView.Set( 0, 0, aView.width, aView.height, r_nearz * velocity_scale, r_farz * velocity_scale, r_fov );

	HandleSystemEvents();

	CheckPaused(  );

	if ( aPaused )
	{
		//ResetInputs(  );
		//players->Update( 0.f );
		//return;
		aFrameTime = 0.f;
	}

	aCurTime += aFrameTime;

#if 0
	materialsystem->AddRenderable( g_world->mdl );

#if BULLET_PHYSICS

	for ( auto &physObj : g_world->physObj )
	{
		physObj->SetContinuousCollisionEnabled( true );

		// uhhhhh
		Transform worldTransform = g_world->mdl->GetTransform();
		worldTransform.aAng = {phys_rot_x, phys_rot_y, phys_rot_z};
		worldTransform.aAng = glm::radians( worldTransform.aAng );

		// glm::vec3 ang = glm::radians( g_world->mdl->GetModelData().GetTransform().aAng );
		// worldTransform.aAng = {ang.z, ang.y, ang.x};

		physObj->SetWorldTransform( worldTransform );
		//physObj->SetAngularFactor( {0, 0, 0} );
	}
#endif
#endif

	mapmanager->Update();

	// WORLD GLOBAL AXIS
	if ( dbg_global_axis )
	{
		graphics->DrawLine( {0, 0, 0}, {dbg_global_axis_size, 0, 0}, {1, 0, 0} );
		graphics->DrawLine( {0, 0, 0}, {0, dbg_global_axis_size, 0}, {0, 1, 0} );
		graphics->DrawLine( {0, 0, 0}, {0, 0, dbg_global_axis_size}, {0, 0, 1} );
	}

	//voxelworld->Update( frameTime );
	players->Update( aFrameTime );

#if BULLET_PHYSICS
	physenv->Simulate(  );

	// blech
	for ( auto &ent : g_otherEnts )
	{
		Model* model = entities->GetComponent< Model* >( ent );

		if ( !model )
			continue;

		// Model *physObjList = &entities->GetComponent< Model >( ent );

		// Transform& transform = entities->GetComponent< Transform >( ent );
		Transform &transform = model->GetTransform();
		PhysicsObject* phys = entities->GetComponent< PhysicsObject* >( ent );

		if ( phys )
		{
			phys->SetFriction( phys_friction );

			transform.aPos = phys->GetWorldTransform().aPos;
			transform.aAng = phys->GetWorldTransform().aAng;

			model->SetTransform( transform );

			gui->DebugMessage( 13, "Phys Obj Ang: %s", Vec2Str( transform.aAng ).c_str() );
		}

		materialsystem->AddRenderable( model );
	}

	// stupid
	for (auto& player: players->aPlayerList)
	{
		players->apMove->UpdatePosition( player );
	}
#endif

	players->apMove->DisplayPlayerStats( aLocalPlayer );

	SetupModels( frameTime );

	ResetInputs(  );

	UpdateAudio(  );

	if ( input->WindowHasFocus() && !aPaused )
	{
		CenterMouseOnScreen(  );
	}
}


void GameSystem::CheckPaused(  )
{
	bool wasPaused = aPaused;
	aPaused = gui->IsConsoleShown();

	if ( wasPaused != aPaused )
	{
		SDL_SetRelativeMouseMode( (SDL_bool)!aPaused );

		if ( aPaused )
		{
			CenterMouseOnScreen(  );
		}
	}

	audio->SetPaused( aPaused );
}

CONVAR( proto_x,  550 );
CONVAR( proto_y,  240 );
CONVAR( proto_z, -360 );

CONVAR( proto_spin_p, 0 );
CONVAR( proto_spin_y, 90 );
CONVAR( proto_spin_r, 0 );

CONVAR( proto_look, 1 );


// from vkquake
glm::vec3 VectorToAngles( const glm::vec3& forward )
{
	glm::vec3 angles;

	if (forward.x == 0 && forward.y == 0)
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


glm::vec3 VectorToAngles( const glm::vec3& forward, const glm::vec3& up )
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


// more vkquake stuff
void AngleToVectors( const glm::vec3& angles, glm::vec3& forward, glm::vec3& right, glm::vec3& up )
{
	float angle;

	angle = glm::radians( angles[YAW] );
	float sy = sin(angle);
	float cy = cos(angle);

	angle = glm::radians( angles[PITCH] );
	float sp = sin(angle);
	float cp = cos(angle);

	angle = glm::radians( angles[ROLL] );
	float sr = sin(angle);
	float cr = cos(angle);

	forward[0] = cp*cy;
	forward[1] = cp*sy;
	forward[2] = -sp;
	right[0] = (-1*sr*sp*cy+-1*cr*-sy);
	right[1] = (-1*sr*sp*sy+-1*cr*cy);
	right[2] = -1*sr*cp;
	up[0] = (cr*sp*cy+-sr*-sy);
	up[1] = (cr*sp*sy+-sr*cy);
	up[2] = cr*cp;
}


extern ConVar cl_view_height;


// will be used in the future for when updating bones and stuff
void GameSystem::SetupModels( float frameTime )
{
#if 0
	if ( g_proto && g_proto->mdl )
	{
		/*g_proto->mdl->GetModelData().aTransform.aPos = {
			proto_x * velocity_scale,
			proto_y * velocity_scale,
			proto_z * velocity_scale
		};*/
		
		g_proto->mdl->GetModelData().aTransform.aAng[PITCH] += proto_spin_p * aFrameTime;
		g_proto->mdl->GetModelData().aTransform.aAng[YAW]   += proto_spin_y * aFrameTime;
		g_proto->mdl->GetModelData().aTransform.aAng[ROLL]  += proto_spin_r * aFrameTime;
	}
#endif

	if ( !aPaused )
	{
		if ( input->KeyJustPressed( SDL_SCANCODE_E ) || input->KeyPressed( SDL_SCANCODE_R ) )
		{
			CreateProtogen(  );
		}
	}

	auto& playerTransform = entities->GetComponent< Transform >( game->aLocalPlayer );
	auto& camTransform = entities->GetComponent< CCamera >( game->aLocalPlayer ).aTransform;

	Transform transform = playerTransform;
	//transform.aPos += camTransform.aPos;
	//transform.aAng += camTransform.aAng;

	for ( auto& proto: g_protos )
	{
		auto model = entities->GetComponent< Model* >( proto );
		Transform& protoTransform = model->GetTransform();

		if ( proto_look.GetBool() )
		{
			glm::vec3 forward{}, right{}, up{};
			//AngleToVectors( protoTransform.aAng, forward, right, up );
			AngleToVectors( playerTransform.aAng, forward, right, up );

			glm::vec3 protoView = protoTransform.aPos;
			//protoView.z += cl_view_height;

			glm::vec3 direction = (protoView - transform.aPos);
			// glm::vec3 rotationAxis = VectorToAngles( direction );
			glm::vec3 rotationAxis = VectorToAngles( direction, up );

			protoTransform.aAng = rotationAxis;
			protoTransform.aAng[PITCH] = 0.f;
			protoTransform.aAng[YAW] -= 90.f;
			protoTransform.aAng[ROLL] = (-rotationAxis[PITCH]) + 90.f;
			//protoTransform.aAng[ROLL] = 90.f;
		}

		protoTransform.aScale = {vrcmdl_scale, vrcmdl_scale, vrcmdl_scale};
		// model.SetTransform( protoTransform );

		materialsystem->AddRenderable( model );
	}
	
	// scale the world
	g_world->mdl->SetScale( glm::vec3(1.f) * velocity_scale.GetFloat() );

	if ( g_streamModel )
	{
		g_streamModel->SetScale( {snd_cube_scale, snd_cube_scale, snd_cube_scale} );
	}
}


void GameSystem::ResetInputs(  )
{
}


CONVAR( snd_test_vol, 0.25 );


void GameSystem::UpdateAudio(  )
{
	if ( aPaused )
		return;

	auto& transform = entities->GetComponent< Transform >( aLocalPlayer );

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
		Handle stream = streams.emplace_back( audio->LoadSound( "sound/endymion2.ogg" ) );

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
			audio->SetWorldPos( stream, transform.aPos );  // play it where the player currently is
			audio->SetLoop( stream, true );
			audio->SetEffects( stream, AudioEffect_World );

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

	audio->SetListenerTransform( transform.aPos, transform.aAng );
}


void GameSystem::HandleSystemEvents()
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
						graphics->GetWindowSize( &aView.width, &aView.height );
						aView.ComputeProjection();
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


void GameSystem::SetViewMatrix( const glm::mat4& viewMatrix )
{
	aView.viewMatrix = viewMatrix;
	graphics->SetView( aView );
}

