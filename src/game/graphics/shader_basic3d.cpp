#include "util.h"
#include "render/irender.h"
#include "graphics.h"


extern IRender*                             render;

static Handle                               gFallbackAO            = InvalidHandle;
static Handle                               gFallbackEmissive      = InvalidHandle;

constexpr const char*                       gpFallbackAOPath       = "materials/base/white.ktx";
constexpr const char*                       gpFallbackEmissivePath = "materials/base/black.ktx";

// descriptor set layouts
extern UniformBufferArray_t                 gUniformMaterialBasic3D;

constexpr EShaderFlags                      gShaderFlags =
  EShaderFlags_Sampler |
  EShaderFlags_ViewInfo |
  EShaderFlags_PushConstant |
  EShaderFlags_MaterialUniform |
  EShaderFlags_Lights;


struct Basic3D_Push
{
	alignas( 16 ) glm::mat4 aModelMatrix{};  // model matrix
	alignas( 16 ) int aMaterial = 0;         // material index
	int aProjView = 0;         // projection * view index

	// debugging
	int aDebugDraw;
};


struct Basic3D_Material
{
	int   diffuse       = 0;
	int   ao            = 0;
	int   emissive      = 0;

	float aoPower       = 1.f;
	float emissivePower = 1.f;
};


struct Basic3D_MaterialBuf_t
{
	Handle aBuffer;
	int    aIndex;
};

// Material Handle, Buffer
static std::unordered_map< Handle, Basic3D_MaterialBuf_t >     gMaterialBuffers;
static std::unordered_map< SurfaceDraw_t*, Basic3D_Push > gPushData;
static std::unordered_map< Handle, Basic3D_Material >          gMaterialData;


static Handle                                                  gStagingBuffer = InvalidHandle;


CONVAR( r_basic3d_dbg_mode, 0 );


EShaderFlags Shader_Basic3D_Flags()
{
	return EShaderFlags_Sampler | EShaderFlags_ViewInfo | EShaderFlags_PushConstant | EShaderFlags_MaterialUniform | EShaderFlags_Lights;
}


bool Shader_Basic3D_CreateMaterialBuffer( Handle sMat )
{
	// IDEA: since materials shouldn't be updated very often,
	// maybe have the buffer be on the gpu (EBufferMemory_Device)?

	Handle newBuffer = render->CreateBuffer( Mat_GetName( sMat ), sizeof( Basic3D_Material ), EBufferFlags_Uniform | EBufferFlags_TransferDst, EBufferMemory_Device );

	if ( newBuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Material Uniform Buffer\n" );
		return false;
	}

	gMaterialBuffers[ sMat ] = { newBuffer, 0 };

	// update the material descriptor sets
	UpdateVariableDescSet_t update{};

	// what
	update.aDescSets.push_back( gUniformMaterialBasic3D.aSets[ 0 ] );
	update.aDescSets.push_back( gUniformMaterialBasic3D.aSets[ 1 ] );

	update.aType = EDescriptorType_UniformBuffer;

	int i = 0;
	for ( auto& [ mat, buffer ] : gMaterialBuffers )
	{
		buffer.aIndex = i++;
		update.aBuffers.push_back( buffer.aBuffer );
	}

	render->UpdateVariableDescSet( update );

	return true;
}


