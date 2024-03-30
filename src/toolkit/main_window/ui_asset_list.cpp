#include "main.h"

#include "imgui/imgui.h"

#include <unordered_set>


enum ESortMode
{
	ESortMode_Name_Decending,
	ESortMode_Name_Ascending,

	ESortMode_DateModified_Decending,
	ESortMode_DateModified_Ascending,

	ESortMode_DateCreated_Decending,
	ESortMode_DateCreated_Ascending,

	ESortMode_FileSize_Decending,
	ESortMode_FileSize_Ascending,

	ESortMode_Count,
};


enum EAssetType
{
	EAssetType_Unknown,
	EAssetType_Directory,

	EAssetType_Image,
	EAssetType_Model,
	EAssetType_Texture,
	EAssetType_Material,
	EAssetType_Sound,

	EAssetType_Count,
};


const char* gAssetTypeStr[] = {
	"Unknown",
	"Directory",

	"Image",
	"Model",
	"Texture",
	"Material",
	"Sound",
};


const char* gAssetTypeIconPaths[] = {
	"resources/icons/icon-fuck_is_this",
	"resources/icons/icon-fuck_is_this",

	"resources/icons/icon-username",
	"resources/icons/icon-model",
	"resources/icons/icon-username",
	"resources/icons/icon-text_resource",
	"resources/icons/icon-text_resource",
};


static_assert( CH_ARR_SIZE( gAssetTypeStr ) == EAssetType_Count );
static_assert( CH_ARR_SIZE( gAssetTypeIconPaths ) == EAssetType_Count );


struct Asset_t
{
	std::string path;
	std::string fileName;
	std::string ext;

	EAssetType  type;
};


EAssetType gAssetTypeView = EAssetType_Count;  // EAssetType_Count will be all assets


struct AssetBrowserData_t
{
	ChHandle_t             icons[ EAssetType_Count ];
	ImTextureID            iconsImGui[ EAssetType_Count ];

	ESearchPathType        searchType;
	std::string            currentPath;
	std::string            currentSearchPath;

	std::vector< Asset_t > fileList;
	bool                   fileListDirty = true;

	char                   searchText[ 512 ];
	std::vector< size_t >  searchResults;
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
			auto fileList2 = FileSys_ScanDir( searchPath, ReadDir_AbsPaths | ReadDir_Recursive | ReadDir_NoDirs );
			fileList.insert( fileList.end(), fileList2.begin(), fileList2.end() );
		}
	}
	else
	{
		srData.currentPath = FileSys_CleanPath( srData.currentPath );
		fileList           = FileSys_ScanDir( srData.currentPath, ReadDir_AbsPaths | ReadDir_Recursive | ReadDir_NoDirs );
	}

	for ( size_t i = 0; i < fileList.size(); i++ )
	{
		std::string_view file    = fileList[ i ];
		std::string      fileExt = FileSys_GetFileExt( file );

		Asset_t          asset{};
		asset.path     = file;
		asset.fileName = FileSys_GetFileName( file );
		asset.ext      = fileExt;

		// TODO: improve this
		if ( fileExt == "obj" || fileExt == "gltf" || fileExt == "glb" )
		{
			asset.type = EAssetType_Model;
		}
		else if ( fileExt == "ktx" )
		{
			asset.type = EAssetType_Texture;
		}
		else if ( fileExt == "cmt" )
		{
			asset.type = EAssetType_Material;
		}
		else if ( fileExt == "png" || fileExt == "jpg" || fileExt == "jpeg" || fileExt == "webp" || fileExt == "jxl" || fileExt == "gif" )
		{
			asset.type = EAssetType_Image;
		}
		else if ( fileExt == "ogg" || fileExt == "wav" )
		{
			asset.type = EAssetType_Sound;
		}
		else if ( FileSys_IsDir( file.data(), true ) )
		{
			asset.type = EAssetType_Directory;
		}
		else
		{
			continue;
		}

		// TODO: store each asset type in it's own list?
		srData.fileList.push_back( asset );
	}
}


bool AssetBrowser_Init()
{
	TextureCreateData_t createInfo{};
	createInfo.aUsage = EImageUsage_Sampled;

	for ( u32 i = 0; i < EAssetType_Count; i++ )
	{
		render->LoadTexture( gAssetBrowserData.icons[ i ], gAssetTypeIconPaths[ i ], createInfo );

		if ( gAssetBrowserData.icons[ i ] == CH_INVALID_HANDLE )
		{
			Log_ErrorF( "Failed to Load Icon: \"%s\"\n", gAssetTypeIconPaths[ i ] );
			return false;
		}

		gAssetBrowserData.iconsImGui[ i ] = render->AddTextureToImGui( gAssetBrowserData.icons[ i ] );
	}

	return true;
}


