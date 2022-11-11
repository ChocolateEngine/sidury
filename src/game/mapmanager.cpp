/* 

Loads Sidury Map Files (smf)

*/

#include "core/filesystem.h"
#include "core/console.h"
#include "util.h"
#include "main.h"
#include "player.h"
#include "skybox.h"
#include "graphics/graphics.h"

#include "mapmanager.h"

#include "speedykeyv/KeyValue.h"


LOG_REGISTER_CHANNEL( Map, LogColor::DarkGreen );

SiduryMap*    gpMap      = nullptr;

extern ConVar velocity_scale;
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
		Log_Warn( gMapChannel, "No Map Path/Name specified!\n" );
		return;
	}

	MapManager_LoadMap( args[0] );
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

	if ( gpMap->aRenderable.aScene != InvalidHandle )
	{
		Graphics_RemoveSceneDraw( &gpMap->aRenderable );
		Graphics_FreeScene( gpMap->aRenderable.aScene );
	}

	for ( auto physObj : gpMap->aWorldPhysObjs )
		physenv->DestroyObject( physObj );

	for ( auto physShape : gpMap->aWorldPhysShapes )
		physenv->DestroyShape( physShape );

	gpMap->aWorldPhysObjs.clear();
	gpMap->aWorldPhysShapes.clear();

	delete gpMap;
	gpMap = nullptr;
}


bool MapManager_LoadMap( const std::string &path )
{
	if ( gpMap )
		MapManager_CloseMap();

	std::string absPath = FileSys_FindDir( "maps/" + path );

	if ( absPath == "" )
	{
		Log_WarnF( gMapChannel, "Map does not exist: \"%s\"", path.c_str() );
		return false;
	}

	MapInfo* mapInfo = MapManager_ParseMapInfo( absPath + "/mapInfo.smf" );

	if ( mapInfo == nullptr )
		return false;

	gpMap = new SiduryMap;
	gpMap->aMapInfo = mapInfo;

	Skybox_SetMaterial( mapInfo->skybox );

	if ( !MapManager_LoadWorldModel() )
	{
		MapManager_CloseMap();
		return false;
	}

	// ParseEntities( absPath + "/entities.smf" );

	MapManager_SpawnPlayer();

	Graphics_AddSceneDraw( &gpMap->aRenderable );

	return true;
}


bool MapManager_HasMap()
{
	return gpMap != nullptr;
}


bool MapManager_LoadWorldModel()
{
	if ( !( gpMap->aRenderable.aScene = Graphics_LoadScene( gpMap->aMapInfo->modelPath ) ) )
		return false;

	// rotate the world model
	glm::mat4 modelMatrix;
	Util_ToMatrix( modelMatrix, {}, gpMap->aMapInfo->ang );
	
	for ( size_t i = 0; i < Graphics_GetSceneModelCount( gpMap->aRenderable.aScene ); i++ )
	{
		Handle           model = Graphics_GetSceneModel( gpMap->aRenderable.aScene, i );

		gpMap->aRenderable.aDraw.emplace_back( model, modelMatrix );

#if 0
		PhysicsShapeInfo shapeInfo( PhysShapeType::Mesh );

		Phys_GetModelInd( model, shapeInfo.aConcaveData );
	
		IPhysicsShape* physShape = physenv->CreateShape( shapeInfo );

		if ( physShape == nullptr )
			return false;

		Assert( physShape );
	
		PhysicsObjectInfo physInfo;
		physInfo.aAng           = glm::radians( gpMap->aMapInfo->physAng );
	
		IPhysicsObject* physObj = physenv->CreateObject( physShape, physInfo );
	
		gpMap->aWorldPhysShapes.push_back( physShape );
		gpMap->aWorldPhysObjs.push_back( physObj );
#endif
	}

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
		Log_WarnF( gMapChannel, "Map Info does not exist: \"%s\"", path.c_str() );
		return nullptr;
	}

	std::vector< char > rawData = FileSys_ReadFile( path );

	if ( rawData.empty() )
	{
		Log_WarnF( gMapChannel, "Failed to read file: %s\n", path.c_str() );
		return nullptr;
	}

	// append a null terminator for c strings
	rawData.push_back( '\0' );

	KeyValueRoot kvRoot;
	KeyValueErrorCode err = kvRoot.Parse( rawData.data() );

	if ( err != KeyValueErrorCode::NO_ERROR )
	{
		Log_WarnF( gMapChannel, "Failed to parse file: %s\n", path.c_str() );
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
			Log_MsgF( gMapChannel, "Skipping extra children in kv file: %s", path.c_str() );
			kv = kv->next;
			continue;
		}

		else if ( gMapInfoKeys.version == kv->key.string )
		{
			mapInfo->version = ToLong( kv->value.string, 0 );
			if ( mapInfo->version != MAP_VERSION )
			{
				Log_ErrorF( gMapChannel, "Invalid Version: %ud - Expected Version %ud\n", mapInfo->version, MAP_VERSION );
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
				Log_WarnF( gMapChannel, "Empty Model Path for map \"%s\"\n", path.c_str() );
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
	players->Respawn( gLocalPlayer );
}


glm::vec3 MapManager_GetSpawnPos()
{
	return ( gpMap ) ? gpMap->aMapInfo->spawnPos : glm::vec3( 0, 0, 0 );
}

glm::vec3 MapManager_GetSpawnAng()
{
	return ( gpMap ) ? gpMap->aMapInfo->spawnAng : glm::vec3( 0, 0, 0 );
}

