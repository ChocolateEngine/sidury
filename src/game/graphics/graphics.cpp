#include "core/core.h"
#include "igui.h"
#include "render/irender.h"
#include "graphics.h"
#include "mesh_builder.h"
#include "imgui/imgui.h"
#include "../main.h"

#include <forward_list>
#include <set>


LOG_REGISTER_CHANNEL_EX( gLC_ClientGraphics, "ClientGraphics", LogColor::DarkMagenta )

// --------------------------------------------------------------------------------------
// Interfaces

extern BaseGuiSystem*        gui;
extern IRender*              render;

// --------------------------------------------------------------------------------------

void                         Graphics_LoadObj( const std::string& srBasePath, const std::string& srPath, Model* spModel );
void                         Graphics_LoadGltf( const std::string& srBasePath, const std::string& srPath, const std::string& srExt, Model* spModel );

// shaders, fun
void                         Shader_Basic3D_UpdateMaterialData( Handle sMat );

void                         Shader_UI_Draw( Handle cmd, size_t sCmdIndex, Handle shColor );
void                         Shader_DeferredFS_Draw( Handle cmd, size_t sCmdIndex );

// --------------------------------------------------------------------------------------
// Rendering

// TODO: rethink this so you can have draw ordering
static std::unordered_map<
  Handle,
  std::forward_list< ModelSurfaceDraw_t > >
												 gMeshDrawList;

static std::vector< Handle >                     gCommandBuffers;
static size_t                                    gCmdIndex = 0;

Handle                                           gRenderPassGraphics;

static glm::mat4                                 gViewProjMat;
static bool                                      gViewProjMatUpdate = false;
static std::vector< Handle >                     gViewProjBuffers( 1 );

// stores backbuffer color and depth
static Handle                                    gBackBuffer[ 2 ];
static Handle                                    gBackBufferTex[ 3 ];

// descriptor set layouts
Handle                                           gLayoutSampler         = InvalidHandle;
Handle                                           gLayoutViewProj        = InvalidHandle;
Handle                                           gLayoutMaterialBasic3D = InvalidHandle;  // blech

// descriptor sets
Handle                                           gLayoutSamplerSets[ 2 ];
Handle                                           gLayoutViewProjSets[ 2 ];
Handle*                                          gLayoutLightSets;
Handle*                                          gLayoutMaterialBasic3DSets;
constexpr u32                                    MAX_MATERIALS_BASIC3D = 500;

extern std::set< Handle >                        gDirtyMaterials;

static bool                                      gDrawingSkybox = false;
static Handle                                    gSkyboxShader  = InvalidHandle;

ViewInfo_t                                       gViewInfo;
bool                                             gViewInfoUpdate = false;

// --------------------------------------------------------------------------------------
// Lighting

UniformBufferArray_t                             gUniformLightInfo;

LightUniformBuffer_t                             gUniformLightDirectional( ELightType_Directional );
LightUniformBuffer_t                             gUniformLightPoint( ELightType_Point );
LightUniformBuffer_t                             gUniformLightCone( ELightType_Cone );
LightUniformBuffer_t                             gUniformLightCapsule( ELightType_Capsule );

std::unordered_map< Light_t*, Handle >           gLightBuffers;

constexpr int                                    MAX_LIGHTS = 32;
std::vector< Light_t* >                          gLights;
std::vector< Light_t* >                          gDirtyLights;

LightInfo_t                                      gLightInfo;
Handle                                           gLightInfoBuffer     = InvalidHandle;
Light_t*                                         gpWorldLight         = nullptr;

static bool                                      gNeedLightInfoUpdate = false;

// --------------------------------------------------------------------------------------
// Assets

static ResourceList< Model* >                    gModels;
static std::unordered_map< std::string, Handle > gModelPaths;

// --------------------------------------------------------------------------------------


