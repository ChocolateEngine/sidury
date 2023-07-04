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

#include <filesystem>


LOG_REGISTER_CHANNEL2( Map, LogColor::DarkGreen );

SiduryMap*    gpMap      = nullptr;

extern Entity gLocalPlayer;


enum ESMF_CommandVersion : u16
{
	ESMF_CommandVersion_Invalid       = 0,
	ESMF_CommandVersion_EntityList    = 1,
	ESMF_CommandVersion_Skybox        = 1,
	ESMF_CommandVersion_ComponentList = 2,
};


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


CONCMD( map_save )
{
	if ( args.size() )
		MapManager_WriteMap( args[ 0 ] );
	else
		MapManager_WriteMap( MapManager_GetMapPath().data() );
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

	for ( Entity entity : gpMap->aMapEntities )
	{
		GetEntitySystem()->DeleteEntity( entity );
	}
	
	gpMap->aMapEntities.clear();

	delete gpMap;
	gpMap = nullptr;
}


bool MapManager_FindMap( const std::string& path )
{
	std::string absPath = FileSys_FindDir( FileSys_IsAbsolute( path.c_str() ) ? path : "maps/" + path );

	if ( absPath == "" )
		return false;

	return true;
}


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

	if ( Game_ProcessingServer() )
	{
		if ( !MapManager_LoadWorldModel() )
		{
			MapManager_CloseMap();
			return false;
		}

		Entity playerSpawnEnt = GetEntitySystem()->CreateEntity();

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
	}

	return true;
}


bool MapManager_ReadMapHeader( const std::vector< char >& srMapData )
{
	if ( srMapData.size() < sizeof( SiduryMapHeader_t ) )
	{
		Log_ErrorF( gLC_Map, "Map Data is Less than the size of the map header (%zd < %zd)\n", srMapData.size(), sizeof( SiduryMapHeader_t ) );
		return false;
	}

	SiduryMapHeader_t* header = (SiduryMapHeader_t*)srMapData.data();

	if ( header->aVersion != CH_MAP_VERSION )
	{
		Log_ErrorF( gLC_Map, "Expected Map Version %zd, got %zd", CH_MAP_VERSION, header->aVersion );
		return false;
	}

	if ( header->aSignature != CH_MAP_SIGNATURE )
	{
		Log_ErrorF( gLC_Map, "Expected Map Signature %zd, got %zd", CH_MAP_SIGNATURE, header->aSignature );
		return false;
	}

	return true;
}


ESMF_CommandVersion Map_GetCommandVersion( ESMF_Command sCommand )
{
	switch ( sCommand )
	{
		default:
		case ESMF_Command_Invalid:
			return ESMF_CommandVersion_Invalid;

		case ESMF_Command_EntityList:
			return ESMF_CommandVersion_EntityList;

		case ESMF_Command_ComponentList:
			return ESMF_CommandVersion_ComponentList;
	}
}


template< typename T >
inline const T* Map_GetCommandData( const SMF_Command* spCommand, fb::Verifier& srVerifier )
{
	auto msg = fb::GetRoot< T >( spCommand->data()->data() );

	if ( !msg->Verify( srVerifier ) )
	{
		// Log_WarnF( gLC_Map, "Command Data is not Valid: %s\n", SV_MsgToString( sMsgType ) );
		Log_WarnF( gLC_Map, "Command Data is not Valid\n" );
		return nullptr;
	}

	ESMF_CommandVersion version = Map_GetCommandVersion( spCommand->command() );
	if ( spCommand->version() != version )
	{
		Log_WarnF( gLC_Map, "Older command version (got %zd, expected %zd), skipping: \"%s\"\n",
		           spCommand->version(), version, EnumNameESMF_Command( spCommand->command() ) );

		return nullptr;
	}

	return msg;
}


void MapManager_ReadCommand( const SMF_Command* spCommand )
{
	fb::Verifier cmdVerify( spCommand->data()->data(), spCommand->data()->size() );

	switch ( spCommand->command() )
	{
		case ESMF_Command_Skybox:
		{
			if ( auto msg = Map_GetCommandData< SMF_Skybox >( spCommand, cmdVerify ) )
			{
				Skybox_SetMaterial( msg->material()->c_str() );
			}

			break;
		}
		case ESMF_Command_EntityList:
		{
			// we don't read entities or components from the map if we are not hosting
			// the server will send this data to us
			if ( !Game_IsHosting() )
				return;

			if ( auto msg = Map_GetCommandData< NetMsg_EntityUpdates >( spCommand, cmdVerify ) )
				GetEntitySystem()->ReadEntityUpdates( msg );
			break;
		}
		case ESMF_Command_ComponentList:
		{
			if ( !Game_IsHosting() )
				return;

			// IDEA: for reading multiple maps to stream in, maybe have an option to insert our own entity translation table?
			// This would be so the translation table doesn't conflict with the previous map
			if ( auto msg = Map_GetCommandData< NetMsg_ComponentUpdates >( spCommand, cmdVerify ) )
				GetEntitySystem()->ReadComponentUpdates( msg );
			break;
		}
		default:
		{
			Log_ErrorF( gLC_Map, "Invalid Map Command Type: %zd\n", spCommand->command() );
			return;
		}
	}
}


