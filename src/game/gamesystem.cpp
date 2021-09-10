#include "gamesystem.h"
#include "../../chocolate/inc/core/engine.h"
#include "player.h"
#include <algorithm>


#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>


GameSystem* g_pGame = NULL;
Player* g_player = NULL;


GameSystem::GameSystem(  ):
	aPaused( true ),
	aMouseDelta( 0, 0 ),
	aMousePos(0, 0),
	aFrameTime(0.f),
	// only large near and farz for riverhouse and quake movement
	aView( 0, 0, 200, 200, 1, 10000, 90 )
{
	g_pGame = this;
}


GameSystem::~GameSystem(  )
{
	if ( apPhysEnv )
		delete apPhysEnv;

	if ( g_player )
		delete g_player;
}

const int swarmModelCount = 2;

struct ModelPhysTest
{
	Model* mdl;
	PhysicsObject* physObj;
};


ModelPhysTest* g_riverhouse = new ModelPhysTest{new Model, NULL};
ModelPhysTest* g_swarmModels[swarmModelCount] = {NULL};

// TEMP
#define SPAWN_POS 1085.69824, 322.443970, 644.222046

void GameSystem::Init(  )
{
	BaseClass::Init(  );

	apGui = GET_SYSTEM( BaseGuiSystem );
	if ( apGui == nullptr )
		apCommandManager->Execute( Engine::Commands::EXIT );

	apGraphics = GET_SYSTEM( BaseGraphicsSystem );
	if ( apGraphics == nullptr )
		apCommandManager->Execute( Engine::Commands::EXIT );

	apGraphics->GetWindowSize( &aView.width, &aView.height );
	aView.ComputeProjection();

	// ========================================

	apPhysEnv = new PhysicsEnvironment;
	apPhysEnv->Init(  );

	//apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", "materials/act_like_a_baka.jpg", g_riverhouse );
	apGraphics->LoadModel( "materials/models/riverhouse/riverhouse_source_scale.obj", "materials/act_like_a_baka.jpg", g_riverhouse->mdl );
	aModels.push_back( g_riverhouse->mdl );

	// setup rand(  )
	srand( ( unsigned int )time( 0 ) );

	{
		PhysicsObjectInfo physInfo;
		physInfo.shapeType = ShapeType::Box;
		physInfo.bounds = {1500, 200, 1500};
		g_riverhouse->physObj = apPhysEnv->CreatePhysicsObject( physInfo );
	}

	PhysicsObjectInfo physInfo;
	/*physInfo.callbacks = true;
	// physInfo.shapeType = ShapeType::Concave;
	physInfo.shapeType = ShapeType::Convex;
	//physInfo.collisionType = CollisionType::Static;
	physInfo.modelData = &g_riverhouse->mdl->GetModelData();

	g_riverhouse->physObj = apPhysEnv->CreatePhysicsObject( physInfo );*/

	physInfo.shapeType = ShapeType::Convex;
	physInfo.collisionType = CollisionType::Kinematic;
	physInfo.mass = 20.f;

	for ( int i = 0; i < swarmModelCount; i++ )
	{
		g_swarmModels[i] = new ModelPhysTest{new Model, NULL};
		aModels.push_back(g_swarmModels[i]->mdl);
		apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", "materials/1aaaaaaa.jpg", g_swarmModels[i]->mdl );

		/*for (int j = 0; j < 3; j++)
		{
			g_swarmModels[i]->mdl->GetModelData().aPos[j] = ( float )( rand(  ) / ( float )( RAND_MAX / 10.0f ) );
		}
		
		g_swarmModels[i]->mdl->GetModelData().aPos.y += 500.f;
		physInfo.transform.position = g_swarmModels[i]->mdl->GetModelData().aPos;*/
		physInfo.modelData = &g_riverhouse->mdl->GetModelData();

		//g_swarmModels[i]->physObj = apPhysEnv->CreatePhysicsObject( physInfo );
		//g_swarmModels[i]->physObj->SetAlwaysActive( true );
	}

	// ========================================

	// create a player
	g_player = new Player;
	g_player->Spawn();

	float r = 0.f;
	for (int i = 0; i < swarmModelCount; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			g_swarmModels[i]->mdl->GetModelData().aPos[j] = ( float )( rand(  ) / ( float )( RAND_MAX / 10.0f ) );
		}

		g_swarmModels[i]->mdl->GetModelData().aPos.y += 500.f;
		//g_swarmModels[i]->rigidBody->getWorldTransform().setOrigin( toBt(g_swarmModels[i]->mdl->GetModelData().aPos) );
		//Transform& transform = g_swarmModels[i]->physObj->GetWorldTransform();
		//transform.position = g_swarmModels[i]->mdl->GetModelData().aPos;
		//g_swarmModels[i]->physObj->SetWorldTransform( transform );
	}
}


void GameSystem::InitConsoleCommands(  )
{
	ConCommand cmd;

	cmd.str = "respawn";
	cmd.func = [ & ]( std::vector< std::string > sArgs )
	{
		g_player->Respawn();
	};
	aConsoleCommands.push_back( cmd );
}


void GameSystem::Update( float frameTime )
{
	BaseClass::Update( frameTime );

	aFrameTime = frameTime;

	CheckPaused(  );

	if ( aPaused )
		return;

	g_player->Update( frameTime );

	apPhysEnv->Simulate(  );

	g_player->UpdatePosition(  );

	SetupModels( frameTime );

	ResetInputs(  );
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
			int w, h;
			SDL_GetWindowSize( apGraphics->GetWindow(), &w, &h );
			SDL_WarpMouseInWindow( apGraphics->GetWindow(), w/2, h/2 );
		}
	}

	if ( aPaused )
	{
		ResetInputs(  );
	}
}


void GameSystem::SetupModels( float frameTime )
{
	// swarm models
	float r = 0.f;
	for (int i = 0; i < swarmModelCount; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			g_swarmModels[i]->mdl->GetModelData().aPos[j] = ( float )( rand(  ) / ( float )( RAND_MAX / 10.0f ) );

		}

		//g_swarmModels[i]->mdl->GetModelData().aPos = g_swarmModels[i]->physObj->GetWorldTransform().position;
	}

	/*btTransform transform = g_riverhouse->rigidBody->getWorldTransform();
	glm::vec3 pos = fromBt( transform.getOrigin() );
	//glm::quat ang = fromBt( transform.getRotation() );
	g_riverhouse->mdl->GetModelData().aPos = pos;*/
}


void GameSystem::ResetInputs(  )
{
	aMouseDelta.x = 0;
	aMouseDelta.y = 0;
}


void GameSystem::HandleSDLEvent( SDL_Event* e )
{
	switch (e->type)
	{
		case SDL_MOUSEMOTION:
		{
			aMousePos.x = e->motion.x;
			aMousePos.y = e->motion.y;
			aMouseDelta.x += e->motion.xrel;
			aMouseDelta.y += e->motion.yrel;
			break;
		}

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