// TEMP LIGHTING - WILL BE DEFINED IN MAP FORMAT LATER
CONVAR( r_world_dir_x, 65 );
CONVAR( r_world_dir_y, 35 );
CONVAR( r_world_dir_z, 0 );


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

	Model* model = new Model;
	Handle handle = InvalidHandle;

	// TODO: try to do file header checking
	if ( fileExt == "obj" )
	{
		handle = gModels.Add( model );
		Graphics_LoadObj( srPath, fullPath, model );
	}
	else if ( fileExt == "glb" || fileExt == "gltf" )
	{
		handle = gModels.Add( model );
		Graphics_LoadGltf( srPath, fullPath, fileExt, model );
	}
	else
	{
		Log_DevF( gLC_ClientGraphics, 1, "Unknown Model File Extension: %s\n", fileExt.c_str() );
	}

	//sModel->aRadius = glm::distance( mesh->aMinSize, mesh->aMaxSize ) / 2.0f;

	// TODO: load in an error model here instead
	if ( model->aMeshes.empty() )
	{
		gModels.Remove( handle );
		delete model;
		return InvalidHandle;
	}

	gModelPaths[ srPath ] = handle;
	return handle;
}


Handle Graphics_AddModel( Model* spModel )
{
	return gModels.Add( spModel );
}


void Graphics_FreeModel( Handle shModel )
{
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
}


bool Graphics_CreateRenderPasses()
{
	RenderPassCreate_t create{};

	// ImGui Render Pass
#if 0
	{
		create.aAttachments.resize( 2 );

		create.aAttachments[ 0 ].aFormat  = GraphicsFmt::BGRA8888_UNORM;
		create.aAttachments[ 0 ].aUseMSAA = false;
		create.aAttachments[ 0 ].aType    = EAttachmentType_Color;
		create.aAttachments[ 0 ].aLoadOp  = EAttachmentLoadOp_Clear;

		create.aAttachments[ 1 ].aFormat  = render->GetSwapFormatDepth();
		create.aAttachments[ 1 ].aUseMSAA = false;
		create.aAttachments[ 1 ].aType    = EAttachmentType_Depth;
		create.aAttachments[ 1 ].aLoadOp  = EAttachmentLoadOp_Clear;

		create.aSubpasses.resize( 1 );
		create.aSubpasses[ 0 ].aBindPoint = EPipelineBindPoint_Graphics;

		gRenderPassUI                     = render->CreateRenderPass( create );

		if ( gRenderPassUI == InvalidHandle )
			return false;
	}
#endif

	return true;
}


bool Graphics_CreateLightInfoLayout( UniformBufferArray_t& srBuffer, const char* spBufferName, size_t sBufferSize )
{
	CreateVariableDescLayout_t createLayout{};
	createLayout.apName   = "Light Info Layout";
	createLayout.aType    = EDescriptorType_UniformBuffer;
	createLayout.aStages  = ShaderStage_Vertex | ShaderStage_Fragment;
	createLayout.aBinding = 0;
	createLayout.aCount   = 1;

	srBuffer.aLayout      = render->CreateVariableDescLayout( createLayout );

	if ( srBuffer.aLayout == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Light Layout\n" );
		return false;
	}

	AllocVariableDescLayout_t allocLayout{};
	allocLayout.apName    = "Light Info Set";
	allocLayout.aLayout   = srBuffer.aLayout;
	allocLayout.aCount    = 1;
	allocLayout.aSetCount = srBuffer.aSets.size();

	if ( !render->AllocateVariableDescLayout( allocLayout, srBuffer.aSets.data() ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to allocate Light Layout\n" );
		return false;
	}

	return true;
}


bool Graphics_CreateLightLayout( LightUniformBuffer_t& srBuffer, const char* spLayoutName, const char* spSetName )
{
	CreateVariableDescLayout_t createLayout{};
	createLayout.apName   = spLayoutName;
	createLayout.aType    = EDescriptorType_UniformBuffer;
	createLayout.aStages  = ShaderStage_Vertex | ShaderStage_Fragment;
	createLayout.aBinding = 0;
	createLayout.aCount   = MAX_LIGHTS;

	srBuffer.aLayout      = render->CreateVariableDescLayout( createLayout );

	if ( srBuffer.aLayout == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Light Layout\n" );
		return false;
	}

	AllocVariableDescLayout_t allocLayout{};
	allocLayout.apName    = spSetName;
	allocLayout.aLayout   = srBuffer.aLayout;
	allocLayout.aCount    = MAX_LIGHTS;
	allocLayout.aSetCount = srBuffer.aSets.size();

	if ( !render->AllocateVariableDescLayout( allocLayout, srBuffer.aSets.data() ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to allocate Light Layout\n" );
		return false;
	}

	return true;
}


#if 0
bool Graphics_UpdateLightLayout( LightUniformBuffer_t& srBuffer, const char* spBufferName, size_t sBufferSize )
{
	// create buffers for it
	for ( size_t i = 0; i < srBuffer.aBuffers.size(); i++ )
	{
		Handle buffer = render->CreateBuffer( spBufferName, sBufferSize, EBufferFlags_Uniform, EBufferMemory_Host );

		if ( buffer == InvalidHandle )
		{
			Log_Error( gLC_ClientGraphics, "Failed to Create Light Uniform Buffer\n" );
			return false;
		}

		srBuffer.aBuffers[ i ] = buffer;
	}

	// update the descriptor sets
	UpdateVariableDescSet_t update{};

	for ( size_t i = 0; i < srBuffer.aSets.size(); i++ )
		update.aDescSets.push_back( srBuffer.aSets[ i ] );

	update.aType    = EDescriptorType_UniformBuffer;
	update.aBuffers = srBuffer.aBuffers;
	render->UpdateVariableDescSet( update );

	return true;
}
#endif


Handle Graphics_AddLightBuffer( LightUniformBuffer_t& srBuffer, const char* spBufferName, size_t sBufferSize, Light_t* spLight )
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
		if ( light->aType == srBuffer.aLightType )
			update.aBuffers.push_back( bufferHandle );
	}

	render->UpdateVariableDescSet( update );

	return buffer;
}