void AssetBrowser_Draw()
{
	// set position
	int width, height;
	render->GetSurfaceSize( gGraphicsWindow, width, height );

	// ImGui::SetNextWindowSizeConstraints( { (float)width, (float)( (height) - gMainMenuBarHeight ) }, { (float)width, (float)( (height) - gMainMenuBarHeight ) } );
	ImGui::SetNextWindowSizeConstraints( { (float)width, (float)( (height) - gMainMenuBarHeight - 64 ) }, { (float)width, (float)( (height) - gMainMenuBarHeight - 64 ) } );

	if ( !ImGui::Begin( "##Asset Browser", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize ) )
	{
		gAssetBrowserSize.x = ImGui::GetWindowWidth();
		gAssetBrowserSize.y = ImGui::GetWindowHeight();
		ImGui::End();
		return;
	}

	// ImGui::Text( "Asset Browser" );
	// ImGui::SameLine();

	ImGui::SetNextItemWidth( 100 );

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

	ImGui::SetNextItemWidth( 100 );

	bool assetTypeChanged = false;

	// TODO: have a filter option for this instead
	if ( ImGui::BeginCombo( "Asset Type", gAssetTypeView == EAssetType_Count ? "All" : gAssetTypeStr[ gAssetTypeView ] ) )
	{
		assetTypeChanged = true;

		if ( ImGui::Selectable( "All" ) )
		{
			gAssetTypeView = EAssetType_Count;
		}

		for ( u32 i = 2; i < EAssetType_Count; i++ )
		{
			if ( ImGui::Selectable( gAssetTypeStr[ i ] ) )
			{
				gAssetTypeView = (EAssetType)i;
			}
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
		gAssetBrowserData.fileList.clear();
		AssetBrowser_ScanFolder( gAssetBrowserData );
	}

	ImGui::Separator();

	if ( ImGui::InputText( "Search", gAssetBrowserData.searchText, 512 ) || gAssetBrowserData.fileListDirty || assetTypeChanged )
	{
		gAssetBrowserData.searchResults.clear();

		size_t searchTextLen = strlen( gAssetBrowserData.searchText );

		u32 assetIndex = 0;
		for ( const auto& file : gAssetBrowserData.fileList )
		{
			if ( gAssetTypeView != EAssetType_Count && gAssetTypeView != file.type )
			{
				assetIndex++;
				continue;
			}

			if ( searchTextLen && file.path.find( gAssetBrowserData.searchText ) != std::string::npos )
			{
				gAssetBrowserData.searchResults.push_back( assetIndex );
			}
			else if ( searchTextLen == 0 )
			{
				gAssetBrowserData.searchResults.push_back( assetIndex );
			}

			assetIndex++;
		}

		// TODO: Sort the results depending on sorting mode
	}

	if ( gAssetBrowserData.fileListDirty )
		gAssetBrowserData.fileListDirty = false;

	ImVec2 windowSize        = ImGui::GetWindowSize();
	ImVec2 imageDisplaySize  = { 128.f, 128.f };

	bool   wrapIconList      = false;
	int    currentImageWidth = 0;
	int    imagesInRow       = 0;

	// Show Files
	if ( ImGui::BeginChild( "##Files", {}, ImGuiChildFlags_Border ) )
	{
		u32 searchResultIndex = 0;
		for ( size_t assetIndex : gAssetBrowserData.searchResults )
		{
			Asset_t& asset = gAssetBrowserData.fileList[ assetIndex ];

			if ( searchResultIndex != 0 )
				ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 4, 4 } );
			else
				ImGui::PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );

			float assetBoxX = imageDisplaySize.x + 16.f;

			if ( windowSize.x < currentImageWidth + assetBoxX + ( imagesInRow * 2 ) )  // imagesInRow is for padding
			{
				currentImageWidth = 0;
				imagesInRow       = 0;
			}
			else
			{
				ImGui::SameLine();
			}

			ImGui::PushStyleVar( ImGuiStyleVar_ChildBorderSize, 1 );

			ImVec2 textSize = ImGui::CalcTextSize( asset.fileName.data(), 0, false, imageDisplaySize.x );

			if ( ImGui::BeginChild( assetIndex + 1, { assetBoxX, imageDisplaySize.y + ( textSize.y ) + 18.f }, ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse ) )
			{
				ImGui::Image( gAssetBrowserData.iconsImGui[ asset.type ], { imageDisplaySize.x, imageDisplaySize.y } );

				if ( ImGui::IsItemHovered() )
				{
					if ( ImGui::BeginTooltip() )
					{
						ImGui::Text( asset.path.data() );
						// TextureInfo_t info = render->GetTextureInfo( CH_INVALID_HANDLE );
						// Editor_DrawTextureInfo( info );
					}
				
					ImGui::EndTooltip();
				}

				ImGui::TextWrapped( asset.fileName.data() );
			}

			ImGui::EndChild();

			ImGui::PopStyleVar();
			ImGui::PopStyleVar();

			currentImageWidth += assetBoxX;
			imagesInRow++;
			searchResultIndex++;
		}
	}

	ImGui::EndChild();

	int windowHeight = ImGui::GetWindowHeight();
	ImGui::SetWindowPos( ImVec2( 0, height - ImGui::GetWindowHeight() ), true );

	gAssetBrowserSize.x = ImGui::GetWindowWidth();
	gAssetBrowserSize.y = ImGui::GetWindowHeight();

	ImGui::End();
}

