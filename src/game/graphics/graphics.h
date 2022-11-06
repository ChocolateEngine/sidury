#pragma once

#include "core/core.h"
#include "render/irender.h"

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
	EShaderFlags_None             = 0,
	EShaderFlags_Sampler          = ( 1 << 0 ),  // Shader Uses Texture Sampler Array
	EShaderFlags_ViewInfo         = ( 1 << 1 ),  // Shader Uses View Info UBO
	EShaderFlags_VertexAttributes = ( 1 << 2 ),  // Shader Uses Vertex Attributes
	EShaderFlags_PushConstant     = ( 1 << 3 ),  // Shader makes of a Push Constant
	EShaderFlags_MaterialUniform  = ( 1 << 4 ),  // Shader Makes use of Material Uniform Buffers
	EShaderFlags_Lights           = ( 1 << 5 ),  // Shader Makes use of Lights
};


enum ELightType // : char
{
	ELightType_Directional,  // World
	ELightType_Point,
	ELightType_Cone,
	ELightType_Capsule,
};


// ======================================================


struct ViewportCamera_t
{
	void ComputeProjection( float sWidth, float sHeight )
	{
		float hAspect = (float)sWidth / (float)sHeight;
		float vAspect = (float)sHeight / (float)sWidth;

		float V       = 2.0f * atanf( tanf( glm::radians( aFOV ) / 2.0f ) * vAspect );

		aProjMat      = glm::perspective( V, hAspect, aNearZ, aFarZ );

		aProjViewMat  = aProjMat * aViewMat;
	}

	float     aNearZ;
	float     aFarZ;
	float     aFOV;

	glm::mat4 aViewMat;
	glm::mat4 aProjMat;

	// projection matrix * view matrix
	glm::mat4 aProjViewMat;
};


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

// TODO: use descriptor indexing on forward rendering
// would allow you to do all lighting on a model in one draw call
struct LightInfo_t
{
	int aCountWorld   = 0;
	int aCountPoint   = 0;
	int aCountCone    = 0;
	int aCountCapsule = 0;
};

// Light Types
struct UBO_LightDirectional_t
{
	alignas( 16 ) glm::vec4 aColor{};
	alignas( 16 ) glm::vec3 aDir{};
	// int aShadow = -1;
};

struct UBO_LightPoint_t
{
	alignas( 16 ) glm::vec4 aColor{};
	alignas( 16 ) glm::vec3 aPos{};
	float aRadius = 0.f;
	// int   aShadow = -1;
};

struct UBO_LightCone_t
{
	alignas( 16 ) glm::vec4 aColor{};
	alignas( 16 ) glm::vec3 aPos{};
	alignas( 16 ) glm::vec3 aDir{};
	alignas( 16 ) glm::vec2 aFov{};
	int aShadow = -1;
};

struct UBO_LightCapsule_t
{
	alignas( 16 ) glm::vec4 aColor{};
	alignas( 16 ) glm::vec3 aPos{};
	alignas( 16 ) glm::vec3 aDir{};
	float     aLength    = 0.f;
	float     aThickness = 0.f;
};

// maybe use this instead? would be easier to manage
struct Light_t
{
	ELightType aType = ELightType_Directional;
	glm::vec3  aColor{};
	glm::vec3  aPos{};
	glm::vec3  aAng{};
	float      aInnerFov = 45.f;
	float      aOuterFov = 45.f;
	float      aRadius   = 0.f;
	float      aLength   = 0.f;
	bool       aShadow   = true;
};

struct UniformBufferArray_t
{
	Handle                aLayout = InvalidHandle;
	std::vector< Handle > aSets;
};

struct UBO_ViewInfo_t
{
	Handle                aLayout = InvalidHandle;
	std::vector< Handle > aSets;
	Handle                aBuffer;
};

struct ViewInfo_t
{
	glm::mat4 aViewProj{};
	glm::mat4 aProjection{};
	glm::mat4 aView{};
	glm::vec3 aViewPos{};
};

extern ViewInfo_t  gViewInfo;
extern bool        gViewInfoUpdate;


// Push Constant Function Pointers
using FResetPushData = void();
using FModelPushConstants = void( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo );
using FSetupModelPushData = void( ModelSurfaceDraw_t& srDrawInfo );


struct ShaderInfo_t
{
	EPipelineBindPoint aBindPoint    = EPipelineBindPoint_Graphics;
	EShaderFlags       aFlags        = EShaderFlags_None;
	VertexFormat       aVertexFormat = VertexFormat_None;
};


struct IPushConstant
{
	virtual void ResetPushData()
	{
	}

	// kinda weird and tied to models, hmm
	virtual void ModelPushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo )
	{
	}

	virtual void SetupModelPushData( ModelSurfaceDraw_t& srDrawInfo )
	{
	}
};


struct IShaderMaterial
{
	virtual void ResetPushData()
	{
	}

	// kinda weird and tied to models, hmm
	virtual void ModelPushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo )
	{
	}

	virtual void SetupModelPushData( ModelSurfaceDraw_t& srDrawInfo )
	{
	}
};


// DO NOT ADD ANY LOCAL VARIABLES TO THIS WHEN YOU OVERRIDE IT !!!
struct IPipelineLayout
{

};


