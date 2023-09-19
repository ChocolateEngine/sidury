#pragma once

#include <unordered_set>


// KEEP IN SYNC WITH core.glsl
constexpr u32 CH_R_MAX_TEXTURES                   = 4096;
constexpr u32 CH_R_MAX_VIEWPORTS                  = 32;
constexpr u32 CH_R_MAX_RENDERABLES                = 4096;
constexpr u32 CH_R_MAX_MATERIALS                  = 64;  // max materials per renderable

constexpr u32 CH_R_MAX_VERTEX_BUFFERS             = CH_R_MAX_RENDERABLES * 2;
constexpr u32 CH_R_MAX_INDEX_BUFFERS              = CH_R_MAX_RENDERABLES;
constexpr u32 CH_R_MAX_BLEND_SHAPE_WEIGHT_BUFFERS = CH_R_MAX_RENDERABLES;
constexpr u32 CH_R_MAX_BLEND_SHAPE_DATA_BUFFERS   = CH_R_MAX_RENDERABLES;

constexpr u32 CH_R_MAX_LIGHT_TYPE                 = 256;
constexpr u32 CH_R_MAX_LIGHTS                     = CH_R_MAX_LIGHT_TYPE * ELightType_Count;

constexpr u32 CH_R_LIGHT_LIST_SIZE                = 16;

constexpr u32 CH_BINDING_TEXTURES                 = 0;
constexpr u32 CH_BINDING_CORE                     = 1;
constexpr u32 CH_BINDING_VIEWPORTS                = 2;
constexpr u32 CH_BINDING_RENDERABLES              = 3;
constexpr u32 CH_BINDING_MODEL_MATRICES           = 4;
constexpr u32 CH_BINDING_VERTEX_BUFFERS           = 5;
constexpr u32 CH_BINDING_INDEX_BUFFERS            = 6;


struct ViewRenderList_t
{
	// TODO: needs improvement and further sorting
	// [ Shader ] = vector of surfaces to draw
	std::unordered_map< ChHandle_t, ChVector< SurfaceDraw_t > > aRenderLists;
};


constexpr glm::vec4 gFrustumFaceData[ 8u ] = {
	// Near Face
	{ 1, 1, -1, 1.f },
	{ -1, 1, -1, 1.f },
	{ 1, -1, -1, 1.f },
	{ -1, -1, -1, 1.f },

	// Far Face
	{ 1, 1, 1, 1.f },
	{ -1, 1, 1, 1.f },
	{ 1, -1, 1, 1.f },
	{ -1, -1, 1, 1.f },
};


struct ShaderDescriptorData_t
{
	// Descriptor Set Layouts for the Shader Slots
	// ChHandle_t                                           aLayouts[ EShaderSlot_Count ];
	ChHandle_t                                                 aGlobalLayout;

	// Descriptor Sets Used by all shaders
	ShaderDescriptor_t                                         aGlobalSets;

	// Sets per render pass
	// ShaderDescriptor_t                                   aPerPassSets[ ERenderPass_Count ];

	// Shader Descriptor Set Layout, [shader name] = layout
	std::unordered_map< std::string_view, ChHandle_t >         aPerShaderLayout;

	// Sets per shader, [shader handle] = descriptor sets
	std::unordered_map< std::string_view, ShaderDescriptor_t > aPerShaderSets;

	// Sets per renderable, [renderable handle] = descriptor sets
	// std::unordered_map< ChHandle_t, ShaderDescriptor_t > aPerObjectSets;
};


extern ShaderDescriptorData_t gShaderDescriptorData;


// Match core.glsl!
struct Shader_Viewport_t
{
	glm::mat4 aProjView{};
	glm::mat4 aProjection{};
	glm::mat4 aView{};
	glm::vec3 aViewPos{};
	float     aNearZ = 0.f;
	float     aFarZ  = 0.f;
};


// shared renderable data
struct Shader_Renderable_t
{
	// alignas( 16 ) u32 aMaterialCount;
	// u32 aMaterials[ CH_R_MAX_MATERIALS ];
	
	u32 aVertexBuffer;
	u32 aIndexBuffer;

	u32 aLightCount;
	// u32 aLightList[ CH_R_LIGHT_LIST_SIZE ];
};


