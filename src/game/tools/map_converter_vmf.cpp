#include "map_converter.h"
#include "../entity.h"
#include "../graphics/mesh_builder.h"

#include "speedykeyv/KeyValue.h"

#include <glm/gtx/hash.hpp>
#include <unordered_set>

// A lot of credit goes to VMF2OBJ
// https://github.com/Dylancyclone/VMF2OBJ


// probably get rid of this, it won't work for all shaders
struct ValveMaterial_t
{
	std::string aChocMatPath;

	std::string aBaseTexture;
	std::string aBaseTextureAbs;

	// Needed for uaxis/vaxis
	glm::uvec2  aBaseTextureSize;

	bool        aAlphaTest;
	bool        aTranslucent;
};


struct VMFTriangle_t
{
	// std::vector< glm::vec3 > aPos;
	glm::vec3 aPos[ 3 ];
};


// Side of A Brush
struct VMFSolidSide_t
{
	glm::vec3     aPoints[ 3 ];
	glm::vec4     aPlane;
	VMFTriangle_t aTriangle;

	glm::vec3     aUAxis;
	float         aUAxisTranslation;
	float         aUAxisScale;

	glm::vec3     aVAxis;
	float         aVAxisTranslation;
	float         aVAxisScale;

	Handle        aMaterial;

	bool          aSkip = false;
};


// A Brush
struct VMFSolid_t
{
	// int aID;
	std::vector< VMFSolidSide_t > aSides;
};


static std::string gSkipTools[] = {
	"tools/toolsskybox",
	"tools/toolsskip",
	"tools/toolsnodraw",  // This one is a bit odd for real time shadows, as it can help prevent shadow bleeding easier
};


// TODO:
// - Decompile VTF files and convert them to KTX files (does KTX and VTFLib have an easy way to do this losslessly?)
// - Convert vmt's to cmt files
// - Store a list of texture resolutions to use for UV's for VMF parsing
// - Then, parse VMF's
// 


// information read from the vmf that we can use
struct ParsedVMF_t
{
	std::string                                   aSkyName;
	int                                           aVersion;

	std::vector< VMFSolid_t >                     aSolids;

	std::unordered_map< std::string, Handle >     aMaterials;
	std::unordered_map< Handle, std::string >     aMaterialHandleToStr;
	std::unordered_map< Handle, size_t >          aMaterialToSurface;
	std::unordered_map< Handle, ValveMaterial_t > aValveMaterials;

	MeshBuilder                                   aWorldMeshBuilder;

	// Creates it if it doesn't exist
	Handle GetMaterial( const std::string& srName )
	{
		auto it = aMaterials.find( srName );
		if ( it != aMaterials.end() )
		{
			return it->second;
		}

		Handle shader                         = Graphics_GetShader( "basic_3d" );
		Handle worldMaterial                  = Graphics_CreateMaterial( srName, shader );

		aMaterials[ srName ]                  = worldMaterial;
		aMaterialHandleToStr[ worldMaterial ] = srName;
		aMaterialToSurface[ worldMaterial ]   = aMaterialToSurface.size();

		aWorldMeshBuilder.SetSurfaceCount( aMaterialToSurface.size() );

		return worldMaterial;
	}


	size_t GetSurfaceIndex( Handle sMaterial )
	{
		auto it = aMaterialToSurface.find( sMaterial );
		if ( it != aMaterialToSurface.end() )
		{
			return it->second;
		}

		return 0;
	}

	void CreateVert( const glm::vec3& pos )
	{
		aWorldMeshBuilder.SetPos( pos );
		aWorldMeshBuilder.NextVertex();
	}

	void CreateTri( const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec3& pos2 )
	{
		CreateVert( pos0 );
		CreateVert( pos1 );
		CreateVert( pos2 );
	}
};


static void CreateObj()
{
}


#if 0
// temp until speedykeyv can parse lists
static std::vector< std::string > KV_GetVec( const char* value, int len, int& pos, char start, char end )
{
	if ( len == 1 || value[ pos ] != start )
	{
		Log_ErrorF( "not a list: %s\n", value );
		return {};
	}

	std::vector< std::string > strVec;

	std::string                curVal;
	bool                       inQuote = false;

	pos++;
	for ( ; pos < len; pos++ )
	{
		char ch = value[ pos ];

		if ( !inQuote )
		{
			if ( ch == end )
			{
				if ( curVal.size() )
					strVec.push_back( curVal );

				break;
			}

			else if ( ch == ' ' || ch == ',' )
			{
				if ( curVal.size() )
				{
					strVec.push_back( curVal );
					curVal = "";
				}

				continue;
			}
		}

		curVal += ch;
	}

	return strVec;
}


