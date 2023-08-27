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

using Entity = size_t;


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
	EMatVar_Bool,
	EMatVar_Vec2,
	EMatVar_Vec3,
	EMatVar_Vec4,
};


// shader stuff
using EShaderFlags = int;
enum : EShaderFlags
{
	EShaderFlags_None             = 0,
	EShaderFlags_VertexAttributes = ( 1 << 0 ),  // Shader Uses Vertex Attributes
	EShaderFlags_PushConstant     = ( 1 << 1 ),  // Shader Uses a Push Constant
};


enum ERenderPass
{
	ERenderPass_Graphics, 
	ERenderPass_Shadow, 
	ERenderPass_Count, 
};


using ELightType = int;
enum : ELightType
{
	ELightType_Directional,  // World
	ELightType_Point,
	ELightType_Cone,
	// ELightType_Capsule,
	ELightType_Count,
};


using ERenderableFlags = unsigned char;
enum : ERenderableFlags
{
	ERenderableFlags_None       = 0,
	ERenderableFlags_Visible    = ( 1 << 0 ),
	ERenderableFlags_TestVis    = ( 1 << 1 ),
	ERenderableFlags_CastShadow = ( 1 << 2 ),
	// ERenderableFlags_RecieveShadow = ( 1 << 3 ),
};


enum EShaderSlot
{
	// Global Resources usable by all shaders and render passes (ex. Textures)
	EShaderSlot_Global,

	// Resources bound once per render pass (ex. Viewport Info)
	// EShaderSlot_PerPass,

	// Resources bound on each shader bind (ex. Materials)
	EShaderSlot_PerShader,  // PerMaterial

	// Per Object Resources (ex. Lights)
	// EShaderSlot_PerObject,

	EShaderSlot_Count,
};


// ======================================================


inline glm::mat4 Util_ComputeProjection( float sWidth, float sHeight, float sNearZ, float sFarZ, float sFov )
{
	float hAspect = (float)sWidth / (float)sHeight;
	float vAspect = (float)sHeight / (float)sWidth;

	float V       = 2.0f * atanf( tanf( glm::radians( sFov ) / 2.0f ) * vAspect );

	return glm::perspective( V, hAspect, sNearZ, sFarZ );
}