bool Graphics_CreateDescriptorSets()
{
	// TODO: just create the sampler here and have a
	// Graphics_LoadTexture() function to auto add to the image sampler sets
	gLayoutSampler = render->GetSamplerLayout();
	render->GetSamplerSets( gLayoutSamplerSets );

	// ------------------------------------------------------
	// Create ViewInfo UBO
	{
		CreateVariableDescLayout_t createViewProj{};
		createViewProj.aType    = EDescriptorType_UniformBuffer;
		createViewProj.aStages  = ShaderStage_Vertex | ShaderStage_Fragment;
		createViewProj.aBinding = 0;
		createViewProj.aCount   = 2;

		gLayoutViewProj         = render->CreateVariableDescLayout( createViewProj );

		if ( gLayoutViewProj == InvalidHandle )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create ViewProj UBO Layout\n" );
			return false;
		}

		AllocVariableDescLayout_t allocViewProj{};
		allocViewProj.aLayout   = gLayoutViewProj;
		allocViewProj.aCount    = gViewProjBuffers.size();
		allocViewProj.aSetCount = 2;

		if ( !render->AllocateVariableDescLayout( allocViewProj, gLayoutViewProjSets ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to allocate Basic 3D Material Layout\n" );
			return false;
		}

		// create buffer for it
		// (NOTE: not changing this from a std::vector cause you could use this for multiple views in the future probably)
		for ( u32 i = 0; i < gViewProjBuffers.size(); i++ )
		{
			Handle buffer = render->CreateBuffer( "View Info Buffer", sizeof( ViewInfo_t ), EBufferFlags_Uniform, EBufferMemory_Host );

			if ( buffer == InvalidHandle )
			{
				Log_Error( gLC_ClientGraphics, "Failed to Create Material Uniform Buffer\n" );
				return false;
			}

			gViewProjBuffers[ i ] = buffer;
		}

		// update the material descriptor sets
		UpdateVariableDescSet_t update{};

		// what
		update.aDescSets.push_back( gLayoutViewProjSets[ 0 ] );
		update.aDescSets.push_back( gLayoutViewProjSets[ 1 ] );

		update.aType    = EDescriptorType_UniformBuffer;
		update.aBuffers = gViewProjBuffers;
		render->UpdateVariableDescSet( update );
	}

	// ------------------------------------------------------
	// Create Light Layouts
	{
		gUniformLightInfo.aSets.resize( 1 );
		if ( !Graphics_CreateLightInfoLayout( gUniformLightInfo, "Light Info Buffer", sizeof( LightInfo_t ) ) )
			return false;

		gUniformLightDirectional.aSets.resize( 1 );
		if ( !Graphics_CreateLightLayout( gUniformLightDirectional, "Light Directional Layout", "Light Directional Set" ) )
			return false;

		gUniformLightPoint.aSets.resize( 1 );
		if ( !Graphics_CreateLightLayout( gUniformLightPoint, "Light Point Layout", "Light Point Set" ) )
			return false;

		gUniformLightCone.aSets.resize( 1 );
		if ( !Graphics_CreateLightLayout( gUniformLightCone, "Light Cone Layout", "Light Cone Set" ) )
			return false;

		gUniformLightCapsule.aSets.resize( 1 );
		if ( !Graphics_CreateLightLayout( gUniformLightCapsule, "Light Capsule Layout", "Light Capsule Set" ) )
			return false;
	}

	// ------------------------------------------------------
	// Create Material Buffers for Basic3D shader

	CreateVariableDescLayout_t createBasic3DMat{};
	createBasic3DMat.aType           = EDescriptorType_UniformBuffer;
	createBasic3DMat.aStages         = ShaderStage_Vertex | ShaderStage_Fragment;
	createBasic3DMat.aBinding        = 0;
	createBasic3DMat.aCount          = MAX_MATERIALS_BASIC3D;

	gLayoutMaterialBasic3D           = render->CreateVariableDescLayout( createBasic3DMat );

	if ( gLayoutMaterialBasic3D == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Basic 3D Material Layout\n" );
		return false;
	}

	AllocVariableDescLayout_t allocBasic3DMat{};
	allocBasic3DMat.aLayout    = gLayoutMaterialBasic3D;
	allocBasic3DMat.aCount     = MAX_MATERIALS_BASIC3D; // max materials for basic 3d
	allocBasic3DMat.aSetCount  = 2;

	// wtf
	gLayoutMaterialBasic3DSets = new Handle[ MAX_MATERIALS_BASIC3D ];

	if ( !render->AllocateVariableDescLayout( allocBasic3DMat, gLayoutMaterialBasic3DSets ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to allocate Basic 3D Material Layout\n" );
		return false;
	}

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
	gLightInfoBuffer = render->CreateBuffer( "Light Info Buffer", sizeof( LightInfo_t ), EBufferFlags_Uniform, EBufferMemory_Host );

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

	// TEMP: make a world light
	gpWorldLight = Graphics_CreateLight( ELightType_Directional );
	gpWorldLight->aColor = { 0.5, 1.0, 0.5 };
	// gpWorldLight->aDir   = { 1.0, 0.5, 1.5, 0.f };

	Graphics_UpdateLight( gpWorldLight );

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

	gMeshDrawList.clear();
	gDrawingSkybox = false;
}


void Graphics_CmdDrawModel( Handle cmd, ModelSurfaceDraw_t& srDrawInfo )
{
	Model* model = nullptr;
	if ( !gModels.Get( srDrawInfo.apDraw->aModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_ModelDraw: model is nullptr\n" );
		return;
	}

	Mesh& mesh = model->aMeshes[ srDrawInfo.aSurface ];

	// TODO: figure out a way to use vertex and index offsets with this vertex format stuff
	// ideally, it would be less vertex buffer binding, but would be harder to pull off
	if ( mesh.aIndexBuffer )
		render->CmdDrawIndexed(
		    cmd,
			mesh.aIndices.size(),
			1,
			0,  // first index
			0,  // vertex offset
			0
		);

	else
		render->CmdDraw(
			cmd,
			mesh.aVertexData.aCount,
			1,
			0,  // no offset
			0
		);

	// gModelDrawCalls++;
	// gVertsDrawn += renderable->GetSurfaceVertexData( matIndex ).aCount;
}


void Graphics_BindModel( Handle cmd, ModelSurfaceDraw_t& srDrawInfo )
{
	// get model and check if it's nullptr
	if ( srDrawInfo.apDraw->aModel == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_BindModel: model handle is InvalidHandle\n" );
		return;
	}

	Model* model = nullptr;
	if ( !gModels.Get( srDrawInfo.apDraw->aModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_BindModel: model is nullptr\n" );
		return;
	}

	if ( srDrawInfo.aSurface > model->aMeshes.size() )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_BindModel: model surface index is out of range!\n" );
		return;
	}

	Mesh& mesh = model->aMeshes[ srDrawInfo.aSurface ];
	
	// Bind the mesh's vertex and index buffers

	// um
	// std::vector< size_t > offsets( mesh.aVertexBuffers.size() );

	size_t* offsets = (size_t*)CH_STACK_ALLOC( sizeof( size_t ) * mesh.aVertexBuffers.size() );
	if ( offsets == nullptr )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_BindModel: Failed to allocate vertex buffer offsets!\n" );
		return;
	}

	memset( offsets, 0, sizeof( size_t ) * mesh.aVertexBuffers.size() );

	render->CmdBindVertexBuffers( cmd, 0, mesh.aVertexBuffers.size(), mesh.aVertexBuffers.data(), offsets );

	// TODO: store index type here somewhere
	render->CmdBindIndexBuffer( cmd, mesh.aIndexBuffer, 0, EIndexType_U32 );

	CH_STACK_FREE( offsets );
}


void Graphics_DrawShaderRenderables( Handle cmd, Handle shader )
{
	if ( !Shader_Bind( cmd, gCmdIndex, shader ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to bind shader\n" );
		return;
	}

	auto&               renderList     = gMeshDrawList[ shader ];
	ModelSurfaceDraw_t* prevRenderable = nullptr;

	for ( auto& renderable : renderList )
	{
		// if ( prevRenderable != renderable || prevMatIndex != matIndex )
		// if ( prevRenderable && prevRenderable->aModel != renderable->aModel || prevMatIndex != matIndex )
		if ( !prevRenderable || prevRenderable->apDraw->aModel != renderable.apDraw->aModel || prevRenderable->aSurface != renderable.aSurface )
		{
			prevRenderable = &renderable;
			Graphics_BindModel( cmd, renderable );
		}

		if ( !Shader_PreRenderableDraw( cmd, gCmdIndex, shader, renderable ) )
			continue;

		Graphics_CmdDrawModel( cmd, renderable );
	}
}


// Do Rendering with shader system and user land meshes
void Graphics_Render( Handle cmd )
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

	ModelSurfaceDraw_t* prevRenderable = nullptr;

	// TODO: still could be better, but it's better than what we used to have
	for ( auto& [ shader, renderList ] : gMeshDrawList )
	{
		if ( shader == skybox )
		{
			hasSkybox = true;
			continue;
		}

		Graphics_DrawShaderRenderables( cmd, shader );
	}

	// Draw Skybox - and set depth for skybox
	if ( hasSkybox )
	{
		viewPort.minDepth = 0.999f;
		viewPort.maxDepth = 1.f;

		render->CmdSetViewport( cmd, 0, &viewPort, 1 );

		Graphics_DrawShaderRenderables( cmd, skybox );
	}
}


void Graphics_UpdateLightBuffer( Light_t* spLight )
{
	Handle buffer = InvalidHandle;

	auto it = gLightBuffers.find( spLight );

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
			light.aColor = spLight->aColor;

			Util_GetDirectionVectors( spLight->aAng, nullptr, nullptr, &light.aDir );

			render->MemWriteBuffer( buffer, sizeof( light ), &light );
			return;
		}
		case ELightType_Point:
		{
			UBO_LightPoint_t light;
			light.aColor  = spLight->aColor;
			light.aPos    = spLight->aPos;
			light.aRadius = spLight->aRadius;

			render->MemWriteBuffer( buffer, sizeof( light ), &light );
			return;
		}
		case ELightType_Cone:
		{
			UBO_LightCone_t light;
			light.aColor = spLight->aColor;
			light.aPos   = spLight->aPos;
			light.aFov.x = glm::radians( spLight->aInnerFov );
			light.aFov.y = glm::radians( spLight->aOuterFov );

			glm::mat4 matrix;
			ToMatrix( matrix, spLight->aPos, spLight->aAng );

			GetDirectionVectors( matrix, light.aDir );

			// Util_GetDirectionVectors( spLight->aAng, nullptr, nullptr, &light.aDir );
			// Util_GetDirectionVectors( spLight->aAng, &light.aDir );

			render->MemWriteBuffer( buffer, sizeof( light ), &light );
			return;
		}
		case ELightType_Capsule:
		{
			UBO_LightCapsule_t light;
			light.aColor     = spLight->aColor;
			light.aPos       = spLight->aPos;
			light.aLength    = spLight->aLength;
			light.aThickness = spLight->aRadius;

			Util_GetDirectionVectors( spLight->aAng, nullptr, nullptr, &light.aDir );

			render->MemWriteBuffer( buffer, sizeof( light ), &light );
			return;
		}
	}
}


