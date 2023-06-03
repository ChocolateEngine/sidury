#include "testing.h"
#include "main.h"
#include "inputsystem.h"
#include "graphics/graphics.h"

#include "cl_main.h"
#include "sv_main.h"

#include "imgui/imgui.h"

// ---------------------------------------------------------------------------
// This file is dedicated for random stuff and experiments with the game
// and will be quite messy

// #define DEFAULT_PROTOGEN_PATH "materials/models/protogen_wip_25d/protogen_wip_25d.obj"
#define DEFAULT_PROTOGEN_PATH "materials/models/protogen_wip_25d/protogen_25d.glb"

CONCMD( servertest )
{
	// start loopback server

	// connect to the loopback server
	CL_Connect( "127.0.0.1" );
}

CONVAR( vrcmdl_scale, 40 );
CONVAR( in_proto_spam, 0, CVARF_INPUT );
CONVAR( proto_look, 1 );
CONVAR( proto_follow, 0 );
CONVAR( proto_follow_speed, 400 );

extern ConVar                 phys_friction;
extern ConVar                 cl_view_height;

extern Entity                 gLocalPlayer;

// Audio Channels
Handle                        hAudioMusic = InvalidHandle;


// GET RID OF THIS
std::vector< Entity >         g_protos;
std::vector< Entity >         g_otherEnts;
std::vector< Entity >         g_staticEnts;
std::vector< ModelPhysTest* > g_physEnts;

// testing
std::vector< Handle >         streams{};
Model*                        g_streamModel = nullptr;

ConVar                        snd_cube_scale( "snd_cube_scale", "0.05" );

struct ProtoLookData_t
{
	std::vector< Entity > aProtos;
	Transform*            aPlayerTransform;
};


void CreateProtogen( const std::string& path )
{
#if 0
	Entity         proto       = GetEntitySystem()->CreateEntity();

	Handle         model       = Graphics_LoadModel( path );

	CRenderable_t& renderComp  = GetEntitySystem()->AddComponent< CRenderable_t >( proto );
	renderComp.aHandle         = Graphics_CreateRenderable( model );

	Transform& transform       = GetEntitySystem()->AddComponent< Transform >( proto );

	auto       playerTransform = GetEntitySystem()->GetComponent< Transform >( gLocalPlayer );

	transform.aPos             = playerTransform.aPos;
	transform.aScale           = { vrcmdl_scale.GetFloat(), vrcmdl_scale.GetFloat(), vrcmdl_scale.GetFloat() };

	g_protos.push_back( proto );
#endif
}

void CreateModelEntity( const std::string& path )
{
#if 0
	Entity physEnt = GetEntitySystem()->CreateEntity();

	Model* model   = Graphics_LoadModel( path );
	GetEntitySystem()->AddComponent< Model* >( physEnt, model );
	Transform& transform = GetEntitySystem()->AddComponent< Transform >( physEnt );

	Transform& playerTransform = GetEntitySystem()->GetComponent< Transform >( game->aLocalPlayer );
	transform = playerTransform;

	g_staticEnts.push_back( physEnt );
#endif
}

void CreatePhysEntity( const std::string& path )
{
#if 0
	Entity         physEnt    = GetEntitySystem()->CreateEntity();

	Handle         model      = Graphics_LoadModel( path );

	CRenderable_t& renderComp = GetEntitySystem()->AddComponent< CRenderable_t >( physEnt );
	renderComp.aHandle        = Graphics_CreateRenderable( model );

	PhysicsShapeInfo shapeInfo( PhysShapeType::Convex );

	Phys_GetModelVerts( model, shapeInfo.aConvexData );

	IPhysicsShape* shape = physenv->CreateShape( shapeInfo );

	if ( shape == nullptr )
	{
		Log_ErrorF( "Failed to create physics shape for model: \"%s\"\n", path.c_str() );
		return;
	}

	Transform&        transform = GetEntitySystem()->GetComponent< Transform >( gLocalPlayer );

	PhysicsObjectInfo physInfo;
	physInfo.aPos         = transform.aPos;  // NOTE: THIS IS THE CENTER OF MASS
	physInfo.aAng         = transform.aAng;
	physInfo.aMass        = 1.f;
	physInfo.aMotionType  = PhysMotionType::Dynamic;
	physInfo.aStartActive = true;

	IPhysicsObject* phys  = physenv->CreateObject( shape, physInfo );
	phys->SetFriction( phys_friction );

	Phys_SetMaxVelocities( phys );

	GetEntitySystem()->AddComponent< IPhysicsShape* >( physEnt, shape );
	GetEntitySystem()->AddComponent< IPhysicsObject* >( physEnt, phys );

	g_otherEnts.push_back( physEnt );
#endif
}