// surface index and renderable index
// unique to each viewport
// basically, "Draw the current renderable surface with this material"
struct Shader_SurfaceDraw_t
{
	u32 aRenderable;  // index into gCore.aRenderables
	u32 aMaterial;    // index into the shader material array
};


// Light Types
struct UBO_LightDirectional_t
{
	alignas( 16 ) glm::vec4 aColor{};
	alignas( 16 ) glm::vec3 aDir{};
	alignas( 16 ) glm::mat4 aProjView{};
	int aShadow = -1;
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
	alignas( 16 ) glm::mat4 aProjView{};
	int aShadow = -1;
};


struct UBO_LightCapsule_t
{
	alignas( 16 ) glm::vec4 aColor{};
	alignas( 16 ) glm::vec3 aPos{};
	alignas( 16 ) glm::vec3 aDir{};
	float aLength    = 0.f;
	float aThickness = 0.f;
};


struct Buffer_Core_t
{
	u32                    aNumTextures      = 0;
	u32                    aNumViewports     = 0;
	u32                    aNumVertexBuffers = 0;
	u32                    aNumIndexBuffers  = 0;
	
	u32                    aNumLights[ ELightType_Count ];

	// NOTE: this could probably be brought down to one light struct, with packed data to unpack in the shader
	//UBO_LightDirectional_t aLightWorld[ CH_R_MAX_LIGHT_TYPE ];
	//UBO_LightPoint_t       aLightPoint[ CH_R_MAX_LIGHT_TYPE ];
	//UBO_LightCone_t        aLightCone[ CH_R_MAX_LIGHT_TYPE ];
	//UBO_LightCapsule_t     aLightCapsule[ CH_R_MAX_LIGHT_TYPE ];
};


struct DeviceBufferStaging_t
{
	ChHandle_t aStagingBuffer = CH_INVALID_HANDLE;
	ChHandle_t aBuffer        = CH_INVALID_HANDLE;
	bool       aDirty         = true;
};


struct ModelAABBUpdate_t
{
	Renderable_t* apModelDraw;
	ModelBBox_t   aBBox;
};


struct GraphicsViewData_t
{
	// std::vector< ChHandle_t >       aBuffers;
	std::vector< ViewportShader_t > aViewports;  // Get rid of thus and only use the one in core data for the gpu
	std::vector< Frustum_t >        aFrustums;
	//std::stack< ViewportShader_t >  aStack;
	//bool                            aUpdate = false;
};


struct ShaderArrayAllocator_t
{
	u32  aAllocated = 0;
	u32  aUsed      = 0;
	u32* apFree     = nullptr;
	bool aDirty     = false;
};


struct ShaderBufferList_t
{
	std::unordered_map< u32, ChHandle_t > aBuffers;
	bool                                  aDirty = false;  // ew
};


enum EShaderCoreArray : u32
{
	EShaderCoreArray_LightWorld   = 1,
	EShaderCoreArray_LightPoint   = 2,
	EShaderCoreArray_LightCone    = 3,
	EShaderCoreArray_LightCapsule = 4,

	EShaderCoreArray_Count,
};


// :skull:
struct ShaderSkinning_Push
{
	u32 aRenderable            = 0;
	u32 aSourceVertexBuffer    = 0;
	u32 aVertexCount           = 0;
	u32 aBlendShapeCount       = 0;
	u32 aBlendShapeWeightIndex = 0;
	u32 aBlendShapeDataIndex   = 0;
};


constexpr u32 CH_SHADER_CORE_SLOT_INVALID = UINT32_MAX;


// I don't like this at all
struct GraphicsData_t
{
	ResourceList< Renderable_t >                  aRenderables;
	std::unordered_map< ChHandle_t, ModelBBox_t > aRenderAABBUpdate;

	std::vector< ViewRenderList_t >               aViewRenderLists;

	// Renderables that need skinning applied to them
	//ChVector< ChHandle_t >                        aSkinningRenderList;
	std::unordered_set< ChHandle_t >              aSkinningRenderList;

	ChHandle_t*                                   aCommandBuffers;
	u32                                           aCommandBufferCount;