void Graphics_PrepareDrawData()
{
	// glm::vec3 ang( r_world_dir_x.GetFloat(), r_world_dir_y.GetFloat(), r_world_dir_z.GetFloat() );
	// 
	// Util_GetDirectionVectors( ang, nullptr, nullptr, &gpWorldLight->aDir );
	// 
	// Graphics_UpdateLight( gpWorldLight );
	// 
	// ImGui::Text( Vec2Str( gpWorldLight->aDir ).c_str() );

	ImGui::Render();

	for ( const auto& mat : gDirtyMaterials )
	{
		Handle shader = Mat_GetShader( mat );

		// HACK HACK
		if ( Graphics_GetShader( "basic_3d" ) == shader )
			Shader_Basic3D_UpdateMaterialData( mat );
	}

	gDirtyMaterials.clear();

	// Update UBO's for lights if needed
	for ( Light_t* light : gDirtyLights )
		Graphics_UpdateLightBuffer( light );

	gDirtyLights.clear();

	// Update Light Info UBO
	if ( gNeedLightInfoUpdate )
	{
		render->MemWriteBuffer( gLightInfoBuffer, sizeof( LightInfo_t ), &gLightInfo );
		gNeedLightInfoUpdate = false;
	}

	if ( gViewInfoUpdate )
	{
		gViewInfoUpdate = false;
		for ( size_t i = 0; i < gViewProjBuffers.size(); i++ )
			render->MemWriteBuffer( gViewProjBuffers[ i ], sizeof( ViewInfo_t ), &gViewInfo );
	}

	Shader_ResetPushData();

	for ( auto& [ shader, renderList ] : gMeshDrawList )
	{
		for ( auto& renderable : renderList )
		{
			Shader_SetupRenderableDrawData( shader, renderable );
		}
	}
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

		render->BeginCommandBuffer( c );

		RenderPassBegin_t renderPassBegin{};

		// ----------------------------------------------------------
		// ImGui RenderPass
		// renderPassBegin.aRenderPass  = gRenderPassUI;
		// renderPassBegin.aFrameBuffer = gImGuiBuffer[ gCmdIndex ];
		// renderPassBegin.aClear.resize( 2 );
		// renderPassBegin.aClear[ 0 ].aColor   = { 0.f, 0.f, 0.f, 1.f };
		// renderPassBegin.aClear[ 1 ].aIsDepth = true;
		// 
		// render->BeginRenderPass( c, renderPassBegin );
		// render->DrawImGui( ImGui::GetDrawData(), c );
		// render->EndRenderPass( c );

		// ----------------------------------------------------------
		// G-Buffer RenderPass

#if 0
		renderPassBegin.aRenderPass  = gRenderPassGBuffer;
		renderPassBegin.aFrameBuffer = gGBuffer[ gCmdIndex ];
		renderPassBegin.aClear.resize( 6 );
		renderPassBegin.aClear[ 0 ].aColor   = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear[ 1 ].aColor   = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear[ 2 ].aColor   = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear[ 3 ].aColor   = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear[ 4 ].aColor   = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear[ 5 ].aIsDepth = true;
		renderPassBegin.aClear[ 1 ].aIsDepth = false;

		render->BeginRenderPass( c, renderPassBegin );  // VK_SUBPASS_CONTENTS_INLINE
		Graphics_SetupGBuffer( c );
		render->EndRenderPass( c );
#endif

		// ----------------------------------------------------------
		// Main RenderPass
		renderPassBegin.aRenderPass  = gRenderPassGraphics;
		renderPassBegin.aFrameBuffer = gBackBuffer[ gCmdIndex ];
		renderPassBegin.aClear.resize( 2 );
		renderPassBegin.aClear[ 0 ].aColor   = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear[ 1 ].aDepth   = 1.f;
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
	gViewInfo.aViewProj = srMat;
	gViewProjMat = srMat;
	gViewInfoUpdate = true;
}


