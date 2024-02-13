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
#include "imgui/imgui_internal.h"


CONVAR( r_fov, 106.f, CVARF_ARCHIVE );
CONVAR( r_nearz, 1.f );
CONVAR( r_farz, 10000.f );

extern ConVar m_yaw, m_pitch;

CONVAR( view_move_slow, 0.2f );
CONVAR( view_move_fast, 3.0f );

CONVAR( view_move_forward, 500.0f );
CONVAR( view_move_side, 500.0f );
CONVAR( view_move_up, 500.0f );

CONVAR( view_move_min, 0.125f );
CONVAR( view_move_max, 15.f );
CONVAR( view_move_scroll_sens, 0.125f );

CONVAR( editor_show_pos, 1.f );
CONVAR( editor_spew_imgui_window_hover, 0 );


// TODO: Remove This for multiselect
extern ChHandle_t gSelectedEntity;


// Check the function FindHoveredWindow() in imgui.cpp to see if you need to update this when updating imgui
bool MouseHoveringImGuiWindow( glm::ivec2 mousePos )
{
	ImGuiContext& g = *ImGui::GetCurrentContext();

	ImVec2        imMousePos{ (float)mousePos.x, (float)mousePos.y };

	ImGuiWindow*  hovered_window                        = NULL;
	ImGuiWindow*  hovered_window_ignoring_moving_window = NULL;
	if ( g.MovingWindow && !( g.MovingWindow->Flags & ImGuiWindowFlags_NoMouseInputs ) )
		hovered_window = g.MovingWindow;

	ImVec2 padding_regular    = g.Style.TouchExtraPadding;
	ImVec2 padding_for_resize = g.IO.ConfigWindowsResizeFromEdges ? g.WindowsHoverPadding : padding_regular;
	for ( int i = g.Windows.Size - 1; i >= 0; i-- )
	{
		ImGuiWindow* window = g.Windows[ i ];
		IM_MSVC_WARNING_SUPPRESS( 28182 );  // [Static Analyzer] Dereferencing NULL pointer.
		if ( !window->WasActive || window->Hidden )
			continue;
		if ( window->Flags & ImGuiWindowFlags_NoMouseInputs )
			continue;
		IM_ASSERT( window->Viewport );
		if ( window->Viewport != g.MouseViewport )
			continue;

		// Using the clipped AABB, a child window will typically be clipped by its parent (not always)
		ImVec2 hit_padding = ( window->Flags & ( ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize ) ) ? padding_regular : padding_for_resize;
		if ( !window->OuterRectClipped.ContainsWithPad( imMousePos, hit_padding ) )
			continue;

		// Support for one rectangular hole in any given window
		// FIXME: Consider generalizing hit-testing override (with more generic data, callback, etc.) (#1512)
		if ( window->HitTestHoleSize.x != 0 )
		{
			ImVec2 hole_pos( window->Pos.x + (float)window->HitTestHoleOffset.x, window->Pos.y + (float)window->HitTestHoleOffset.y );
			ImVec2 hole_size( (float)window->HitTestHoleSize.x, (float)window->HitTestHoleSize.y );
			ImVec2 hole_pos_size{};

			hole_pos_size.x = hole_pos.x + hole_size.x;
			hole_pos_size.y = hole_pos.y + hole_size.y;

			if ( ImRect( hole_pos, hole_pos_size ).Contains( imMousePos ) )
				continue;
		}

		if ( hovered_window == NULL )
			hovered_window = window;
		IM_MSVC_WARNING_SUPPRESS( 28182 );  // [Static Analyzer] Dereferencing NULL pointer.
		if ( hovered_window_ignoring_moving_window == NULL && ( !g.MovingWindow || window->RootWindowDockTree != g.MovingWindow->RootWindowDockTree ) )
			hovered_window_ignoring_moving_window = window;
		if ( hovered_window && hovered_window_ignoring_moving_window )
			break;
	}

	if ( hovered_window )
	{
		if ( editor_spew_imgui_window_hover )
			Log_DevF( 1, "HOVERING WINDOW \"%s\"\n", hovered_window->Name );

		return true;
	}

	return false;
}


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
static bool gCheckSelectionResult = false;


bool EditorView_IsMouseInView()
{
	// TEMP
	ViewportShader_t* viewport = graphics->GetViewportData( 0 );

	glm::ivec2 mousePos = input->GetMousePos();

	if ( viewport->aOffset.x > mousePos.x )
		return false;

	if ( viewport->aOffset.y > mousePos.y )
		return false;

	if ( viewport->aOffset.x + viewport->aSize.x < mousePos.x )
		return false;

	if ( viewport->aOffset.y + viewport->aSize.y < mousePos.y )
		return false;

	if ( MouseHoveringImGuiWindow( mousePos ) )
		return false;

	return true;
}


