#include "core/core.h"
#include "igui.h"
#include "render/irender.h"
#include "graphics.h"
#include "mesh_builder.h"
#include "imgui/imgui.h"

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

static std::vector< Handle > gCommandBuffers;
static size_t                gCmdIndex = 0;

Handle                       gRenderPassGraphics;
Handle                       gRenderPassUI;

// deferred rendering
Handle                       gRenderPassGBuffer;

// ew
static std::unordered_map<
  Handle,
  std::forward_list< ModelSurfaceDraw_t > >
												 gMeshDrawList;

static glm::mat4                                 gViewProjMat;
static bool                                      gViewProjMatUpdate = false;
static std::vector< Handle >                     gViewProjBuffers( 1 );

// stores backbuffer color and depth
static Handle                                    gBackBuffer[ 2 ];
static Handle                                    gBackBufferTex[ 3 ];
static Handle                                    gImGuiBuffer[ 2 ];
static Handle                                    gImGuiTextures[ 2 ];

// deferred rendering framebuffer
Handle                                           gGBufferTex[ 6 ];  // pos, normal, color, ao, emission, depth
// static Handle                                    gGBufferTarget[ 2 ];
static Handle                                    gGBuffer[ 2 ];

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
static Handle                                    gSkyboxShader = InvalidHandle;

// --------------------------------------------------------------------------------------
// Lighting

UniformBufferArray_t                             gUniformLightInfo;
UniformBufferArray_t                             gUniformLightWorld;
UniformBufferArray_t                             gUniformLightPoint;
UniformBufferArray_t                             gUniformLightCone;
UniformBufferArray_t                             gUniformLightCapsule;

constexpr int                                    MAX_LIGHTS = 128;  // unused lol

static std::vector< LightBase_t* >               gLights;
static std::vector< LightBase_t* >               gDirtyLights;
static bool                                      gNeedLightInfoUpdate = false;

// --------------------------------------------------------------------------------------
// Assets

static ResourceList< Model* >                    gModels;
static std::unordered_map< std::string, Handle > gModelPaths;

// --------------------------------------------------------------------------------------

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
	Graphics_SetAllMaterialsDirty();

	// TODO: reuse the handles, not replace them
	if ( gImGuiTextures[ 0 ] )
		render->FreeTexture( gImGuiTextures[ 0 ] );

	if ( gImGuiTextures[ 1 ] )
		render->FreeTexture( gImGuiTextures[ 1 ] );

	memset( gImGuiTextures, InvalidHandle, sizeof( gImGuiTextures ) );

	if ( gImGuiBuffer[ 0 ] )
		render->DestroyFramebuffer( gImGuiBuffer[ 0 ] );

	if ( gImGuiBuffer[ 1 ] )
		render->DestroyFramebuffer( gImGuiBuffer[ 1 ] );

	memset( gImGuiBuffer, InvalidHandle, sizeof( gImGuiBuffer ) );

	// ------------------------------------------------------------------
	// G-Buffer

	for ( int i = 0; i > 6; i++)
	{
		if ( gGBufferTex[ i ] )
			render->FreeTexture( gGBufferTex[ i ] );
	}

	memset( gGBufferTex, InvalidHandle, sizeof( gGBufferTex ) );

	if ( gGBuffer[ 0 ] )
		render->DestroyFramebuffer( gGBuffer[ 0 ] );

	if ( gGBuffer[ 1 ] )
		render->DestroyFramebuffer( gGBuffer[ 1 ] );

	memset( gGBuffer, InvalidHandle, sizeof( gGBuffer ) );
}