const glm::mat4& Graphics_GetViewProjMatrix()
{
	return gViewProjMat;
}


void Graphics_DrawModel( ModelDraw_t* spDrawInfo )
{
	if ( !spDrawInfo )
		return;

	Model* model = nullptr;
	if ( !gModels.Get( spDrawInfo->aModel, &model ) )
	{
		Log_Warn( gLC_ClientGraphics, "Renderable has no model!\n" );
		return;
	}

	for ( size_t i = 0; i < model->aMeshes.size(); i++ )
	{
		Handle mat = model->aMeshes[ i ].aMaterial;

		// TODO: add Mat_IsValid()
		if ( mat == InvalidHandle )
		{
			Log_ErrorF( gLC_ClientGraphics, "Model part \"%d\" has no material!\n", i );
			continue;
		}

		Handle shader = Mat_GetShader( mat );
		gMeshDrawList[ shader ].emplace_front( spDrawInfo, i );

		if ( shader == gSkyboxShader )
			gDrawingSkybox = true;
	}
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


void Graphics_CreateVertexBuffers( Mesh& srMesh, const char* spDebugName )
{
	VertexData_t& vertData = srMesh.aVertexData;

	if ( vertData.aCount == 0 )
	{
		Log_Warn( gLC_ClientGraphics, "Trying to create Vertex Buffers for mesh with no vertices!\n" );
		return;
	}

	VertexFormat shaderFormat = Mat_GetVertexFormat( srMesh.aMaterial );

	if ( shaderFormat == VertexFormat_None )
	{
		Log_Error( gLC_ClientGraphics, "No Vertex Format for shader!\n" );
		return;
	}

	// Get Attributes the shader wants
	// TODO: what about if we don't have an attribute the shader wants???
	// maybe create a temporary empty buffer full of zeros? idk
	std::vector< VertAttribData_t* > attribs;

	for ( size_t j = 0; j < vertData.aData.size(); j++ )
	{
		VertAttribData_t& data = vertData.aData[ j ];

		if ( shaderFormat & ( 1 << data.aAttrib ) )
			attribs.push_back( &data );
	}

	srMesh.aVertexBuffers.clear();

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
		  Graphics_GetVertexAttributeSize( data->aAttrib ) * vertData.aCount,
		  EBufferFlags_Vertex );

		srMesh.aVertexBuffers.push_back( buffer );
	}
}


