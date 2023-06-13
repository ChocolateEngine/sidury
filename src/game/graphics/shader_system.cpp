#include "util.h"
#include "render/irender.h"
#include "graphics.h"


static std::unordered_map< std::string_view, Handle >   gShaderNames;

static std::unordered_map< Handle, EPipelineBindPoint > gShaderBindPoint;  // [shader] = bind point
static std::unordered_map< Handle, Handle >             gShaderMaterials;  // [shader] = uniform layout
static std::unordered_map< Handle, VertexFormat >       gShaderVertFormat; // [shader] = vertex format
static std::unordered_map< Handle, FShader_Destroy* >   gShaderDestroy;    // [shader] = shader destroy function
static std::unordered_map< Handle, ShaderData_t >       gShaderData;       // [shader] = assorted shader data

extern Handle                                           gRenderPassGraphics;
extern Handle                                           gRenderPassShadow;

// descriptor set layouts
extern UniformBufferArray_t                             gUniformSampler;
extern UniformBufferArray_t                             gUniformViewInfo;
extern UniformBufferArray_t                             gUniformMaterialBasic3D;
extern UniformBufferArray_t                             gUniformLightInfo;
extern UniformBufferArray_t                             gUniformLights[ ELightType_Count ];

static std::unordered_map< SurfaceDraw_t*, void* > gShaderPushData;

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


