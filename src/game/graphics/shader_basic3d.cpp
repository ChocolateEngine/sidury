#include "util.h"
#include "render/irender.h"
#include "graphics.h"


extern IRender*       render;

// static VertexFormat   gVertexFormat   = VertexFormat_Position | VertexFormat_Normal | VertexFormat_TexCoord;
static VertexFormat   gVertexFormat   = VertexFormat_Position | VertexFormat_TexCoord;

static Handle         gPipeline       = InvalidHandle;
static Handle         gPipelineLayout = InvalidHandle;

// constexpr const char* gpVertShader    = "shaders/basic3d.vert.spv";
// constexpr const char* gpFragShader    = "shaders/basic3d.frag.spv";

constexpr const char* gpVertShader    = "shaders/unlit.vert.spv";
constexpr const char* gpFragShader    = "shaders/unlit.frag.spv";


struct Basic3D_Push
{
	// glm::mat4 projView;
	// glm::mat4 model;

	alignas( 16 ) glm::mat4 trans;
	alignas( 16 ) int index;
};


// struct Basic3D_UBO
// {
// 	int   diffuse = 0, ao = 0, emissive = 0;
// 	float aoPower = 1.f, emissivePower = 1.f;
// 
// 	float morphWeight = 0.f;
// };


static std::unordered_map< ModelSurfaceDraw_t*, Basic3D_Push > gPushData;


Handle Shader_Basic3D_Create()
{
	PipelineLayoutCreate_t pipelineCreateInfo{};
	// pipelineCreateInfo.aLayouts = DescriptorLayout_Image | DescriptorLayout_UniformBuffer;
	pipelineCreateInfo.aLayouts = EDescriptorLayout_Image;
	pipelineCreateInfo.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Basic3D_Push ) );

	gPipelineLayout = render->CreatePipelineLayout( pipelineCreateInfo );

	// --------------------------------------------------------------

	GraphicsPipelineCreate_t pipelineInfo{};

	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Vertex, gpVertShader, "main" );
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Fragment, gpFragShader, "main" );

	Graphics_GetVertexBindingDesc( gVertexFormat, pipelineInfo.aVertexBindings );
	Graphics_GetVertexAttributeDesc( gVertexFormat, pipelineInfo.aVertexAttributes );

	pipelineInfo.aPrimTopology   = EPrimTopology_Tri;
	pipelineInfo.aDynamicState   = EDynamicState_Viewport | EDynamicState_Scissor;
	pipelineInfo.aCullMode       = ECullMode_Back;
	pipelineInfo.aPipelineLayout = gPipelineLayout;

	// TODO: expose the rest later

	gPipeline = render->CreateGraphicsPipeline( pipelineInfo );
	return gPipeline;
}


void Shader_Basic3D_ResetPushData()
{
	gPushData.clear();
}


void Shader_Basic3D_SetupPushData( ModelSurfaceDraw_t& srDrawInfo )
{
	Basic3D_Push& push = gPushData[ &srDrawInfo ];
	push.trans = Graphics_GetViewProjMatrix() * srDrawInfo.apDraw->aModelMatrix;

	Handle mat = Model_GetMaterial( srDrawInfo.apDraw->aModel, srDrawInfo.aSurface );
	Handle tex = Mat_GetTexture( mat, "diffuse", InvalidHandle );
	push.index = render->GetTextureIndex( tex );
}


void Shader_Basic3D_Bind( Handle cmd, size_t sCmdIndex )
{
	render->CmdBindDescriptorSets( cmd, sCmdIndex, EPipelineBindPoint_Graphics, gPipelineLayout, EDescriptorLayout_Image );
}


void Shader_Basic3D_PushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo )
{
	Basic3D_Push& push = gPushData.at( &srDrawInfo );

	render->CmdPushConstants( cmd, gPipelineLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Basic3D_Push ), &push );
}


// blech, awful
VertexFormat Shader_Basic3D_GetVertexFormat()
{
	return gVertexFormat;
}

