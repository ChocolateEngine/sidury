// Loads Sidury Map Files (smf)

#include "core/filesystem.h"
#include "core/console.h"
#include "util.h"
#include "main.h"
#include "game_shared.h"
#include "player.h"
#include "skybox.h"
#include "sv_main.h"
#include "graphics/graphics.h"

#include "mapmanager.h"

#include "speedykeyv/KeyValue.h"


LOG_REGISTER_CHANNEL2( Map, LogColor::DarkGreen );

SiduryMap*    gpMap      = nullptr;

extern Entity gLocalPlayer;


void map_dropdown(
	const std::vector< std::string >& args,  // arguments currently typed in by the user
	std::vector< std::string >& results )      // results to populate the dropdown list with
{
	for ( const auto& file: FileSys_ScanDir( "maps", ReadDir_AllPaths | ReadDir_NoFiles ) )
	{
		if ( file.ends_with( ".." ) )
			continue;

		std::string mapName = FileSys_GetFileName( file );

		if ( args.size() && !mapName.starts_with( args[0] ) )
			continue;

		results.push_back( mapName );
	}
}


CONCMD_DROP( map, map_dropdown )
{
	if ( args.size() == 0 )
	{
		Log_Warn( gLC_Map, "No Map Path/Name specified!\n" );
		return;
	}

	// Map Command is always on server, but the function to load can be loaded on client
	// like when connecting to a server
	bool old = Game_ProcessingClient();
	Game_SetClient( false );

	if ( !MapManager_FindMap( args[ 0 ] ) )
	{
		Log_Error( "Failed to Find map - Failed to Start Server\n" );
		return;
	}

	if ( !SV_StartServer() )
	{
		Game_SetClient( old );
		return;
	}

	if ( !MapManager_LoadMap( args[ 0 ] ) )
	{
		Log_Error( "Failed to Load map - Failed to Start Server\n" );
		SV_StopServer();
		return;
	}

	Game_SetClient( old );

	if ( Game_GetCommandSource() == ECommandSource_Console )
	{
		Con_RunCommandArgs( "connect", { "localhost" } );
	}
}


void MapManager_Update()
{
	if ( !gpMap )
		return;
}


void MapManager_CloseMap()
{
	if ( gpMap == nullptr )
		return;

	if ( gpMap->aScene != InvalidHandle )
	{
		Graphics_RemoveSceneDraw( gpMap->aRenderable );
		Graphics_FreeScene( gpMap->aScene );
	}

	for ( auto physObj : gpMap->aWorldPhysObjs )
		GetPhysEnv()->DestroyObject( physObj );

	for ( auto physShape : gpMap->aWorldPhysShapes )
		GetPhysEnv()->DestroyShape( physShape );

	gpMap->aWorldPhysObjs.clear();
	gpMap->aWorldPhysShapes.clear();

	delete gpMap;
	gpMap = nullptr;
}


bool MapManager_FindMap( const std::string& path )
{
	std::string absPath = FileSys_FindDir( "maps/" + path );

	if ( absPath == "" )
		return false;

	return true;
}


static Entity gMapEntity = CH_ENT_INVALID;


// Temporary, will be removed
bool MapManager_LoadLegacyV1Map( const std::string &path )
{
	MapInfo* mapInfo = MapManager_ParseMapInfo( path + "/mapInfo.smf" );

	if ( mapInfo == nullptr )
		return false;

	gpMap           = new SiduryMap;
	gpMap->aMapPath = path;
	gpMap->aMapInfo = mapInfo;

	Skybox_SetMaterial( mapInfo->skybox );

	if ( !MapManager_LoadWorldModel() )
	{
		MapManager_CloseMap();
		return false;
	}

	// ParseEntities( absPath + "/entities.smf" );

	// the map should always be entity index 0
	gMapEntity = GetEntitySystem()->CreateEntity();

	MapManager_SpawnPlayer();

	// rotate the world model
	glm::mat4 modelMatrix      = Util_ToMatrix( nullptr, &gpMap->aMapInfo->ang );

	// gpMap->aRenderable = Graphics_AddSceneDraw( gpMap->aScene );

	gpMap->aRenderable         = new SceneDraw_t;
	gpMap->aRenderable->aScene = gpMap->aScene;

	gpMap->aRenderable->aDraw.resize( Graphics_GetSceneModelCount( gpMap->aScene ) );

	for ( uint32_t i = 0; i < gpMap->aRenderable->aDraw.size(); i++ )
	{
		gpMap->aRenderable->aDraw[ i ] = Graphics_CreateRenderable( Graphics_GetSceneModel( gpMap->aScene, i ) );

		if ( Renderable_t* renderable = Graphics_GetRenderableData( gpMap->aRenderable->aDraw[ i ] ) )
		{
			renderable->aModelMatrix = modelMatrix;
			renderable->aAABB        = Graphics_CreateWorldAABB( modelMatrix, renderable->aAABB );
		}
	}

	return true;
}