static std::vector< int > KV_GetVecInt( const char* value, int len, int& pos, bool useHardBracket )
{
	char start = '(';
	char end = ')';

	if ( useHardBracket )
	{
		start = '[';
		end   = ']';
	}

	std::vector< std::string > strVec = KV_GetVec( value, len, pos, start, end );
	if ( strVec.empty() )
		return {};

	std::vector< int > vec;

	for ( auto strVal : strVec )
	{
		long val = 0;
		if ( !ToLong2( strVal, val ) )
		{
			Log_WarnF( "Failed to convert to long: %s - From %s\n", value );
			return {};
		}

		vec.push_back( val );
	}

	return vec;
}


static std::vector< double > KV_GetVecDouble( const char* value, int len, int& pos, bool useHardBracket )
{
	char start = '(';
	char end = ')';

	if ( useHardBracket )
	{
		start = '[';
		end   = ']';
	}

	std::vector< std::string > strVec = KV_GetVec( value, len, pos, start, end );
	if ( strVec.empty() )
		return {};

	std::vector< double > vec;

	for ( auto strVal : strVec )
	{
		double val = 0;
		if ( !ToDouble2( strVal, val ) )
		{
			Log_WarnF( "Failed to convert to double: %s - From %s\n", value );
			return {};
		}

		vec.push_back( val );
	}

	return vec;
}
#endif


// glm::normalize doesn't return a float
// move to util.cpp?
// copied from player.cpp
static float vec3_norm2( glm::vec3& v )
{
	// glm::length();

	float length = sqrt( glm::dot( v, v ) );

	if ( length )
		v *= 1 / length;

	return length;
}


static glm::vec3 PlaneNormalize( const glm::vec3 sPlane[ 3 ] )
{
	glm::vec3 ab = sPlane[ 1 ] - sPlane[ 0 ];
	glm::vec3 ac = sPlane[ 2 ] - sPlane[ 0 ];

	return glm::cross( ab, ac );
}


static float PlaneDistance( const glm::vec3 sPlane[ 3 ] )
{
	glm::vec3 normal = PlaneNormalize( sPlane );

	return ( ( sPlane[ 0 ].x * normal.x ) + ( sPlane[ 0 ].y * normal.y ) + ( sPlane[ 0 ].z * normal.z ) ) / sqrtf( powf( normal.x, 2 ) + powf( normal.y, 2 ) + powf( normal.z, 2 ) );
}


static glm::vec3 PlaneCenter( const glm::vec3 sPlane[ 3 ] )
{
	return ( sPlane[ 0 ] + sPlane[ 1 ] + sPlane[ 2 ] ) / 3.f;
}


// wtf is this doing
static bool PointInHull( const glm::vec3& srPoint, const std::vector< VMFSolidSide_t >& srSides )
{
	for ( const VMFSolidSide_t& side : srSides )
	{
		// glm::vec3 facing = glm::normalize( srPoint - PlaneCenter( side.aPoints ) );
		glm::vec3 facing = srPoint - PlaneCenter( side.aPoints );

		if ( glm::dot( facing, PlaneNormalize( side.aPoints ) ) < -0.01 )
		{
			return false;
		}
	}

	return true;
}


