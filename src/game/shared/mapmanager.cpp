// Loads Sidury Map Files (smf)

#include "core/filesystem.h"
#include "core/console.h"
#include "util.h"
#include "main.h"
#include "game_shared.h"
#include "player.h"
#include "igraphics.h"
#include "mapmanager.h"
#include "skybox.h"

#include "map_system.h"

#if CH_SERVER
	#include "../server/sv_main.h"
#endif

#include "speedykeyv/KeyValue.h"

#include <filesystem>


LOG_REGISTER_CHANNEL2( Map, LogColor::DarkGreen );

SiduryMap*    gpMap    = nullptr;

chmap::Map*   gpChMap  = nullptr;
std::string   gMapPath = "";


enum ESMF_CommandVersion : u16
{
	ESMF_CommandVersion_Invalid       = 0,
	ESMF_CommandVersion_EntityList    = 1,
	ESMF_CommandVersion_Skybox        = 1,
	ESMF_CommandVersion_ComponentList = 2,
};


void MapManager_Update()
{
}


void MapManager_CloseMap()
{
	if ( gpChMap )
	{
		chmap::Free( gpChMap );
		gpChMap = nullptr;
	}

	if ( gMapPath.size() )
	{
		FileSys_RemoveSearchPath( gMapPath.data(), gMapPath.size(), ESearchPathType_Path );
	}

	gMapPath.clear();

	if ( gpMap == nullptr )
		return;

	for ( Entity entity : gpMap->aMapEntities )
	{
		Entity_DeleteEntity( entity );
	}
	
	gpMap->aMapEntities.clear();

	delete gpMap;
	gpMap = nullptr;
}


ch_string MapManager_FindMap( const std::string& path )
{
	ch_string_auto mapPath;

	if ( FileSys_IsAbsolute( path.c_str() ) )
	{
		mapPath = ch_str_copy( path.data(), path.size() );
	}
	else
	{
		const char* strings[] = { "maps/", path.c_str() };
		const u64   lengths[] = { 5, path.size() };
		mapPath               = ch_str_concat( 2, strings, lengths );
	}

	ch_string absPath = FileSys_FindDir( mapPath.data, mapPath.size );
	return absPath;
}


bool MapManager_MapExists( const std::string& path )
{
	ch_string_auto mapPath = MapManager_FindMap( path );
	return mapPath.data;
}


// Temporary, will be removed
bool MapManager_LoadLegacyV1Map( const std::string &path )
{
	MapInfo* mapInfo = MapManager_ParseMapInfo( path + CH_PATH_SEP_STR "mapInfo.smf" );

	if ( mapInfo == nullptr )
		return false;

	gpMap           = new SiduryMap;
	gpMap->aMapPath = path;
	gpMap->aMapInfo = mapInfo;

#if CH_CLIENT
	// Skybox_SetMaterial( mapInfo->skybox );
#endif

	if ( !MapManager_LoadWorldModel() )
	{
		MapManager_CloseMap();
		return false;
	}

	Entity skyboxEnt = Entity_CreateEntity();
	
	if ( skyboxEnt == CH_ENT_INVALID )
	{
		Log_ErrorF( gLC_Map, "Failed to create skybox Entity\n" );
		return false;
	}

	CSkybox* skybox = Ent_AddComponent< CSkybox >( skyboxEnt, "skybox" );

	CH_ASSERT( skybox );

	skybox->aMaterialPath = mapInfo->skybox;

#if CH_SERVER
	Entity playerSpawnEnt = Entity_CreateEntity( false );

	if ( playerSpawnEnt == CH_ENT_INVALID )
	{
		Log_ErrorF( gLC_Map, "Failed to create playerSpawn Entity\n" );
		return false;
	}

	auto spawnTransform = Ent_AddComponent< CTransform >( playerSpawnEnt, "transform" );
	auto spawnComp      = Ent_AddComponent< CPlayerSpawn >( playerSpawnEnt, "playerSpawn" );

	CH_ASSERT( spawnTransform );
	CH_ASSERT( spawnComp );

	spawnTransform->aPos          = gpMap->aMapInfo->spawnPos;
	spawnTransform->aAng          = gpMap->aMapInfo->spawnAng;
	spawnTransform->aScale.Edit() = { 1.f, 1.f, 1.f };

	gpMap->aMapEntities.push_back( playerSpawnEnt );

	// Create a World Light Entity
	if ( gpMap->aMapInfo->worldLight )
	{
		Entity worldLightEnt = Entity_CreateEntity();

		if ( worldLightEnt == CH_ENT_INVALID )
		{
			Log_ErrorF( gLC_Map, "Failed to create worldLight Entity\n" );
			return false;
		}

		auto transform = Ent_AddComponent< CTransform >( worldLightEnt, "transform" );
		auto light     = Ent_AddComponent< CLight >( worldLightEnt, "light" );

		transform->aAng      = gpMap->aMapInfo->worldLightAng;

		light->aColor        = gpMap->aMapInfo->worldLightColor;
		light->aUseTransform = true;

		gpMap->aMapEntities.push_back( worldLightEnt );
	}
#endif

	return true;
}



