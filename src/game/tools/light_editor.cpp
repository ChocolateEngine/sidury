#include "igraphics.h"
#include "../main.h"
#include "../cl_main.h"
#include "../entity_systems.h"
#include "../player.h"
#include "light_editor.h"

#include "imgui/imgui.h"


static Handle                                 gModelLightPoint    = InvalidHandle;
static Handle                                 gModelLightCone     = InvalidHandle;

static bool                                   gLightEditorEnabled = false;

static std::unordered_map< Light_t*, Handle > gDrawLights;

static Light_t*                               gpSelectedLight = nullptr;

extern Handle                                 gLocalPlayer;

// extern std::vector< Light_t* >                gLights;

constexpr const char*                         gModelLightPointPath = "tools/light_editor/light_point.glb";
constexpr const char*                         gModelLightConePath  = "tools/light_editor/light_cone.glb";


CONVAR( r_light_line, 1 );
CONVAR( r_light_line_dist, 64.f );
CONVAR( r_light_line_dist2, 48.f );


static bool IsFlashlight( Light_t* spLight )
{
	if ( !GetLightEntSys() )
		return false;

	for ( Entity entity : GetLightEntSys()->aEntities )
	{
		auto playerInfo = Ent_GetComponent< CPlayerInfo >( entity, "playerInfo" );
		if ( !playerInfo )
		{
			// Not a player, so not a flashlight
			continue;
		}

		auto lightComp = Ent_GetComponent< CLight >( entity, "light" );
		if ( !lightComp )
		{
			Log_Warn( "?????????\n" );
			continue;
		}

		if ( lightComp->apLight == spLight )
			return true;
	}

	return false;
}


static Handle LightEditor_CreateRenderable( Light_t* spLight )
{
	// HACK
	if ( IsFlashlight( spLight ) )
		return InvalidHandle;

	auto it = gDrawLights.find( spLight );

	if ( it == gDrawLights.end() )
	{
		// create a new renderable
		Handle model = gModelLightPoint;
		if ( spLight->aType == ELightType_Cone || spLight->aType == ELightType_Directional )
			model = gModelLightCone;

		Handle renderHandle = graphics->CreateRenderable( model );

		if ( !renderHandle )
		{
			Log_Error( "Failed to create Renderable for light\n" );
			return InvalidHandle;
		}
		
		gDrawLights[ spLight ] = renderHandle;
		return renderHandle;
	}

	return it->second;
}


static void LightEditor_DestroyRenderable( Light_t* spLight )
{
	// HACK
	if ( IsFlashlight( spLight ) )
		return;

	auto it = gDrawLights.find( spLight );

	if ( it != gDrawLights.end() )
	{
		// destroy this renderable
		graphics->FreeRenderable( it->second );
		gDrawLights.erase( it );
	}
}


static void LightEditor_CreateRenderables()
{
	//gDrawLights.reserve( gLights.size() );
	//for ( const auto& light : gLights )
	//{
	//	LightEditor_CreateRenderable( light );
	//}
}


static void LightEditor_DestroyRenderables()
{
	for ( const auto& [light, renderable] : gDrawLights )
	{
		// HACK
		if ( IsFlashlight( light ) )
			continue;

		// destroy this renderable
		graphics->FreeRenderable( renderable );
	}

	gDrawLights.clear();
}


void LightEditor_UpdateLightDraw( Light_t* spLight )
{
	// HACK
	if ( IsFlashlight( spLight ) )
		return;

	Handle renderHandle = LightEditor_CreateRenderable( spLight );

	if ( !renderHandle )
	{
		Log_Error( "Failed to get Renderable for light\n" );
		return;
	}

	Renderable_t* renderable = graphics->GetRenderableData( renderHandle );
	
	if ( !renderable )
	{
		Log_Error( "Renderable data for light is nullptr\n" );
		return;
	}

	graphics->UpdateRenderableAABB( renderHandle );

	renderable->aCastShadow = false;

	if ( spLight->aType == ELightType_Directional )
	{
		glm::vec3 scale = { 2.f, 2.f, 2.f };
		Util_ToMatrix( renderable->aModelMatrix, &spLight->aPos, &spLight->aRot, &scale );
	}
	else
	{
		Util_ToMatrix( renderable->aModelMatrix, &spLight->aPos, &spLight->aRot );
	}

	if ( spLight == gpSelectedLight )
	{
		graphics->DrawBBox( renderable->aAABB.aMin, renderable->aAABB.aMax, { 1.0, 0.5, 1.0 } );
	}

	if ( r_light_line && spLight->aType != ELightType_Point )
	{
		glm::vec3 forward;
		Util_GetMatrixDirection( renderable->aModelMatrix, nullptr, nullptr, &forward );
		graphics->DrawLine( spLight->aPos, spLight->aPos + ( -forward * r_light_line_dist.GetFloat() ), { spLight->aColor.x, spLight->aColor.y, spLight->aColor.z } );
	}
}


