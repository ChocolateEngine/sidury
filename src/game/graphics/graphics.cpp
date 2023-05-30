#include "core/core.h"
#include "igui.h"
#include "render/irender.h"
#include "graphics.h"
#include "graphics_int.h"
#include "lighting.h"
#include "debug_draw.h"
#include "mesh_builder.h"
#include "imgui/imgui.h"
#include "../main.h"

#include <forward_list>
#include <stack>
#include <set>


LOG_REGISTER_CHANNEL_EX( gLC_ClientGraphics, "ClientGraphics", LogColor::Green )

// --------------------------------------------------------------------------------------
// Interfaces

extern IGuiSystem* gui;
extern IRender*    render;

// --------------------------------------------------------------------------------------

void                         Graphics_LoadObj( const std::string& srBasePath, const std::string& srPath, Model* spModel );
void                         Graphics_LoadGltf( const std::string& srBasePath, const std::string& srPath, const std::string& srExt, Model* spModel );

void                         Graphics_LoadSceneObj( const std::string& srBasePath, const std::string& srPath, Scene_t* spScene );

// shaders, fun
void                         Shader_Basic3D_UpdateMaterialData( Handle sMat );
void                         Shader_UI_Draw( Handle cmd, size_t sCmdIndex, Handle shColor );

// --------------------------------------------------------------------------------------
// General Rendering

// TODO: rethink this so you can have draw ordering
// use the RenderLayer idea you had

// struct RenderList_t
// {
// 	std::forward_list< SurfaceDraw_t* > aSurfaces;
// };


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
extern Handle                                     gRenderPassShadow;

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
std::vector< Handle >                             gViewInfoBuffers( MAX_VIEW_INFO_BUFFERS + 1 );
std::vector< ViewInfo_t >                         gViewInfo( 1 );
std::vector< Frustum_t >                          gViewInfoFrustums;
std::stack< ViewInfo_t >                          gViewInfoStack;
bool                                              gViewInfoUpdate    = false;
int                                               gViewInfoCount     = 1;

// --------------------------------------------------------------------------------------
// Assets

struct ModelAABBUpdate_t
{
	Renderable_t* apModelDraw;
	ModelBBox_t  aBBox;
};

ResourceList< Model >                            gModels;
static std::unordered_map< std::string, Handle > gModelPaths;
static std::unordered_map< Handle, ModelBBox_t > gModelBBox;

static ResourceList< Scene_t >                   gScenes;
static std::unordered_map< std::string, Handle > gScenePaths;

// --------------------------------------------------------------------------------------
// Other

size_t                                           gModelDrawCalls = 0;
size_t                                           gVertsDrawn     = 0;

Light_t*                                         gpWorldLight    = nullptr;

extern ChVector< glm::vec3 >                     gDebugLineVertPos;

// TEMP LIGHTING - WILL BE DEFINED IN MAP FORMAT LATER
CONVAR( r_world_dir_x, 65 );
CONVAR( r_world_dir_y, 35 );
CONVAR( r_world_dir_z, 0 );

CONVAR( r_vis, 1 );
CONVAR( r_vis_lock, 0 );

CONVAR( r_line_thickness, 2 );

CONCMD( r_reload_textures )
{
	render->ReloadTextures();
	// Graphics_SetAllMaterialsDirty();
}


