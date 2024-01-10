#include "file_picker.h"
#include "main.h"

#include "imgui/imgui.h"


static int FilePicker_PathInput( ImGuiInputTextCallbackData* data )
{
	std::string* path = (std::string*)data->UserData;
	path->resize( data->BufTextLen );

	return 0;
}


static void FilePicker_ScanFolder( FilePickerData_t& srData, std::string_view srFile )
{
	srData.path   = FileSys_CleanPath( srFile );
	auto fileList = FileSys_ScanDir( srData.path, ReadDir_AbsPaths );

	srData.filesInFolder.clear();

	for ( size_t i = 0; i < fileList.size(); i++ )
	{
		std::string_view file  = fileList[ i ];
		bool isDir = FileSys_IsDir( file.data(), true );

		if ( !isDir && srData.filterExt.size() )
		{
			bool valid = false;
			for ( size_t filterI = 0; filterI < srData.filterExt.size(); filterI++ )
			{
				std::string_view ext = srData.filterExt[ filterI ];
				if ( file.ends_with( ext ) )
				{
					valid = true;
					break;
				}
			}

			if ( !valid )
				continue;
		}

		srData.filesInFolder.push_back( file.data() );
	}
}


#define PATH_LEN 1024

EFilePickerReturn FilePicker_Draw( FilePickerData_t& srData )
{
	if ( !ImGui::Begin( "File Picker" ) )
	{
		ImGui::End();
		return EFilePickerReturn_None;
	}

	if ( srData.path.empty() )
	{
		// Default to Root Path
		srData.path = FileSys_GetExePath();
	}

	// Draw Title Bar
	if ( ImGui::Button( "Up" ) )
	{
		// BLECH
		srData.path = fs::path( srData.path ).parent_path().string();
		srData.filesInFolder.clear();
	}

	ImGui::SameLine();

	srData.path.reserve( PATH_LEN );
	if ( ImGui::InputText( "Path", srData.path.data(), PATH_LEN, ImGuiInputTextFlags_CallbackAlways | ImGuiInputTextFlags_EnterReturnsTrue, FilePicker_PathInput, &srData.path ) )
	{
		srData.filesInFolder.clear();
	}

	ImGui::SameLine();

	if ( ImGui::Button( "Go" ) )
	{
		srData.filesInFolder.clear();
	}

	if ( srData.filesInFolder.empty() )
	{
		FilePicker_ScanFolder( srData, srData.path );
	}

	// Draw Items in Folder
	// ImGui::BeginChild()

	bool   selectedItems = false;
	size_t selectedIndex = SIZE_MAX;

	ImVec2 currentWindowSize = ImGui::GetWindowSize();
	
	ImGui::SetNextWindowSizeConstraints( {}, { currentWindowSize.x, currentWindowSize.y - 82 } );

	if ( ImGui::BeginChild( "##Files", {}, ImGuiChildFlags_Border ) )
	{
		for ( size_t i = 0; i < srData.filesInFolder.size(); i++ )
		{
			std::string_view file = srData.filesInFolder[ i ];
			if ( ImGui::Selectable( file.data() ) )
			{
				bool isDir = FileSys_IsDir( file.data(), true );

				if ( isDir )
				{
					FilePicker_ScanFolder( srData, file );
					srData.selectedItems.clear();
					selectedIndex = SIZE_MAX;
				}
				else
				{
					if ( srData.selectedItems.size() )
						srData.selectedItems.clear();

					srData.selectedItems.push_back( file.data() );
					selectedIndex = i;
					selectedItems = true;
					srData.open   = false;
				}
			}
		}
	}

	ImGui::EndChild();

	// Draw Status Bar
	ImGui::Text( "%d Items in Folder", srData.filesInFolder.size() );

	ImGui::SameLine();
	// ImGui::Spacing();
	// ImGui::SameLine();

	if ( ImGui::Button( "Close" ) )
	{
		srData.open = false;
		ImGui::End();
		srData.selectedItems.clear();
		return EFilePickerReturn_Close;
	}
	
	ImGui::End();

	return selectedItems ? EFilePickerReturn_SelectedItems : EFilePickerReturn_None;
}

