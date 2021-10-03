#include "gamesystem.h"
//#include "../../chocolate/inc/core/engine.h"
#include "../../chocolate/inc/shared/systemmanager.h"
#include "../../chocolate/inc/shared/util.h"
#include "player.h"
#include <algorithm>


GameSystem* game = NULL;

#define SPAWN_PROTOGEN 0

void CenterMouseOnScreen(  )
{
	int w, h;
	SDL_GetWindowSize( game->apGraphics->GetWindow(), &w, &h );
	SDL_WarpMouseInWindow( game->apGraphics->GetWindow(), w/2, h/2 );
}


ConVar cl_fov( "cl_fov", 100 );
ConVar sv_timescale( "sv_timescale", 1 );


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
ModelPhysTest* g_riverhouse = new ModelPhysTest{new Model, NULL};
ModelPhysTest* g_proto = nullptr;
std::vector< ModelPhysTest* > g_physEnts;


void CreateProtogen()
{
	if ( g_proto )
		return;

	g_proto = new ModelPhysTest{ new Model, NULL };
	game->apGraphics->LoadModel( "materials/models/protogen_wip_22/protogen_wip_22.obj", "materials/1aaaaaaa.jpg", g_proto->mdl );
	game->aModels.push_back( g_proto->mdl );
}

CON_COMMAND( create_proto )
{
	CreateProtogen();
}


GameSystem::GameSystem(  ):
	aPaused( true ),
	aFrameTime(0.f),
	// only large near and farz for riverhouse and quake movement
	// aView( 0, 0, 200, 200, 1, 10000, 90 )
	aView( 0, 0, 200, 200, 0.01, 200, cl_fov )
{
	game = this;
}


GameSystem::~GameSystem(  )
{
#if !NO_BULLET_PHYSICS
	if ( apPhysEnv )
		delete apPhysEnv;
#endif

	if ( aLocalPlayer )
		delete aLocalPlayer;

	for ( int i = 0; i < physEntCount; i++ )
	{
		if ( g_physEnts[i] )
			delete g_physEnts[i];

		g_physEnts[i] = nullptr;
	}

	if ( g_proto )
		delete g_proto;
}


void GameSystem::Init(  )
{
	BaseClass::Init(  );

	LoadModules(  );
	RegisterKeys(  );

	srand( ( unsigned int )time( 0 ) );  // setup rand(  )

	LoadWorld(  );

	// create a player
	aLocalPlayer = new Player;
	aLocalPlayer->Spawn();
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
}


void GameSystem::LoadWorld(  )
{
	// apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", "materials/act_like_a_baka.jpg", g_riverhouse->mdl );
	apGraphics->LoadModel( "materials/models/riverhouse/riverhouse_source_scale.obj", "materials/act_like_a_baka.jpg", g_riverhouse->mdl );
	g_riverhouse->mdl->GetModelData().aTransform.scale = {0.025, 0.025, 0.025};
	aModels.push_back( g_riverhouse->mdl );

#if !NO_BULLET_PHYSICS
	PhysicsObjectInfo physInfo( ShapeType::Concave );
	physInfo.modelData = &g_riverhouse->mdl->GetModelData();

	// just have the ground be a box for now since collision on the riverhouse mesh is too jank still
	//PhysicsObjectInfo physInfo( ShapeType::Box );
	//physInfo.bounds = {1500, 200, 1500};

	g_riverhouse->physObj = apPhysEnv->CreatePhysicsObject( physInfo );
	g_riverhouse->physObj->SetContinuousCollisionEnabled( true );
#endif

	CreateEntities(  );
}


void GameSystem::CreateEntities(  )
{
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
		g_physEnts[i]->mdl->GetModelData().aTransform.position.x = 5.f;

#if !NO_BULLET_PHYSICS
		physInfo.transform.position = g_physEnts[i]->mdl->GetModelData().aTransform.position;

		//physInfo.modelData = &g_riverhouse->mdl->GetModelData();

		//g_physEnts[i]->physObj = apPhysEnv->CreatePhysicsObject( physInfo );
		//g_physEnts[i]->physObj->SetAlwaysActive( true );
#endif
	}

#if SPAWN_PROTOGEN
	CreateProtogen();
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


