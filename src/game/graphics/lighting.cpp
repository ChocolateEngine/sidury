#include "graphics.h"
#include "graphics_int.h"
#include "lighting.h"
#include "types/transform.h"

// --------------------------------------------------------------------------------------
// Lighting

// TODO: use handles for lights instead of storing pointers to them

// static std::unordered_map< Light_t*, Handle >      gLightBuffers;
static std::unordered_map< Light_t*, u32 >  gLightSlots;
std::unordered_map< Light_t*, ShadowMap_t > gLightShadows;

std::vector< Light_t* >                     gLights;
static std::vector< Light_t* >              gDirtyLights;
static std::vector< Light_t* >              gDestroyLights;

Handle                                      gRenderPassShadow;

extern void                                 Shader_ShadowMap_SetViewInfo( int sViewInfo );

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


const char* gLightTypeStr[] = {
	"World",
	"Point",
	"Cone",
	// "Capsule",
};


static_assert( CH_ARR_SIZE( gLightTypeStr ) == ELightType_Count );


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
	ViewportShader_t* viewport      = nullptr;
	u32               viewportIndex = Graphics_CreateViewport( &viewport );

	if ( viewport == nullptr )
	{
		Log_Error( gLC_ClientGraphics, "Failed to allocate viewport for shadow map\n" );
		return;
	}

	ShadowMap_t& shadowMap     = gLightShadows[ spLight ];
	shadowMap.aSize            = { r_shadowmap_size.GetFloat(), r_shadowmap_size.GetFloat() };
	shadowMap.aViewInfoIndex   = viewportIndex;

	viewport->aShaderOverride  = Graphics_GetShader( "__shadow_map" );
	viewport->aSize            = shadowMap.aSize;
	viewport->aActive          = false;

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

	shadowMap.aTexture         = Graphics_CreateTexture( texCreate, createData );

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
		Graphics_FreeTexture( it->second.aTexture );

	if ( it->second.aViewInfoIndex != UINT32_MAX )
	{
		Graphics_FreeViewport( it->second.aViewInfoIndex );
	}

	gLightShadows.erase( spLight );
}


void Graphics_UpdateLightDescSets( ELightType sLightType )
{
#if 0
	PROF_SCOPE();

	// update the descriptor sets
	WriteDescSet_t update{};
	update.aType = EDescriptorType_UniformBuffer;

	for ( size_t i = 0; i < gUniformLights[ sLightType ].aSets.size(); i++ )
		update.aDescSets.push_back( gUniformLights[ sLightType ].aSets[ i ] );

	for ( const auto& [ light, bufferHandle ] : gLightBuffers )
	{
		if ( light->aType == sLightType )
			update.aBuffers.push_back( bufferHandle );
	}

	// Update all light indexes for the shaders
	render->UpdateDescSet( update );
#endif
}


// Returns the light index
u32 Graphics_AllocateLightSlot( Light_t* spLight )
{
	u32 index = CH_SHADER_CORE_SLOT_INVALID;

	switch ( spLight->aType )
	{
		case ELightType_Directional:
			index = Graphics_AllocateCoreSlot( EShaderCoreArray_LightWorld );
			break;

		case ELightType_Point:
			index = Graphics_AllocateCoreSlot( EShaderCoreArray_LightPoint );
			break;

		case ELightType_Cone:
			index = Graphics_AllocateCoreSlot( EShaderCoreArray_LightCone );
			break;

		// case ELightType_Capsule:
		// 	index = Graphics_AllocateCoreSlot( EShaderCoreArray_LightCapsule );
		// 	break;
	}

	if ( index == CH_SHADER_CORE_SLOT_INVALID )
		return index;

	// if ( spLight->aType == ELightType_Cone || spLight->aType == ELightType_Directional )
	if ( spLight->aType == ELightType_Cone )
	{
		Graphics_AddShadowMap( spLight );
	}

	return index;
}


void Graphics_FreeLightSlot( Light_t* spLight )
{
	auto it = gLightSlots.find( spLight );
	if ( it == gLightSlots.end() )
	{
		Log_Error( gLC_ClientGraphics, "Light not found for deletion!\n" );
		return;
	}

	u32 lightSlot = it->second;
	gLightSlots.erase( spLight );

	switch ( spLight->aType )
	{
		case ELightType_Directional:
			Graphics_FreeCoreSlot( EShaderCoreArray_LightWorld, lightSlot );
			break;

		case ELightType_Point:
			Graphics_FreeCoreSlot( EShaderCoreArray_LightPoint, lightSlot );
			break;

		case ELightType_Cone:
			Graphics_FreeCoreSlot( EShaderCoreArray_LightCone, lightSlot );
			break;

		// case ELightType_Capsule:
		// 	Graphics_FreeCoreSlot( EShaderCoreArray_LightCapsule, lightSlot );
		// 	break;
	}

	if ( spLight->aType == ELightType_Cone )
	{
		Graphics_DestroyShadowMap( spLight );
	}

	vec_remove( gLights, spLight );
	delete spLight;

	Log_Dev( gLC_ClientGraphics, 1, "Deleted Light\n" );
}


