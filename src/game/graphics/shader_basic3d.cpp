#include "util.h"
#include "render/irender.h"
#include "graphics.h"


extern IRender*                             render;

static VertexFormat                         gVertexFormat          = VertexFormat_Position | VertexFormat_Normal | VertexFormat_TexCoord;

static Handle                               gPipeline              = InvalidHandle;
static Handle                               gPipelineLayout        = InvalidHandle;

static Handle                               gFallbackAO            = InvalidHandle;
static Handle                               gFallbackEmissive      = InvalidHandle;

constexpr const char*                       gpFallbackAOPath       = "materials/base/white.ktx";
constexpr const char*                       gpFallbackEmissivePath = "materials/base/black.ktx";

constexpr const char*                       gpVertShader           = "shaders/basic3d.vert.spv";
constexpr const char*                       gpFragShader           = "shaders/basic3d.frag.spv";

// descriptor set layouts
extern Handle                               gLayoutSampler;
extern Handle                               gLayoutViewProj;
extern Handle                               gLayoutMaterialBasic3D;

extern Handle                               gLayoutSamplerSets[ 2 ];
extern Handle                               gLayoutViewProjSets[ 2 ];
extern Handle*                              gLayoutMaterialBasic3DSets;

// Material Handle, Buffer
static std::unordered_map< Handle, Handle > gMaterialBuffers;
// static std::vector< MaterialBuffer_t > gMaterialBuffers;
// static std::unordered_map< Handle, u32 >                   gMaterialBufferIndex;
static std::vector< Handle >                gMaterialBufferIndex;

// constexpr const char*                gpVertShader = "shaders/unlit.vert.spv";
// constexpr const char*                gpFragShader = "shaders/unlit.frag.spv";


struct Basic3D_Push
{
	// glm::mat4 projView;
	// glm::mat4 model;

	// alignas( 16 ) glm::mat4 trans;
	// alignas( 16 ) int index;

	alignas( 16 ) glm::mat4 aModelMatrix{};  // model matrix
	alignas( 16 ) int aMaterial = 0;         // material index
	int aProjView = 0;         // projection * view index
};


struct Basic3D_Material
{
	int   diffuse       = 0;
	int   ao            = 0;
	int   emissive      = 0;

	float aoPower       = 1.f;
	float emissivePower = 1.f;
};

// struct Basic3D_UBO
// {
// 	int   diffuse = 0, ao = 0, emissive = 0;
// 	float aoPower = 1.f, emissivePower = 1.f;
// 
// 	float morphWeight = 0.f;
// };


static std::unordered_map< ModelSurfaceDraw_t*, Basic3D_Push > gPushData;
static std::unordered_map< Handle, Basic3D_Material >          gMaterialData;


// TODO: use this in a shader system later on, unless i go with json5 for the shader info
void Shader_Basic3D_GetCreateInfo( Handle sRenderPass, PipelineLayoutCreate_t& srPipeline, GraphicsPipelineCreate_t& srGraphics )
{
	srPipeline.aLayouts.push_back( gLayoutSampler );
	srPipeline.aLayouts.push_back( gLayoutViewProj );
	srPipeline.aLayouts.push_back( gLayoutMaterialBasic3D );
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

	// create fallback textures
	gFallbackAO       = render->LoadTexture( gpFallbackAOPath );
	gFallbackEmissive = render->LoadTexture( gpFallbackEmissivePath );

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

	render->FreeTexture( gFallbackAO );
	render->FreeTexture( gFallbackEmissive );

	Graphics_SetAllMaterialsDirty();
}


void Shader_Basic3D_ResetPushData()
{
	gPushData.clear();
}


void Shader_Basic3D_SetupPushData( ModelSurfaceDraw_t& srDrawInfo )
{
	Basic3D_Push& push = gPushData[ &srDrawInfo ];
	push.aModelMatrix  = srDrawInfo.apDraw->aModelMatrix;

	Handle mat         = Model_GetMaterial( srDrawInfo.apDraw->aModel, srDrawInfo.aSurface );
	// push.aMaterial     = GET_HANDLE_INDEX( mat );
	// push.aMaterial     = gMaterialBufferIndex[ mat ];
	push.aMaterial     = vec_index( gMaterialBufferIndex, mat );

	push.aProjView     = 0;
}


void Shader_Basic3D_Bind( Handle cmd, size_t sCmdIndex )
{
	Handle descSets[] = { gLayoutSamplerSets[ sCmdIndex ], gLayoutViewProjSets[ sCmdIndex ], gLayoutMaterialBasic3DSets[ sCmdIndex ] };
	render->CmdBindDescriptorSets( cmd, sCmdIndex, EPipelineBindPoint_Graphics, gPipelineLayout, descSets, 3 );
}


void Shader_Basic3D_PushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo )
{
	Basic3D_Push& push = gPushData.at( &srDrawInfo );

	render->CmdPushConstants( cmd, gPipelineLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Basic3D_Push ), &push );
}


bool Shader_Basic3D_CreateMaterialBuffer( Handle sMat )
{
	// IDEA: since materials shouldn't be updated very often,
	// maybe have the buffer be on the gpu (EBufferMemory_Device)?

	Handle buffer = render->CreateBuffer( Mat_GetName( sMat ), sizeof( Basic3D_Material ), EBufferFlags_Uniform, EBufferMemory_Host );

	if ( buffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Material Uniform Buffer\n" );
		return false;
	}

	gMaterialBuffers[ sMat ] = buffer;

	// update the material descriptor sets
	UpdateVariableDescSet_t update{};

	// what
	update.aDescSets.push_back( gLayoutMaterialBasic3DSets[ 0 ] );
	update.aDescSets.push_back( gLayoutMaterialBasic3DSets[ 1 ] );

	update.aType = EDescriptorType_UniformBuffer;

	// blech
	gMaterialBufferIndex.clear();
	for ( const auto& [ mat, buffer ] : gMaterialBuffers )
	{
		update.aBuffers.push_back( buffer );
		gMaterialBufferIndex.push_back( mat );
	}

	// update.aBuffers = gMaterialBuffers[ sMat ];
	render->UpdateVariableDescSet( update );

	return true;
}


// TODO: this doesn't handle shaders being changed on materials, or materials being freed
void Shader_Basic3D_UpdateMaterialData( Handle sMat )
{
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

	mat->aoPower       = Mat_GetFloat( sMat, "aoPower", 1.f );
	mat->emissivePower = Mat_GetFloat( sMat, "emissivePower", 0.f );

	// write new material data to the buffer
	Handle buffer = gMaterialBuffers[ sMat ];
	render->MemWriteBuffer( buffer, sizeof( Basic3D_Material ), mat );
}


// blech, awful
VertexFormat Shader_Basic3D_GetVertexFormat()
{
	return gVertexFormat;
}


Handle Shader_Basic3D_GetPipelineLayout()
{
	return gPipelineLayout;
}

