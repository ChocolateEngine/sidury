#include "util.h"
#include "render/irender.h"
#include "graphics.h"


extern IRender*                             render;

constexpr const char*                       gpVertColShader        = "shaders/debug_col.vert.spv";
constexpr const char*                       gpVertShader           = "shaders/debug.vert.spv";
constexpr const char*                       gpFragShader           = "shaders/debug.frag.spv";

// descriptor set layouts
extern UniformBufferArray_t                 gUniformViewInfo;


struct Debug_Push
{
	alignas( 16 ) glm::mat4 aModelMatrix;
	alignas( 16 ) glm::vec4 aColor;
};


static std::unordered_map< SurfaceDraw_t*, Debug_Push > gDebugPushData;


static void Shader_Debug_GetPipelineLayoutCreate( PipelineLayoutCreate_t& srPipeline )
{
	Graphics_AddPipelineLayouts( srPipeline, EShaderFlags_ViewInfo );
	srPipeline.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Debug_Push ) );
}


static void Shader_Debug_GetGraphicsPipelineCreate( GraphicsPipelineCreate_t& srGraphics )
{
	srGraphics.aShaderModules.emplace_back( ShaderStage_Vertex, gpVertColShader, "main" );
	srGraphics.aShaderModules.emplace_back( ShaderStage_Fragment, gpFragShader, "main" );

	srGraphics.aColorBlendAttachments.emplace_back( true );

	srGraphics.aPrimTopology = EPrimTopology_Tri;
	srGraphics.aDynamicState = EDynamicState_Viewport | EDynamicState_Scissor;
	srGraphics.aCullMode     = ECullMode_Back;
}


static void Shader_Debug_ResetPushData()
{
	gDebugPushData.clear();
}


static void Shader_Debug_SetupPushData( Renderable_t* spDrawData, SurfaceDraw_t& srDrawInfo )
{
	Debug_Push& push  = gDebugPushData[ &srDrawInfo ];
	push.aModelMatrix = spDrawData->aModelMatrix;
	Handle mat        = Model_GetMaterial( spDrawData->aModel, srDrawInfo.aSurface );
	push.aColor       = Mat_GetVec4( mat, "color" );
}


static void Shader_Debug_PushConstants( Handle cmd, Handle sLayout, SurfaceDraw_t& srDrawInfo )
{
	Debug_Push& push = gDebugPushData.at( &srDrawInfo );
	render->CmdPushConstants( cmd, sLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Debug_Push ), &push );
}


static IShaderPush gShaderPush_Debug = {
	.apReset = Shader_Debug_ResetPushData,
	.apSetup = Shader_Debug_SetupPushData,
	.apPush  = Shader_Debug_PushConstants,
};


ShaderCreate_t gShaderCreate_Debug = {
	.apName           = "debug",
	.aStages          = ShaderStage_Vertex | ShaderStage_Fragment,
	.aBindPoint       = EPipelineBindPoint_Graphics,
	.aFlags           = EShaderFlags_ViewInfo | EShaderFlags_PushConstant,
	.aDynamicState    = EDynamicState_Viewport | EDynamicState_Scissor,
	.aVertexFormat    = VertexFormat_Position,
	.apInit           = nullptr,
	.apDestroy        = nullptr,
	.apLayoutCreate   = Shader_Debug_GetPipelineLayoutCreate,
	.apGraphicsCreate = Shader_Debug_GetGraphicsPipelineCreate,
	.apShaderPush     = &gShaderPush_Debug,
};


static void Shader_DebugLine_GetPipelineLayoutCreate( PipelineLayoutCreate_t& srPipeline )
{
	Graphics_AddPipelineLayouts( srPipeline, EShaderFlags_ViewInfo );
}


static void Shader_DebugLine_GetGraphicsPipelineCreate( GraphicsPipelineCreate_t& srGraphics )
{
	srGraphics.aShaderModules.emplace_back( ShaderStage_Vertex, gpVertShader, "main" );
	srGraphics.aShaderModules.emplace_back( ShaderStage_Fragment, gpFragShader, "main" );

	srGraphics.aColorBlendAttachments.emplace_back( true );

	srGraphics.aPrimTopology = EPrimTopology_Line;
	srGraphics.aDynamicState = EDynamicState_Viewport | EDynamicState_Scissor | EDynamicState_LineWidth;
	srGraphics.aCullMode     = ECullMode_Back;
}


ShaderCreate_t gShaderCreate_DebugLine = {
	.apName           = "debug_line",
	.aBindPoint       = EPipelineBindPoint_Graphics,
	.aFlags           = EShaderFlags_ViewInfo,
	.aDynamicState    = EDynamicState_Viewport | EDynamicState_Scissor | EDynamicState_LineWidth,
	.aVertexFormat    = VertexFormat_Position | VertexFormat_Color,
	.apInit           = nullptr,
	.apDestroy        = nullptr,
	.apLayoutCreate   = Shader_DebugLine_GetPipelineLayoutCreate,
	.apGraphicsCreate = Shader_DebugLine_GetGraphicsPipelineCreate,
};

