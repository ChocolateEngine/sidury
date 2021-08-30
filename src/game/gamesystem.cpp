#include "gamesystem.h"
#include "../../chocolate/inc/core/engine.h"
#include "player.h"
#include <algorithm>


GameSystem* g_pGame = NULL;


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
}

const int swarmModelCount = 4;

Player* g_player = NULL;

Model* g_riverhouse = new Model;
Model* g_swarmModels[swarmModelCount] = {NULL};

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

	//apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", "materials/act_like_a_baka.jpg", g_riverhouse );
	apGraphics->LoadModel( "materials/models/riverhouse/riverhouse_source_scale.obj", "materials/act_like_a_baka.jpg", g_riverhouse );
	aModels.push_back( g_riverhouse );
	// apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", g_riverhouse );

	for ( int i = 0; i < swarmModelCount; i++ )
	{
		//g_swarmModels[i] = new Model;
		//aModels.push_back(g_swarmModels[i]);
		//apGraphics->LoadModel( "materials/models/protogen_wip_22/protogen_wip_22.obj", "materials/act_like_a_baka.jpg", g_swarmModels[i] );
	}

	// create a player
	g_player = new Player;
	g_player->Spawn();

	// setup rand(  )
	srand( ( unsigned int )time( 0 ) );
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
			//g_swarmModels[i]->GetModelData().aPos[j] = ( float )( rand(  ) / ( float )( RAND_MAX / 10.0f ) );
		}
	}
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