void MapManager_BuildCommand( fb::FlatBufferBuilder& srBuilder, std::vector< fb::Offset< SMF_Command > >& srCommandsBuilt, ESMF_Command sCommand )
{
	fb::FlatBufferBuilder messageBuilder;
	bool                  wroteData = false;

	switch ( sCommand )
	{
		case ESMF_Command_Skybox:
		{
			const char*       skyboxName       = Skybox_GetMaterialName();

			auto              skyboxNameOffset = messageBuilder.CreateString( skyboxName ? skyboxName : "" );
			SMF_SkyboxBuilder skyboxBuilder( messageBuilder );
			skyboxBuilder.add_material( skyboxNameOffset );
			messageBuilder.Finish( skyboxBuilder.Finish() );

			wroteData = true;
			break;
		}
		case ESMF_Command_EntityList:
		{
			GetEntitySystem()->WriteEntityUpdates( messageBuilder, true );
			wroteData = true;
			break;
		}
		case ESMF_Command_ComponentList:
		{
			GetEntitySystem()->WriteComponentUpdates( messageBuilder, true, true );
			wroteData = true;
			break;
		}
		default:
		{
			Log_ErrorF( gLC_Map, "Invalid Map Command Type: %zd\n", sCommand );
			return;
		}
	}

	flatbuffers::Offset< flatbuffers::Vector< u8 > > dataVector{};

	if ( wroteData )
		dataVector = srBuilder.CreateVector( messageBuilder.GetBufferPointer(), messageBuilder.GetSize() );

	SMF_CommandBuilder command( srBuilder );
	command.add_command( sCommand );
	command.add_version( Map_GetCommandVersion( sCommand ) );

	if ( wroteData )
		command.add_data( dataVector );

	fb::Offset< SMF_Command > offset = command.Finish();
	srBuilder.Finish( offset );
	srCommandsBuilt.push_back( offset );
}


bool MapManager_LoadMap( const std::string &path )
{
	if ( gpMap )
		MapManager_CloseMap();

	std::string absPath = FileSys_FindDir( FileSys_IsAbsolute( path.c_str() ) ? path : "maps/" + path );

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

	// ======================================================
	// Reading the new Map Format

	std::string mapDataPath = FileSys_FindFile( absPath + "/mapData.smf" );

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

	if ( !MapManager_ReadMapHeader( mapData ) )
	{
		Log_ErrorF( gLC_Map, "Failed to read map header: \"%s\"", path.c_str() );
		return false;
	}

	// Start parsing commands
	char*        serializedData = mapData.data() + sizeof( SiduryMapHeader_t );
	size_t       serializedSize = mapData.size() - sizeof( SiduryMapHeader_t );

	fb::Verifier cmdVerify( (u8*)serializedData, serializedSize );
	auto         mapDataRoot = fb::GetRoot< SMF_Data >( (u8*)serializedData );

	if ( mapDataRoot->Verify( cmdVerify ) )
	{
		for ( size_t i = 0; i < mapDataRoot->commands()->size(); i++ )
		{
			const SMF_Command* command = mapDataRoot->commands()->Get( i );

			if ( !command )
				continue;

			MapManager_ReadCommand( command );
		}
	}
	else
	{
		Log_ErrorF( gLC_Map, "Invalid Map Data: \"%s\"", path.c_str() );
		return false;
	}

	// After all entities are parsed, copy them into the SiduryMap structure
	gpMap               = new SiduryMap;
	gpMap->aMapPath     = path;
	gpMap->aMapEntities = GetEntitySystem()->aUsedEntities;

	return true;
}


SiduryMap* MapManager_CreateMap()
{
	return nullptr;
}


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
	Entity worldEntity = GetEntitySystem()->CreateEntity();

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

	Assert( transform );
	Assert( renderable );
	Assert( physShape );
	Assert( physObject );

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

		// we skip this one cause it was made with incorrect matrix rotations
		// else if ( gMapInfoKeys.ang == kv->key.string )
		// 	continue;

		// physAng contains the correct values, so we only use that now
		else if ( gMapInfoKeys.physAng == kv->key.string )
			mapInfo->ang = KV_GetVec3( kv->value.string );

		else if ( gMapInfoKeys.spawnPos == kv->key.string )
			mapInfo->spawnPos = KV_GetVec3( kv->value.string );

		else if ( gMapInfoKeys.spawnAng == kv->key.string )
			mapInfo->spawnAng = KV_GetVec3( kv->value.string );

		kv = kv->next;
	}

	return mapInfo;
}