// TODO: Get rid of this, merge it with the ViewportShader_t struct, it's the same thing
struct ViewportCamera_t
{
	void ComputeProjection( float sWidth, float sHeight )
	{
		aProjMat     = Util_ComputeProjection( sWidth, sHeight, aNearZ, aFarZ, aFOV );
		aProjViewMat = aProjMat * aViewMat;
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
	std::vector< ChHandle_t > aBuffers;  // all mesh blend shape buffers
	std::vector< float >      aValues;
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


struct VertFormatData_t
{
	VertexFormat aFormat = VertexFormat_None;
	void*        apData  = nullptr;

	VertFormatData_t()
	{
	}

	~VertFormatData_t()
	{
		if ( apData )
			free( apData );
	}

private:
	VertFormatData_t( const VertFormatData_t& other );
};


struct VertexData_t
{
	VertexFormat                             aFormat = VertexFormat_None;
	u32                                      aCount  = 0;
	ChVector< VertAttribData_t >             aData;
	ChVector< uint32_t >                     aIndices;

	// ChVector< ChVector< VertAttribData_t > > aBlendShapeData;
	u32                                      aBlendShapeCount = 0;
	VertFormatData_t                         aBlendShapeData;  // size of data is (blend shape count) * (vertex count)
};


struct ModelBuffers_t
{
	// TODO: only have one vertex buffer again, and add an attributes and skin buffer, like godot does
	ChVector< ChHandle_t > aVertex;
	ChHandle_t             aIndex  = CH_INVALID_HANDLE;

	// Contains Morph Information
	ChHandle_t             aMorphs = CH_INVALID_HANDLE;

	// Contains Bones and Bone Weights
	ChHandle_t             aSkin   = CH_INVALID_HANDLE;

	~ModelBuffers_t()
	{
		for ( auto& buf : aVertex )
			if ( buf )
				render->DestroyBuffer( buf );

		if ( aIndex )
			render->DestroyBuffer( aIndex );

		if ( aSkin )
			render->DestroyBuffer( aSkin );
	}
};


// A Mesh is a simple struct that only contains vertex/index counts, offsets, and a material
// It is intended to be part of a Model struct, where the buffers and vertex data are
struct Mesh
{
	u32        aVertexOffset;
	u32        aVertexCount;

	u32        aIndexOffset;
	u32        aIndexCount;

	ChHandle_t aMaterial;
};


// TODO: here's an idea, store a handle to ModelBuffers_t and VertexData_t
// internally, we will store a "handle count" or "instance count" or something idk
// and then every time a model or scene is loaded, just up that internal ref count
// and lower it when a model or scene is freed
// they will never free themselves, only the system managing it will free it

// A Model contains vertex and index buffers, vertex data, and a vector of meshes the model contains
struct Model : public RefCounted
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


// A Renderable is an instance of a model for drawing to the screen
// You can have multiple renderables point to one model, this is how you draw many instances of a single tree model, or etc.
// It contains a model handle, a model matrix
// It also contains an AABB, and bools for testing vis, casting a shadow, and whether it's visible or not
struct Renderable_t
{
	// used in actual drawing
	ChHandle_t             aModel;
	glm::mat4              aModelMatrix;

	// TODO: store the materials for each mesh here so you can override them for this renderable
	// ChVector< ChHandle_t > aMaterials
	// or
	// u32         aMeshCount;
	// ChHandle_t* apMaterials;

	// used for calculating render lists
	ModelBBox_t            aAABB;

	// used for blend shapes, i don't like this here because very few models will have blend shapes
	ChVector< float >      aBlendShapeWeights;

	// technically we could have this here for skinning results if needed
	// I don't like this here as only very few models will use skinning
	ChVector< ChHandle_t > aVertexBuffers;
	ChHandle_t             aBlendShapeWeightsBuffer;

	// I don't like these bools that have to be checked every frame
	bool                   aTestVis;
	bool                   aCastShadow;
	bool                   aVisible;
	bool                   aBlendShapesDirty;  // AAAAAAA
};


// Surface Draw contains drawing information on how to draw a surface of a renderable
// It contains draw data and a mesh surface index
// Unique for each viewport
struct SurfaceDraw_t
{
	ChHandle_t aRenderable;
	size_t     aSurface;
	u32        aShaderSlot;  // index into core data shader draw
};


// For Bindless Rendering?
// instead of drawing on material at a time,
// we can group all materials on that model that share the same shader
// and then do it in one draw call
// Or, we can try to draw every surface that uses that shader in one draw call
// making even further use of bindless rendering
// though debugging it would be tricky, but it would be far faster to render tons of things
// 
// just imagine in renderdoc, you see a shader bind, one draw call, and everything using that shader is just drawn
// 
struct RenderableSurfaceDrawList_t
{
	ChHandle_t aRenderable;
	u32*   apSurfaces;
	u32    aSurfaceCount;
	u32    aShaderSlot;  // index into core data shader draw
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


struct RenderPassData_t
{
	ChVector< ChHandle_t > aSets;
};


struct Light_t
{
	ELightType aType = ELightType_Directional;
	glm::vec4  aColor{};
	glm::vec3  aPos{};
	glm::vec3  aAng{};
	float      aInnerFov = 45.f;
	float      aOuterFov = 45.f;
	float      aRadius   = 0.f;
	float      aLength   = 0.f;
	bool       aShadow   = true;
	bool       aEnabled  = true;
};


struct ShaderBinding_t
{
	ChHandle_t aDescSet;  // useless?
	int        aBinding;
	ChHandle_t aShaderData;  // get slot from this?
};


struct ShaderDescriptor_t
{
	// these sets should be associated with the area this data is used
	// like when doing a shader bind, the specifc sets we want to bind should be there
	// don't store all of them here
	// what we did before was store 2 sets, which was a set for each swapchain image
	ChHandle_t* apSets = nullptr;
	u32         aCount = 0;
};


struct ShaderRequirement_t
{
	std::string_view     aShader;
	CreateDescBinding_t* apBindings    = nullptr;
	u32                  aBindingCount = 0;
};


struct ShaderRequirmentsList_t
{
	// vector of shader bindings
	std::vector< ShaderRequirement_t > aItems;
};


struct RenderFrameData_t
{

};


struct ViewportShader_t
{
	glm::mat4  aProjView{};
	glm::mat4  aProjection{};
	glm::mat4  aView{};
	glm::vec3  aViewPos{};
	float      aNearZ = 0.f;
	float      aFarZ  = 0.f;

	glm::uvec2 aSize{};
	bool       aActive = true;
	bool       aAllocated = false;

	// HACK: if this is set, it overrides the shader used for all renderables in this view
	ChHandle_t aShaderOverride = CH_INVALID_HANDLE;
};


struct ShadowMap_t
{
	ChHandle_t aTexture     = CH_INVALID_HANDLE;
	ChHandle_t aFramebuffer = CH_INVALID_HANDLE;
	glm::ivec2 aSize{};
	u32        aViewInfoIndex = UINT32_MAX;
};


// Push Constant Function Pointers
using FShader_ResetPushData = void();
using FShader_SetupPushData = void( u32 sSurfaceIndex, u32 sViewportIndex, Renderable_t* spDrawData, SurfaceDraw_t& srDrawInfo );
using FShader_PushConstants = void( ChHandle_t cmd, ChHandle_t sLayout, SurfaceDraw_t& srDrawInfo );

// blech
using FShader_SetupPushDataComp = void( ChHandle_t srRenderableHandle, Renderable_t* spDrawData );
using FShader_PushConstantsComp = void( ChHandle_t cmd, ChHandle_t sLayout, ChHandle_t srRenderableHandle, Renderable_t* spDrawData );

using FShader_Init = bool();
using FShader_Destroy = void();

using FShader_GetPipelineLayoutCreate   = void( PipelineLayoutCreate_t& srPipeline );
using FShader_GetGraphicsPipelineCreate = void( GraphicsPipelineCreate_t& srCreate );
using FShader_GetComputePipelineCreate = void( ComputePipelineCreate_t& srCreate );

using FShader_DescriptorData = void();

// blech
using FShader_MaterialData = u32( u32 sRenderableIndex, Renderable_t* spDrawData, SurfaceDraw_t& srDrawInfo );

struct IShaderPush
{
	FShader_ResetPushData* apReset = nullptr;
	FShader_SetupPushData* apSetup = nullptr;
	FShader_PushConstants* apPush  = nullptr;
};


// awful hacky push interface for compute shaders, need to rethink it
struct IShaderPushComp
{
	FShader_ResetPushData*     apReset = nullptr;
	FShader_SetupPushDataComp* apSetup = nullptr;
	FShader_PushConstantsComp* apPush  = nullptr;
};


struct ShaderCreate_t
{
	const char*                        apName           = nullptr;
	ShaderStage                        aStages          = ShaderStage_None;
	EPipelineBindPoint                 aBindPoint       = EPipelineBindPoint_Graphics;
	EShaderFlags                       aFlags           = EShaderFlags_None;
	EDynamicState                      aDynamicState    = EDynamicState_None;
	VertexFormat                       aVertexFormat    = VertexFormat_None;
	ERenderPass                        aRenderPass      = ERenderPass_Graphics;

	FShader_Init*                      apInit           = nullptr;
	FShader_Destroy*                   apDestroy        = nullptr;

	FShader_GetPipelineLayoutCreate*   apLayoutCreate   = nullptr;
	FShader_GetGraphicsPipelineCreate* apGraphicsCreate = nullptr;
	FShader_GetComputePipelineCreate*  apComputeCreate  = nullptr;

	IShaderPush*                       apShaderPush     = nullptr;
	IShaderPushComp*                   apShaderPushComp = nullptr;
	FShader_MaterialData*              apMaterialData   = nullptr;

	CreateDescBinding_t*               apBindings       = nullptr;
	u32                                aBindingCount    = 0;
};


// stored data internally
struct ShaderData_t
{
	ShaderStage           aStages         = ShaderStage_None;
	EShaderFlags          aFlags          = EShaderFlags_None;
	EDynamicState         aDynamicState   = EDynamicState_None;
	ChHandle_t            aLayout         = CH_INVALID_HANDLE;
	IShaderPush*          apPush          = nullptr;
	IShaderPushComp*      apPushComp      = nullptr;
	FShader_MaterialData* apMaterialIndex = nullptr;
};


struct ShaderSets_t
{
	ChVector< ChHandle_t > aSets;
};


std::vector< ShaderCreate_t* >& Shader_GetCreateList();


struct ShaderStaticRegistration
{
	ShaderStaticRegistration( ShaderCreate_t& srCreate )
	{
		Shader_GetCreateList().push_back( &srCreate );
	}
};


#define CH_REGISTER_SHADER( srShaderCreate ) static ShaderStaticRegistration __gRegister__##srShaderCreate( srShaderCreate );


// ---------------------------------------------------------------------------------------
// Models

Handle             Graphics_LoadModel( const std::string& srPath );
Handle             Graphics_CreateModel( Model** spModel );
void               Graphics_FreeModel( Handle hModel );
Model*             Graphics_GetModelData( Handle hModel );
std::string_view   Graphics_GetModelPath( Handle sModel );
ModelBBox_t        Graphics_CalcModelBBox( Handle sModel );
bool               Graphics_GetModelBBox( Handle sModel, ModelBBox_t& srBBox );

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
// Textures

ChHandle_t         Graphics_LoadTexture( ChHandle_t& srHandle, const std::string& srTexturePath, const TextureCreateData_t& srCreateData );

ChHandle_t         Graphics_CreateTexture( const TextureCreateInfo_t& srTextureCreateInfo, const TextureCreateData_t& srCreateData );

void               Graphics_FreeTexture( ChHandle_t shTexture );

// ---------------------------------------------------------------------------------------
// Materials

// Load a cmt file from disk, increments ref count
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

// Get the total amount of materials created
size_t             Graphics_GetMaterialCount();

// Get the path to the material
const std::string& Graphics_GetMaterialPath( Handle sMaterial );

// Tell all materials to rebuild
void               Graphics_SetAllMaterialsDirty();

// Modifying Material Data
const char*        Mat_GetName( Handle mat );
size_t             Mat_GetVarCount( Handle mat );
EMatVar            Mat_GetVarType( Handle mat, size_t sIndex );

Handle             Mat_GetShader( Handle mat );
void               Mat_SetShader( Handle mat, Handle shShader );

VertexFormat       Mat_GetVertexFormat( Handle mat );

// Increments Reference Count for material
void               Mat_AddRef( ChHandle_t sMat );

// Decrements Reference Count for material - returns true if the material is deleted
bool               Mat_RemoveRef( ChHandle_t sMat );

void               Mat_SetVar( Handle mat, const std::string& name, Handle texture );
void               Mat_SetVar( Handle mat, const std::string& name, float data );
void               Mat_SetVar( Handle mat, const std::string& name, int data );
void               Mat_SetVar( Handle mat, const std::string& name, bool data );
void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec2& data );
void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec3& data );
void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec4& data );