#if 0
Handle Graphics_AddLightBuffer( const char* spBufferName, size_t sBufferSize, Light_t* spLight )
{
#if 0
	Handle buffer = render->CreateBuffer( spBufferName, sBufferSize, EBufferFlags_Uniform, EBufferMemory_Host );

	if ( buffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Light Uniform Buffer\n" );
		return InvalidHandle;
	}

	gLightBuffers[ spLight ] = buffer;

	// update the descriptor sets
	WriteDescSet_t update{};
	update.aType = EDescriptorType_UniformBuffer;

	for ( size_t i = 0; i < gUniformLights[ spLight->aType ].aSets.size(); i++ )
		update.aDescSets.push_back( gUniformLights[ spLight->aType ].aSets[ i ] );

	for ( const auto& [ light, bufferHandle ] : gLightBuffers )
	{
		if ( light->aType == spLight->aType )
			update.aBuffers.push_back( bufferHandle );
	}

	// Update all light indexes for the shaders
	render->UpdateDescSet( update );

	// if ( spLight->aType == ELightType_Cone || spLight->aType == ELightType_Directional )
	if ( spLight->aType == ELightType_Cone )
	{
		Graphics_AddShadowMap( spLight );
	}

	return buffer;
#endif
	return CH_INVALID_HANDLE;
}


void Graphics_DestroyLightBuffer( Light_t* spLight )
{
#if 0
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
	WriteDescSet_t update{};
	update.aType = EDescriptorType_UniformBuffer;

	if ( spLight->aType < 0 || spLight->aType > ELightType_Count )
	{
		Log_ErrorF( gLC_ClientGraphics, "Unknown Light Buffer: %d\n", spLight->aType );
		return;
	}

	ShaderBufferArray_t* buffer = &gUniformLights[ spLight->aType ];
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
		case ELightType_Capsule:
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

	render->UpdateDescSet( update );

	vec_remove( gLights, spLight );
	delete spLight;

	Log_Dev( gLC_ClientGraphics, 1, "Deleted Light\n" );
#endif
}
#endif


