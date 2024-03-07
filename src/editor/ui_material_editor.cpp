#include "entity_editor.h"
#include "entity.h"
#include "main.h"
#include "file_picker.h"
#include "skybox.h"
#include "importer.h"

#include "imgui/imgui.h"

#include <unordered_set>


CONVAR( matedit_texture_size, 64, CVARF_ARCHIVE, "Size of textures drawn in the image preview" );


struct TextureParameterData
{
	char*       path[ 512 ];
	ImTextureID imTexture;
};


struct MaterialEditorData
{
	ChHandle_t                                      mat           = CH_INVALID_HANDLE;

	bool                                            drawNewDialog = false;
	std::string                                     newMatName    = "";

	std::string                                     newVarName    = "";

	std::unordered_map< ChHandle_t, ImTextureID >   imguiTextures;

	std::unordered_map< u32, TextureParameterData > textureData;

	FilePickerData_t                                textureBrowser;
	u32                                             textureBrowserVar = UINT32_MAX;
};


static MaterialEditorData gMatEditor;


static int StringTextInput( ImGuiInputTextCallbackData* data )
{
	std::string* string = (std::string*)data->UserData;
	string->resize( data->BufTextLen );

	return 0;
}



void MaterialEditor_DrawNewDialog()
{
	gMatEditor.newMatName.reserve( 128 );
	
	if ( ImGui::InputText( "Name", gMatEditor.newMatName.data(), 128, ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_EnterReturnsTrue, StringTextInput, &gMatEditor.newMatName ) )
	{
		ChHandle_t shader        = graphics->GetShader( "basic_3d" );
		gMatEditor.mat           = graphics->CreateMaterial( gMatEditor.newMatName, shader );
		gMatEditor.drawNewDialog = false;
		gMatEditor.newMatName.clear();
	}
}


void MaterialEditor_DrawViewLoadedDialog()
{
	const char* matPath = nullptr;
	if ( gMatEditor.mat )
		matPath = graphics->Mat_GetName( gMatEditor.mat );

	if ( !ImGui::BeginCombo( "Material", matPath ? matPath : "" ) )
	{
		return;
	}

	u32 matCount = graphics->GetMaterialCount();

	for ( u32 i = 0; i < matCount; i++ )
	{
		ChHandle_t  mat     = graphics->GetMaterialByIndex( i );
		const char* matName = graphics->Mat_GetName( mat );

		if ( ImGui::Selectable( matName ) )
		{
			gMatEditor.mat = mat;
		}
	}

	ImGui::EndCombo();
}


void MaterialEditor_DrawTextureVar()
{
}


