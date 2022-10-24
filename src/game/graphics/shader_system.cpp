#include "util.h"
#include "render/irender.h"
#include "graphics.h"


extern IRender*                                         render;

// temp shader
static Handle                                           gTempShader   = InvalidHandle;
static Handle                                           gSkyboxShader = InvalidHandle;
static Handle                                           gUIShader     = InvalidHandle;

static std::unordered_map< std::string_view, Handle >   gShaderNames;

static std::unordered_map< Handle, EShaderFlags >       gShaderFlags;      // [name]   = shader
static std::unordered_map< Handle, Handle >             gShaderLayouts;    // [name]   = pipeline layout
static std::unordered_map< Handle, EPipelineBindPoint > gShaderBindPoint;  // [shader] = bind point
static std::unordered_map< Handle, Handle >             gShaderMaterials;  // [shader] = uniform layout

// static Handle                                    gPipeline       = InvalidHandle;
// static Handle                                    gPipelineLayout = InvalidHandle;

// descriptor set layouts
extern Handle                                           gLayoutSampler;
// extern Handle                                    gLayoutStorage;
extern Handle                                           gLayoutViewProj;
// extern Handle                                           gLayoutModelMatrix;

extern Handle                                           gRenderPassGraphics;
extern Handle                                           gRenderPassUI;

extern Handle                                           gLayoutSamplerSets[ 2 ];
extern Handle                                           gLayoutViewProjSets[ 2 ];
extern Handle*                                          gLayoutMaterialBasic3DSets;

struct Shader_PushConst
{
	int       aMaterial = 0;   // material index
	int       aProjView = 0;   // projection * view index
	glm::mat4 aModelMatrix{};  // model matrix
	// int       aModelMatrix = 0;  // model matrix index
};


// --------------------------------------------------------------------------------------

// shaders, fun
Handle       Shader_Basic3D_Create( Handle sRenderPass, bool sRecreate );
void         Shader_Basic3D_Destroy();
void         Shader_Basic3D_Bind( Handle cmd, size_t sCmdIndex );
void         Shader_Basic3D_PushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo );
void         Shader_Basic3D_ResetPushData();
void         Shader_Basic3D_SetupPushData( ModelSurfaceDraw_t& srDrawInfo );
VertexFormat Shader_Basic3D_GetVertexFormat();
Handle       Shader_Basic3D_GetPipelineLayout();

Handle       Shader_Skybox_Create( Handle sRenderPass, bool sRecreate );
void         Shader_Skybox_Destroy();
void         Shader_Skybox_Bind( Handle cmd, size_t sCmdIndex );
void         Shader_Skybox_PushConstants( Handle cmd, size_t sCmdIndex, ModelSurfaceDraw_t& srDrawInfo );
void         Shader_Skybox_ResetPushData();
void         Shader_Skybox_SetupPushData( ModelSurfaceDraw_t& srDrawInfo );
VertexFormat Shader_Skybox_GetVertexFormat();
void         Shader_Skybox_UpdateMaterialData( Handle sMat );
Handle       Shader_Skybox_GetPipelineLayout();

Handle       Shader_UI_Create( Handle sRenderPass, bool sRecreate );
void         Shader_UI_Destroy();
void         Shader_UI_Draw( Handle cmd, size_t sCmdIndex, Handle shColor );

// --------------------------------------------------------------------------------------


void Graphics_AddShader( const char* spName, Handle sShader, EShaderFlags sFlags, EPipelineBindPoint sBindPoint )
{
	if ( !gShaderNames.empty() )
	{
		auto it = gShaderNames.find( spName );
		if ( it != gShaderNames.end() )
		{
			Log_WarnF( gLC_ClientGraphics, "Graphics_AddShader: Shader Already Registered: %s\n", spName );
			return;
		}
	}

	gShaderNames[ spName ]      = sShader;
	gShaderFlags[ sShader ]     = sFlags;
	gShaderBindPoint[ sShader ] = sBindPoint;
}


Handle Graphics_GetShader( std::string_view sName )
{
	auto it = gShaderNames.find( sName );
	if ( it != gShaderNames.end() )
		return it->second;

	Log_ErrorF( gLC_ClientGraphics, "Graphics_GetShader: Shader not found: %s\n", sName );
	return InvalidHandle;
}


Handle Shader_GetPipelineLayout( Handle sShader )
{
	// HACK HACK HACK
	if ( sShader == gTempShader )
		return Shader_Basic3D_GetPipelineLayout();

	if ( sShader == gSkyboxShader )
		return Shader_Skybox_GetPipelineLayout();

	return InvalidHandle;
}


