#include "util.h"
#include "render/irender.h"
#include "graphics.h"


struct ShaderData_t
{
	ShaderStage    aStages = ShaderStage_None;
};


extern IRender*                                         render;

// temp shader
static Handle                                           gTempShader       = InvalidHandle;
static Handle                                           gSkyboxShader     = InvalidHandle;
static Handle                                           gUIShader         = InvalidHandle;
static Handle                                           gShadowShader     = InvalidHandle;
static Handle                                           gDbgShader        = InvalidHandle;
static Handle                                           gDbgLineShader    = InvalidHandle;

static std::unordered_map< std::string_view, Handle >   gShaderNames;

static std::unordered_map< Handle, EShaderFlags >       gShaderFlags;      // [shader] = flags
static std::unordered_map< Handle, Handle >             gShaderLayouts;    // [shader] = pipeline layout
static std::unordered_map< Handle, EPipelineBindPoint > gShaderBindPoint;  // [shader] = bind point
static std::unordered_map< Handle, Handle >             gShaderMaterials;  // [shader] = uniform layout
static std::unordered_map< Handle, VertexFormat >       gShaderVertFormat; // [shader] = vertex format
static std::unordered_map< Handle, FShader_Destroy* >   gShaderDestroy;    // [shader] = shader destroy function
static std::unordered_map< Handle, IShaderPush* >       gShaderPush;       // [shader] = shader push interface (REMOVE THIS ONE DAY)
static std::unordered_map< Handle, ShaderData_t >       gShaderData;       // [shader] = assorted shader data

extern Handle                                           gRenderPassGraphics;
extern Handle                                           gRenderPassShadow;

// descriptor set layouts
extern UniformBufferArray_t                             gUniformSampler;
extern UniformBufferArray_t                             gUniformViewInfo;
extern UniformBufferArray_t                             gUniformMaterialBasic3D;
extern UniformBufferArray_t                             gUniformLightInfo;
extern UniformBufferArray_t                             gUniformLightDirectional;
extern UniformBufferArray_t                             gUniformLightPoint;
extern UniformBufferArray_t                             gUniformLightCone;
extern UniformBufferArray_t                             gUniformLightCapsule;

static std::unordered_map< ModelSurfaceDraw_t*, void* > gShaderPushData;

CONCMD( shader_reload )
{
	render->WaitForQueues();

	Graphics_ShaderInit( true );
}


CONCMD( shader_dump )
{
	Log_MsgF( gLC_ClientGraphics, "Shader Count: %zd\n", gShaderNames.size() );

	for ( const auto& [name, shader] : gShaderNames )
	{
		Log_MsgF( gLC_ClientGraphics, "  %s\n", name.data() );
	}

	Log_Msg( gLC_ClientGraphics, "\n" );
}


// --------------------------------------------------------------------------------------
// Shaders

extern ShaderCreate_t gShaderCreate_Basic3D;
extern ShaderCreate_t gShaderCreate_Debug;
extern ShaderCreate_t gShaderCreate_DebugLine;
extern ShaderCreate_t gShaderCreate_Skybox;
extern ShaderCreate_t gShaderCreate_ShadowMap;

// --------------------------------------------------------------------------------------


// TODO: this is mainly geared toward graphics shaders, compute shaders will be a little different
void Graphics_AddShader(
  const char*        spName,
  Handle             sShader,
  Handle             sLayout,
  EShaderFlags       sFlags,
  VertexFormat       sVertFormat,
  EPipelineBindPoint sBindPoint )
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

	gShaderNames[ spName ]       = sShader;
	gShaderFlags[ sShader ]      = sFlags;
	gShaderLayouts[ sShader ]    = sLayout;
	gShaderBindPoint[ sShader ]  = sBindPoint;
	gShaderVertFormat[ sShader ] = sVertFormat;
}


Handle Graphics_GetShader( std::string_view sName )
{
	auto it = gShaderNames.find( sName );
	if ( it != gShaderNames.end() )
		return it->second;

	Log_ErrorF( gLC_ClientGraphics, "Graphics_GetShader: Shader not found: %s\n", sName.data() );
	return InvalidHandle;
}


const char* Graphics_GetShaderName( Handle sShader )
{
	for ( const auto& [name, shader] : gShaderNames )
	{
		if ( shader == sShader )
		{
			return name.data();
		}
	}

	Log_ErrorF( gLC_ClientGraphics, "Graphics_GetShader: Shader not found: %zd\n", sShader );
	return nullptr;
}


Handle Shader_GetPipelineLayout( Handle sShader )
{
	auto it = gShaderLayouts.find( sShader );
	if ( it != gShaderLayouts.end() )
		return it->second;

	Log_ErrorF( gLC_ClientGraphics, "Shader_GetPipelineLayout(): Failed get Shader Pipeline Layout %zd\n", sShader );
	return InvalidHandle;
}


