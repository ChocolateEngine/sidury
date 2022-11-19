#include "core/core.h"
#include "igui.h"
#include "render/irender.h"
#include "graphics.h"
#include "mesh_builder.h"
#include "imgui/imgui.h"
#include "../main.h"

#include <forward_list>
#include <stack>
#include <set>


LOG_REGISTER_CHANNEL_EX( gLC_ClientGraphics, "ClientGraphics", LogColor::Green )

// --------------------------------------------------------------------------------------
// Interfaces

extern BaseGuiSystem*        gui;
extern IRender*              render;

// --------------------------------------------------------------------------------------

void                         Graphics_LoadObj( const std::string& srBasePath, const std::string& srPath, Model* spModel );
void                         Graphics_LoadGltf( const std::string& srBasePath, const std::string& srPath, const std::string& srExt, Model* spModel );

void                         Graphics_LoadSceneObj( const std::string& srBasePath, const std::string& srPath, Scene_t* spScene );

// shaders, fun
void                         Shader_Basic3D_UpdateMaterialData( Handle sMat );
void                         Shader_UI_Draw( Handle cmd, size_t sCmdIndex, Handle shColor );
void                         Shader_ShadowMap_SetViewInfo( int sViewInfo );

// --------------------------------------------------------------------------------------
// General Rendering

// TODO: rethink this so you can have draw ordering
// use the RenderLayer idea you had

// struct RenderList_t
// {
// 	std::forward_list< SurfaceDraw_t* > aSurfaces;
// };

struct ViewRenderList_t
{
	// TODO: needs improvement and further sorting
	// [ Shader ] = vector of surfaces to draw
	std::unordered_map< Handle, ChVector< SurfaceDraw_t > > aRenderLists;
};

// static std::unordered_map<
//   Handle,
//   ChVector< SurfaceDraw_t > >
// 												 gModelDrawList;

static ResourceList< Renderable_t >               gRenderables;
static std::unordered_map< Handle, ModelBBox_t >  gRenderAABBUpdate;

std::vector< ViewRenderList_t >                   gViewRenderLists;

static std::vector< Handle >                      gCommandBuffers;
static size_t                                     gCmdIndex = 0;

Handle                                            gRenderPassGraphics;
Handle                                            gRenderPassShadow;

Handle                                            gCurFramebuffer = InvalidHandle;

// stores backbuffer color and depth
static Handle                                     gBackBuffer[ 2 ];
static Handle                                     gBackBufferTex[ 3 ];

// descriptor sets
UniformBufferArray_t                              gUniformSampler;
UniformBufferArray_t                              gUniformViewInfo;
UniformBufferArray_t                              gUniformMaterialBasic3D;
constexpr u32                                     MAX_MATERIALS_BASIC3D = 500;

extern std::set< Handle >                         gDirtyMaterials;

static Handle                                     gSkyboxShader = InvalidHandle;

// --------------------------------------------------------------------------------------
// Viewports

static glm::mat4                                  gViewProjMat;
constexpr int                                     MAX_VIEW_INFO_BUFFERS = 32;
static std::vector< Handle >                      gViewInfoBuffers( MAX_VIEW_INFO_BUFFERS + 1 );
std::vector< ViewInfo_t >                         gViewInfo( 1 );
std::vector< Frustum_t >                          gViewInfoFrustums;
std::stack< ViewInfo_t >                          gViewInfoStack;
bool                                              gViewInfoUpdate    = false;
int                                               gViewInfoCount     = 1;
int                                               gViewInfoIndex     = 0;

// --------------------------------------------------------------------------------------
// Debug Drawing

// static MeshBuilder                               gDebugLineBuilder;
static Handle                                     gDebugLineModel    = InvalidHandle;
static Handle                                     gDebugLineDraw     = InvalidHandle;
static Handle                                     gDebugLineMaterial = InvalidHandle;
static ChVector< glm::vec3 >                      gDebugLineVertPos;
static ChVector< glm::vec3 >                      gDebugLineVertColor;
static size_t                                     gDebugLineBufferSize = 0;

// --------------------------------------------------------------------------------------
// Lighting

UniformBufferArray_t                              gUniformLightInfo;

UniformBufferArray_t                              gUniformLightDirectional;
UniformBufferArray_t                              gUniformLightPoint;
UniformBufferArray_t                              gUniformLightCone;
UniformBufferArray_t                              gUniformLightCapsule;

std::unordered_map< Light_t*, Handle >            gLightBuffers;

constexpr int                                     MAX_LIGHTS = MAX_VIEW_INFO_BUFFERS;
std::vector< Light_t* >                           gLights;
std::vector< Light_t* >                           gDirtyLights;
std::vector< Light_t* >                           gDestroyLights;

LightInfo_t                                       gLightInfo;
Handle                                            gLightInfoStagingBuffer = InvalidHandle;
Handle                                            gLightInfoBuffer        = InvalidHandle;
Light_t*                                          gpWorldLight            = nullptr;

static bool                                       gNeedLightInfoUpdate    = false;

// --------------------------------------------------------------------------------------
// Shadow Mapping

struct ShadowMap_t
{
	Handle     aTexture     = InvalidHandle;
	Handle     aFramebuffer = InvalidHandle;
	glm::ivec2 aSize{};
	int        aViewInfoIndex = 0;
};

std::unordered_map< Light_t*, ShadowMap_t >      gLightShadows;
UniformBufferArray_t                             gUniformShadows;

// --------------------------------------------------------------------------------------
// Assets

struct ModelAABBUpdate_t
{
	Renderable_t* apModelDraw;
	ModelBBox_t  aBBox;
};

static ResourceList< Model >                     gModels;
static std::unordered_map< std::string, Handle > gModelPaths;
static std::unordered_map< Handle, ModelBBox_t > gModelBBox;

static ResourceList< Scene_t >                   gScenes;
static std::unordered_map< std::string, Handle > gScenePaths;

// --------------------------------------------------------------------------------------
// Other

size_t                                           gModelDrawCalls = 0;
size_t                                           gVertsDrawn = 0;


// TEMP LIGHTING - WILL BE DEFINED IN MAP FORMAT LATER
CONVAR( r_world_dir_x, 65 );
CONVAR( r_world_dir_y, 35 );
CONVAR( r_world_dir_z, 0 );

CONVAR( r_shadowmap_size, 2048 );

CONVAR( r_shadowmap_constant, 16.f );  // 1.25f
CONVAR( r_shadowmap_clamp, 0.f );
CONVAR( r_shadowmap_slope, 1.75f );

CONVAR( r_shadowmap_fov_hack, 90.f );
CONVAR( r_shadowmap_nearz, 1.f );
CONVAR( r_shadowmap_farz, 2000.f );

CONVAR( r_shadowmap_othro_left, -1000.f );
CONVAR( r_shadowmap_othro_right, 1000.f );
CONVAR( r_shadowmap_othro_bottom, -1000.f );
CONVAR( r_shadowmap_othro_top, 1000.f );

CONVAR( r_vis, 1 );
CONVAR( r_vis_lock, 0 );

// Debug Drawing
CONVAR( r_debug_draw, 1 );
CONVAR( r_debug_aabb, 1 );
CONVAR( r_debug_frustums, 0 );

CONVAR( r_line_thickness, 2 );

CONCMD( r_reload_textures )
{
	render->ReloadTextures();
	// Graphics_SetAllMaterialsDirty();
}


void Graphics_CalcModelBBox( Handle sModel, Model* spModel )
{
	if ( !spModel )
		return;

	ModelBBox_t bbox{};
	bbox.aMax = { INT_MIN, INT_MIN, INT_MIN };
	bbox.aMin = { INT_MAX, INT_MAX, INT_MAX };

	auto*      vertData = spModel->apVertexData;
	glm::vec3* data     = nullptr;

	for ( auto& attrib : vertData->aData )
	{
		if ( attrib.aAttrib == VertexAttribute_Position )
		{
			data = (glm::vec3*)attrib.apData;
			break;
		}
	}

	if ( data == nullptr )
	{
		Log_Error( "Position Vertex Data not found?\n" );
		gModelBBox[ sModel ] = {};
		return;
	}

	auto UpdateBBox = [ & ]( const glm::vec3& vertex )
	{
		bbox.aMin.x = glm::min( bbox.aMin.x, vertex.x );
		bbox.aMin.y = glm::min( bbox.aMin.y, vertex.y );
		bbox.aMin.z = glm::min( bbox.aMin.z, vertex.z );

		bbox.aMax.x = glm::max( bbox.aMax.x, vertex.x );
		bbox.aMax.y = glm::max( bbox.aMax.y, vertex.y );
		bbox.aMax.z = glm::max( bbox.aMax.z, vertex.z );
	};

	for ( Mesh& mesh : spModel->aMeshes )
	{
		if ( vertData->aIndices.size() )
		{
			for ( u32 i = 0; i < mesh.aIndexCount; i++ )
				UpdateBBox( data[ vertData->aIndices[ mesh.aIndexOffset + i ] ] );
		}
		else
		{
			for ( u32 i = 0; i < mesh.aVertexCount; i++ )
				UpdateBBox( data[ mesh.aVertexOffset + i ] );
		}
	}

	gModelBBox[ sModel ] = bbox;
}


Handle Graphics_LoadModel( const std::string& srPath )
{
	// Have we loaded this model already?
	auto it = gModelPaths.find( srPath );

	if ( it != gModelPaths.end() )
	{
		// We have, use that model instead
		return it->second;
	}

	// We have not, so try to load this model in
	std::string fullPath = FileSys_FindFile( srPath );

	if ( fullPath.empty() )
	{
		Log_DevF( gLC_ClientGraphics, 1, "LoadModel: Failed to Find Model: %s\n", srPath.c_str() );
		return InvalidHandle;
	}

	std::string fileExt = FileSys_GetFileExt( srPath );

	Model* model = nullptr;
	Handle handle = InvalidHandle;

	// TODO: try to do file header checking
	if ( fileExt == "obj" )
	{
		handle = gModels.Create( &model );

		if ( handle == InvalidHandle )
		{
			Log_ErrorF( gLC_ClientGraphics, "LoadModel: Failed to Allocate Model: %s\n", srPath.c_str() );
			return InvalidHandle;
		}

		Graphics_LoadObj( srPath, fullPath, model );
	}
	else if ( fileExt == "glb" || fileExt == "gltf" )
	{
		handle = gModels.Create( &model );

		if ( handle == InvalidHandle )
		{
			Log_ErrorF( gLC_ClientGraphics, "LoadModel: Failed to Allocate Model: %s\n", srPath.c_str() );
			return InvalidHandle;
		}

		Graphics_LoadGltf( srPath, fullPath, fileExt, model );
	}
	else
	{
		Log_DevF( gLC_ClientGraphics, 1, "Unknown Model File Extension: %s\n", fileExt.c_str() );
		return InvalidHandle;
	}

	//sModel->aRadius = glm::distance( mesh->aMinSize, mesh->aMaxSize ) / 2.0f;

	// TODO: load in an error model here instead
	if ( model->aMeshes.empty() )
	{
		gModels.Remove( handle );
		return InvalidHandle;
	}

	// calculate a bounding box
	Graphics_CalcModelBBox( handle, model );

	gModelPaths[ srPath ]     = handle;

	return handle;
}


Handle Graphics_CreateModel( Model** spModel )
{
	return gModels.Create( spModel );
}


