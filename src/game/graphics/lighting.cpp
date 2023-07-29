#include "graphics.h"
#include "graphics_int.h"
#include "lighting.h"
#include "types/transform.h"

// --------------------------------------------------------------------------------------
// Lighting

// IDEA:
// For the applying the lights in the shader code
// Don't pass in a count of lights, when you delete lights
// instead, just mark them disabled
// and use a fixed array of lights instead?
// 

UniformBufferArray_t                               gUniformLightInfo;
UniformBufferArray_t                               gUniformLights[ ELightType_Count ];
UniformBufferArray_t                               gUniformShadows;

static std::unordered_map< Light_t*, Handle >      gLightBuffers;
std::unordered_map< Light_t*, ShadowMap_t >        gLightShadows;

constexpr int                                      MAX_LIGHTS = MAX_VIEW_INFO_BUFFERS;
std::vector< Light_t* >                            gLights;
static std::vector< Light_t* >                     gDirtyLights;
static std::vector< Light_t* >                     gDestroyLights;

static LightInfo_t                                 gLightInfo;
static Handle                                      gLightInfoStagingBuffer = InvalidHandle;
static Handle                                      gLightInfoBuffer        = InvalidHandle;

static bool                                        gNeedLightInfoUpdate    = false;

Handle                                             gRenderPassShadow;

extern int                                         gViewInfoCount;
extern std::vector< Handle >                       gViewInfoBuffers;
extern std::vector< ViewRenderList_t >             gViewRenderLists;

extern void                                        Shader_ShadowMap_SetViewInfo( int sViewInfo );

// --------------------------------------------------------------------------------------

CONVAR( r_shadowmap_size, 2048 );

CONVAR( r_shadowmap_fov_hack, 90.f );
CONVAR( r_shadowmap_nearz, 1.f );
CONVAR( r_shadowmap_farz, 2000.f );

CONVAR( r_shadowmap_othro_left, -1000.f );
CONVAR( r_shadowmap_othro_right, 1000.f );
CONVAR( r_shadowmap_othro_bottom, -1000.f );
CONVAR( r_shadowmap_othro_top, 1000.f );

CONVAR( r_shadowmap_constant, 16.f );  // 1.25f
CONVAR( r_shadowmap_clamp, 0.f );
CONVAR( r_shadowmap_slope, 1.75f );

// --------------------------------------------------------------------------------------


bool Graphics_CreateLightDescriptorSets()
{
	gUniformLightInfo.aSets.resize( 1 );
	if ( !Graphics_CreateVariableUniformLayout( gUniformLightInfo, "Light Info Layout", "Light Info Set", 1 ) )
		return false;

	for ( int i = 0; i < ELightType_Count; i++ )
		gUniformLights[ i ].aSets.resize( 1 );

	if ( !Graphics_CreateVariableUniformLayout( gUniformLights[ ELightType_Directional ], "Light Directional Layout", "Light Directional Set", MAX_LIGHTS ) )
		return false;

	if ( !Graphics_CreateVariableUniformLayout( gUniformLights[ ELightType_Point ], "Light Point Layout", "Light Point Set", MAX_LIGHTS ) )
		return false;

	if ( !Graphics_CreateVariableUniformLayout( gUniformLights[ ELightType_Cone ], "Light Cone Layout", "Light Cone Set", MAX_LIGHTS ) )
		return false;

	//if ( !Graphics_CreateVariableUniformLayout( gUniformLights[ ELightType_Capsule ], "Light Capsule Layout", "Light Capsule Set", MAX_LIGHTS ) )
	//	return false;

	// ------------------------------------------------------
	// Create Shadow Map Layout

	gUniformShadows.aSets.resize( 1 );
	if ( !Graphics_CreateVariableUniformLayout( gUniformShadows, "Shadow Map Layout", "Shadow Map Set", MAX_LIGHTS ) )
		return false;

	// ------------------------------------------------------
	// Create Light Info Buffer

	gLightInfoStagingBuffer = render->CreateBuffer( "Light Info Buffer", sizeof( LightInfo_t ), EBufferFlags_Storage | EBufferFlags_TransferSrc, EBufferMemory_Host );
	gLightInfoBuffer        = render->CreateBuffer( "Light Info Buffer", sizeof( LightInfo_t ), EBufferFlags_Storage | EBufferFlags_TransferDst, EBufferMemory_Device );

	if ( !gLightInfoBuffer || !gLightInfoStagingBuffer )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Light Info Uniform Buffer\n" );
		return false;
	}

	// Update the Descriptor Sets
	UpdateVariableDescSet_t update{};

	for ( size_t i = 0; i < gUniformLightInfo.aSets.size(); i++ )
		update.aDescSets.push_back( gUniformLightInfo.aSets[ i ] );

	update.aType = EDescriptorType_StorageBuffer;
	update.aBuffers.push_back( gLightInfoBuffer );
	render->UpdateVariableDescSet( update );

	return true;
}


void Graphics_DestroyShadowRenderPass()
{
	if ( gRenderPassShadow != InvalidHandle )
		render->DestroyRenderPass( gRenderPassShadow );
}


