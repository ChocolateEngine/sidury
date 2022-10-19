#include "core/core.h"
#include "igui.h"
#include "render/irender.h"
#include "graphics.h"
#include "mesh_builder.h"
#include "imgui/imgui.h"

#include <forward_list>


LOG_REGISTER_CHANNEL_EX( gLC_ClientGraphics, "ClientGraphics", LogColor::DarkMagenta )

// --------------------------------------------------------------------------------------
// Interfaces

extern BaseGuiSystem*        gui;
extern IRender*              render;

// --------------------------------------------------------------------------------------

void                         Graphics_LoadObj( const std::string& srPath, Model* spModel );
// void Graphics_LoadGltf( const std::string& srPath, const std::string& srExt, Model* spModel );

// shaders, fun
Handle                       Shader_Basic3D_Create( Handle sRenderPass, bool sRecreate );
void                         Shader_Basic3D_Destroy();
void                         Shader_Basic3D_Bind( Handle cmd, size_t sCmdIndex );
void                         Shader_Basic3D_PushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo );
void                         Shader_Basic3D_ResetPushData();
void                         Shader_Basic3D_SetupPushData( ModelSurfaceDraw_t& srDrawInfo );
VertexFormat                 Shader_Basic3D_GetVertexFormat();

Handle                       Shader_UI_Create( Handle sRenderPass, bool sRecreate );
void                         Shader_UI_Destroy();
void                         Shader_UI_Draw( Handle cmd, size_t sCmdIndex, Handle shColor );

// --------------------------------------------------------------------------------------
// Rendering

static std::vector< Handle > gCommandBuffers;
static size_t                gCmdIndex = 0;

static Handle                gRenderPassGraphics;
static Handle                gRenderPassUI;  // TODO
// static Handle                                                       gRenderPassShadow;  // maybe for future?

// ew
static std::unordered_map<
  Handle,
  std::forward_list< ModelSurfaceDraw_t > >
												 gMeshDrawList;

static glm::mat4                                 gViewProjMat;

// stores backbuffer color and depth
static Handle                                    gBackBuffer[ 2 ];
static Handle                                    gBackBufferTex[ 3 ];
static Handle                                    gImGuiBuffer[ 2 ];
static Handle                                    gImGuiTextures[ 2 ];

// temp shader
static Handle                                    gTempShader = InvalidHandle;
static Handle                                    gUIShader   = InvalidHandle;

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
		Graphics_LoadObj( fullPath, model );
	}
	// else if ( fileExt == "glb" || fileExt == "gltf" )
	// {
	// 	handle = gModels.Add( model );
	// 	Graphics_LoadGltf( fullPath, fileExt, model );
	// }
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


// Get Fallback Texture if the texture doesn't exist
Handle Graphics_GetMissingTexture()
{
	return InvalidHandle;
}


void Graphics_DestroyRenderTargets()
{
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
}


bool Graphics_CreateRenderTargets()
{
	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );
	
	// Create ImGui Color Attachment
	TextureCreateInfo_t texCreate{};
	texCreate.aSize     = { width, height };
	texCreate.aFormat   = GraphicsFmt::BGRA8888_UNORM;
	texCreate.aUseMSAA  = false;
	texCreate.aViewType = EImageView_2D;
	texCreate.aUsage    = EImageUsage_AttachColor | EImageUsage_Sampled;

	gImGuiTextures[ 0 ] = render->CreateTexture( texCreate );

	// Create ImGui Depth Stencil Attachment
	texCreate.aFormat   = render->GetSwapFormatDepth();
	texCreate.aUsage    = EImageUsage_AttachDepthStencil | EImageUsage_Sampled;

	gImGuiTextures[ 1 ] = render->CreateTexture( texCreate );

	// Create a new framebuffer for ImGui to draw on
	CreateFramebuffer_t frameBufCreate{};
	frameBufCreate.aRenderPass = gRenderPassUI;  // ImGui will be drawn onto the graphics RenderPass
	frameBufCreate.aSize       = { width, height };

	// Create Color
	frameBufCreate.aPass.aAttachColors.push_back( gImGuiTextures[ 0 ] );
	frameBufCreate.aPass.aAttachDepth = gImGuiTextures[ 1 ];
	gImGuiBuffer[ 0 ] = render->CreateFramebuffer( frameBufCreate );

	// Create Depth
	// frameBufCreate.aPass.aAttachColors.clear();
	// gImGuiBuffer[ 1 ]                 = render->CreateFramebuffer( frameBufCreate );
	gImGuiBuffer[ 1 ] = render->CreateFramebuffer( frameBufCreate );

