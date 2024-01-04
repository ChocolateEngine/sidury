#include "editor_view.h"
#include "main.h"
#include "game_shared.h"
#include "skybox.h"
#include "inputsystem.h"

#include "igui.h"
#include "iinput.h"
#include "render/irender.h"
#include "igraphics.h"

#include "imgui/imgui.h"


CONVAR( r_fov, 106.f, CVARF_ARCHIVE );
CONVAR( r_nearz, 1.f );
CONVAR( r_farz, 10000.f );

extern ConVar m_yaw, m_pitch;

CONVAR( view_move_slow, 0.2f );
CONVAR( view_move_fast, 3.0f );

CONVAR( view_move_forward, 500.0f );
CONVAR( view_move_side, 500.0f );
CONVAR( view_move_up, 500.0f );

CONVAR( view_move_min, 0.01f );
CONVAR( view_move_max, 50000.f );

CONVAR( editor_show_pos, 1.f );


void EditorView_Init()
{
}


void EditorView_Shutdown()
{
}


static bool gDrawMouseTrace = false;
static glm::vec3 gMouseTraceStart{};
static glm::vec3 gMouseTraceEnd{};
static float gMoveScale = 1.f;


void EditorView_UpdateInputs()
{
	PROF_SCOPE();

	gEditorData.aMove = { 0.f, 0.f, 0.f };

	glm::ivec2 mouseScroll = input->GetMouseScroll();

	if ( mouseScroll.y != 0 )
		gMoveScale += mouseScroll.y * 0.25;

	gMoveScale = std::clamp( gMoveScale, view_move_min.GetFloat(), view_move_max.GetFloat() );

	float moveScale = gFrameTime * gMoveScale;
	
	if ( Input_KeyPressed( EBinding_Viewport_Slow ) )
	{
		moveScale *= view_move_slow;
	}

	if ( Input_KeyPressed( EBinding_Viewport_Sprint ) )
	{
		moveScale *= view_move_fast;
	}

	const float forwardSpeed = view_move_forward * moveScale;
	const float sideSpeed    = view_move_side * moveScale;
	const float upSpeed      = view_move_up * moveScale;
	// apMove->aMaxSpeed        = max_speed * moveScale;

	if ( Input_KeyPressed( EBinding_Viewport_MoveForward ) )
		gEditorData.aMove[ W_FORWARD ] = forwardSpeed;

	if ( Input_KeyPressed( EBinding_Viewport_MoveBack ) )
		gEditorData.aMove[ W_FORWARD ] += -forwardSpeed;

	if ( Input_KeyPressed( EBinding_Viewport_MoveLeft ) )
		gEditorData.aMove[ W_RIGHT ] = -sideSpeed;

	if ( Input_KeyPressed( EBinding_Viewport_MoveRight ) )
		gEditorData.aMove[ W_RIGHT ] += sideSpeed;

	if ( Input_KeyPressed( EBinding_Viewport_MoveUp ) )
		gEditorData.aMove[ W_UP ] = upSpeed;

	if ( Input_KeyPressed( EBinding_Viewport_MoveDown ) )
		gEditorData.aMove[ W_UP ] -= upSpeed;

	if ( Input_KeyPressed( EBinding_Viewport_Select ) )
	{
		//gDrawMouseTrace = true;

		//

		// calculate a mouse trace line
		glm::ivec2 mousePos = input->GetMousePos();

		Editor_GetContext()->aView.aResolution;
		Editor_GetContext()->aView.aProjViewMat;

	}

	if ( gDrawMouseTrace )
	{
		graphics->DrawLine( gMouseTraceStart, gMouseTraceEnd, {1.f, 0.6f, 0.6f, 1.f} );
	}
}