CON_COMMAND( create_proto )
{
	CreateProtogen( DEFAULT_PROTOGEN_PATH );
}

CON_COMMAND( create_gltf_proto )
{
	CreateProtogen( "materials/models/protogen_wip_25d/protogen_25d.glb" );
}

static void model_dropdown(
  const std::vector< std::string >& args,  // arguments currently typed in by the user
  std::vector< std::string >&       results )    // results to populate the dropdown list with
{
	for ( const auto& file : FileSys_ScanDir( "materials", ReadDir_AllPaths | ReadDir_NoDirs | ReadDir_Recursive ) )
	{
		if ( file.ends_with( ".." ) )
			continue;

		if ( args.size() && !file.starts_with( args[ 0 ] ) )
			continue;

		// make sure it's a format we can open
		if ( file.ends_with( ".obj" ) || file.ends_with( ".glb" ) || file.ends_with( ".gltf" ) )
			results.push_back( file );
	}
}

CONCMD_DROP( create_look_entity, model_dropdown )
{
	if ( args.size() )
		CreateProtogen( args[ 0 ] );
}

CON_COMMAND( delete_protos )
{
	for ( auto& proto : g_protos )
	{
		// Graphics_FreeModel( GetEntitySystem()->GetComponent< Model* >( proto ) );
		// Graphics_FreeRenderable( GetEntitySystem()->GetComponent< CRenderable_t >( proto ).aHandle );
		// GetEntitySystem()->DeleteEntity( proto );
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


void TEST_Shutdown()
{
	for ( int i = 0; i < g_physEnts.size(); i++ )
	{
		if ( g_physEnts[ i ] )
			delete g_physEnts[ i ];

		g_physEnts[ i ] = nullptr;
	}
}


static ImVec2 ImVec2Mul( ImVec2 vec, float sScale )
{
	vec.x *= sScale;
	vec.y *= sScale;
	return vec;
}

static ImVec2 ImVec2MulMin( ImVec2 vec, float sScale, float sMin = 1.f )
{
	vec.x *= sScale;
	vec.y *= sScale;

	vec.x = glm::max( sMin, vec.x );
	vec.y = glm::max( sMin, vec.y );

	return vec;
}

static void ScaleImGui( float sScale )
{
	static ImGuiStyle baseStyle     = ImGui::GetStyle();

	ImGuiStyle&       style         = ImGui::GetStyle();

	style.ChildRounding             = baseStyle.ChildRounding * sScale;
	style.WindowRounding            = baseStyle.WindowRounding * sScale;
	style.PopupRounding             = baseStyle.PopupRounding * sScale;
	style.FrameRounding             = baseStyle.FrameRounding * sScale;
	style.IndentSpacing             = baseStyle.IndentSpacing * sScale;
	style.ColumnsMinSpacing         = baseStyle.ColumnsMinSpacing * sScale;
	style.ScrollbarSize             = baseStyle.ScrollbarSize * sScale;
	style.ScrollbarRounding         = baseStyle.ScrollbarRounding * sScale;
	style.GrabMinSize               = baseStyle.GrabMinSize * sScale;
	style.GrabRounding              = baseStyle.GrabRounding * sScale;
	style.LogSliderDeadzone         = baseStyle.LogSliderDeadzone * sScale;
	style.TabRounding               = baseStyle.TabRounding * sScale;
	style.MouseCursorScale          = baseStyle.MouseCursorScale * sScale;
	style.TabMinWidthForCloseButton = ( baseStyle.TabMinWidthForCloseButton != FLT_MAX ) ? ( baseStyle.TabMinWidthForCloseButton * sScale ) : FLT_MAX;

	style.WindowPadding             = ImVec2Mul( baseStyle.WindowPadding, sScale );
	style.WindowMinSize             = ImVec2MulMin( baseStyle.WindowMinSize, sScale );
	style.FramePadding              = ImVec2Mul( baseStyle.FramePadding, sScale );
	style.ItemSpacing               = ImVec2Mul( baseStyle.ItemSpacing, sScale );
	style.ItemInnerSpacing          = ImVec2Mul( baseStyle.ItemInnerSpacing, sScale );
	style.CellPadding               = ImVec2Mul( baseStyle.CellPadding, sScale );
	style.TouchExtraPadding         = ImVec2Mul( baseStyle.TouchExtraPadding, sScale );
	style.DisplayWindowPadding      = ImVec2Mul( baseStyle.DisplayWindowPadding, sScale );
	style.DisplaySafeAreaPadding    = ImVec2Mul( baseStyle.DisplaySafeAreaPadding, sScale );
}


void TEST_EntUpdate()
{
#if 0
	PROF_SCOPE();

	// blech
	for ( auto& ent : g_otherEnts )
	{
		CRenderable_t& renderComp = GetEntitySystem()->GetComponent< CRenderable_t >( ent );

		if ( Renderable_t* renderable = Graphics_GetRenderableData( renderComp.aHandle ) )
		{
			if ( renderable->aModel == InvalidHandle )
				continue;

			// Model *physObjList = &GetEntitySystem()->GetComponent< Model >( ent );

			// Transform& transform = GetEntitySystem()->GetComponent< Transform >( ent );
			IPhysicsObject* phys = GetEntitySystem()->GetComponent< IPhysicsObject* >( ent );

			if ( !phys )
				continue;

			phys->SetFriction( phys_friction );
			Util_ToMatrix( renderable->aModelMatrix, phys->GetPos(), phys->GetAng() );
		}
	}

#if 0
	// what
	if ( ImGui::Begin( "What" ) )
	{
		ImGuiStyle& style = ImGui::GetStyle();

		static float imguiScale = 1.f;

		if ( ImGui::SliderFloat( "Scale", &imguiScale, 0.25f, 4.f, "%.4f", 1.f ) )
		{
			ScaleImGui( imguiScale );
		}
	}

	ImGui::End();
#endif
#endif
}


#if 0
void TaskUpdateProtoLook( ftl::TaskScheduler *taskScheduler, void *arg )
{
	float protoScale = vrcmdl_scale;

	ProtoLookData_t* lookData = (ProtoLookData_t*)arg;
	
	// TODO: maybe make this into some kind of "look at player" component? idk lol
	// also could thread this as a test
	for ( auto& proto : lookData->aProtos )
	{
		DefaultRenderable* renderable = (DefaultRenderable*)GetEntitySystem()->GetComponent< RenderableHandle_t >( proto );
		auto& protoTransform = GetEntitySystem()->GetComponent< Transform >( proto );

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
void TEST_UpdateProtos( float frameTime )
{
#if 0
	PROF_SCOPE();

	auto& playerTransform = GetEntitySystem()->GetComponent< Transform >( gLocalPlayer );
	// auto& camTransform = GetEntitySystem()->GetComponent< CCamera >( gLocalPlayer ).aTransform;

	if ( !Game_IsPaused() )
	{
		if ( in_proto_spam.GetBool() )
		{
			CreateProtogen( DEFAULT_PROTOGEN_PATH );
		}
	}

	//transform.aPos += camTransform.aPos;
	//transform.aAng += camTransform.aAng;

	// ?????
	float protoScale = vrcmdl_scale;

	// funny temp
	struct ProtoTurn_t
	{
		double aNextTime;
		float  aLastRand;
		float  aRand;
	};

	static std::unordered_map< Entity, ProtoTurn_t > protoTurnMap;

	// TODO: maybe make this into some kind of "look at player" component? idk lol
	// also could thread this as a test
	for ( auto& proto : g_protos )
	{
		CRenderable_t& renderComp     = GetEntitySystem()->GetComponent< CRenderable_t >( proto );
		auto&          protoTransform = GetEntitySystem()->GetComponent< Transform >( proto );

		bool           matrixChanged  = false;

		Renderable_t*  renderable     = Graphics_GetRenderableData( renderComp.aHandle );

		if ( renderable == nullptr )
			continue;

		// glm::length( renderable->aModelMatrix ) == 0.f

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

			renderable->aModelMatrix = modelMatrix;
		}
		else if ( proto_look.GetBool() && !Game_IsPaused() )
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
			matrixChanged         = true;
		}

		if ( proto_follow.GetBool() && !Game_IsPaused() )
		{
			matrixChanged = true;

			glm::vec3 modelUp, modelRight, modelForward;
			Util_GetMatrixDirection( renderable->aModelMatrix, &modelForward, &modelRight, &modelUp );

			ProtoTurn_t& protoTurn = protoTurnMap[ proto ];

			if ( protoTurn.aNextTime <= gCurTime )
			{
				protoTurn.aNextTime = gCurTime + ( rand() / ( RAND_MAX / 8.f ) );
				protoTurn.aLastRand = protoTurn.aRand;
				// protoTurn.aRand     = ( rand() / ( RAND_MAX / 40.0f ) ) - 20.f;
				protoTurn.aRand     = ( rand() / ( RAND_MAX / 360.0f ) ) - 180.f;
			}

			float randTurn      = std::lerp( protoTurn.aLastRand, protoTurn.aRand, protoTurn.aNextTime - gCurTime );

			protoTransform.aPos = protoTransform.aPos + ( modelUp * proto_follow_speed.GetFloat() * gFrameTime );

			// protoTransform.aAng    = protoTransform.aAng + ( modelForward * randTurn * proto_follow_speed.GetFloat() * gFrameTime );
			protoTransform.aAng = protoTransform.aAng + ( modelRight * randTurn * proto_follow_speed.GetFloat() * gFrameTime );
		}

		if ( matrixChanged )
		{
			renderable->aModelMatrix = protoTransform.ToMatrix();
			Graphics_UpdateRenderableAABB( renderComp.aHandle );
		}

		// TEMP
		{
			glm::vec3 modelForward, modelRight, modelUp;
			Util_GetMatrixDirection( renderable->aModelMatrix, &modelForward, &modelRight, &modelUp );
			Graphics_DrawLine( protoTransform.aPos, protoTransform.aPos + ( modelForward * r_proto_line_dist.GetFloat() ), { 1.f, 0.f, 0.f } );
			Graphics_DrawLine( protoTransform.aPos, protoTransform.aPos + ( modelRight * r_proto_line_dist.GetFloat() ), { 0.f, 1.f, 0.f } );
			Graphics_DrawLine( protoTransform.aPos, protoTransform.aPos + ( modelUp * r_proto_line_dist.GetFloat() ), { 0.f, 0.f, 1.f } );
		}
	}
#endif
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

CONCMD( snd_test )
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

CONCMD( snd_test_clear )
{
	for ( Handle stream : streams )
	{
		if ( audio->IsValid( stream ) )
		{
			audio->FreeSound( stream );
		}
	}

	streams.clear();
}


void TEST_UpdateAudio()
{
	// auto& transform = GetEntitySystem()->GetComponent< Transform >( gLocalPlayer );

	for ( Handle stream : streams )
	{
		if ( audio->IsValid( stream ) )
		{
			audio->SetVolume( stream, snd_test_vol );
		}
	}
}


