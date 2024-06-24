#include "map_system.h"
#include "core/json5.h"


using namespace chmap;


// returns true if the match failed
static bool CheckJsonType( JsonObject_t& object, EJsonType type )
{
	if ( object.aType == type )
		return false;

	Log_ErrorF( "Expected Type \"%s\" for key \"%s\", got \"%s\" instead\n", Json_TypeToStr( type ), object.apName, Json_TypeToStr( object.aType ) );
	return true;
}


static void CopyString( char*& out, char* string, size_t stringLen )
{
	out = ch_str_copy( string, stringLen ).data;
}


static void CopyString( char*& out, const char* string, size_t stringLen )
{
	out = ch_str_copy( string, stringLen ).data;
}


template< typename VECTOR >
static VECTOR JsonToVector( JsonObject_t& object )
{
	VECTOR vec{};
	if ( object.aObjects.size() < vec.length() )
	{
		Log_ErrorF( "Not enough items in array to fill Vector with a size of %d: \"%s\"\n", object.apName, vec.length() );
		return vec;
	}

	int i = 0;
	for ( JsonObject_t& value : object.aObjects )
	{
		if ( i == vec.length() )
			break;

		if ( value.aType == EJsonType_Int )
			vec[ i++ ] = value.aInt;

		else if ( value.aType == EJsonType_Double )
			vec[ i++ ] = value.aDouble;

		else
		{
			Log_ErrorF( "Invalid Json Type for Vector: \"%s\"\n", Json_TypeToStr( value.aType ) );
			return vec;
		}
	}

	return vec;
}


static void LoadComponent( Scene& scene, Entity& entity, JsonObject_t& cur )
{
	if ( CheckJsonType( cur, EJsonType_Object ) )
	{
		Log_Error( "Entity Component is not a Json Object type\n" );
		return;
	}

	// Check for component name
	if ( cur.apName == nullptr )
	{
		Log_Error( "Entity Component does not have a name!\n" );
		return;
	}

	// Check if this component already exists
	for ( Component& component : entity.components )
	{
		if ( strcmp( component.name, cur.apName ) == 0 )
		{
			Log_ErrorF( "Entity already has a \"%s\" component\n", cur.apName );
			return;
		}
	}

	Component& comp = entity.components.emplace_back();
	CopyString( comp.name, cur.apName, strlen( cur.apName ) );

	for ( JsonObject_t& object : cur.aObjects )
	{
		// ComponentValue& value = comp.values.emplace_back();
		// CopyString( value.name, object.apName, strlen( object.apName ) );

		auto it = comp.values.find( object.apName );
		if ( it != comp.values.end() )
		{
			Log_ErrorF( "Entity already has a \"%s\" component value!\n", object.apName );
			continue;
		}

		ComponentValue& value = comp.values[ object.apName ];

		switch ( object.aType )
		{
			default:
				value.type = EComponentType_Invalid;
				break;

			case EJsonType_Double:
				value.type    = EComponentType_Double;
				value.aDouble = object.aDouble;
				break;

			case EJsonType_Int:
				value.type    = EComponentType_Int;
				value.aInteger = object.aInt;
				break;

			case EJsonType_String:
				value.type = EComponentType_String;
				CopyString( value.apString, object.apString, strlen( object.apString ) );
				break;

			case EJsonType_Array:
				if ( object.aObjects.size() == 2 )
				{
					value.type  = EComponentType_Vec2;
					value.aVec2 = JsonToVector< glm::vec2 >( object );
				}
				else if ( object.aObjects.size() == 3 )
				{
					value.type  = EComponentType_Vec3;
					value.aVec3 = JsonToVector< glm::vec3 >( object );
				}
				else if ( object.aObjects.size() == 4 )
				{
					value.type  = EComponentType_Vec4;
					value.aVec4 = JsonToVector< glm::vec4 >( object );
				}
				else
				{
					Log_ErrorF( "Invalid Entity Component Vector Size: %d", object.aObjects.size() );
				}
				break;
		}
	}
}