static bool GetPlaneIntersectionPoint( glm::vec3& srOut, glm::vec3 sPlane0[ 3 ], glm::vec3 sPlane1[ 3 ], glm::vec3 sPlane2[ 3 ] )
{
	glm::vec3 plane0Normal = PlaneNormalize( sPlane0 );
	glm::vec3 plane1Normal = PlaneNormalize( sPlane1 );
	glm::vec3 plane2Normal = PlaneNormalize( sPlane2 );

	float     determinant =
	  ( (
		  plane0Normal.x * plane1Normal.y * plane2Normal.z +
		  plane0Normal.y * plane1Normal.z * plane2Normal.x +
		  plane0Normal.z * plane1Normal.x * plane2Normal.y ) -
	    ( plane0Normal.z * plane1Normal.y * plane2Normal.x +
	      plane0Normal.y * plane1Normal.x * plane2Normal.z +
	      plane0Normal.x * plane1Normal.z * plane2Normal.y ) );

	// Can't intersect parallel planes.
	if ( ( determinant <= 0.01 && determinant >= -0.01 ) || glm::isnan( determinant ) )
	{
		return false;
	}

	glm::vec3 cross0 = glm::cross( plane1Normal, plane2Normal ) * PlaneDistance( sPlane0 );
	glm::vec3 cross1 = glm::cross( plane2Normal, plane0Normal ) * PlaneDistance( sPlane1 );
	glm::vec3 cross2 = glm::cross( plane0Normal, plane1Normal ) * PlaneDistance( sPlane2 );

	srOut            = ( cross0 + cross1 + cross2 ) / determinant;
	return true;
}


// inline bool operator<( const glm::vec3& srLeft, const glm::vec3& srRight )
// {
// 	return srLeft < srRight;
// }


bool CompareVec3( const glm::vec3& srLeft, const glm::vec3& srRight )
{
	return srLeft == srRight;
}


static bool CompleteSide( VMFSolidSide_t& srSide, VMFSolid_t& srSolid, VMFSolidSide_t& srNewSide )
{
	//std::set< glm::vec3, bool ( * )( const glm::vec3&, const glm::vec3& ) > intersections( &CompareVec3 );
	std::unordered_set< glm::vec3 > intersections;
	
	// Get all intersections on this Solid
	for ( VMFSolidSide_t& side2 : srSolid.aSides )
	{
		for ( VMFSolidSide_t& side3 : srSolid.aSides )
		{
			glm::vec3 intersection;
			if ( !GetPlaneIntersectionPoint( intersection, srSide.aPoints, side2.aPoints, side3.aPoints ) )
				continue;
	
			// Make sure we haven't already processed this
			auto intersectIt = intersections.find( intersection );
			if ( intersectIt != intersections.end() )
				continue;
	
			if ( !PointInHull( intersection, srSolid.aSides ) )
				continue;
	
			// Check to see if we are close to an existing intersection
			bool exists = false;
			for ( const glm::vec3& point : intersections )
			{
				if ( glm::distance( point, intersection ) < 0.2 )
				{
					exists = true;
					break;
				}
			}
	
			if ( exists )
				continue;
	
			// Check if the intersection is close to an existing point on another side
			for ( VMFSolidSide_t& side : srSolid.aSides )
			{
			}

			intersections.emplace( intersection );
		}
	}
	
	if ( intersections.size() < 3 )
	{
		Log_Error( "Side with less than 3 intersections??\n" );
		return false;
	}

	glm::vec3 sum{};
	for ( const glm::vec3& point : intersections )
		sum += point;

	glm::vec3 center       = sum / static_cast< float >( intersections.size() );
	glm::vec3 normal       = PlaneNormalize( srSide.aPoints );

	glm::vec3 forwardCross = glm::cross( normal, vec_forward );
	glm::vec3 rightCross   = glm::cross( normal, vec_right );
	glm::vec3 upCross      = glm::cross( normal, vec_up );
	glm::vec3 maxCross     = glm::max( glm::max( forwardCross, rightCross ), upCross );
	glm::vec3 qp           = glm::cross( normal, maxCross );

	// wtf is the point of this????????????
	auto      CompareVectors = [ & ]( const glm::vec3& srLeft, const glm::vec3& srRight )
	{
		glm::vec3 orderLeftVec = glm::normalize( srLeft - center );
		float     orderLeft    = atan2f(
				 glm::dot( normal, glm::cross( orderLeftVec, maxCross ) ),
				 glm::dot( normal, glm::cross( orderLeftVec, qp ) ) );

		glm::vec3 orderRightVec = glm::normalize( srRight - center );
		float     orderRight    = atan2f(
				 glm::dot( normal, glm::cross( orderRightVec, maxCross ) ),
				 glm::dot( normal, glm::cross( orderRightVec, qp ) ) );

		return orderLeft < orderRight;
    };

	std::vector< glm::vec3 > sorted( intersections.begin(), intersections.end() );
	std::sort( sorted.begin(), sorted.end(), CompareVectors );

	VMFSolidSide_t newSide = srSide;
	// newSide.aPoints;

	return true;
}


