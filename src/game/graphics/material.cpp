#include "core/resources.hh"
#include "graphics.h"
#include "render/irender.h"

#include "core/json5.h"
#include "core/string.hpp"

#include <set>

#if __unix__
#include <limits.h>
#endif /* __unix__  */

extern IRender* render;

struct MaterialVar
{
	MaterialVar( const std::string_view name, EMatVar type ) :
		apName( name.data() ), aNameLen( name.size() ), aType( type )
	{}

	MaterialVar( const std::string_view name, Handle data ) :
		MaterialVar( name, EMatVar_Texture )
	{ aDataTexture = data; }

	MaterialVar( const std::string_view name, float data ) :
		MaterialVar( name, EMatVar_Float )
	{ aDataFloat = data; }

	MaterialVar( const std::string_view name, int data ) :
		MaterialVar( name, EMatVar_Int )
	{ aDataInt = data; }

	MaterialVar( const std::string_view name, const glm::vec2& data ) :
		MaterialVar( name, EMatVar_Vec2 )
	{ aDataVec2 = data; }

	MaterialVar( const std::string_view name, const glm::vec3& data ) :
		MaterialVar( name, EMatVar_Vec3 )
	{ aDataVec3 = data; }

	MaterialVar( const std::string_view name, const glm::vec4& data ) :
		MaterialVar( name, EMatVar_Vec4 )
	{ aDataVec4 = data; }

	~MaterialVar()
	{
		if ( apName )
			delete[] apName;
	}

	const char* apName;
	u32         aNameLen;
	EMatVar     aType;
	
	// TODO: can this be smaller somehow? this takes up 16 bytes due to the vec4
	// which will only be used very, very rarely!!
	union
	{
		Handle    aDataTexture;
		float     aDataFloat;
		int       aDataInt;
		bool      aDataBool;
		glm::vec2 aDataVec2;
		glm::vec3 aDataVec3;
		glm::vec4 aDataVec4;
	};
	
	// inline void             SetTexture( Handle val )                      { aType = EMatVar_Texture; aDataTexture = val; }
	// inline void             SetFloat( float val )                         { aType = EMatVar_Float; aDataFloat = val; }
	// inline void             SetInt( int val )                             { aType = EMatVar_Int; aDataInt = val; }
	// inline void             SetVec2( const glm::vec2 &val )               { aType = EMatVar_Vec2; aDataVec2 = val; }
	// inline void             SetVec3( const glm::vec3 &val )               { aType = EMatVar_Vec3; aDataVec3 = val; }
	// inline void             SetVec4( const glm::vec4 &val )               { aType = EMatVar_Vec4; aDataVec4 = val; }
	
	inline void             SetVar( Handle val )                          { aType = EMatVar_Texture; aDataTexture = val; }
	inline void             SetVar( float val )                           { aType = EMatVar_Float; aDataFloat = val; }
	inline void             SetVar( int val )                             { aType = EMatVar_Int; aDataInt = val; }
	inline void             SetVar( bool val )                            { aType = EMatVar_Bool; aDataBool = val; }
	inline void             SetVar( const glm::vec2 &val )                { aType = EMatVar_Vec2; aDataVec2 = val; }
	inline void             SetVar( const glm::vec3 &val )                { aType = EMatVar_Vec3; aDataVec3 = val; }
	inline void             SetVar( const glm::vec4 &val )                { aType = EMatVar_Vec4; aDataVec4 = val; }