struct IShader
{
	// Returns the shader name, and fills in data in the struct with general shader info 
	virtual const char* GetShaderInfo( ShaderInfo_t& srInfo )                                             = 0;

	// Shader Creation Info
	virtual void        GetPushConstantData( std::vector< PushConstantRange_t >& srPushConstants )        = 0;

	virtual void        GetGraphicsCreateInfo( Handle sRenderPass, GraphicsPipelineCreate_t& srGraphics ) = 0;

	// Optional to override
	// Used if the shader has the push constants flag
	virtual void ResetPushData()
	{
	}

	// kinda weird and tied to models, hmm
	virtual void ModelPushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo )
	{
	}

	virtual void SetupModelPushData( ModelSurfaceDraw_t& srDrawInfo )
	{
	}

	// Used if the shader has the material data flag
	virtual void AddMaterial( Handle sMat )
	{
	}

	virtual void RemoveMaterial( Handle sMat )
	{
	}

	virtual void UpdateMaterialData( Handle sMat )
	{
	}
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
// Name is a path to the cmt file if it was loaded on disk
// EXAMPLE: C:/chocolate/sidury/materials/dev/grid01.cmt
// NAME: materials/dev/grid01
Handle             Graphics_FindMaterial( const char* spName );

// Is This Material an Error Material?
bool               Graphics_IsErrorMaterial( Handle sMaterial );

// Get a fallback error material
Handle             Graphics_GetErrorMaterial( Handle shShader );

// Get the total amount of materials created
size_t             Graphics_GetMaterialCount();

// Tell all materials to rebuild
void               Graphics_SetAllMaterialsDirty();

// Modifying Material Data
const char*        Mat_GetName( Handle mat );
size_t             Mat_GetVarCount( Handle mat );
EMatVar            Mat_GetVarType( Handle mat, size_t sIndex );

Handle             Mat_GetShader( Handle mat );
void               Mat_SetShader( Handle mat, Handle shShader );

VertexFormat       Mat_GetVertexFormat( Handle mat );

void               Mat_SetVar( Handle mat, const std::string& name, Handle texture );
void               Mat_SetVar( Handle mat, const std::string& name, float data );
void               Mat_SetVar( Handle mat, const std::string& name, int data );
void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec2& data );
void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec3& data );
void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec4& data );

int                Mat_GetTextureIndex( Handle mat, std::string_view name, Handle fallback = InvalidHandle );
Handle             Mat_GetTexture( Handle mat, std::string_view name, Handle fallback = InvalidHandle );
float              Mat_GetFloat( Handle mat, std::string_view name, float fallback = 0.f );
int                Mat_GetInt( Handle mat, std::string_view name, int fallback = 0 );
const glm::vec2&   Mat_GetVec2( Handle mat, std::string_view name, const glm::vec2& fallback = {} );
const glm::vec3&   Mat_GetVec3( Handle mat, std::string_view name, const glm::vec3& fallback = {} );
const glm::vec4&   Mat_GetVec4( Handle mat, std::string_view name, const glm::vec4& fallback = {} );

// ---------------------------------------------------------------------------------------
// Shaders

bool               Graphics_ShaderInit( bool sRecreate );
Handle             Graphics_GetShader( std::string_view sName );
const char*        Graphics_GetShaderName( Handle sShader );
void               Graphics_AddPipelineLayouts( PipelineLayoutCreate_t& srPipeline, EShaderFlags sFlags );

bool               Shader_Bind( Handle sCmd, u32 sIndex, Handle sShader );
void               Shader_ResetPushData();
bool               Shader_SetupRenderableDrawData( Handle sShader, ModelSurfaceDraw_t& srRenderable );
bool               Shader_PreRenderableDraw( Handle sCmd, u32 sIndex, Handle sShader, ModelSurfaceDraw_t& srRenderable );
VertexFormat       Shader_GetVertexFormat( Handle sShader );

// ---------------------------------------------------------------------------------------
// Buffers

void               Graphics_CreateVertexBuffers( Mesh& spMesh, const char* spDebugName = nullptr );
void               Graphics_CreateIndexBuffer( Mesh& spMesh, const char* spDebugName = nullptr );

// ---------------------------------------------------------------------------------------
// Lighting

Light_t*           Graphics_CreateLight( ELightType sType );
void               Graphics_EnableLight( Light_t* spLight );
void               Graphics_DisableLight( Light_t* spLight );
void               Graphics_UpdateLight( Light_t* spLight );
void               Graphics_DestroyLight( Light_t* spLight );

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

void               Graphics_DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor );

// ---------------------------------------------------------------------------------------
// Other

GraphicsFmt        Graphics_GetVertexAttributeFormat( VertexAttribute attrib );
size_t             Graphics_GetVertexAttributeTypeSize( VertexAttribute attrib );
size_t             Graphics_GetVertexAttributeSize( VertexAttribute attrib );
size_t             Graphics_GetVertexFormatSize( VertexFormat format );

void               Graphics_GetVertexBindingDesc( VertexFormat format, std::vector< VertexInputBinding_t >& srAttrib );
void               Graphics_GetVertexAttributeDesc( VertexFormat format, std::vector< VertexInputAttribute_t >& srAttrib );

