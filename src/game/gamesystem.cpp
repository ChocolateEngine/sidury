#include "gamesystem.h"
#include "../../chocolate/inc/core/engine.h"
#include "player.h"
#include <algorithm>


GameSystem* g_pGame = NULL;

void CenterMouseOnScreen(  )
{
	int w, h;
	SDL_GetWindowSize( g_pGame->apGraphics->GetWindow(), &w, &h );
	SDL_WarpMouseInWindow( g_pGame->apGraphics->GetWindow(), w/2, h/2 );
}


struct ModelPhysTest
{
	Model* mdl;
#if !NO_BULLET_PHYSICS
	PhysicsObject* physObj;
#else
	int* physObj;
#endif
};


const int physEntCount = 2;
ModelPhysTest* g_riverhouse = new ModelPhysTest{new Model, NULL};
ModelPhysTest* g_physEnts[physEntCount] = {NULL};


GameSystem::GameSystem(  ):
	aPaused( true ),
	aFrameTime(0.f),
	// only large near and farz for riverhouse and quake movement
	aView( 0, 0, 200, 200, 1, 10000, 90 )
{
	g_pGame = this;
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
}


void GameSystem::Init(  )
{
	BaseClass::Init(  );

	LoadModules(  );

	srand( ( unsigned int )time( 0 ) );  // setup rand(  )

	LoadWorld(  );

	// create a player
	aLocalPlayer = new Player;
	aLocalPlayer->Spawn();
}


void GameSystem::LoadModules(  )
{
	GET_SYSTEM_CHECK( apGui, BaseGuiSystem );
	GET_SYSTEM_CHECK( apGraphics, BaseGraphicsSystem );
	GET_SYSTEM_CHECK( apInput, BaseInputSystem );

	apGraphics->GetWindowSize( &aView.width, &aView.height );
	aView.ComputeProjection();

#if !NO_BULLET_PHYSICS
	apPhysEnv = new PhysicsEnvironment;
	apPhysEnv->Init(  );
#endif
}


void GameSystem::LoadWorld(  )
{
	//apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", "materials/act_like_a_baka.jpg", g_riverhouse->mdl );
	apGraphics->LoadModel( "materials/models/riverhouse/riverhouse_source_scale.obj", "materials/act_like_a_baka.jpg", g_riverhouse->mdl );
	aModels.push_back( g_riverhouse->mdl );

#if !NO_BULLET_PHYSICS
	//PhysicsObjectInfo physInfo( ShapeType::Concave );
	//physInfo.modelData = &g_riverhouse->mdl->GetModelData();

	// just have the ground be a box for now since collision on the riverhouse mesh is too jank still
	PhysicsObjectInfo physInfo( ShapeType::Box );
	physInfo.bounds = {1500, 200, 1500};

	g_riverhouse->physObj = apPhysEnv->CreatePhysicsObject( physInfo );
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
		g_physEnts[i] = new ModelPhysTest{ new Model, NULL };
		aModels.push_back( g_physEnts[i]->mdl );
		apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", "materials/1aaaaaaa.jpg", g_physEnts[i]->mdl );

		for (int j = 0; j < 3; j++)
		{
			// set a random spawn position in the world
			g_physEnts[i]->mdl->GetModelData().aTransform.position[j] = ( float )( rand(  ) / ( float )( RAND_MAX / 10.0f ) );
		}

		// raise it up in the air
		g_physEnts[i]->mdl->GetModelData().aTransform.position.y += 500.f;

#if !NO_BULLET_PHYSICS
		physInfo.transform.position = g_physEnts[i]->mdl->GetModelData().aPos;

		//physInfo.modelData = &g_riverhouse->mdl->GetModelData();

		//g_physEnts[i]->physObj = apPhysEnv->CreatePhysicsObject( physInfo );
		//g_physEnts[i]->physObj->SetAlwaysActive( true );
#endif
	}
}


void GameSystem::InitConsoleCommands(  )
{
	BaseClass::InitConsoleCommands(  );
}


void GameSystem::Update( float frameTime )
{
	BaseClass::Update( frameTime );

	aFrameTime = frameTime;

	CheckPaused(  );

	if ( aPaused )
		return;

	// uhh
	aLocalPlayer->Update( frameTime );

#if !NO_BULLET_PHYSICS
	apPhysEnv->Simulate(  );

	// stupid
	aLocalPlayer->UpdatePosition(  );
#endif

	SetupModels( frameTime );

	ResetInputs(  );

	CenterMouseOnScreen(  );
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

	if ( aPaused )
	{
		ResetInputs(  );
	}
}


// will be used in the future for when updating bones and stuff
void GameSystem::SetupModels( float frameTime )
{
}


void GameSystem::ResetInputs(  )
{
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