// TODO: this doesn't handle shaders being changed on materials, or materials being freed
void Shader_Basic3D_UpdateMaterialData( Handle sMat )
{
	PROF_SCOPE();

	if ( sMat == 0 )
		return;

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
			return;

		// create new material data
		mat = &gMaterialData[ sMat ];
	}

	mat->diffuse       = Mat_GetTextureIndex( sMat, "diffuse" );
	mat->ao            = Mat_GetTextureIndex( sMat, "ao", gFallbackAO );
	mat->emissive      = Mat_GetTextureIndex( sMat, "emissive", gFallbackEmissive );

	mat->aoPower       = Mat_GetFloat( sMat, "aoPower", 0.f );
	mat->emissivePower   = Mat_GetFloat( sMat, "emissivePower", 0.f );

	if ( !gStagingBuffer )
		gStagingBuffer = render->CreateBuffer( "Staging Buffer", sizeof( Basic3D_Material ), EBufferFlags_Uniform | EBufferFlags_TransferSrc, EBufferMemory_Host );

	if ( gStagingBuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Staging Material Uniform Buffer\n" );
		return;
	}

	render->BufferWrite( gStagingBuffer, sizeof( Basic3D_Material ), mat );

	// write new material data to the buffer
	Handle buffer = gMaterialBuffers[ sMat ].aBuffer;
	// render->MemWriteBuffer( buffer, sizeof( Basic3D_Material ), mat );
	render->BufferCopy( gStagingBuffer, buffer, sizeof( Basic3D_Material ) );
}


static bool Shader_Basic3D_Init()
{
	TextureCreateData_t createData{};
	createData.aFilter = EImageFilter_Nearest;
	createData.aUsage  = EImageUsage_Sampled;

	// create fallback textures
	render->LoadTexture( gFallbackAO, gpFallbackAOPath, createData );
	render->LoadTexture( gFallbackEmissive, gpFallbackEmissivePath, createData );

	return true;
}


static void Shader_Basic3D_Destroy()
{
	render->FreeTexture( gFallbackAO );
	render->FreeTexture( gFallbackEmissive );

	Graphics_SetAllMaterialsDirty();
}


static void Shader_Basic3D_GetPipelineLayoutCreate( PipelineLayoutCreate_t& srPipeline )
{
	Graphics_AddPipelineLayouts( srPipeline, Shader_Basic3D_Flags() );
	srPipeline.aLayouts.push_back( gUniformMaterialBasic3D.aLayout );
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


static void Shader_Basic3D_ResetPushData()
{
	gPushData.clear();
}


static void Shader_Basic3D_SetupPushData( Renderable_t* spModelDraw, SurfaceDraw_t& srDrawInfo )
{
	PROF_SCOPE();

	Basic3D_Push& push = gPushData[ &srDrawInfo ];
	push.aModelMatrix  = spModelDraw->aModelMatrix;

	Handle mat         = Model_GetMaterial( spModelDraw->aModel, srDrawInfo.aSurface );
	// push.aMaterial     = std::distance( std::begin( gMaterialBuffers ), gMaterialBuffers.find( mat ) );

	if ( !mat )
		return;

	auto it = gMaterialBuffers.find( mat );
	if ( it == gMaterialBuffers.end() )
	{
		push.aMaterial = 0;
		Shader_Basic3D_UpdateMaterialData( mat );
	}
	else
	{
		push.aMaterial = it->second.aIndex;
	}

	push.aProjView     = 0;
	push.aDebugDraw    = r_basic3d_dbg_mode;
}


static void Shader_Basic3D_PushConstants( Handle cmd, Handle sLayout, SurfaceDraw_t& srDrawInfo )
{
	PROF_SCOPE();

	Basic3D_Push& push = gPushData.at( &srDrawInfo );
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
	.aFlags           = EShaderFlags_Sampler | EShaderFlags_ViewInfo | EShaderFlags_PushConstant | EShaderFlags_MaterialUniform | EShaderFlags_Lights,
	.aDynamicState    = EDynamicState_Viewport | EDynamicState_Scissor,
	.aVertexFormat    = VertexFormat_Position | VertexFormat_Normal | VertexFormat_TexCoord,
	.apInit           = Shader_Basic3D_Init,
	.apDestroy        = Shader_Basic3D_Destroy,
	.apLayoutCreate   = Shader_Basic3D_GetPipelineLayoutCreate,
	.apGraphicsCreate = Shader_Basic3D_GetGraphicsPipelineCreate,
	.apShaderPush     = &gShaderPush_Basic3D,
};