#if 0
	CreateRenderTarget_t createTarget{};
	createTarget.aFormat  = GraphicsFmt::BGRA8888_UNORM;
	createTarget.aSize    = { width, height };
	createTarget.aUseMSAA = false;
	createTarget.aDepth   = false;

	gImGuiBuffer[ 0 ]     = render->CreateRenderTarget( createTarget );

	createTarget.aDepth   = true;
	gImGuiBuffer[ 1 ]     = render->CreateRenderTarget( createTarget );
#endif

	if ( gImGuiBuffer[ 0 ] == InvalidHandle || gImGuiBuffer[ 1 ] == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create render targets\n" );
		return false;
	}

	return true;
}


void Graphics_DestroyRenderPasses()
{
	render->DestroyRenderPass( gRenderPassUI );
}


bool Graphics_CreateRenderPasses()
{
	RenderPassCreate_t create{};

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

	gRenderPassUI = render->CreateRenderPass( create );

	return gRenderPassUI != InvalidHandle;
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
		if ( !render->InitImGui( gRenderPassUI ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to re-init ImGui for Vulkan\n" );
			return;
		}

		if ( !( gUIShader = Shader_UI_Create( gRenderPassGraphics, true ) ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create ui shader\n" );
			return;
		}

		if ( !( gTempShader = Shader_Basic3D_Create( gRenderPassGraphics, true ) ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create temp shader\n" );
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

	if ( !( gUIShader = Shader_UI_Create( gRenderPassGraphics, false ) ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create ui shader\n" );
		return false;
	}

	if ( !( gTempShader = Shader_Basic3D_Create( gRenderPassGraphics, false ) ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create temp shader\n" );
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

	return render->InitImGui( gRenderPassUI );
	// return render->InitImGui( gRenderPassGraphics );
}


void Graphics_Shutdown()
{
}


void Graphics_Reset()
{
	render->Reset();
	// Graphics_OnResetCallback();
}


void Graphics_NewFrame()
{
	render->NewFrame();
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


void Graphics_BindBuffers( Handle cmd, ModelSurfaceDraw_t& srDrawInfo )
{
	// get model and check if it's nullptr
	if ( srDrawInfo.apDraw->aModel == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_BindBuffers: model handle is InvalidHandle\n" );
		return;
	}

	Model* model = nullptr;
	if ( !gModels.Get( srDrawInfo.apDraw->aModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_BindBuffers: model is nullptr\n" );
		return;
	}

	if ( srDrawInfo.aSurface > model->aMeshes.size() )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_BindBuffers: model surface index is out of range!\n" );
		return;
	}

	Mesh& mesh = model->aMeshes[ srDrawInfo.aSurface ];
	
	// Bind the mesh's vertex and index buffers

	// um
	// std::vector< size_t > offsets( mesh.aVertexBuffers.size() );

	size_t* offsets = (size_t*)CH_STACK_ALLOC( sizeof( size_t ) * mesh.aVertexBuffers.size() );
	if ( offsets == nullptr )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_BindBuffers: Failed to allocate vertex buffer offsets!\n" );
		return;
	}

	memset( offsets, 0, sizeof( size_t ) * mesh.aVertexBuffers.size() );

	render->CmdBindVertexBuffers( cmd, 0, mesh.aVertexBuffers.size(), mesh.aVertexBuffers.data(), offsets );

	// TODO: store index type here somewhere
	render->CmdBindIndexBuffer( cmd, mesh.aIndexBuffer, 0, EIndexType_U32 );

	CH_STACK_FREE( offsets );
}


// Do Rendering with shader system and user land meshes
void Graphics_Render( Handle cmd )
{
	ModelSurfaceDraw_t* prevRenderable = nullptr;

	int                 width = 0, height = 0;
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

	// TODO: still could be better, but it's better than what we used to have
	for ( auto& [ shader, renderList ] : gMeshDrawList )
	{
		if ( !render->CmdBindPipeline( cmd, shader ) )
			continue;

		Shader_Basic3D_Bind( cmd, gCmdIndex );

		for ( auto& renderable : renderList )
		{
			// if ( prevRenderable != renderable || prevMatIndex != matIndex )
			// if ( prevRenderable && prevRenderable->aModel != renderable->aModel || prevMatIndex != matIndex )
			if ( !prevRenderable || prevRenderable->apDraw->aModel != renderable.apDraw->aModel || prevRenderable->aSurface != renderable.aSurface )
			{
				prevRenderable = &renderable;
				Graphics_BindBuffers( cmd, renderable );
			}

			// bind shader info (blech)
			Shader_Basic3D_PushConstants( cmd, gCmdIndex, renderable );
			Graphics_CmdDrawModel( cmd, renderable );
		}
	}

	// return;

	// un-flip viewport
	viewPort.x        = 0.f;
	viewPort.y        = 0.f;
	viewPort.minDepth = 0.f;
	viewPort.maxDepth = 1.f;
	viewPort.width    = width;
	viewPort.height   = height;

	render->CmdSetViewport( cmd, 0, &viewPort, 1 );

	Shader_UI_Draw( cmd, gCmdIndex, gImGuiTextures[ 0 ] );
	// Shader_UI_Draw( cmd, gCmdIndex, gBackBufferTex[ 0 ] );
}


void Graphics_PrepareDrawData()
{
	ImGui::Render();

	Shader_Basic3D_ResetPushData();

	for ( auto& [ shader, renderList ] : gMeshDrawList )
	{
		for ( auto& renderable : renderList )
		{
			Shader_Basic3D_SetupPushData( renderable );
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

		// ImGui RenderPass
		renderPassBegin.aRenderPass  = gRenderPassUI;
		renderPassBegin.aFrameBuffer = gImGuiBuffer[ gCmdIndex ];
		renderPassBegin.aClearColor  = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear       = true;

		render->BeginRenderPass( c, renderPassBegin );
		render->DrawImGui( ImGui::GetDrawData(), c );
		render->EndRenderPass( c );

		// Main RenderPass
		renderPassBegin.aRenderPass  = gRenderPassGraphics;
		renderPassBegin.aFrameBuffer = gBackBuffer[ gCmdIndex ];
		renderPassBegin.aClearColor  = { 0.f, 0.f, 0.f, 1.f };
		renderPassBegin.aClear       = true;

		render->BeginRenderPass( c, renderPassBegin );  // VK_SUBPASS_CONTENTS_INLINE

		Graphics_Render( c );

		// TODO: this should be a on a separate render pass that doesn't use SRGB or MSAA
		// render->DrawImGui( ImGui::GetDrawData(), c );

		render->EndRenderPass( c );

		render->EndCommandBuffer( c );
	}

	gMeshDrawList.clear();

	render->Present();
	// render->UnlockGraphicsMutex();
}


void Graphics_SetViewProjMatrix( const glm::mat4& srMat )
{
	gViewProjMat = srMat;
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

		// if ( Mat_GetShader( mat ) == InvalidHandle )
		// {
		// 	Log_Error( gLC_ClientGraphics, "Material has no shader!\n" );
		// 	continue;
		// }

		// auto search = aDrawList.find( mat->apShader );
		//
		// if ( search != aDrawList.end() )
		// 	search->second.push_back( renderable );
		// else
		// 	aDrawList[mat->apShader].push_back( renderable );

		// aDrawList[mat->apShader].push_front( renderable );

		gMeshDrawList[ Mat_GetShader( mat ) ].emplace_front( spDrawInfo, i );
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


// ---------------------------------------------------------------------------------------
// Shaders


Handle Graphics_GetShader( std::string_view name )
{
	Log_Msg( gLC_ClientGraphics, "Graphics_GetShader: IMPLEMENT A SHADER SYSTEM AAAA\n" );
	return gTempShader;
}


// ---------------------------------------------------------------------------------------
// Buffers

// sBufferSize is sizeof(element) * count
static Handle CreateModelBuffer( void* spData, size_t sBufferSize, EBufferFlags sUsage )
{
	Handle stagingBuffer = render->CreateBuffer( sBufferSize, sUsage | EBufferFlags_TransferSrc, EBufferMemory_Host );

	// Copy Data to Buffer
	render->MemWriteBuffer( stagingBuffer, sBufferSize, spData );

	Handle deviceBuffer = render->CreateBuffer( sBufferSize, sUsage | EBufferFlags_TransferDst, EBufferMemory_Device );

	// Copy Local Buffer data to Device
	render->MemCopyBuffer( stagingBuffer, deviceBuffer, sBufferSize );

	render->DestroyBuffer( stagingBuffer );

	return deviceBuffer;
}


void Graphics_CreateVertexBuffers( Mesh& srMesh )
{
	VertexData_t& vertData     = srMesh.aVertexData;

	// HACK HACK HACK HACK HACK !!!!!!!!!!!!!!!!!!!!!!
	VertexFormat  shaderFormat = Shader_Basic3D_GetVertexFormat();

	// wtf
	if ( shaderFormat == VertexFormat_None )
		return;

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
		auto&  data   = attribs[ j ];

		Handle buffer = CreateModelBuffer(
		  data->apData,
		  Graphics_GetVertexAttributeSize( data->aAttrib ) * vertData.aCount,
		  EBufferFlags_Vertex );

		srMesh.aVertexBuffers.push_back( buffer );
	}
}


void Graphics_CreateIndexBuffer( Mesh& srMesh )
{
	srMesh.aIndexBuffer = CreateModelBuffer(
	  srMesh.aIndices.data(),
	  sizeof( u32 ) * srMesh.aIndices.size(),
	  EBufferFlags_Index );
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

