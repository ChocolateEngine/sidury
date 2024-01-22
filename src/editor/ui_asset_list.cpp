#include "entity_editor.h"
#include "entity.h"
#include "main.h"
#include "file_picker.h"
#include "skybox.h"
#include "importer.h"

#include "imgui/imgui.h"

#include <unordered_set>


// maybe make EFileType? idk
enum EAssetType
{
	EAssetType_Unknown,

	EAssetType_Directory,
	EAssetType_Image,

	EAssetType_Model,
	EAssetType_Texture,
	EAssetType_Material,

	EAssetType_Count,
};


const char* gAssetTypeStr[] = {
	"EAssetType_Unknown",

	"EAssetType_Directory",
	"EAssetType_Image",

	"EAssetType_Model",
	"EAssetType_Texture",
	"EAssetType_Material",
};


static_assert( CH_ARR_SIZE( gAssetTypeStr ) == EAssetType_Count );


struct Asset_t
{
	std::string path;
	std::string fileName;

	EAssetType  type;
};


struct AssetBrowserData_t
{
	ESearchPathType        searchType;
	std::string            currentPath;
	std::string            currentSearchPath;

	std::vector< Asset_t > fileList;
	bool                   fileListDirty = true;
};


glm::ivec2 gAssetBrowserSize;
extern int gMainMenuBarHeight;

AssetBrowserData_t gAssetBrowserData{};


// this will check the file extension only
// TODO: acutally read the header of the file somehow
bool AssetBrowser_CanDragAssetToView( Asset_t& srAsset )
{
	return false;
}


static void AssetBrowser_ScanFolder( AssetBrowserData_t& srData )
{
	std::vector< std::string > fileList;

	if ( gAssetBrowserData.currentPath.empty() )
	{
		const std::vector< std::string >* searchPaths = nullptr;

		if ( gAssetBrowserData.searchType == ESearchPathType_SourceAssets )
		{
			searchPaths = &FileSys_GetSourcePaths();
		}
		else
		{
			searchPaths = &FileSys_GetSearchPaths();
		}

		// TODO: this isn't very good
		for ( const auto& searchPath : *searchPaths )
		{
			auto fileList2 = FileSys_ScanDir( searchPath, ReadDir_AbsPaths );
			fileList.insert( fileList.end(), fileList2.begin(), fileList2.end() );
		}
	}
	else
	{
		srData.currentPath = FileSys_CleanPath( srData.currentPath );
		fileList = FileSys_ScanDir( srData.currentPath, ReadDir_AbsPaths );
	}

	for ( size_t i = 0; i < fileList.size(); i++ )
	{
		std::string_view file = fileList[ i ];

		Asset_t          asset{};
		asset.path     = file;
		asset.fileName = FileSys_GetFileName( file );

		std::string fileExt = FileSys_GetFileExt( file );
		
		// TODO: improve this
		if ( fileExt == ".obj" || fileExt == ".gltf" || fileExt == ".glb" )
		{
			asset.type = EAssetType_Model;
		}
		else if ( fileExt == ".ktx" )
		{
			asset.type = EAssetType_Image;
		}
		else if ( fileExt == ".cmt" )
		{
			asset.type = EAssetType_Material;
		}
		else if ( fileExt == ".jpg" || fileExt == ".jpeg" || fileExt == ".png" || fileExt == ".webp" || fileExt == ".jxl" || fileExt == ".gif" )
		{
			asset.type = EAssetType_Image;
		}
		else if ( FileSys_IsDir( file.data(), true ) )
		{
			asset.type = EAssetType_Directory;
		}

		srData.fileList.push_back( asset );
	}
}


void AssetBrowser_Draw()
{
	// set position
	int width, height;
	render->GetSurfaceSize( width, height );

	ImGui::SetNextWindowSizeConstraints( { (float)width, 0.f }, { (float)width, (float)( ( height ) - gMainMenuBarHeight ) } );

	if ( !ImGui::Begin( "##Asset Browser", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove ) )
	{
		gAssetBrowserSize.x = ImGui::GetWindowWidth();
		gAssetBrowserSize.y = ImGui::GetWindowHeight();
		ImGui::End();
		return;
	}

	ImGui::Text( "Asset Browser" );

	ImGui::SameLine();

	ImGui::SetNextItemWidth( 120 );

	// Search Path Type Drop Down
	if ( ImGui::BeginCombo( "Search Path Type", gAssetBrowserData.searchType == ESearchPathType_Path ? "Game Assets" : "Source Assets" ) )
	{
		if ( ImGui::Selectable( "Game Assets" ) )
		{
			gAssetBrowserData.searchType = ESearchPathType_Path;
		}

		if ( ImGui::Selectable( "Source Assets" ) )
		{
			gAssetBrowserData.searchType = ESearchPathType_SourceAssets;
		}

		ImGui::EndCombo();
	}

	ImGui::SameLine();

	// Search Path Drop Down - TODO: allow custom path inputs?
	if ( ImGui::BeginCombo( "Search Path", gAssetBrowserData.currentPath.empty() ? "All" : gAssetBrowserData.currentPath.c_str() ) )
	{
		if ( ImGui::Selectable( "All" ) )
		{
			gAssetBrowserData.fileListDirty = true;
			gAssetBrowserData.currentPath.clear();
		}

		//ImGui::Separator();

		// TODO: check if this isn't an existing search path
		//if ( gAssetBrowserData.currentPath.size() && ImGui::Selectable( gAssetBrowserData.currentPath.c_str() ) )
		//{
		//	gAssetBrowserData.fileListDirty = true;
		//}

		//ImGui::Separator();

		const std::vector< std::string >* searchPaths = nullptr;

		if ( gAssetBrowserData.searchType == ESearchPathType_SourceAssets )
		{
			searchPaths = &FileSys_GetSourcePaths();
		}
		else
		{
			searchPaths = &FileSys_GetSearchPaths();
		}

		for ( const auto& searchPath : *searchPaths )
		{
			if ( ImGui::Selectable( searchPath.c_str() ) )
			{
				gAssetBrowserData.currentPath   = searchPath;
				gAssetBrowserData.fileListDirty = true;
			}
		}

		ImGui::EndCombo();
	}

	// TODO: maybe input a custom path here?

	if ( gAssetBrowserData.fileListDirty )
	{
		gAssetBrowserData.fileListDirty = false;
		gAssetBrowserData.fileList.clear();
		AssetBrowser_ScanFolder( gAssetBrowserData );
	}

	// Show Files
	if ( ImGui::BeginChild( "##Files", {}, ImGuiChildFlags_Border ) )
	{
		for ( const auto& file : gAssetBrowserData.fileList )
		{
			if ( file.type == EAssetType_Directory )
			{
				if ( ImGui::Selectable( (file.fileName + CH_PATH_SEP_STR).c_str() ) )
				{
					gAssetBrowserData.currentPath   = file.path;
					gAssetBrowserData.fileListDirty = true;
				}
			}
			else
			{
				if ( ImGui::Selectable( file.fileName.c_str() ) )
				{
					// idk show in editor somewhere?
				}
			}
		}
	}

	ImGui::EndChild();

	int windowHeight = ImGui::GetWindowHeight();
	ImGui::SetWindowPos( ImVec2( 0, height - ImGui::GetWindowHeight() ), true );

	gAssetBrowserSize.x = ImGui::GetWindowWidth();
	gAssetBrowserSize.y = ImGui::GetWindowHeight();

	ImGui::End();
}