// First 3 values is there normal vector, w is the distance
static glm::vec4 Util_PlaneFromPoints( const glm::vec3& srPoint0, const glm::vec3& srPoint1, const glm::vec3& srPoint2 )
{
	glm::vec3 normal = glm::normalize( glm::cross( srPoint0 - srPoint1, srPoint2 - srPoint1 ) );
	float     dist   = glm::dot( srPoint0, normal );

	// NOTE: vbsp has some SnapPlane thing and FindFloatPlane here as well
	// unsure if i actually need it here

	return { normal, dist };
}


static VMFTriangle_t PolygonFromPlane( const glm::vec4& srPlane )
{
#if 0
	// Find the major axis
	float max  = -1.f;
	int   axis = -1;
	
	// what is this doing??
	for ( int i = 0; i < 3; i++ )
	{
		float v = fabsf( srPlane[ i ] );
		if ( v > max )
		{
			axis = i;
			max  = v;
		}
	}
	
	if ( axis == -1 )
	{
		Log_Error( "No Axis found when creating a polygon from a plane\n" );
		return {};
	}
	
	glm::vec3 up{};
	
	// what is this???
	if ( axis == 0 || axis == 1 )
		up.z = 1;
	else
		up.x = 1;
	
	glm::vec3 normal( srPlane );
	up += normal * -glm::dot( up, normal );
	up = glm::normalize( up );
	
	glm::vec3 original = normal * srPlane.w;
	glm::vec3 right    = glm::cross( up, normal );
	
	// right and up are now scaled up?
	
	VMFTriangle_t polygon{};
	polygon.aPos.resize( 4 );
	
	// project a really big	axis aligned box onto the plane
	polygon.aPos[ 0 ] = ( original - right ) + up;
	polygon.aPos[ 1 ] = ( original + right ) + up;
	polygon.aPos[ 2 ] = ( original + right ) - up;
	polygon.aPos[ 3 ] = ( original - right ) - up;
	
	return polygon;
#endif
	return {};
}


static bool ParseSolid( ParsedVMF_t& srMap, KeyValue* spRoot )
{
	VMFSolid_t solid;

	KeyValue* kv = spRoot->children;
	for ( int ch = 0; ch < spRoot->childCount; ch++ )
	{
		Log_MsgF( "%s\n", kv->key.string );

		if ( strcmp( kv->key.string, "side" ) == 0 )
		{
			VMFSolidSide_t side;

			KeyValue*      sideKV = kv->children;
			for ( int j = 0; j < kv->childCount; j++ )
			{
				if ( side.aSkip )
					break;

				Log_MsgF( "%s\n", sideKV->key.string );

				if ( strcmp( sideKV->key.string, "plane" ) == 0 )
				{
					int read = sscanf( sideKV->value.string, "(%f %f %f) (%f %f %f) (%f %f %f)",
					                   &side.aPoints[ 0 ][ 0 ], &side.aPoints[ 0 ][ 1 ], &side.aPoints[ 0 ][ 2 ],
					                   &side.aPoints[ 1 ][ 0 ], &side.aPoints[ 1 ][ 1 ], &side.aPoints[ 1 ][ 2 ],
					                   &side.aPoints[ 2 ][ 0 ], &side.aPoints[ 2 ][ 1 ], &side.aPoints[ 2 ][ 2 ] );

					if ( read != 9 )
					{
						Log_Error( "Error while parsing plane value\n" );
						return false;
					}
				}

				else if ( strcmp( sideKV->key.string, "material" ) == 0 )
				{
					for ( int skipTexI = 0; skipTexI < ARR_SIZE( gSkipTools ); skipTexI++ )
					{
						if ( gSkipTools[ skipTexI ].size() == sideKV->value.length && gSkipTools[ skipTexI ] == sideKV->value.string )
						{
							side.aSkip = true;
							break;
						}
					}

					if ( !side.aSkip )
						side.aMaterial = srMap.GetMaterial( sideKV->value.string );
				}

				else if ( strcmp( sideKV->key.string, "uaxis" ) == 0 )
				{
					int read = sscanf( sideKV->value.string, "[%f %f %f %f] %f",
					                   &side.aUAxis.x, &side.aUAxis.y, &side.aUAxis.z,
					                   &side.aUAxisTranslation, &side.aUAxisScale );

					if ( read != 5 )
					{
						Log_Error( "Error while parsing uaxis value\n" );
						return false;
					}
				}

				else if ( strcmp( sideKV->key.string, "vaxis" ) == 0 )
				{
					int read = sscanf( sideKV->value.string, "[%f %f %f %f] %f",
					                   &side.aVAxis.x, &side.aVAxis.y, &side.aVAxis.z,
					                   &side.aVAxisTranslation, &side.aVAxisScale );

					if ( read != 5 )
					{
						Log_Error( "Error while parsing vaxis value\n" );
						return false;
					}
				}

				sideKV = sideKV->next;
			}

			side.aPlane = Util_PlaneFromPoints( side.aPoints[ 0 ], side.aPoints[ 1 ], side.aPoints[ 2 ] );
			solid.aSides.push_back( side );
		}

		kv = kv->next;
	}
	
	// Make a VMFTriangle_t out of this solid
	//for ( VMFSolidSide_t& side : solid.aSides )
	//{
	//	VMFSolidSide_t newSide;
	//	if ( CompleteSide( side, solid, newSide ) )
	//	{
	//		side = newSide;
	//	}
	//
	//	// VMFTriangle_t polygon = PolygonFromPlane( side.aPlane );
	//	// side.aTriangle        = polygon;
	//}


	// we need to add more sides in or something for a missing triangle?




	srMap.aSolids.push_back( solid );

	return true;
}