void MaterialEditor_Draw()
{
	if ( !ImGui::BeginChild( "Material Editor" ) )
	{
		ImGui::EndChild();
		return;
	}

	// Buttons to Create New Material, Load One, Save, and Save As
	//if ( ImGui::BeginMenuBar() )
	{
		if ( ImGui::Button( "New" ) )
		{
			gMatEditor.drawNewDialog = true;
		}

		ImGui::SameLine();

		if ( ImGui::Button( "Open" ) )
		{
		}

		ImGui::SameLine();

		if ( ImGui::Button( "Save" ) )
		{
		}

		ImGui::SameLine();

		if ( ImGui::Button( "Save As" ) )
		{
		}

		ImGui::SameLine();

		if ( ImGui::Button( "Pick Material Under Cursor" ) )
		{
		}
	}

	if ( gMatEditor.drawNewDialog )
	{
		MaterialEditor_DrawNewDialog();
	}

	MaterialEditor_DrawViewLoadedDialog();

	if ( gMatEditor.mat == CH_INVALID_HANDLE )
	{
		ImGui::Text( "No Material Selected" );
		ImGui::EndChild();
		return;
	}

	ImGui::Separator();

	const char* matPath       = graphics->Mat_GetName( gMatEditor.mat );
	ChHandle_t  oldShader     = graphics->Mat_GetShader( gMatEditor.mat );
	const char* oldShaderName = graphics->GetShaderName( oldShader );

	ImGui::Text( matPath );

	// Draw Shader Dropdown
	if ( ImGui::BeginCombo( "Shader", oldShaderName ? oldShaderName : "" ) )
	{
		u32 shaderCount = graphics->GetGraphicsShaderCount();

		for ( u32 i = 0; i < shaderCount; i++ )
		{
			ChHandle_t  shader     = graphics->GetGraphicsShaderByIndex( i );
			const char* shaderName = graphics->GetShaderName( shader );

			if ( ImGui::Selectable( shaderName ) )
			{
				graphics->Mat_SetShader( gMatEditor.mat, shader );
			}
		}

		ImGui::EndCombo();
	}

	ChHandle_t  shader     = graphics->Mat_GetShader( gMatEditor.mat );
	const char* shaderName = graphics->GetShaderName( shader );

	ImGui::Separator();

	// Get Shader Variable Data
	u32 shaderVarCount = graphics->GetShaderVarCount( shader );

	if ( !shaderVarCount )
	{
		ImGui::Text( "Shader has no material properties" );
		ImGui::EndChild();
		return;
	}

	size_t                 varCount   = graphics->Mat_GetVarCount( gMatEditor.mat );
	ShaderMaterialVarDesc* shaderVars = graphics->GetShaderVars( shader );

	if ( !shaderVars )
	{
		ImGui::Text( "FAILED TO GET SHADER VARS!!!!" );
		ImGui::EndChild();
		return;
	}

	// Shader Booleans
	ImGui::Text( "Booleans" );

	for ( u32 varI = 0; varI < shaderVarCount; varI++ )
	{
		if ( shaderVars[ varI ].type != EMatVar_Bool )
			continue;

		bool value = graphics->Mat_GetBool( gMatEditor.mat, shaderVars[ varI ].name );
		bool out   = value;

		if ( ImGui::BeginChild( shaderVars[ varI ].name, {}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY ) )
		{
			if ( ImGui::Checkbox( shaderVars[ varI ].name, &out ) )
			{
				graphics->Mat_SetVar( gMatEditor.mat, shaderVars[ varI ].name, out );
			}

			if ( !shaderVars[ varI ].desc )
				continue;

			ImGui::Text( shaderVars[ varI ].desc );
		}

		ImGui::EndChild();
	}

	ImGui::Spacing();

	u32 imId = 1;

	// maybe make a section for textures only?

	ImGui::Text( "Textures" );
	
	for ( u32 varI = 0; varI < shaderVarCount; varI++ )
	{
		ShaderMaterialVarDesc& shaderVar = shaderVars[ varI ];

		if ( shaderVar.type != EMatVar_Texture )
			continue;

		if ( ImGui::BeginChild( shaderVar.name, {}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY ) )
		{
			ChHandle_t        texHandle   = graphics->Mat_GetTexture( gMatEditor.mat, shaderVar.name );
			EFilePickerReturn filePickRet = EFilePickerReturn_None;

			float             size        = std::clamp( matedit_texture_size.GetFloat(), 2.f, 8192.f );

			ImVec2            texSize     = { size, size };

			auto              texIt       = gMatEditor.imguiTextures.find( texHandle );
			ImTextureID       imTex       = 0;

			if ( texIt == gMatEditor.imguiTextures.end() )
			{
				imTex                                 = render->AddTextureToImGui( texHandle );
				gMatEditor.imguiTextures[ texHandle ] = imTex;
			}
			else
			{
				imTex = texIt->second;
			}

			// if ( ImGui::BeginChild( shaderVar.name, {}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY ) )
			{
				if ( ImGui::BeginChild( imId++, {}, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY ) )
				{
					ImGui::Image( imTex, texSize );
				}

				ImGui::EndChild();

				ImGui::SameLine();

				// ImGui::SetNextWindowSizeConstraints( { 50, 50 }, { 100000, 100000 } );

				if ( ImGui::BeginChild( imId++, {}, ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY ) )
				{
					ImGui::Text( shaderVar.name );

					if ( shaderVar.desc )
					{
						ImGui::SameLine();
						ImGui::Text( "- %s", shaderVar.desc );
					}

					if ( ImGui::Button( "Select" ) || ( gMatEditor.textureBrowser.open && gMatEditor.textureBrowserVar == varI ) )
					{
						gMatEditor.textureBrowserVar        = varI;
						gMatEditor.textureBrowser.open      = true;
						gMatEditor.textureBrowser.filterExt = { ".ktx" };
						filePickRet                         = FilePicker_Draw( gMatEditor.textureBrowser, shaderVar.name );
					}

					ImGui::SameLine();

					bool resetTexture = false;
					if ( ImGui::Button( "Reset" ) )
					{
						resetTexture = true;
					}

					// TODO: show texture import options?
					// Mainly an option to have texture filtering on or off

					TextureInfo_t texInfo = render->GetTextureInfo( texHandle );

					ImGui::Text( texInfo.aName.empty() ? texInfo.aPath.c_str() : texInfo.aName.c_str() );

					if ( ImGui::IsItemHovered() )
					{
						if ( ImGui::BeginTooltip() )
							Editor_DrawTextureInfo( texInfo );

						ImGui::EndTooltip();
					}

					if ( filePickRet == EFilePickerReturn_SelectedItems )
					{
						if ( texHandle )
							render->FreeTexture( texHandle );

						TextureCreateData_t createInfo{};
						createInfo.aUsage     = EImageUsage_Sampled;

						ChHandle_t newTexture = CH_INVALID_HANDLE;
						render->LoadTexture( newTexture, gMatEditor.textureBrowser.selectedItems[ 0 ], createInfo );
						graphics->Mat_SetVar( gMatEditor.mat, shaderVar.name, newTexture );

						texHandle                    = newTexture;

						gMatEditor.textureBrowserVar = UINT32_MAX;
					}

					if ( resetTexture )
					{
						graphics->Mat_SetVar( gMatEditor.mat, shaderVar.name, shaderVar.defaultTextureHandle );
					}
				}

				ImGui::EndChild();
			}
		}

		ImGui::EndChild();
	}

	ImGui::Text( "Variables" );

	// Shader Variables
	for ( u32 varI = 0; varI < shaderVarCount; varI++ )
	{
		ShaderMaterialVarDesc& shaderVar = shaderVars[ varI ];

		if ( shaderVar.type == EMatVar_Invalid || shaderVar.type == EMatVar_Bool  || shaderVar.type == EMatVar_Texture )
			continue;

		if ( ImGui::BeginChild( shaderVar.name, {}, ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY ) )
		{
			ImGui::Text( shaderVar.name );

			if ( shaderVar.desc )
			{
				ImGui::SameLine();
				ImGui::Text( "- %s", shaderVar.desc );
			}

			switch ( shaderVar.type )
			{
				default:
				case EMatVar_Invalid:
				case EMatVar_Bool:
				case EMatVar_Texture:
					continue;

				case EMatVar_Float:
				{
					float value = graphics->Mat_GetFloat( gMatEditor.mat, shaderVar.name );

					if ( ImGui::DragFloat( shaderVar.name, &value, 0.05f ) )
					{
						graphics->Mat_SetVar( gMatEditor.mat, shaderVar.name, value );
					}

					break;
				}

				case EMatVar_Int:
				{
					int value = graphics->Mat_GetInt( gMatEditor.mat, shaderVar.name );

					if ( ImGui::DragInt( shaderVar.name, &value ) )
					{
						graphics->Mat_SetVar( gMatEditor.mat, shaderVar.name, value );
					}

					break;
				}

				case EMatVar_Vec2:
				{
					glm::vec2 value = graphics->Mat_GetVec2( gMatEditor.mat, shaderVar.name );

					if ( ImGui::DragScalarN( shaderVar.name, ImGuiDataType_Float, &value.x, 2, 0.25f, nullptr, nullptr, nullptr, 1.f ) )
					{
						graphics->Mat_SetVar( gMatEditor.mat, shaderVar.name, value );
					}

					break;
				}

				case EMatVar_Vec3:
				{
					glm::vec3 value = graphics->Mat_GetVec3( gMatEditor.mat, shaderVar.name );

					if ( ImGui::DragScalarN( shaderVar.name, ImGuiDataType_Float, &value.x, 3, 0.25f, nullptr, nullptr, nullptr, 1.f ) )
					{
						graphics->Mat_SetVar( gMatEditor.mat, shaderVar.name, value );
					}

					break;
				}

				case EMatVar_Vec4:
				{
					glm::vec4 value = graphics->Mat_GetVec4( gMatEditor.mat, shaderVar.name );

					if ( ImGui::DragScalarN( shaderVar.name, ImGuiDataType_Float, &value.x, 4, 0.25f, nullptr, nullptr, nullptr, 1.f ) )
					{
						graphics->Mat_SetVar( gMatEditor.mat, shaderVar.name, value );
					}

					break;
				}
			}
		}

		ImGui::EndChild();
	}

	ImGui::EndChild();
}


