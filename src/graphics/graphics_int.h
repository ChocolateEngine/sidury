#pragma once

#include <unordered_set>
#include "igraphics.h"


LOG_CHANNEL2( ClientGraphics )


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
	UBO_LightDirectional_t aLightWorld[ CH_R_MAX_LIGHT_TYPE ];
	UBO_LightPoint_t       aLightPoint[ CH_R_MAX_LIGHT_TYPE ];
	UBO_LightCone_t        aLightCone[ CH_R_MAX_LIGHT_TYPE ];
	UBO_LightCapsule_t     aLightCapsule[ CH_R_MAX_LIGHT_TYPE ];
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

bool                  Graphics_ShaderInit( bool sRecreate );

bool                  Shader_Bind( Handle sCmd, u32 sIndex, Handle sShader );
void                  Shader_ResetPushData();
bool                  Shader_SetupRenderableDrawData( u32 sRenderableIndex, u32 sViewportIndex, Renderable_t* spModelDraw, ShaderData_t* spShaderData, SurfaceDraw_t& srRenderable );
bool                  Shader_PreRenderableDraw( Handle sCmd, u32 sIndex, ShaderData_t* spShaderData, SurfaceDraw_t& srRenderable );
bool                  Shader_ParseRequirements( ShaderRequirmentsList_t& srOutput );

VertexFormat          Shader_GetVertexFormat( Handle sShader );
ShaderData_t*         Shader_GetData( Handle sShader );
ShaderSets_t*         Shader_GetSets( Handle sShader );
ShaderSets_t*         Shader_GetSets( std::string_view sShader );

Handle                Shader_RegisterDescriptorData( EShaderSlot sSlot, FShader_DescriptorData* sCallback );


// interface

class Graphics : public IGraphics
{
   public:
	virtual void               Update( float sDT ){};

	// ---------------------------------------------------------------------------------------
	// Models

	virtual Handle             LoadModel( const std::string& srPath )                                                                                                                         override;
	virtual Handle             CreateModel( Model** spModel )                                                                                                                                 override;
	virtual void               FreeModel( Handle hModel )                                                                                                                                     override;
	virtual Model*             GetModelData( Handle hModel )                                                                                                                                  override;
	virtual std::string_view   GetModelPath( Handle sModel )                                                                                                                                  override;
	virtual ModelBBox_t        CalcModelBBox( Handle sModel )                                                                                                                                 override;
	virtual bool               GetModelBBox( Handle sModel, ModelBBox_t& srBBox )                                                                                                             override;

	virtual void               Model_SetMaterial( Handle shModel, size_t sSurface, Handle shMat )                                                                                             override;
	virtual Handle             Model_GetMaterial( Handle shModel, size_t sSurface )                                                                                                           override;

	// ---------------------------------------------------------------------------------------
	// Scenes

	virtual Handle             LoadScene( const std::string& srPath )                                                                                                                         override;
	virtual void               FreeScene( Handle sScene )                                                                                                                                     override;

	virtual SceneDraw_t*       AddSceneDraw( Handle sScene )                                                                                                                                  override;
	virtual void               RemoveSceneDraw( SceneDraw_t* spScene )                                                                                                                        override;

	virtual size_t             GetSceneModelCount( Handle sScene )                                                                                                                            override;
	virtual Handle             GetSceneModel( Handle sScene, size_t sIndex )                                                                                                                  override;

	// ---------------------------------------------------------------------------------------
	// Render Lists

	// virtual RenderList_t*      Graphics_CreateRenderList() override;
	// virtual void               Graphics_DestroyRenderList( RenderList_t* spList ) override;
	// virtual void               Graphics_DrawRenderList( RenderList_t* spList ) override;

	// ---------------------------------------------------------------------------------------
	// Render Layers

	// virtual RenderLayer_t*     Graphics_CreateRenderLayer() override;
	// virtual void               Graphics_DestroyRenderLayer( RenderLayer_t* spLayer ) override;

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

	virtual ChHandle_t         LoadTexture( ChHandle_t& srHandle, const std::string& srTexturePath, const TextureCreateData_t& srCreateData )                                                 override;

	virtual ChHandle_t         CreateTexture( const TextureCreateInfo_t& srTextureCreateInfo, const TextureCreateData_t& srCreateData )                                                       override;

	virtual void               FreeTexture( ChHandle_t shTexture )                                                                                                                            override;

