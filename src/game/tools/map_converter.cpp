#include "map_converter.h"


// IDEA: what if this was in another dll, and we loaded it and passed in information for converting maps
// like the component registry


CONCMD_VA( tool_map_convert_run, 0,
	"Convert a Map to the Sidury Map Format (tool_map_convert_run \"inputPath\" \"outputName\" \"assetsPaths\" \"assetsOut\")" )
{
	if ( args.size() < 4 )
		return;

	MapConverter_ConvertMap( args[ 0 ], args[ 1 ], args[ 2 ], args[ 3 ] );
}


// CONCMD_VA( tool_map_convert_run_assets, 0,
// 	"Convert Assets to Chocolate Engine Formats (tool_map_convert_run_assets \"srcAssets\" \"outAssets\")" )
// {
// 	if ( args.size() < 2 )
// 		return;
// 
// 	MapConverter_ConvertAllAssets( args[ 0 ], args[ 1 ] );
// }


// ============================================================


bool MapConverter_Init()
{
	return true;
}


void MapConverter_Shutdown()
{
}


void MapConverter_Update()
{
}


void MapConverter_DrawUI()
{
}


// ============================================================


void MapConverter_ConvertMap( const std::string& srPath, const std::string& srOutPath, const std::string& srAssetPath, const std::string& srAssetsOut )
{
	// std::string absPath = FileSys_FindFile( srPath );
	// 
	// if ( absPath.empty() )
	// {
	// 	Log_WarnF( "Map does not exist: \"%s\"", srPath.c_str() );
	// 	return;
	// }
	// 
	// Log_MsgF( "Converting Map: %s\n", srPath.c_str() );
	// 
	// std::string fileExt = FileSys_GetFileExt( srPath );
	// 
	// if ( fileExt == "vmf" )
	// {
	// 	MapConverter_ConvertVMF( srPath, srOutPath, srAssetPath, srAssetsOut );
	// }
	// else
	// {
	// 	Log_ErrorF( "Unknown Map Format: %s\n", srPath.c_str() );
	// 	return;
	// }
}


void MapConverter_ConvertAsset()
{
}


void MapConverter_ConvertAllAssets( const std::string& srSrcAssets, const std::string& srOutAssetss )
{
}