SiduryMap* MapManager_ReadMapHeader( const std::vector< char >& srMapData )
{
	if ( srMapData.size() < sizeof( SiduryMapHeader_t ) )
	{
		Log_ErrorF( gLC_Map, "Map Data is Less than the size of the map header (%zd < %zd)", srMapData.size(), sizeof( SiduryMapHeader_t ) );
		return nullptr;
	}

	SiduryMapHeader_t* header = (SiduryMapHeader_t*)srMapData.data();
}


ESiduryMapCmd MapManager_GetNextCommand( const std::vector< char >& srMapData, SiduryMap* spMap )
{
	return ESiduryMapCmd_Invalid;
}


void MapManager_ReadCommand( const std::vector< char >& srMapData, SiduryMap* spMap )
{
	
}


bool MapManager_LoadMap( const std::string &path )
{
	if ( gpMap )
		MapManager_CloseMap();

	std::string absPath = FileSys_FindDir( "maps/" + path );

	if ( absPath.empty() )
	{
		Log_WarnF( gLC_Map, "Map does not exist: \"%s\"", path.c_str() );
		return false;
	}

	Log_DevF( gLC_Map, 1, "Loading Map: %s\n", path.c_str() );

	std::string mapInfoPath = FileSys_FindFile( absPath + "/mapInfo.smf" );
	if ( FileSys_FindFile( absPath + "/mapInfo.smf" ).size() )
	{
		// It's a Legacy Map
		return MapManager_LoadLegacyV1Map( absPath );
	}

	std::string mapDataPath = FileSys_FindFile( path + "/mapData.smf" );

	if ( mapDataPath.empty() )
	{
		Log_WarnF( gLC_Map, "Map does not contain a mapData.smf file: \"%s\"", path.c_str() );
		return false;
	}

	std::vector< char > mapData = FileSys_ReadFile( mapDataPath );

	if ( mapData.empty() )
	{
		Log_ErrorF( gLC_Map, "Map data file is empty: \"%s\"", path.c_str() );
		return false;
	}

	SiduryMap* map = MapManager_ReadMapHeader( mapData );

	if ( !map )
	{
		Log_ErrorF( gLC_Map, "Failed to read map header: \"%s\"", path.c_str() );
		return false;
	}

	// return sceneDraw;
	// 
	// int      heap = _heapchk();
	// 
	// uint32_t i = 0;
	// for ( auto& renderable : gpMap->aRenderable->aDraw )
	// {
	// 	renderable->aModelMatrix = modelMatrix;
	// 	renderable->aAABB        = Graphics_CreateWorldAABB( modelMatrix, renderable->aAABB );
	// 	i++;
	// 
	// 	_heapchk();
	// }

	return true;
}


SiduryMap* MapManager_CreateMap()
{
	return nullptr;
}


bool MapManager_HasMap()
{
	return gpMap != nullptr;
}


std::string_view MapManager_GetMapName()
{
	if ( !gpMap )
		return "";

	return gpMap->aMapInfo->mapName;
}


std::string_view MapManager_GetMapPath()
{
	if ( !gpMap )
		return "";

	return gpMap->aMapPath;
}


bool MapManager_LoadWorldModel()
{
	if ( !( gpMap->aScene = Graphics_LoadScene( gpMap->aMapInfo->modelPath ) ) )
		return false;

	PhysicsShapeInfo shapeInfo( PhysShapeType::Mesh );
	
	for ( size_t i = 0; i < Graphics_GetSceneModelCount( gpMap->aScene ); i++ )
	{
		Handle       model     = Graphics_GetSceneModel( gpMap->aScene, i );
		// Renderable_t* modelDraw = Graphics_AddModelDraw( model );
		// 
		// if ( modelDraw )
		// 	continue;
		// 
		// modelDraw->aModel       = model;
		// modelDraw->aModelMatrix = modelMatrix;

#if 1
		Phys_GetModelInd( model, shapeInfo.aConcaveData );
#endif
	}

#if 1
	IPhysicsShape* physShape = GetPhysEnv()->CreateShape( shapeInfo );

	if ( physShape == nullptr )
		return false;

	Assert( physShape );

	PhysicsObjectInfo physInfo;
	physInfo.aAng           = glm::radians( gpMap->aMapInfo->physAng );

	IPhysicsObject* physObj = GetPhysEnv()->CreateObject( physShape, physInfo );

	gpMap->aWorldPhysShapes.push_back( physShape );
	gpMap->aWorldPhysObjs.push_back( physObj );
#endif

	return true;
}