static void UpdateSelectionRenderables()
{
	glm::ivec2                      mousePos = input->GetMousePos();
	EditorContext_t*                context  = Editor_GetContext();

	if ( ImGui::IsAnyItemHovered() )
		return;

	ChVector< SelectionRenderable > selectList;
	for ( ChHandle_t entHandle : context->aMap.aMapEntities )
	{
		Entity_t* ent = Entity_GetData( entHandle );

		if ( !ent )
		{
			continue;
		}

		if ( !ent->aRenderable )
		{
			continue;
		}

		SelectionRenderable selectRenderable;
		selectRenderable.renderable = ent->aRenderable;
		selectRenderable.color[ 0 ] = ent->aSelectColor[ 0 ];
		selectRenderable.color[ 1 ] = ent->aSelectColor[ 1 ];
		selectRenderable.color[ 2 ] = ent->aSelectColor[ 2 ];

		selectList.push_back( selectRenderable );
	}

	renderOld->SetSelectionRenderablesAndCursorPos( selectList, mousePos );

	gCheckSelectionResult = true;
}


void EditorView_UpdateInputs()
{
	PROF_SCOPE();

	gEditorData.aMove = { 0.f, 0.f, 0.f };

	if ( !EditorView_IsMouseInView() )
		return;

	glm::ivec2 mouseScroll = input->GetMouseScroll();

	if ( mouseScroll.y != 0 )
	{
		gMoveScale += mouseScroll.y * view_move_scroll_sens;
		gMoveScale = std::clamp( gMoveScale, view_move_min.GetFloat(), view_move_max.GetFloat() );
		Log_DevF( 2, "Movement Speed Scale: %.4f\n", gMoveScale );
	}

	gui->DebugMessage( "Movement Speed Scale: %.4f", gMoveScale );

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

	if ( Input_KeyJustPressed( EBinding_Viewport_SelectMulti ) )
	{
		UpdateSelectionRenderables();
	}
	else if ( Input_KeyJustPressed( EBinding_Viewport_SelectSingle ) )
	{
		Editor_GetContext()->aEntitiesSelected.clear();
		// gSelectedEntity = CH_INVALID_HANDLE;
		UpdateSelectionRenderables();
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


static void CenterMouseOnScreen( EditorContext_t* context )
{
	int x = ( context->aView.aResolution.x / 2.f ) + context->aView.aOffset.x;
	int y = ( context->aView.aResolution.y / 2.f ) + context->aView.aOffset.y;

	// int x = context->aView.aOffset.x;
	// int y = context->aView.aOffset.y;

	SDL_WarpMouseInWindow( render->GetWindow(), x, y );
}


void EditorView_Update()
{
	EditorContext_t* context = Editor_GetContext();

	if ( !context )
		return;

	if ( gCheckSelectionResult )
	{
		gCheckSelectionResult = false;
		u8        selectColor[ 3 ];

		gSelectedEntity = CH_INVALID_HANDLE;

		if ( renderOld->GetSelectionResult( selectColor[ 0 ], selectColor[ 1 ], selectColor[ 2 ] ) )
		{
			// scan entities to find a matching color
			for ( ChHandle_t entHandle : context->aMap.aMapEntities )
			{
				Entity_t* ent = Entity_GetData( entHandle );

				if ( !ent )
				{
					continue;
				}

				if ( ent->aSelectColor[ 0 ] == selectColor[ 0 ] )
				{
					context->aEntitiesSelected.push_back( entHandle );
					gSelectedEntity = entHandle;
					break;
				}
			}
		}
	}

	// for now until some focus thing
	if ( !gui->IsConsoleShown() )
	{
		// Handle Inputs
		EditorView_UpdateInputs();
	}

	bool centerMouse = false;

	// for now until some focus thing
	if ( !gui->IsConsoleShown() && Input_KeyPressed( EBinding_Viewport_MouseLook ) )
	{
		if ( Input_KeyJustPressed( EBinding_Viewport_MouseLook ) )
		{
  			SDL_SetRelativeMouseMode( SDL_TRUE );
			SDL_ShowCursor( SDL_FALSE );
		}

		// Handle Mouse Input
		const glm::vec2 mouse = Input_GetMouseDelta();

		// transform.aAng[PITCH] = -mouse.y;
		context->aView.aAng[ PITCH ] += mouse.y * m_pitch;
		context->aView.aAng[ YAW ] += mouse.x * m_yaw;

		ClampAngles( context->aView.aAng );

		centerMouse = true;
	}
	else if ( Input_KeyJustReleased( EBinding_Viewport_MouseLook ) )
	{
		SDL_SetRelativeMouseMode( SDL_FALSE );
		SDL_ShowCursor( SDL_TRUE );

		centerMouse = true;
	}

	// Handle View
	EditorView_UpdateView( context );

	if ( centerMouse )
		CenterMouseOnScreen( context );
}


void Util_ComputeCameraRay( glm::vec3& srStart, glm::vec3& srDir, glm::vec2 sMousePos, glm::vec2 sViewportSize )
{
	if ( sMousePos.x == FLT_MAX && sMousePos.y == FLT_MAX )
	{
		sMousePos = input->GetMousePos();
	}

	if ( sViewportSize.x == FLT_MAX && sViewportSize.y == FLT_MAX )
	{
		int width, height;
		render->GetSurfaceSize( width, height );

		sViewportSize.x = width;
		sViewportSize.y = height;
	}


}

