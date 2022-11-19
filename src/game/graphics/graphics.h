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

extern IRender* render;

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


inline glm::mat4 Util_ComputeProjection( float sWidth, float sHeight, float sNearZ, float sFarZ, float sFov )
{
	float hAspect = (float)sWidth / (float)sHeight;
	float vAspect = (float)sHeight / (float)sWidth;

	float V       = 2.0f * atanf( tanf( glm::radians( sFov ) / 2.0f ) * vAspect );

	return glm::perspective( V, hAspect, sNearZ, sFarZ );
}


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


enum EFrustum
{
	EFrustum_Top,
	EFrustum_Bottom,
	EFrustum_Right,
	EFrustum_Left,
	EFrustum_Near,
	EFrustum_Far,
	EFrustum_Count,
};


struct Frustum_t
{
	glm::vec4 aPlanes[ EFrustum_Count ];
	glm::vec3 aPoints[ 8 ];

	bool IsBoxVisible( const glm::vec3& minp, const glm::vec3& maxp ) const;
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
	}

private:
	VertAttribData_t( const VertAttribData_t& other );
};


struct VertexData_t : public RefCounted
{
	VertexFormat                 aFormat = VertexFormat_None;
	u32                          aCount  = 0;
	ChVector< VertAttribData_t > aData;
	ChVector< uint32_t >         aIndices;
};


struct ModelBuffers_t : public RefCounted
{
	ChVector< Handle > aVertex;
	Handle             aIndex = InvalidHandle;

	~ModelBuffers_t()
	{
		for ( auto& buf : aVertex )
			if ( buf )
				render->DestroyBuffer( buf );

		if ( aIndex )
			render->DestroyBuffer( aIndex );
	}
};


struct Mesh
{
	u32    aVertexOffset;
	u32    aVertexCount;

	u32    aIndexOffset;
	u32    aIndexCount;

	Handle aMaterial;
};


struct Model
{
	ModelBuffers_t*  apBuffers    = nullptr;
	VertexData_t*    apVertexData = nullptr;

	ChVector< Mesh > aMeshes;
};


struct ModelBBox_t
{
	glm::vec3 aMin{};
	glm::vec3 aMax{};
};


struct Renderable_t
{
	// used in actual drawing
	Handle      aModel;
	glm::mat4   aModelMatrix;

	// used for calculating render lists
	ModelBBox_t aAABB;
	bool        aTestVis;
	bool        aCastShadow;
	bool        aVisible;
};


struct SurfaceDraw_t
{
	Handle aDrawData;
	size_t aSurface;
};


struct Scene_t
{
	// std::vector< Model* > aModels;
	ChVector< Handle > aModels{};
};


struct SceneDraw_t
{
	Handle                aScene;
	std::vector< Handle > aDraw;
};


struct RenderList_t
{
	// List of Renderables
	ChVector< Renderable_t* > aModels;
};


struct RenderLayer_t
{
	// specify a render pass?

	// contain depth options

	// List of Renderables
	ChVector< Renderable_t* > aModels;
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
	int aViewInfo = -1;
	int aShadow   = -1;
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
	int aViewInfo = -1;
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
	bool       aEnabled  = true;
};

struct UniformBufferArray_t
{
	Handle                aLayout = InvalidHandle;
	std::vector< Handle > aSets;
};

struct UBO_ViewInfo_t
{
	glm::mat4 aProjView{};
	glm::mat4 aProjection{};
	glm::mat4 aView{};
	glm::vec3 aViewPos{};
	float     aNearZ = 0.f;
	float     aFarZ  = 0.f;
};

struct ViewInfo_t
{
	glm::mat4  aProjView{};
	glm::mat4  aProjection{};
	glm::mat4  aView{};
	glm::vec3  aViewPos{};
	float      aNearZ = 0.f;
	float      aFarZ  = 0.f;

	glm::uvec2 aSize{};
	bool       aActive = true;

	// HACK: if this is set, it overrides the shader used for all renderables in this view
	Handle     aShaderOverride = InvalidHandle;
};

extern std::vector< ViewInfo_t > gViewInfo;
extern bool                      gViewInfoUpdate;


// Push Constant Function Pointers
using FShader_ResetPushData = void();
using FShader_SetupPushData = void( Renderable_t* spDrawData, SurfaceDraw_t& srDrawInfo );
using FShader_PushConstants = void( Handle cmd, Handle sLayout, SurfaceDraw_t& srDrawInfo );

using FShader_Init = bool();
using FShader_Destroy = void();

using FShader_GetPipelineLayoutCreate   = void( PipelineLayoutCreate_t& srPipeline );
using FShader_GetGraphicsPipelineCreate = void( GraphicsPipelineCreate_t& srGraphics );


struct IShaderPush
{
	FShader_ResetPushData* apReset = nullptr;
	FShader_SetupPushData* apSetup = nullptr;
	FShader_PushConstants* apPush  = nullptr;
};


struct ShaderCreate_t
{
	const char*                        apName           = nullptr;
	ShaderStage                        aStages          = ShaderStage_None;
	EPipelineBindPoint                 aBindPoint       = EPipelineBindPoint_Graphics;
	EShaderFlags                       aFlags           = EShaderFlags_None;
	EDynamicState                      aDynamicState    = EDynamicState_None;
	VertexFormat                       aVertexFormat    = VertexFormat_None;