void Graphics_FreeModel( Handle shModel )
{
	// HACK HACK PERF: we have to wait for queues to finish, so we could just free this model later
	// maybe right before the next draw?
	render->WaitForQueues();

	Model* model = nullptr;
	if ( !gModels.Get( shModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Model_SetMaterial: Model is nullptr\n" );
		return;
	}

	if ( model->apVertexData )
	{
		model->apVertexData->Release();
	}

	if ( model->apBuffers )
	{
		model->apBuffers->Release();
	// 	for ( auto& buf : model->apBuffers->aVertex )
	// 		render->DestroyBuffer( buf );
	// 
	// 	if ( model->apBuffers->aIndex )
	// 		render->DestroyBuffer( model->apBuffers->aIndex );
	// 
	// 	model->apBuffers->aVertex.clear();
	// 	model->apBuffers->aIndex = InvalidHandle;
	}

	for ( auto& [ path, modelHandle ] : gModelPaths )
	{
		if ( modelHandle == shModel )
		{
			gModelPaths.erase( path );
			break;
		}
	}

	gModels.Remove( shModel );
	gModelBBox.erase( shModel );
}


Model* Graphics_GetModelData( Handle shModel )
{
	Model* model = nullptr;
	if ( !gModels.Get( shModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Model_SetMaterial: Model is nullptr\n" );
		return nullptr;
	}

	return model;
}


void Graphics_UpdateRenderableAABB( Handle sRenderable )
{
	if ( !sRenderable )
		return;

	if ( Renderable_t* renderable = Graphics_GetRenderableData( sRenderable ) )
		gRenderAABBUpdate.emplace( sRenderable, gModelBBox[ renderable->aModel ] );
}


void Model_SetMaterial( Handle shModel, size_t sSurface, Handle shMat )
{
	Model* model = nullptr;
	if ( !gModels.Get( shModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Model_SetMaterial: Model is nullptr\n" );
		return;
	}

	if ( sSurface > model->aMeshes.size() )
	{
		Log_ErrorF( gLC_ClientGraphics, "Model_SetMaterial: surface is out of range: %zu (Surface Count: %zu)\n", sSurface, model->aMeshes.size() );
		return;
	}

	model->aMeshes[ sSurface ].aMaterial = shMat;
}


Handle Model_GetMaterial( Handle shModel, size_t sSurface )
{
	Model* model = nullptr;
	if ( !gModels.Get( shModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Model_SetMaterial: Model is nullptr\n" );
		return InvalidHandle;
	}

	if ( sSurface >= model->aMeshes.size() )
	{
		Log_ErrorF( gLC_ClientGraphics, "Model_SetMaterial: surface is out of range: %zu (Surface Count: %zu)\n", sSurface, model->aMeshes.size() );
		return InvalidHandle;
	}

	return model->aMeshes[ sSurface ].aMaterial;
}

// ---------------------------------------------------------------------------------------
// Scenes


Handle Graphics_LoadScene( const std::string& srPath )
{
	// Have we loaded this scene already?
	auto it = gScenePaths.find( srPath );

	if ( it != gScenePaths.end() )
	{
		// We have, use that scene instead
		return it->second;
	}

	// We have not, so try to load this model in
	std::string fullPath = FileSys_FindFile( srPath );

	if ( fullPath.empty() )
	{
		Log_ErrorF( gLC_ClientGraphics, "LoadScene: Failed to Find Scene: %s\n", srPath.c_str() );
		return InvalidHandle;
	}

	std::string fileExt = FileSys_GetFileExt( srPath );

	// Scene_t*    scene   = new Scene_t;
	Scene_t*    scene   = nullptr;
	Handle      handle  = InvalidHandle;

	// TODO: try to do file header checking
	if ( fileExt == "obj" )
	{
		handle = gScenes.Create( &scene );
		
		if ( handle == InvalidHandle )
		{
			Log_ErrorF( gLC_ClientGraphics, "LoadScene: Failed to Allocate Scene: %s\n", srPath.c_str() );
			return InvalidHandle;
		}

		memset( &scene->aModels, 0, sizeof( scene->aModels ) );
		Graphics_LoadSceneObj( srPath, fullPath, scene );
	}
	// else if ( fileExt == "glb" || fileExt == "gltf" )
	// {
	// 	// handle = gScenes.Add( scene );
	// 	// Graphics_LoadGltf( srPath, fullPath, fileExt, model );
	// }
	else
	{
		Log_DevF( gLC_ClientGraphics, 1, "Unknown Model File Extension: %s\n", fileExt.c_str() );
	}

	//sModel->aRadius = glm::distance( mesh->aMinSize, mesh->aMaxSize ) / 2.0f;

	// TODO: load in an error model here instead
	if ( scene->aModels.empty() )
	{
		gScenes.Remove( handle );
		// delete scene;
		return InvalidHandle;
	}

	// Calculate Bounding Boxes for Models
	for ( const auto& modelHandle : scene->aModels )
	{
		Model* model = Graphics_GetModelData( modelHandle );
		Graphics_CalcModelBBox( modelHandle, model );
	}

	gScenePaths[ srPath ] = handle;
	return handle;
}


void Graphics_FreeScene( Handle sScene )
{
	// HACK HACK PERF: we have to wait for queues to finish, so we could just free this model later
	// maybe right before the next draw?
	render->WaitForQueues();

	Scene_t* scene = nullptr;
	if ( !gScenes.Get( sScene, &scene ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_FreeScene: Failed to find Scene\n" );
		return;
	}

	for ( auto& model : scene->aModels )
	{
		Graphics_FreeModel( model );
	}
	
	for ( auto& [ path, sceneHandle ] : gScenePaths )
	{
		if ( sceneHandle == sScene )
		{
			gScenePaths.erase( path );
			break;
		}
	}

	// delete scene;
	gScenes.Remove( sScene );
}


SceneDraw_t* Graphics_AddSceneDraw( Handle sScene )
{
	if ( !sScene )
		return nullptr;

	Scene_t* scene = nullptr;
	if ( !gScenes.Get( sScene, &scene ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_DrawScene: Failed to find Scene\n" );
		return nullptr;
	}

	SceneDraw_t* sceneDraw = new SceneDraw_t;
	sceneDraw->aScene      = sScene;
	sceneDraw->aDraw.resize( scene->aModels.size() );

	for ( uint32_t i = 0; i < scene->aModels.size(); i++ )
	{
		sceneDraw->aDraw[ i ] = Graphics_CreateRenderable( scene->aModels[ i ] );
	}

	return sceneDraw;
}


void Graphics_RemoveSceneDraw( SceneDraw_t* spScene )
{
	if ( !spScene )
		return;

	// Scene_t* scene = nullptr;
	// if ( !gScenes.Get( spScene->aScene, &scene ) )
	// {
	// 	Log_Error( gLC_ClientGraphics, "Graphics_DrawScene: Failed to find Scene\n" );
	// 	return;
	// }

	for ( auto& modelDraw : spScene->aDraw )
	{
		Graphics_FreeRenderable( modelDraw );
	}

	delete spScene;
}


size_t Graphics_GetSceneModelCount( Handle sScene )
{
	Scene_t* scene = nullptr;
	if ( !gScenes.Get( sScene, &scene ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_GetSceneModelCount: Failed to find Scene\n" );
		return 0;
	}

	return scene->aModels.size();
}


Handle Graphics_GetSceneModel( Handle sScene, size_t sIndex )
{
	Scene_t* scene = nullptr;
	if ( !gScenes.Get( sScene, &scene ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_GetSceneModel: Failed to find Scene\n" );
		return 0;
	}

	if ( sIndex >= scene->aModels.size() )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_GetSceneModel: Index out of range\n" );
		return InvalidHandle;
	}

	return scene->aModels[ sIndex ];
}


// ---------------------------------------------------------------------------------------
// 

void Graphics_DestroyRenderTargets()
{
	// Graphics_SetAllMaterialsDirty();
}


bool Graphics_CreateRenderTargets()
{
	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	// ---------------------------------------------------------
	// ImGui (here for future reference)
#if 0
	{
		// Create ImGui Color Attachment
		TextureCreateInfo_t texCreate{};
		texCreate.apName    = "ImGui Color";
		texCreate.aSize     = { width, height };
		texCreate.aFormat   = GraphicsFmt::BGRA8888_UNORM;
		texCreate.aViewType = EImageView_2D;

		TextureCreateData_t createData{};
		createData.aUsage   = EImageUsage_AttachColor | EImageUsage_Sampled;
		createData.aFilter  = EImageFilter_Nearest;

		gImGuiTextures[ 0 ] = render->CreateTexture( texCreate, createData );

		// Create ImGui Depth Stencil Attachment
		texCreate.apName    = "ImGui Depth";
		texCreate.aFormat   = render->GetSwapFormatDepth();
		createData.aUsage   = EImageUsage_AttachDepthStencil | EImageUsage_Sampled;

		gImGuiTextures[ 1 ] = render->CreateTexture( texCreate, createData );

		// Create a new framebuffer for ImGui to draw on
		CreateFramebuffer_t frameBufCreate{};
		frameBufCreate.aRenderPass = gRenderPassUI;  // ImGui will be drawn onto the graphics RenderPass
		frameBufCreate.aSize       = { width, height };

		// Create Color
		frameBufCreate.aPass.aAttachColors.push_back( gImGuiTextures[ 0 ] );
		frameBufCreate.aPass.aAttachDepth = gImGuiTextures[ 1 ];
		gImGuiBuffer[ 0 ] = render->CreateFramebuffer( frameBufCreate );  // Color
		gImGuiBuffer[ 1 ] = render->CreateFramebuffer( frameBufCreate );  // Depth

		if ( gImGuiBuffer[ 0 ] == InvalidHandle || gImGuiBuffer[ 1 ] == InvalidHandle )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create render targets\n" );
			return false;
		}
	}
#endif

	return true;
}


void Graphics_DestroyRenderPasses()
{
	if ( gRenderPassShadow != InvalidHandle )
		render->DestroyRenderPass( gRenderPassShadow );
}


bool Graphics_CreateRenderPasses()
{
	RenderPassCreate_t create{};

	// Shadow Map Render Pass
	{
		create.aAttachments.resize( 1 );

		create.aAttachments[ 0 ].aFormat         = render->GetSwapFormatDepth();
		create.aAttachments[ 0 ].aUseMSAA        = false;
		create.aAttachments[ 0 ].aType           = EAttachmentType_Depth;
		create.aAttachments[ 0 ].aLoadOp         = EAttachmentLoadOp_Clear;
		create.aAttachments[ 0 ].aStoreOp        = EAttachmentStoreOp_Store;
		create.aAttachments[ 0 ].aStencilLoadOp  = EAttachmentLoadOp_Load;
		create.aAttachments[ 0 ].aStencilStoreOp = EAttachmentStoreOp_Store;

		create.aSubpasses.resize( 1 );
		create.aSubpasses[ 0 ].aBindPoint = EPipelineBindPoint_Graphics;

		gRenderPassShadow                 = render->CreateRenderPass( create );

		if ( gRenderPassShadow == InvalidHandle )
			return false;
	}

	return true;
}


bool Graphics_CreateVariableUniformLayout( UniformBufferArray_t& srBuffer, const char* spLayoutName, const char* spSetName, int sCount )
{
	CreateVariableDescLayout_t createLayout{};
	createLayout.apName   = spLayoutName;
	createLayout.aType    = EDescriptorType_UniformBuffer;
	createLayout.aStages  = ShaderStage_Vertex | ShaderStage_Fragment;
	createLayout.aBinding = 0;
	createLayout.aCount   = sCount;

	srBuffer.aLayout      = render->CreateVariableDescLayout( createLayout );

	if ( srBuffer.aLayout == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create variable desc layout\n" );
		return false;
	}

	AllocVariableDescLayout_t allocLayout{};
	allocLayout.apName    = spSetName;
	allocLayout.aLayout   = srBuffer.aLayout;
	allocLayout.aCount    = sCount;
	allocLayout.aSetCount = srBuffer.aSets.size();

	if ( !render->AllocateVariableDescLayout( allocLayout, srBuffer.aSets.data() ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to allocate variable desc layout\n" );
		return false;
	}

	return true;
}


bool Graphics_CreateUniformBuffers( UniformBufferArray_t& srUniform, std::vector< Handle >& srBuffers, const char* spBufferName, size_t sBufferSize )
{
	// create buffers for it
	for ( size_t i = 0; i < srBuffers.size(); i++ )
	{
		Handle buffer = render->CreateBuffer( spBufferName, sBufferSize, EBufferFlags_Uniform, EBufferMemory_Host );

		if ( buffer == InvalidHandle )
		{
			Log_Error( gLC_ClientGraphics, "Failed to Create Light Uniform Buffer\n" );
			return false;
		}

		srBuffers[ i ] = buffer;
	}

	// update the descriptor sets
	UpdateVariableDescSet_t update{};

	for ( size_t i = 0; i < srUniform.aSets.size(); i++ )
		update.aDescSets.push_back( srUniform.aSets[ i ] );

	update.aType    = EDescriptorType_UniformBuffer;
	update.aBuffers = srBuffers;
	render->UpdateVariableDescSet( update );

	return true;
}


void Graphics_AddShadowMap( Light_t* spLight )
{
	ShadowMap_t& shadowMap   = gLightShadows[ spLight ];
	shadowMap.aSize          = { r_shadowmap_size.GetFloat(), r_shadowmap_size.GetFloat() };
	shadowMap.aViewInfoIndex = gViewInfoCount++;

	// gViewInfo.resize( gViewInfoCount );
	// ViewInfo_t& viewInfo     = gViewInfo.at( shadowMap.aViewInfoIndex );
	ViewInfo_t& viewInfo     = gViewInfo.emplace_back();
	viewInfo.aShaderOverride = Graphics_GetShader( "__shadow_map" );

	// Create Textures
	TextureCreateInfo_t texCreate{};
	texCreate.apName    = "Shadow Map";
	texCreate.aSize     = shadowMap.aSize;
	texCreate.aFormat   = render->GetSwapFormatDepth();
	texCreate.aViewType = EImageView_2D;

	TextureCreateData_t createData{};
	createData.aUsage          = EImageUsage_AttachDepthStencil | EImageUsage_Sampled;
	createData.aFilter         = EImageFilter_Linear;
	// createData.aSamplerAddress = ESamplerAddressMode_ClampToEdge;
	createData.aSamplerAddress = ESamplerAddressMode_ClampToBorder;

	shadowMap.aTexture         = render->CreateTexture( texCreate, createData );

	// Create Framebuffer
	CreateFramebuffer_t frameBufCreate{};
	frameBufCreate.aRenderPass = gRenderPassShadow;  // ImGui will be drawn onto the graphics RenderPass
	frameBufCreate.aSize       = shadowMap.aSize;

	// Create Color
	frameBufCreate.aPass.aAttachDepth = shadowMap.aTexture;
	shadowMap.aFramebuffer            = render->CreateFramebuffer( frameBufCreate );

	if ( shadowMap.aFramebuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create shadow map!\n" );
	}
}


void Graphics_DestroyShadowMap( Light_t* spLight )
{
	auto it = gLightShadows.find( spLight );
	if ( it == gLightShadows.end() )
	{
		Log_Error( gLC_ClientGraphics, "Light Shadow Map not found for deletion!\n" );
		return;
	}

	if ( it->second.aFramebuffer )
		render->DestroyFramebuffer( it->second.aFramebuffer );

	if ( it->second.aTexture )
		render->FreeTexture( it->second.aTexture );

	gLightShadows.erase( spLight );
}


Handle Graphics_AddLightBuffer( UniformBufferArray_t& srBuffer, const char* spBufferName, size_t sBufferSize, Light_t* spLight )
{
	Handle buffer = render->CreateBuffer( spBufferName, sBufferSize, EBufferFlags_Uniform, EBufferMemory_Host );

	if ( buffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Light Uniform Buffer\n" );
		return InvalidHandle;
	}

	gLightBuffers[ spLight ] = buffer;

	// update the descriptor sets
	UpdateVariableDescSet_t update{};
	update.aType = EDescriptorType_UniformBuffer;

	for ( size_t i = 0; i < srBuffer.aSets.size(); i++ )
		update.aDescSets.push_back( srBuffer.aSets[ i ] );

	for ( const auto& [ light, bufferHandle ] : gLightBuffers )
	{
		if ( light->aType == spLight->aType )
			update.aBuffers.push_back( bufferHandle );
	}

	render->UpdateVariableDescSet( update );

	// if ( spLight->aType == ELightType_Cone || spLight->aType == ELightType_Directional )
	if ( spLight->aType == ELightType_Cone )
	{
		Graphics_AddShadowMap( spLight );
	}

	return buffer;
}


void Graphics_DestroyLightBuffer( Light_t* spLight )
{
	auto it = gLightBuffers.find( spLight );
	if ( it == gLightBuffers.end() )
	{
		Log_Error( gLC_ClientGraphics, "Light not found for deletion!\n" );
		return;
	}

	render->DestroyBuffer( it->second );
	gLightBuffers.erase( spLight );

	if ( spLight->aType == ELightType_Cone )
	{
		Graphics_DestroyShadowMap( spLight );
	}

	// update the descriptor sets
	UpdateVariableDescSet_t update{};
	update.aType = EDescriptorType_UniformBuffer;

	UniformBufferArray_t* buffer = nullptr;
	switch ( spLight->aType )
	{
		default:
			Assert( false );
			Log_ErrorF( gLC_ClientGraphics, "Unknown Light Buffer: %d\n", spLight->aType );
			return;
		case ELightType_Directional:
			buffer = &gUniformLightDirectional;
			gLightInfo.aCountWorld--;
			break;
		case ELightType_Point:
			buffer = &gUniformLightPoint;
			gLightInfo.aCountPoint--;
			break;
		case ELightType_Cone:
			buffer = &gUniformLightCone;
			gLightInfo.aCountCone--;
			break;
		case ELightType_Capsule:
			buffer = &gUniformLightCapsule;
			gLightInfo.aCountCapsule--;
			break;
	}

	for ( size_t i = 0; i < buffer->aSets.size(); i++ )
		update.aDescSets.push_back( buffer->aSets[ i ] );

	for ( const auto& [ light, bufferHandle ] : gLightBuffers )
	{
		if ( light->aType == spLight->aType )
			update.aBuffers.push_back( bufferHandle );
	}

	render->UpdateVariableDescSet( update );

	vec_remove( gLights, spLight );
	delete spLight;
}


void Graphics_UpdateLightBuffer( Light_t* spLight )
{
	Handle buffer = InvalidHandle;

	auto   it     = gLightBuffers.find( spLight );

	if ( it == gLightBuffers.end() )
	{
		switch ( spLight->aType )
		{
			case ELightType_Directional:
				buffer = Graphics_AddLightBuffer( gUniformLightDirectional, "Light Directional Buffer", sizeof( UBO_LightDirectional_t ), spLight );
				gLightInfo.aCountWorld++;
				break;
			case ELightType_Point:
				buffer = Graphics_AddLightBuffer( gUniformLightPoint, "Light Point Buffer", sizeof( UBO_LightPoint_t ), spLight );
				gLightInfo.aCountPoint++;
				break;
			case ELightType_Cone:
				buffer = Graphics_AddLightBuffer( gUniformLightCone, "Light Cone Buffer", sizeof( UBO_LightCone_t ), spLight );
				gLightInfo.aCountCone++;
				break;
			case ELightType_Capsule:
				buffer = Graphics_AddLightBuffer( gUniformLightCapsule, "Light Capsule Buffer", sizeof( UBO_LightCapsule_t ), spLight );
				gLightInfo.aCountCapsule++;
				break;
		}

		if ( buffer == InvalidHandle )
		{
			Log_Warn( gLC_ClientGraphics, "Failed to Create Light Buffer!\n" );
			return;
		}

		gLightBuffers[ spLight ] = buffer;
		gNeedLightInfoUpdate     = true;
	}
	else
	{
		buffer = it->second;
	}

	switch ( spLight->aType )
	{
		default:
		{
			Log_Error( gLC_ClientGraphics, "Unknown Light Type!\n" );
			return;
		}
		case ELightType_Directional:
		{
			UBO_LightDirectional_t light;
			light.aColor.x = spLight->aColor.x;
			light.aColor.y = spLight->aColor.y;
			light.aColor.z = spLight->aColor.z;
			light.aColor.w = spLight->aEnabled;

			Transform temp{};
			temp.aPos = spLight->aPos;
			temp.aAng = spLight->aAng;
			Util_GetMatrixDirection( temp.ToMatrix( false ), nullptr, nullptr, &light.aDir );

			// glm::mat4 matrix;
			// ToMatrix( matrix, spLight->aPos, spLight->aAng );
			// matrix = temp.ToViewMatrixZ();
			// ToViewMatrix( matrix, spLight->aPos, spLight->aAng );

			// GetDirectionVectors( matrix, light.aDir );

			// Util_GetDirectionVectors( spLight->aAng, nullptr, nullptr, &light.aDir );
			// Util_GetDirectionVectors( spLight->aAng, &light.aDir );

			// update shadow map view info
#if 0
			ShadowMap_t&     shadowMap = gLightShadows[ spLight ];

			ViewportCamera_t view{};
			view.aFarZ  = r_shadowmap_farz;
			view.aNearZ = r_shadowmap_nearz;
			// view.aFOV   = spLight->aOuterFov;
			view.aFOV   = r_shadowmap_fov_hack;

			Transform transform{};
			transform.aPos                  = spLight->aPos;
			// transform.aAng = spLight->aAng;

			transform.aAng.x                = -spLight->aAng.z + 90.f;
			transform.aAng.y                = -spLight->aAng.y;
			transform.aAng.z                = spLight->aAng.x;

			// shadowMap.aViewInfo.aProjection = glm::ortho< float >( -10, 10, -10, 10, -10, 20 );

			shadowMap.aViewInfo.aProjection = glm::ortho< float >(
			  r_shadowmap_othro_left,
			  r_shadowmap_othro_right,
			  r_shadowmap_othro_bottom,
			  r_shadowmap_othro_top,
			  view.aNearZ,
			  view.aFarZ );

			// shadowMap.aViewInfo.aView       = view.aViewMat;
			shadowMap.aViewInfo.aView       = glm::lookAt( -light.aDir, glm::vec3( 0, 0, 0 ), glm::vec3( 0, -1, 0 ) );
			shadowMap.aViewInfo.aProjView   = shadowMap.aViewInfo.aProjection * shadowMap.aViewInfo.aView;
			shadowMap.aViewInfo.aNearZ      = view.aNearZ;
			shadowMap.aViewInfo.aFarZ       = view.aFarZ;

			Handle shadowBuffer             = gViewInfoBuffers[ shadowMap.aViewInfoIndex ];
			render->MemWriteBuffer( shadowBuffer, sizeof( UBO_ViewInfo_t ), &shadowMap.aViewInfo );

			// get shadow map view info
			light.aViewInfo = shadowMap.aViewInfoIndex;
			light.aShadow   = render->GetTextureIndex( shadowMap.aTexture );
#else
			// get shadow map view info
			light.aViewInfo = 0;
			light.aShadow   = -1;
#endif

			render->MemWriteBuffer( buffer, sizeof( light ), &light );
			break;
		}
		case ELightType_Point:
		{
			UBO_LightPoint_t light;
			light.aColor.x = spLight->aColor.x;
			light.aColor.y = spLight->aColor.y;
			light.aColor.z = spLight->aColor.z;
			light.aColor.w = spLight->aEnabled;

			light.aPos     = spLight->aPos;
			light.aRadius  = spLight->aRadius;

			render->MemWriteBuffer( buffer, sizeof( light ), &light );
			break;
		}
		case ELightType_Cone:
		{
			UBO_LightCone_t light;
			light.aColor.x = spLight->aColor.x;
			light.aColor.y = spLight->aColor.y;
			light.aColor.z = spLight->aColor.z;
			light.aColor.w = spLight->aEnabled;

			light.aPos     = spLight->aPos;
			light.aFov.x   = glm::radians( spLight->aInnerFov );
			light.aFov.y   = glm::radians( spLight->aOuterFov );

			glm::mat4 matrix;
			Util_ToMatrix( matrix, spLight->aPos, spLight->aAng );
			Util_GetMatrixDirection( matrix, nullptr, nullptr, &light.aDir );
#if 1
			// update shadow map view info
			ShadowMap_t&     shadowMap = gLightShadows[ spLight ];

			ViewportCamera_t view{};
			view.aFarZ  = r_shadowmap_farz;
			view.aNearZ = r_shadowmap_nearz;
			// view.aFOV   = spLight->aOuterFov;
			view.aFOV   = r_shadowmap_fov_hack;

			Transform transform{};
			transform.aPos   = spLight->aPos;
			// transform.aAng = spLight->aAng;

			transform.aAng.x = -spLight->aAng.z + 90.f;
			transform.aAng.y = -spLight->aAng.y;
			transform.aAng.z = spLight->aAng.x;

			view.aViewMat    = transform.ToViewMatrixZ();
			view.ComputeProjection( shadowMap.aSize.x, shadowMap.aSize.y );

			gViewInfo[ shadowMap.aViewInfoIndex ].aProjection = view.aProjMat;
			gViewInfo[ shadowMap.aViewInfoIndex ].aView       = view.aViewMat;
			gViewInfo[ shadowMap.aViewInfoIndex ].aProjView   = view.aProjViewMat;
			gViewInfo[ shadowMap.aViewInfoIndex ].aNearZ      = view.aNearZ;
			gViewInfo[ shadowMap.aViewInfoIndex ].aFarZ       = view.aFarZ;
			gViewInfo[ shadowMap.aViewInfoIndex ].aSize       = shadowMap.aSize;

			Handle shadowBuffer                               = gViewInfoBuffers[ shadowMap.aViewInfoIndex ];
			render->MemWriteBuffer( shadowBuffer, sizeof( UBO_ViewInfo_t ), &gViewInfo[ shadowMap.aViewInfoIndex ] );

			// get shadow map view info
			light.aViewInfo                               = shadowMap.aViewInfoIndex;
			light.aShadow                                 = render->GetTextureIndex( shadowMap.aTexture );

			gViewInfo[ shadowMap.aViewInfoIndex ].aActive = spLight->aShadow && spLight->aEnabled;
#endif

			// Update Light Buffer
			render->MemWriteBuffer( buffer, sizeof( light ), &light );
			break;
		}
		case ELightType_Capsule:
		{
			UBO_LightCapsule_t light;
			light.aColor.x   = spLight->aColor.x;
			light.aColor.y   = spLight->aColor.y;
			light.aColor.z   = spLight->aColor.z;
			light.aColor.w   = spLight->aEnabled;

			light.aPos       = spLight->aPos;
			light.aLength    = spLight->aLength;
			light.aThickness = spLight->aRadius;

			Transform temp{};
			temp.aPos = spLight->aPos;
			temp.aAng = spLight->aAng;
			Util_GetMatrixDirection( temp.ToMatrix( false ), nullptr, nullptr, &light.aDir );

			// Util_GetDirectionVectors( spLight->aAng, nullptr, nullptr, &light.aDir );
			// Util_GetDirectionVectors( spLight->aAng, &light.aDir );

			render->MemWriteBuffer( buffer, sizeof( light ), &light );
			break;
		}
	}
}


bool Graphics_CreateDescriptorSets()
{
	// TODO: just create the sampler here and have a
	// Graphics_LoadTexture() function to auto add to the image sampler sets
	gUniformSampler.aLayout = render->GetSamplerLayout();
	gUniformSampler.aSets.resize( 2 );
	render->GetSamplerSets( gUniformSampler.aSets.data() );

	// ------------------------------------------------------
	// Create ViewInfo UBO
	{
		gUniformViewInfo.aSets.resize( 2 );
		if ( !Graphics_CreateVariableUniformLayout( gUniformViewInfo, "View Info Layout", "View Info Set", gViewInfoBuffers.size() ) )
			return false;

		// create buffer for it
		// (NOTE: not changing this from a std::vector cause you could use this for multiple views in the future probably)
		for ( u32 i = 0; i < gViewInfoBuffers.size(); i++ )
		{
			Handle buffer = render->CreateBuffer( "View Info Buffer", sizeof( UBO_ViewInfo_t ), EBufferFlags_Uniform, EBufferMemory_Host );

			if ( buffer == InvalidHandle )
			{
				Log_Error( gLC_ClientGraphics, "Failed to Create View Info Uniform Buffer\n" );
				return false;
			}

			gViewInfoBuffers[ i ] = buffer;
		}

		// update the material descriptor sets
		UpdateVariableDescSet_t update{};

		// what
		update.aDescSets.push_back( gUniformViewInfo.aSets[ 0 ] );
		update.aDescSets.push_back( gUniformViewInfo.aSets[ 1 ] );

		update.aType    = EDescriptorType_UniformBuffer;
		update.aBuffers = gViewInfoBuffers;
		render->UpdateVariableDescSet( update );
	}

	// ------------------------------------------------------
	// Create Light Layouts

	gUniformLightInfo.aSets.resize( 1 );
	if ( !Graphics_CreateVariableUniformLayout( gUniformLightInfo, "Light Info Layout", "Light Info Set", 1 ) )
		return false;

	gUniformLightDirectional.aSets.resize( 1 );
	if ( !Graphics_CreateVariableUniformLayout( gUniformLightDirectional, "Light Directional Layout", "Light Directional Set", MAX_LIGHTS ) )
		return false;

	gUniformLightPoint.aSets.resize( 1 );
	if ( !Graphics_CreateVariableUniformLayout( gUniformLightPoint, "Light Point Layout", "Light Point Set", MAX_LIGHTS ) )
		return false;

	gUniformLightCone.aSets.resize( 1 );
	if ( !Graphics_CreateVariableUniformLayout( gUniformLightCone, "Light Cone Layout", "Light Cone Set", MAX_LIGHTS ) )
		return false;

	gUniformLightCapsule.aSets.resize( 1 );
	if ( !Graphics_CreateVariableUniformLayout( gUniformLightCapsule, "Light Capsule Layout", "Light Capsule Set", MAX_LIGHTS ) )
		return false;

	gUniformShadows.aSets.resize( 1 );
	if ( !Graphics_CreateVariableUniformLayout( gUniformShadows, "Shadow Map Layout", "Shadow Map Set", MAX_LIGHTS ) )
		return false;

	// ------------------------------------------------------
	// Create Material Buffers for Basic3D shader

	gUniformMaterialBasic3D.aSets.resize( 2 );
	if ( !Graphics_CreateVariableUniformLayout( gUniformMaterialBasic3D, "Basic3D Materials Layout", "Basic3D Materials Set", MAX_MATERIALS_BASIC3D ) )
		return false;

	return true;
}


void Graphics_OnResetCallback( ERenderResetFlags sFlags )
{
	gBackBuffer[ 0 ] = render->GetBackBufferColor();
	gBackBuffer[ 1 ] = render->GetBackBufferDepth();

	if ( gBackBuffer[ 0 ] == InvalidHandle || gBackBuffer[ 1 ] == InvalidHandle )
	{
		Log_Fatal( gLC_ClientGraphics, "Failed to get Back Buffer Handles!\n" );
	}

	Graphics_DestroyRenderTargets();

	if ( !Graphics_CreateRenderTargets() )
	{
		Log_Fatal( gLC_ClientGraphics, "Failed to create Render Targets!\n" );
	}

	// actually stupid, they are HANDLES, YOU SHOULDN'T NEED NEW ONES
	// only exception if we are in msaa now or not, blech
	render->GetBackBufferTextures( &gBackBufferTex[ 0 ], &gBackBufferTex[ 1 ], &gBackBufferTex[ 2 ] );

	int width, height;
	render->GetSurfaceSize( width, height );
	gViewInfo[ 0 ].aSize = { width, height };

	if ( sFlags & ERenderResetFlags_MSAA )
	{
		if ( !Graphics_CreateRenderPasses() )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create render passes\n" );
			return;
		}

		render->ShutdownImGui();
		if ( !render->InitImGui( gRenderPassGraphics ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to re-init ImGui for Vulkan\n" );
			return;
		}

		if ( !Graphics_ShaderInit( true ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to Recreate Shaders!\n" );
			return;
		}
	}
}


bool Graphics_Init()
{
	render->GetCommandBufferHandles( gCommandBuffers );

	if ( gCommandBuffers.empty() )
	{
		Log_Fatal( gLC_ClientGraphics, "Failed to get render command buffers!\n" );
		return false;
	}

	render->SetResetCallback( Graphics_OnResetCallback );

	render->GetBackBufferTextures( &gBackBufferTex[ 0 ], &gBackBufferTex[ 1 ], &gBackBufferTex[ 2 ] );

	// TODO: the backbuffer should probably be created in game code
	gBackBuffer[ 0 ] = render->GetBackBufferColor();
	gBackBuffer[ 1 ] = render->GetBackBufferDepth();

	if ( gBackBuffer[ 0 ] == InvalidHandle || gBackBuffer[ 1 ] == InvalidHandle )
	{
		Log_Fatal( gLC_ClientGraphics, "Failed to get Back Buffer Handles!\n" );
		return false;
	}

	int width, height;
	render->GetSurfaceSize( width, height );
	gViewInfo[ 0 ].aSize = { width, height };

	if ( !Graphics_CreateRenderPasses() )
	{
		return false;
	}
	
	if ( !Graphics_CreateRenderTargets() )
	{
		return false;
	}

	if ( !Graphics_CreateDescriptorSets() )
	{
		return false;
	}

	if ( !Graphics_ShaderInit( false ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Shaders!\n" );
		return false;
	}

	// Create Light Info Buffer
	gLightInfoStagingBuffer = render->CreateBuffer( "Light Info Buffer", sizeof( LightInfo_t ), EBufferFlags_Uniform | EBufferFlags_TransferSrc, EBufferMemory_Host );
	gLightInfoBuffer        = render->CreateBuffer( "Light Info Buffer", sizeof( LightInfo_t ), EBufferFlags_Uniform | EBufferFlags_TransferDst, EBufferMemory_Device );

	if ( gLightInfoBuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Light Info Uniform Buffer\n" );
		return false;
	}

	// update the descriptor sets
	UpdateVariableDescSet_t update{};

	for ( size_t i = 0; i < gUniformLightInfo.aSets.size(); i++ )
		update.aDescSets.push_back( gUniformLightInfo.aSets[ i ] );

	update.aType    = EDescriptorType_UniformBuffer;
	update.aBuffers.push_back( gLightInfoBuffer );
	render->UpdateVariableDescSet( update );

	gDebugLineMaterial = Graphics_CreateMaterial( "__debug_line_mat", Graphics_GetShader( "debug_line" ) );

	// TEMP: make a world light
	gpWorldLight = Graphics_CreateLight( ELightType_Directional );
	gpWorldLight->aColor = { 1.0, 1.0, 1.0 };
	// gpWorldLight->aColor = { 0.1, 0.1, 0.1 };

	return render->InitImGui( gRenderPassGraphics );
	// return render->InitImGui( gRenderPassGraphics );
}


void Graphics_Shutdown()
{
}


void Graphics_Reset()
{
	render->Reset();
}


void Graphics_NewFrame()
{
	render->NewFrame();

	gDebugLineVertPos.clear();
	gDebugLineVertColor.clear();

	if ( !r_debug_draw )
	{
		if ( gDebugLineModel )
		{
			Graphics_FreeModel( gDebugLineModel );
			gDebugLineModel = InvalidHandle;
		}

		if ( gDebugLineDraw )
		{
			Renderable_t* renderable = Graphics_GetRenderableData( gDebugLineDraw );

			if ( !renderable )
				return;

			// Graphics_FreeModel( renderable->aModel );
			Graphics_FreeRenderable( gDebugLineDraw );
			gDebugLineDraw = InvalidHandle;
		}

		return;
	}

	if ( !gDebugLineModel )
	{
		Model* model    = nullptr;
		gDebugLineModel = gModels.Create( &model );

		if ( !gDebugLineModel )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create Debug Line Model\n" );
			return;
		}

		model->aMeshes.resize( 1 );

		model->apVertexData = new VertexData_t;
		model->apBuffers    = new ModelBuffers_t;

		model->apVertexData->AddRef();
		model->apBuffers->AddRef();

		// gpDebugLineModel->apBuffers->aVertex.resize( 2, true );
		model->apVertexData->aData.resize( 2, true );
		model->apVertexData->aData[ 0 ].aAttrib = VertexAttribute_Position;
		model->apVertexData->aData[ 1 ].aAttrib = VertexAttribute_Color;

		Model_SetMaterial( gDebugLineModel, 0, gDebugLineMaterial );

		if ( gDebugLineDraw )
		{
			if ( Renderable_t* renderable = Graphics_GetRenderableData( gDebugLineDraw ) )
			{
				renderable->aModel = gDebugLineDraw;
			}
		}
	}

	if ( !gDebugLineDraw )
	{
		gDebugLineDraw = Graphics_CreateRenderable( gDebugLineModel );

		if ( !gDebugLineDraw )
			return;

		Renderable_t* renderable = Graphics_GetRenderableData( gDebugLineDraw );

		if ( !renderable )
			return;

		renderable->aTestVis    = false;
		renderable->aCastShadow = false;
		renderable->aVisible    = true;
	}
}


// TODO: experiment with instanced drawing
void Graphics_CmdDrawSurface( Handle cmd, Model* spModel, size_t sSurface )
{
	Mesh& mesh = spModel->aMeshes[ sSurface ];

	// TODO: figure out a way to use vertex and index offsets with this vertex format stuff
	// ideally, it would be less vertex buffer binding, but would be harder to pull off
	if ( spModel->apBuffers->aIndex )
		render->CmdDrawIndexed(
		  cmd,
		  mesh.aIndexCount,
		  1,                  // instance count
		  mesh.aIndexOffset,
		  0, // mesh.aVertexOffset,
		  0 );

	else
		render->CmdDraw(
		  cmd,
		  mesh.aVertexCount,
		  1,
		  mesh.aVertexOffset,
		  0 );

	gModelDrawCalls++;
	gVertsDrawn += mesh.aVertexCount;
}


bool Graphics_BindModel( Handle cmd, VertexFormat sVertexFormat, Model* spModel, SurfaceDraw_t& srDrawInfo )
{
	// if ( srDrawInfo.aSurface > spModel->aMeshes.size() )
	// {
	// 	Log_Error( gLC_ClientGraphics, "Graphics_BindModel: model surface index is out of range!\n" );
	// 	return false;
	// }

	// Mesh& mesh = spModel->aMeshes[ srDrawInfo.aSurface ];
	
	// Bind the mesh's vertex and index buffers

	// Get Vertex Buffers the shader wants
	// TODO: what about if we don't have an attribute the shader wants???
	ChVector< Handle > vertexBuffers;

	// TODO: THIS CAN BE DONE WHEN ADDING THE MODEL TO THE MAIN DRAW LIST, AND PUT IN SurfaceDraw_t
	for ( size_t i = 0; i < spModel->apVertexData->aData.size(); i++ )
	{
		VertAttribData_t& data = spModel->apVertexData->aData[ i ];

		if ( sVertexFormat & ( 1 << data.aAttrib ) )
			vertexBuffers.push_back( spModel->apBuffers->aVertex[ i ] );
	}

	size_t* offsets = (size_t*)CH_STACK_ALLOC( sizeof( size_t ) * vertexBuffers.size() );
	if ( offsets == nullptr )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_BindModel: Failed to allocate vertex buffer offsets!\n" );
		return false;
	}

	// TODO: i could probably use offsets here, i imagine it might actually be faster?
	memset( offsets, 0, sizeof( size_t ) * vertexBuffers.size() );

	render->CmdBindVertexBuffers( cmd, 0, vertexBuffers.size(), vertexBuffers.data(), offsets );

	// TODO: store index type here somewhere
	if ( spModel->apBuffers->aIndex )
		render->CmdBindIndexBuffer( cmd, spModel->apBuffers->aIndex, 0, EIndexType_U32 );

	CH_STACK_FREE( offsets );
	return true;
}


extern ConVar r_fov;


// https://iquilezles.org/articles/frustumcorrect/
bool Frustum_t::IsBoxVisible( const glm::vec3& sMin, const glm::vec3& sMax ) const
{
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


void Graphics_CreateFrustum( Frustum_t& srFrustum, const glm::mat4& srViewMat )
{
	glm::mat4 m                          = glm::transpose( srViewMat );
	glm::mat4 inv                        = glm::inverse( srViewMat );

	srFrustum.aPlanes[ EFrustum_Left ]   = m[ 3 ] + m[ 0 ];
	srFrustum.aPlanes[ EFrustum_Right ]  = m[ 3 ] - m[ 0 ];
	srFrustum.aPlanes[ EFrustum_Bottom ] = m[ 3 ] + m[ 1 ];
	srFrustum.aPlanes[ EFrustum_Top ]    = m[ 3 ] - m[ 1 ];
	srFrustum.aPlanes[ EFrustum_Near ]   = m[ 3 ] + m[ 2 ];
	srFrustum.aPlanes[ EFrustum_Far ]    = m[ 3 ] - m[ 2 ];

	// Calculate Frustum Points
	for ( int i = 0; i < 8; i++ )
	{
		glm::vec4 ff             = inv * gFrustumFaceData[ i ];
		srFrustum.aPoints[ i ].x = ff.x / ff.w;
		srFrustum.aPoints[ i ].y = ff.y / ff.w;
		srFrustum.aPoints[ i ].z = ff.z / ff.w;
	}
}


Frustum_t Graphics_CreateFrustum( const glm::mat4& srViewInfo )
{
	Frustum_t frustum;
	Graphics_CreateFrustum( frustum, srViewInfo );
	return frustum;
}


ModelBBox_t Graphics_CreateWorldAABB( glm::mat4& srMatrix, const ModelBBox_t& srBBox )
{
	glm::vec4 corners[ 8 ];

	// Fill array with the corners of the AABB 
	corners[ 0 ] = { srBBox.aMin.x, srBBox.aMin.y, srBBox.aMin.z, 1.f };
	corners[ 1 ] = { srBBox.aMin.x, srBBox.aMin.y, srBBox.aMax.z, 1.f };
	corners[ 2 ] = { srBBox.aMin.x, srBBox.aMax.y, srBBox.aMin.z, 1.f };
	corners[ 3 ] = { srBBox.aMax.x, srBBox.aMin.y, srBBox.aMin.z, 1.f };
	corners[ 4 ] = { srBBox.aMin.x, srBBox.aMax.y, srBBox.aMax.z, 1.f };
	corners[ 5 ] = { srBBox.aMax.x, srBBox.aMin.y, srBBox.aMax.z, 1.f };
	corners[ 6 ] = { srBBox.aMax.x, srBBox.aMax.y, srBBox.aMin.z, 1.f };
	corners[ 7 ] = { srBBox.aMax.x, srBBox.aMax.y, srBBox.aMax.z, 1.f };

	glm::vec3 globalMin;
	glm::vec3 globalMax;

	// Transform all of the corners, and keep track of the greatest and least
	// values we see on each coordinate axis.
	for ( int i = 0; i < 8; i++ )
	{
		glm::vec3 transformed = srMatrix * corners[ i ];

		if ( i > 0 )
		{
			globalMin = glm::min( globalMin, transformed );
			globalMax = glm::max( globalMax, transformed );
		}
		else
		{
			globalMin = transformed;
			globalMax = transformed;
		}
	}

	ModelBBox_t aabb( globalMin, globalMax );
	return aabb;
}


bool Graphics_ViewFrustumTest( Renderable_t* spModelDraw, int sViewInfoIndex )
{
	if ( !spModelDraw )
		return false;

	if ( gViewInfo.size() <= sViewInfoIndex || !r_vis || !spModelDraw->aTestVis )
		return true;

	if ( !spModelDraw->aVisible )
		return false;

	ViewInfo_t& viewInfo = gViewInfo[ sViewInfoIndex ];

	if ( !viewInfo.aActive )
		return false;

	Frustum_t& frustum  = gViewInfoFrustums[ sViewInfoIndex ];

	return frustum.IsBoxVisible( spModelDraw->aAABB.aMin, spModelDraw->aAABB.aMax );
}


void Graphics_DrawShaderRenderables( Handle cmd, Handle shader, ChVector< SurfaceDraw_t >& srRenderList )
{
	if ( !Shader_Bind( cmd, gCmdIndex, shader ) )
	{
		Log_ErrorF( gLC_ClientGraphics, "Failed to bind shader: %s\n", Graphics_GetShaderName( shader ) );
		return;
	}

	SurfaceDraw_t* prevRenderable = nullptr;
	Model*         prevModel      = nullptr;

	ShaderData_t*  shaderData     = Shader_GetData( shader );
	if ( !shaderData )
		return;

	if ( shaderData->aDynamicState & EDynamicState_LineWidth )
		render->CmdSetLineWidth( cmd, r_line_thickness );

	VertexFormat   vertexFormat   = Shader_GetVertexFormat( shader );

	for ( uint32_t i = 0; i < srRenderList.size(); )
	{
		auto& renderable = srRenderList[ i ];

		Renderable_t* modelDraw = nullptr;
		if ( !gRenderables.Get( renderable.aDrawData, &modelDraw ) )
		{
			Log_Warn( gLC_ClientGraphics, "Draw Data does not exist for renderable!\n" );
			srRenderList.remove( i );
			continue;
		}

		// get model and check if it's nullptr
		if ( modelDraw->aModel == InvalidHandle )
		{
			Log_Error( gLC_ClientGraphics, "Graphics_DrawShaderRenderables: model handle is InvalidHandle\n" );
			srRenderList.remove( i );
			continue;
		}

		// get model data
		Model* model = nullptr;
		if ( !gModels.Get( modelDraw->aModel, &model ) )
		{
			Log_Error( gLC_ClientGraphics, "Graphics_DrawShaderRenderables: model is nullptr\n" );
			srRenderList.remove( i );
			continue;
		}

		// make sure this model has valid vertex buffers
		if ( model->apBuffers == nullptr || model->apBuffers->aVertex.empty() )
		{
			Log_Error( gLC_ClientGraphics, "No Vertex/Index Buffers for Model??\n" );
			srRenderList.remove( i );
			continue;
		}

		bool bindModel = !prevRenderable;

		if ( prevRenderable )
		{
			// bindModel |= prevRenderable->apDraw->aModel != renderable->apDraw->aModel;
			// bindModel |= prevRenderable->aSurface != renderable->aSurface;

			if ( prevModel )
			{
				bindModel |= prevModel->apBuffers != model->apBuffers;
				bindModel |= prevModel->apVertexData != model->apVertexData;
			}
		}

		if ( bindModel )
		{
			prevModel      = model;
			prevRenderable = &renderable;
			if ( !Graphics_BindModel( cmd, vertexFormat, model, renderable ) )
				continue;
		}

		// NOTE: not needed if the material is the same i think
		if ( !Shader_PreRenderableDraw( cmd, gCmdIndex, shader, renderable ) )
			continue;

		Graphics_CmdDrawSurface( cmd, model, renderable.aSurface );
		i++;
	}
}


void Graphics_RenderShadowMap( Handle cmd, Light_t* spLight, const ShadowMap_t& srShadowMap )
{
	Rect2D_t rect{};
	rect.aOffset.x = 0;
	rect.aOffset.y = 0;
	rect.aExtent.x = srShadowMap.aSize.x;
	rect.aExtent.y = srShadowMap.aSize.y;

	render->CmdSetScissor( cmd, 0, &rect, 1 );

	Viewport_t viewPort{};
	viewPort.x        = 0.f;
	viewPort.y        = 0.f;
	viewPort.minDepth = 0.f;
	viewPort.maxDepth = 1.f;
	viewPort.width    = srShadowMap.aSize.x;
	viewPort.height   = srShadowMap.aSize.y;

	render->CmdSetViewport( cmd, 0, &viewPort, 1 );

	render->CmdSetDepthBias( cmd, r_shadowmap_constant, r_shadowmap_clamp, r_shadowmap_slope );

	// HACK: need to setup a view push and pop system?
	Shader_ShadowMap_SetViewInfo( srShadowMap.aViewInfoIndex );

	ViewRenderList_t& viewList = gViewRenderLists[ srShadowMap.aViewInfoIndex ];

	for ( auto& [ shader, renderList ] : viewList.aRenderLists )
	{
		Graphics_DrawShaderRenderables( cmd, shader, renderList );
	}
}


// Do Rendering with shader system and user land meshes
void Graphics_RenderView( Handle cmd, ViewRenderList_t& srViewList )
{
	// here we go again
	static Handle skybox    = Graphics_GetShader( "skybox" );

	bool          hasSkybox = false;

	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	Rect2D_t rect{};
	rect.aOffset.x = 0;
	rect.aOffset.y = 0;
	rect.aExtent.x = width;
	rect.aExtent.y = height;

	render->CmdSetScissor( cmd, 0, &rect, 1 );

	// flip viewport
	Viewport_t viewPort{};
	viewPort.x        = 0.f;
	viewPort.y        = height;
	viewPort.minDepth = 0.f;
	viewPort.maxDepth = 1.f;
	viewPort.width    = width;
	viewPort.height   = height * -1.f;

	render->CmdSetViewport( cmd, 0, &viewPort, 1 );

	for ( auto& [ shader, renderList ] : srViewList.aRenderLists )
	{
		if ( shader == InvalidHandle )
		{
			Log_Warn( gLC_ClientGraphics, "Invalid Shader Handle (0) in View RenderList\n" );
			continue;
		}

		if ( shader == skybox )
		{
			hasSkybox = true;
			continue;
		}

		Graphics_DrawShaderRenderables( cmd, shader, renderList );
	}

	// Draw Skybox - and set depth for skybox
	if ( hasSkybox )
	{
		viewPort.minDepth = 0.999f;
		viewPort.maxDepth = 1.f;

		render->CmdSetViewport( cmd, 0, &viewPort, 1 );

		Graphics_DrawShaderRenderables( cmd, skybox, srViewList.aRenderLists[ skybox ] );
	}
}


void Graphics_Render( Handle cmd )
{
	for ( size_t i = 0; i < gViewRenderLists.size(); i++ )
	{
		// HACK HACK !!!!
		// don't render views with shader overrides here, the only override is the shadow map shader
		// and that is rendered in a separate render pass
		if ( gViewInfo[ i ].aShaderOverride )
			continue;

		gViewInfoIndex = i;
		Graphics_RenderView( cmd, gViewRenderLists[ i ] );
	}
}


void Graphics_AddToViewRenderList()
{
}


static void Graphics_UpdateDebugDraw()
{
	if ( r_debug_aabb )
	{
		ViewRenderList_t& viewList = gViewRenderLists[ 0 ];

		for ( auto& [ shader, modelList ] : viewList.aRenderLists )
		{
			for ( auto& surfaceDraw : modelList )
			{
				// hack to not draw this AABB multiple times, need to change this render list system
				if ( surfaceDraw.aSurface == 0 )
				{
					// Graphics_DrawModelAABB( renderable->apDraw );

					Renderable_t* modelDraw = nullptr;
					if ( !gRenderables.Get( surfaceDraw.aDrawData, &modelDraw ) )
					{
						Log_Warn( gLC_ClientGraphics, "Draw Data does not exist for renderable!\n" );
						return;
					}

					Graphics_DrawBBox( modelDraw->aAABB.aMin, modelDraw->aAABB.aMax, { 1.0, 0.5, 1.0 } );

					// ModelBBox_t& bbox = gModelBBox[ renderable->apDraw->aModel ];
					// Graphics_DrawBBox( bbox.aMin, bbox.aMax, { 1.0, 0.5, 1.0 } );
				}
			}
		}
	}

	if ( r_debug_draw && gDebugLineModel && gDebugLineVertPos.size() )
	{
		// Mesh& mesh = gDebugLineModel->aMeshes[ 0 ];

		Model* model = nullptr;
		if ( !gModels.Get( gDebugLineModel, &model ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to get Debug Draw Model!\n" );
			gDebugLineModel = InvalidHandle;
			return;
		}

		if ( !model->apVertexData )
		{
			model->apVertexData = new VertexData_t;
			model->apVertexData->AddRef();
		}

		if ( !model->apBuffers )
		{
			model->apBuffers = new ModelBuffers_t;
			model->apBuffers->AddRef();
		}

		// Is our current buffer size too small? If so, free the old ones
		if ( gDebugLineVertPos.size() > gDebugLineBufferSize )
		{
			if ( model->apBuffers && model->apBuffers->aVertex.size() )
			{
				render->DestroyBuffer( model->apBuffers->aVertex[ 0 ] );
				render->DestroyBuffer( model->apBuffers->aVertex[ 1 ] );
				model->apBuffers->aVertex.clear();
			}

			gDebugLineBufferSize = gDebugLineVertPos.size();
		}

		size_t bufferSize = ( 3 * sizeof( float ) ) * gDebugLineVertPos.size();

		// Create new Buffers if needed
		if ( model->apBuffers->aVertex.empty() )
		{
			model->apBuffers->aVertex.resize( 2 );
			model->apBuffers->aVertex[ 0 ]            = render->CreateBuffer( "DebugLine Position", bufferSize, EBufferFlags_Vertex, EBufferMemory_Host );
			model->apBuffers->aVertex[ 1 ] = render->CreateBuffer( "DebugLine Color", bufferSize, EBufferFlags_Vertex, EBufferMemory_Host );
		}

		model->apVertexData->aCount = gDebugLineVertPos.size();

		if ( model->aMeshes.empty() )
			model->aMeshes.resize( 1 );

		model->aMeshes[ 0 ].aIndexCount              = 0;
		model->aMeshes[ 0 ].aVertexOffset           = 0;
		model->aMeshes[ 0 ].aVertexCount             = gDebugLineVertPos.size();

		// Update the Buffers
		render->MemWriteBuffer( model->apBuffers->aVertex[ 0 ], bufferSize, gDebugLineVertPos.data() );
		render->MemWriteBuffer( model->apBuffers->aVertex[ 1 ], bufferSize, gDebugLineVertColor.data() );
	}
}


void Graphics_PrepareDrawData()
{
	// fun
	static Handle        shadow_map       = Graphics_GetShader( "__shadow_map" );
	static ShaderData_t* shadowShaderData = Shader_GetData( shadow_map );

	render->PreRenderPass();
	
	// glm::vec3 ang( r_world_dir_x.GetFloat(), r_world_dir_y.GetFloat(), r_world_dir_z.GetFloat() );
	// 
	// Util_GetDirectionVectors( ang, nullptr, nullptr, &gpWorldLight->aDir );
	// 
	// Graphics_UpdateLight( gpWorldLight );
	// 
	// ImGui::Text( Vec2Str( gpWorldLight->aDir ).c_str() );

	ImGui::Text( "Model Draw Calls: %zd", gModelDrawCalls );
	ImGui::Text( "Verts Drawn: %zd", gVertsDrawn );
	ImGui::Text( "Debug Line Verts: %zd", gDebugLineVertPos.size() );

	ImGui::Render();

	gModelDrawCalls = 0;
	gVertsDrawn     = 0;

	for ( const auto& mat : gDirtyMaterials )
	{
		Handle shader = Mat_GetShader( mat );

		// HACK HACK
		if ( Graphics_GetShader( "basic_3d" ) == shader )
			Shader_Basic3D_UpdateMaterialData( mat );
	}

	gDirtyMaterials.clear();

	// update renderable AABB's
	for ( auto& [ renderHandle, bbox ] : gRenderAABBUpdate )
	{
		if ( Renderable_t* renderable = Graphics_GetRenderableData( renderHandle ) )
			renderable->aAABB = Graphics_CreateWorldAABB( renderable->aModelMatrix, bbox );
	}

	gRenderAABBUpdate.clear();

	// Destroy lights if needed
	for ( Light_t* light : gDestroyLights )
		Graphics_DestroyLightBuffer( light );

	// Update UBO's for lights if needed
	for ( Light_t* light : gDirtyLights )
		Graphics_UpdateLightBuffer( light );

	gDestroyLights.clear();
	gDirtyLights.clear();

	// Update Light Info UBO
	if ( gNeedLightInfoUpdate )
	{
		gNeedLightInfoUpdate = false;
		render->MemWriteBuffer( gLightInfoStagingBuffer, sizeof( LightInfo_t ), &gLightInfo );
		render->MemCopyBuffer( gLightInfoStagingBuffer, gLightInfoBuffer, sizeof( LightInfo_t ) );
	}

	if ( gViewInfoUpdate )
	{
		gViewInfoUpdate = false;
		// for ( size_t i = 0; i < gViewInfoBuffers.size(); i++ )
		for ( size_t i = 0; i < 1; i++ )
		{
			render->MemWriteBuffer( gViewInfoBuffers[ i ], sizeof( UBO_ViewInfo_t ), &gViewInfo[ i ] );
		}
	}

	// update view frustums (CHANGE THIS, SHOULD NOT UPDATE EVERY SINGLE ONE PER FRAME  !!!!)
	if ( !r_vis_lock.GetBool() || gViewInfoFrustums.size() != gViewInfo.size() )
	{
		gViewInfoFrustums.resize( gViewInfo.size() );

		for ( size_t i = 0; i < gViewInfo.size(); i++ )
		{
			Graphics_CreateFrustum( gViewInfoFrustums[ i ], gViewInfo[ i ].aProjView );

			if ( r_debug_frustums )
				Graphics_DrawFrustum( gViewInfoFrustums[ i ] );
		}
	}

	// --------------------------------------------------------------------

	Shader_ResetPushData();

	bool usingShadow = false;
	for ( const auto& [ light, shadowMap ] : gLightShadows )
	{
		if ( !light->aEnabled || !light->aShadow )
			continue;

		usingShadow = true;
		break;
	}

	// --------------------------------------------------------------------
	// Prepare View Render Lists

	if ( !r_vis_lock.GetBool() )
	{
		for ( ViewRenderList_t& viewList : gViewRenderLists )
			for ( auto& [ handle, vec ] : viewList.aRenderLists )
				vec.clear();

		gViewRenderLists.resize( gViewInfoCount );

		for ( uint32_t i = 0; i < gRenderables.size(); )
		{
			Renderable_t* modelDraw = nullptr;
			if ( !gRenderables.Get( gRenderables.aHandles[ i ], &modelDraw ) )
			{
				Log_Warn( gLC_ClientGraphics, "ModelDraw handle is invalid!\n" );
				gRenderables.Remove( gRenderables.aHandles[ i ] );
				continue;
			}

			if ( !modelDraw->aVisible )
			{
				i++;
				continue;
			}

			Model* model = nullptr;
			if ( !gModels.Get( modelDraw->aModel, &model ) )
			{
				Log_Warn( gLC_ClientGraphics, "Renderable has no model!\n" );
				gRenderables.Remove( gRenderables.aHandles[ i ] );
				continue;
			}

			// check if we need this in any views
			for ( int viewIndex = 0; viewIndex < gViewInfoCount; viewIndex++ )
			{
				// HACK: kind of of hack with the shader override check
				// If we don't want to cast a shadow and are in a shadowmap view, don't add to the view's render list
				if ( !modelDraw->aCastShadow && gViewInfo[ viewIndex ].aShaderOverride )
					continue;

				// Is this model visible in this view?
				if ( !Graphics_ViewFrustumTest( modelDraw, viewIndex ) )
					continue;

				// Add each surface to the shader draw list
				for ( uint32_t surf = 0; surf < model->aMeshes.size(); surf++ )
				{
					Handle mat = model->aMeshes[ surf ].aMaterial;

					// TODO: add Mat_IsValid()
					if ( mat == InvalidHandle )
					{
						Log_ErrorF( gLC_ClientGraphics, "Model part \"%d\" has no material!\n", surf );
						// gModelDrawList.remove( i );
						continue;
					}

					// Handle shader = InvalidHandle;
					Handle shader = gViewInfo[ viewIndex ].aShaderOverride;

					if ( !shader )
						shader = Mat_GetShader( mat );

					// add a SurfaceDraw_t to this render list
					SurfaceDraw_t& surfDraw = gViewRenderLists[ viewIndex ].aRenderLists[ shader ].emplace_back();
					surfDraw.aDrawData      = gRenderables.aHandles[ i ];
					surfDraw.aSurface       = surf;
				}
			}

			i++;
		}
	}

	// --------------------------------------------------------------------
	// Update Debug Draw Buffers
	// TODO: replace this system with instanced drawing

	if ( r_debug_draw )
	{
		Graphics_UpdateDebugDraw();
	}

	// --------------------------------------------------------------------
	// Update Shader Draw Data

	for ( int viewIndex = 0; viewIndex < gViewInfoCount; viewIndex++ )
	{
		ViewRenderList_t& viewList = gViewRenderLists[ viewIndex ];

		for ( auto& [ shader, modelList ] : viewList.aRenderLists )
		{
			ShaderData_t* shaderData = Shader_GetData( shader );
			if ( !shaderData )
				continue;

			for ( auto& renderable : modelList )
			{
				Renderable_t* modelDraw = nullptr;
				if ( !gRenderables.Get( renderable.aDrawData, &modelDraw ) )
				{
					Log_Warn( gLC_ClientGraphics, "Draw Data does not exist for renderable!\n" );
					continue;
				}

				Shader_SetupRenderableDrawData( modelDraw, shaderData, renderable );

				if ( !modelDraw->aCastShadow )
					continue;

				if ( shaderData->aFlags & EShaderFlags_Lights && usingShadow && shadowShaderData )
					Shader_SetupRenderableDrawData( modelDraw, shadowShaderData, renderable );
			}
		}
	}

#if 0
	for ( auto& [ shader, modelList ] : gModelDrawList )
	{
		ShaderData_t* shaderData = Shader_GetData( shader );
		if ( !shaderData )
			continue;

		for ( auto& renderable : modelList )
		{
			// check if we need this in any views
			bool visible = true;
			for ( int viewIndex = 0; viewIndex < gViewInfoCount; viewIndex++ )
			{
				bool viewVisible = Graphics_ViewFrustumTest( renderable, viewIndex );
				visible |= viewVisible;

				if ( !viewVisible )
					continue;

				gViewRenderLists[ viewIndex ].aRenderLists[ shader ].push_back( &renderable );

				if ( viewIndex == 0 )
					Shader_SetupRenderableDrawData( shaderData, renderable );
			}

			if ( !visible || !renderable.apDraw->aCastShadow )
				continue;

			if ( shaderData->aFlags & EShaderFlags_Lights && usingShadow && shadowShaderData )
				Shader_SetupRenderableDrawData( shadowShaderData, renderable );
		}
	}
#endif
}


void Graphics_Present()
{
	// render->LockGraphicsMutex();
	render->WaitForQueues();
	render->ResetCommandPool();

	Graphics_PrepareDrawData();

	// For each framebuffer, begin a primary
	// command buffer, and record the commands.
	for ( gCmdIndex = 0; gCmdIndex < gCommandBuffers.size(); gCmdIndex++ )
	{
		auto c = gCommandBuffers[ gCmdIndex ];

		// for ( Light_t* light : gLights )
		// 	Graphics_UpdateLightBuffer( light );

		render->BeginCommandBuffer( c );

		RenderPassBegin_t renderPassBegin{};
		renderPassBegin.aClear.resize( 1 );
		renderPassBegin.aClear[ 0 ].aColor   = { 0.f, 0.f, 0.f, 1.f };
		renderPassBegin.aClear[ 0 ].aIsDepth = true;

		for ( const auto& [ light, shadowMap ] : gLightShadows )
		{
			if ( !light->aEnabled || !light->aShadow )
				continue;

			renderPassBegin.aRenderPass  = gRenderPassShadow;
			renderPassBegin.aFrameBuffer = shadowMap.aFramebuffer;

			if ( renderPassBegin.aFrameBuffer == InvalidHandle )
				continue;

			gCurFramebuffer = renderPassBegin.aFrameBuffer;
			gViewInfoIndex  = shadowMap.aViewInfoIndex;

			render->BeginRenderPass( c, renderPassBegin );
			Graphics_RenderShadowMap( c, light, shadowMap );
			render->EndRenderPass( c );
		}

		// ----------------------------------------------------------
		// Main RenderPass

		renderPassBegin.aRenderPass  = gRenderPassGraphics;
		renderPassBegin.aFrameBuffer = gBackBuffer[ gCmdIndex ];
		renderPassBegin.aClear.resize( 2 );
		renderPassBegin.aClear[ 0 ].aColor   = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear[ 0 ].aIsDepth = false;
		renderPassBegin.aClear[ 1 ].aColor   = { 0.f, 0.f, 0.f, 1.f };
		renderPassBegin.aClear[ 1 ].aIsDepth = true;

		render->BeginRenderPass( c, renderPassBegin );  // VK_SUBPASS_CONTENTS_INLINE

		gCurFramebuffer = renderPassBegin.aFrameBuffer;
		gViewInfoIndex  = 0;

		Graphics_Render( c );
		render->DrawImGui( ImGui::GetDrawData(), c );

		render->EndRenderPass( c );

		render->EndCommandBuffer( c );
	}

	gCurFramebuffer = InvalidHandle;

	render->Present();
	// render->UnlockGraphicsMutex();
}


void Graphics_SetViewProjMatrix( const glm::mat4& srMat )
{
	gViewInfo[ 0 ].aProjView = srMat;
	gViewProjMat = srMat;
	gViewInfoUpdate = true;
}


const glm::mat4& Graphics_GetViewProjMatrix()
{
	return gViewProjMat;
}


void Graphics_PushViewInfo( const ViewInfo_t& srViewInfo )
{
	gViewInfoStack.push( srViewInfo );
}


void Graphics_PopViewInfo()
{
	if ( gViewInfoStack.empty() )
	{
		Log_Error( "Misplaced View Info Pop!\n" );
		return;
	}

	gViewInfoStack.pop();
}


ViewInfo_t& Graphics_GetViewInfo()
{
	if ( gViewInfoStack.empty() )
		return gViewInfo[ 0 ];

	return gViewInfoStack.top();
}



Handle Graphics_CreateRenderable( Handle sModel )
{
	Model* model = nullptr;
	if ( !gModels.Get( sModel, &model ) )
	{
		Log_Warn( gLC_ClientGraphics, "Renderable has no model!\n" );
		return InvalidHandle;
	}

	Renderable_t* modelDraw  = nullptr;
	Handle        drawHandle = InvalidHandle;

	if ( !( drawHandle = gRenderables.Create( &modelDraw ) ) )
	{
		Log_ErrorF( gLC_ClientGraphics, "Failed to create Renderable_t\n" );
		return InvalidHandle;
	}

	modelDraw->aModel              = sModel;
	modelDraw->aModelMatrix        = glm::identity< glm::mat4 >();
	modelDraw->aTestVis            = true;
	modelDraw->aCastShadow         = true;
	modelDraw->aVisible            = true;

	// memset( &modelDraw->aAABB, 0, sizeof( ModelBBox_t ) );
	// Graphics_UpdateModelAABB( modelDraw );
	modelDraw->aAABB               = Graphics_CreateWorldAABB( modelDraw->aModelMatrix, gModelBBox[ modelDraw->aModel ] );

	return drawHandle;
}


Renderable_t* Graphics_GetRenderableData( Handle sRenderable )
{
	Renderable_t* renderable = nullptr;
	if ( !gRenderables.Get( sRenderable, &renderable ) )
	{
		Log_Warn( gLC_ClientGraphics, "Failed to find Renderable!\n" );
		return nullptr;
	}

	return renderable;
}


void Graphics_FreeRenderable( Handle sRenderable )
{
	if ( !sRenderable )
		return;

	gRenderables.Remove( sRenderable );
}


void Graphics_DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor )
{
	if ( !r_debug_draw || !gDebugLineModel )
		return;

	gDebugLineVertPos.push_back( sX );
	gDebugLineVertPos.push_back( sY );

	gDebugLineVertColor.push_back( sColor );
	gDebugLineVertColor.push_back( sColor );
}


#if 0
void Graphics_DrawAxis( const glm::vec3& sPos, const glm::vec3& sAng, const glm::vec3& sScale )
{
	if ( !r_debug_draw || !gDebugLineModel )
		return;
}
#endif


void Graphics_DrawBBox( const glm::vec3& sMin, const glm::vec3& sMax, const glm::vec3& sColor )
{
	if ( !r_debug_draw || !gDebugLineModel )
		return;

	// bottom
	Graphics_DrawLine( sMin, glm::vec3( sMax.x, sMin.y, sMin.z ), sColor );
	Graphics_DrawLine( sMin, glm::vec3( sMin.x, sMax.y, sMin.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMin.x, sMax.y, sMin.z ), glm::vec3( sMax.x, sMax.y, sMin.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMax.x, sMin.y, sMin.z ), glm::vec3( sMax.x, sMax.y, sMin.z ), sColor );

	// top
	Graphics_DrawLine( sMax, glm::vec3( sMin.x, sMax.y, sMax.z ), sColor );
	Graphics_DrawLine( sMax, glm::vec3( sMax.x, sMin.y, sMax.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMax.x, sMin.y, sMax.z ), glm::vec3( sMin.x, sMin.y, sMax.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMin.x, sMax.y, sMax.z ), glm::vec3( sMin.x, sMin.y, sMax.z ), sColor );

	// sides
	Graphics_DrawLine( sMin, glm::vec3( sMin.x, sMin.y, sMax.z ), sColor );
	Graphics_DrawLine( sMax, glm::vec3( sMax.x, sMax.y, sMin.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMax.x, sMin.y, sMin.z ), glm::vec3( sMax.x, sMin.y, sMax.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMin.x, sMax.y, sMin.z ), glm::vec3( sMin.x, sMax.y, sMax.z ), sColor );
}


void Graphics_DrawProjView( const glm::mat4& srProjView )
{
	if ( !r_debug_draw || !gDebugLineModel )
		return;

	glm::mat4 inv = glm::inverse( srProjView );

	// Calculate Frustum Points
	glm::vec3 v[ 8u ];
	for ( int i = 0; i < 8; i++ )
	{
		glm::vec4 ff = inv * gFrustumFaceData[ i ];
		v[ i ].x     = ff.x / ff.w;
		v[ i ].y     = ff.y / ff.w;
		v[ i ].z     = ff.z / ff.w;
	}

	Graphics_DrawLine( v[ 0 ], v[ 1 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 0 ], v[ 2 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 3 ], v[ 1 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 3 ], v[ 2 ], glm::vec3( 1, 1, 1 ) );

	Graphics_DrawLine( v[ 4 ], v[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 4 ], v[ 6 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 7 ], v[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 7 ], v[ 6 ], glm::vec3( 1, 1, 1 ) );

	Graphics_DrawLine( v[ 0 ], v[ 4 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 1 ], v[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 3 ], v[ 7 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 2 ], v[ 6 ], glm::vec3( 1, 1, 1 ) );
}


void Graphics_DrawFrustum( const Frustum_t& srFrustum )
{
	if ( !r_debug_draw || !gDebugLineModel )
		return;

	Graphics_DrawLine( srFrustum.aPoints[ 0 ], srFrustum.aPoints[ 1 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 0 ], srFrustum.aPoints[ 2 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 3 ], srFrustum.aPoints[ 1 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 3 ], srFrustum.aPoints[ 2 ], glm::vec3( 1, 1, 1 ) );

	Graphics_DrawLine( srFrustum.aPoints[ 4 ], srFrustum.aPoints[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 4 ], srFrustum.aPoints[ 6 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 7 ], srFrustum.aPoints[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 7 ], srFrustum.aPoints[ 6 ], glm::vec3( 1, 1, 1 ) );

	Graphics_DrawLine( srFrustum.aPoints[ 0 ], srFrustum.aPoints[ 4 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 1 ], srFrustum.aPoints[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 3 ], srFrustum.aPoints[ 7 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 2 ], srFrustum.aPoints[ 6 ], glm::vec3( 1, 1, 1 ) );
}


GraphicsFmt Graphics_GetVertexAttributeFormat( VertexAttribute attrib )
{
	switch ( attrib )
	{
		default:
			Log_ErrorF( gLC_ClientGraphics, "GetVertexAttributeFormat: Invalid VertexAttribute specified: %d\n", attrib );
			return GraphicsFmt::INVALID;

		case VertexAttribute_Position:
			return GraphicsFmt::RGB323232_SFLOAT;

		// NOTE: could be smaller probably
		case VertexAttribute_Normal:
			return GraphicsFmt::RGB323232_SFLOAT;

		case VertexAttribute_Color:
			return GraphicsFmt::RGB323232_SFLOAT;

		case VertexAttribute_TexCoord:
			return GraphicsFmt::RG3232_SFLOAT;

		// case VertexAttribute_MorphPos:
		// 	return GraphicsFmt::RGB323232_SFLOAT;
	}
}


size_t Graphics_GetVertexAttributeTypeSize( VertexAttribute attrib )
{
	GraphicsFmt format = Graphics_GetVertexAttributeFormat( attrib );

	switch ( format )
	{
		default:
			Log_ErrorF( gLC_ClientGraphics, "GetVertexAttributeTypeSize: Invalid DataFormat specified from vertex attribute: %d\n", format );
			return 0;

		case GraphicsFmt::INVALID:
			return 0;

		case GraphicsFmt::RGB323232_SFLOAT:
		case GraphicsFmt::RG3232_SFLOAT:
			return sizeof( float );
	}
}


size_t Graphics_GetVertexAttributeSize( VertexAttribute attrib )
{
	GraphicsFmt format = Graphics_GetVertexAttributeFormat( attrib );

	switch ( format )
	{
		default:
			Log_ErrorF( gLC_ClientGraphics, "GetVertexAttributeSize: Invalid DataFormat specified from vertex attribute: %d\n", format );
			return 0;

		case GraphicsFmt::INVALID:
			return 0;

		case GraphicsFmt::RGB323232_SFLOAT:
			return ( 3 * sizeof( float ) );

		case GraphicsFmt::RG3232_SFLOAT:
			return ( 2 * sizeof( float ) );
	}
}


size_t Graphics_GetVertexFormatSize( VertexFormat format )
{
	size_t size = 0;

	for ( int attrib = 0; attrib < VertexAttribute_Count; attrib++ )
	{
		// does this format contain this attribute?
		// if so, add the attribute size to it
		if ( format & ( 1 << attrib ) )
			size += Graphics_GetVertexAttributeSize( (VertexAttribute)attrib );
	}

	return size;
}


void Graphics_GetVertexBindingDesc( VertexFormat format, std::vector< VertexInputBinding_t >& srAttrib )
{
	u32 binding = 0;

	for ( u8 attrib = 0; attrib < VertexAttribute_Count; attrib++ )
	{
		// does this format contain this attribute?
		// if so, add this attribute to the vector
		if ( format & ( 1 << attrib ) )
		{
			srAttrib.emplace_back(
			  binding++,
			  (u32)Graphics_GetVertexAttributeSize( (VertexAttribute)attrib ),
			  false );
		}
	}
}


void Graphics_GetVertexAttributeDesc( VertexFormat format, std::vector< VertexInputAttribute_t >& srAttrib )
{
	u32  location   = 0;
	u32  binding    = 0;
	u32  offset     = 0;

	for ( u8 attrib = 0; attrib < VertexAttribute_Count; attrib++ )
	{
		// does this format contain this attribute?
		// if so, add this attribute to the vector
		if ( format & ( 1 << attrib ) )
		{
			srAttrib.emplace_back(
			  location++,
			  binding++,
			  Graphics_GetVertexAttributeFormat( (VertexAttribute)attrib ),
			  0  // no offset
			);
		}
	}
}


const char* Graphics_GetVertexAttributeName( VertexAttribute attrib )
{
	switch ( attrib )
	{
		default:
		case VertexAttribute_Count:
			return "ERROR";

		case VertexAttribute_Position:
			return "Position";

		case VertexAttribute_Normal:
			return "Normal";

		case VertexAttribute_TexCoord:
			return "TexCoord";

		case VertexAttribute_Color:
			return "Color";
	}
}


// ---------------------------------------------------------------------------------------
// Buffers

// sBufferSize is sizeof(element) * count
static Handle CreateModelBuffer( const char* spName, void* spData, size_t sBufferSize, EBufferFlags sUsage )
{
	Handle stagingBuffer = render->CreateBuffer( "Staging Model Buffer", sBufferSize, sUsage | EBufferFlags_TransferSrc, EBufferMemory_Host );

	// Copy Data to Buffer
	render->MemWriteBuffer( stagingBuffer, sBufferSize, spData );

	Handle deviceBuffer = render->CreateBuffer( spName, sBufferSize, sUsage | EBufferFlags_TransferDst, EBufferMemory_Device );

	// Copy Local Buffer data to Device
	render->MemCopyBuffer( stagingBuffer, deviceBuffer, sBufferSize );

	render->DestroyBuffer( stagingBuffer );

	return deviceBuffer;
}


void Graphics_CreateVertexBuffers( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName )
{
	if ( spVertexData == nullptr || spVertexData->aCount == 0 )
	{
		Log_Warn( gLC_ClientGraphics, "Trying to create Vertex Buffers for mesh with no vertices!\n" );
		return;
	}

	if ( spBuffer == nullptr )
	{
		Log_Warn( gLC_ClientGraphics, "Graphics_CreateVertexBuffers: ModelBuffers_t is nullptr!\n" );
		return;
	}

	// VertexFormat shaderFormat = Mat_GetVertexFormat( srMesh.aMaterial );
	// 
	// if ( shaderFormat == VertexFormat_None )
	// {
	// 	Log_Error( gLC_ClientGraphics, "No Vertex Format for shader!\n" );
	// 	return;
	// }

	// Get Attributes the shader wants
	// TODO: what about if we don't have an attribute the shader wants???
	// maybe create a temporary empty buffer full of zeros? idk
	std::vector< VertAttribData_t* > attribs;

	for ( size_t j = 0; j < spVertexData->aData.size(); j++ )
	{
		VertAttribData_t& data = spVertexData->aData[ j ];

		// if ( shaderFormat & ( 1 << data.aAttrib ) )
		attribs.push_back( &data );
	}

	spBuffer->aVertex.resize( attribs.size() );

	for ( size_t j = 0; j < attribs.size(); j++ )
	{
		auto& data       = attribs[ j ];
		char* bufferName = nullptr;

#ifdef _DEBUG
		if ( spDebugName )
		{
			const char* attribName = Graphics_GetVertexAttributeName( data->aAttrib );

			size_t      len        = strlen( spDebugName ) + strlen( attribName );
			bufferName             = new char[ len + 9 ];  // MEMORY LEAK - need string memory pool

			snprintf( bufferName, len + 9, "VB | %s | %s", attribName, spDebugName );
		}
#endif

		Handle buffer = CreateModelBuffer(
		  bufferName ? bufferName : "VB",
		  data->apData,
		  Graphics_GetVertexAttributeSize( data->aAttrib ) * spVertexData->aCount,
		  EBufferFlags_Vertex );

		spBuffer->aVertex[ j ] = buffer;
	}
}


void Graphics_CreateIndexBuffer( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName )
{
	char* bufferName = nullptr;

	if ( spVertexData->aIndices.empty() )
	{
		Log_Warn( gLC_ClientGraphics, "Trying to create Index Buffer for mesh with no indices!\n" );
		return;
	}

#ifdef _DEBUG
	if ( spDebugName )
	{
		size_t len = strlen( spDebugName );
		bufferName = new char[ len + 6 ];  // MEMORY LEAK - need string memory pool

		snprintf( bufferName, len + 6, "IB | %s", spDebugName );
	}
#endif

	spBuffer->aIndex = CreateModelBuffer(
	  bufferName ? bufferName : "IB",
	  spVertexData->aIndices.data(),
	  // sizeof( u32 ) * spVertexData->aIndices.size(),
	  spVertexData->aIndices.size_bytes(),
	  EBufferFlags_Index );
}


#if 0
void Graphics_CreateModelBuffers( ModelBuffers_t* spBuffers, VertexData_t* spVertexData, bool sCreateIndex, const char* spDebugName )
{
	if ( !spBuffers )
	{
		Log_Error( gLC_ClientGraphics, "ModelBuffers_t is nullptr\n" );
		return;
	}
	else if ( spBuffers->aVertex.size() )
	{
		Log_Error( gLC_ClientGraphics, "Model Vertex Buffers already created\n" );
		return;
	}

	char* debugName = nullptr;
#ifdef _DEBUG
	if ( spDebugName )
	{
		size_t nameLen = strlen( spDebugName );
		debugName      = new char[ nameLen ];  // MEMORY LEAK - need string memory pool
		snprintf( debugName, nameLen, "%s", spDebugName );
	}
#endif

	Graphics_CreateVertexBuffers( spBuffers, spVertexData, debugName );

	if ( sCreateIndex )
		Graphics_CreateIndexBuffer( spBuffers, spVertexData, debugName );
}
#endif


// ---------------------------------------------------------------------------------------
// Lighting


Light_t* Graphics_CreateLight( ELightType sType )
{
	Light_t* light       = new Light_t;
	light->aType         = sType;
	gNeedLightInfoUpdate = true;

	gLights.push_back( light );
	gDirtyLights.push_back( light );

	return light;
}


void Graphics_UpdateLight( Light_t* spLight )
{
	gDirtyLights.push_back( spLight );
}


void Graphics_DestroyLight( Light_t* spLight )
{
	if ( !spLight )
		return;

	gNeedLightInfoUpdate = true;

	gDestroyLights.push_back( spLight );

	vec_remove_if( gDirtyLights, spLight );
}