	// ---------------------------------------------------------------------------------------
	// Materials

	// Load a cmt file from disk, increments ref count
	virtual Handle             LoadMaterial( const std::string& srPath )                                                                                                                      override;

	// Create a new material with a name and a shader
	virtual Handle             CreateMaterial( const std::string& srName, Handle shShader )                                                                                                   override;

	// Free a material
	virtual void               FreeMaterial( Handle sMaterial )                                                                                                                               override;

	// Find a material by name
	// Name is a path to the cmt file if it was loaded on disk
	// EXAMPLE: C:/chocolate/sidury/materials/dev/grid01.cmt
	// NAME: materials/dev/grid01
	virtual Handle             FindMaterial( const char* spName )                                                                                                                             override;

	// Get the total amount of materials created
	virtual size_t             GetMaterialCount()                                                                                                                                             override;

	// Get the path to the material
	virtual const std::string& GetMaterialPath( Handle sMaterial )                                                                                                                            override;

	// Tell all materials to rebuild
	virtual void               SetAllMaterialsDirty()                                                                                                                                         override;

	// Modifying Material Data
	virtual const char*        Mat_GetName( Handle mat )                                                                                                                                      override;
	virtual size_t             Mat_GetVarCount( Handle mat )                                                                                                                                  override;
	virtual EMatVar            Mat_GetVarType( Handle mat, size_t sIndex )                                                                                                                    override;

	virtual Handle             Mat_GetShader( Handle mat )                                                                                                                                    override;
	virtual void               Mat_SetShader( Handle mat, Handle shShader )                                                                                                                   override;

	virtual VertexFormat       Mat_GetVertexFormat( Handle mat )                                                                                                                              override;

	// Increments Reference Count for material
	virtual void               Mat_AddRef( ChHandle_t sMat )                                                                                                                                  override;

	// Decrements Reference Count for material - returns true if the material is deleted
	virtual bool               Mat_RemoveRef( ChHandle_t sMat )                                                                                                                               override;

	virtual void               Mat_SetVar( Handle mat, const std::string& name, Handle texture )                                                                                              override;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, float data )                                                                                                  override;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, int data )                                                                                                    override;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, bool data )                                                                                                   override;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec2& data )                                                                                       override;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec3& data )                                                                                       override;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec4& data )                                                                                       override;

	virtual int                Mat_GetTextureIndex( Handle mat, std::string_view name, Handle fallback = InvalidHandle )                                                                      override;
	virtual Handle             Mat_GetTexture( Handle mat, std::string_view name, Handle fallback = InvalidHandle )                                                                           override;
	virtual float              Mat_GetFloat( Handle mat, std::string_view name, float fallback = 0.f )                                                                                        override;
	virtual int                Mat_GetInt( Handle mat, std::string_view name, int fallback = 0 ) override;
	virtual bool               Mat_GetBool( Handle mat, std::string_view name, bool fallback = false )                                                                                        override;
	virtual const glm::vec2&   Mat_GetVec2( Handle mat, std::string_view name, const glm::vec2& fallback = {} )                                                                               override;
	virtual const glm::vec3&   Mat_GetVec3( Handle mat, std::string_view name, const glm::vec3& fallback = {} )                                                                               override;
	virtual const glm::vec4&   Mat_GetVec4( Handle mat, std::string_view name, const glm::vec4& fallback = {} )                                                                               override;

	// ---------------------------------------------------------------------------------------
	// Shaders

	// virtual bool               Shader_Init( bool sRecreate )                                                                                                                                  override;
	virtual Handle             GetShader( std::string_view sName )                                                                                                                            override;
	virtual const char*        GetShaderName( Handle sShader )                                                                                                                                override;

	// Used to know if this material needs to be ordered and drawn after all opaque ones are drawn
	// virtual bool               Shader_IsMaterialTransparent( Handle sMat ) override;

	// Used to get all material vars the shader can use, and throw warnings on unknown material vars
	// virtual void               Shader_GetMaterialVars( Handle sShader ) override;

	// ---------------------------------------------------------------------------------------
	// Buffers

	virtual void               CreateVertexBuffers( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName = nullptr )                                                 override;
	virtual void               CreateIndexBuffer( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName = nullptr )                                                   override;
	// void               CreateModelBuffers( ModelBuffers_t* spBuffers, VertexData_t* spVertexData, bool sCreateIndex, const char* spDebugName ) override;

	// ---------------------------------------------------------------------------------------
	// Lighting

	virtual Light_t*           CreateLight( ELightType sType )                                                                                                                                override;
	virtual void               UpdateLight( Light_t* spLight )                                                                                                                                override;
	virtual void               DestroyLight( Light_t* spLight )                                                                                                                               override;

	// ---------------------------------------------------------------------------------------
	// Rendering

	virtual bool               Init()                                                                                                                                                         override;
	virtual void               Shutdown()                                                                                                                                                     override;

	virtual void               NewFrame()                                                                                                                                                     override;
	virtual void               Reset()                                                                                                                                                        override;
	virtual void               Present()                                                                                                                                                      override;

	// ChHandle_t         CreateRenderPass() override;
	// virtual void               UpdateRenderPass( ChHandle_t sRenderPass ) override;

	// Returns the Viewport Index - input is the address of a pointer
	virtual u32                CreateViewport( ViewportShader_t** spViewport = nullptr )                                                                                                      override;
	virtual void               FreeViewport( u32 sViewportIndex )                                                                                                                             override;

	virtual ViewportShader_t*  GetViewportData( u32 sViewportIndex )                                                                                                                          override;
	virtual void               SetViewportUpdate( bool sUpdate )                                                                                                                              override;

	// virtual void               PushViewInfo( const ViewportShader_t& srViewInfo ) override;
	// virtual void               PopViewInfo() override;
	// virtual ViewportShader_t&  GetViewInfo() override;

	virtual ChHandle_t         CreateRenderable( ChHandle_t sModel )                                                                                                                          override;
	virtual Renderable_t*      GetRenderableData( ChHandle_t sRenderable )                                                                                                                    override;
	virtual void               SetRenderableModel( ChHandle_t sRenderable, ChHandle_t sModel )                                                                                                override;
	virtual void               FreeRenderable( ChHandle_t sRenderable )                                                                                                                       override;
	virtual void               UpdateRenderableAABB( ChHandle_t sRenderable )                                                                                                                 override;
	virtual ModelBBox_t        GetRenderableAABB( ChHandle_t sRenderable )                                                                                                                    override;