void Graphics_UpdateLightBuffer( Light_t* spLight )
{
	PROF_SCOPE();

	u32  lightSlot = CH_SHADER_CORE_SLOT_INVALID;

	auto it        = gLightSlots.find( spLight );

	if ( it == gLightSlots.end() )
	{
		lightSlot = Graphics_AllocateLightSlot( spLight );

		if ( lightSlot == CH_SHADER_CORE_SLOT_INVALID )
		{
			Log_Warn( gLC_ClientGraphics, "Failed to Allocate Slot for Light!\n" );
			return;
		}

		gLightSlots[ spLight ] = lightSlot;
	}
	else
	{
		lightSlot = it->second;
	}

	gGraphicsData.aCoreDataStaging.aDirty = true;

#if 0
	switch ( spLight->aType )
	{
		default:
		{
			Log_Error( gLC_ClientGraphics, "Unknown Light Type!\n" );
			return;
		}
		case ELightType_Directional:
		{
			UBO_LightDirectional_t& light = gGraphicsData.aCoreData.aLightWorld[ lightSlot ];
			light.aColor.x                = spLight->aColor.x;
			light.aColor.y                = spLight->aColor.y;
			light.aColor.z                = spLight->aColor.z;
			light.aColor.w                = spLight->aEnabled ? spLight->aColor.w : 0.f;

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

			Handle shadowBuffer             = gViewportBuffers[ shadowMap.aViewInfoIndex ];
			render->MemWriteBuffer( shadowBuffer, sizeof( UBO_Viewport_t ), &shadowMap.aViewInfo );

			// get shadow map view info
			light.aProjView = view.aProjViewMat;
			light.aShadow   = render->GetTextureIndex( shadowMap.aTexture );
#else
			// get shadow map view info
			light.aProjView = glm::identity< glm::mat4 >();
			light.aShadow   = -1;
#endif

			break;
		}
		case ELightType_Point:
		{
			UBO_LightPoint_t& light = gGraphicsData.aCoreData.aLightPoint[ lightSlot ];
			light.aColor.x          = spLight->aColor.x;
			light.aColor.y          = spLight->aColor.y;
			light.aColor.z          = spLight->aColor.z;
			light.aColor.w          = spLight->aEnabled ? spLight->aColor.w : 0.f;

			light.aPos              = spLight->aPos;
			light.aRadius           = spLight->aRadius;

			break;
		}
		case ELightType_Cone:
		{
			UBO_LightCone_t& light = gGraphicsData.aCoreData.aLightCone[ lightSlot ];
			light.aColor.x         = spLight->aColor.x;
			light.aColor.y         = spLight->aColor.y;
			light.aColor.z         = spLight->aColor.z;
			light.aColor.w         = spLight->aEnabled ? spLight->aColor.w : 0.f;

			light.aPos             = spLight->aPos;
			light.aFov.x           = glm::radians( spLight->aInnerFov );
			light.aFov.y           = glm::radians( spLight->aOuterFov );

			glm::mat4 matrix;
			Util_ToMatrix( matrix, spLight->aPos, spLight->aAng );
			Util_GetMatrixDirectionNoScale( matrix, nullptr, nullptr, &light.aDir );
#if 0
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

			gViewport[ shadowMap.aViewInfoIndex ].aProjection = view.aProjMat;
			gViewport[ shadowMap.aViewInfoIndex ].aView       = view.aViewMat;
			gViewport[ shadowMap.aViewInfoIndex ].aProjView   = view.aProjViewMat;
			gViewport[ shadowMap.aViewInfoIndex ].aNearZ      = view.aNearZ;
			gViewport[ shadowMap.aViewInfoIndex ].aFarZ       = view.aFarZ;
			gViewport[ shadowMap.aViewInfoIndex ].aSize       = shadowMap.aSize;

			Handle shadowBuffer                               = gViewportBuffers[ shadowMap.aViewInfoIndex ];
			render->BufferWrite( shadowBuffer, sizeof( UBO_Viewport_t ), &gViewport[ shadowMap.aViewInfoIndex ] );

			// get shadow map view info

			// TODO: MAYBE MOVE aProjView OUT OF THIS LIGHT BUFFER, AND ON THE GPU,
			// USE THE ProjViewMat FROM THE ACTUAL VIEW IN THE VIEWPORT BUFFER
			// see for more info: https://vkguide.dev/docs/chapter-4/descriptors_code_more/
			// is there also a performance cost to doing this though? since we have to read memory elsewhere not in the cache?
			light.aProjView                               = view.aProjViewMat;
			light.aShadow                                 = render->GetTextureIndex( shadowMap.aTexture );

			gViewport[ shadowMap.aViewInfoIndex ].aActive = spLight->aShadow && spLight->aEnabled;
#endif

			break;
		}
		// case ELightType_Capsule:
		// {
		// 	UBO_LightCapsule_t light;
		// 	light.aColor.x   = spLight->aColor.x;
		// 	light.aColor.y   = spLight->aColor.y;
		// 	light.aColor.z   = spLight->aColor.z;
		// 	light.aColor.w   = spLight->aEnabled ? spLight->aColor.w : 0.f;
		// 
		// 	light.aPos       = spLight->aPos;
		// 	light.aLength    = spLight->aLength;
		// 	light.aThickness = spLight->aRadius;
		// 
		// 	glm::mat4 matrix;
		// 	Util_ToMatrix( matrix, spLight->aPos, spLight->aAng );
		// 	Util_GetMatrixDirectionNoScale( matrix, nullptr, nullptr, &light.aDir );
		// 
		// 	// Util_GetDirectionVectors( spLight->aAng, nullptr, nullptr, &light.aDir );
		// 	// Util_GetDirectionVectors( spLight->aAng, &light.aDir );
		// 
		// 	render->BufferWrite( buffer, sizeof( light ), &light );
		// 	break;
		// }
	}
#endif
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

	light->aType                 = sType;
	gGraphicsData.aCoreDataStaging.aDirty = true;

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

	gGraphicsData.aCoreDataStaging.aDirty = true;

	gDestroyLights.push_back( spLight );

	Log_Dev( gLC_ClientGraphics, 1, "Queued Light For Deletion\n" );

	vec_remove_if( gDirtyLights, spLight );
}


void Graphics_PrepareLights()
{
	PROF_SCOPE();

	// Destroy lights if needed
	for ( Light_t* light : gDestroyLights )
		Graphics_FreeLightSlot( light );

	// Update UBO's for lights if needed
	for ( Light_t* light : gDirtyLights )
		Graphics_UpdateLightBuffer( light );

	gDestroyLights.clear();
	gDirtyLights.clear();
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


void Graphics_RenderShadowMap( Handle cmd, size_t sIndex, Light_t* spLight, const ShadowMap_t& srShadowMap )
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

	if ( gGraphicsData.aViewRenderLists.size() <= srShadowMap.aViewInfoIndex )
	{
		Log_Error( gLC_ClientGraphics, "Invalid Viewport Index for Shadow Map Rendering\n" );
		return;
	}

	ViewRenderList_t& viewList = gGraphicsData.aViewRenderLists[ srShadowMap.aViewInfoIndex ];

	for ( auto& [ shader, renderList ] : viewList.aRenderLists )
	{
		Graphics_DrawShaderRenderables( cmd, sIndex, shader, srShadowMap.aViewInfoIndex, renderList );
	}
}


void Graphics_DrawShadowMaps( Handle sCmd, size_t sIndex )
{
	PROF_SCOPE();

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

		render->BeginRenderPass( sCmd, renderPassBegin );
		Graphics_RenderShadowMap( sCmd, sIndex, light, shadowMap );
		render->EndRenderPass( sCmd );
	}
}