	inline Handle           GetTexture( Handle fallback = InvalidHandle ) { return ( aType == EMatVar_Texture ) ? aDataTexture : fallback; }
	inline const float&     GetFloat( float fallback = 0.f )              { return ( aType == EMatVar_Float ) ? aDataFloat : fallback; }
	inline const int&       GetInt( int fallback = 0 )                    { return ( aType == EMatVar_Int ) ? aDataInt : fallback; }
	inline bool             GetBool( bool fallback = false )              { return ( aType == EMatVar_Bool ) ? aDataBool : fallback; }
	inline const glm::vec2& GetVec2( const glm::vec2 &fallback )          { return ( aType == EMatVar_Vec2 ) ? aDataVec2 : fallback; }
	inline const glm::vec3& GetVec3( const glm::vec3 &fallback )          { return ( aType == EMatVar_Vec3 ) ? aDataVec3 : fallback; }
	inline const glm::vec4& GetVec4( const glm::vec4 &fallback )          { return ( aType == EMatVar_Vec4 ) ? aDataVec4 : fallback; }
};


struct MaterialData_t
{
	// std::vector< MaterialVar > aVars;
	ChVector< MaterialVar > aVars;
};


static ResourceList< MaterialData_t* >                gMaterials;
static std::unordered_map< std::string, Handle >      gMaterialPaths;
static std::unordered_map< std::string_view, Handle > gMaterialNames;  // TODO: use string hashing for this?
static std::unordered_map< Handle, Handle >           gMaterialShaders;

static Handle                                         gInvalidMaterial;
static std::string                                    gStrEmpty;

std::set< Handle >                                    gDirtyMaterials;


const char* Mat_GetName( Handle shMat )
{
	for ( auto& [name, mat] : gMaterialNames )
	{
		if ( mat == shMat )
			return name.data();
	}

	return nullptr;
}


size_t Mat_GetVarCount( Handle mat )
{
	MaterialData_t* data = nullptr;
	if ( !gMaterials.Get( mat, &data ) )
	{
		Log_Error( gLC_ClientGraphics, "Mat_SetVar: No Vars found, material must of been freed\n" );
		return 0;
	}

	return data->aVars.size();
}


EMatVar Mat_GetVarType( Handle mat, size_t sIndex )
{
	MaterialData_t* data = nullptr;
	if ( !gMaterials.Get( mat, &data ) )
	{
		Log_Error( gLC_ClientGraphics, "Mat_GetVarType: No Vars found, material must of been freed\n" );
		return EMatVar_Invalid;
	}

	if ( sIndex >= data->aVars.size() )
	{
		Log_ErrorF( gLC_ClientGraphics, "Mat_GetVarType: Index out of bounds (index: %zu - size: %zd)\n", sIndex, data->aVars.size() );
		return EMatVar_Invalid;
	}

	return data->aVars[ sIndex ].aType;
}


Handle Mat_GetShader( Handle mat )
{
	auto it = gMaterialShaders.find( mat );
	if ( it != gMaterialShaders.end() )
		return it->second;

	Log_Warn( gLC_ClientGraphics, "Mat_GetShader: No Material found in gMaterialShaders!\n" );
	return InvalidHandle;
}


void Mat_SetShader( Handle mat, Handle shShader )
{
	auto it = gMaterialShaders.find( mat );
	if ( it != gMaterialShaders.end() )
	{
		it->second = shShader;
		gDirtyMaterials.emplace( mat );
	}
	else
	{
		Log_Warn( gLC_ClientGraphics, "Mat_SetShader: No Material found in gMaterialShaders!\n" );
	}
}


VertexFormat Mat_GetVertexFormat( Handle mat )
{
	Handle shader = Mat_GetShader( mat );

	if ( shader == InvalidHandle )
		return VertexFormat_None;

	return Shader_GetVertexFormat( shader );
}


template <typename T>
void Mat_SetVarInternal( Handle mat, const std::string& name, const T& value )
{
	PROF_SCOPE();

	MaterialData_t* data = nullptr;
	if ( !gMaterials.Get( mat, &data ) )
	{
		Log_Error( gLC_ClientGraphics, "Mat_SetVar: No Vars found, material must of been freed\n" );
		return;
	}

	gDirtyMaterials.emplace( mat );

	for ( MaterialVar& var : data->aVars )
	{
		if ( name.size() != var.aNameLen )
			continue;

		if ( var.apName == name )
		{
			var.SetVar( value );
			return;
		}
	}

	// data->aVars.emplace_back( name, value );
	MaterialVar& var = data->aVars.emplace_back( true );

	var.aNameLen     = name.size();
	char* varName    = new char[ name.size() + 1 ];
	strncpy( varName, name.data(), name.size() );
	varName[ name.size() ] = '\0';
	var.apName             = varName;

	var.SetVar( value );
}


