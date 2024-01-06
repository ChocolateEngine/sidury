#include "entity_editor.h"
#include "entity.h"
#include "main.h"

#include "imgui/imgui.h"

#include <unordered_set>


static ChHandle_t gSelectedEntity       = CH_INVALID_HANDLE;


// blech
static std::string gModelSelectPath = "";
static bool gInModelSelect = false;

bool EntEditor_Init()
{
	return true;
}


void EntEditor_Shutdown()
{
}


void EntEditor_Update( float sFrameTime )
{
}


static int EntEditor_StdStringCallback( ImGuiInputTextCallbackData* data )
{
	auto value = static_cast< std::string* >( data->UserData );
	value->resize( data->BufTextLen );
	return 0;
}


#if 0
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
#endif


void EntEditor_DrawLightUI( Entity_t* spEntity )
{
	if ( !ImGui::BeginChild( "Light Data" ) )
	{
		ImGui::EndChild();
		return;
	}

	bool updateLight = false;

	updateLight |= ImGui::ColorEdit4( "Color", &spEntity->apLight->aColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR );

	if ( spEntity->apLight->aType == ELightType_Directional )
	{
		// updateLight |= ImGui::SliderFloat( "Rotation X", &spEntity->apLight->aAng.x, -180, 180 );
		// updateLight |= ImGui::SliderFloat( "Rotation Y", &spEntity->apLight->aAng.y, -180, 180 );
		// updateLight |= ImGui::SliderFloat( "Rotation Z", &spEntity->apLight->aAng.z, -180, 180 );
	}
	else if ( spEntity->apLight->aType == ELightType_Point )
	{
		updateLight |= ImGui::DragScalarN( "Position", ImGuiDataType_Float, &spEntity->apLight->aPos.x, 3, 1.f, nullptr, nullptr, nullptr, 1.f );
		updateLight |= ImGui::SliderFloat( "Radius", &spEntity->apLight->aRadius, 0, 1000 );
	}
	else if ( spEntity->apLight->aType == ELightType_Cone )
	{
		// updateLight |= ImGui::DragScalarN( "Position", ImGuiDataType_Float, &spEntity->apLight->aPos.x, 3, 1.f, nullptr, nullptr, nullptr, 1.f );

		// updateLight |= ImGui::SliderFloat( "Rotation X", &spEntity->apLight->aAng.x, -180, 180 );
		// updateLight |= ImGui::SliderFloat( "Rotation Y", &spEntity->apLight->aAng.y, -180, 180 );
		// updateLight |= ImGui::SliderFloat( "Rotation Z", &spEntity->apLight->aAng.z, -180, 180 );

		updateLight |= ImGui::SliderFloat( "Inner FOV", &spEntity->apLight->aInnerFov, 0, 180 );
		updateLight |= ImGui::SliderFloat( "Outer FOV", &spEntity->apLight->aOuterFov, 0, 180 );
	}
	//else if ( spEntity->apLight->aType == ELightType_Capsule )
	//{
	//}

	// if ( updateLight )
		graphics->UpdateLight( spEntity->apLight );

	ImGui::EndChild();
}


#define NAME_LEN 64


static int EntityNameInput( ImGuiInputTextCallbackData* data )
{
	Entity_t* entity = (Entity_t*)data->UserData;
	entity->aName.resize( data->BufTextLen );

	return 0;
}


// return true if model selected
std::string EntEditor_DrawModelSelectionWindow()
{
	if ( !ImGui::Begin( "Model Selection Window" ) )
	{
		ImGui::End();
		return "";
	}

	if ( ImGui::Button( "Close" ) )
	{
		gInModelSelect = false;
		gModelSelectPath.clear();
	}

	auto fileList = FileSys_ScanDir( gModelSelectPath, ReadDir_AbsPaths );

	for ( std::string_view file : fileList )
	{
		bool isDir = FileSys_IsDir( file.data(), true );

		if ( !isDir )
		{
			bool model = file.ends_with( ".obj" );
			model |= file.ends_with( ".gltf" );
			model |= file.ends_with( ".glb" );

			if ( !model )
				continue;
		}

		if ( ImGui::Selectable( file.data() ) )
		{
			if ( isDir )
			{
				gModelSelectPath = FileSys_CleanPath( file );
			}
			else
			{
				ImGui::End();
				return file.data();
			}
		}
	}

	ImGui::End();

	return "";
}


