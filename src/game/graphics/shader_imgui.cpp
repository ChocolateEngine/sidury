#include "util.h"
#include "render/irender.h"
#include "graphics.h"


extern IRender*       render;

static Handle         gPipeline       = InvalidHandle;
static Handle         gPipelineLayout = InvalidHandle;

// constexpr const char* gpVertShader    = "shaders/basic3d.vert.spv";
// constexpr const char* gpFragShader    = "shaders/basic3d.frag.spv";

constexpr const char* gpVertShader    = "shaders/imgui.vert.spv";
constexpr const char* gpFragShader    = "shaders/imgui.frag.spv";


struct UI_Push
{
	int index;
};


// struct Basic3D_UBO
// {
// 	int   diffuse = 0, ao = 0, emissive = 0;
// 	float aoPower = 1.f, emissivePower = 1.f;
// 
// 	float morphWeight = 0.f;
// };


static std::unordered_map< Handle, UI_Push > gPushData;


Handle Shader_UI_Create()
{
	PipelineLayoutCreate_t pipelineCreateInfo{};
	pipelineCreateInfo.aLayouts = EDescriptorLayout_Image;
	pipelineCreateInfo.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( UI_Push ) );

	gPipelineLayout = render->CreatePipelineLayout( pipelineCreateInfo );

	// --------------------------------------------------------------

	GraphicsPipelineCreate_t pipelineInfo{};

	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Vertex, gpVertShader, "main" );
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Fragment, gpFragShader, "main" );

	pipelineInfo.aPrimTopology   = EPrimTopology_Tri;
	pipelineInfo.aDynamicState   = EDynamicState_Viewport | EDynamicState_Scissor;
	pipelineInfo.aCullMode       = ECullMode_None;
	pipelineInfo.aPipelineLayout = gPipelineLayout;

	// TODO: expose the rest later

	gPipeline                    = render->CreateGraphicsPipeline( pipelineInfo );
	return gPipeline;
}


void Shader_UI_SetupPushData( Handle shColor )
{
	UI_Push& push = gPushData[ shColor ];
	push.index    = render->GetTextureIndex( shColor );
}


void Shader_UI_Draw( Handle cmd, size_t sCmdIndex, Handle shColor )
{
	// UI_Push& push = gPushData.at( shColor );
	UI_Push push{};
	push.index    = render->GetTextureIndex( shColor );

	render->CmdBindPipeline( cmd, gPipeline );
	render->CmdPushConstants( cmd, gPipelineLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( UI_Push ), &push );
	render->CmdBindDescriptorSets( cmd, sCmdIndex, EPipelineBindPoint_Graphics, gPipelineLayout, EDescriptorLayout_Image );
	render->CmdDraw( cmd, 3, 1, 0, 0 );
}

