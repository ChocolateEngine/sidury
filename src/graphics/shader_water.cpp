#include "graphics_int.h"
#include "render/irender.h"
#include "util.h"


// TODO: rename this file and shader to "standard" or "generic"?

static bool                gArgPCF                  = false;  // Args_Register( "Enable PCF Shadow Filtering", "-pcf" );

static Handle              gFallbackNormal          = InvalidHandle;
constexpr const char*      gpFallbackNormalPath     = "materials/base/black.ktx";

constexpr u32              CH_WATER_MAX_MATERIALS = 2048;

static glm::vec3           gWaterColor              = { 90.f / 255.f, 188.f / 255.f, 216.f / 255.f };


static CreateDescBinding_t gWater_Bindings[]      = {
    { EDescriptorType_StorageBuffer, ShaderStage_Vertex | ShaderStage_Fragment, 0, CH_WATER_MAX_MATERIALS },
};


struct Water_Push_t
{
	glm::vec3 aColor;

	u32       aRenderable = 0;
	// u32       aMaterial   = 0;
	u32       aViewport   = 0;


	// Hack for GTX 1050 Ti until I figure this out properly
	// bool aPCF = false;
};


struct Water_Material_t
{
	u32 aNormalMap = 0;
};


// Material Handle, Buffer
static std::unordered_map< Handle, ChHandle_t >       gMaterialBuffers;
static std::unordered_map< Handle, u32 >              gMaterialBufferIndex;
static std::unordered_map< u32, Water_Push_t >        gPushData;
static std::unordered_map< Handle, Water_Material_t > gWaterMaterialData;


static Handle                                         gStagingBuffer = InvalidHandle;


EShaderFlags Shader_Water_Flags()
{
	// return EShaderFlags_PushConstant | EShaderFlags_SlotPerShader | EShaderFlags_SlotPerObject | EShaderFlags_Lights;
	return EShaderFlags_PushConstant;
}


bool Shader_Water_CreateMaterialBuffer( Handle sMat )
{
	// IDEA: since materials shouldn't be updated very often,
	// maybe have the buffer be on the gpu (EBufferMemory_Device)?

	Handle newBuffer = render->CreateBuffer( gGraphics.Mat_GetName( sMat ), sizeof( Water_Material_t ), EBufferFlags_Storage | EBufferFlags_TransferDst, EBufferMemory_Device );

	if ( newBuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Material Uniform Buffer\n" );
		return false;
	}

	gMaterialBuffers[ sMat ] = newBuffer;

	// TODO: this should probably be moved to the shader system
	// update the descriptor sets
	WriteDescSet_t update{};

	update.aDescSetCount = gShaderDescriptorData.aPerShaderSets[ "water" ].aCount;
	update.apDescSets    = gShaderDescriptorData.aPerShaderSets[ "water" ].apSets;

	update.aBindingCount = CH_ARR_SIZE( gWater_Bindings );
	update.apBindings    = ch_calloc_count< WriteDescSetBinding_t >( update.aBindingCount );

	size_t i             = 0;
	for ( size_t binding = 0; binding < update.aBindingCount; binding++ )
	{
		update.apBindings[ i ].aBinding = gWater_Bindings[ binding ].aBinding;
		update.apBindings[ i ].aType    = gWater_Bindings[ binding ].aType;
		update.apBindings[ i ].aCount   = gWater_Bindings[ binding ].aCount;
		i++;
	}

	update.apBindings[ 0 ].apData = ch_calloc_count< ChHandle_t >( CH_WATER_MAX_MATERIALS );
	update.apBindings[ 0 ].aCount = gMaterialBuffers.size();

	// GOD
	i                             = 0;
	for ( const auto& [ mat, buffer ] : gMaterialBuffers )
	{
		gMaterialBufferIndex[ mat ]          = i;
		update.apBindings[ 0 ].apData[ i++ ] = buffer;
	}

	// update.aImages = gViewportBuffers;
	render->UpdateDescSets( &update, 1 );

	free( update.apBindings[ 0 ].apData );
	free( update.apBindings );

	return true;
}


// TODO: this doesn't handle shaders being changed on materials, or materials being freed
u32 Shader_Water_UpdateMaterialData( ChHandle_t sMat )
{
	PROF_SCOPE();

	if ( sMat == 0 )
		return 0;

	Water_Material_t* mat = nullptr;

	auto              it  = gWaterMaterialData.find( sMat );

	if ( it != gWaterMaterialData.end() )
	{
		mat = &it->second;
	}
	else
	{
		// New Material Using this shader
		if ( !Shader_Water_CreateMaterialBuffer( sMat ) )
			return 0;

		// create new material data
		mat = &gWaterMaterialData[ sMat ];
	}

	mat->aNormalMap    = gGraphics.Mat_GetTextureIndex( sMat, "normalMap", gFallbackNormal );

	if ( !gStagingBuffer )
		gStagingBuffer = render->CreateBuffer( "Staging Buffer", sizeof( Water_Material_t ), EBufferFlags_Uniform | EBufferFlags_TransferSrc, EBufferMemory_Host );

	if ( gStagingBuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Staging Material Uniform Buffer\n" );
		return 0;
	}

	render->BufferWrite( gStagingBuffer, sizeof( Water_Material_t ), mat );

	// write new material data to the buffer
	ChHandle_t&        buffer = gMaterialBuffers[ sMat ];

	BufferRegionCopy_t copy;
	copy.aSrcOffset = 0;
	copy.aDstOffset = 0;
	copy.aSize      = sizeof( Water_Material_t );

	// render->MemWriteBuffer( buffer, sizeof( Water_Material_t ), mat );
	render->BufferCopy( gStagingBuffer, buffer, &copy, 1 );

	return gMaterialBufferIndex[ sMat ];
}