int                Mat_GetTextureIndex( Handle mat, std::string_view name, Handle fallback = InvalidHandle );
Handle             Mat_GetTexture( Handle mat, std::string_view name, Handle fallback = InvalidHandle );
float              Mat_GetFloat( Handle mat, std::string_view name, float fallback = 0.f );
int                Mat_GetInt( Handle mat, std::string_view name, int fallback = 0 );
bool               Mat_GetBool( Handle mat, std::string_view name, bool fallback = false );
const glm::vec2&   Mat_GetVec2( Handle mat, std::string_view name, const glm::vec2& fallback = {} );
const glm::vec3&   Mat_GetVec3( Handle mat, std::string_view name, const glm::vec3& fallback = {} );
const glm::vec4&   Mat_GetVec4( Handle mat, std::string_view name, const glm::vec4& fallback = {} );

// ---------------------------------------------------------------------------------------
// Shaders

bool               Shader_ParseRequirements( ShaderRequirmentsList_t& srOutput );
bool               Graphics_ShaderInit( bool sRecreate );
Handle             Graphics_GetShader( std::string_view sName );
const char*        Graphics_GetShaderName( Handle sShader );

bool               Shader_Bind( Handle sCmd, u32 sIndex, Handle sShader );
void               Shader_ResetPushData();
bool               Shader_SetupRenderableDrawData( u32 sRenderableIndex, u32 sViewportIndex, Renderable_t* spModelDraw, ShaderData_t* spShaderData, SurfaceDraw_t& srRenderable );
bool               Shader_PreRenderableDraw( Handle sCmd, u32 sIndex, Handle sShader, SurfaceDraw_t& srRenderable );

