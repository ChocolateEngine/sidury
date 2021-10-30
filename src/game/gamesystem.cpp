#include "gamesystem.h"
#include "core/systemmanager.h"
#include "util.h"
#include "player.h"
#include "entity.h"
#include "terrain/terrain.h"
#include <algorithm>


GameSystem* game = nullptr;

// bruh
IMaterialSystem* materialsystem = nullptr;

#define SPAWN_PROTOGEN 0

void CenterMouseOnScreen(  )
{
	int w, h;
	SDL_GetWindowSize( game->apGraphics->GetWindow(), &w, &h );
	SDL_WarpMouseInWindow( game->apGraphics->GetWindow(), w/2, h/2 );
}


CONVAR( r_fov, 100.f );
CONVAR( r_nearz, 1.f );
CONVAR( r_farz, 10000.f );

CONVARREF( en_timescale );

extern ConVar velocity_scale;


struct ModelPhysTest
{
	Model* mdl;
#if !NO_BULLET_PHYSICS
	PhysicsObject* physObj;
#else
	int* physObj;
#endif
};


const int physEntCount = 0;
ModelPhysTest* g_world = new ModelPhysTest{new Model, NULL};
std::vector< Entity > g_protos;
std::vector< ModelPhysTest* > g_physEnts;


void CreateProtogen()
{
	Entity proto = entities->CreateEntity();
	Model* model = &entities->AddComponent< Model >( proto );

	game->apGraphics->LoadModel( "materials/models/protogen_wip_22/protogen_wip_22.obj", "materials/1aaaaaaa.jpg", model );

	auto& transform = entities->GetComponent< Transform >( game->aLocalPlayer );

	model->GetModelData().SetPos( transform.aPos );

	g_protos.push_back( proto );
}

CON_COMMAND( create_proto )
{
	CreateProtogen();
}


CON_COMMAND( load_world )
{
	if ( args.size() == 0 )
	{
		Print( "No Arguments! Args: \"Model Path\" \"1\" (optional value to rotate the world)\"" );
	}

	bool rotate = args.size() > 2 && args[1] == "1";

	game->LoadWorld( args[0], rotate );
}


// TEMP
CON_COMMAND( load_surf_utopia )
{
	game->LoadWorld( "D:\\tmp\\surf_utopia_decompile\\surf_utopia_v3_d.obj", false );
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
#if !NO_BULLET_PHYSICS
	if ( apPhysEnv )
		delete apPhysEnv;
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
	BaseClass::Init(  );

	LoadModules(  );
	RegisterKeys(  );

	srand( ( unsigned int )time( 0 ) );  // setup rand(  )

	entities = new EntityManager;
	entities->Init();

	// LoadWorld( "materials/models/riverhouse/riverhouse_source_scale.obj", true );
	// LoadWorld( "D:\\tmp\\surf_utopia_decompile\\surf_utopia_v3_d.obj", false );

	// should be part of LoadWorld, but that will come later when we actually have a map format for this game
	CreateEntities(  );

	voxelworld->Init(  );

	players = entities->RegisterSystem<PlayerManager>();
	players->Init();

	aLocalPlayer = players->Create();

	// mark this as the local player
	auto& playerInfo = entities->GetComponent< CPlayerInfo >( aLocalPlayer );
	playerInfo.aIsLocalPlayer = true;

	players->Spawn( aLocalPlayer );

	Print( "Game Loaded!\n" );
}


void GameSystem::RegisterKeys(  )
{
	apInput->RegisterKey( SDL_SCANCODE_W );
	apInput->RegisterKey( SDL_SCANCODE_S );
	apInput->RegisterKey( SDL_SCANCODE_A );
	apInput->RegisterKey( SDL_SCANCODE_D );

	apInput->RegisterKey( SDL_SCANCODE_LCTRL );
	apInput->RegisterKey( SDL_SCANCODE_LSHIFT );
	apInput->RegisterKey( SDL_SCANCODE_SPACE );
	
	apInput->RegisterKey( SDL_SCANCODE_V ); // noclip
	apInput->RegisterKey( SDL_SCANCODE_B ); // flight

	apInput->RegisterKey( SDL_SCANCODE_G ); // play test sound at current position in world
	apInput->RegisterKey( SDL_SCANCODE_E ); // create protogen
	apInput->RegisterKey( SDL_SCANCODE_R ); // create protogen hold down key
}


void GameSystem::LoadModules(  )
{
	GET_SYSTEM_CHECK( apGui, BaseGuiSystem );
	GET_SYSTEM_CHECK( apGraphics, BaseGraphicsSystem );
	GET_SYSTEM_CHECK( apInput, BaseInputSystem );
	GET_SYSTEM_CHECK( apAudio, BaseAudioSystem );

	apGraphics->GetWindowSize( &aView.width, &aView.height );
	aView.ComputeProjection();

#if !NO_BULLET_PHYSICS
	apPhysEnv = new PhysicsEnvironment;
	apPhysEnv->Init(  );
#endif

	materialsystem = apGraphics->GetMaterialSystem();
}