bool Graphics_CreateShadowRenderPass()
{
	RenderPassCreate_t create{};

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

	return gRenderPassShadow;
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

	createData.aDepthCompare   = true;

	shadowMap.aTexture         = render->CreateTexture( texCreate, createData );

	// Create Framebuffer
	CreateFramebuffer_t frameBufCreate{};
	frameBufCreate.aRenderPass        = gRenderPassShadow;  // ImGui will be drawn onto the graphics RenderPass
	frameBufCreate.aSize              = shadowMap.aSize;

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

	if ( it->second.aViewInfoIndex )
	{
		if ( gViewInfo.size() > it->second.aViewInfoIndex )
		{
			vec_remove_index( gViewInfo, it->second.aViewInfoIndex );
			gViewInfoCount--;
			gViewInfoUpdate = true;
		}
	}

	gLightShadows.erase( spLight );
}


void Graphics_UpdateLightDescSets( ELightType sLightType )
{
	PROF_SCOPE();

	// update the descriptor sets
	UpdateVariableDescSet_t update{};
	update.aType = EDescriptorType_UniformBuffer;

	for ( size_t i = 0; i < gUniformLights[ sLightType ].aSets.size(); i++ )
		update.aDescSets.push_back( gUniformLights[ sLightType ].aSets[ i ] );

	for ( const auto& [ light, bufferHandle ] : gLightBuffers )
	{
		if ( light->aType == sLightType )
			update.aBuffers.push_back( bufferHandle );
	}

	// Update all light indexes for the shaders
	render->UpdateVariableDescSet( update );
}


