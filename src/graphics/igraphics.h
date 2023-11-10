#pragma once

#include "core/core.h"
#include "render/irender.h"

// ======================================================
// User Land abstraction of renderer
// ======================================================

extern ChHandle_t gResource_Model;

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
	VertexAttribute_Color,     // vec4

	// this and morphs will be calculated in a compute shader
	// VertexAttribute_BoneIndex,        // uvec4
	// VertexAttribute_BoneWeight,       // vec4

	VertexAttribute_Count
};


// Flags to Determine what the Vertex Data contains
using VertexFormat = u16;
enum : VertexFormat
{
	VertexFormat_None     = 0,
	VertexFormat_Position = ( 1 << VertexAttribute_Position ),
	VertexFormat_Normal   = ( 1 << VertexAttribute_Normal ),
	VertexFormat_TexCoord = ( 1 << VertexAttribute_TexCoord ),
	VertexFormat_Color    = ( 1 << VertexAttribute_Color ),

	VertexFormat_All      = ( 1 << VertexAttribute_Count ) - 1,
	// VertexFormat_All      = VertexFormat_Position | VertexFormat_Normal | VertexFormat_TexCoord | VertexFormat_Color,
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
// except this has fov
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

	// https://iquilezles.org/articles/frustumcorrect/
	bool      IsBoxVisible( const glm::vec3& sMin, const glm::vec3& sMax ) const
	{
		PROF_SCOPE();

		// Check Box Outside/Inside of Frustum
		for ( int i = 0; i < EFrustum_Count; i++ )
		{
			if ( ( glm::dot( aPlanes[ i ], glm::vec4( sMin.x, sMin.y, sMin.z, 1.0f ) ) < 0.0 ) &&
			     ( glm::dot( aPlanes[ i ], glm::vec4( sMax.x, sMin.y, sMin.z, 1.0f ) ) < 0.0 ) &&
			     ( glm::dot( aPlanes[ i ], glm::vec4( sMin.x, sMax.y, sMin.z, 1.0f ) ) < 0.0 ) &&
			     ( glm::dot( aPlanes[ i ], glm::vec4( sMax.x, sMax.y, sMin.z, 1.0f ) ) < 0.0 ) &&
			     ( glm::dot( aPlanes[ i ], glm::vec4( sMin.x, sMin.y, sMax.z, 1.0f ) ) < 0.0 ) &&
			     ( glm::dot( aPlanes[ i ], glm::vec4( sMax.x, sMin.y, sMax.z, 1.0f ) ) < 0.0 ) &&
			     ( glm::dot( aPlanes[ i ], glm::vec4( sMin.x, sMax.y, sMax.z, 1.0f ) ) < 0.0 ) &&
			     ( glm::dot( aPlanes[ i ], glm::vec4( sMax.x, sMax.y, sMax.z, 1.0f ) ) < 0.0 ) )
			{
				return false;
			}
		}

		// Check Frustum Outside/Inside Box
		for ( int j = 0; j < 3; j++ )
		{
			int out = 0;
			for ( int i = 0; i < 8; i++ )
			{
				if ( aPoints[ i ][ j ] > sMax[ j ] )
					out++;
			}

			if ( out == 8 )
				return false;

			out = 0;
			for ( int i = 0; i < 8; i++ )
			{
				if ( aPoints[ i ][ j ] < sMin[ j ] )
					out++;
			}

			if ( out == 8 )
				return false;
		}

		return true;
	}
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


struct Shader_VertexData_t
{
	glm::vec4 aPosNormX;
	glm::vec4 aNormYZ_UV;
	glm::vec4 aColor;
};


// Each Bone Transform is local to the parent bone
// We don't store this as a calculated matrix
// as we just calculate it on the gpu once per skeleton update
struct BoneTransform_t
{
	u32       aParent;
	glm::vec3 aPos;
	glm::vec3 aAng;
	glm::vec3 aScale;
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
	VertexFormat                 aFormat = VertexFormat_None;
	u32                          aCount  = 0;
	ChVector< VertAttribData_t > aData;
	ChVector< uint32_t >         aIndices;

