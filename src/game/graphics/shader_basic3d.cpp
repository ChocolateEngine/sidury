#include "util.h"
#include "render/irender.h"
#include "graphics.h"
#include "graphics_int.h"


// TODO: rename this file and shader to "standard" or "generic"?

static bool                gArgPCF                  = false;  // Args_Register( "Enable PCF Shadow Filtering", "-pcf" );

static Handle              gFallbackAO              = InvalidHandle;
static Handle              gFallbackEmissive        = InvalidHandle;

constexpr const char*      gpFallbackAOPath         = "materials/base/white.ktx";
constexpr const char*      gpFallbackEmissivePath   = "materials/base/black.ktx";


constexpr u32              CH_BASIC3D_MAX_MATERIALS = 2048;


static CreateDescBinding_t gBasic3D_Bindings[]      = {
		 { EDescriptorType_StorageBuffer, ShaderStage_Vertex | ShaderStage_Fragment, 0, CH_BASIC3D_MAX_MATERIALS },
};


struct Basic3D_Push
{
	u32 aRenderable = 0;
	u32 aMaterial   = 0;
	u32 aViewport   = 0;

	// Hack for GTX 1050 Ti until I figure this out properly
	// bool aPCF = false;

	// debugging
	u32 aDebugDraw  = 0;
};


struct Basic3D_Material
{
	int   albedo        = 0;
	int   ao            = 0;
	int   emissive      = 0;

	float aoPower       = 1.f;
	float emissivePower = 1.f;

	bool  aAlphaTest    = false;
};


// Material Handle, Buffer
static std::unordered_map< Handle, ChHandle_t >       gMaterialBuffers;
static std::unordered_map< Handle, u32 >              gMaterialBufferIndex;
static std::unordered_map< u32, Basic3D_Push >        gPushData;
static std::unordered_map< Handle, Basic3D_Material > gMaterialData;


static Handle                                         gStagingBuffer = InvalidHandle;


CONVAR( r_basic3d_dbg_mode, 0 );


EShaderFlags Shader_Basic3D_Flags()
{
	// return EShaderFlags_PushConstant | EShaderFlags_SlotPerShader | EShaderFlags_SlotPerObject | EShaderFlags_Lights;
	return EShaderFlags_PushConstant;
}


bool Shader_Basic3D_CreateMaterialBuffer( Handle sMat )
{
	// IDEA: since materials shouldn't be updated very often,
	// maybe have the buffer be on the gpu (EBufferMemory_Device)?

	Handle newBuffer = render->CreateBuffer( Mat_GetName( sMat ), sizeof( Basic3D_Material ), EBufferFlags_Storage | EBufferFlags_TransferDst, EBufferMemory_Device );

	if ( newBuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Material Uniform Buffer\n" );
		return false;
	}

	gMaterialBuffers[ sMat ] = newBuffer;

	// TODO: this should probably be moved to the shader system
	// update the descriptor sets
	WriteDescSet_t update{};

	update.aDescSetCount = gShaderDescriptorData.aPerShaderSets[ "basic_3d" ].aCount;
	update.apDescSets    = gShaderDescriptorData.aPerShaderSets[ "basic_3d" ].apSets;

	update.aBindingCount = CH_ARR_SIZE( gBasic3D_Bindings );
	update.apBindings    = ch_calloc_count< WriteDescSetBinding_t >( update.aBindingCount );

	size_t i             = 0;
	for ( size_t binding = 0; binding < update.aBindingCount; binding++ )
	{
		update.apBindings[ i ].aBinding = gBasic3D_Bindings[ binding ].aBinding;
		update.apBindings[ i ].aType    = gBasic3D_Bindings[ binding ].aType;
		update.apBindings[ i ].aCount   = gBasic3D_Bindings[ binding ].aCount;
		i++;
	}

	update.apBindings[ 0 ].apData = ch_calloc_count< ChHandle_t >( CH_BASIC3D_MAX_MATERIALS );
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
u32 Shader_Basic3D_UpdateMaterialData( ChHandle_t sMat )
{
	PROF_SCOPE();

	if ( sMat == 0 )
		return 0;

	Basic3D_Material* mat = nullptr;

	auto it = gMaterialData.find( sMat );

	if ( it != gMaterialData.end() )
	{
		mat = &it->second;
	}
	else
	{
		// New Material Using this shader
		if ( !Shader_Basic3D_CreateMaterialBuffer( sMat ) )
			return 0;

		// create new material data
		mat = &gMaterialData[ sMat ];
	}

	mat->albedo        = Mat_GetTextureIndex( sMat, "diffuse" );
	mat->ao            = Mat_GetTextureIndex( sMat, "ao", gFallbackAO );
	mat->emissive      = Mat_GetTextureIndex( sMat, "emissive", gFallbackEmissive );

	mat->aoPower       = Mat_GetFloat( sMat, "aoPower", 0.f );
	mat->emissivePower = Mat_GetFloat( sMat, "emissivePower", 0.f );

	mat->aAlphaTest    = Mat_GetBool( sMat, "alphaTest" );

	if ( !gStagingBuffer )
		gStagingBuffer = render->CreateBuffer( "Staging Buffer", sizeof( Basic3D_Material ), EBufferFlags_Uniform | EBufferFlags_TransferSrc, EBufferMemory_Host );

	if ( gStagingBuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Staging Material Uniform Buffer\n" );
		return 0;
	}

	render->BufferWrite( gStagingBuffer, sizeof( Basic3D_Material ), mat );

	// write new material data to the buffer
	ChHandle_t& buffer = gMaterialBuffers[ sMat ];

	BufferRegionCopy_t copy;
	copy.aSrcOffset = 0;
	copy.aDstOffset = 0;
	copy.aSize      = sizeof( Basic3D_Material );

	// render->MemWriteBuffer( buffer, sizeof( Basic3D_Material ), mat );
	render->BufferCopy( gStagingBuffer, buffer, &copy, 1 );

	return gMaterialBufferIndex[ sMat ];
}


static bool Shader_Basic3D_Init()
{
	TextureCreateData_t createData{};
	createData.aFilter = EImageFilter_Nearest;
	createData.aUsage  = EImageUsage_Sampled;

	// create fallback textures
	Graphics_LoadTexture( gFallbackAO, gpFallbackAOPath, createData );
	Graphics_LoadTexture( gFallbackEmissive, gpFallbackEmissivePath, createData );

	return true;
}


static void Shader_Basic3D_Destroy()
{
	Graphics_FreeTexture( gFallbackAO );
	Graphics_FreeTexture( gFallbackEmissive );
	
	if ( gStagingBuffer )
		render->DestroyBuffer( gStagingBuffer );

	for ( auto& [ buffer, material ] : gMaterialData )
		render->DestroyBuffer( buffer );

	gStagingBuffer = InvalidHandle;
	gMaterialData.clear();

	Graphics_SetAllMaterialsDirty();
}


static void Shader_Basic3D_GetPipelineLayoutCreate( PipelineLayoutCreate_t& srPipeline )
{
	// NOTE: maybe create the descriptor set layout for this shader here, then add it? idk

	srPipeline.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Basic3D_Push ) );
}


