#include "util.h"
#include "render/irender.h"
#include "graphics.h"


struct ShadowMap_Push
{
	alignas( 16 ) glm::mat4 aModelMatrix{};  // model matrix
	int aViewInfo = 0;                       // view info index
};


extern IRender*                                                  render;

static std::unordered_map< ModelSurfaceDraw_t*, ShadowMap_Push > gPushData;
static int                                                       gViewInfoIndex = 0;


static void Shader_ShadowMap_GetPipelineLayoutCreate( PipelineLayoutCreate_t& srPipeline )
{
	Graphics_AddPipelineLayouts( srPipeline, EShaderFlags_ViewInfo | EShaderFlags_PushConstant );
	srPipeline.aPushConstants.emplace_back( ShaderStage_Vertex, 0, sizeof( ShadowMap_Push ) );
}


static void Shader_ShadowMap_GetGraphicsPipelineCreate( GraphicsPipelineCreate_t& srGraphics )
{
	srGraphics.aShaderModules.emplace_back( ShaderStage_Vertex, "shaders/shadow.vert.spv", "main" );
	srGraphics.aColorBlendAttachments.emplace_back( false );
	srGraphics.aPrimTopology    = EPrimTopology_Tri;
	srGraphics.aDynamicState    = EDynamicState_Viewport | EDynamicState_Scissor | EDynamicState_DepthBias;
	srGraphics.aCullMode        = ECullMode_None;
	srGraphics.aDepthBiasEnable = true;
}


// blech
void Shader_ShadowMap_SetViewInfo( int sViewInfo )
{
	gViewInfoIndex = sViewInfo;
}


static void Shader_ShadowMap_ResetPushData()
{
	gPushData.clear();
}


static void Shader_ShadowMap_SetupPushData( ModelSurfaceDraw_t& srDrawInfo )
{
	ShadowMap_Push& push = gPushData[ &srDrawInfo ];
	push.aModelMatrix    = srDrawInfo.apDraw->aModelMatrix;
	push.aViewInfo       = gViewInfoIndex;
}


static void Shader_ShadowMap_PushConstants( Handle cmd, Handle sLayout, ModelSurfaceDraw_t& srDrawInfo )
{
	ShadowMap_Push& push = gPushData.at( &srDrawInfo );
	push.aViewInfo       = gViewInfoIndex;
	render->CmdPushConstants( cmd, sLayout, ShaderStage_Vertex, 0, sizeof( ShadowMap_Push ), &push );
}


static IShaderPush gShaderPush_ShadowMap = {
	.apReset = Shader_ShadowMap_ResetPushData,
	.apSetup = Shader_ShadowMap_SetupPushData,
	.apPush  = Shader_ShadowMap_PushConstants,
};


ShaderCreate_t gShaderCreate_ShadowMap = {
	.apName           = "__shadow_map",
	.aStages          = ShaderStage_Vertex | ShaderStage_Fragment,
	.aBindPoint       = EPipelineBindPoint_Graphics,
	.aFlags           = EShaderFlags_ViewInfo | EShaderFlags_PushConstant,
	.aVertexFormat    = VertexFormat_Position,
	.apInit           = nullptr,
	.apDestroy        = nullptr,
	.apLayoutCreate   = Shader_ShadowMap_GetPipelineLayoutCreate,
	.apGraphicsCreate = Shader_ShadowMap_GetGraphicsPipelineCreate,
	.apShaderPush     = &gShaderPush_ShadowMap,
};

