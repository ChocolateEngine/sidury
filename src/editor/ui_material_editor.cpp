#include "entity_editor.h"
#include "entity.h"
#include "main.h"
#include "file_picker.h"
#include "skybox.h"
#include "importer.h"

#include "imgui/imgui.h"

#include <unordered_set>


struct MaterialEditorData
{
	ChHandle_t  mat                  = CH_INVALID_HANDLE;

	bool        drawNewDialog        = false;
	std::string newMatName           = "";
	
	std::string newVarName           = "";
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

	const char* matPath    = graphics->Mat_GetName( gMatEditor.mat );
	ChHandle_t  shader     = graphics->Mat_GetShader( gMatEditor.mat );
	const char* shaderName = graphics->GetShaderName( shader );

	ImGui::Text( matPath );

	// Draw Shader Dropdown
	if ( ImGui::BeginCombo( "Shader", shaderName ? shaderName : "" ) )
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

	ImGui::Separator();

	gMatEditor.newVarName.reserve( 128 );
	ImGui::InputText( "New Var Name", gMatEditor.newVarName.data(), 128, ImGuiInputTextFlags_CallbackAlways, StringTextInput, &gMatEditor.newVarName );

	// Add Material Var - Remove when we have shaders registering var types
	if ( ImGui::BeginCombo( "Add Var", "Texture" ) )
	{
		if ( ImGui::Selectable( "Texture" ) && gMatEditor.newVarName.size() )
		{
			// TODO: open a texture browser
			graphics->Mat_SetVar( gMatEditor.mat, gMatEditor.newVarName, CH_INVALID_HANDLE );
			gMatEditor.newVarName.clear();
		}

		if ( ImGui::Selectable( "Float" ) && gMatEditor.newVarName.size() )
		{
			graphics->Mat_SetVar( gMatEditor.mat, gMatEditor.newVarName, 0.f );
			gMatEditor.newVarName.clear();
		}

		if ( ImGui::Selectable( "Int" ) && gMatEditor.newVarName.size() )
		{
			graphics->Mat_SetVar( gMatEditor.mat, gMatEditor.newVarName, 0 );
			gMatEditor.newVarName.clear();
		}

		if ( ImGui::Selectable( "Vec2" ) && gMatEditor.newVarName.size() )
		{
			graphics->Mat_SetVar( gMatEditor.mat, gMatEditor.newVarName, glm::vec2() );
			gMatEditor.newVarName.clear();
		}

		if ( ImGui::Selectable( "Vec3" ) && gMatEditor.newVarName.size() )
		{
			graphics->Mat_SetVar( gMatEditor.mat, gMatEditor.newVarName, glm::vec3() );
			gMatEditor.newVarName.clear();
		}

		if ( ImGui::Selectable( "Vec4" ) && gMatEditor.newVarName.size() )
		{
			graphics->Mat_SetVar( gMatEditor.mat, gMatEditor.newVarName, glm::vec4() );
			gMatEditor.newVarName.clear();
		}

		ImGui::EndCombo();
	}

	// Draw Material Vars
	if ( ImGui::BeginTable( matPath, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable ) )
	{
		// Draw Shader
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex( 0 );

		ImGui::Text( "shader" );

		ImGui::TableSetColumnIndex( 1 );

		ImGui::Text( shaderName ? shaderName : "INVALID" );

		size_t varCount = graphics->Mat_GetVarCount( gMatEditor.mat );

		for ( u32 varI = 0; varI < varCount; varI++ )
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 );

			// TODO: probably expose MaterialVar directly? this re-accessing the data a lot
			const char* varName    = graphics->Mat_GetVarName( gMatEditor.mat, varI );
			EMatVar     matVarType = graphics->Mat_GetVarType( gMatEditor.mat, varI );

			ImGui::Text( varName ? varName : "ERROR NAME" );

			ImGui::TableSetColumnIndex( 1 );

			switch ( matVarType )
			{
				default:
				case EMatVar_Invalid:
					break;

				case EMatVar_Texture:
				{
					ChHandle_t    texHandle = graphics->Mat_GetTexture( gMatEditor.mat, varI );
					TextureInfo_t texInfo   = render->GetTextureInfo( texHandle );

					ImGui::Text( texInfo.aName.empty() ? texInfo.aPath.c_str() : texInfo.aName.c_str() );

					if ( ImGui::IsItemHovered() )
					{
						if ( ImGui::BeginTooltip() )
							Editor_DrawTextureInfo( texInfo );

						ImGui::EndTooltip();
					}

					break;
				}

				case EMatVar_Float:
				{
					float value = graphics->Mat_GetFloat( gMatEditor.mat, varI );
					ImGui::Text( "%.6f", value );
					break;
				}

				case EMatVar_Int:
				{
					int value = graphics->Mat_GetInt( gMatEditor.mat, varI );
					ImGui::Text( "%d", value );
					break;
				}

				case EMatVar_Bool:
				{
					bool value = graphics->Mat_GetBool( gMatEditor.mat, varI );
					ImGui::Text( value ? "True" : "False" );
					break;
				}

				case EMatVar_Vec2:
				{
					glm::vec2 value = graphics->Mat_GetVec2( gMatEditor.mat, varI );
					ImGui::Text( "X: %.6f, Y: %.6f", value.x, value.y );
					break;
				}

				case EMatVar_Vec3:
				{
					glm::vec3 value = graphics->Mat_GetVec3( gMatEditor.mat, varI );
					ImGui::Text( "X: %.6f, Y: %.6f, Z: %.6f", value.x, value.y, value.z );
					break;
				}

				case EMatVar_Vec4:
				{
					glm::vec4 value = graphics->Mat_GetVec4( gMatEditor.mat, varI );
					ImGui::Text( "X: %.6f, Y: %.6f, Z: %.6f, W: %.6f", value.x, value.y, value.z, value.w );
					break;
				}
			}
		}
	}

	ImGui::EndTable();
	ImGui::EndChild();
}


void MaterialEditor_SetMaterial( ChHandle_t sMat )
{
	gMatEditor.mat = sMat;
}


ChHandle_t MaterialEditor_GetMaterial()
{
	return gMatEditor.mat;
}

