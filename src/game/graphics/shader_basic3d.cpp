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


// TODO: use this in a shader system later on, unless i go with json5 for the shader info
void Shader_Basic3D_GetCreateInfo( Handle sRenderPass, PipelineLayoutCreate_t& srPipeline, GraphicsPipelineCreate_t& srGraphics )
{
	// srPipeline.aLayouts = DescriptorLayout_Image | DescriptorLayout_UniformBuffer;
	srPipeline.aLayouts = EDescriptorLayout_Image;
	srPipeline.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Basic3D_Push ) );

	// --------------------------------------------------------------

	srGraphics.aShaderModules.emplace_back( ShaderStage_Vertex, gpVertShader, "main" );
	srGraphics.aShaderModules.emplace_back( ShaderStage_Fragment, gpFragShader, "main" );

	Graphics_GetVertexBindingDesc( gVertexFormat, srGraphics.aVertexBindings );
	Graphics_GetVertexAttributeDesc( gVertexFormat, srGraphics.aVertexAttributes );

	srGraphics.aPrimTopology     = EPrimTopology_Tri;
	srGraphics.aDynamicState     = EDynamicState_Viewport | EDynamicState_Scissor;
	srGraphics.aCullMode         = ECullMode_Back;
	srGraphics.aPipelineLayout   = gPipelineLayout;
	srGraphics.aRenderPass       = sRenderPass;
	// TODO: expose the rest later
}


Handle Shader_Basic3D_Create( Handle sRenderPass, bool sRecreate )
{
	PipelineLayoutCreate_t   pipelineCreateInfo{};
	GraphicsPipelineCreate_t pipelineInfo{};

	Shader_Basic3D_GetCreateInfo( sRenderPass, pipelineCreateInfo, pipelineInfo );

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


void Shader_Basic3D_Destroy()
{
	if ( gPipelineLayout )
		render->DestroyPipelineLayout( gPipelineLayout );

	if ( gPipeline )
		render->DestroyPipeline( gPipeline );

	gPipelineLayout = InvalidHandle;
	gPipeline       = InvalidHandle;
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

