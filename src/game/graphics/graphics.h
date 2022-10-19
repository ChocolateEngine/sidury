#pragma once

#include "core/core.h"

// ======================================================
// User Land abstraction of renderer
// ======================================================

LOG_CHANNEL2( ClientGraphics )

struct VertexInputBinding_t;
struct VertexInputAttribute_t;

enum class GraphicsFmt;

// hack for bad entity system
struct HModel
{
	Handle handle;
};

enum EModelData
{
	EModelData_None        = 0,
	EModelData_HasSurfaces = ( 1 << 0 ),
	EModelData_HasSkeleton = ( 1 << 1 ),
	EModelData_HasMorphs   = ( 1 << 2 ),
};


// Always in order from top to bottom in terms of order in each vertex
// technically, you could use the above only
enum VertexAttribute : u8
{
	VertexAttribute_Position,  // vec3
	VertexAttribute_Normal,    // vec3
	VertexAttribute_TexCoord,  // vec2
	VertexAttribute_Color,     // vec3 (should be vec4 probably)

	// this and morphs will be calculated in a compute shader
	// VertexAttribute_BoneIndex,        // uvec4
	// VertexAttribute_BoneWeight,       // vec4

	VertexAttribute_Count
};


// Flags to Determine what the Vertex Data contains
using VertexFormat  = u16;
enum : VertexFormat
{
	VertexFormat_None     = 0,
	VertexFormat_Position = ( 1 << VertexAttribute_Position ),
	VertexFormat_Normal   = ( 1 << VertexAttribute_Normal ),
	VertexFormat_TexCoord = ( 1 << VertexAttribute_TexCoord ),
	VertexFormat_Color    = ( 1 << VertexAttribute_Color ),
};


using EMatVar = char;
enum : EMatVar
{
	EMatVar_Invalid,
	EMatVar_Texture,
	EMatVar_Float,
	EMatVar_Int,
	EMatVar_Vec2,
	EMatVar_Vec3,
	EMatVar_Vec4,
};


// shader stuff
using EShaderFlags = int;
enum : EShaderFlags
{
	EShaderFlags_None          = 0,
	EShaderFlags_PushConstant  = ( 1 << 0 ),
	EShaderFlags_UniformBuffer = ( 1 << 1 ),
};


// ======================================================


struct RenderTarget_t
{

};


struct MeshBlendShapeData_t
{
	std::vector< std::string > aNames;
};


struct MeshBlendShapeDrawData_t
{
	std::vector< Handle > aBuffers;  // all mesh blend shape buffers
	std::vector< float >  aValues;
};


struct VertAttribData_t
{
	VertexAttribute aAttrib = VertexAttribute_Position;
	void*           apData  = nullptr;

	VertAttribData_t()
	{
	}

	~VertAttribData_t()
	{
		if ( apData )
			free( apData );

		// TEST
		apData = nullptr;
	}

	// disabled because of std::vector::resize()
// private:
// 	VertAttribData_t( const VertAttribData_t& other );
};


struct VertexData_t
{
	VertexFormat                    aFormat;
	u32                             aCount;
	std::vector< VertAttribData_t > aData;
	bool                            aLocked;  // if locked, we can create a vertex buffer
};


struct Mesh
{
	std::vector< Handle >   aVertexBuffers;
	Handle                  aIndexBuffer;

	Handle                  aMaterial;

	// NOTE: these could be a pointer, and multiple surfaces could share the same vertex data and indices
	VertexData_t            aVertexData;
	std::vector< uint32_t > aIndices;
};


struct Model
{
	std::vector< Mesh > aMeshes;

	void SetSurfaceCount( size_t sCount )
	{
		aMeshes.resize( sCount );
	}
};


struct ModelDraw_t
{
	Handle    aModel;
	glm::mat4 aModelMatrix;
};


struct ModelSurfaceDraw_t
{
	ModelDraw_t* apDraw;
	size_t       aSurface;
};


// ---------------------------------------------------------------------------------------
// Models