static bool MapManager_LoadScene( chmap::Scene& scene )
{
	std::unordered_map< u32, ChHandle_t > entityHandles;

	for ( chmap::Entity& mapEntity : scene.entites )
	{
		Entity ent = Entity_CreateEntity();

		if ( ent == CH_ENT_INVALID )
		{
			return false;
		}

		// Entity_SetName( ent, mapEntity.name );
		entityHandles[ mapEntity.id ] = ent;

		auto transform                = Ent_AddComponent< CTransform >( ent, "transform" );

		transform->aPos               = mapEntity.pos;
		transform->aAng               = mapEntity.ang;
		transform->aScale             = mapEntity.scale;

		// Check Built in components (TODO: IMPROVE THIS)
		for ( chmap::Component& comp : mapEntity.components )
		{
			// Load a renderable
			if ( ch_str_equals( comp.name, "renderable", 10 ) )
			{
				auto it = comp.values.find( "path" );
				if ( it == comp.values.end() )
				{
					Log_Error( gLC_Map, "Failed to find renderable model path in component\n" );
					continue;
				}

				if ( it->second.type != chmap::EComponentType_String )
					continue;

				auto renderable   = Ent_AddComponent< CRenderable >( ent, "renderable" );
				renderable->aPath = it->second.aString.data;

				// Load other renderable data
				// for ( const auto& [ name, compValue ] : comp.values )
				// {
				// }
			}
			else if ( ch_str_equals( comp.name, "light", 5 ) )
			{
				auto it = comp.values.find( "type" );
				if ( it == comp.values.end() )
				{
					Log_Error( gLC_Map, "Failed to find light type in component\n" );
					continue;
				}

				if ( it->second.type != chmap::EComponentType_String )
					continue;

				auto light = Ent_AddComponent< CLight >( ent, "light" );

				if ( ch_str_equals( it->second.aString, "world", 5 ) )
				{
					light->aType = ELightType_World;
				}
				else if ( ch_str_equals( it->second.aString, "point", 5 ) )
				{
					light->aType = ELightType_Point;
				}
				else if ( ch_str_equals( it->second.aString, "spot", 4 ) )
				{
					light->aType = ELightType_Spot;
				}
				// else if ( ch_str_equals( it->second.aString, "capsule" ))
				// {
				// 	light->aType = ELightType_Capsule;
				// }
				else
				{
					Log_ErrorF( gLC_Map, "Unknown Light Type: %s\n", it->second.aString.data );
					continue;
				}

				// Read the rest of the light data
				for ( const auto& [ name, compValue ] : comp.values )
				{
					if ( ch_str_equals( name.data(), name.size(), "color", 5 ) )
					{
						if ( compValue.type != chmap::EComponentType_Vec4 )
							continue;

						light->aColor = compValue.aVec4;
					}
					else if ( ch_str_equals( name.data(), name.size(), "radius", 6 ) )
					{
						if ( compValue.type == chmap::EComponentType_Int )
							light->aRadius = compValue.aInteger;

						else if ( compValue.type == chmap::EComponentType_Double )
							light->aRadius = compValue.aDouble;
					}
				}
			}
			else if ( ch_str_equals( comp.name, "phys_object", 11 ) )
			{
				auto it = comp.values.find( "path" );
				if ( it == comp.values.end() )
				{
					Log_Error( gLC_Map, "Failed to find physics object path in component\n" );
					continue;
				}

				auto itType = comp.values.find( "type" );
				if ( itType == comp.values.end() )
				{
					Log_Error( gLC_Map, "Failed to find physics object type in component\n" );
					continue;
				}

				// why did you keep it split up like this?
				auto physShape   = Ent_AddComponent< CPhysShape >( ent, "physShape" );
				auto physObject  = Ent_AddComponent< CPhysObject >( ent, "physObject" );

				physShape->aPath = it->second.aString.data;

				if ( ch_str_equals( itType->second.aString, "convex", 6 ) )
				{
					physObject->aStartActive   = true;
					physObject->aMass          = 10.f;
					physObject->aTransformMode = EPhysTransformMode_Update;
					physShape->aShapeType      = PhysShapeType::Convex;
				}
				else if ( ch_str_equals( itType->second.aString, "static_compound", 15 ) )
				{
					physObject->aStartActive   = true;
					physObject->aMass          = 10.f;
					physObject->aCustomMass    = true;
					physObject->aTransformMode = EPhysTransformMode_Update;
					physObject->aMotionType    = PhysMotionType::Dynamic;
					physObject->aAllowSleeping = false;
					physShape->aShapeType      = PhysShapeType::StaticCompound;
				}
				else if ( ch_str_equals( itType->second.aString, "mesh", 4 ) )
					physShape->aShapeType = PhysShapeType::Mesh;
				else
					physShape->aShapeType = PhysShapeType::Convex;

			}
			else
			{
				// TODO: Try to search for this component

			}
		}
	}

	// Check entity parents
	for ( chmap::Entity& mapEntity : scene.entites )
	{
		if ( mapEntity.parent == UINT32_MAX )
			continue;

		auto itID     = entityHandles.find( mapEntity.id );
		auto itParent = entityHandles.find( mapEntity.parent );

		if ( itID == entityHandles.end() || itParent == entityHandles.end() )
		{
			Log_ErrorF( "Failed to parent entity %d", mapEntity.id );
			continue;
		}

		Entity_ParentEntity( itID->second, itParent->second );
	}
}