bool Graphics_ShaderInit( bool sRecreate )
{
	if ( !( gUIShader = Shader_UI_Create( gRenderPassGraphics, sRecreate ) ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create ui shader\n" );
		return false;
	}

	if ( !( gTempShader = Shader_Basic3D_Create( gRenderPassGraphics, sRecreate ) ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create temp shader\n" );
		return false;
	}

	if ( !( gSkyboxShader = Shader_Skybox_Create( gRenderPassGraphics, sRecreate ) ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create skybox shader\n" );
		return false;
	}

	if ( !sRecreate )
	{
		Graphics_AddShader( "basic_3d", gTempShader,
		                    EShaderFlags_Sampler | EShaderFlags_ViewProj | EShaderFlags_PushConstant | EShaderFlags_MaterialUniform,
		                    EPipelineBindPoint_Graphics );

		Graphics_AddShader( "skybox", gSkyboxShader,
		                    EShaderFlags_Sampler | EShaderFlags_PushConstant,
		                    EPipelineBindPoint_Graphics );

		Graphics_AddShader( "imgui", gUIShader,
		                    EShaderFlags_Sampler | EShaderFlags_PushConstant,
		                    EPipelineBindPoint_Graphics );
	}

	return true;
}


EShaderFlags Shader_GetFlags( Handle sShader )
{
	auto it = gShaderFlags.find( sShader );
	if ( it != gShaderFlags.end() )
		return it->second;
	
	Log_Error( gLC_ClientGraphics, "Unable to find Shader Flags!\n" );
	return EShaderFlags_None;
}


EPipelineBindPoint Shader_GetPipelineBindPoint( Handle sShader )
{
	auto it = gShaderBindPoint.find( sShader );
	if ( it != gShaderBindPoint.end() )
		return it->second;
	
	Log_Error( gLC_ClientGraphics, "Unable to find Shader Pipeline Bind Point!\n" );
	return EPipelineBindPoint_Graphics;
}


Handle* Shader_GetMaterialUniform( Handle sShader )
{
	// HACK HACK HACK
	if ( sShader == gTempShader )
		return gLayoutMaterialBasic3DSets;
	else
		return nullptr;

	// auto it = gShaderMaterials.find( sShader );
	// if ( it != gShaderMaterials.end() )
	// 	return it->second;
	// 
	// Log_Error( gLC_ClientGraphics, "Unable to find Shader Material Uniform Layout!\n" );
	// return nullptr;
}


bool Shader_Bind( Handle sCmd, u32 sIndex, Handle sShader )
{
	if ( !render->CmdBindPipeline( sCmd, sShader ) )
		return false;

	EShaderFlags shaderFlags = Shader_GetFlags( sShader );

	std::vector< Handle > descSets;

	if ( shaderFlags & EShaderFlags_Sampler )
		descSets.push_back( gLayoutSamplerSets[ sIndex ] );

	if ( shaderFlags & EShaderFlags_ViewProj )
		descSets.push_back( gLayoutViewProjSets[ sIndex ] );

	if ( shaderFlags & EShaderFlags_MaterialUniform )
	{
		Handle* mats = Shader_GetMaterialUniform( sShader );
		if ( mats == nullptr )
			return false;

		descSets.push_back( mats[ sIndex ] );
	}

	if ( descSets.size() )
	{
		Handle             layout    = Shader_GetPipelineLayout( sShader );
		EPipelineBindPoint bindPoint = Shader_GetPipelineBindPoint( sShader );

		render->CmdBindDescriptorSets( sCmd, sIndex, bindPoint, layout, descSets.data(), static_cast< u32 >( descSets.size() ) );
	}

	return true;
}


void Shader_ResetPushData()
{
	Shader_Basic3D_ResetPushData();
	Shader_Skybox_ResetPushData();
}


bool Shader_SetupRenderableDrawData( Handle sShader, ModelSurfaceDraw_t& srRenderable )
{
	EShaderFlags shaderFlags = Shader_GetFlags( sShader );

	if ( shaderFlags & EShaderFlags_PushConstant )
	{
		if ( sShader == gTempShader )
			Shader_Basic3D_SetupPushData( srRenderable );

		else if ( sShader == gSkyboxShader )
			Shader_Skybox_SetupPushData( srRenderable );
	}

	return true;
}


bool Shader_PreRenderableDraw( Handle sCmd, u32 sIndex, Handle sShader, ModelSurfaceDraw_t& srRenderable )
{
	EShaderFlags shaderFlags = Shader_GetFlags( sShader );

	if ( shaderFlags & EShaderFlags_PushConstant )
	{
		if ( sShader == gTempShader )
			Shader_Basic3D_PushConstants( sCmd, sIndex, srRenderable );

		else if ( sShader == gSkyboxShader )
			Shader_Skybox_PushConstants( sCmd, sIndex, srRenderable );
	}

	return true;
}


VertexFormat Shader_GetVertexFormat( Handle sShader )
{
	// HACK HACK HACK
	if ( sShader == gTempShader )
		return Shader_Basic3D_GetVertexFormat();

	if ( sShader == gSkyboxShader )
		return Shader_Skybox_GetVertexFormat();

	return VertexFormat_None;
}