void Graphics_AddPipelineLayouts( PipelineLayoutCreate_t& srPipeline, EShaderFlags sFlags )
{
	if ( sFlags & EShaderFlags_Sampler )
		srPipeline.aLayouts.push_back( gUniformSampler.aLayout );

	if ( sFlags & EShaderFlags_ViewInfo )
		srPipeline.aLayouts.push_back( gUniformViewInfo.aLayout );

	if ( sFlags & EShaderFlags_Lights )
	{
		srPipeline.aLayouts.push_back( gUniformLightInfo.aLayout );
		srPipeline.aLayouts.push_back( gUniformLightDirectional.aLayout );
		srPipeline.aLayouts.push_back( gUniformLightPoint.aLayout );
		srPipeline.aLayouts.push_back( gUniformLightCone.aLayout );
		srPipeline.aLayouts.push_back( gUniformLightCapsule.aLayout );
	}

	// if ( sFlags & EShaderFlags_MaterialUniform )
}


bool Shader_CreatePipelineLayout( Handle& srLayout, FShader_GetPipelineLayoutCreate fCreate )
{
	if ( fCreate == nullptr )
	{
		Log_Error( gLC_ClientGraphics, "FShader_GetPipelineLayoutCreate is nullptr!\n" );
		return false;
	}

	PipelineLayoutCreate_t pipelineCreate{};
	fCreate( pipelineCreate );

	if ( !render->CreatePipelineLayout( srLayout, pipelineCreate ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Pipeline Layout\n" );
		return false;
	}
	
	return true;
}


bool Shader_CreateGraphicsPipeline( ShaderCreate_t& srCreate, Handle& srPipeline, Handle& srLayout, Handle sRenderPass )
{
	if ( srCreate.apGraphicsCreate == nullptr )
	{
		Log_Error( gLC_ClientGraphics, "FShader_GetGraphicsPipelineCreate is nullptr!\n" );
		return false;
	}

	GraphicsPipelineCreate_t pipelineCreate{};
	srCreate.apGraphicsCreate( pipelineCreate );

	Graphics_GetVertexBindingDesc( srCreate.aVertexFormat, pipelineCreate.aVertexBindings );
	Graphics_GetVertexAttributeDesc( srCreate.aVertexFormat, pipelineCreate.aVertexAttributes );

	pipelineCreate.aRenderPass     = sRenderPass;
	pipelineCreate.apName          = srCreate.apName;
	pipelineCreate.aPipelineLayout = srLayout;

	if ( !render->CreateGraphicsPipeline( srPipeline, pipelineCreate ) )
	{
		Log_ErrorF( "Failed to create Graphics Pipeline for Shader \"%s\"\n", srCreate.apName );
		return false;
	}

	return true;
}


bool Graphics_CreateShader( bool sRecreate, Handle sRenderPass, ShaderCreate_t& srCreate )
{
	Handle layout   = InvalidHandle;
	Handle pipeline = InvalidHandle;

	auto   nameFind = gShaderNames.find( srCreate.apName );
	if ( nameFind != gShaderNames.end() )
		pipeline = nameFind->second;

	if ( pipeline )
	{
		auto it = gShaderLayouts.find( pipeline );
		if ( it != gShaderLayouts.end() )
			layout = it->second;
	}

	if ( !Shader_CreatePipelineLayout( layout, srCreate.apLayoutCreate ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Pipeline Layout\n" );
		return false;
	}

	if ( !Shader_CreateGraphicsPipeline( srCreate, pipeline, layout, sRenderPass ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Graphics Pipeline\n" );
		return false;
	}

	gShaderNames[ srCreate.apName ] = pipeline;
	gShaderFlags[ pipeline ]        = srCreate.aFlags;
	gShaderLayouts[ pipeline ]      = layout;
	gShaderBindPoint[ pipeline ]    = srCreate.aBindPoint;
	gShaderVertFormat[ pipeline ]   = srCreate.aVertexFormat;

	if ( srCreate.apShaderPush )
		gShaderPush[ pipeline ] = srCreate.apShaderPush;

	gShaderData[ pipeline ]         = {
				.aStages   = srCreate.aStages,
	};

	if ( !sRecreate )
	{
		if ( srCreate.apDestroy )
			gShaderDestroy[ pipeline ] = srCreate.apDestroy;

		if ( srCreate.apInit )
			return srCreate.apInit();
	}

	return true;
}


void Shader_Destroy( Handle sShader )
{
	auto it = gShaderLayouts.find( sShader );
	if ( it != gShaderLayouts.end() )
	{
		Log_WarnF( gLC_ClientGraphics, "Shader_Destroy: Failed to find shader: %zd\n", sShader );
		return;
	}

	render->DestroyPipelineLayout( it->second );
	render->DestroyPipeline( sShader );

	for ( const auto& [ name, shader ] : gShaderNames )
	{
		if ( shader == sShader )
		{
			gShaderNames.erase( name );
			break;
		}
	}

	gShaderFlags.erase( sShader );
	gShaderLayouts.erase( sShader );
	gShaderBindPoint.erase( sShader );
	gShaderVertFormat.erase( sShader );
	gShaderData.erase( sShader );
}


bool Graphics_ShaderInit( bool sRecreate )
{
	if ( !Graphics_CreateShader( sRecreate, gRenderPassGraphics, gShaderCreate_Basic3D ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create basic_3d shader\n" );
		return false;
	}

	if ( !Graphics_CreateShader( sRecreate, gRenderPassGraphics, gShaderCreate_Debug ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create debug shader\n" );
		return false;
	}

	if ( !Graphics_CreateShader( sRecreate, gRenderPassGraphics, gShaderCreate_DebugLine ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create debug_line shader\n" );
		return false;
	}

	if ( !Graphics_CreateShader( sRecreate, gRenderPassGraphics, gShaderCreate_Skybox ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create skybox shader\n" );
		return false;
	}

	if ( !Graphics_CreateShader( sRecreate, gRenderPassShadow, gShaderCreate_ShadowMap ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create shadow_map shader\n" );
		return false;
	}

	return true;
}


void Graphics_ShaderDestroy()
{
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


IShaderPush* Shader_GetPushInterface( Handle sShader )
{
	auto it = gShaderPush.find( sShader );
	if ( it != gShaderPush.end() )
		return it->second;
	
	Log_Error( gLC_ClientGraphics, "Unable to find Shader Push Constants Interface!\n" );
	return nullptr;
}


ShaderData_t* Shader_GetData( Handle sShader )
{
	auto it = gShaderData.find( sShader );
	if ( it != gShaderData.end() )
		return &it->second;
	
	Log_Error( gLC_ClientGraphics, "Unable to find Shader Data!\n" );
	return nullptr;
}


Handle* Shader_GetMaterialUniform( Handle sShader )
{
	// HACK HACK HACK
	// if ( sShader == gTempShader )
	return gUniformMaterialBasic3D.aSets.data();

	// Log_Error( gLC_ClientGraphics, "TODO: PROPERLY IMPLEMENT GETTING SHADER MATERIAL UNIFORM BUFFER!\n" );
	// return nullptr;

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
		descSets.push_back( gUniformSampler.aSets[ sIndex ] );

	if ( shaderFlags & EShaderFlags_ViewInfo )
		descSets.push_back( gUniformViewInfo.aSets[ sIndex ] );

	if ( shaderFlags & EShaderFlags_Lights )
	{
		for ( const auto& set : gUniformLightInfo.aSets )
			descSets.push_back( set );

		for ( const auto& set : gUniformLightDirectional.aSets )
			descSets.push_back( set );

		for ( const auto& set : gUniformLightPoint.aSets )
			descSets.push_back( set );
		
		for ( const auto& set : gUniformLightCone.aSets )
			descSets.push_back( set );
		
		for ( const auto& set : gUniformLightCapsule.aSets )
			descSets.push_back( set );
	}

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
	for ( auto& [ renderable, push ] : gShaderPush )
	{
		push->apReset();
	}
	
	// for ( auto& [ renderable, data ] : gShaderPushData )
	// {
	// 	delete data;
	// }
	// 
	// gShaderPushData.clear();
}


bool Shader_SetupRenderableDrawData( Handle sShader, ModelSurfaceDraw_t& srRenderable )
{
	EShaderFlags shaderFlags = Shader_GetFlags( sShader );

	if ( shaderFlags & EShaderFlags_PushConstant )
	{
		IShaderPush* shaderPush = Shader_GetPushInterface( sShader );
		if ( !shaderPush )
			return false;

		shaderPush->apSetup( srRenderable );
	}

	return true;
}


bool Shader_PreRenderableDraw( Handle sCmd, u32 sIndex, Handle sShader, ModelSurfaceDraw_t& srRenderable )
{
	EShaderFlags  shaderFlags = Shader_GetFlags( sShader );
	Handle        layout      = Shader_GetPipelineLayout( sShader );

	if ( shaderFlags & EShaderFlags_PushConstant )
	{
		IShaderPush* shaderPush = Shader_GetPushInterface( sShader );
		shaderPush->apPush( sCmd, layout, srRenderable );

		// void* data = gShaderPushData.at( &srRenderable );
		// render->CmdPushConstants( sCmd, layout, shaderData->aStages, 0, shaderData->aPushSize, data );
	}

	return true;
}


VertexFormat Shader_GetVertexFormat( Handle sShader )
{
	auto it = gShaderVertFormat.find( sShader );

	if ( it == gShaderVertFormat.end() )
	{
		Log_Error( gLC_ClientGraphics, "Failed to find vertex format for shader!\n" );
		return VertexFormat_None;
	}

	return it->second;
}