VertexFormat       Shader_GetVertexFormat( Handle sShader );
ShaderData_t*      Shader_GetData( Handle sShader );
ShaderSets_t*      Shader_GetSets( Handle sShader );
ShaderSets_t*      Shader_GetSets( std::string_view sShader );

Handle             Shader_RegisterDescriptorData( EShaderSlot sSlot, FShader_DescriptorData* sCallback );

// Used to know if this material needs to be ordered and drawn after all opaque ones are drawn
// bool               Shader_IsMaterialTransparent( Handle sMat );

// Used to get all material vars the shader can use, and throw warnings on unknown material vars
// void               Shader_GetMaterialVars( Handle sShader );

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

// ChHandle_t         Graphics_CreateRenderPass();
// void               Graphics_UpdateRenderPass( ChHandle_t sRenderPass );

// Returns the Viewport Index
u32                Graphics_CreateViewport( ViewportShader_t* spViewport = nullptr );
void               Graphics_FreeViewport( u32 sViewportIndex );

ViewportShader_t*  Graphics_GetViewportData( u32 sViewportIndex );
void               Graphics_SetViewportUpdate( bool sUpdate );

// void               Graphics_PushViewInfo( const ViewportShader_t& srViewInfo );
// void               Graphics_PopViewInfo();
// ViewportShader_t&  Graphics_GetViewInfo();