void Mat_SetVar( Handle mat, const std::string& name, Handle value )
{
	Mat_SetVarInternal( mat, name, value );
}

void Mat_SetVar( Handle mat, const std::string& name, float value )              { Mat_SetVarInternal( mat, name, value ); }
void Mat_SetVar( Handle mat, const std::string& name, int value )                { Mat_SetVarInternal( mat, name, value ); }
void Mat_SetVar( Handle mat, const std::string& name, bool value )               { Mat_SetVarInternal( mat, name, value ); }
void Mat_SetVar( Handle mat, const std::string& name, const glm::vec2& value )   { Mat_SetVarInternal( mat, name, value ); }
void Mat_SetVar( Handle mat, const std::string& name, const glm::vec3& value )   { Mat_SetVarInternal( mat, name, value ); }
void Mat_SetVar( Handle mat, const std::string& name, const glm::vec4& value )   { Mat_SetVarInternal( mat, name, value ); }


MaterialVar* Mat_GetVarInternal( Handle mat, std::string_view name )
{
	PROF_SCOPE();

	MaterialData_t* data = nullptr;
	if ( !gMaterials.Get( mat, &data ) )
	{
		Log_Error( gLC_ClientGraphics, "Mat_SetVar: No Vars found, material must of been freed\n" );
		return nullptr;
	}

	for ( MaterialVar& var : data->aVars )
	{
		if ( name.size() != var.aNameLen )
			continue;

		if ( var.apName == name )
			return &var;
	}

	return nullptr;
}


int Mat_GetTextureIndex( Handle mat, std::string_view name, Handle fallback )
{
	MaterialVar* var = Mat_GetVarInternal( mat, name );
	return render->GetTextureIndex( var ? var->GetTexture( fallback ) : fallback );
}


Handle Mat_GetTexture( Handle mat, std::string_view name, Handle fallback )
{
	MaterialVar* var = Mat_GetVarInternal( mat, name );
	return var ? var->GetTexture( fallback ) : fallback;
}


float Mat_GetFloat( Handle mat, std::string_view name, float fallback )
{
	MaterialVar* var = Mat_GetVarInternal( mat, name );
	return var ? var->GetFloat( fallback ) : fallback;
}


int Mat_GetInt( Handle mat, std::string_view name, int fallback )
{
	MaterialVar* var = Mat_GetVarInternal( mat, name );
	return var ? var->GetInt( fallback ) : fallback;
}


bool Mat_GetBool( Handle mat, std::string_view name, bool fallback )
{
	MaterialVar* var = Mat_GetVarInternal( mat, name );
	return var ? var->GetBool( fallback ) : fallback;
}


const glm::vec2& Mat_GetVec2( Handle mat, std::string_view name, const glm::vec2& fallback )
{
	MaterialVar* var = Mat_GetVarInternal( mat, name );
	return var ? var->GetVec2( fallback ) : fallback;
}


const glm::vec3& Mat_GetVec3( Handle mat, std::string_view name, const glm::vec3& fallback )
{
	MaterialVar* var = Mat_GetVarInternal( mat, name );
	return var ? var->GetVec3( fallback ) : fallback;
}


const glm::vec4& Mat_GetVec4( Handle mat, std::string_view name, const glm::vec4& fallback )
{
	MaterialVar* var = Mat_GetVarInternal( mat, name );
	return var ? var->GetVec4( fallback ) : fallback;
}


// ---------------------------------------------------------------------------------------------------------