// This function parses the world key in the vmf and exports all brush data to an obj file
static bool ConvertWorld( ParsedVMF_t& srMap, KeyValue* spRoot )
{
	KeyValue* kv = spRoot->children;
	for ( int i = 0; i < spRoot->childCount; i++ )
	{
		Log_MsgF( "%s\n", kv->key.string );

		if ( strcmp( kv->key.string, "skyname" ) == 0 )
		{
			srMap.aSkyName = kv->value.string;
		}
		else if ( strcmp( kv->key.string, "mapversion" ) == 0 )
		{
			long out;
			if ( !ToLong3( kv->value.string, out ) )
			{
				Log_ErrorF( "Failed to convert mapversion to int: %s\n", kv->value.string );
				return false;
			}

			srMap.aVersion = out;
		}
		else if ( strcmp( kv->key.string, "solid" ) == 0 )
		{
			if ( !ParseSolid( srMap, kv ) )
			{
				Log_Error( "Failed to parse solid\n" );
				return false;
			}
		}

		kv = kv->next;
	}

	return true;
}


static bool ConvertEntity( ParsedVMF_t& srMap, KeyValue* spRoot )
{
	return true;

	// KeyValue* kv = spRoot->children;
	// for ( int i = 0; i < spRoot->childCount; i++ )
	// {
	// 	Log_MsgF( "%s\n", kv->key.string );
	// 	kv = kv->next;
	// }
}