// here so we don't need calculate sizes all the time for string comparing
struct MapInfoKeys
{
	std::string version   = "version";
	std::string mapName   = "mapName";
	std::string modelPath = "modelPath";
	std::string ang       = "ang";
	std::string physAng   = "physAng";
	std::string skybox    = "skybox";
	std::string spawnPos  = "spawnPos";
	std::string spawnAng  = "spawnAng";
}
gMapInfoKeys;


MapInfo *MapManager_ParseMapInfo( const std::string &path )
{
	if ( !FileSys_Exists( path ) )
	{
		Log_WarnF( gLC_Map, "Map Info does not exist: \"%s\"", path.c_str() );
		return nullptr;
	}

	std::vector< char > rawData = FileSys_ReadFile( path );

	if ( rawData.empty() )
	{
		Log_WarnF( gLC_Map, "Failed to read file: %s\n", path.c_str() );
		return nullptr;
	}

	// append a null terminator for c strings
	rawData.push_back( '\0' );

	KeyValueRoot kvRoot;
	KeyValueErrorCode err = kvRoot.Parse( rawData.data() );

	if ( err != KeyValueErrorCode::NO_ERROR )
	{
		Log_WarnF( gLC_Map, "Failed to parse file: %s\n", path.c_str() );
		return nullptr;
	}

	// parsing time
	kvRoot.Solidify();

	KeyValue *kvShader = kvRoot.children;

	MapInfo *mapInfo = new MapInfo;

	KeyValue *kv = kvShader->children;
	for ( int i = 0; i < kvShader->childCount; i++ )
	{
		if ( kv->hasChildren )
		{
			Log_MsgF( gLC_Map, "Skipping extra children in kv file: %s", path.c_str() );
			kv = kv->next;
			continue;
		}

		else if ( gMapInfoKeys.version == kv->key.string )
		{
			mapInfo->version = ToLong( kv->value.string, 0 );
			if ( mapInfo->version != MAP_VERSION )
			{
				Log_ErrorF( gLC_Map, "Invalid Version: %ud - Expected Version %ud\n", mapInfo->version, MAP_VERSION );
				delete mapInfo;
				return nullptr;
			}
		}

		else if ( gMapInfoKeys.mapName == kv->key.string )
		{
			mapInfo->mapName = kv->value.string;
		}

		else if ( gMapInfoKeys.modelPath == kv->key.string )
		{
			mapInfo->modelPath = kv->value.string;

			if ( mapInfo->modelPath == "" )
			{
				Log_WarnF( gLC_Map, "Empty Model Path for map \"%s\"\n", path.c_str() );
				delete mapInfo;
				return nullptr;
			}
		}

		else if ( gMapInfoKeys.skybox == kv->key.string )
			mapInfo->skybox = kv->value.string;

		else if ( gMapInfoKeys.ang == kv->key.string )
			mapInfo->ang = KV_GetVec3( kv->value.string );

		else if ( gMapInfoKeys.physAng == kv->key.string )
			mapInfo->physAng = KV_GetVec3( kv->value.string );

		else if ( gMapInfoKeys.spawnPos == kv->key.string )
			mapInfo->spawnPos = KV_GetVec3( kv->value.string );

		else if ( gMapInfoKeys.spawnAng == kv->key.string )
			mapInfo->spawnAng = KV_GetVec3( kv->value.string );

		kv = kv->next;
	}

	return mapInfo;
}


void MapManager_SpawnPlayer()
{
	// players->Respawn( gLocalPlayer );
}


glm::vec3 MapManager_GetSpawnPos()
{
	return ( gpMap ) ? gpMap->aMapInfo->spawnPos : glm::vec3( 0, 0, 0 );
}

glm::vec3 MapManager_GetSpawnAng()
{
	return ( gpMap ) ? gpMap->aMapInfo->spawnAng : glm::vec3( 0, 0, 0 );
}