	ChHandle_t                                    aRenderPassGraphics;
	ChHandle_t                                    aRenderPassShadow;

	// stores backbuffer color and depth
	ChHandle_t                                    aBackBuffer[ 2 ];
	ChHandle_t                                    aBackBufferTex[ 3 ];

	std::unordered_set< ChHandle_t >              aDirtyMaterials;

	// --------------------------------------------------------------------------------------
	// Descriptor Sets

	ShaderDescriptorData_t                        aShaderDescriptorData;
	// ResourceList< RenderPassData_t >              aRenderPassData;

	// --------------------------------------------------------------------------------------
	// Textures

	// std::vector< ChHandle_t >                     aTextureBuffers;
	// bool                                          aTexturesDirty = true;

	// --------------------------------------------------------------------------------------
	// Viewports

	GraphicsViewData_t                            aViewData;

	// --------------------------------------------------------------------------------------
	// Assets

	ResourceList< Model >                         aModels;
	std::unordered_map< std::string, ChHandle_t > aModelPaths;
	std::unordered_map< ChHandle_t, ModelBBox_t > aModelBBox;

	std::unordered_set< ChHandle_t >              aModelsToFree;

	ResourceList< Scene_t >                       aScenes;
	std::unordered_map< std::string, ChHandle_t > aScenePaths;

	// --------------------------------------------------------------------------------------
	// Shader Buffers

	Buffer_Core_t                                 aCoreData;
	DeviceBufferStaging_t                         aCoreDataStaging;

	// Free Indexes
	ShaderArrayAllocator_t                        aCoreDataSlots[ EShaderCoreArray_Count ];

	ShaderBufferList_t                            aVertexBuffers;
	ShaderBufferList_t                            aIndexBuffers;
	ShaderBufferList_t                            aBlendShapeWeightBuffers;
	ShaderBufferList_t                            aBlendShapeDataBuffers;
	
	glm::mat4*                                    aModelMatrixData;  // shares the same slot index as renderables, which is the handle index
	Shader_Renderable_t*                          aRenderableData;

	DeviceBufferStaging_t                         aModelMatrixStaging;
	DeviceBufferStaging_t                         aRenderableStaging;

	Shader_Viewport_t*                            aViewportData;
	ShaderArrayAllocator_t                        aViewportSlots;
	DeviceBufferStaging_t                         aViewportStaging;
};


constexpr const char* gVertexBufferStr      = "Vertex Buffer";
constexpr const char* gIndexBufferStr       = "Vertex Buffer";
constexpr const char* gBlendShapeWeightsStr = "Blend Shape Weights Buffer";


extern GraphicsData_t gGraphicsData;


bool                  Graphics_CreateVariableUniformLayout( ShaderDescriptor_t& srBuffer, const char* spLayoutName, const char* spSetName, int sCount );

void                  Graphics_DrawShaderRenderables( ChHandle_t cmd, size_t sIndex, ChHandle_t shader, size_t sViewIndex, ChVector< SurfaceDraw_t >& srRenderList );

void                  Graphics_CreateFrustum( Frustum_t& srFrustum, const glm::mat4& srViewMat );
Frustum_t             Graphics_CreateFrustum( const glm::mat4& srViewInfo );

u32                   Graphics_AllocateShaderSlot( ShaderArrayAllocator_t& srAllocator, const char* spDebugName );
void                  Graphics_FreeShaderSlot( ShaderArrayAllocator_t& srAllocator, const char* spDebugName, u32 sIndex );

u32                   Graphics_AllocateCoreSlot( EShaderCoreArray sSlot );
void                  Graphics_FreeCoreSlot( EShaderCoreArray sSlot, u32 sIndex );

// return a magic number
u32                   Graphics_AddShaderBuffer( ShaderBufferList_t& srBufferList, ChHandle_t sBuffer );
void                  Graphics_RemoveShaderBuffer( ShaderBufferList_t& srBufferList, u32 sHandle );
ChHandle_t            Graphics_GetShaderBuffer( const ShaderBufferList_t& srBufferList, u32 sHandle );
u32                   Graphics_GetShaderBufferIndex( const ShaderBufferList_t& srBufferList, u32 sHandle );

void                  Graphics_FreeQueuedResources();