static void ConvertVMT( ParsedVMF_t& srMap, Handle sMaterial, const std::string& srVMTPath, const std::string& srPath, const std::string& srAssetPath, const std::string& srAssetsOut )
{
	std::vector< char > rawData = FileSys_ReadFile( srPath );

	if ( rawData.empty() )
	{
		Log_WarnF( "Failed to read file: %s\n", srPath.c_str() );
		return;
	}

	// append a null terminator for c strings
	rawData.push_back( '\0' );

	KeyValueRoot      kvRoot;
	KeyValueErrorCode err = kvRoot.Parse( rawData.data() );

	if ( err != KeyValueErrorCode::NO_ERROR )
	{
		Log_WarnF( "Failed to parse file: %s\n", srPath.c_str() );
		return;
	}

	// parsing time
	kvRoot.Solidify();

	ValveMaterial_t vmt;

	auto            AddTexture = [ & ]()
	{

    };

	KeyValue* kvRoot2 = kvRoot.children;
	KeyValue* kv      = kvRoot2->children;
	for ( int i = 0; i < kvRoot2->childCount; i++ )
	{
		Log_MsgF( "%s\n", kv->key.string );

		if ( ch_strcasecmp( kv->key.string, "$basetexture" ) == 0 )
		{
			std::string baseTexture = kv->value.string;
			// std::string searchPath  = srAssetPath + "/materials/" + baseTexture + ".vtf";
			std::string searchPath  = srAssetsOut + "/materials/" + baseTexture + ".ktx";
			bool        foundFile   = FileSys_IsFile( searchPath );

			// if ( foundFile )
			{
				vmt.aBaseTexture    = baseTexture;
				vmt.aBaseTextureAbs = searchPath;
			}
		}
		else if ( ch_strcasecmp( kv->key.string, "$alphatest" ) == 0 )
		{
			vmt.aAlphaTest = ( ch_strcasecmp( kv->value.string, "1" ) == 0 ) ? true : false;
		}
		else if ( ch_strcasecmp( kv->key.string, "$translucent" ) == 0 )
		{
			vmt.aTranslucent = ( ch_strcasecmp( kv->value.string, "1" ) == 0 ) ? true : false;
		}

		// TODO: handle patches

		kv = kv->next;
	}

	
	if ( vmt.aBaseTextureAbs.empty() )
	{
		Log_MsgF( "skipping %s because no basetexture\n", srPath.c_str() );
		return;
	}
	
	// TODO: IMPLEMENT PROPER JSON5 TO STRING
	std::string json5Buf;
	json5Buf += "{\n\tshader: \"basic_3d\",\n\t\n\tdiffuse: \"" + vmt.aBaseTextureAbs + "\",\n";

	if ( vmt.aAlphaTest )
	{
		json5Buf += "\talphaTest: true,\n";
	}

	json5Buf += "}\n";

	vmt.aChocMatPath = srAssetsOut + "/materials/" + srVMTPath + ".cmt";

	// Write the data
	FILE* fp = fopen( vmt.aChocMatPath.c_str(), "wb" );

	if ( fp == nullptr )
	{
		Log_ErrorF( "Failed to open file handle for writing cmt file: \"%s\"\n", vmt.aChocMatPath.c_str() );
		return;
	}

	fwrite( json5Buf.c_str(), sizeof( char ), json5Buf.size(), fp );
	fclose( fp );

	TextureCreateData_t createInfo{};
	createInfo.aUsage  = EImageUsage_Sampled;
	createInfo.aFilter = EImageFilter_Linear;

	Handle texture     = CH_INVALID_HANDLE;
	texture            = Graphics_LoadTexture( texture, vmt.aBaseTextureAbs, createInfo );
	Mat_SetVar( sMaterial, "diffuse", texture );

	if ( texture != CH_INVALID_HANDLE )
	{
		vmt.aBaseTextureSize = render->GetTextureSize( texture );
	}

	if ( vmt.aBaseTextureAbs.size() )
	{
		// int ret = Sys_ExecuteV(
		//   "VTFCmd.exe",
		//   "-folder", "INPUT_FOLDER_HERE" );
		// 
		// Log_Msg( "hi vtfcmd.exe hopefully in game folder for laziness lol\n" );
	}

	srMap.aValveMaterials[ sMaterial ] = vmt;
}


glm::vec2 Map_GetSidePlaneUV( VMFSolidSide_t& side, ValveMaterial_t& vmt, int plane )
{
	double u = glm::dot( side.aPoints[ plane ], side.aUAxis ) / ( vmt.aBaseTextureSize.x * side.aUAxisScale ) + side.aUAxisTranslation / vmt.aBaseTextureSize.x;
	double v = glm::dot( side.aPoints[ plane ], side.aVAxis ) / ( vmt.aBaseTextureSize.y * side.aVAxisScale ) + side.aVAxisTranslation / vmt.aBaseTextureSize.y;
	u        = -u + vmt.aBaseTextureSize.x;
	v        = -v + vmt.aBaseTextureSize.y;

	return { u, v };
}