// Used in normal material loading, and eventually, live material reloading
bool Graphics_ParseMaterial( const std::string& srPath, Handle& handle )
{
	PROF_SCOPE();

	std::string fullPath;

	if ( srPath.ends_with( ".cmt" ) )
		fullPath = FileSys_FindFile( srPath );
	else
		fullPath = FileSys_FindFile( srPath + ".cmt" );

	if ( fullPath.empty() )
	{
		Log_ErrorF( gLC_ClientGraphics, "Failed to Find Material: \"%s\"", srPath.c_str() );
		return false;
	}

	std::vector< char > data = FileSys_ReadFile( fullPath );

	if ( data.empty() )
		return false;

	JsonObject_t root;
	EJsonError   err = Json_Parse( &root, data.data() );

	if ( err != EJsonError_None )
	{
		Log_ErrorF( gLC_ClientGraphics, "Error Parsing Material: %d\n", err );
		return false;
	}

	Handle        shader = InvalidHandle;

	for ( size_t i = 0; i < root.aObjects.size(); i++ )
	{
		JsonObject_t& cur = root.aObjects[ i ];

		if ( strcmp( cur.apName, "shader" ) == 0 )
		{
			if ( shader != InvalidHandle )
			{
				Log_WarnF( gLC_ClientGraphics, "Shader is specified multiple times in material \"%s\"", srPath.c_str() );
				continue;
			}

			if ( cur.aType != EJsonType_String )
			{
				Log_WarnF( gLC_ClientGraphics, "Shader value is not a string!: \"%s\"", srPath.c_str() );
				Json_Free( &root );
				return false;
			}

			shader = Graphics_GetShader( cur.apString );
			if ( shader == InvalidHandle )
			{
				Log_ErrorF( gLC_ClientGraphics, "Failed to find Material Shader: %s - \"%s\"\n", cur.apString, srPath.c_str() );
				Json_Free( &root );
				return false;
			}

			MaterialData_t* matData = nullptr;
			if ( handle == InvalidHandle )
			{
				matData    = new MaterialData_t;
				handle     = gMaterials.Add( matData );

				char* name = new char[ srPath.size() + 1 ];
				strncpy( name, srPath.c_str(), srPath.size() );
				name[ srPath.size() ]  = '\0';

				gMaterialNames[ name ] = handle;
			}
			else
			{
				if ( !gMaterials.Get( handle, &matData ) )
				{
					Log_WarnF( gLC_ClientGraphics, "Failed to find Material Data while updating: \"%s\"\n", srPath.c_str() );
					Json_Free( &root );
					return false;
				}
			}
			gMaterialShaders[ handle ] = shader;

			continue;
		}

		switch ( cur.aType )
		{
			default:
			{
				Log_WarnF( gLC_ClientGraphics, "Unused Value Type: %d - \"%s\"\n", cur.aType, srPath.c_str() );
				break;
			}

			// Texture Path
			case EJsonType_String:
			{
				TextureCreateData_t createData{};
				createData.aUsage  = EImageUsage_Sampled;
				createData.aFilter = EImageFilter_Linear;

				Handle texture     = InvalidHandle;
				Mat_SetVar( handle, cur.apName, render->LoadTexture( texture, cur.apString, createData ) );
				break;
			}

			case EJsonType_Int:
			{
				// integer is here is an int64_t
				if ( cur.aInt > INT_MAX )
				{
					Log_WarnF( gLC_ClientGraphics, "Overflowed Int Value for key \"%s\", clamping to INT_MAX - \"%s\"\n", cur.apName, srPath.c_str() );
					Mat_SetVar( handle, cur.apName, INT_MAX );
					break;
				}
				else if ( cur.aInt < INT_MIN )
				{
					Log_WarnF( gLC_ClientGraphics, "Underflowed Int Value for key \"%s\", clamping to INT_MIN - \"%s\"\n", cur.apName, srPath.c_str() );
					Mat_SetVar( handle, cur.apName, INT_MIN );
					break;
				}

				Mat_SetVar( handle, cur.apName, static_cast< int >( cur.aInt ) );
				break;
			}

			// double
			case EJsonType_Double:
			{
				Mat_SetVar( handle, cur.apName, static_cast< float >( cur.aDouble ) );
				break;
			}

			case EJsonType_True:
			{
				Mat_SetVar( handle, cur.apName, true );
				break;
			}

			case EJsonType_False:
			{
				Mat_SetVar( handle, cur.apName, false );
				break;
			}

			case EJsonType_Array:
			{
				Log_Msg( "TODO: IMPLEMENT ARRAY PARSING\n" );
				break;
			}
		}
	}

	Json_Free( &root );

	return true;
}