void Graphics_CalcModelBBox( Handle sModel )
{
	Model* model = Graphics_GetModelData( sModel );

	if ( !model )
		return;

	ModelBBox_t bbox{};
	bbox.aMax = { INT_MIN, INT_MIN, INT_MIN };
	bbox.aMin = { INT_MAX, INT_MAX, INT_MAX };

	auto*      vertData = model->apVertexData;
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

	for ( Mesh& mesh : model->aMeshes )
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
	PROF_SCOPE();

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
	Graphics_CalcModelBBox( handle );

	gModelPaths[ srPath ] = handle;

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
		return InvalidHandle;
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
		Graphics_CalcModelBBox( modelHandle );
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


void Graphics_DestroyRenderPasses()
{
	Graphics_DestroyShadowRenderPass();
}


bool Graphics_CreateRenderPasses()
{
	// Shadow Map Render Pass
	if ( !Graphics_CreateShadowRenderPass() )
	{
		Log_Error( "Failed to create Shadow Map Render Pass\n" );
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
	// Create Light and Shadowmap Layouts

	if ( !Graphics_CreateLightDescriptorSets() )
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

	// actually stupid, they are HANDLES, YOU SHOULDN'T NEED NEW ONES
	// only exception if we are in msaa now or not, blech
	render->GetBackBufferTextures( &gBackBufferTex[ 0 ], &gBackBufferTex[ 1 ], &gBackBufferTex[ 2 ] );

	int width, height;
	render->GetSurfaceSize( width, height );
	gViewInfo[ 0 ].aSize = { width, height };

	if ( sFlags & ERenderResetFlags_MSAA )
	{
		Graphics_DestroyRenderPasses();

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

	if ( !Graphics_CreateDescriptorSets() )
	{
		return false;
	}

	if ( !Graphics_ShaderInit( false ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Shaders!\n" );
		return false;
	}

	if ( !Graphics_DebugDrawInit() )
		return false;

	// TEMP: make a world light
	gpWorldLight = Graphics_CreateLight( ELightType_Directional );
	gpWorldLight->aColor = { 1.0, 1.0, 1.0, 1.0 };
	// gpWorldLight->aColor = { 0.1, 0.1, 0.1 };

	return render->InitImGui( gRenderPassGraphics );
	// return render->InitImGui( gRenderPassGraphics );
}


void Graphics_Shutdown()
{
}


void Graphics_Reset()
{
	PROF_SCOPE();

	render->Reset();
}


void Graphics_NewFrame()
{
	PROF_SCOPE();

	render->NewFrame();

	Graphics_DebugDrawNewFrame();
}


// TODO: experiment with instanced drawing
void Graphics_CmdDrawSurface( Handle cmd, Model* spModel, size_t sSurface )
{
	PROF_SCOPE();

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
	PROF_SCOPE();

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


// https://iquilezles.org/articles/frustumcorrect/
bool Frustum_t::IsBoxVisible( const glm::vec3& sMin, const glm::vec3& sMax ) const
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


void Graphics_CreateFrustum( Frustum_t& srFrustum, const glm::mat4& srViewMat )
{
	PROF_SCOPE();

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
	PROF_SCOPE();

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
	PROF_SCOPE();

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
	PROF_SCOPE();

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


// Do Rendering with shader system and user land meshes
void Graphics_RenderView( Handle cmd, ViewRenderList_t& srViewList )
{
	PROF_SCOPE();

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
	PROF_SCOPE();

	for ( size_t i = 0; i < gViewRenderLists.size(); i++ )
	{
		// HACK HACK !!!!
		// don't render views with shader overrides here, the only override is the shadow map shader
		// and that is rendered in a separate render pass
		if ( gViewInfo[ i ].aShaderOverride )
			continue;

		Graphics_RenderView( cmd, gViewRenderLists[ i ] );
	}
}


void Graphics_AddToViewRenderList()
{
}


void Graphics_PrepareDrawData()
{
	PROF_SCOPE();

	// fun
	static Handle        shadow_map       = Graphics_GetShader( "__shadow_map" );
	static ShaderData_t* shadowShaderData = Shader_GetData( shadow_map );

	render->PreRenderPass();

	ImGui::Text( "Model Draw Calls: %zd", gModelDrawCalls );
	ImGui::Text( "Verts Drawn: %zd", gVertsDrawn );
	ImGui::Text( "Debug Line Verts: %zd", gDebugLineVertPos.size() );

	{
		PROF_SCOPE_NAMED( "Imgui Render" );
		ImGui::Render();
	}

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
		{
			if ( glm::length( bbox.aMin ) == 0 && glm::length( bbox.aMax ) == 0 )
			{
				Log_Warn( gLC_ClientGraphics, "Model Bounding Box not calculated, length of min and max is 0\n" );
				Graphics_CalcModelBBox( renderable->aModel );
				bbox = gModelBBox[ renderable->aModel ];
			}

			renderable->aAABB = Graphics_CreateWorldAABB( renderable->aModelMatrix, bbox );
		}
	}

	gRenderAABBUpdate.clear();

	// Update Light Data
	Graphics_PrepareLights();

	if ( gViewInfoUpdate )
	{
		gViewInfoUpdate = false;
		// for ( size_t i = 0; i < gViewInfoBuffers.size(); i++ )
		for ( size_t i = 0; i < 1; i++ )
		{
			render->BufferWrite( gViewInfoBuffers[ i ], sizeof( UBO_ViewInfo_t ), &gViewInfo[ i ] );
		}
	}

	// update view frustums (CHANGE THIS, SHOULD NOT UPDATE EVERY SINGLE ONE PER FRAME  !!!!)
	if ( !r_vis_lock.GetBool() || gViewInfoFrustums.size() != gViewInfo.size() )
	{
		gViewInfoFrustums.resize( gViewInfo.size() );

		for ( size_t i = 0; i < gViewInfo.size(); i++ )
		{
			Graphics_CreateFrustum( gViewInfoFrustums[ i ], gViewInfo[ i ].aProjView );
			Graphics_DrawFrustum( gViewInfoFrustums[ i ] );
		}
	}

	// --------------------------------------------------------------------

	Shader_ResetPushData();

	bool usingShadow = Graphics_IsUsingShadowMaps();

	// --------------------------------------------------------------------
	// Prepare View Render Lists

	if ( !r_vis_lock.GetBool() )
	{
		PROF_SCOPE_NAMED( "Prepare View Render Lists" );

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
				PROF_SCOPE_NAMED( "Viewport Testing" );

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

	Graphics_UpdateDebugDraw();

	// --------------------------------------------------------------------
	// Update Shader Draw Data

	for ( int viewIndex = 0; viewIndex < gViewInfoCount; viewIndex++ )
	{
		PROF_SCOPE_NAMED( "Update Shader Draw Data" );

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
}


void Graphics_Present()
{
	PROF_SCOPE();

	// render->LockGraphicsMutex();
	render->WaitForQueues();
	render->ResetCommandPool();

	Graphics_PrepareDrawData();

	// For each framebuffer, begin a primary
	// command buffer, and record the commands.
	for ( gCmdIndex = 0; gCmdIndex < gCommandBuffers.size(); gCmdIndex++ )
	{
		PROF_SCOPE_NAMED( "Primary Command Buffer" );

		auto c = gCommandBuffers[ gCmdIndex ];

		render->BeginCommandBuffer( c );

		// Draw Shadow Maps
		Graphics_DrawShadowMaps( c );

		// ----------------------------------------------------------
		// Main RenderPass

		RenderPassBegin_t renderPassBegin{};
		renderPassBegin.aRenderPass  = gRenderPassGraphics;
		renderPassBegin.aFrameBuffer = gBackBuffer[ gCmdIndex ];
		renderPassBegin.aClear.resize( 2 );
		renderPassBegin.aClear[ 0 ].aColor   = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear[ 0 ].aIsDepth = false;
		renderPassBegin.aClear[ 1 ].aColor   = { 0.f, 0.f, 0.f, 1.f };
		renderPassBegin.aClear[ 1 ].aIsDepth = true;

		render->BeginRenderPass( c, renderPassBegin );  // VK_SUBPASS_CONTENTS_INLINE

		Graphics_Render( c );
		render->DrawImGui( ImGui::GetDrawData(), c );

		render->EndRenderPass( c );

		render->EndCommandBuffer( c );
	}

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


void Graphics_UpdateRenderableAABB( Handle sRenderable )
{
	if ( !sRenderable )
		return;

	if ( Renderable_t* renderable = Graphics_GetRenderableData( sRenderable ) )
		gRenderAABBUpdate.emplace( sRenderable, gModelBBox[ renderable->aModel ] );
}


void Graphics_ConsolidateRenderables()
{
	gRenderables.Consolidate();
}


// ---------------------------------------------------------------------------------------
// Vertex Format/Attributes


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
	render->BufferWrite( stagingBuffer, sBufferSize, spData );

	Handle deviceBuffer = render->CreateBuffer( spName, sBufferSize, sUsage | EBufferFlags_TransferDst, EBufferMemory_Device );

	// Copy Local Buffer data to Device
	render->BufferCopy( stagingBuffer, deviceBuffer, sBufferSize );

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