void MapConverter_ConvertVMF( const std::string& srPath, const std::string& srOutPath, const std::string& srAssetPath, const std::string& srAssetsOut )
{
	std::vector< char > rawData = FileSys_ReadFile( srPath );

	if ( rawData.empty() )
	{
		Log_WarnF( "Failed to read file: %s\n", srPath.c_str() );
		return;
	}

	// append a null terminator for c strings
	rawData.push_back( '\0' );

	KeyValueRoot      kvRoot;
	KeyValueErrorCode err = kvRoot.Parse( rawData.data() );

	if ( err != KeyValueErrorCode::NO_ERROR )
	{
		Log_WarnF( "Failed to parse file: %s\n", srPath.c_str() );
		return;
	}

	// parsing time
	kvRoot.Solidify();

	ParsedVMF_t vmf;
	Model*      brushModel    = nullptr;
	Handle      brushHandle   = Graphics_CreateModel( &brushModel );

	Handle      shader        = Graphics_GetShader( "basic_3d" );
	Handle      worldMaterial = Graphics_CreateMaterial( "__world", shader );

	// Missing Texture by default
	ValveMaterial_t worldVmt;
	// this doesn't work? why?
	// worldVmt.aBaseTextureSize = render->GetTextureSize( 0 );
	worldVmt.aBaseTextureSize = render->GetTextureSize( 0 );
	Mat_SetVar( worldMaterial, "diffuse", CH_INVALID_HANDLE );

	vmf.aValveMaterials[ worldMaterial ] = worldVmt;

	vmf.aWorldMeshBuilder.Start( brushModel, "world" );
	vmf.aWorldMeshBuilder.SetMaterial( worldMaterial );

	KeyValue* kv      = kvRoot.children;
	for ( int i = 0; i < kvRoot.childCount; i++ )
	{
		if ( strcmp( kv->key.string, "world" ) == 0 )
		{
			if ( !ConvertWorld( vmf, kv ) )
			{
				Log_Error( "Failed to parse world\n" );
				return;
			}
		}
		else if ( strcmp( kv->key.string, "entity" ) == 0 )
		{
			if ( !ConvertEntity( vmf, kv ) )
			{
				Log_Error( "Failed to parse entity\n" );
				return;
			}
		}

		kv = kv->next;
	}

	// Convert all found materials
	for ( auto& [ path, handle ] : vmf.aMaterials )
	{
		std::string searchPath = srAssetPath + "/materials/" + path + ".vmt";

		bool        foundFile  = FileSys_IsFile( searchPath );

		if ( foundFile )
		{
			ConvertVMT( vmf, handle, path, searchPath, srAssetPath, srAssetsOut );
		}
	}

	// Now, let's convert all solids to meshes with mesh builder
	
	for ( VMFSolid_t& solid : vmf.aSolids )
	{
		for ( VMFSolidSide_t& side : solid.aSides )
		{
			if ( side.aSkip )
				continue;

			size_t surfIndex = vmf.GetSurfaceIndex( side.aMaterial );

			vmf.aWorldMeshBuilder.SetCurrentSurface( surfIndex );
			vmf.aWorldMeshBuilder.SetMaterial( side.aMaterial );

			auto vmtIt = vmf.aValveMaterials.find( side.aMaterial );
			if ( vmtIt == vmf.aValveMaterials.end() )
			{
				Log_Error( "Unable to find Valve Material?\n" );
			}

			// Use fallback material
			ValveMaterial_t& vmt = ( vmtIt != vmf.aValveMaterials.end() ) ? vmtIt->second : worldVmt;

			for ( int i = 0; i < 3; i++ )
			{
				vmf.aWorldMeshBuilder.SetPos( side.aPoints[ i ] );
				vmf.aWorldMeshBuilder.SetTexCoord( Map_GetSidePlaneUV( side, vmt, i ) );
				vmf.aWorldMeshBuilder.NextVertex();
			}

			// for ( int i = 0; i < 3; i++ )
			// {
			// 	vmf.aWorldMeshBuilder.SetPos( side.aTriangle.aPos[ i ] );
			// 	// vmf.aWorldMeshBuilder.SetTexCoord( Map_GetSidePlaneUV( side, vmt, i ) );
			// 	vmf.aWorldMeshBuilder.NextVertex();
			// }
		}
	}

	// lets draw it lmao
	vmf.aWorldMeshBuilder.End();

	Handle        renderHandle = Graphics_CreateRenderable( brushHandle );
	Renderable_t* renderable   = Graphics_GetRenderableData( renderHandle );
	renderable->aTestVis       = false;

	Graphics_UpdateRenderableAABB( renderHandle );
}