	// ChVector< ChVector< VertAttribData_t > > aBlendShapeData;
	u32                          aBlendShapeCount = 0;
	Shader_VertexData_t*         apBlendShapeData;  // size of data is (blend shape count) * (vertex count)
};


struct ModelBuffers_t
{
	ChHandle_t aVertex           = CH_INVALID_HANDLE;
	ChHandle_t aIndex            = CH_INVALID_HANDLE;

	// Contains Blend Shape Information
	ChHandle_t aBlendShape       = CH_INVALID_HANDLE;

	// Contains Bones and Bone Weights
	ChHandle_t aSkin             = CH_INVALID_HANDLE;

	u32        aVertexHandle     = UINT32_MAX;
	u32        aIndexHandle      = UINT32_MAX;
	u32        aBlendShapeHandle = UINT32_MAX;

	~ModelBuffers_t()
	{
		if ( aVertex )
			render->DestroyBuffer( aVertex );

		if ( aIndex )
			render->DestroyBuffer( aIndex );

		if ( aBlendShape )
			render->DestroyBuffer( aBlendShape );

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
	ChHandle_t                  aModel;
	glm::mat4                   aModelMatrix;

	// TODO: store the materials for each mesh here so you can override them for this renderable
	// all material handles are 0 by default, which means no custom materials are being used
	// also will start with 1 material by default for async loading, and then will expand upon model loading finish
	// or
	// u32         aMeshCount;
	// ChHandle_t* apMaterials;

	// used for blend shapes and skeleton data, i don't like this here because very few models will have blend shapes/skeletons
	ChVector< float >           aBlendShapeWeights;
	ChVector< BoneTransform_t > aBoneTransforms;

	// -------------------------------------------------
	// Internal Rendering Parts

	// used for calculating render lists
	ModelBBox_t                 aAABB;

	// technically we could have this here for skinning results if needed
	// I don't like this here as only very few models will use skinning
	ChHandle_t                  aVertexBuffer;
	ChHandle_t                  aBlendShapeWeightsBuffer;
	ChHandle_t                  aBoneTransformBuffer;

	u32                         aVertexIndex            = UINT32_MAX;
	u32                         aIndexHandle            = UINT32_MAX;
	// u32               aBlendShapeWeightsMagic = UINT32_MAX;
	u32                         aBlendShapeWeightsIndex = UINT32_MAX;
	u32                         aBoneTransformHandle    = UINT32_MAX;

	// -------------------------------------------------
	// User Defined Bools

	// I don't like these bools that have to be checked every frame
	bool                        aTestVis;
	bool                        aCastShadow;
	bool                        aVisible;
	bool                        aBlendShapesDirty;  // AAAAAAA
	bool                        aBonesDirty;        // AAAAAAA
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
	u32*       apSurfaces;
	u32        aSurfaceCount;
	u32        aShaderSlot;  // index into core data shader draw
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
	bool       aActive         = true;
	bool       aAllocated      = false;

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
using FShader_ResetPushData             = void();
using FShader_SetupPushData             = void( u32 sSurfaceIndex, u32 sViewportIndex, Renderable_t* spDrawData, SurfaceDraw_t& srDrawInfo );
using FShader_PushConstants             = void( ChHandle_t cmd, ChHandle_t sLayout, SurfaceDraw_t& srDrawInfo );

// blech
using FShader_SetupPushDataComp         = void( ChHandle_t srRenderableHandle, Renderable_t* spDrawData );
using FShader_PushConstantsComp         = void( ChHandle_t cmd, ChHandle_t sLayout, ChHandle_t srRenderableHandle, Renderable_t* spDrawData );

using FShader_Init                      = bool();
using FShader_Destroy                   = void();

using FShader_GetPipelineLayoutCreate   = void( PipelineLayoutCreate_t& srPipeline );
using FShader_GetGraphicsPipelineCreate = void( GraphicsPipelineCreate_t& srCreate );
using FShader_GetComputePipelineCreate  = void( ComputePipelineCreate_t& srCreate );

using FShader_DescriptorData            = void();

// blech
using FShader_MaterialData              = u32( u32 sRenderableIndex, Renderable_t* spDrawData, SurfaceDraw_t& srDrawInfo );

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


class IGraphics : public ISystem
{
   public:
	// ---------------------------------------------------------------------------------------
	// Models

	virtual Handle             LoadModel( const std::string& srPath )                                                                                                                         = 0;
	virtual Handle             CreateModel( Model** spModel )                                                                                                                                 = 0;
	virtual void               FreeModel( Handle hModel )                                                                                                                                     = 0;
	virtual Model*             GetModelData( Handle hModel )                                                                                                                                  = 0;
	virtual std::string_view   GetModelPath( Handle sModel )                                                                                                                                  = 0;
	virtual ModelBBox_t        CalcModelBBox( Handle sModel )                                                                                                                                 = 0;
	virtual bool               GetModelBBox( Handle sModel, ModelBBox_t& srBBox )                                                                                                             = 0;

	virtual void               Model_SetMaterial( Handle shModel, size_t sSurface, Handle shMat )                                                                                             = 0;
	virtual Handle             Model_GetMaterial( Handle shModel, size_t sSurface )                                                                                                           = 0;

	// ---------------------------------------------------------------------------------------
	// Scenes

	virtual Handle             LoadScene( const std::string& srPath )                                                                                                                         = 0;
	virtual void               FreeScene( Handle sScene )                                                                                                                                     = 0;

	virtual SceneDraw_t*       AddSceneDraw( Handle sScene )                                                                                                                                  = 0;
	virtual void               RemoveSceneDraw( SceneDraw_t* spScene )                                                                                                                        = 0;

	virtual size_t             GetSceneModelCount( Handle sScene )                                                                                                                            = 0;
	virtual Handle             GetSceneModel( Handle sScene, size_t sIndex )                                                                                                                  = 0;

	// ---------------------------------------------------------------------------------------
	// Render Lists

	// virtual RenderList_t*      Graphics_CreateRenderList() = 0;
	// virtual void               Graphics_DestroyRenderList( RenderList_t* spList ) = 0;
	// virtual void               Graphics_DrawRenderList( RenderList_t* spList ) = 0;

	// ---------------------------------------------------------------------------------------
	// Render Layers

	// virtual RenderLayer_t*     Graphics_CreateRenderLayer() = 0;
	// virtual void               Graphics_DestroyRenderLayer( RenderLayer_t* spLayer ) = 0;

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

	virtual ChHandle_t         LoadTexture( ChHandle_t& srHandle, const std::string& srTexturePath, const TextureCreateData_t& srCreateData )                                                 = 0;

	virtual ChHandle_t         CreateTexture( const TextureCreateInfo_t& srTextureCreateInfo, const TextureCreateData_t& srCreateData )                                                       = 0;

	virtual void               FreeTexture( ChHandle_t shTexture )                                                                                                                            = 0;

	// ---------------------------------------------------------------------------------------
	// Materials

	// Load a cmt file from disk, increments ref count
	virtual Handle             LoadMaterial( const std::string& srPath )                                                                                                                      = 0;

	// Create a new material with a name and a shader
	virtual Handle             CreateMaterial( const std::string& srName, Handle shShader )                                                                                                   = 0;

	// Free a material
	virtual void               FreeMaterial( Handle sMaterial )                                                                                                                               = 0;

	// Find a material by name
	// Name is a path to the cmt file if it was loaded on disk
	// EXAMPLE: C:/chocolate/sidury/materials/dev/grid01.cmt
	// NAME: materials/dev/grid01
	virtual Handle             FindMaterial( const char* spName )                                                                                                                             = 0;

	// Get the total amount of materials created
	virtual size_t             GetMaterialCount()                                                                                                                                             = 0;

	// Get the path to the material
	virtual const std::string& GetMaterialPath( Handle sMaterial )                                                                                                                            = 0;

	// Tell all materials to rebuild
	virtual void               SetAllMaterialsDirty()                                                                                                                                         = 0;

	// Modifying Material Data
	virtual const char*        Mat_GetName( Handle mat )                                                                                                                                      = 0;
	virtual size_t             Mat_GetVarCount( Handle mat )                                                                                                                                  = 0;
	virtual EMatVar            Mat_GetVarType( Handle mat, size_t sIndex )                                                                                                                    = 0;

	virtual Handle             Mat_GetShader( Handle mat )                                                                                                                                    = 0;
	virtual void               Mat_SetShader( Handle mat, Handle shShader )                                                                                                                   = 0;

	virtual VertexFormat       Mat_GetVertexFormat( Handle mat )                                                                                                                              = 0;

	// Increments Reference Count for material
	virtual void               Mat_AddRef( ChHandle_t sMat )                                                                                                                                  = 0;

	// Decrements Reference Count for material - returns true if the material is deleted
	virtual bool               Mat_RemoveRef( ChHandle_t sMat )                                                                                                                               = 0;

	virtual void               Mat_SetVar( Handle mat, const std::string& name, Handle texture )                                                                                              = 0;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, float data )                                                                                                  = 0;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, int data )                                                                                                    = 0;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, bool data )                                                                                                   = 0;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec2& data )                                                                                       = 0;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec3& data )                                                                                       = 0;
	virtual void               Mat_SetVar( Handle mat, const std::string& name, const glm::vec4& data )                                                                                       = 0;

	virtual int                Mat_GetTextureIndex( Handle mat, std::string_view name, Handle fallback = InvalidHandle )                                                                      = 0;
	virtual Handle             Mat_GetTexture( Handle mat, std::string_view name, Handle fallback = InvalidHandle )                                                                           = 0;
	virtual float              Mat_GetFloat( Handle mat, std::string_view name, float fallback = 0.f )                                                                                        = 0;
	virtual int                Mat_GetInt( Handle mat, std::string_view name, int fallback = 0 )                                                                                              = 0;
	virtual bool               Mat_GetBool( Handle mat, std::string_view name, bool fallback = false )                                                                                        = 0;
	virtual const glm::vec2&   Mat_GetVec2( Handle mat, std::string_view name, const glm::vec2& fallback = {} )                                                                               = 0;
	virtual const glm::vec3&   Mat_GetVec3( Handle mat, std::string_view name, const glm::vec3& fallback = {} )                                                                               = 0;
	virtual const glm::vec4&   Mat_GetVec4( Handle mat, std::string_view name, const glm::vec4& fallback = {} )                                                                               = 0;

	// ---------------------------------------------------------------------------------------
	// Shaders

	// virtual bool               Shader_Init( bool sRecreate )                                                                                                                                  = 0;
	virtual Handle             GetShader( std::string_view sName )                                                                                                                            = 0;
	virtual const char*        GetShaderName( Handle sShader )                                                                                                                                = 0;

	// Used to know if this material needs to be ordered and drawn after all opaque ones are drawn
	// virtual bool               Shader_IsMaterialTransparent( Handle sMat ) = 0;

	// Used to get all material vars the shader can use, and throw warnings on unknown material vars
	// virtual void               Shader_GetMaterialVars( Handle sShader ) = 0;

	// ---------------------------------------------------------------------------------------
	// Buffers

	virtual void               CreateVertexBuffers( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName = nullptr )                                                 = 0;
	virtual void               CreateIndexBuffer( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName = nullptr )                                                   = 0;
	// void               CreateModelBuffers( ModelBuffers_t* spBuffers, VertexData_t* spVertexData, bool sCreateIndex, const char* spDebugName ) = 0;

	// ---------------------------------------------------------------------------------------
	// Lighting

	virtual Light_t*           CreateLight( ELightType sType )                                                                                                                                = 0;
	virtual void               UpdateLight( Light_t* spLight )                                                                                                                                = 0;
	virtual void               DestroyLight( Light_t* spLight )                                                                                                                               = 0;

	// ---------------------------------------------------------------------------------------
	// Rendering

	virtual bool               Init()                                                                                                                                                         = 0;
	virtual void               Shutdown()                                                                                                                                                     = 0;

	virtual void               NewFrame()                                                                                                                                                     = 0;
	virtual void               Reset()                                                                                                                                                        = 0;
	virtual void               Present()                                                                                                                                                      = 0;

	// ChHandle_t         CreateRenderPass() = 0;
	// virtual void               UpdateRenderPass( ChHandle_t sRenderPass ) = 0;

	// Returns the Viewport Index - input is the address of a pointer
	virtual u32                CreateViewport( ViewportShader_t** spViewport = nullptr )                                                                                                      = 0;
	virtual void               FreeViewport( u32 sViewportIndex )                                                                                                                             = 0;

	virtual ViewportShader_t*  GetViewportData( u32 sViewportIndex )                                                                                                                          = 0;
	virtual void               SetViewportUpdate( bool sUpdate )                                                                                                                              = 0;

	// virtual void               PushViewInfo( const ViewportShader_t& srViewInfo ) = 0;
	// virtual void               PopViewInfo() = 0;
	// virtual ViewportShader_t&  GetViewInfo() = 0;

	virtual ChHandle_t         CreateRenderable( ChHandle_t sModel )                                                                                                                          = 0;
	virtual Renderable_t*      GetRenderableData( ChHandle_t sRenderable )                                                                                                                    = 0;
	virtual void               FreeRenderable( ChHandle_t sRenderable )                                                                                                                       = 0;
	virtual void               UpdateRenderableAABB( ChHandle_t sRenderable )                                                                                                                 = 0;
	// virtual void               ConsolidateRenderables()                                                                                                                                       = 0;

	virtual ModelBBox_t        CreateWorldAABB( glm::mat4& srMatrix, const ModelBBox_t& srBBox )                                                                                              = 0;

	// ---------------------------------------------------------------------------------------
	// Debug Rendering

	virtual void               DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor )                                                                                  = 0;
	virtual void               DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec4& sColor )                                                                                  = 0;
	virtual void               DrawAxis( const glm::vec3& sPos, const glm::vec3& sAng, const glm::vec3& sScale )                                                                              = 0;
	virtual void               DrawBBox( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor )                                                                                  = 0;
	virtual void               DrawProjView( const glm::mat4& srProjView )                                                                                                                    = 0;
	virtual void               DrawFrustum( const Frustum_t& srFrustum )                                                                                                                      = 0;
	virtual void               DrawNormals( ChHandle_t sModel, const glm::mat4& srMatrix )                                                                                                    = 0;

	// ---------------------------------------------------------------------------------------
	// Vertex Format/Attributes

	virtual GraphicsFmt        GetVertexAttributeFormat( VertexAttribute attrib )                                                                                                             = 0;
	virtual size_t             GetVertexAttributeTypeSize( VertexAttribute attrib )                                                                                                           = 0;
	virtual size_t             GetVertexAttributeSize( VertexAttribute attrib )                                                                                                               = 0;
	virtual size_t             GetVertexFormatSize( VertexFormat format )                                                                                                                     = 0;

	virtual void               GetVertexBindingDesc( VertexFormat format, std::vector< VertexInputBinding_t >& srAttrib )                                                                     = 0;
	virtual void               GetVertexAttributeDesc( VertexFormat format, std::vector< VertexInputAttribute_t >& srAttrib )                                                                 = 0;
};


#define IGRAPHICS_NAME "Graphics"
#define IGRAPHICS_VER  1