Handle Graphics_LoadMaterial( const std::string& srPath )
{
	auto nameIt = gMaterialNames.find( srPath.c_str() );
	if ( nameIt != gMaterialNames.end() )
	{
		Log_WarnF( gLC_ClientGraphics, "Material Already Loaded: \"%s\"\n", srPath.c_str() );
		return nameIt->second;
	}

	Handle handle = InvalidHandle;
	if ( !Graphics_ParseMaterial( srPath, handle ) )
		return InvalidHandle;

	gMaterialPaths[ srPath ] = handle;
	return handle;
}


// Create a new material with a name and a shader
Handle Graphics_CreateMaterial( const std::string& srName, Handle shShader )
{
	auto nameIt = gMaterialNames.find( srName.c_str() );
	if ( nameIt != gMaterialNames.end() )
	{
		Log_ErrorF( gLC_ClientGraphics, "Graphics_CreateMaterial(): Material with this name already exists: \"%s\"\n", srName.c_str() );
		return nameIt->second;
	}

	Handle handle = gMaterials.Add( new MaterialData_t );

	char*  name   = new char[ srName.size() + 1 ];
	strncpy( name, srName.c_str(), srName.size() );
	name[ srName.size() ]      = '\0';

	gMaterialNames[ name ]     = handle;
	gMaterialShaders[ handle ] = shShader;

	return handle;
}


// Free a material
void Graphics_FreeMaterial( Handle shMaterial )
{
	MaterialData_t* data = nullptr;
	if ( !gMaterials.Get( shMaterial, &data ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_FreeMaterial: No Data Found\n" );
		return;
	}
	else
	{
		delete data;
		gMaterials.Remove( shMaterial );

		for ( auto& [ name, mat ] : gMaterialNames )
		{
			if ( mat == shMaterial )
			{
				gMaterialNames.erase( name ); // name is freed here
				break;
			}
		}

		// make sure it's not in the dirty materials list
		if ( gDirtyMaterials.contains( shMaterial ) )
			gDirtyMaterials.erase( shMaterial );
	}
}


// Find a material by name
// Name is a full path to the cmt file
// EXAMPLE: C:/chocolate/sidury/materials/dev/grid01.cmt
// NAME: dev/grid01
Handle Graphics_FindMaterial( const char* spName )
{
	auto nameIt = gMaterialNames.find( spName );
	if ( nameIt != gMaterialNames.end() )
		return nameIt->second;

	return InvalidHandle;
}


// Get the total amount of materials created
size_t Graphics_GetMaterialCount()
{
	return gMaterials.size();
}


// Get the path to the material
const std::string& Graphics_GetMaterialPath( Handle sMaterial )
{
	if ( sMaterial == CH_INVALID_HANDLE )
		return "";

	for ( auto& [ path, handle ] : gMaterialPaths )
	{
		if ( sMaterial == handle )
			return path;
	}

	return "";
}


// Tell all materials to rebuild
void Graphics_SetAllMaterialsDirty()
{
	if ( gMaterialShaders.size() == gDirtyMaterials.size() )
		return;

	for ( const auto& [ mat, shader ] : gMaterialShaders )
		gDirtyMaterials.emplace( mat );
}


CONCMD( r_mark_all_materials_dirty )
{
	Graphics_SetAllMaterialsDirty();
}