static void Shader_Basic3D_GetGraphicsPipelineCreate( GraphicsPipelineCreate_t& srGraphics )
{
	srGraphics.aShaderModules.emplace_back( ShaderStage_Vertex, "shaders/basic3d.vert.spv", "main" );
	srGraphics.aShaderModules.emplace_back( ShaderStage_Fragment, "shaders/basic3d.frag.spv", "main" );

	srGraphics.aColorBlendAttachments.emplace_back( false ); 

	srGraphics.aPrimTopology   = EPrimTopology_Tri;
	srGraphics.aDynamicState   = EDynamicState_Viewport | EDynamicState_Scissor;
	srGraphics.aCullMode       = ECullMode_Back;
}


static u32 Shader_Basic3D_GetMaterialIndex( u32 sRenderableIndex, Renderable_t* spModelDraw, SurfaceDraw_t& srDrawInfo )
{
	Handle mat = Model_GetMaterial( spModelDraw->aModel, srDrawInfo.aSurface );

	if ( !mat )
		return 0;

	auto it = gMaterialBufferIndex.find( mat );
	if ( it == gMaterialBufferIndex.end() )
	{
		return Shader_Basic3D_UpdateMaterialData( mat );
	}

	return it->second;
}


// TODO: Move this push constant management into the shader system
// we should only need the SetupPushData function
static void Shader_Basic3D_ResetPushData()
{
	gPushData.clear();
}


static void Shader_Basic3D_SetupPushData( u32 sSurfaceIndex, u32 sViewportIndex, Renderable_t* spModelDraw, SurfaceDraw_t& srDrawInfo )
{
	PROF_SCOPE();

	Basic3D_Push& push = gPushData[ srDrawInfo.aShaderSlot ];
	// push.aModelMatrix  = spModelDraw->aModelMatrix;
	push.aRenderable   = CH_GET_HANDLE_INDEX( srDrawInfo.aRenderable );
	push.aMaterial     = Shader_Basic3D_GetMaterialIndex( sSurfaceIndex, spModelDraw, srDrawInfo );
	push.aViewport     = sViewportIndex;

	push.aDebugDraw    = r_basic3d_dbg_mode;
	// push.aPCF          = gArgPCF;
}


static void Shader_Basic3D_PushConstants( Handle cmd, Handle sLayout, SurfaceDraw_t& srDrawInfo )
{
	PROF_SCOPE();

	Basic3D_Push& push = gPushData.at( srDrawInfo.aShaderSlot );
	render->CmdPushConstants( cmd, sLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Basic3D_Push ), &push );
}


static IShaderPush gShaderPush_Basic3D = {
	.apReset = Shader_Basic3D_ResetPushData,
	.apSetup = Shader_Basic3D_SetupPushData,
	.apPush  = Shader_Basic3D_PushConstants,
};


ShaderCreate_t gShaderCreate_Basic3D = {
	.apName           = "basic_3d",
	.aStages          = ShaderStage_Vertex | ShaderStage_Fragment,
	.aBindPoint       = EPipelineBindPoint_Graphics,
	.aFlags           = Shader_Basic3D_Flags(),
	.aDynamicState    = EDynamicState_Viewport | EDynamicState_Scissor,
	.aVertexFormat    = VertexFormat_Position | VertexFormat_Normal | VertexFormat_TexCoord,

	.apInit           = Shader_Basic3D_Init,
	.apDestroy        = Shader_Basic3D_Destroy,
	.apLayoutCreate   = Shader_Basic3D_GetPipelineLayoutCreate,
	.apGraphicsCreate = Shader_Basic3D_GetGraphicsPipelineCreate,
	.apShaderPush     = &gShaderPush_Basic3D,
	.apMaterialData   = Shader_Basic3D_GetMaterialIndex,

	.apBindings       = gBasic3D_Bindings,
	.aBindingCount    = CH_ARR_SIZE( gBasic3D_Bindings ),
};


CH_REGISTER_SHADER( gShaderCreate_Basic3D );