void EntEditor_DrawRenderableUI( Entity_t* spEntity )
{
	if ( spEntity->aModel )
	{
		std::string_view modelPath = graphics->GetModelPath( spEntity->aModel );
		ImGui::Text( modelPath.data() );

		// ImVec2 rect = ImGui::GetItemRectSize();
		//
		// ImGui::IsMouseHoveringRect();
		//
		// ImGui::BeginTooltip();
		// ImGui::Text( modelPath.data() );
		// ImGui::EndTooltip();
	}

	if ( ImGui::Button( "Load Model" ) )
	{
		gInModelSelect = true;

		if ( spEntity->aModel )
		{
			std::string_view modelPath = graphics->GetModelPath( spEntity->aModel );
			gModelSelectPath           = FileSys_GetDirName( modelPath );
		}
		else
		{
			gModelSelectPath = FileSys_GetExePath();
		}
	}

	if ( spEntity->aRenderable )
	{
		Renderable_t* renderable = graphics->GetRenderableData( spEntity->aRenderable );

		ImGui::Checkbox( "Visible", &renderable->aVisible );
		ImGui::Checkbox( "Test Visibility", &renderable->aTestVis );
		ImGui::Checkbox( "Cast Shadows", &renderable->aCastShadow );

		// List Materials
		if ( ImGui::CollapsingHeader( "Materials" ) )
		{
			ImGui::Text( "Materials: %d", renderable->aMaterialCount );
			for ( u32 matI = 0; matI < renderable->aMaterialCount; matI++ )
			{
			}
		}

		// Do we have blend shapes?
		if ( ImGui::CollapsingHeader( "Blend Shapes" ) && renderable->aBlendShapeWeights.size() )
		{
			bool resetBlendShapes = ImGui::Button( "Reset Blend Shapes" );

			// Display Them (TODO: names)

			if ( ImGui::BeginChild( "Blend Shapes", { 0, 200 }, true ) )
			{
				u32 imguiID = 0;
				for ( u32 i = 0; i < renderable->aBlendShapeWeights.size(); i++ )
				{
					ImGui::PushID( imguiID++ );

					if ( ImGui::SliderFloat( "##blend_shape", &renderable->aBlendShapeWeights[ i ], -1.f, 4.f, "%.4f", 1.f ) )
					{
						renderable->aBlendShapesDirty = true;
					}

					ImGui::PopID();
					ImGui::SameLine();
					ImGui::PushID( imguiID++ );

					if ( ImGui::Button( "Reset" ) || resetBlendShapes )
					{
						renderable->aBlendShapeWeights[ i ] = 0.f;
						renderable->aBlendShapesDirty       = true;
					}

					ImGui::PopID();
				}

				ImGui::EndChild();
			}
		}
	}
}