ChHandle_t         Graphics_CreateRenderable( ChHandle_t sModel );
Renderable_t*      Graphics_GetRenderableData( ChHandle_t sRenderable );
void               Graphics_FreeRenderable( ChHandle_t sRenderable );
void               Graphics_UpdateRenderableAABB( ChHandle_t sRenderable );
void               Graphics_ConsolidateRenderables();

ModelBBox_t        Graphics_CreateWorldAABB( glm::mat4& srMatrix, const ModelBBox_t& srBBox );

// ---------------------------------------------------------------------------------------
// Debug Rendering

void               Graphics_DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor );
void               Graphics_DrawAxis( const glm::vec3& sPos, const glm::vec3& sAng, const glm::vec3& sScale );
void               Graphics_DrawBBox( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor );
void               Graphics_DrawProjView( const glm::mat4& srProjView );
void               Graphics_DrawFrustum( const Frustum_t& srFrustum );
void               Graphics_DrawNormals( ChHandle_t sModel, const glm::mat4& srMatrix );

// ---------------------------------------------------------------------------------------
// Vertex Format/Attributes

GraphicsFmt        Graphics_GetVertexAttributeFormat( VertexAttribute attrib );
size_t             Graphics_GetVertexAttributeTypeSize( VertexAttribute attrib );
size_t             Graphics_GetVertexAttributeSize( VertexAttribute attrib );
size_t             Graphics_GetVertexFormatSize( VertexFormat format );

void               Graphics_GetVertexBindingDesc( VertexFormat format, std::vector< VertexInputBinding_t >& srAttrib );
void               Graphics_GetVertexAttributeDesc( VertexFormat format, std::vector< VertexInputAttribute_t >& srAttrib );