void LightEditor_UpdateAllLightDraws()
{
	// Load up all lights
	//for ( const auto& light : gLights )
	//	LightEditor_UpdateLightDraw( light );
}


CONVAR_CMD( tool_light_editor_draw_lights, 1 )
{
	if ( !gLightEditorEnabled )
		return;

	if ( tool_light_editor_draw_lights.GetBool() )
		LightEditor_CreateRenderables();
	else
		LightEditor_DestroyRenderables();
}


CONCMD( tool_light_editor )
{
	gLightEditorEnabled = !gLightEditorEnabled;

	if ( !gLightEditorEnabled )
	{
		LightEditor_DestroyRenderables();
		return;
	}

	if ( tool_light_editor_draw_lights.GetBool() )
		LightEditor_CreateRenderables();

	// LightEditor_UpdateAllLightDraws();
}


bool LightEditor_Init()
{
	// Load Light Models
	gModelLightPoint = graphics->LoadModel( gModelLightPointPath );

	if ( gModelLightPoint == InvalidHandle )
	{
		Log_ErrorF( "Failed to load point light model for light editor: %s\n", gModelLightPointPath );
		return false;
	}

	gModelLightCone = graphics->LoadModel( gModelLightConePath );

	if ( gModelLightCone == InvalidHandle )
	{
		Log_ErrorF( "Failed to load cone light model for light editor: %s\n", gModelLightConePath );
		return false;
	}

	return true;
}


void LightEditor_Shutdown()
{
	graphics->FreeModel( gModelLightPoint );
	graphics->FreeModel( gModelLightCone );
}


