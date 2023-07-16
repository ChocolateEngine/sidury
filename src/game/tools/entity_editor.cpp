#include "entity_editor.h"
#include "../entity.h"

#include "imgui/imgui.h"


static bool   gToolEntEditorEnabled = false;
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
		case EEntNetField_Invalid:
			return false;

		case EEntNetField_Bool:
		{
			auto value = static_cast< bool* >( spData );
			return ImGui::Checkbox( srVarData.apName, value );
		}

		case EEntNetField_Float:
		{
			auto value = static_cast< float* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Float, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntNetField_Double:
		{
			auto value = static_cast< double* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Double, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}

		case EEntNetField_S8:
		{
			auto value = static_cast< s8* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_S8, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntNetField_S16:
		{
			auto value = static_cast< s16* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_S16, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntNetField_S32:
		{
			auto value = static_cast< s32* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_S32, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntNetField_S64:
		{
			auto value = static_cast< s64* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_S64, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}

		case EEntNetField_U8:
		{
			auto value = static_cast< u8* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_U8, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntNetField_U16:
		{
			auto value = static_cast< u16* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_U16, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntNetField_U32:
		{
			auto value = static_cast< u32* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_U32, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntNetField_U64:
		{
			auto value = static_cast< u64* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_U64, value, 1, 1.f, nullptr, nullptr, nullptr, 1.f );
		}

		case EEntNetField_StdString:
		{
			auto value = static_cast< std::string* >( spData );
			value->reserve( 512 );
			bool enterPressed = ImGui::InputText( srVarData.apName, value->data(), 512, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackAlways, &EntEditor_StdStringCallback, value );
			return enterPressed;
		}

		case EEntNetField_Vec2:
		{
			auto value = static_cast< glm::vec2* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Float, &value->x, 2, 0.25f, nullptr, nullptr, nullptr, 1.f );

		}
		case EEntNetField_Vec3:
		{
			auto value = static_cast< glm::vec3* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Float, &value->x, 3, 0.25f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntNetField_Vec4:
		{
			auto value = static_cast< glm::vec4* >( spData );
			return ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Float, &value->x, 4, 0.25f, nullptr, nullptr, nullptr, 1.f );
		}
		case EEntNetField_Quat:
		{
			auto value = static_cast< glm::quat* >( spData );
			glm::vec3 euler = glm::degrees( glm::eulerAngles( *value ) );

			bool modified = ImGui::DragScalarN( srVarData.apName, ImGuiDataType_Float, &euler.x, 3, 0.25f, nullptr, nullptr, nullptr, 1.f );

			if ( modified )
			{
				*value = glm::radians( euler );
			}

			return modified;
		}
		
		case EEntNetField_Color3:
		{
			auto value = static_cast< glm::vec3* >( spData );
			return ImGui::ColorEdit3( srVarData.apName, &value->x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR );
		}
		case EEntNetField_Color4:
		{
			auto value = static_cast< glm::vec4* >( spData );
			return ImGui::ColorEdit4( srVarData.apName, &value->x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR );
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

	// TODO: make this create entity stuff into a context menu with plenty of presets
	// also, create a duplicate entity option when selected on one, and move delete entity and clear parent to that

	if ( ImGui::Button( "Create" ) )
	{
		gSelectedEntity = GetEntitySystem()->CreateEntity();
	}

	if ( ImGui::Button( "Create At Camera Position" ) )
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

	if ( ImGui::Button( "Create At Camera" ) )
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

	if ( ImGui::Button( "Delete" ) )
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

	const ImVec2      itemSize   = ImGui::GetItemRectSize();
	const float       textHeight = ImGui::GetTextLineHeight();
	const ImGuiStyle& style      = ImGui::GetStyle();

	if ( ImGui::BeginCombo( "Parent", "" ) )
	{
		Entity parent = GetEntitySystem()->GetParent( gSelectedEntity );

		for ( auto& [ entity, flags ] : GetEntitySystem()->aEntityFlags )
		{
			if ( entity == gSelectedEntity )
				continue;

			if ( parent == entity )
				continue;

			std::string entName = vstring( "Entity %zd", entity );
			if ( ImGui::Selectable( entName.c_str() ) )
			{
				GetEntitySystem()->ParentEntity( gSelectedEntity, entity );
			}
		}

		ImGui::EndCombo();
	}

	// Entity List
	if ( ImGui::BeginChild( "Entity List", {}, true ) )
	{
		for ( auto& [ entity, flags ] : GetEntitySystem()->aEntityFlags )
		{
			std::string entName = vstring( "Entity %zd", entity );

			ImGui::PushID( entity );

			if ( ImGui::Selectable( entName.c_str(), gSelectedEntity == entity ) )
			{
				gSelectedEntity = entity;
			}

			ImGui::PopID();
		}
	}

	ImGui::EndChild();
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
		for ( auto& [ name, regData ] : GetEntComponentRegistry().aComponentNames )
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

	if ( ImGui::BeginChild( "Entity Component Data" ) )
	{
		for ( auto& [name, regData] : GetEntComponentRegistry().aComponentNames )
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
					ImGui::PopID();
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
						if ( varData.aFlags & ECompRegFlag_IsNetVar )
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
	}

	ImGui::EndChild();

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

