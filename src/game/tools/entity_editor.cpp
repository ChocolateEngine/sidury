#include "entity_editor.h"
#include "../entity.h"

#include "imgui/imgui.h"


static bool   gToolEntEditorEnabled = true;
static Entity gSelectedEntity       = CH_ENT_INVALID;
extern Handle gLocalPlayer;


CONCMD( tool_ent_editor )
{
	gToolEntEditorEnabled = !gToolEntEditorEnabled;
}


bool EntEditor_Init()
{
	return true;
}


void EntEditor_Shutdown()
{
}


void EntEditor_Update( float sFrameTime )
{
	if ( !gToolEntEditorEnabled )
		return;

	if ( !GetEntitySystem() )
		return;
}


static int EntEditor_StdStringCallback( ImGuiInputTextCallbackData* data )
{
	auto value = static_cast< std::string* >( data->UserData );
	value->resize( data->BufTextLen );
	return 0;
}


// Returns true if the value changed
bool EntEditor_DrawComponentVarUI( void* spData, EntComponentVarData_t& srVarData )
{
	switch ( srVarData.aType )
	{
		default:
		case EEntComponentVarType_Invalid:
			return false;

		case EEntComponentVarType_Bool:
		{
			auto value = static_cast< bool* >( spData );
			return ImGui::Checkbox( srVarData.apName, value );
		}

		case EEntComponentVarType_Float:
		{
			auto value = static_cast< float* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Float, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntComponentVarType_Double:
		{
			auto value = static_cast< double* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Double, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}

		case EEntComponentVarType_S8:
		{
			auto value = static_cast< s8* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_S8, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntComponentVarType_S16:
		{
			auto value = static_cast< s16* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_S16, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntComponentVarType_S32:
		{
			auto value = static_cast< s32* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_S32, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntComponentVarType_S64:
		{
			auto value = static_cast< s64* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_S64, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}

		case EEntComponentVarType_U8:
		{
			auto value = static_cast< u8* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_U8, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntComponentVarType_U16:
		{
			auto value = static_cast< u16* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_U16, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntComponentVarType_U32:
		{
			auto value = static_cast< u32* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_U32, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntComponentVarType_U64:
		{
			auto value = static_cast< u64* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_U64, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}

		case EEntComponentVarType_StdString:
		{
			auto value = static_cast< std::string* >( spData );
			value->reserve( 512 );
			bool enterPressed = ImGui::InputText( srVarData.apName, value->data(), 512, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways, &EntEditor_StdStringCallback, value );
			return enterPressed;
		}

		case EEntComponentVarType_Vec2:
		{
			auto value = static_cast< glm::vec2* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Float, &value->x, 2, 1.f, nullptr, nullptr, nullptr, 1.f );

		}
		case EEntComponentVarType_Vec3:
		{
			auto value = static_cast< glm::vec3* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Float, &value->x, 3, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntComponentVarType_Vec4:
		{
			auto value = static_cast< glm::vec4* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Float, &value->x, 4, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
	}

	return false;
}


void EntEditor_DrawEntityList()
{
	if ( !ImGui::Begin( "Entity List" ) )
	{
		ImGui::End();
		return;
	}

	if ( ImGui::Button( "Create Entity" ) )
	{
		gSelectedEntity = GetEntitySystem()->CreateEntity();
	}

	if ( ImGui::Button( "Create Entity At Camera Position" ) )
	{
		gSelectedEntity = GetEntitySystem()->CreateEntity();

		// Automatically add the transform in and copy the position from the local player
		auto playerTransform = Ent_GetComponent< CTransform >( gLocalPlayer, "transform" );

		if ( !playerTransform )
			return;

		auto transform = Ent_AddComponent< CTransform >( gSelectedEntity, "transform" );
		if ( transform )
		{
			transform->aPos.Edit() = playerTransform->aPos;
		}
	}

	if ( ImGui::Button( "Create Entity At Camera" ) )
	{
		gSelectedEntity = GetEntitySystem()->CreateEntity();

		// Automatically add the transform in and copy the position from the local player
		auto playerTransform = Ent_GetComponent< CTransform >( gLocalPlayer, "transform" );

		if ( !playerTransform )
			return;

		auto transform = Ent_AddComponent< CTransform >( gSelectedEntity, "transform" );
		if ( transform )
		{
			transform->aPos.Edit() = playerTransform->aPos;
			transform->aAng.Edit() = playerTransform->aAng;
		}
	}

	if ( ImGui::Button( "Delete Entity" ) )
	{
		if ( gSelectedEntity )
			GetEntitySystem()->DeleteEntity( gSelectedEntity );

		gSelectedEntity = CH_ENT_INVALID;
	}

	if ( ImGui::Button( "Clear Parent" ) )
	{
		if ( gSelectedEntity )
			GetEntitySystem()->ParentEntity( gSelectedEntity, CH_ENT_INVALID );
	}

	ImGui::Separator();

	for ( Entity entity : GetEntitySystem()->aUsedEntities )
	{
		std::string entName = vstring( "Entity %zd", entity );

		ImGui::PushID( entity );

		if ( ImGui::Selectable( entName.c_str(), gSelectedEntity == entity ) )
		{
			gSelectedEntity = entity;
		}

		ImGui::PopID();
	}

	if ( ImGui::BeginCombo( "Parent Entity", "" ) )
	{
		for ( Entity entity : GetEntitySystem()->aUsedEntities )
		{
			if ( entity == gSelectedEntity )
				continue;

			std::string entName = vstring( "Entity %zd", entity );
			if ( ImGui::Selectable( entName.c_str() ) )
			{
				GetEntitySystem()->ParentEntity( gSelectedEntity, entity );
			}
		}

		ImGui::EndCombo();
	}

	ImGui::End();
}


void EntEditor_DrawEntityData()
{
	if ( !ImGui::Begin( "Entity Component List" ) )
	{
		ImGui::End();
		return;
	}

	if ( gSelectedEntity == CH_ENT_INVALID )
	{
		ImGui::End();
		return;
	}

	// TODO: is there anything in imgui that would allow me to search through the components list, and have an optional combo box?
	// you might be able to hack it together if you add in an arrow button yourself and InputText()
	if ( ImGui::BeginCombo( "Add Component", "transform" ) )
	{
		for ( auto& [ name, regData ] : gEntComponentRegistry.aComponentNames )
		{
			if ( GetEntitySystem()->HasComponent( gSelectedEntity, name.data() ) )
				continue;

			if ( ImGui::Selectable( name.data() ) )
			{
				GetEntitySystem()->AddComponent( gSelectedEntity, name.data() );
			}
		}

		ImGui::EndCombo();
	}

	ImGui::Separator();

	// go through the component registry and check to see if the entity has this component (probably slow af)
	size_t imguiID = 0;

	for ( auto& [name, regData] : gEntComponentRegistry.aComponentNames )
	{
		void* compData = GetEntitySystem()->GetComponent( gSelectedEntity, name.data() );

		if ( !compData )
			continue;

		// TreeNode

		IEntityComponentSystem* system = GetEntitySystem()->GetComponentSystem( name.data() );

		ImGui::PushID( regData );

		if ( ImGui::CollapsingHeader( name.data(), ImGuiTreeNodeFlags_None ) )
		{
			if ( ImGui::Button( "Remove Component" ) )
			{
				GetEntitySystem()->RemoveComponent( gSelectedEntity, name.data() );
				continue;
			}

			// Draw Component Data
			// TODO: allow a custom function to be registered for editing component data in imgui
			for ( auto& [varOffset, varData] : regData->aVars )
			{
				char* dataChar   = static_cast< char* >( compData );
				char* dataOffset = dataChar + varOffset;

				if ( EntEditor_DrawComponentVarUI( dataOffset, varData ) && system )
				{
					// Mark variable as dirty
					if ( varData.aIsNetVar )
					{
						char* dataChar = static_cast< char* >( compData );
						bool* isDirty  = reinterpret_cast< bool* >( dataChar + varData.aSize + varOffset );
						*isDirty       = true;
					}

					system->ComponentUpdated( gSelectedEntity, compData );
				}
			}
		}

		ImGui::PopID();
	}

	ImGui::End();
}


void EntEditor_DrawUI()
{
	if ( !gToolEntEditorEnabled )
		return;

	if ( !GetEntitySystem() )
		return;

	EntEditor_DrawEntityList();
	EntEditor_DrawEntityData();
}