void MaterialEditor_SetMaterial( ChHandle_t sMat )
{
	gMatEditor.textureBrowser.filterExt.push_back( ".ktx" );
	gMatEditor.textureBrowser.open = false;
	gMatEditor.textureBrowserVar   = UINT32_MAX;

	// Clear ImGui Textures
	if ( gMatEditor.mat )
	{
		size_t varCount = graphics->Mat_GetVarCount( gMatEditor.mat );

		for ( u32 varI = 0; varI < varCount; varI++ )
		{
			EMatVar matVarType = graphics->Mat_GetVarType( gMatEditor.mat, varI );

			if ( matVarType == EMatVar_Texture )
			{
				ChHandle_t texHandle = graphics->Mat_GetTexture( gMatEditor.mat, varI );
				render->FreeTextureFromImGui( texHandle );
			}
		}
	}

	gMatEditor.imguiTextures.clear();

	gMatEditor.mat = sMat;
	
	if ( gMatEditor.mat == CH_INVALID_HANDLE )
	{
		return;
	}

	// size_t varCount = graphics->Mat_GetVarCount( gMatEditor.mat );
	// 
	// for ( u32 varI = 0; varI < varCount; varI++ )
	// {
	// 	EMatVar matVarType = graphics->Mat_GetVarType( gMatEditor.mat, varI );
	// 
	// 	if ( matVarType == EMatVar_Texture )
	// 	{
	// 		ChHandle_t texHandle = graphics->Mat_GetTexture( gMatEditor.mat, varI );
	// 		ImTextureID imTex = render->AddTextureToImGui( texHandle );
	// 
	// 		if ( imTex )
	// 		{
	// 			gMatEditor.imguiTextures[ texHandle ] = imTex;
	// 		}
	// 		else
	// 		{
	// 			Log_ErrorF( "Failed to add imgui texture!\n" );
	// 			MaterialEditor_SetMaterial( CH_INVALID_HANDLE );
	// 			return;
	// 		}
	// 	}
	// }
}


ChHandle_t MaterialEditor_GetMaterial()
{
	return gMatEditor.mat;
}