void GameSystem::UnloadWorld()
{
	apGraphics->UnloadModel( g_world->mdl );
	//vec_remove( aModels, g_world->mdl );
	//g_world->mdl = nullptr;
}


void GameSystem::LoadWorld( const std::string& path, bool rotate )
{
	//if ( g_world->mdl )
	//	UnloadWorld();

	apGraphics->LoadModel( path, "materials/act_like_a_baka.jpg", g_world->mdl );

	// apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", "materials/act_like_a_baka.jpg", g_world->mdl );
	//apGraphics->LoadModel( "materials/models/riverhouse/riverhouse_source_scale.obj", "materials/act_like_a_baka.jpg", g_world->mdl );
	//apGraphics->LoadModel( "D:\\tmp\\surf_utopia_decompile\\surf_utopia_v3_d.obj", "materials/act_like_a_baka.jpg", g_world->mdl );
	//apGraphics->LoadModel( "D:/usr/Downloads/surf_kitsune2_go/br/d1_trainstation_02.obj", "materials/act_like_a_baka.jpg", g_world->mdl );
	//apGraphics->LoadModel( "materials/models/riverhouse/riverhouse_bsp_export.obj", "materials/act_like_a_baka.jpg", g_world->mdl );
	//g_world->mdl->GetModelData().aTransform.aScale = {0.025, 0.025, 0.025};

	// rotate the world model to match Z up if we want to rotate it
	g_world->mdl->GetModelData().SetAng( {0, 0, rotate ? 90.f : 0.f} );

	//aModels.push_back( g_world->mdl );

#if 0 // !NO_BULLET_PHYSICS
	PhysicsObjectInfo physInfo( ShapeType::Concave );
	physInfo.modelData = &g_world->mdl->GetModelData();

	// just have the ground be a box for now since collision on the riverhouse mesh is too jank still
	//PhysicsObjectInfo physInfo( ShapeType::Box );
	//physInfo.bounds = {1500, 200, 1500};

	g_world->physObj = apPhysEnv->CreatePhysicsObject( physInfo );
	g_world->physObj->SetContinuousCollisionEnabled( true );

	// uhhhhh
	Transform worldTransform = g_world->mdl->GetModelData().aTransform;
	worldTransform.aAng = glm::degrees(g_world->mdl->GetModelData().aTransform.aAng);

	g_world->physObj->SetWorldTransform( worldTransform );
	//g_world->physObj->SetAngularFactor( {0, 0, 0} );
#endif
}


void GameSystem::CreateEntities(  )
{
#if 0  // outdated stuff

#if !NO_BULLET_PHYSICS
	PhysicsObjectInfo physInfo( ShapeType::Convex );
	physInfo.collisionType = CollisionType::Kinematic;
	physInfo.mass = 20.f;
#endif

	// Create a ton of phys objects to test physics
	float r = 0.f;
	for ( int i = 0; i < physEntCount; i++ )
	{
		g_physEnts.push_back( new ModelPhysTest{ new Model, NULL } );
		aModels.push_back( g_physEnts[i]->mdl );
		apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", "materials/1aaaaaaa.jpg", g_physEnts[i]->mdl );
		// apGraphics->LoadModel( "materials/models/protogen_wip_22/protogen_wip_22.obj", "materials/1aaaaaaa.jpg", g_physEnts[i]->mdl );

		for (int j = 0; j < 3; j++)
		{
			// set a random spawn position in the world
			//g_physEnts[i]->mdl->GetModelData().aTransform.position[j] = ( float )( rand(  ) / ( float )( RAND_MAX / 10.0f ) );
		}

		// raise it up in the air
		// g_physEnts[i]->mdl->GetModelData().aTransform.position.y += 500.f;
		// g_physEnts[i]->mdl->GetModelData().aTransform.position.y += 5.f;
		//g_physEnts[i]->mdl->GetModelData().aTransform.aPos.x = 5.f;

#if !NO_BULLET_PHYSICS
		physInfo.transform.aPos = g_physEnts[i]->mdl->GetModelData().aTransform.aPos;

		//physInfo.modelData = &g_world->mdl->GetModelData();

		//g_physEnts[i]->physObj = apPhysEnv->CreatePhysicsObject( physInfo );
		//g_physEnts[i]->physObj->SetAlwaysActive( true );
#endif
	}

#if SPAWN_PROTOGEN
	CreateProtogen();
#endif

#endif
}


void GameSystem::InitConsoleCommands(  )
{
	BaseClass::InitConsoleCommands(  );
}


// testing
AudioStream *stream = nullptr;
Model* g_streamModel = nullptr;


ConVar snd_cube_scale("snd_cube_scale", "0.05");
extern ConVar velocity_scale;