void Graphics_AddPipelineLayouts( PipelineLayoutCreate_t& srPipeline, EShaderFlags sFlags )
{
	if ( sFlags & EShaderFlags_Sampler )
		srPipeline.aLayouts.push_back( gUniformSampler.aLayout );

	if ( sFlags & EShaderFlags_ViewInfo )
		srPipeline.aLayouts.push_back( gUniformViewInfo.aLayout );

	if ( sFlags & EShaderFlags_Lights )
	{
		srPipeline.aLayouts.push_back( gUniformLightInfo.aLayout );

		for ( int i = 0; i < ELightType_Count; i++ )
			srPipeline.aLayouts.push_back( gUniformLights[ i ].aLayout );
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
	Handle pipeline = InvalidHandle;

	auto   nameFind = gShaderNames.find( srCreate.apName );
	if ( nameFind != gShaderNames.end() )
		pipeline = nameFind->second;

	ShaderData_t shaderData{};

	if ( pipeline )
	{
		auto it = gShaderData.find( pipeline );
		if ( it != gShaderData.end() )
		{
			shaderData = it->second;
		}
	}

	if ( !Shader_CreatePipelineLayout( shaderData.aLayout, srCreate.apLayoutCreate ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Pipeline Layout\n" );
		return false;
	}

	if ( !Shader_CreateGraphicsPipeline( srCreate, pipeline, shaderData.aLayout, sRenderPass ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Graphics Pipeline\n" );
		return false;
	}

	gShaderNames[ srCreate.apName ] = pipeline;
	gShaderBindPoint[ pipeline ]    = srCreate.aBindPoint;
	gShaderVertFormat[ pipeline ]   = srCreate.aVertexFormat;

	shaderData.aFlags               = srCreate.aFlags;
	shaderData.aStages              = srCreate.aStages;
	shaderData.aDynamicState        = srCreate.aDynamicState;

	if ( srCreate.apShaderPush )
		shaderData.apPush = srCreate.apShaderPush;

	gShaderData[ pipeline ] = shaderData;

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
	auto it = gShaderData.find( sShader );
	if ( it != gShaderData.end() )
	{
		Log_WarnF( gLC_ClientGraphics, "Shader_Destroy: Failed to find shader: %zd\n", sShader );
		return;
	}

	ShaderData_t& shaderData = it->second;

	render->DestroyPipelineLayout( shaderData.aLayout );
	render->DestroyPipeline( sShader );

	for ( const auto& [ name, shader ] : gShaderNames )
	{
		if ( shader == sShader )
		{
			gShaderNames.erase( name );
			break;
		}
	}

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


EPipelineBindPoint Shader_GetPipelineBindPoint( Handle sShader )
{
	PROF_SCOPE();

	auto it = gShaderBindPoint.find( sShader );
	if ( it != gShaderBindPoint.end() )
		return it->second;
	
	Log_Error( gLC_ClientGraphics, "Unable to find Shader Pipeline Bind Point!\n" );
	return EPipelineBindPoint_Graphics;
}


ShaderData_t* Shader_GetData( Handle sShader )
{
	PROF_SCOPE();

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
	PROF_SCOPE();

	ShaderData_t* shaderData = Shader_GetData( sShader );
	if ( !shaderData )
		return false;

	if ( !render->CmdBindPipeline( sCmd, sShader ) )
		return false;

	ChVector< Handle > descSets;
	descSets.reserve( 8 );

	// TODO: bind a single uniform variable so the shader can do a bounds check on this
	if ( shaderData->aFlags & EShaderFlags_Sampler )
		descSets.push_back( gUniformSampler.aSets[ sIndex ] );

	if ( shaderData->aFlags & EShaderFlags_ViewInfo )
		descSets.push_back( gUniformViewInfo.aSets[ sIndex ] );

	if ( shaderData->aFlags & EShaderFlags_Lights )
	{
		for ( const auto& set : gUniformLightInfo.aSets )
			descSets.push_back( set );

		for ( int i = 0; i < ELightType_Count; i++ )
		{
			for ( const auto& set : gUniformLights[ i ].aSets )
				descSets.push_back( set );
		}
	}

	if ( shaderData->aFlags & EShaderFlags_MaterialUniform )
	{
		Handle* mats = Shader_GetMaterialUniform( sShader );
		if ( mats == nullptr )
			return false;

		descSets.push_back( mats[ sIndex ] );
	}

	if ( descSets.size() )
	{
		EPipelineBindPoint bindPoint = Shader_GetPipelineBindPoint( sShader );

		render->CmdBindDescriptorSets( sCmd, sIndex, bindPoint, shaderData->aLayout, descSets.data(), descSets.size() );
	}

	return true;
}


void Shader_ResetPushData()
{
	PROF_SCOPE();

	// this is where data oriented would be better, but the way i did it was slow
	for ( auto& [ shader, data ] : gShaderData )
	{
		if ( data.apPush )
			data.apPush->apReset();
	}
	
	// for ( auto& [ renderable, data ] : gShaderPushData )
	// {
	// 	delete data;
	// }
	// 
	// gShaderPushData.clear();
}


bool Shader_SetupRenderableDrawData( Renderable_t* spModelDraw, ShaderData_t* spShaderData, SurfaceDraw_t& srRenderable )
{
	PROF_SCOPE();

	if ( !spShaderData )
		return false;

	if ( spShaderData->aFlags & EShaderFlags_PushConstant )
	{
		if ( !spShaderData->apPush )
			return false;

		spShaderData->apPush->apSetup( spModelDraw, srRenderable );
	}

	return true;
}


bool Shader_PreRenderableDraw( Handle sCmd, u32 sIndex, Handle sShader, SurfaceDraw_t& srRenderable )
{
	PROF_SCOPE();

	ShaderData_t* shaderData = Shader_GetData( sShader );
	if ( !shaderData )
		return false;

	if ( shaderData->aFlags & EShaderFlags_PushConstant )
	{
		if ( !shaderData->apPush )
			return false;

		shaderData->apPush->apPush( sCmd, shaderData->aLayout, srRenderable );

		// void* data = gShaderPushData.at( &srRenderable );
		// render->CmdPushConstants( sCmd, layout, shaderData->aStages, 0, shaderData->aPushSize, data );
	}

	return true;
}


VertexFormat Shader_GetVertexFormat( Handle sShader )
{
	PROF_SCOPE();

	auto it = gShaderVertFormat.find( sShader );

	if ( it == gShaderVertFormat.end() )
	{
		Log_Error( gLC_ClientGraphics, "Failed to find vertex format for shader!\n" );
		return VertexFormat_None;
	}

	return it->second;
}

