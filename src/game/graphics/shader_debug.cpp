#include "util.h"
#include "render/irender.h"
#include "graphics.h"


extern IRender*                             render;

constexpr VertexFormat                      gDbgVertexFormat       = VertexFormat_Position;
constexpr VertexFormat                      gDbgLineVertexFormat   = VertexFormat_Position | VertexFormat_Color;

static Handle                               gDbgPipeline           = InvalidHandle;
static Handle                               gDbgPipelineLayout     = InvalidHandle;

static Handle                               gDbgLinePipeline       = InvalidHandle;
static Handle                               gDbgLinePipelineLayout = InvalidHandle;

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


static std::unordered_map< ModelSurfaceDraw_t*, Debug_Push > gDebugPushData;


struct ShaderDebug : public IShader
{
	// Returns the shader name, and fills in data in the struct with general shader info
	const char* GetShaderInfo( ShaderInfo_t& srInfo ) override
	{
	}

	// Shader Creation Info
	void GetPushConstantData( std::vector< PushConstantRange_t >& srPushConstants ) override
	{
		srPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Debug_Push ) );
	}

	void GetGraphicsCreateInfo( Handle sRenderPass, GraphicsPipelineCreate_t& srGraphics ) override
	{
	}

	// Optional to override
	// Used if the shader has the push constants flag
	void ResetPushData() override
	{
	}

	// kinda weird and tied to models, hmm
	void ModelPushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo ) override
	{
	}

	void SetupModelPushData( ModelSurfaceDraw_t& srDrawInfo ) override
	{
	}
};


// ShaderDebug gShaderDebug;


Handle Shader_Debug_Create( Handle sRenderPass, bool sRecreate )
{
	PipelineLayoutCreate_t   pipelineCreateInfo{};
	GraphicsPipelineCreate_t pipelineInfo{};

	pipelineCreateInfo.aLayouts.push_back( gUniformViewInfo.aLayout );
	pipelineCreateInfo.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Debug_Push ) );

	// --------------------------------------------------------------

	pipelineInfo.apName = "debug";
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Vertex, gpVertColShader, "main" );
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Fragment, gpFragShader, "main" );

	Graphics_GetVertexBindingDesc( gDbgVertexFormat, pipelineInfo.aVertexBindings );
	Graphics_GetVertexAttributeDesc( gDbgVertexFormat, pipelineInfo.aVertexAttributes );

	pipelineInfo.aColorBlendAttachments.emplace_back( true );

	pipelineInfo.aPrimTopology   = EPrimTopology_Tri;
	pipelineInfo.aDynamicState   = EDynamicState_Viewport | EDynamicState_Scissor;
	pipelineInfo.aCullMode       = ECullMode_Back;
	pipelineInfo.aPipelineLayout = gDbgPipelineLayout;
	pipelineInfo.aRenderPass     = sRenderPass;

	// --------------------------------------------------------------

	if ( !render->CreatePipelineLayout( gDbgPipelineLayout, pipelineCreateInfo ) )
	{
		Log_Error( "Failed to create Pipeline Layout\n" );
		return InvalidHandle;
	}

	pipelineInfo.aPipelineLayout = gDbgPipelineLayout;
	if ( !render->CreateGraphicsPipeline( gDbgPipeline, pipelineInfo ) )
	{
		Log_Error( "Failed to create Graphics Pipeline\n" );
		return InvalidHandle;
	}

	return gDbgPipeline;
}