#if DEBUG
	virtual void               SetRenderableDebugName( ChHandle_t sRenderable, std::string_view sName )                                                                                       override;
#endif

	// virtual void               ConsolidateRenderables()                                                                                                                                       override;

	virtual ModelBBox_t        CreateWorldAABB( glm::mat4& srMatrix, const ModelBBox_t& srBBox )                                                                                              override;

	// ---------------------------------------------------------------------------------------
	// Debug Rendering

	virtual void               DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor )                                                                                  override;
	virtual void               DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec4& sColor )                                                                                  override;
	virtual void               DrawAxis( const glm::vec3& sPos, const glm::vec3& sAng, const glm::vec3& sScale )                                                                              override;
	virtual void               DrawAxis( const glm::mat4& sMat, const glm::vec3& sScale )                                                                                                     override;
	virtual void               DrawAxis( const glm::mat4& sMat )                                                                                                                              override;
	virtual void               DrawBBox( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor )                                                                                  override;
	virtual void               DrawProjView( const glm::mat4& srProjView )                                                                                                                    override;
	virtual void               DrawFrustum( const Frustum_t& srFrustum )                                                                                                                      override;
	virtual void               DrawNormals( ChHandle_t sModel, const glm::mat4& srMatrix )                                                                                                    override;

	// ---------------------------------------------------------------------------------------
	// Vertex Format/Attributes

	virtual GraphicsFmt        GetVertexAttributeFormat( VertexAttribute attrib )                                                                                                             override;
	virtual size_t             GetVertexAttributeTypeSize( VertexAttribute attrib )                                                                                                           override;
	virtual size_t             GetVertexAttributeSize( VertexAttribute attrib )                                                                                                               override;
	virtual size_t             GetVertexFormatSize( VertexFormat format )                                                                                                                     override;

	virtual void               GetVertexBindingDesc( VertexFormat format, std::vector< VertexInputBinding_t >& srAttrib )                                                                     override;
	virtual void               GetVertexAttributeDesc( VertexFormat format, std::vector< VertexInputAttribute_t >& srAttrib )                                                                 override;
};


extern Graphics gGraphics;

