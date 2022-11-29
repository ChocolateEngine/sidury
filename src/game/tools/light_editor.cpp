#include "../graphics/graphics.h"
#include "../main.h"
#include "light_editor.h"

#include "imgui/imgui.h"


static Handle                                             gModelLightPoint    = InvalidHandle;
static Handle                                             gModelLightCone     = InvalidHandle;

static bool                                               gLightEditorEnabled = true;

static std::unordered_map< Light_t*, Renderable_t >        gDrawLights;

static Light_t*                                           gpSelectedLight    = nullptr;

extern Handle                                             gLocalPlayer;

extern std::vector< Light_t* >                            gLights;

static Light_t*                                           gpFlashlight = nullptr;


CONVAR( r_light_line, 1 );
CONVAR( r_light_line_dist, 64.f );
CONVAR( r_light_line_dist2, 48.f );


void LightEditor_UpdateLightDraw( Light_t* spLight )
{
	// HACK
	if ( gpFlashlight == spLight )
		return;

	Renderable_t& modelDraw = gDrawLights[ spLight ];

	if ( spLight->aType == ELightType_Cone || spLight->aType == ELightType_Directional )
		modelDraw.aModel = gModelLightCone;
	else
		modelDraw.aModel = gModelLightPoint;

	// Dumb
	Transform transform{};
	transform.aPos = spLight->aPos;
	transform.aAng = spLight->aAng;

	if ( spLight->aType == ELightType_Directional )
	{
		transform.aScale       = { 2.f, 2.f, 2.f };
		modelDraw.aModelMatrix = transform.ToMatrix();
	}
	else
	{
		modelDraw.aModelMatrix = transform.ToMatrix( false );
	}
	
	// glm::vec3 modelForward, modelRight, modelUp;
	// Util_GetMatrixDirection( modelDraw.aModelMatrix, &modelForward, &modelRight, &modelUp );
	// Graphics_DrawLine( spLight->aPos, spLight->aPos + ( modelForward * r_light_line_dist2.GetFloat() ), { 1.f, 0.f, 0.f } );
	// Graphics_DrawLine( spLight->aPos, spLight->aPos + ( modelRight * r_light_line_dist2.GetFloat() ), { 0.f, 1.f, 0.f } );
	// Graphics_DrawLine( spLight->aPos, spLight->aPos + ( modelUp * r_light_line_dist2.GetFloat() ), { 0.f, 0.f, 1.f } );

	if ( r_light_line )
	{
		glm::vec3 forward;
		Util_GetDirectionVectors( spLight->aAng, &forward );
		Graphics_DrawLine( spLight->aPos, spLight->aPos + ( forward * r_light_line_dist.GetFloat() ), { spLight->aColor.x, spLight->aColor.y, spLight->aColor.z } );
	}
}


void LightEditor_UpdateAllLightDraws()
{
	// Load up all lights
	for ( const auto& light : gLights )
		LightEditor_UpdateLightDraw( light );
}


CONCMD( tool_light_editor )
{
	gLightEditorEnabled = !gLightEditorEnabled;

	if ( !gLightEditorEnabled )
		return;

	// LightEditor_UpdateAllLightDraws();
}


CONVAR( tool_light_editor_draw_lights, 1 );


void LightEditor_Init()
{
	// Load Light Models
	gModelLightPoint = Graphics_LoadModel( "tools/light_editor/light_point.glb" );
	gModelLightCone  = Graphics_LoadModel( "tools/light_editor/light_cone.glb" );
}


void LightEditor_Shutdown()
{
	Graphics_FreeModel( gModelLightPoint );
	Graphics_FreeModel( gModelLightCone );
}