Handle Shader_DebugLine_Create( Handle sRenderPass, bool sRecreate )
{
	PipelineLayoutCreate_t   pipelineCreateInfo{};
	GraphicsPipelineCreate_t pipelineInfo{};

	pipelineCreateInfo.aLayouts.push_back( gUniformViewInfo.aLayout );

	// --------------------------------------------------------------

	pipelineInfo.apName = "debug_line";
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Vertex, gpVertShader, "main" );
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Fragment, gpFragShader, "main" );

	Graphics_GetVertexBindingDesc( gDbgLineVertexFormat, pipelineInfo.aVertexBindings );
	Graphics_GetVertexAttributeDesc( gDbgLineVertexFormat, pipelineInfo.aVertexAttributes );

	pipelineInfo.aColorBlendAttachments.emplace_back( true );

	pipelineInfo.aPrimTopology   = EPrimTopology_Line;
	pipelineInfo.aDynamicState   = EDynamicState_Viewport | EDynamicState_Scissor;
	pipelineInfo.aCullMode       = ECullMode_Back;
	pipelineInfo.aPipelineLayout = gDbgLinePipelineLayout;
	pipelineInfo.aRenderPass     = sRenderPass;

	// --------------------------------------------------------------

	if ( !render->CreatePipelineLayout( gDbgLinePipelineLayout, pipelineCreateInfo ) )
	{
		Log_Error( "Failed to create Pipeline Layout\n" );
		return InvalidHandle;
	}

	pipelineInfo.aPipelineLayout = gDbgLinePipelineLayout;
	if ( !render->CreateGraphicsPipeline( gDbgLinePipeline, pipelineInfo ) )
	{
		Log_Error( "Failed to create Graphics Pipeline\n" );
		return InvalidHandle;
	}

	return gDbgLinePipeline;
}


void Shader_Debug_Destroy()
{
	if ( gDbgPipelineLayout )
		render->DestroyPipelineLayout( gDbgPipelineLayout );

	if ( gDbgPipeline )
		render->DestroyPipeline( gDbgPipeline );

	if ( gDbgLinePipelineLayout )
		render->DestroyPipelineLayout( gDbgLinePipelineLayout );

	if ( gDbgLinePipeline )
		render->DestroyPipeline( gDbgLinePipeline );

	gDbgPipelineLayout     = InvalidHandle;
	gDbgPipeline           = InvalidHandle;
	gDbgLinePipelineLayout = InvalidHandle;
	gDbgLinePipeline       = InvalidHandle;
}


void Shader_Debug_ResetPushData()
{
	gDebugPushData.clear();
}


void Shader_Debug_SetupPushData( ModelSurfaceDraw_t& srDrawInfo )
{
	Debug_Push& push  = gDebugPushData[ &srDrawInfo ];
	push.aModelMatrix = srDrawInfo.apDraw->aModelMatrix;
	Handle mat        = Model_GetMaterial( srDrawInfo.apDraw->aModel, srDrawInfo.aSurface );
	push.aColor       = Mat_GetVec4( mat, "color" );
}


void Shader_Debug_PushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo )
{
	Debug_Push& push = gDebugPushData.at( &srDrawInfo );
	render->CmdPushConstants( cmd, gDbgPipelineLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Debug_Push ), &push );
}


void Shader_DebugLine_SetupPushData( ModelSurfaceDraw_t& srDrawInfo )
{
	// Debug_Push& push = gDebugPushData[ &srDrawInfo ];
	// Handle mat       = Model_GetMaterial( srDrawInfo.apDraw->aModel, srDrawInfo.aSurface );
	// push.aColor      = Mat_GetVec4( mat, "color" );
}


void Shader_DebugLine_PushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo )
{
	// Debug_Push& push = gDebugPushData.at( &srDrawInfo );
	// render->CmdPushConstants( cmd, gDbgPipelineLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Debug_Push ), &push );
}


// blech, awful
VertexFormat Shader_Debug_GetVertexFormat()
{
	return gDbgVertexFormat;
}


VertexFormat Shader_DebugLine_GetVertexFormat()
{
	return gDbgLineVertexFormat;
}


Handle Shader_Debug_GetPipelineLayout()
{
	return gDbgPipelineLayout;
}


Handle Shader_DebugLine_GetPipelineLayout()
{
	return gDbgLinePipelineLayout;
}

