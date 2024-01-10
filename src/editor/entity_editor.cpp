#include "entity_editor.h"
#include "entity.h"
#include "main.h"
#include "file_picker.h"

#include "imgui/imgui.h"

#include <unordered_set>


FilePickerData_t  gModelBrowserData{};
FilePickerData_t  gImporterFilePicker{};

static ChHandle_t gSelectedEntity = CH_INVALID_HANDLE;


bool EntEditor_Init()
{
	gModelBrowserData.filterExt = { ".obj", ".glb", ".gltf" };

	return true;
}


void EntEditor_Shutdown()
{
}


void EntEditor_Update( float sFrameTime )
{
}


void EntEditor_DrawImporter()
{
	if ( ImGui::Button( "Import File" ) )
	{
		gImporterFilePicker.open = true;
	}

	if ( gImporterFilePicker.open )
	{
		EFilePickerReturn status = FilePicker_Draw( gImporterFilePicker );

		if ( status == EFilePickerReturn_SelectedItems )
		{

		}
	}
}


static std::unordered_map< ChHandle_t, ImTextureID >   gImGuiTextures;
static std::unordered_map< ChHandle_t, TextureInfo_t > gTextureInfo;
int                                                    gTextureListViewMode = 0;


void Editor_DrawTextureInfo( TextureInfo_t& info )
{
	ImGui::Text( "Name: %s", info.aName.size() ? info.aName.data() : "UNNAMED" );

	if ( info.aPath.size() )
		ImGui::Text( info.aPath.data() );

	ImGui::Text( "Size: %d x %d", info.aSize.x, info.aSize.y );
	ImGui::Text( "GPU Index: %d", info.aGpuIndex );
}


