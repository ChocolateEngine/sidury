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

MapManager *mapmanager = nullptr;

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
	if ( !mapmanager )
	{
		Log_Warn( gMapChannel, "Map Manager doesn't exist yet, oops\n" );
		return;
	}

	if ( args.size() == 0 )
	{
		Log_Warn( gMapChannel, "No Map Path/Name specified!\n" );
		return;
	}

	mapmanager->LoadMap( args[0] );
}


MapManager::MapManager()
{
	GetSkybox().Init();
}

MapManager::~MapManager()
{
}


void MapManager::Update()
{
	if ( !apMap )
		return;

	GetSkybox().Draw();

	Graphics_DrawModel( &apMap->aRenderable );
}


void MapManager::CloseMap()
{
	if ( apMap == nullptr )
		return;

	if ( apMap->aRenderable.aModel != InvalidHandle )
		Graphics_FreeModel( apMap->aRenderable.aModel );

	for ( auto physObj: apMap->aWorldPhysObjs )
		physenv->DestroyObject( physObj );

	for ( auto physShape: apMap->aWorldPhysShapes )
		physenv->DestroyShape( physShape );

	apMap->aWorldPhysObjs.clear();
	apMap->aWorldPhysShapes.clear();

	delete apMap;
	apMap = nullptr;
}


bool MapManager::LoadMap( const std::string &path )
{
	if ( apMap )
		CloseMap();

	std::string absPath = FileSys_FindDir( "maps/" + path );

	if ( absPath == "" )
	{
		Log_WarnF( gMapChannel, "Map does not exist: \"%s\"", path.c_str() );
		return false;
	}

	MapInfo *mapInfo = ParseMapInfo( absPath + "/mapInfo.smf" );

	if ( mapInfo == nullptr )
		return false;

	apMap = new SiduryMap;
	apMap->aMapInfo = mapInfo;

	GetSkybox().SetSkybox( mapInfo->skybox );

	if ( !LoadWorldModel() )
	{
		CloseMap();
		return false;
	}

	// ParseEntities( absPath + "/entities.smf" );

	SpawnPlayer();

	return true;
}


bool MapManager::LoadWorldModel()
{
	if ( !( apMap->aRenderable.aModel = Graphics_LoadModel( apMap->aMapInfo->modelPath ) ) )
	{
		return false;
	}

	// rotate the world model
	Transform mapTransform{};
	mapTransform.aAng = apMap->aMapInfo->ang;
	
	apMap->aRenderable.aModelMatrix = mapTransform.ToMatrix();
	
	// temp disabled
#if 0
	PhysicsShapeInfo shapeInfo( PhysShapeType::Mesh );
	shapeInfo.aMeshData.apModel = apMap->apWorldModel;
	
	IPhysicsShape* physShape = physenv->CreateShape( shapeInfo );
	Assert( physShape );
	
	PhysicsObjectInfo physInfo;
	physInfo.aAng = glm::radians( apMap->aMapInfo->physAng );
	
	IPhysicsObject* physObj = physenv->CreateObject( physShape, physInfo );
	
	apMap->aWorldPhysShapes.push_back( physShape );
	apMap->aWorldPhysObjs.push_back( physObj );
#endif

	return true;
}


// here so we don't need calculate sizes all the time for string comparing
struct MapInfoKeys
{
	std::string version = "version";
	std::string mapName = "mapName";
	std::string modelPath = "modelPath";
	std::string ang = "ang";
	std::string physAng = "physAng";
	std::string skybox = "skybox";
	std::string spawnPos = "spawnPos";
	std::string spawnAng = "spawnAng";
}
gMapInfoKeys;


MapInfo *MapManager::ParseMapInfo( const std::string &path )
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


void MapManager::SpawnPlayer()
{
	players->Respawn( gLocalPlayer );
}


glm::vec3 MapManager::GetSpawnPos()
{
	return (apMap) ? apMap->aMapInfo->spawnPos : glm::vec3( 0, 0, 0 );
}

glm::vec3 MapManager::GetSpawnAng()
{
	return (apMap) ? apMap->aMapInfo->spawnAng : glm::vec3( 0, 0, 0 );
}