void GameSystem::Update( float frameTime )
{
	BaseClass::Update( frameTime );

	aFrameTime = frameTime * sv_timescale;

	CheckPaused(  );

	if ( aPaused )
		return;

	aCurTime += aFrameTime;

	// uhh
	aLocalPlayer->Update( frameTime );
	aLocalPlayer->UpdateView();  // shit

#if !NO_BULLET_PHYSICS
	apPhysEnv->Simulate(  );

	// stupid
	aLocalPlayer->UpdatePosition(  );
#endif

	apGui->DebugMessage( 0, "Player Pos:  %s", Vec2Str(aLocalPlayer->aTransform.position).c_str() );
	apGui->DebugMessage( 1, "Player Rot:  %s", Quat2Str(aLocalPlayer->aTransform.rotation).c_str() );
	apGui->DebugMessage( 2, "Player Vel:  %s", Vec2Str(aLocalPlayer->aVelocity).c_str() );
	apGui->DebugMessage( 3, "View Offset: %s", Vec2Str(aLocalPlayer->aViewOffset).c_str() );

	SetupModels( frameTime );

	ResetInputs(  );

	UpdateAudio(  );

	if ( apInput->WindowHasFocus() )
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

	if ( aPaused )
	{
		ResetInputs(  );
		aLocalPlayer->UpdateView();
	}
}

ConVar proto_x("proto_x", "550");
ConVar proto_y("proto_y", "240");
ConVar proto_z("proto_z", "-360");

extern ConVar velocity_scale;


// will be used in the future for when updating bones and stuff
void GameSystem::SetupModels( float frameTime )
{
	if ( g_proto && g_proto->mdl )
	{
		g_proto->mdl->GetModelData().aTransform.position = {
			proto_x * velocity_scale,
			proto_y * velocity_scale,
			proto_z * velocity_scale
		};
	}

	// scale the world
	g_riverhouse->mdl->GetModelData().aTransform.scale = {
		velocity_scale,
		velocity_scale,
		velocity_scale
	};

	if ( g_streamModel )
	{
		g_streamModel->GetModelData().aTransform.scale = {snd_cube_scale, snd_cube_scale, snd_cube_scale};
	}

	// scale the nearz and farz
	aView.Set( 0, 0, aView.width, aView.height, 1 * velocity_scale, 10000 * velocity_scale, cl_fov );
}


void GameSystem::ResetInputs(  )
{
}


void GameSystem::UpdateAudio(  )
{
	if ( apInput->KeyJustPressed(SDL_SCANCODE_G) )
	{
		if ( stream && stream->Valid() )
		{
			apAudio->FreeSound( &stream );

			if ( g_streamModel )
				g_streamModel->GetModelData().aNoDraw = true;
		}
		// test sound
		// stereo plays twice as fast as mono right now?
		// else if ( apAudio->LoadSound("sound/rain2.ogg", &stream) )  
		// else if ( apAudio->LoadSound("sound/endymion2.ogg", &stream) )  
		//else if ( apAudio->LoadSound("sound/endymion_mono.ogg", &stream) )  
		//else if ( apAudio->LoadSound("sound/endymion2.wav", &stream) )  
		//else if ( apAudio->LoadSound("sound/endymion_mono.wav", &stream) )  
		else if ( apAudio->LoadSound("sound/robots_cropped.ogg", &stream) )  
		{
			stream->vol = 1.0;
			stream->pos = aLocalPlayer->GetPos();  // play it where the player currently is
			stream->inWorld = false;

			apAudio->PlaySound( stream );

			if ( g_streamModel == nullptr )
			{
				g_streamModel = new Model;
				apGraphics->LoadModel( "materials/models/cube.obj", "", g_streamModel );
				aModels.push_back( g_streamModel );
			}

			g_streamModel->GetModelData().aNoDraw = false;  // !stream->inWorld
			g_streamModel->SetPosition( aLocalPlayer->GetPos() );
		}
	}

	apAudio->SetListenerTransform( aLocalPlayer->GetPos(), aLocalPlayer->aTransform.rotation );
}


void GameSystem::HandleSDLEvent( SDL_Event* e )
{
	switch (e->type)
	{
		case SDL_WINDOWEVENT_SIZE_CHANGED:
		{
			apGraphics->GetWindowSize( &aView.width, &aView.height );
			aView.ComputeProjection();
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