void EntEditor_DrawEntityData()
{
	if ( gSelectedEntity == CH_INVALID_HANDLE )
	{
		ImGui::Text( "No Entity Selected" );
		return;
	}

	Entity_t* entity = Entity_GetData( gSelectedEntity );
	if ( entity == nullptr )
	{
		Log_ErrorF( "Selected Entity is nullptr?\n" );
		gSelectedEntity = CH_INVALID_HANDLE;
		return;
	}

	entity->aName.reserve( NAME_LEN );
	ImGui::InputText( "Name", entity->aName.data(), NAME_LEN, ImGuiInputTextFlags_CallbackAlways, EntityNameInput, entity );

	// std::string entName = vstring( "Entity %zd", gSelectedEntity );
	// ImGui::Text( entName.data() );
	
	EditorContext_t* context = Editor_GetContext();

	if ( context == nullptr )
		return;

	ChHandle_t parent = Entity_GetParent( gSelectedEntity );
	std::string parentName = "None";

	if ( parent )
	{
		Entity_t* parentData = Entity_GetData( parent );
		parentName = parentData->aName.size() ? parentData->aName : vstring( "Entity %zd", parent );
	}
	
	if ( ImGui::BeginCombo( "Set Parent", parentName.c_str() ) )
	{
		if ( ImGui::Selectable( "Clear Parent" ) )
		{
			Entity_SetParent( gSelectedEntity, CH_INVALID_HANDLE );
		}

		// Can't parent it to any of these
		std::unordered_set< ChHandle_t > children;
		Entity_GetChildrenRecurse( gSelectedEntity, children );

		const ChVector< ChHandle_t >& entityHandles = context->aMap.aMapEntities;

		for ( ChHandle_t entityHandle : entityHandles )
		{
			if ( Entity_GetParent( gSelectedEntity ) == entityHandle || gSelectedEntity == entityHandle )
				continue;

			// Make sure this isn't one of our children
			auto it = children.find( entityHandle );

			// it is, we can't parent to a child, continue
			if ( it != children.end() )
				continue;

			Entity_t* entityToParent = Entity_GetData( entityHandle );
			if (entityToParent == nullptr )
			{
				Log_ErrorF( "Entity is nullptr????\n" );
				continue;
			}
	
			std::string entName = vstring( "Entity %zd", entityHandle );

			ImGui::PushID( entityHandle );
	
			if ( ImGui::Selectable( entityToParent->aName.size() ? entityToParent->aName.c_str() : entName.c_str() ) )
			{
				Entity_SetParent( gSelectedEntity, entityHandle );
			}
	
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	ImGui::Separator();

	// Entity Transform
	ImGui::DragScalarN( "Position", ImGuiDataType_Float, &entity->aTransform.aPos.x, 3, 0.25f, nullptr, nullptr, nullptr, 1.f );
	ImGui::DragScalarN( "Angle", ImGuiDataType_Float, &entity->aTransform.aAng.x, 3, 0.25f, nullptr, nullptr, nullptr, 1.f );
	ImGui::DragScalarN( "Scale", ImGuiDataType_Float, &entity->aTransform.aScale.x, 3, 0.25f, nullptr, nullptr, nullptr, 1.f );

	ImGui::Separator();

	// Entity Model/Renderable
	if ( ImGui::CollapsingHeader( "Renderable", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_FramePadding ) )
	{
		EntEditor_DrawRenderableUI( entity );
	}

	if ( gInModelSelect )
	{
		// update model for renderable
		std::string newModel = EntEditor_DrawModelSelectionWindow();

		if ( newModel.size() )
		{
			gInModelSelect = false;
			gModelSelectPath.clear();

			ChHandle_t model = graphics->LoadModel( newModel );

			if ( model == CH_INVALID_HANDLE )
			{
				Log_ErrorF( "Invalid Model: %s\n", newModel.data() );
			}
			else
			{
				if ( entity->aModel )
					graphics->FreeModel( entity->aModel );

				entity->aModel = model;

				if ( entity->aRenderable )
				{
					Renderable_t* renderable = graphics->GetRenderableData( entity->aRenderable );
					graphics->SetRenderableModel( entity->aRenderable, model );
				}
				else
				{
					entity->aRenderable = graphics->CreateRenderable( entity->aModel );
				}
			}
		}
	}

	// Physics Model
	// entity->aPhysicsModel;

	// Light Editing
	if ( ImGui::CollapsingHeader( "Light", ImGuiTreeNodeFlags_DefaultOpen ) )
	{
		if ( entity->apLight )
		{
			ImGui::SameLine();

			if ( ImGui::Button( "Destroy Light" ) )
			{
				graphics->DestroyLight( entity->apLight );
				entity->apLight = nullptr;
			}
			else
			{
				EntEditor_DrawLightUI( entity );
			}
		}
		else
		{
			if ( ImGui::BeginCombo( "Create Light", "Point Light" ) )
			{
				if ( ImGui::Selectable( "Point Light" ) )
				{
					entity->apLight          = graphics->CreateLight( ELightType_Point );
					entity->apLight->aColor  = { 1, 1, 1, 10 };
					entity->apLight->aRadius = 500;
				}
				else if ( ImGui::Selectable( "Cone Light" ) )
				{
					entity->apLight         = graphics->CreateLight( ELightType_Cone );
					entity->apLight->aColor = { 1, 1, 1, 10 };

					// weird stuff to get the angle of the light correct from the player's view matrix stuff
					// light->aAng.x = playerWorldTransform.aAng.z;
					// light->aAng.y = -playerWorldTransform.aAng.y;
					// light->aAng.z = -playerWorldTransform.aAng.x + 90.f;

					entity->apLight->aInnerFov = 0.f;  // FOV
					entity->apLight->aOuterFov = 45.f;  // FOV
				}
				else if ( ImGui::Selectable( "World Light" ) )
				{
					entity->apLight = graphics->CreateLight( ELightType_Directional );
				}
				// else if ( ImGui::Selectable( "Capsule Light" ) )
				// {
				// }

				ImGui::EndCombo();
			}
		}
	}

	// Entity Components?
	
	// ImGui::End();

#if 0
	if ( !ImGui::Begin( "Entity Component List" ) )
	{
		ImGui::End();
		return;
	}

	if ( gSelectedEntity == CH_INVALID_HANDLE )
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
#endif
}


void EntEditor_DrawMapDataUI()
{
	ImGui::Text( "Skybox" );
}


void EntEditor_DrawEntityChildTree( ChHandle_t sParent )
{
	Entity_t* entity = Entity_GetData( sParent );
	if ( entity == nullptr )
	{
		Log_ErrorF( "Entity is nullptr????\n" );
		return;
	}

	std::string entName = vstring( "Entity %zd", sParent );

	ImGui::PushID( sParent );

	bool treeOpen = ImGui::TreeNode( "##" );

	ImGui::SameLine();

	if ( ImGui::Selectable( entity->aName.size() ? entity->aName.c_str() : entName.c_str(), gSelectedEntity == sParent ) )
	{
		gSelectedEntity = sParent;
		gInModelSelect  = false;
		gModelSelectPath.clear();
	}

	if ( treeOpen )
	{
		ChVector< ChHandle_t > children;

		Entity_GetChildren( sParent, children );

		for ( ChHandle_t child : children )
		{
			EntEditor_DrawEntityChildTree( child );
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}


void EntEditor_DrawEntityList()
{
	EditorContext_t* context = Editor_GetContext();

	if ( !context )
		return;

	if ( !ImGui::Begin( "Entity List" ) )
	{
		ImGui::End();
		return;
	}

	ImGui::BeginChild( "##Entity List_", {180, 0} );

	// TODO: make this create entity stuff into a context menu with plenty of presets
	// also, create a duplicate entity option when selected on one, and move delete entity and clear parent to that

	// if ( ImGui::Button( "Create" ) )
	// {
	// 	gSelectedEntity = Entity_Create();
	// }
	// 
	// ImGui::SameLine();

	if ( ImGui::Button( "Create" ) )
	{
		gSelectedEntity = Entity_Create();

		Entity_t* entity = Entity_GetData( gSelectedEntity );
		if ( entity == nullptr )
		{
			Log_ErrorF( "Created Entity is nullptr????\n" );
			gSelectedEntity = CH_INVALID_HANDLE;
		}
		else
		{
			entity->aTransform.aPos = context->aView.aPos;
		}
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Delete" ) )
	{
		if ( gSelectedEntity )
			Entity_Delete( gSelectedEntity );

		gSelectedEntity = CH_INVALID_HANDLE;
	}

	// Entity List
	if ( ImGui::BeginChild( "Entity List", {}, true ) )
	{
		const ChVector< ChHandle_t >& entityHandles = context->aMap.aMapEntities;
		auto&                         entityParents = Entity_GetParentMap();

		for ( ChHandle_t entityHandle : entityHandles )
		{
			auto it = entityParents.find( entityHandle );

			// this entity is already parented to something, it will be drawn in the recursive function if it's the parent tree is expanded
			if ( it != entityParents.end() )
				continue;

			EntEditor_DrawEntityChildTree( entityHandle );

		}
	}

	ImGui::EndChild();
	ImGui::EndChild();

	ImGui::SameLine( 195 );

	ImGui::BeginChild("tabs");
	if ( ImGui::BeginTabBar( "entity editor tabs") )
	{
		if ( ImGui::BeginTabItem( "Entity Data" ) )
		{
			// ImGui::BeginChild("Entity Data", {}, true);
			EntEditor_DrawEntityData();
			// ImGui::EndChild();
			ImGui::EndTabItem();
		}
		
		if ( ImGui::BeginTabItem( "Model Data" ) )
		{
			ImGui::EndTabItem();
		}
		
		if ( ImGui::BeginTabItem( "Map Data" ) )
		{
			EntEditor_DrawMapDataUI();
			ImGui::EndTabItem();
		}
		
		if ( ImGui::BeginTabItem( "Material Editor" ) )
		{
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
	ImGui::EndChild();

	ImGui::End();
}


static const glm::vec3 gSelectScale = { 10.f, 10.f, 10.f };
static const glm::vec3 gSelectBoxColor = { 1.f, 1.f, 1.f };


void EntEditor_DrawUI()
{
	EntEditor_DrawEntityList();
	// EntEditor_DrawEntityData();

	if ( gSelectedEntity == CH_INVALID_HANDLE )
		return;

	Entity_t* entity = Entity_GetData( gSelectedEntity );

	if ( entity->aRenderable )
	{
		// Draw a box around the selected entity
		ModelBBox_t bbox = graphics->GetRenderableAABB( entity->aRenderable );
		graphics->DrawBBox( bbox.aMin, bbox.aMax, gSelectBoxColor );
	}

	// Draw an axis where the selected is (TODO: change this to arrows, rotation, and scaling)
	glm::mat4 mat( 1.f );
	Entity_GetWorldMatrix( mat, gSelectedEntity );
	graphics->DrawAxis( mat, gSelectScale );
}