void EditorView_UpdateView( EditorContext_t* spContext )
{
	PROF_SCOPE();

	// move viewport
	for ( int i = 0; i < 3; i++ )
	{
		// spContext->aView.aPos[ i ] += spContext->aView.aForward[ i ] * gEditorData.aMove.x + spContext->aView.aRight[ i ] * gEditorData.aMove[ W_RIGHT ];
		spContext->aView.aPos[ i ] += spContext->aView.aForward[ i ] * gEditorData.aMove.x + spContext->aView.aRight[ i ] * gEditorData.aMove[ W_RIGHT ] + spContext->aView.aUp[ i ] * gEditorData.aMove.z;
	}

	glm::mat4 viewMatrixZ;

	{
		Util_ToViewMatrixZ( viewMatrixZ, spContext->aView.aPos, spContext->aView.aAng );
		Util_GetViewMatrixZDirection( viewMatrixZ, spContext->aView.aForward, spContext->aView.aRight, spContext->aView.aUp );
	}

	// Transform transformViewTmp = GetEntitySystem()->GetWorldTransform( info->aCamera );
	// Transform transformView    = transformViewTmp;

	//transformView.aAng[ PITCH ] = transformViewTmp.aAng[ YAW ];
	//transformView.aAng[ YAW ]   = transformViewTmp.aAng[ PITCH ];

	//transformView.aAng.Edit()[ YAW ] *= -1;

	//Transform transformView = transform;
	//transformView.aAng += move.aViewAngOffset;

	// if ( cl_thirdperson.GetBool() )
	// {
	// 	Transform thirdPerson = {
	// 		.aPos = { cl_cam_x.GetFloat(), cl_cam_y.GetFloat(), cl_cam_z.GetFloat() }
	// 	};
	// 
	// 	// thirdPerson.aPos = {cl_cam_x.GetFloat(), cl_cam_y.GetFloat(), cl_cam_z.GetFloat()};
	// 
	// 	glm::mat4 viewMatrixZ;
	// 	Util_ToViewMatrixZ( viewMatrixZ, transformView.aPos, transformView.aAng );
	// 
	// 	glm::mat4 viewMat = thirdPerson.ToMatrix( false ) * viewMatrixZ;
	// 
	// 	if ( info->aIsLocalPlayer )
	// 	{
	// 		ViewportShader_t* viewport = graphics->GetViewportData( 0 );
	// 
	// 		if ( viewport )
	// 			viewport->aViewPos = thirdPerson.aPos;
	// 
	// 		// TODO: PERF: this also queues the viewport data
	// 		Game_SetView( viewMat );
	// 		// audio->SetListenerTransform( thirdPerson.aPos, transformView.aAng );
	// 	}
	// 
	// 	Util_GetViewMatrixZDirection( viewMat, camDir->aForward.Edit(), camDir->aRight.Edit(), camDir->aUp.Edit() );
	// }
	// else
	{
		glm::mat4 viewMat;
		Util_ToViewMatrixZ( viewMat, spContext->aView.aPos, spContext->aView.aAng );

		// wtf broken??
		// audio->SetListenerTransform( transformView.aPos, transformView.aAng );

		ViewportShader_t* viewport = graphics->GetViewportData( spContext->aView.aViewportIndex );

		if ( viewport )
			viewport->aViewPos = spContext->aView.aPos;

		// TODO: PERF: this also queues the viewport data
		Game_SetView( viewMat );

		Util_GetViewMatrixZDirection( viewMatrixZ, spContext->aView.aForward, spContext->aView.aRight, spContext->aView.aUp );
	}

	// temp
	//graphics->DrawAxis( transformView.aPos, transformView.aAng, { 40.f, 40.f, 40.f } );
	//graphics->DrawAxis( transform->aPos, transform->aAng, { 40.f, 40.f, 40.f } );

	Game_UpdateProjection();

	// i feel like there's gonna be a lot more here in the future...
	Skybox_SetAng( spContext->aView.aAng );

	//glm::vec3 listenerAng{
	//	transformView.aAng.Get().x,
	//	transformView.aAng.Get().z,
	//	transformView.aAng.Get().y,
	//};
	//
	//audio->SetListenerTransform( transformView.aPos, listenerAng );

#if AUDIO_OPENAL
	//audio->SetListenerVelocity( rigidBody->aVel );
	// audio->SetListenerOrient( camDir->aForward, camDir->aUp );
#endif

	if ( editor_show_pos )
	{
		gui->DebugMessage( "Pos: %s", Vec2Str( spContext->aView.aPos ).c_str() );
		gui->DebugMessage( "Ang: %s", Vec2Str( spContext->aView.aAng ).c_str() );
	}
}


inline float DegreeConstrain( float num )
{
	num = std::fmod( num, 360.0f );
	return ( num < 0.0f ) ? num += 360.0f : num;
}


inline void ClampAngles( glm::vec3& srAng )
{
	srAng[ YAW ]   = DegreeConstrain( srAng[ YAW ] );
	srAng[ PITCH ] = std::clamp( srAng[ PITCH ], -90.0f, 90.0f );
};


static void CenterMouseOnScreen()
{
	int w, h;
	SDL_GetWindowSize( render->GetWindow(), &w, &h );
	SDL_WarpMouseInWindow( render->GetWindow(), w / 2, h / 2 );
}


void EditorView_Update()
{
	EditorContext_t* context = Editor_GetContext();

	if ( !context )
		return;

	// for now until some focus thing
	if ( !gui->IsConsoleShown() )
	{
		// Handle Inputs
		EditorView_UpdateInputs();
	}

	// for now until some focus thing
	if ( !gui->IsConsoleShown() && Input_KeyPressed( EBinding_Viewport_MouseLook ) )
	{
		if ( Input_KeyJustPressed( EBinding_Viewport_MouseLook ) )
		{
			SDL_SetRelativeMouseMode( SDL_TRUE );
		}

		// Handle Mouse Input
		const glm::vec2 mouse = Input_GetMouseDelta();

		// transform.aAng[PITCH] = -mouse.y;
		context->aView.aAng[ PITCH ] += mouse.y * m_pitch;
		context->aView.aAng[ YAW ] += mouse.x * m_yaw;

		ClampAngles( context->aView.aAng );

		CenterMouseOnScreen();
	}
	else if ( Input_KeyJustReleased( EBinding_Viewport_MouseLook ) )
	{
		SDL_SetRelativeMouseMode( SDL_FALSE );

		CenterMouseOnScreen();
	}

	// Handle View
	EditorView_UpdateView( context );
}

