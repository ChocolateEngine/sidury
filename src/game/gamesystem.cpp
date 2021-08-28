#include "gamesystem.h"
#include "../../chocolate/inc/core/engine.h"
#include <algorithm>


GameSystem::GameSystem(  ):
	aPaused( true ),
	aMouseDelta( 0, 0 ),
	aView( 0, 0, 200, 200, 0.1, 100, 90 )
{
}


GameSystem::~GameSystem(  )
{
}

const int swarmModelCount = 1;

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

	apGraphics->LoadModel( "materials/models/riverhouse/riverhouse.obj", "materials/act_like_a_baka.jpg", g_riverhouse );

	for ( int i = 0; i < swarmModelCount; i++ )
	{
		//g_swarmModels[i] = new Model;
		//apGraphics->LoadModel( "materials/models/protogen_wip_22/protogen_wip_22.obj", "materials/act_like_a_baka.jpg", g_swarmModels[i] );
	}

	srand( ( unsigned int )time( 0 ) );
}


void GameSystem::Update( float dt )
{
	CheckPaused();

	if ( aPaused )
		return;

	InputCamera( dt );
	UpdateCamera();
	SetupModels( dt );
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
		aMouseDelta.x = 0;
		aMouseDelta.y = 0;
	}
}


void GameSystem::SetupModels( float dt )
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


void GameSystem::UpdateCamera(  )
{
	static glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
	static glm::vec3 forward = glm::vec3(0.0f, 0.0f, -1.0f);
	static glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);

	glm::quat xRot = glm::angleAxis(glm::radians(mY), glm::normalize(glm::vec3(-1.0f, 0.0f, 0.0f)));
	glm::quat yRot = glm::angleAxis(glm::radians(mX), glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f)));
	aCamera.transform.rotation = xRot * yRot;

	aView.viewMatrix = ToFirstPersonCameraTransformation(aCamera.transform);

	aCamera.forward = forward * yRot;
	//aCamera.up = up;
	aCamera.up = up * yRot;
	aCamera.right = right * yRot;

	aCamera.back = -aCamera.forward;
	aCamera.down = -aCamera.up;
	aCamera.left = -aCamera.right;

	apGraphics->SetView( aView );

	aMouseDelta.x = 0;
	aMouseDelta.y = 0;
}

void GameSystem::InputCamera( float dt )
{
	const Uint8* state = SDL_GetKeyboardState( NULL );

	const float speed = state[SDL_SCANCODE_LSHIFT] ? 12.f : 6.f;

	glm::vec3 move = {};

	if ( state[SDL_SCANCODE_W] ) move += aCamera.forward;
	if ( state[SDL_SCANCODE_S] ) move += aCamera.back;
	if ( state[SDL_SCANCODE_A] ) move += aCamera.left;
	if ( state[SDL_SCANCODE_D] ) move += aCamera.right;

	if ( state[SDL_SCANCODE_SPACE] ) move += aCamera.up;
	if ( state[SDL_SCANCODE_LCTRL] ) move += aCamera.down;

	glm::normalize( move );
	aCamera.transform.position += move * speed * dt;

	/*if (input->IsPressed("Jump"))
		camera.transform.position += glm::vec3(0.0f, 1.0f, 0.0f) * speed * dt;
	else if (input->IsPressed("Crouch"))
		camera.transform.position += glm::vec3(0.0f, -1.0f, 0.0f) * speed * dt;*/

	/*if (input->IsPressed("LookUp"))
		mY += 180.0f * dt;
	else if (input->IsPressed("LookDown"))
		mY -= 180.0f *dt;

	if (input->IsPressed("TurnRight"))
		mX += 180.0f * dt;
	else if (input->IsPressed("TurnLeft"))
		mX -= 180.0f * dt;*/

	mX += aMouseDelta.x * 0.1f;
	mY -= aMouseDelta.y * 0.1f;

	auto constrain = [](float num) -> float
	{
		num = std::fmod(num, 360.0f);
		return (num < 0.0f) ? num += 360.0f : num;
	};

	mX = constrain(mX);
	mY = std::clamp(mY, -70.0f, 70.0f);

	/*if (input->IsJustPressed("ZoomIn"))
		camera.transform.scale += 0.1f;
	else if (input->IsJustPressed("ZoomOut"))
		camera.transform.scale -= 0.1f;*/
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