void GameSystem::Update( float frameTime )
{
	BaseClass::Update( frameTime );

	// move to engine?
	aFrameTime = frameTime * en_timescale;

	// scale the nearz and farz
	aView.Set( 0, 0, aView.width, aView.height, r_nearz * velocity_scale, r_farz * velocity_scale, r_fov );

	CheckPaused(  );

	if ( aPaused )
	{
		//ResetInputs(  );
		//players->Update( 0.f );
		//return;
		aFrameTime = 0.f;
	}

	aCurTime += aFrameTime;

	voxelworld->Update( frameTime );
	players->Update( aFrameTime );

#if !NO_BULLET_PHYSICS
	apPhysEnv->Simulate(  );

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

	if ( apInput->WindowHasFocus() && !aPaused )
	{
		CenterMouseOnScreen(  );
	}
}


void GameSystem::CheckPaused(  )
{
	bool wasPaused = aPaused;
	aPaused = apGui->IsConsoleShown();

	if ( wasPaused != aPaused )
	{
		SDL_SetRelativeMouseMode( (SDL_bool)!aPaused );

		if ( aPaused )
		{
			CenterMouseOnScreen(  );
		}
	}

	apAudio->SetPaused( aPaused );
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

	if (forward[0] == 0 && forward[1] == 0)
	{
		// either vertically up or down
		angles[PITCH] = (forward[2] > 0) ? -90 : 90;
		angles[YAW] = 0;
		angles[ROLL] = 0;
	}
	else
	{
		angles[PITCH] = -atan2(forward[2], sqrt( glm::dot(forward, forward) ));
		angles[YAW] = atan2(forward[1], forward[0]);
		angles[ROLL] = 0;

		angles = glm::degrees( angles );
	}
	
	return angles;
}


glm::vec3 VectorToAngles( const glm::vec3& forward, const glm::vec3& up )
{
	glm::vec3 angles;

	if (forward[0] == 0 && forward[1] == 0)
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
		angles[PITCH] = -atan2(forward[2], sqrt( glm::dot(forward, forward) ));
		angles[YAW] = atan2(forward[1], forward[0]);

		float cp = cos(angles[PITCH]), sp = sin(angles[PITCH]);
		float cy = cos(angles[YAW]), sy = sin(angles[YAW]);
		glm::vec3 tleft, tup;
		tleft[0] = -sy;
		tleft[1] = cy;
		tleft[2] = 0;
		tup[0] = sp*cy;
		tup[1] = sp*sy;
		tup[2] = cp;
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
		if ( apInput->KeyJustPressed( SDL_SCANCODE_E ) || apInput->KeyPressed( SDL_SCANCODE_R ) )
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
		auto& model = entities->GetComponent< Model >( proto );
		Transform& protoTransform = model.GetModelData().GetTransform();

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

		for ( auto& mesh: model.GetModelData().aMeshes )
		{
			mesh->aTransform = protoTransform;
			materialsystem->AddRenderable( mesh );
		}
	}
	
	// scale the world
	g_world->mdl->GetModelData().SetScale( glm::vec3(1.f) * velocity_scale.GetFloat() );

	if ( g_streamModel )
	{
		g_streamModel->GetModelData().SetScale( {snd_cube_scale, snd_cube_scale, snd_cube_scale} );
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

	if ( apInput->KeyJustPressed(SDL_SCANCODE_G) )
	{
		if ( stream && stream->Valid() )
		{
			apAudio->FreeSound( &stream );

			if ( g_streamModel )
				g_streamModel->GetModelData().aNoDraw = true;
		}
		// test sound
		//else if ( apAudio->LoadSound("sound/rain2.ogg", &stream) )  
		else if ( apAudio->LoadSound("sound/endymion2.ogg", &stream) )  
		//else if ( apAudio->LoadSound("sound/endymion_mono.ogg", &stream) )  
		//else if ( apAudio->LoadSound("sound/endymion2.wav", &stream) )  
		//else if ( apAudio->LoadSound("sound/endymion_mono.wav", &stream) )  
		//else if ( apAudio->LoadSound("sound/robots_cropped.ogg", &stream) )  
		{
			stream->vol = snd_test_vol;
			stream->pos = transform.aPos;  // play it where the player currently is
			//stream->effects = AudioEffectPreset_World;
			stream->loop = true;

			apAudio->PlaySound( stream );

			if ( g_streamModel == nullptr )
			{
				g_streamModel = new Model;
				apGraphics->LoadModel( "materials/models/cube.obj", "", g_streamModel );
				//aModels.push_back( g_streamModel );
			}

			g_streamModel->GetModelData().aNoDraw = false;  // !stream->inWorld
			g_streamModel->GetModelData().SetPos( transform.aPos );
		}
	}

	if ( stream && stream->Valid() )
	{
		stream->vol = snd_test_vol;
	}

	apAudio->SetListenerTransform( transform.aPos, transform.aAng );
}


void GameSystem::HandleSDLEvent( SDL_Event* e )
{
	switch (e->type)
	{
		case SDL_WINDOWEVENT:
		{
			switch (e->window.event)
			{
				case SDL_WINDOWEVENT_SIZE_CHANGED:
				{
					apGraphics->GetWindowSize( &aView.width, &aView.height );
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


void GameSystem::SetViewMatrix( const glm::mat4& viewMatrix )
{
	aView.viewMatrix = viewMatrix;
	apGraphics->SetView( aView );
}

