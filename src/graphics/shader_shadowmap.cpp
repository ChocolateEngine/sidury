#include "util.h"
#include "render/irender.h"
#include "graphics_int.h"


struct ShadowMap_Push
{
	alignas( 16 ) glm::mat4 aModelMatrix{};  // model matrix
	int aViewInfo = 0;                       // view info index
	int aAlbedo   = 0;                       // albedo texture index
};


static std::unordered_map< SurfaceDraw_t*, ShadowMap_Push > gPushData;
static int                                                  gShadowViewInfoIndex = 0;


static void Shader_ShadowMap_GetPipelineLayoutCreate( PipelineLayoutCreate_t& srPipeline )
{
	srPipeline.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( ShadowMap_Push ) );
}


static void Shader_ShadowMap_GetGraphicsPipelineCreate( GraphicsPipelineCreate_t& srGraphics )
{
	srGraphics.aShaderModules.emplace_back( ShaderStage_Vertex, "shaders/shadow.vert.spv", "main" );
	srGraphics.aShaderModules.emplace_back( ShaderStage_Fragment, "shaders/shadow.frag.spv", "main" );
	srGraphics.aColorBlendAttachments.emplace_back( false );
	srGraphics.aPrimTopology    = EPrimTopology_Tri;
	srGraphics.aDynamicState    = EDynamicState_Viewport | EDynamicState_Scissor | EDynamicState_DepthBias;
	srGraphics.aCullMode        = ECullMode_None;
	srGraphics.aDepthBiasEnable = true;
}


// blech
void Shader_ShadowMap_SetViewInfo( int sViewInfo )
{
	gShadowViewInfoIndex = sViewInfo;
}


static void Shader_ShadowMap_ResetPushData()
{
	gPushData.clear();
}


static void Shader_ShadowMap_SetupPushData( u32 sRenderableIndex, u32 sViewportIndex, Renderable_t* spModelDraw, SurfaceDraw_t& srDrawInfo )
{
	PROF_SCOPE();

	ShadowMap_Push& push = gPushData[ &srDrawInfo ];
	push.aModelMatrix    = spModelDraw->aModelMatrix;
	push.aViewInfo       = gShadowViewInfoIndex;
	push.aAlbedo         = -1;

	Handle mat           = gGraphics.Model_GetMaterial( spModelDraw->aModel, srDrawInfo.aSurface );
	if ( mat == InvalidHandle )
		return;

	Handle texture = gGraphics.Mat_GetTexture( mat, "diffuse" );

	if ( texture == InvalidHandle )
		return;

	bool alphaTest = gGraphics.Mat_GetBool( mat, "alphaTest" );

#if 0
	GraphicsFmt format   = render->GetTextureFormat( texture );

	// Check the texture format to see if it has an alpha channel
	switch ( format )
	{
		default:
			break;

		case GraphicsFmt::BC1_RGBA_SRGB_BLOCK:
		case GraphicsFmt::BC3_SRGB_BLOCK:
		case GraphicsFmt::BC7_SRGB_BLOCK:
			alphaTest = true;
	}
#endif

	if ( alphaTest )
		push.aAlbedo = render->GetTextureIndex( texture );
}


static void Shader_ShadowMap_PushConstants( Handle cmd, Handle sLayout, SurfaceDraw_t& srDrawInfo )
{
	PROF_SCOPE();

	ShadowMap_Push& push = gPushData.at( &srDrawInfo );
	push.aViewInfo       = gShadowViewInfoIndex;
	render->CmdPushConstants( cmd, sLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( ShadowMap_Push ), &push );
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
	.aFlags           = EShaderFlags_PushConstant,
	.aDynamicState    = EDynamicState_Viewport | EDynamicState_Scissor,
	.aVertexFormat    = VertexFormat_Position | VertexFormat_TexCoord,
	.aRenderPass      = ERenderPass_Shadow,
	.apInit           = nullptr,
	.apDestroy        = nullptr,
	.apLayoutCreate   = Shader_ShadowMap_GetPipelineLayoutCreate,
	.apGraphicsCreate = Shader_ShadowMap_GetGraphicsPipelineCreate,
	.apShaderPush     = &gShaderPush_ShadowMap,
};


// CH_REGISTER_SHADER( gShaderCreate_ShadowMap );