bool MapManager_LoadMap( const std::string& path )
{
	ch_string absPath = MapManager_FindMap( path );

	if ( !absPath.data )
	{
		Log_WarnF( gLC_Map, "Map does not exist: \"%s\"\n", path.c_str() );
		return false;
	}

	chmap::Map* map = chmap::Load( absPath.data, absPath.size );

	if ( map == nullptr )
	{
		Log_ErrorF( gLC_Map, "Failed to Load Map: \"%s\"\n", path.c_str() );
		return false;
	}

	FileSys_InsertSearchPath( 0, absPath.data, absPath.size );

	// Only load the primary scene for now
	// Each scene gets it's own editor context
	// TODO: make an editor project system
	if ( !MapManager_LoadScene( map->scenes[ map->primaryScene ] ) )
	{
		Log_ErrorF( gLC_Map, "Failed to Load Primary Scene: \"%s\" - Scene \"%s\"\n", path.c_str(), map->scenes[ map->primaryScene ].name );
		return false;
	}

	if ( map->skybox )
	{
		Entity skyboxEnt      = Entity_CreateEntity();
		auto   skybox         = Ent_AddComponent< CSkybox >( skyboxEnt, "skybox" );
		skybox->aMaterialPath = map->skybox;
	}

	return true;
}


SiduryMap* MapManager_CreateMap()
{
	return nullptr;
}


#if 0
void MapManager_WriteMap( const std::string& srPath )
{
	// Must be in a map to save it
	if ( !Game_InMap() )
		return;

	// Build Map Format Commands
	std::vector< fb::FlatBufferBuilder >     commands;
	std::vector< fb::Offset< SMF_Command > > commandsBuilt;
	fb::FlatBufferBuilder                    rootBuilder;

	MapManager_BuildCommand( rootBuilder, commandsBuilt, ESMF_Command_Skybox );
	MapManager_BuildCommand( rootBuilder, commandsBuilt, ESMF_Command_EntityList );
	MapManager_BuildCommand( rootBuilder, commandsBuilt, ESMF_Command_ComponentList );

	// Build the main map format data table
	auto                  commandListOffset = rootBuilder.CreateVector( commandsBuilt.data(), commandsBuilt.size() );

	SMF_DataBuilder       mapDataBuilder( rootBuilder );
	mapDataBuilder.add_commands( commandListOffset );
	rootBuilder.Finish( mapDataBuilder.Finish() );

	std::string outBuffer;

	std::string basePath = srPath;
	if ( FileSys_IsRelative( srPath.c_str() ) )
	{
		basePath = std::filesystem::current_path().string() + "/maps/" + srPath;
	}

	// Find an Empty Filename/Path to use so we don't overwrite anything
	std::string outPath       = basePath;
	size_t      renameCounter = 0;
	// while ( FileSys_Exists( outPath ) )
	// {
	// 	outPath = vstring( "%s_%zd", basePath.c_str(), ++renameCounter );
	// }

	// Open the file handle
	if ( !std::filesystem::create_directories( outPath ) )
	{
		Log_ErrorF( gLC_Map, "Failed to create directory for map: %s\n", outPath.c_str() );
		return;
	}

	std::string mapDataPath = outPath + "/mapData.smf";

	// Write the data
	FILE*       fp          = fopen( mapDataPath.c_str(), "wb" );

	if ( fp == nullptr )
	{
		Log_ErrorF( gLC_Map, "Failed to open file handle for mapData.smf: \"%s\"\n", mapDataPath.c_str() );
		return;
	}
	
	// Write Map Header
	SiduryMapHeader_t mapHeader;
	mapHeader.aVersion   = CH_MAP_VERSION;
	mapHeader.aSignature = CH_MAP_SIGNATURE;
	fwrite( &mapHeader, sizeof( SiduryMapHeader_t ), 1, fp );

	// Write the commands
	// fwrite( outBuffer.c_str(), sizeof( char ), outBuffer.size(), fp );
	fwrite( rootBuilder.GetBufferPointer(), sizeof( u8 ), rootBuilder.GetSize(), fp );
	fclose( fp );

	// If we had to use a custom name for it, rename the old file, and rename the new file to what we wanted to save it as
	if ( renameCounter > 0 )
	{
	}
}
#endif


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
	Entity worldEntity = Entity_CreateEntity();

	if ( worldEntity == CH_ENT_INVALID )
	{
		Log_ErrorF( gLC_Map, "Failed to create Legacy World Model Entity\n" );
		return false;
	}

	gpMap->aMapEntities.push_back( worldEntity );

	auto transform  = Ent_AddComponent< CTransform >( worldEntity, "transform" );
	auto renderable = Ent_AddComponent< CRenderable >( worldEntity, "renderable" );
	auto physShape  = Ent_AddComponent< CPhysShape >( worldEntity, "physShape" );
	auto physObject = Ent_AddComponent< CPhysObject >( worldEntity, "physObject" );

	CH_ASSERT( transform );
	CH_ASSERT( renderable );
	CH_ASSERT( physShape );
	CH_ASSERT( physObject );

	renderable->aPath = gpMap->aMapInfo->modelPath;

	// rotate the world model
	transform->aAng = gpMap->aMapInfo->ang;

	// TODO: Have a root map entity with transform, and probably a physics shape for StaticCompound
	// then in CPhysShape, add an option to use an entity and everything parented to that

	physShape->aShapeType      = PhysShapeType::Mesh;
	physShape->aPath           = gpMap->aMapInfo->modelPath;

	physObject->aGravity       = false;
	physObject->aTransformMode = EPhysTransformMode_Inherit;

	return true;
}