void Graphics_CreateIndexBuffer( Mesh& srMesh, const char* spDebugName )
{
	char* bufferName = nullptr;

	if ( srMesh.aIndices.empty() )
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

	srMesh.aIndexBuffer = CreateModelBuffer(
	  bufferName ? bufferName : "IB",
	  srMesh.aIndices.data(),
	  sizeof( u32 ) * srMesh.aIndices.size(),
	  EBufferFlags_Index );
}


// ---------------------------------------------------------------------------------------
// Lighting


template< typename T >
T* Graphics_CreateLightInternal( LightUniformBuffer_t& srBuffer, const char* spName, std::vector< T* >& srLights, std::vector< T* >& srDirty )
{
	T* light = new T;
	srLights.push_back( light );
	// Graphics_AddLightBuffer( srBuffer, spName, sizeof( T ), light );
	gNeedLightInfoUpdate = true;
	srDirty.push_back( light );
	return light;
}


template< typename T >
void Graphics_UpdateLightInternal( std::vector< T* >& srDirty, T* spLight )
{
	if ( spLight )
		srDirty.push_back( spLight );
}

Light_t* Graphics_CreateLight( ELightType sType )
{
	Light_t* light       = new Light_t;
	light->aType         = sType;
	gNeedLightInfoUpdate = true;

	gLights.push_back( light );
	gDirtyLights.push_back( light );

	return light;
}

void Graphics_EnableLight( Light_t* spLight )
{
}

void Graphics_DisableLight( Light_t* spLight )
{
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

	// Destroy Descriptor Set

	vec_remove( gLights, spLight );
	vec_remove_if( gDirtyLights, spLight );

	delete spLight;
}


// ---------------------------------------------------------------------------------------
// Render Targets


struct CreateRenderTarget2_t
{
	Handle aRenderPass;
};


// RenderTarget_t Graphics_CreateRenderTarget( const CreateRenderTarget2_t& srCreate )
void Graphics_CreateRenderTarget( const CreateRenderTarget2_t& srCreate )
{
	CreateFramebuffer_t createBuf{};
	createBuf.aRenderPass = srCreate.aRenderPass;

	Handle buffer         = render->CreateFramebuffer( createBuf );
}