Handle             Graphics_LoadModel( const std::string& srPath );
Handle             Graphics_AddModel( Model* spModel );
void               Graphics_FreeModel( Handle hModel );
Model*             Graphics_GetModelData( Handle hModel );

void               Model_SetMaterial( Handle shModel, size_t sSurface, Handle shMat );
Handle             Model_GetMaterial( Handle shModel, size_t sSurface );

// ---------------------------------------------------------------------------------------
// Materials

// Load a cmt file from disk
Handle             Graphics_LoadMaterial( const std::string& srPath );

// Create a new material with a name and a shader
Handle             Graphics_CreateMaterial( const std::string& srName, Handle shShader );

// Free a material
void               Graphics_FreeMaterial( Handle sMaterial );

// Find a material by name
// Name is a full path to the cmt file
// EXAMPLE: C:/chocolate/sidury/materials/dev/grid01.cmt
// NAME: dev/grid01
Handle             Graphics_FindMaterial( std::string_view srName );

// Is This Material an Error Material?
bool               Graphics_IsErrorMaterial( Handle sMaterial );

// Get a fallback error material
Handle             Graphics_GetErrorMaterial( Handle shShader );

// Get Fallback Texture if the texture doesn't exist
Handle             Graphics_GetMissingTexture();

// Get the total amount of materials created
size_t             Graphics_GetMaterialCount();

// Modifying Material Data
const std::string& Mat_GetName( Handle mat );
size_t             Mat_GetVarCount( Handle mat );
EMatVar            Mat_GetVarType( Handle mat, size_t sIndex );

Handle             Mat_GetShader( Handle mat );
void               Mat_SetShader( Handle mat, Handle shShader );

void               Mat_SetVar( Handle mat, const std::string& name, Handle texture );
void               Mat_SetVar( Handle mat, const std::string& name, float data );
void               Mat_SetVar( Handle mat, const std::string& name, int data );
void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec2& data );
void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec3& data );
void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec4& data );

Handle             Mat_GetTexture( Handle mat, std::string_view name, Handle fallback = InvalidHandle );
float              Mat_GetFloat( Handle mat, std::string_view name, float fallback = 0.f );
int                Mat_GetInt( Handle mat, std::string_view name, int fallback = 0 );
const glm::vec2&   Mat_GetVec2( Handle mat, std::string_view name, const glm::vec2& fallback = {} );
const glm::vec3&   Mat_GetVec3( Handle mat, std::string_view name, const glm::vec3& fallback = {} );
const glm::vec4&   Mat_GetVec4( Handle mat, std::string_view name, const glm::vec4& fallback = {} );

// ---------------------------------------------------------------------------------------
// Shaders

Handle             Graphics_GetShader( std::string_view name );

// ---------------------------------------------------------------------------------------
// Buffers

void               Graphics_CreateVertexBuffers( Mesh& spMesh );
void               Graphics_CreateIndexBuffer( Mesh& spMesh );

// ---------------------------------------------------------------------------------------
// Render Targets

// ---------------------------------------------------------------------------------------
// Rendering

bool               Graphics_Init();
void               Graphics_Shutdown();

void               Graphics_NewFrame();
void               Graphics_Reset();
void               Graphics_Present();

void               Graphics_SetViewProjMatrix( const glm::mat4& srMat );
const glm::mat4&   Graphics_GetViewProjMatrix();

void               Graphics_DrawModel( ModelDraw_t* spDrawInfo );

// ---------------------------------------------------------------------------------------
// Other

GraphicsFmt        Graphics_GetVertexAttributeFormat( VertexAttribute attrib );
size_t             Graphics_GetVertexAttributeTypeSize( VertexAttribute attrib );
size_t             Graphics_GetVertexAttributeSize( VertexAttribute attrib );
size_t             Graphics_GetVertexFormatSize( VertexFormat format );

void               Graphics_GetVertexBindingDesc( VertexFormat format, std::vector< VertexInputBinding_t >& srAttrib );
void               Graphics_GetVertexAttributeDesc( VertexFormat format, std::vector< VertexInputAttribute_t >& srAttrib );