// here so we don't need calculate sizes all the time for string comparing
struct MapInfoKeys
{
	std::string version         = "version";
	std::string mapName         = "mapName";
	std::string modelPath       = "modelPath";
	std::string ang             = "ang";
	std::string physAng         = "physAng";
	std::string skybox          = "skybox";
	std::string spawnPos        = "spawnPos";
	std::string spawnAng        = "spawnAng";
	std::string worldLight      = "worldLight";
	std::string worldLightAng   = "worldLightAng";
	std::string worldLightColor = "worldLightColor";
}
gMapInfoKeys;


MapInfo *MapManager_ParseMapInfo( const std::string &path )
{
	if ( !FileSys_Exists( path.data(), path.size() ) )
	{
		Log_WarnF( gLC_Map, "Map Info does not exist: \"%s\"", path.data() );
		return nullptr;
	}

	ch_string_auto rawData = FileSys_ReadFile( path.data(), path.size() );

	if ( !rawData.data )
	{
		Log_WarnF( gLC_Map, "Failed to read file: %s\n", path.data() );
		return nullptr;
	}

	KeyValueRoot kvRoot;
	KeyValueErrorCode err = kvRoot.Parse( rawData.data );

	if ( err != KeyValueErrorCode::NO_ERROR )
	{
		Log_WarnF( gLC_Map, "Failed to parse file: %s\n", path.data() );
		return nullptr;
	}

	// parsing time
	kvRoot.Solidify();

	KeyValue *kvShader  = kvRoot.children;

	MapInfo*  mapInfo   = new MapInfo;
	mapInfo->worldLight = false;

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

		// we skip this one cause it was made with incorrect matrix rotations
		// else if ( gMapInfoKeys.ang == kv->key.string )
		// 	continue;

		// physAng contains the correct values, so we only use that now
		else if ( gMapInfoKeys.physAng == kv->key.string )
			mapInfo->ang = KV_GetVec3( kv->value.string );

		// Spawn Info
		else if ( gMapInfoKeys.spawnPos == kv->key.string )
			mapInfo->spawnPos = KV_GetVec3( kv->value.string );

		else if ( gMapInfoKeys.spawnAng == kv->key.string )
			mapInfo->spawnAng = KV_GetVec3( kv->value.string );
		
		// World Light Info
		else if ( gMapInfoKeys.worldLight == kv->key.string )
		{
			if ( strcmp( kv->value.string, "1" ) == 0 )
				mapInfo->worldLight = true;

			else if ( strcmp( kv->value.string, "true" ) == 0 )
				mapInfo->worldLight = true;
		}

		else if ( gMapInfoKeys.worldLightAng == kv->key.string )
			mapInfo->worldLightAng = KV_GetVec3( kv->value.string );

		else if ( gMapInfoKeys.worldLightColor == kv->key.string )
			mapInfo->worldLightColor = KV_GetVec4( kv->value.string );

		kv = kv->next;
	}

	return mapInfo;
}