bool Graphics_CreateGBufferTextures()
{
	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	TextureCreateInfo_t texCreate{};
	texCreate.apName    = "G-Buffer Position";
	texCreate.aSize     = { width, height };
	texCreate.aFormat   = GraphicsFmt::RGBA16161616_SFLOAT;
	texCreate.aViewType = EImageView_2D;

	TextureCreateData_t createData{};
	createData.aUsage  = EImageUsage_AttachColor | EImageUsage_Sampled;
	createData.aFilter = EImageFilter_Nearest;

	// Position
	gGBufferTex[ 0 ]   = render->CreateTexture( texCreate, createData );

	// Normal
	texCreate.apName   = "G-Buffer Normal";
	gGBufferTex[ 1 ]   = render->CreateTexture( texCreate, createData );

	// Color
	texCreate.apName   = "G-Buffer Color";
	texCreate.aFormat  = GraphicsFmt::RGBA8888_UNORM;
	gGBufferTex[ 2 ]   = render->CreateTexture( texCreate, createData );

	// AO
	texCreate.apName   = "G-Buffer AO";
	texCreate.aFormat  = GraphicsFmt::R16_SFLOAT;
	gGBufferTex[ 3 ]   = render->CreateTexture( texCreate, createData );

	// Emission
	texCreate.apName   = "G-Buffer Emission";
	texCreate.aFormat  = GraphicsFmt::RGBA8888_UNORM;
	gGBufferTex[ 4 ]   = render->CreateTexture( texCreate, createData );

	// Depth
	texCreate.apName   = "G-Buffer Depth";
	texCreate.aFormat  = render->GetSwapFormatDepth();
	createData.aUsage  = EImageUsage_AttachDepthStencil | EImageUsage_Sampled;
	gGBufferTex[ 5 ]   = render->CreateTexture( texCreate, createData );

	// Create a new framebuffer
	CreateFramebuffer_t frameBufCreate{};
	frameBufCreate.aRenderPass        = gRenderPassGBuffer;
	frameBufCreate.aSize              = { width, height };

	frameBufCreate.aPass.aAttachDepth = gGBufferTex[ 5 ];
	frameBufCreate.aPass.aAttachColors.push_back( gGBufferTex[ 0 ] );
	frameBufCreate.aPass.aAttachColors.push_back( gGBufferTex[ 1 ] );
	frameBufCreate.aPass.aAttachColors.push_back( gGBufferTex[ 2 ] );
	frameBufCreate.aPass.aAttachColors.push_back( gGBufferTex[ 3 ] );
	frameBufCreate.aPass.aAttachColors.push_back( gGBufferTex[ 4 ] );

	gGBuffer[ 0 ] = render->CreateFramebuffer( frameBufCreate );  // Color
	gGBuffer[ 1 ] = render->CreateFramebuffer( frameBufCreate );  // Depth

	if ( gGBuffer[ 0 ] == InvalidHandle || gGBuffer[ 1 ] == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create G-Buffer Framebuffers\n" );
		return false;
	}

	return true;
}


bool Graphics_CreateRenderTargets()
{
	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	// ---------------------------------------------------------
	// ImGui
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

	return Graphics_CreateGBufferTextures();
}


void Graphics_DestroyRenderPasses()
{
	render->DestroyRenderPass( gRenderPassUI );
	render->DestroyRenderPass( gRenderPassGBuffer );
}


bool Graphics_CreateRenderPasses()
{
	RenderPassCreate_t create{};

	// ImGui Render Pass
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

	// --------------------------------------------------------------------------
	// G-Buffer Render Pass
	{
		create.aAttachments.resize( 6 );

		// Position
		create.aAttachments[ 0 ].aFormat  = GraphicsFmt::RGBA16161616_SFLOAT;
		create.aAttachments[ 0 ].aUseMSAA = false;
		create.aAttachments[ 0 ].aType    = EAttachmentType_Color;
		create.aAttachments[ 0 ].aLoadOp  = EAttachmentLoadOp_Clear;

		// Normal
		create.aAttachments[ 1 ].aFormat  = GraphicsFmt::RGBA16161616_SFLOAT;
		create.aAttachments[ 1 ].aUseMSAA = false;
		create.aAttachments[ 1 ].aType    = EAttachmentType_Color;
		create.aAttachments[ 1 ].aLoadOp  = EAttachmentLoadOp_Clear;

		// Color
		create.aAttachments[ 2 ].aFormat  = GraphicsFmt::RGBA8888_UNORM;
		create.aAttachments[ 2 ].aUseMSAA = false;
		create.aAttachments[ 2 ].aType    = EAttachmentType_Color;
		create.aAttachments[ 2 ].aLoadOp  = EAttachmentLoadOp_Clear;

		// AO
		create.aAttachments[ 3 ].aFormat  = GraphicsFmt::R16_SFLOAT;
		create.aAttachments[ 3 ].aUseMSAA = false;
		create.aAttachments[ 3 ].aType    = EAttachmentType_Color;
		create.aAttachments[ 3 ].aLoadOp  = EAttachmentLoadOp_Clear;

		// Emission
		create.aAttachments[ 4 ].aFormat  = GraphicsFmt::RGBA8888_UNORM;
		create.aAttachments[ 4 ].aUseMSAA = false;
		create.aAttachments[ 4 ].aType    = EAttachmentType_Color;
		create.aAttachments[ 4 ].aLoadOp  = EAttachmentLoadOp_Clear;

		// Depth
		create.aAttachments[ 5 ].aFormat  = render->GetSwapFormatDepth();
		create.aAttachments[ 5 ].aUseMSAA = false;
		create.aAttachments[ 5 ].aType    = EAttachmentType_Depth;
		create.aAttachments[ 5 ].aLoadOp  = EAttachmentLoadOp_Clear;

		create.aSubpasses.resize( 1 );
		create.aSubpasses[ 0 ].aBindPoint = EPipelineBindPoint_Graphics;

		gRenderPassGBuffer               = render->CreateRenderPass( create );

		if ( gRenderPassGBuffer == InvalidHandle )
			return false;
	}

	return true;
}