	FShader_Init*                      apInit           = nullptr;
	FShader_Destroy*                   apDestroy        = nullptr;

	FShader_GetPipelineLayoutCreate*   apLayoutCreate   = nullptr;
	FShader_GetGraphicsPipelineCreate* apGraphicsCreate = nullptr;

	IShaderPush*                       apShaderPush     = nullptr;
};


// stored data internally
struct ShaderData_t
{
	ShaderStage   aStages       = ShaderStage_None;
	EShaderFlags  aFlags        = EShaderFlags_None;
	EDynamicState aDynamicState = EDynamicState_None;
	Handle        aLayout       = InvalidHandle;
	IShaderPush*  apPush        = nullptr;
};


// ---------------------------------------------------------------------------------------
// Models

Handle             Graphics_LoadModel( const std::string& srPath );
Handle             Graphics_CreateModel( Model** spModel );
void               Graphics_FreeModel( Handle hModel );
Model*             Graphics_GetModelData( Handle hModel );

void               Model_SetMaterial( Handle shModel, size_t sSurface, Handle shMat );
Handle             Model_GetMaterial( Handle shModel, size_t sSurface );

// ---------------------------------------------------------------------------------------
// Scenes

Handle             Graphics_LoadScene( const std::string& srPath );
void               Graphics_FreeScene( Handle sScene );

SceneDraw_t*       Graphics_AddSceneDraw( Handle sScene );
void               Graphics_RemoveSceneDraw( SceneDraw_t* spScene );

size_t             Graphics_GetSceneModelCount( Handle sScene );
Handle             Graphics_GetSceneModel( Handle sScene, size_t sIndex );

// ---------------------------------------------------------------------------------------
// Render Lists

// RenderList_t*      Graphics_CreateRenderList();
// void               Graphics_DestroyRenderList( RenderList_t* spList );
// void               Graphics_DrawRenderList( RenderList_t* spList );

// ---------------------------------------------------------------------------------------
// Render Layers

// RenderLayer_t*     Graphics_CreateRenderLayer();
// void               Graphics_DestroyRenderLayer( RenderLayer_t* spLayer );

// control ordering of them somehow?
// maybe do Graphics_DrawRenderLayer() ?

// render layers, to the basic degree that i want, is something contains a list of models to render
// you can manually sort these render layers to draw them in a specific order
// (viewmodel first, standard view next, skybox last)
// and is also able to control depth, for skybox and viewmodel drawing
// or should that be done outside of that? hmm, no idea
// 
// maybe don't add items to the render list struct directly
// or, just straight up make the "RenderLayer_t" thing as a Handle
// how would this work with immediate mode style drawing? good for a rythem like game
// and how would it work for VR, or multiple viewports in a level editor or something?
// and shadowmapping? overthinking this? probably

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
bool               Shader_SetupRenderableDrawData( Renderable_t* spModelDraw, ShaderData_t* spShaderData, SurfaceDraw_t& srRenderable );
bool               Shader_PreRenderableDraw( Handle sCmd, u32 sIndex, Handle sShader, SurfaceDraw_t& srRenderable );

VertexFormat       Shader_GetVertexFormat( Handle sShader );
ShaderData_t*      Shader_GetData( Handle sShader );

// ---------------------------------------------------------------------------------------
// Buffers

void               Graphics_CreateVertexBuffers( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName = nullptr );
void               Graphics_CreateIndexBuffer( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName = nullptr );
// void               Graphics_CreateModelBuffers( ModelBuffers_t* spBuffers, VertexData_t* spVertexData, bool sCreateIndex, const char* spDebugName );

// ---------------------------------------------------------------------------------------
// Lighting

Light_t*           Graphics_CreateLight( ELightType sType );
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

void               Graphics_PushViewInfo( const ViewInfo_t& srViewInfo );
void               Graphics_PopViewInfo();
ViewInfo_t&        Graphics_GetViewInfo();

Handle             Graphics_CreateRenderable( Handle sModel );
Renderable_t*      Graphics_GetRenderableData( Handle sRenderable );
void               Graphics_FreeRenderable( Handle sRenderable );
void               Graphics_UpdateRenderableAABB( Handle sRenderable );

ModelBBox_t        Graphics_CreateWorldAABB( glm::mat4& srMatrix, const ModelBBox_t& srBBox );

// ---------------------------------------------------------------------------------------
// Debug Rendering

void               Graphics_DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor );
void               Graphics_DrawBBox( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor );
void               Graphics_DrawProjView( const glm::mat4& srProjView );
void               Graphics_DrawFrustum( const Frustum_t& srFrustum );

// ---------------------------------------------------------------------------------------
// Other

GraphicsFmt        Graphics_GetVertexAttributeFormat( VertexAttribute attrib );
size_t             Graphics_GetVertexAttributeTypeSize( VertexAttribute attrib );
size_t             Graphics_GetVertexAttributeSize( VertexAttribute attrib );
size_t             Graphics_GetVertexFormatSize( VertexFormat format );

void               Graphics_GetVertexBindingDesc( VertexFormat format, std::vector< VertexInputBinding_t >& srAttrib );
void               Graphics_GetVertexAttributeDesc( VertexFormat format, std::vector< VertexInputAttribute_t >& srAttrib );

