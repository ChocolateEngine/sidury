#include "core/resources.hh"
#include "graphics.h"
#include "render/irender.h"

#include <set>

extern IRender* render;

struct MaterialVar
{
	MaterialVar( const std::string &name, EMatVar type ):
		aName( name ), aType( type )
	{}

	MaterialVar( const std::string& name, Handle data ) :
		MaterialVar( name, EMatVar_Texture )
	{ aDataTexture = data; }

	MaterialVar( const std::string &name, float data ):
		MaterialVar( name, EMatVar_Float )
	{ aDataFloat = data; }

	MaterialVar( const std::string &name, int data ):
		MaterialVar( name, EMatVar_Int )
	{ aDataInt = data; }

	MaterialVar( const std::string &name, const glm::vec2 &data ):
		MaterialVar( name, EMatVar_Vec2 )
	{ aDataVec2 = data; }

	MaterialVar( const std::string &name, const glm::vec3 &data ):
		MaterialVar( name, EMatVar_Vec3 )
	{ aDataVec3 = data; }

	MaterialVar( const std::string &name, const glm::vec4 &data ):
		MaterialVar( name, EMatVar_Vec4 )
	{ aDataVec4 = data; }

	std::string aName;
	EMatVar     aType;
	
	// TODO: can this be smaller somehow? this takes up 16 bytes due to the vec4
	// which will only be used very, very rarely!!
	union
	{
		Handle    aDataTexture;
		float     aDataFloat;
		int       aDataInt;
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
	inline void             SetVar( const glm::vec2 &val )                { aType = EMatVar_Vec2; aDataVec2 = val; }
	inline void             SetVar( const glm::vec3 &val )                { aType = EMatVar_Vec3; aDataVec3 = val; }
	inline void             SetVar( const glm::vec4 &val )                { aType = EMatVar_Vec4; aDataVec4 = val; }

	inline Handle           GetTexture( Handle fallback = InvalidHandle ) { return ( aType == EMatVar_Texture ) ? aDataTexture : fallback; }
	inline const float&     GetFloat( float fallback = 0.f )              { return (aType == EMatVar_Float) ? aDataFloat : fallback; }
	inline const int&       GetInt( int fallback = 0 )                    { return (aType == EMatVar_Int) ? aDataInt : fallback; }
	inline const glm::vec2& GetVec2( const glm::vec2 &fallback )          { return (aType == EMatVar_Vec2) ? aDataVec2 : fallback; }
	inline const glm::vec3& GetVec3( const glm::vec3 &fallback )          { return (aType == EMatVar_Vec3) ? aDataVec3 : fallback; }
	inline const glm::vec4& GetVec4( const glm::vec4 &fallback )          { return (aType == EMatVar_Vec4) ? aDataVec4 : fallback; }
};


struct MaterialData_t
{
	std::vector< MaterialVar > aVars;
};


static ResourceList< MaterialData_t* >            gMaterials;
static std::unordered_map< std::string, Handle >  gMaterialPaths;
// static std::unordered_map< std::string, Handle >  gMaterialNames;
static std::unordered_map< const char*, Handle >  gMaterialNames;
static std::unordered_map< Handle, Handle >       gMaterialShaders;

static Handle                                     gInvalidMaterial;
static std::string                                gStrEmpty;

std::set< Handle >                                gDirtyMaterials;


const char* Mat_GetName( Handle shMat )
{
	for ( auto& [name, mat] : gMaterialNames )
	{
		if ( mat == shMat )
			return name;
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
		Log_ErrorF( gLC_ClientGraphics, "Mat_GetVarType: Index out of bounds (index: %zu - size: %zu)\n", sIndex, data->aVars.size() );
		return EMatVar_Invalid;
	}

	return data->aVars.at( sIndex ).aType;
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


template <typename T>
void Mat_SetVarInternal( Handle mat, const std::string& name, const T& value )
{
	MaterialData_t* data = nullptr;
	if ( !gMaterials.Get( mat, &data ) )
	{
		Log_Error( gLC_ClientGraphics, "Mat_SetVar: No Vars found, material must of been freed\n" );
		return;
	}

	gDirtyMaterials.emplace( mat );

	for ( MaterialVar& var : data->aVars )
	{
		if ( var.aName == name )
		{
			var.SetVar( value );
			return;
		}
	}

	data->aVars.emplace_back( name, value );
}


void Mat_SetVar( Handle mat, const std::string& name, Handle value )
{
	Mat_SetVarInternal( mat, name, value );
}

void Mat_SetVar( Handle mat, const std::string& name, float value )              { Mat_SetVarInternal( mat, name, value ); }
void Mat_SetVar( Handle mat, const std::string& name, int value )                { Mat_SetVarInternal( mat, name, value ); }
void Mat_SetVar( Handle mat, const std::string& name, const glm::vec2& value )   { Mat_SetVarInternal( mat, name, value ); }
void Mat_SetVar( Handle mat, const std::string& name, const glm::vec3& value )   { Mat_SetVarInternal( mat, name, value ); }
void Mat_SetVar( Handle mat, const std::string& name, const glm::vec4& value )   { Mat_SetVarInternal( mat, name, value ); }


MaterialVar* Mat_GetVarInternal( Handle mat, std::string_view name )
{
	MaterialData_t* data = nullptr;
	if ( !gMaterials.Get( mat, &data ) )
	{
		Log_Error( gLC_ClientGraphics, "Mat_SetVar: No Vars found, material must of been freed\n" );
		return nullptr;
	}

	for ( MaterialVar& var : data->aVars )
	{
		if ( var.aName == name )
		{
			return &var;
		}
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


Handle Graphics_LoadMaterial( const std::string& srPath )
{
	// gMaterialPaths
	return InvalidHandle;
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

	Handle handle = gMaterials.Add( new MaterialData_t{} );

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


// Is This Material an Error Material?
bool Graphics_IsErrorMaterial( Handle sMaterial )
{
	return sMaterial == gInvalidMaterial;
}


// Get a fallback error material
Handle Graphics_GetErrorMaterial( Handle shShader )
{
	return gInvalidMaterial;
}


// Get the total amount of materials created
size_t Graphics_GetMaterialCount()
{
	return gMaterials.size();
}


// Tell all materials to rebuild
void Graphics_SetAllMaterialsDirty()
{
	if ( gMaterialShaders.size() == gDirtyMaterials.size() )
		return;

	for ( const auto& [ mat, shader ] : gMaterialShaders )
		gDirtyMaterials.emplace( mat );
}