void LightEditor_DrawEditor()
{
	if ( !ImGui::Begin( "Light Editor" ) )
	{
		ImGui::End();
		return;
	}

	auto& playerTransform = entities->GetComponent< Transform >( gLocalPlayer );
	auto& camTransform    = entities->GetComponent< CCamera >( gLocalPlayer ).aTransform;

	if ( ImGui::Button( "Create Directional Light" ) )
	{
		Light_t* light = Graphics_CreateLight( ELightType_Directional );

		Log_Dev( 1, "Created Directional Light\n" );
		// gLightsWorld
	}

	if ( ImGui::Button( "Create Point Light" ) )
	{
		Light_t* light      = Graphics_CreateLight( ELightType_Point );

		if ( light != nullptr )
		{
			light->aPos         = playerTransform.aPos + camTransform.aPos;
			// light->aColor       = { rand() % 1, rand() % 1, rand() % 1 };
			light->aColor       = { 1, 1, 1 };
			light->aRadius      = 500;
		}

		Log_Dev( 1, "Created Point Light\n" );
	}

	if ( ImGui::Button( "Create Cone Light" ) )
	{
		Light_t* light = Graphics_CreateLight( ELightType_Cone );

		if ( light != nullptr )
		{
			light->aPos    = playerTransform.aPos + camTransform.aPos;
			light->aColor  = { 10, 10, 10 };

			// weird stuff to get the angle of the light correct from the player's view matrix stuff
			light->aAng.x =  camTransform.aAng.z;
			light->aAng.y = -camTransform.aAng.y;
			light->aAng.z = -camTransform.aAng.x + 90.f;

			light->aInnerFov = 0.f;  // FOV
			light->aOuterFov = 45.f;  // FOV
		}

		Log_Dev( 1, "Created Cone Light\n" );
	}

	// Show list of lights
	// this REALLY NEEDS TO BE RETHOUGHT
	// public interface should probably have one data type
	// and then internally, use different UBO light types, not have the raw UBO exposed

	ImVec2 itemSize = ImGui::GetItemRectSize();
	itemSize.y *= gLights.size();

	if ( ImGui::BeginChild( "Lights", itemSize, true ) )
	{
		for ( size_t i = 0; i < gLights.size(); i++ )
		{
			const char* name = nullptr;

			switch ( gLights[ i ]->aType )
			{
				case ELightType_Directional:
					name = "Directional Light";
					break;

				case ELightType_Point:
					name = "Point Light";
					break;

				case ELightType_Cone:
					name = "Cone Light";
					break;

				case ELightType_Capsule:
					name = "Capsule Light";
					break;
			}

			ImGui::PushID( i );

			if ( ImGui::Selectable( name, gpSelectedLight == gLights[ i ] ) )
			{
				gpSelectedLight    = gLights[ i ];
			}

			ImGui::PopID();
		}
	}

	ImGui::EndChild();

	if ( !gpSelectedLight )
	{
		ImGui::End();
		return;
	}

	if ( ImGui::Button( "Delete Light" ) )
	{
		ImGui::End();
		Graphics_DestroyLight( gpSelectedLight );
		gpSelectedLight = nullptr;
		return;
	}

	if ( !ImGui::BeginChild( "Light Properties" ) )
	{
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	bool updateLight = false;

	// updateLight |= ImGui::ColorEdit3( "Color", &gpSelectedLight->aColor.x );
	updateLight |= ImGui::InputFloat3( "Color", &gpSelectedLight->aColor.x );

	if ( gpSelectedLight->aType == ELightType_Directional )
	{
		updateLight |= ImGui::SliderFloat( "Rotation X", &gpSelectedLight->aAng.x, -180, 180 );
		updateLight |= ImGui::SliderFloat( "Rotation Y", &gpSelectedLight->aAng.y, -180, 180 );
		updateLight |= ImGui::SliderFloat( "Rotation Z", &gpSelectedLight->aAng.z, -180, 180 );
	}
	else if ( gpSelectedLight->aType == ELightType_Point )
	{
		updateLight |= ImGui::InputFloat3( "Position", &gpSelectedLight->aPos.x );
		updateLight |= ImGui::SliderFloat( "Radius", &gpSelectedLight->aRadius, 0, 1000 );
	}
	else if ( gpSelectedLight->aType == ELightType_Cone )
	{
		// updateLight |= ImGui::ColorEdit3( "Color", &gpSelectedLight->aColor.x );

		updateLight |= ImGui::InputFloat3( "Position", &gpSelectedLight->aPos.x );

		updateLight |= ImGui::SliderFloat( "Rotation X", &gpSelectedLight->aAng.x, -180, 180 );
		updateLight |= ImGui::SliderFloat( "Rotation Y", &gpSelectedLight->aAng.y, -180, 180 );
		updateLight |= ImGui::SliderFloat( "Rotation Z", &gpSelectedLight->aAng.z, -180, 180 );

		updateLight |= ImGui::SliderFloat( "Inner FOV", &gpSelectedLight->aInnerFov, 0, 180 );
		updateLight |= ImGui::SliderFloat( "Outer FOV", &gpSelectedLight->aOuterFov, 0, 180 );
	}
	else if ( gpSelectedLight->aType == ELightType_Capsule )
	{
	}

	if ( updateLight )
		Graphics_UpdateLight( gpSelectedLight );

	ImGui::EndChild();
	ImGui::End();
}


void LightEditor_DrawLightModels()
{
	if ( !tool_light_editor_draw_lights )
		return;

	LightEditor_UpdateAllLightDraws();

	for ( auto& [ light, draw ] : gDrawLights )
	{
		// if ( gpFlashlight != light )
		// 	Graphics_DrawModel( &draw );
	}
}


void LightEditor_Update()
{
	PROF_SCOPE();

	gpFlashlight = entities->GetComponent< Light_t* >( gLocalPlayer );

	LightEditor_DrawLightModels();

	if ( !gLightEditorEnabled )
		return;

	if ( !Game_IsPaused() )
		return;

	LightEditor_DrawEditor();
}

