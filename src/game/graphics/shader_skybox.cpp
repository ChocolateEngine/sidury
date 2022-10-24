#include "util.h"
#include "render/irender.h"
#include "graphics.h"


extern IRender*                             render;

constexpr VertexFormat                      gVertexFormat          = VertexFormat_Position;

static Handle                               gPipeline              = InvalidHandle;
static Handle                               gPipelineLayout        = InvalidHandle;

constexpr const char*                       gpVertShader           = "shaders/skybox.vert.spv";
constexpr const char*                       gpFragShader           = "shaders/skybox.frag.spv";

// descriptor set layouts
extern Handle                               gLayoutSampler;
extern Handle                               gLayoutViewProj;

extern Handle                               gLayoutSamplerSets[ 2 ];
extern Handle                               gLayoutViewProjSets[ 2 ];

// Material Handle, Buffer
static std::unordered_map< Handle, Handle > gMaterialBuffers;
// static std::vector< MaterialBuffer_t > gMaterialBuffers;
// static std::unordered_map< Handle, u32 >                   gMaterialBufferIndex;
static std::vector< Handle >                gMaterialBufferIndex;

// constexpr const char*                gpVertShader = "shaders/unlit.vert.spv";
// constexpr const char*                gpFragShader = "shaders/unlit.frag.spv";


struct Skybox_Push
{
	alignas( 16 ) glm::mat4 aModelMatrix{};  // model matrix
	alignas( 16 ) int aSky = 0;              // sky texture index
};


static std::unordered_map< ModelSurfaceDraw_t*, Skybox_Push > gSkyboxPushData;
// static std::unordered_map< Handle, Skybox_Push >              gMaterialData;


Handle Shader_Skybox_Create( Handle sRenderPass, bool sRecreate )
{
	PipelineLayoutCreate_t   pipelineCreateInfo{};
	GraphicsPipelineCreate_t pipelineInfo{};

	pipelineCreateInfo.aLayouts.push_back( gLayoutSampler );
	pipelineCreateInfo.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Skybox_Push ) );

	// --------------------------------------------------------------

	pipelineInfo.apName = "skybox";
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Vertex, gpVertShader, "main" );
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Fragment, gpFragShader, "main" );

	Graphics_GetVertexBindingDesc( gVertexFormat, pipelineInfo.aVertexBindings );
	Graphics_GetVertexAttributeDesc( gVertexFormat, pipelineInfo.aVertexAttributes );

	pipelineInfo.aPrimTopology   = EPrimTopology_Tri;
	pipelineInfo.aDynamicState   = EDynamicState_Viewport | EDynamicState_Scissor;
	pipelineInfo.aCullMode       = ECullMode_Back;
	pipelineInfo.aPipelineLayout = gPipelineLayout;
	pipelineInfo.aRenderPass     = sRenderPass;
	// TODO: expose the rest later

	// --------------------------------------------------------------

	if ( sRecreate )
	{
		render->RecreatePipelineLayout( gPipelineLayout, pipelineCreateInfo );
		render->RecreateGraphicsPipeline( gPipeline, pipelineInfo );
	}
	else
	{
		gPipelineLayout              = render->CreatePipelineLayout( pipelineCreateInfo );
		pipelineInfo.aPipelineLayout = gPipelineLayout;
		gPipeline                    = render->CreateGraphicsPipeline( pipelineInfo );
	}

	return gPipeline;
}


void Shader_Skybox_Destroy()
{
	if ( gPipelineLayout )
		render->DestroyPipelineLayout( gPipelineLayout );

	if ( gPipeline )
		render->DestroyPipeline( gPipeline );

	gPipelineLayout = InvalidHandle;
	gPipeline       = InvalidHandle;
}


void Shader_Skybox_ResetPushData()
{
	gSkyboxPushData.clear();
}


void Shader_Skybox_SetupPushData( ModelSurfaceDraw_t& srDrawInfo )
{
	Skybox_Push& push = gSkyboxPushData[ &srDrawInfo ];
	push.aModelMatrix  = srDrawInfo.apDraw->aModelMatrix;

	Handle mat         = Model_GetMaterial( srDrawInfo.apDraw->aModel, srDrawInfo.aSurface );
	push.aSky          = Mat_GetTextureIndex( mat, "sky" );
}


void Shader_Skybox_Bind( Handle cmd, size_t sCmdIndex )
{
	Handle descSets[] = { gLayoutSamplerSets[ sCmdIndex ] };
	render->CmdBindDescriptorSets( cmd, sCmdIndex, EPipelineBindPoint_Graphics, gPipelineLayout, descSets, 1 );
}


void Shader_Skybox_PushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo )
{
	Skybox_Push& push = gSkyboxPushData.at( &srDrawInfo );

	render->CmdPushConstants( cmd, gPipelineLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Skybox_Push ), &push );
}


void Shader_Skybox_UpdateMaterialData( Handle sMat )
{
}


// blech, awful
VertexFormat Shader_Skybox_GetVertexFormat()
{
	return gVertexFormat;
}


Handle Shader_Skybox_GetPipelineLayout()
{
	return gPipelineLayout;
}