bool Graphics_CreateLightLayout( UniformBufferArray_t& srBuffer, const char* spBufferName, size_t sBufferSize )
{
	CreateVariableDescLayout_t createLayout{};
	createLayout.aType    = EDescriptorType_UniformBuffer;
	createLayout.aStages  = ShaderStage_Vertex | ShaderStage_Fragment;
	createLayout.aBinding = 0;
	createLayout.aCount   = 2;

	srBuffer.aLayout      = render->CreateVariableDescLayout( createLayout );

	if ( srBuffer.aLayout == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Light Layout\n" );
		return false;
	}

	AllocVariableDescLayout_t allocLayout{};
	allocLayout.aLayout   = srBuffer.aLayout;
	allocLayout.aCount    = srBuffer.aBuffers.size();
	allocLayout.aSetCount = 2;

	if ( !render->AllocateVariableDescLayout( allocLayout, gLayoutViewProjSets ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to allocate Light Layout\n" );
		return false;
	}

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


bool Graphics_CreateDescriptorSets()
{
	// TODO: just create the sampler here and have a
	// Graphics_LoadTexture() function to auto add to the image sampler sets
	gLayoutSampler = render->GetSamplerLayout();
	render->GetSamplerSets( gLayoutSamplerSets );

	// ------------------------------------------------------
	// Create ViewProjetion matrix UBO
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
			Handle buffer = render->CreateBuffer( "View * Projection Buffer", sizeof( glm::mat4 ), EBufferFlags_Uniform, EBufferMemory_Host );

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
		if ( !Graphics_CreateLightLayout( gUniformLightInfo, "Light Info Buffer", sizeof( LightInfo_t ) ) )
			return false;

		if ( !Graphics_CreateLightLayout( gUniformLightWorld, "Light World Buffer", sizeof( LightWorld_t ) ) )
			return false;

		if ( !Graphics_CreateLightLayout( gUniformLightPoint, "Light Point Buffer", sizeof( LightPoint_t ) ) )
			return false;

		if ( !Graphics_CreateLightLayout( gUniformLightCone, "Light Cone Buffer", sizeof( LightCone_t ) ) )
			return false;

		if ( !Graphics_CreateLightLayout( gUniformLightCapsule, "Light Capsule Buffer", sizeof( LightCapsule_t ) ) )
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


// Do Rendering with shader system and user land meshes
void Graphics_SetupGBuffer( Handle cmd )
{
	// only ones that work with G-Buffer right now
	static Handle skybox    = Graphics_GetShader( "skybox" );
	static Handle basic3d   = Graphics_GetShader( "basic_3d" );

	bool          hasSkybox = false;

	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	// TODO: check if shader needs dynamic viewport and/or scissor
	Viewport_t viewPort{};
	viewPort.x        = 0.f;
	viewPort.y        = height;
	viewPort.minDepth = 0.f;
	viewPort.maxDepth = 1.f;
	viewPort.width    = width;
	viewPort.height   = height * -1.f;

	render->CmdSetViewport( cmd, 0, &viewPort, 1 );

	Rect2D_t rect{};
	rect.aOffset.x = 0;
	rect.aOffset.y = 0;
	rect.aExtent.x = width;
	rect.aExtent.y = height;

	render->CmdSetScissor( cmd, 0, &rect, 1 );

	ModelSurfaceDraw_t* prevRenderable = nullptr;

	// TODO: still could be better, but it's better than what we used to have
	for ( auto& [ shader, renderList ] : gMeshDrawList )
	{
		if ( shader == skybox )
		{
			hasSkybox = true;
			continue;
		}

		if ( shader != basic3d )
			continue;

		if ( !Shader_Bind( cmd, gCmdIndex, shader ) )
			continue;

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

	// Draw Skybox - and set depth for skybox
	if ( hasSkybox )
	{
		viewPort.minDepth = 0.999f;
		viewPort.maxDepth = 1.f;

		render->CmdSetViewport( cmd, 0, &viewPort, 1 );

		if ( !Shader_Bind( cmd, gCmdIndex, skybox ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to bind skybox shader\n" );
		}
		else
		{
			auto& renderList = gMeshDrawList[ skybox ];

			for ( auto& renderable : renderList )
			{
				// if ( prevRenderable != renderable || prevMatIndex != matIndex )
				// if ( prevRenderable && prevRenderable->aModel != renderable->aModel || prevMatIndex != matIndex )
				if ( !prevRenderable || prevRenderable->apDraw->aModel != renderable.apDraw->aModel || prevRenderable->aSurface != renderable.aSurface )
				{
					prevRenderable = &renderable;
					Graphics_BindModel( cmd, renderable );
				}

				if ( !Shader_PreRenderableDraw( cmd, gCmdIndex, skybox, renderable ) )
					continue;

				Graphics_CmdDrawModel( cmd, renderable );
			}
		}
	}
}


// Do Rendering with shader system and user land meshes
void Graphics_Render( Handle cmd )
{
	// here we go again
	static Handle skybox    = Graphics_GetShader( "skybox" );
	static Handle basic3d   = Graphics_GetShader( "basic_3d" );

	bool          hasSkybox = false;

	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	Rect2D_t rect{};
	rect.aOffset.x = 0;
	rect.aOffset.y = 0;
	rect.aExtent.x = width;
	rect.aExtent.y = height;

	render->CmdSetScissor( cmd, 0, &rect, 1 );

	// TODO: check if shader needs dynamic viewport and/or scissor
	// normal viewport
	Viewport_t viewPort{};
	viewPort.x        = 0.f;
	viewPort.y        = 0.f;
	viewPort.minDepth = 0.f;
	viewPort.maxDepth = 0.9f;
	viewPort.width    = width;
	viewPort.height   = height;

	render->CmdSetViewport( cmd, 0, &viewPort, 1 );

	Shader_DeferredFS_Draw( cmd, gCmdIndex );

	// flip viewport
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

		if ( shader == basic3d )
			continue;

		if ( !Shader_Bind( cmd, gCmdIndex, shader ) )
			continue;

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

	return;

	// un-flip viewport
	viewPort.x        = 0.f;
	viewPort.y        = 0.f;
	viewPort.minDepth = 0.f;
	viewPort.maxDepth = 1.f;
	viewPort.width    = width;
	viewPort.height   = height;

	render->CmdSetViewport( cmd, 0, &viewPort, 1 );

	Shader_UI_Draw( cmd, gCmdIndex, gImGuiTextures[ 0 ] );
}

void Graphics_PrepareDrawData()
{
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
	for ( LightBase_t* light : gDirtyLights )
	{
		if ( typeid( *light ) == typeid( LightWorld_t ) )
		{
		}
		else if ( typeid( *light ) == typeid( LightPoint_t ) )
		{
		}
		else if ( typeid( *light ) == typeid( LightCone_t ) )
		{
		}
		else if ( typeid( *light ) == typeid( LightCapsule_t ) )
		{
		}
	}

	gDirtyLights.clear();

	if ( gNeedLightInfoUpdate )
	{
		gNeedLightInfoUpdate = false;
		// update Light Info UBO
	}

	if ( gViewProjMatUpdate )
	{
		gViewProjMatUpdate = false;
		for ( size_t i = 0; i < gViewProjBuffers.size(); i++ )
			render->MemWriteBuffer( gViewProjBuffers[ i ], sizeof( glm::mat4 ), &gViewProjMat );
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
	gViewProjMat = srMat;
	gViewProjMatUpdate = true;
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
	Handle stagingBuffer = render->CreateBuffer( sBufferSize, sUsage | EBufferFlags_TransferSrc, EBufferMemory_Host );

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
	VertexData_t& vertData     = srMesh.aVertexData;

	VertexFormat  shaderFormat = Mat_GetVertexFormat( srMesh.aMaterial );

	// wtf
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


LightWorld_t* Graphics_CreateLightWorld()
{
	LightWorld_t* light = new LightWorld_t;
	gLights.push_back( light );
	gNeedLightInfoUpdate = true;
	return light;
}


LightPoint_t* Graphics_CreateLightPoint()
{
	LightPoint_t* light = new LightPoint_t;
	gLights.push_back( light );
	gNeedLightInfoUpdate = true;
	return light;
}


LightCone_t* Graphics_CreateLightCone()
{
	LightCone_t* light = new LightCone_t;
	gLights.push_back( light );
	gNeedLightInfoUpdate = true;
	return light;
}


LightCapsule_t* Graphics_CreateLightCapsule()
{
	LightCapsule_t* light = new LightCapsule_t;
	gLights.push_back( light );
	gNeedLightInfoUpdate = true;
	return light;
}


void Graphics_UpdateLight( LightBase_t* spLight )
{
	if ( !spLight )
		return;

	gDirtyLights.push_back( spLight );
}


void Graphics_DestroyLight( LightBase_t* spLight )
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

