#include "util.h"
#include "render/irender.h"
#include "graphics.h"

#include "../entity.h"
#include "../player.h"

extern Entity         gLocalPlayer;

extern IRender*       render;

static Handle         gFSPipeline       = InvalidHandle;
static Handle         gFSPipelineLayout = InvalidHandle;

// constexpr const char* gpVertShader    = "shaders/basic3d.vert.spv";
// constexpr const char* gpFragShader    = "shaders/basic3d.frag.spv";

constexpr const char* gpVertShader    = "shaders/deferred_fullscreen.vert.spv";
constexpr const char* gpFragShader    = "shaders/deferred_fullscreen.frag.spv";

// descriptor set layouts
extern Handle         gLayoutSampler;
extern Handle         gLayoutSamplerSets[ 2 ];

extern Handle         gGBufferTex[ 6 ];  // pos, normal, color, ao, emission, depth

struct DeferredFS_Push
{
	glm::vec3 viewPos;
	int       debugView;
	int       position;
	int       normal;
	int       diffuse;
	int       ao;
	int       emissive;
};

// struct Basic3D_UBO
// {
// 	int   diffuse = 0, ao = 0, emissive = 0;
// 	float aoPower = 1.f, emissivePower = 1.f;
//
// 	float morphWeight = 0.f;
// };

static std::unordered_map< Handle, DeferredFS_Push > gPushData;

CONVAR( def_view, 0 );

Handle Shader_DeferredFS_Create( Handle sRenderPass, bool sRecreate )
{
	PipelineLayoutCreate_t pipelineCreateInfo{};
	pipelineCreateInfo.aLayouts.push_back( gLayoutSampler );
	pipelineCreateInfo.aPushConstants.emplace_back( ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( DeferredFS_Push ) );

	// --------------------------------------------------------------

	GraphicsPipelineCreate_t pipelineInfo{};

	pipelineInfo.apName = "deferred_fullscreen";
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Vertex, gpVertShader, "main" );
	pipelineInfo.aShaderModules.emplace_back( ShaderStage_Fragment, gpFragShader, "main" );

	pipelineInfo.aColorBlendAttachments.emplace_back( false );

	pipelineInfo.aPrimTopology   = EPrimTopology_Tri;
	pipelineInfo.aDynamicState   = EDynamicState_Viewport | EDynamicState_Scissor;
	pipelineInfo.aCullMode       = ECullMode_None;
	pipelineInfo.aPipelineLayout = gFSPipelineLayout;
	pipelineInfo.aRenderPass     = sRenderPass;

	// TODO: expose the rest later

	// --------------------------------------------------------------

	if ( sRecreate )
	{
		render->RecreatePipelineLayout( gFSPipelineLayout, pipelineCreateInfo );
		render->RecreateGraphicsPipeline( gFSPipeline, pipelineInfo );
	}
	else
	{
		gFSPipelineLayout            = render->CreatePipelineLayout( pipelineCreateInfo );
		pipelineInfo.aPipelineLayout = gFSPipelineLayout;
		gFSPipeline                  = render->CreateGraphicsPipeline( pipelineInfo );
	}

	return gFSPipeline;
}

void Shader_DeferredFS_Destroy()
{
	if ( gFSPipelineLayout )
		render->DestroyPipelineLayout( gFSPipelineLayout );

	if ( gFSPipeline )
		render->DestroyPipeline( gFSPipeline );

	gFSPipelineLayout = InvalidHandle;
	gFSPipeline       = InvalidHandle;
}

void Shader_DeferredFS_Draw( Handle cmd, size_t sCmdIndex )
{
	if ( !render->CmdBindPipeline( cmd, gFSPipeline ) )
	{
		Log_Error( gLC_ClientGraphics, "Deferred Fullscreen Shader Not Found!\n" );
		return;
	}

	DeferredFS_Push push{};

	// ??????????????????????????????????????
	push.viewPos   = GetTransform( gLocalPlayer ).aPos + GetCamera( gLocalPlayer ).aTransform.aPos;
	push.debugView = def_view;
	push.position  = render->GetTextureIndex( gGBufferTex[ 0 ] );
	push.normal    = render->GetTextureIndex( gGBufferTex[ 1 ] );
	push.diffuse   = render->GetTextureIndex( gGBufferTex[ 2 ] );
	push.ao        = render->GetTextureIndex( gGBufferTex[ 3 ] );
	push.emissive  = render->GetTextureIndex( gGBufferTex[ 4 ] );

	render->CmdPushConstants( cmd, gFSPipelineLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( DeferredFS_Push ), &push );
	render->CmdBindDescriptorSets( cmd, sCmdIndex, EPipelineBindPoint_Graphics, gFSPipelineLayout, &gLayoutSamplerSets[ sCmdIndex ], 1 );
	render->CmdDraw( cmd, 3, 1, 0, 0 );
}