static void LoadEntity( Scene& scene, JsonObject_t& object )
{
	if ( CheckJsonType( object, EJsonType_Object ) )
	{
		Log_Error( "Entity is not a Json Object type\n" );
		return;
	}

	Entity& entity = scene.entites.emplace_back();

	for ( JsonObject_t& cur : object.aObjects )
	{
		if ( strcmp( cur.apName, "id" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Int ) )
				continue;

			entity.id = cur.aInt;
		}
		else if ( strcmp( cur.apName, "parent" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Int ) )
				continue;

			entity.parent = cur.aInt;
		}
		else if ( strcmp( cur.apName, "name" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_String ) )
				continue;

			CopyString( entity.name, cur.apString, strlen( cur.apString ) );
		}
		else if ( strcmp( cur.apName, "pos" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Array ) )
				continue;

			entity.pos = JsonToVector< glm::vec3 >( cur );
		}
		else if ( strcmp( cur.apName, "ang" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Array ) )
				continue;

			entity.ang = JsonToVector< glm::vec3 >( cur );
		}
		else if ( strcmp( cur.apName, "scale" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Array ) )
				continue;

			entity.scale = JsonToVector< glm::vec3 >( cur );
		}
		else if ( strcmp( cur.apName, "components" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Object ) )
				continue;

			for ( JsonObject_t& object : cur.aObjects )
			{
				LoadComponent( scene, entity, object );
			}
		}
	}
}


static void FreeScene( Scene& scene )
{
	if ( scene.name )
		free( scene.name );
}


static bool LoadScene( Map* map, const char* scenePath, s64 scenePathLen = -1 )
{
	std::vector< char > data = FileSys_ReadFile( scenePath, scenePathLen );

	if ( data.empty() )
		return false;

	JsonObject_t root;
	EJsonError   err = Json_Parse( &root, data.data() );

	if ( err != EJsonError_None )
	{
		Log_ErrorF( "Error Parsing Map Scene: %s\n", Json_ErrorToStr( err ) );
		return false;
	}

	ch_string_auto sceneName = FileSys_GetFileNameNoExt( scenePath, scenePathLen );

	Scene       scene;
	CopyString( scene.name, sceneName.data, sceneName.size );

	for ( size_t i = 0; i < root.aObjects.size(); i++ )
	{
		JsonObject_t& cur = root.aObjects[ i ];

		if ( strcmp( cur.apName, "sceneFormatVersion" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Int ) )
			{
				Json_Free( &root );
				FreeScene( scene );
				return false;
			}

			scene.sceneFormatVersion = cur.aInt;

			if ( scene.sceneFormatVersion < CH_MAP_SCENE_VERSION )
			{
				Log_ErrorF( "Scene version mismatch, expected version %d, got version %d\n", CH_MAP_SCENE_VERSION, scene.sceneFormatVersion );
				Json_Free( &root );
				FreeScene( scene );
				return false;
			}
		}
		else if ( strcmp( cur.apName, "dateCreated" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Int ) )
				continue;

			scene.dateCreated = cur.aInt;
		}
		else if ( strcmp( cur.apName, "dateModified" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Int ) )
				continue;

			scene.dateModified = cur.aInt;
		}
		else if ( strcmp( cur.apName, "changeNumber" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Int ) )
				continue;

			scene.changeNumber = cur.aInt;
		}
		else if ( strcmp( cur.apName, "entities" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Array ) )
				continue;

			for ( JsonObject_t& object : cur.aObjects )
			{
				LoadEntity( scene, object );
			}
		}
	}

	map->scenes.push_back( scene );
	return true;
}