void LightEditor_DrawEditor()
{
	if ( !ImGui::Begin( "Light Editor" ) )
	{
		ImGui::End();
		return;
	}

	auto playerInfo      = Ent_GetComponent< CPlayerInfo >( gLocalPlayer, "playerInfo" );
	auto playerTransform = Ent_GetComponent< CTransform >( gLocalPlayer, "transform" );
	//auto camTransform    = Ent_GetComponent< CTransform >( playerInfo->aCamera, "transform" );

	if ( !playerInfo )
		return;

	if ( playerInfo->aCamera.Get() == CH_ENT_INVALID )
		return;

	if ( !playerTransform )
		return;

	//if ( !camTransform )
	//	return;

	Transform playerWorldTransform = GetEntitySystem()->GetWorldTransform( playerInfo->aCamera );

	if ( ImGui::Button( "Create Directional Light" ) )
	{
		Light_t* light = graphics->CreateLight( ELightType_Directional );

		Log_Dev( 1, "Created Directional Light\n" );
		// gLightsWorld
	}

	if ( ImGui::Button( "Create Point Light" ) )
	{
		Light_t* light = graphics->CreateLight( ELightType_Point );

		if ( light != nullptr )
		{
			light->aPos         = playerWorldTransform.aPos;
			// light->aColor       = { rand() % 1, rand() % 1, rand() % 1 };
			light->aColor       = { 1, 1, 1, 1 };
			light->aRadius      = 500;
		}

		Log_Dev( 1, "Created Point Light\n" );
	}

	if ( ImGui::Button( "Create Cone Light" ) )
	{
		Light_t* light = graphics->CreateLight( ELightType_Cone );

		if ( light != nullptr )
		{
			light->aPos      = playerWorldTransform.aPos;
			light->aColor    = { 1, 1, 1, 10 };

			// weird stuff to get the angle of the light correct from the player's view matrix stuff
			// light->aAng.x    = playerWorldTransform.aAng.z;
			// light->aAng.y    = -playerWorldTransform.aAng.y;
			// light->aAng.z    = -playerWorldTransform.aAng.x + 90.f;

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
	//itemSize.y *= gLights.size();

	const ImGuiStyle& style = ImGui::GetStyle();

	itemSize.x += ( style.FramePadding.x + style.ItemInnerSpacing.x ) * 2;
	itemSize.y += ( style.FramePadding.y + style.ItemInnerSpacing.y ) * 2;

	//if ( ImGui::BeginChild( "Lights", itemSize, true ) )
	//{
	//	for ( size_t i = 0; i < gLights.size(); i++ )
	//	{
	//		const char* name = nullptr;
	//
	//		switch ( gLights[ i ]->aType )
	//		{
	//			case ELightType_Directional:
	//				name = "Directional Light";
	//				break;
	//
	//			case ELightType_Point:
	//				name = "Point Light";
	//				break;
	//
	//			case ELightType_Cone:
	//				name = "Cone Light";
	//				break;
	//
	//			//case ELightType_Capsule:
	//			//	name = "Capsule Light";
	//			//	break;
	//		}
	//
	//		ImGui::PushID( i );
	//
	//		if ( ImGui::Selectable( name, gpSelectedLight == gLights[ i ] ) )
	//		{
	//			gpSelectedLight = gLights[ i ];
	//		}
	//
	//		ImGui::PopID();
	//	}
	//}
	//
	//ImGui::EndChild();

	if ( !gpSelectedLight )
	{
		ImGui::End();
		return;
	}

	if ( ImGui::Button( "Delete Light" ) )
	{
		ImGui::End();
		LightEditor_DestroyRenderable( gpSelectedLight );
		graphics->DestroyLight( gpSelectedLight );
		gpSelectedLight = nullptr;
		return;
	}

	if ( !ImGui::BeginChild( "Light Properties", {}, true ) )
	{
		ImGui::EndChild();
		ImGui::End();
		return;
	}

	bool updateLight = false;

	updateLight |= ImGui::ColorEdit4( "Color", &gpSelectedLight->aColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR );

	if ( gpSelectedLight->aType == ELightType_Directional )
	{
		//updateLight |= ImGui::SliderFloat( "Rotation X", &gpSelectedLight->aAng.x, -180, 180 );
		//updateLight |= ImGui::SliderFloat( "Rotation Y", &gpSelectedLight->aAng.y, -180, 180 );
		//updateLight |= ImGui::SliderFloat( "Rotation Z", &gpSelectedLight->aAng.z, -180, 180 );
	}
	else if ( gpSelectedLight->aType == ELightType_Point )
	{
		updateLight |= ImGui::DragScalarN( "Position", ImGuiDataType_Float, &gpSelectedLight->aPos.x, 3, 1.f, nullptr, nullptr, nullptr, 1.f );
		updateLight |= ImGui::SliderFloat( "Radius", &gpSelectedLight->aRadius, 0, 1000 );
	}
	else if ( gpSelectedLight->aType == ELightType_Cone )
	{
		updateLight |= ImGui::DragScalarN( "Position", ImGuiDataType_Float, &gpSelectedLight->aPos.x, 3, 1.f, nullptr, nullptr, nullptr, 1.f );

		//updateLight |= ImGui::SliderFloat( "Rotation X", &gpSelectedLight->aAng.x, -180, 180 );
		//updateLight |= ImGui::SliderFloat( "Rotation Y", &gpSelectedLight->aAng.y, -180, 180 );
		//updateLight |= ImGui::SliderFloat( "Rotation Z", &gpSelectedLight->aAng.z, -180, 180 );

		updateLight |= ImGui::SliderFloat( "Inner FOV", &gpSelectedLight->aInnerFov, 0, 180 );
		updateLight |= ImGui::SliderFloat( "Outer FOV", &gpSelectedLight->aOuterFov, 0, 180 );
	}
	//else if ( gpSelectedLight->aType == ELightType_Capsule )
	//{
	//}

	if ( updateLight )
		graphics->UpdateLight( gpSelectedLight );

	ImGui::EndChild();
	ImGui::End();
}


void LightEditor_Update()
{
	PROF_SCOPE();

	if ( !tool_light_editor_draw_lights )
		return;

	LightEditor_UpdateAllLightDraws();
}


void LightEditor_DrawUI()
{
	PROF_SCOPE();

	if ( !gLightEditorEnabled )
		return;

	if ( !CL_IsMenuShown() )
		return;

	LightEditor_DrawEditor();
}