static bool Shader_Water_Init()
{
	TextureCreateData_t createData{};
	createData.aFilter = EImageFilter_Nearest;
	createData.aUsage  = EImageUsage_Sampled;

	// create fallback textures
	gGraphics.LoadTexture( gFallbackNormal, gpFallbackNormalPath, createData );

	return true;
}


static void Shader_Water_Destroy()
{
	gGraphics.FreeTexture( gFallbackNormal );

	if ( gStagingBuffer )
		render->DestroyBuffer( gStagingBuffer );

	for ( auto& [ buffer, material ] : gWaterMaterialData )
		render->DestroyBuffer( buffer );

	gStagingBuffer = InvalidHandle;
	gWaterMaterialData.clear();

	gGraphics.SetAllMaterialsDirty();
}


static void Shader_Water_GetPipelineLayoutCreate( PipelineLayoutCreate_t& srPipeline )
{
	// NOTE: maybe create the descriptor set layout for this shader here, then add it? idk

	srPipeline.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Water_Push_t ) );
}


static void Shader_Water_GetGraphicsPipelineCreate( GraphicsPipelineCreate_t& srGraphics )
{
	srGraphics.aShaderModules.emplace_back( ShaderStage_Vertex, "shaders/water.vert.spv", "main" );
	srGraphics.aShaderModules.emplace_back( ShaderStage_Fragment, "shaders/water.frag.spv", "main" );

	srGraphics.aColorBlendAttachments.emplace_back( false );

	srGraphics.aPrimTopology = EPrimTopology_Tri;
	srGraphics.aDynamicState = EDynamicState_Viewport | EDynamicState_Scissor;
	srGraphics.aCullMode     = ECullMode_Back;
}


static u32 Shader_Water_GetMaterialIndex( u32 sRenderableIndex, Renderable_t* spModelDraw, SurfaceDraw_t& srDrawInfo )
{
	Handle mat = gGraphics.Model_GetMaterial( spModelDraw->aModel, srDrawInfo.aSurface );

	if ( !mat )
		return 0;

	auto it = gMaterialBufferIndex.find( mat );
	if ( it == gMaterialBufferIndex.end() )
	{
		return Shader_Water_UpdateMaterialData( mat );
	}

	return it->second;
}


// TODO: Move this push constant management into the shader system
// we should only need the SetupPushData function
static void Shader_Water_ResetPushData()
{
	gPushData.clear();
}


static void Shader_Water_SetupPushData( u32 sSurfaceIndex, u32 sViewportIndex, Renderable_t* spModelDraw, SurfaceDraw_t& srDrawInfo )
{
	PROF_SCOPE();

	Water_Push_t& push = gPushData[ srDrawInfo.aShaderSlot ];
	// push.aModelMatrix  = spModelDraw->aModelMatrix;
	push.aRenderable   = CH_GET_HANDLE_INDEX( srDrawInfo.aRenderable );
	// push.aMaterial     = Shader_Water_GetMaterialIndex( sSurfaceIndex, spModelDraw, srDrawInfo );
	push.aViewport     = sViewportIndex;
	push.aColor        = gWaterColor;
}


static void Shader_Water_PushConstants( Handle cmd, Handle sLayout, SurfaceDraw_t& srDrawInfo )
{
	PROF_SCOPE();

	Water_Push_t& push = gPushData.at( srDrawInfo.aShaderSlot );
	render->CmdPushConstants( cmd, sLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Water_Push_t ), &push );
}


static IShaderPush gShaderPush_Water = {
	.apReset = Shader_Water_ResetPushData,
	.apSetup = Shader_Water_SetupPushData,
	.apPush  = Shader_Water_PushConstants,
};


ShaderCreate_t gShaderCreate_Water = {
	.apName           = "water",
	.aStages          = ShaderStage_Vertex | ShaderStage_Fragment,
	.aBindPoint       = EPipelineBindPoint_Graphics,
	.aFlags           = Shader_Water_Flags(),
	.aDynamicState    = EDynamicState_Viewport | EDynamicState_Scissor,
	.aVertexFormat    = VertexFormat_Position | VertexFormat_Normal | VertexFormat_TexCoord,

	.apInit           = Shader_Water_Init,
	.apDestroy        = Shader_Water_Destroy,
	.apLayoutCreate   = Shader_Water_GetPipelineLayoutCreate,
	.apGraphicsCreate = Shader_Water_GetGraphicsPipelineCreate,
	.apShaderPush     = &gShaderPush_Water,
	.apMaterialData   = Shader_Water_GetMaterialIndex,

	.apBindings       = gWater_Bindings,
	.aBindingCount    = CH_ARR_SIZE( gWater_Bindings ),
};


CH_REGISTER_SHADER( gShaderCreate_Water );