Handle Graphics_AddLightBuffer( const char* spBufferName, size_t sBufferSize, Light_t* spLight )
{
	Handle buffer = render->CreateBuffer( spBufferName, sBufferSize, EBufferFlags_Storage, EBufferMemory_Host );

	if ( buffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Light Uniform Buffer\n" );
		return InvalidHandle;
	}

	gLightBuffers[ spLight ] = buffer;

	// update the descriptor sets
	UpdateVariableDescSet_t update{};
	update.aType = EDescriptorType_StorageBuffer;

	for ( size_t i = 0; i < gUniformLights[ spLight->aType ].aSets.size(); i++ )
		update.aDescSets.push_back( gUniformLights[ spLight->aType ].aSets[ i ] );

	for ( const auto& [ light, bufferHandle ] : gLightBuffers )
	{
		if ( light->aType == spLight->aType )
			update.aBuffers.push_back( bufferHandle );
	}

	// Update all light indexes for the shaders
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

	if ( spLight->aType < 0 || spLight->aType > ELightType_Count )
	{
		Log_ErrorF( gLC_ClientGraphics, "Unknown Light Buffer: %d\n", spLight->aType );
		return;
	}

	UniformBufferArray_t* buffer = &gUniformLights[ spLight->aType ];
	switch ( spLight->aType )
	{
		default:
			Assert( false );
			Log_ErrorF( gLC_ClientGraphics, "Unknown Light Buffer: %d\n", spLight->aType );
			return;
		case ELightType_Directional:
			gLightInfo.aCountWorld--;
			break;
		case ELightType_Point:
			gLightInfo.aCountPoint--;
			break;
		case ELightType_Cone:
			gLightInfo.aCountCone--;
			break;
		//case ELightType_Capsule:
		//	gLightInfo.aCountCapsule--;
		//	break;
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

	Log_Dev( gLC_ClientGraphics, 1, "Deleted Light\n" );
}


void Graphics_UpdateLightBuffer( Light_t* spLight )
{
	PROF_SCOPE();

	Handle buffer = InvalidHandle;

	auto   it     = gLightBuffers.find( spLight );

	if ( it == gLightBuffers.end() )
	{
		switch ( spLight->aType )
		{
			case ELightType_Directional:
				buffer = Graphics_AddLightBuffer( "Light Directional Buffer", sizeof( UBO_LightDirectional_t ), spLight );
				gLightInfo.aCountWorld++;
				break;
			case ELightType_Point:
				buffer = Graphics_AddLightBuffer( "Light Point Buffer", sizeof( UBO_LightPoint_t ), spLight );
				gLightInfo.aCountPoint++;
				break;
			case ELightType_Cone:
				buffer = Graphics_AddLightBuffer( "Light Cone Buffer", sizeof( UBO_LightCone_t ), spLight );
				gLightInfo.aCountCone++;
				break;
			//case ELightType_Capsule:
			//	buffer = Graphics_AddLightBuffer( "Light Capsule Buffer", sizeof( UBO_LightCapsule_t ), spLight );
			//	gLightInfo.aCountCapsule++;
			//	break;
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
			light.aColor.w = spLight->aEnabled ? spLight->aColor.w : 0.f;

			glm::mat4 matrix;
			Util_ToMatrix( matrix, spLight->aPos, spLight->aAng );
			Util_GetMatrixDirectionNoScale( matrix, nullptr, nullptr, &light.aDir );

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

			render->BufferWrite( buffer, sizeof( light ), &light );
			break;
		}
		case ELightType_Point:
		{
			UBO_LightPoint_t light;
			light.aColor.x = spLight->aColor.x;
			light.aColor.y = spLight->aColor.y;
			light.aColor.z = spLight->aColor.z;
			light.aColor.w = spLight->aEnabled ? spLight->aColor.w : 0.f;

			light.aPos     = spLight->aPos;
			light.aRadius  = spLight->aRadius;

			render->BufferWrite( buffer, sizeof( light ), &light );
			break;
		}
		case ELightType_Cone:
		{
			UBO_LightCone_t light;
			light.aColor.x = spLight->aColor.x;
			light.aColor.y = spLight->aColor.y;
			light.aColor.z = spLight->aColor.z;
			light.aColor.w = spLight->aEnabled ? spLight->aColor.w : 0.f;

			light.aPos     = spLight->aPos;
			light.aFov.x   = glm::radians( spLight->aInnerFov );
			light.aFov.y   = glm::radians( spLight->aOuterFov );

			glm::mat4 matrix;
			Util_ToMatrix( matrix, spLight->aPos, spLight->aAng );
			Util_GetMatrixDirectionNoScale( matrix, nullptr, nullptr, &light.aDir );
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
			render->BufferWrite( shadowBuffer, sizeof( UBO_ViewInfo_t ), &gViewInfo[ shadowMap.aViewInfoIndex ] );

			// get shadow map view info
			light.aViewInfo                               = shadowMap.aViewInfoIndex;
			light.aShadow                                 = render->GetTextureIndex( shadowMap.aTexture );

			gViewInfo[ shadowMap.aViewInfoIndex ].aActive = spLight->aShadow && spLight->aEnabled;
#endif

			// Update Light Buffer
			render->BufferWrite( buffer, sizeof( light ), &light );
			break;
		}
		//case ELightType_Capsule:
		//{
		//	UBO_LightCapsule_t light;
		//	light.aColor.x   = spLight->aColor.x;
		//	light.aColor.y   = spLight->aColor.y;
		//	light.aColor.z   = spLight->aColor.z;
		//	light.aColor.w   = spLight->aEnabled ? spLight->aColor.w : 0.f;
		//
		//	light.aPos       = spLight->aPos;
		//	light.aLength    = spLight->aLength;
		//	light.aThickness = spLight->aRadius;
		//
		//	glm::mat4 matrix;
		//	Util_ToMatrix( matrix, spLight->aPos, spLight->aAng );
		//	Util_GetMatrixDirectionNoScale( matrix, nullptr, nullptr, &light.aDir );
		//
		//	// Util_GetDirectionVectors( spLight->aAng, nullptr, nullptr, &light.aDir );
		//	// Util_GetDirectionVectors( spLight->aAng, &light.aDir );
		//
		//	render->BufferWrite( buffer, sizeof( light ), &light );
		//	break;
		//}
	}
}


// TODO: this should be returning Handle's
Light_t* Graphics_CreateLight( ELightType sType )
{
	Light_t* light = new Light_t;

	if ( !light )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create light\n" );
		return nullptr;
	}

	light->aType         = sType;
	gNeedLightInfoUpdate = true;

	gLights.push_back( light );
	gDirtyLights.push_back( light );

	Log_Dev( gLC_ClientGraphics, 1, "Created Light\n" );

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

	Log_Dev( gLC_ClientGraphics, 1, "Queued Light For Deletion\n" );

	vec_remove_if( gDirtyLights, spLight );
}


void Graphics_PrepareLights()
{
	PROF_SCOPE();

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
		render->BufferWrite( gLightInfoStagingBuffer, sizeof( LightInfo_t ), &gLightInfo );
		render->BufferCopy( gLightInfoStagingBuffer, gLightInfoBuffer, sizeof( LightInfo_t ) );
	}
}


bool Graphics_IsUsingShadowMaps()
{
	PROF_SCOPE();

	bool usingShadow = false;
	for ( const auto& [ light, shadowMap ] : gLightShadows )
	{
		if ( !light->aEnabled || !light->aShadow )
			continue;

		usingShadow = true;
		break;
	}

	return usingShadow;
}


void Graphics_RenderShadowMap( Handle cmd, Light_t* spLight, const ShadowMap_t& srShadowMap )
{
	PROF_SCOPE();

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


void Graphics_DrawShadowMaps( Handle cmd )
{
	PROF_SCOPE();

	// make sure we have the shadow map shader
	static Handle shadowMapShader = Graphics_GetShader( "__shadow_map" );
	static bool   warn            = false;

	if ( shadowMapShader == CH_INVALID_HANDLE )
	{
		if ( !warn )
		{
			Log_Error( gLC_ClientGraphics, "Missing ShadowMap Shader\n" );
			warn = true;
		}

		return;
	}

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

		render->BeginRenderPass( cmd, renderPassBegin );
		Graphics_RenderShadowMap( cmd, light, shadowMap );
		render->EndRenderPass( cmd );
	}
}