void Editor_DrawTextureList()
{
	// Draws all currently loaded textures
	ChVector< ChHandle_t > textures   = render->GetTextureList();
	ImVec2                 windowSize = ImGui::GetWindowSize();

	glm::vec2              imageDisplaySize = { 96, 96 };

	if ( ImGui::BeginCombo( "View Type", gTextureListViewMode == 0 ? "List" : "Icons" ) )
	{
		if ( ImGui::Selectable( "List" ) )
			gTextureListViewMode = 0;

		if ( ImGui::Selectable( "Icons" ) )
			gTextureListViewMode = 1;

		ImGui::EndCombo();
	}

	// TODO: add a search bar?

	bool wrapIconList      = false;
	int  currentImageWidth = 0;
	int  imagesInRow       = 0;

	if ( !ImGui::BeginChild( "Texture List" ) )
	{
		ImGui::EndChild();
		return;
	}

	for ( ChHandle_t texture : textures )
	{
		ImTextureID imTexture = 0;

		auto it = gImGuiTextures.find( texture );
		if ( it == gImGuiTextures.end() )
		{
			imTexture = render->AddTextureToImGui( texture );
			gImGuiTextures[ texture ] = imTexture;
		}
		else
		{
			imTexture = it->second;
		}

		if ( imTexture == nullptr )
			continue;

		if ( gTextureListViewMode == 0 )
		{
			TextureInfo_t info = render->GetTextureInfo( texture );

			if ( ImGui::BeginChild( (int)imTexture, { windowSize.x - 16, imageDisplaySize.y + 16 }, ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
			{
				ImGui::Image( imTexture, { imageDisplaySize.x, imageDisplaySize.y } );

				ImGui::SameLine();

				if ( ImGui::BeginChild( (int)imTexture, { windowSize.x - 16 - imageDisplaySize.x, imageDisplaySize.y + 16 }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
				{
					Editor_DrawTextureInfo( info );
				}

				ImGui::EndChild();
			}

			if ( ImGui::IsItemHovered() )
			{
				if ( ImGui::BeginTooltip() )
					Editor_DrawTextureInfo( info );

				ImGui::EndTooltip();
			}

			ImGui::EndChild();
		}
		else
		{
			ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 2, 2 } );

			if ( windowSize.x < currentImageWidth + imageDisplaySize.x + ( imagesInRow * 2 ) )  // imagesInRow is for padding
			{
				currentImageWidth = 0;
				imagesInRow       = 0;
			}
			else
			{
				ImGui::SameLine();
			}

			ImGui::PushStyleVar( ImGuiStyleVar_ChildBorderSize, 0 );

			if ( ImGui::BeginChild( (int)imTexture, { imageDisplaySize.x, imageDisplaySize.y }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
			{
				ImGui::Image( imTexture, { imageDisplaySize.x, imageDisplaySize.y } );

				if ( ImGui::IsItemHovered() )
				{
					if ( ImGui::BeginTooltip() )
					{
						TextureInfo_t info = render->GetTextureInfo( texture );
						Editor_DrawTextureInfo( info );
					}

					ImGui::EndTooltip();
				}
			}

			ImGui::EndChild();

			ImGui::PopStyleVar();
			ImGui::PopStyleVar();

			currentImageWidth += imageDisplaySize.x;
			imagesInRow++;
		}
	}
	
	ImGui::EndChild();
}


void Editor_DrawAssetList()
{
	if ( ImGui::BeginTabBar( "editor tabs" ) )
	{
		if ( ImGui::BeginTabItem( "Texture List" ) )
		{
			Editor_DrawTextureList();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Model List" ) )
		{
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Material List" ) )
		{
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}


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


void EntEditor_DrawRenderableUI( Entity_t* spEntity )
{
	if ( spEntity->aModel )
	{
		std::string_view modelPath = graphics->GetModelPath( spEntity->aModel );
		ImGui::Text( modelPath.data() );

		if ( ImGui::IsItemHovered() )
		{
			// maybe wait a second before showing a tooltip?
			ImGui::BeginTooltip();
			ImGui::TextUnformatted( modelPath.data() );
			ImGui::EndTooltip();
		}

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
		gModelBrowserData.open = true;

		if ( spEntity->aModel )
		{
			std::string_view modelPath = graphics->GetModelPath( spEntity->aModel );
			gModelBrowserData.path     = FileSys_GetDirName( modelPath );
		}
		else
		{
			gModelBrowserData.path = FileSys_GetExePath();
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
			if ( ImGui::BeginChild( "Blend Shapes", {}, ImGuiChildFlags_ResizeY ) )
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

	if ( gModelBrowserData.open )
	{
		// update model for renderable
		EFilePickerReturn status = FilePicker_Draw( gModelBrowserData );

		if ( status == EFilePickerReturn_SelectedItems )
		{
			gModelBrowserData.open = false;

			ChHandle_t model       = graphics->LoadModel( gModelBrowserData.selectedItems[ 0 ] );

			if ( model == CH_INVALID_HANDLE )
			{
				Log_ErrorF( "Invalid Model: %s\n", gModelBrowserData.selectedItems[ 0 ].data() );
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
		gSelectedEntity        = sParent;
		gModelBrowserData.open = false;
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


extern int gMainMenuBarHeight;
glm::vec2  gEntityListSize{};


void EntEditor_DrawEntityList()
{
	EditorContext_t* context = Editor_GetContext();

	if ( !context )
		return;

	int width, height;
	render->GetSurfaceSize( width, height );

	ImGui::SetNextWindowPos( { 0.f, (float)gMainMenuBarHeight } );
	ImGui::SetNextWindowSizeConstraints( { 0.f, (float)( height - gMainMenuBarHeight ) }, { (float)width, (float)( height - gMainMenuBarHeight ) } );

	if ( !ImGui::Begin( "Entity List", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove ) )
	{
		ImGui::End();
		auto windowSize   = ImGui::GetWindowSize();
		gEntityListSize.x = windowSize.x;
		gEntityListSize.y = windowSize.y;
		return;
	}
	
	ImGui::BeginChild( "tabs" );
	if ( ImGui::BeginTabBar( "editor tabs" ) )
	{
		if ( ImGui::BeginTabItem( "Entities" ) )
		{
			ImGui::BeginChild( "##Entity List_", {}, ImGuiChildFlags_ResizeX );

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
				gSelectedEntity  = Entity_Create();

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

			ImGui::SameLine();

			ImGui::BeginChild( "##Entity Data" );
			EntEditor_DrawEntityData();
			ImGui::EndChild();

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

		if ( ImGui::BeginTabItem( "Importer" ) )
		{
			EntEditor_DrawImporter();
			ImGui::EndTabItem();
		}

		if ( ImGui::BeginTabItem( "Asset List" ) )
		{
			Editor_DrawAssetList();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::EndChild();

	auto windowSize   = ImGui::GetWindowSize();
	gEntityListSize.x = windowSize.x;
	gEntityListSize.y = windowSize.y;

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