Map* chmap::Load( const char* path, u64 pathLen )
{
	// Load mapInfo.json5 to kick things off
	const char*    strings[]      = { path, PATH_SEP_STR "mapInfo.json5" };
	const size_t   sizes[]        = { pathLen, 16 };
	ch_string_auto mapInfoPath    = ch_str_concat( 2, strings, sizes );

	ch_string_auto absMapInfoPath = FileSys_FindFile( mapInfoPath.data, mapInfoPath.size );

	if ( !absMapInfoPath.data )
	{
		Log_ErrorF( "No mapInfo.json5 file in map: \"%s\"\n", path );
		return nullptr;
	}
	
	std::vector< char > data = FileSys_ReadFile( mapInfoPath.data, mapInfoPath.size );

	if ( data.empty() )
		return nullptr;

	JsonObject_t root;
	EJsonError   err = Json_Parse( &root, data.data() );

	if ( err != EJsonError_None )
	{
		Log_ErrorF( "Error Parsing Map: %s\n", Json_ErrorToStr( err ) );
		return nullptr;
	}

	Map* map = new Map;

	std::string primaryScene = "";

	for ( size_t i = 0; i < root.aObjects.size(); i++ )
	{
		JsonObject_t& cur = root.aObjects[ i ];

		if ( strcmp( cur.apName, "version" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_Int ) )
			{
				Json_Free( &root );
				Free( map );
				return nullptr;
			}

			map->version = cur.aInt;

			if ( map->version < CH_MAP_VERSION )
			{
				Log_ErrorF( "Map version mismatch, expected version %d, got version %d\n", CH_MAP_VERSION, map->version );
				Json_Free( &root );
				Free( map );
				return nullptr;
			}
		}
		else if ( strcmp( cur.apName, "name" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_String ) )
				continue;

			if ( map->name )
			{
				Log_ErrorF( "Duplicate Name Entry in map: \"%s\"\n", path );
				free( map->name );
			}

			CopyString( map->name, cur.apString, strlen( cur.apString ) );
			// int nameLen = strlen( cur.apString );
			// map->name   = ch_malloc_count< char >( nameLen + 1 );
			// memcpy( map->name, cur.apString, nameLen );
			// map->name[ nameLen ] = '\0';
		}
		else if ( strcmp( cur.apName, "primaryScene" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_String ) )
				continue;

			primaryScene = cur.apString;
		}
		else if ( strcmp( cur.apName, "skybox" ) == 0 )
		{
			if ( CheckJsonType( cur, EJsonType_String ) )
				continue;

			CopyString( map->skybox, cur.apString, strlen( cur.apString ) );
		}
		else
		{
			Log_WarnF( "Unknown Key in map info: \"%s\"\n", cur.apName );
		}
	}

	Json_Free( &root );

	// default name of unnamed map
	if ( map->name == nullptr )
	{
		CopyString( map->name, "unnamed map", strlen( "unnamed map" ) );

		// int nameLen = strlen( "unnamed map" );
		// map->name   = ch_malloc_count< char >( nameLen + 1 );
		// memcpy( map->name, "unnamed map", nameLen );
		// map->name[ nameLen ] = '\0';
	}

	if ( map->version == 0 || map->version == UINT32_MAX )
	{
		Log_ErrorF( "Map Version not specified for map \"%s\"\n", path );
		Free( map );
		return nullptr;
	}

	// Load Scenes
	const char*              scenesStr[]   = { path, PATH_SEP_STR "scenes" };
	const size_t             scenesSizes[] = { pathLen, 7 };
	ch_string                scenesDir     = ch_str_concat( 2, scenesStr, scenesSizes );

	std::vector< ch_string > scenePaths = FileSys_ScanDir( scenesDir.data, scenesDir.size, ReadDir_NoDirs | ReadDir_Recursive );

	for ( const ch_string& scenePath : scenePaths )
	{
		if ( !ch_str_ends_with( scenePath, ".json5", 6 ) )
			continue;

		const ch_string strings[]     = { scenesDir, scenePath };
		ch_string_auto  scenePathFull = ch_str_concat( 2, strings );

		if ( !LoadScene( map, scenePathFull.data ) )	
		{
			Log_ErrorF( "Failed to load map scene: Map \"%s\" - Scene \"%s\"\n", path, scenePath.data );
		}
	}

	ch_str_free( scenePaths.data(), scenePaths.size() );

	if ( map->scenes.empty() )
	{
		Log_ErrorF( "No Scenes Loaded in map: \"%s\"\n", path );
		Free( map );
		return nullptr;
	}

	// Select Primary Scene
	u32 sceneIndex = 0;
	for ( Scene& scene : map->scenes )
	{
		if ( primaryScene == scene.name )
		{
			map->primaryScene = sceneIndex;
			break;
		}

		sceneIndex++;
	}

	if ( map->primaryScene == UINT32_MAX )
	{
		Log_ErrorF( "No Primary Scene specified for map, defaulting to scene 0: \"%s\"\n", path );
		map->primaryScene = 0;
	}

	return map;
}


void chmap::Free( Map* map )
{
	if ( map->name )
		free( map->name );

	delete map;
}

