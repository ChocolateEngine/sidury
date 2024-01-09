#include "util.h"
#include "render/irender.h"
#include "graphics_int.h"


static std::unordered_map< std::string_view, Handle >       gShaderNames;
static std::unordered_map< std::string_view, ShaderSets_t > gShaderSets;  // [shader name] = descriptor sets for this shader

static std::unordered_map< Handle, EPipelineBindPoint >     gShaderBindPoint;   // [shader] = bind point
static std::unordered_map< Handle, VertexFormat >           gShaderVertFormat;  // [shader] = vertex format
static std::unordered_map< Handle, FShader_Destroy* >       gShaderDestroy;     // [shader] = shader destroy function
static std::unordered_map< Handle, ShaderData_t >           gShaderData;        // [shader] = assorted shader data

// descriptor set layouts
// extern ShaderBufferArray_t                              gUniformSampler;
// extern ShaderBufferArray_t                              gUniformViewInfo;
// extern ShaderBufferArray_t                              gUniformMaterialBasic3D;
// extern ShaderBufferArray_t                              gUniformLights;

static std::unordered_map< SurfaceDraw_t*, void* >          gShaderPushData;

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
		auto it = gShaderData.find( shader );
		if ( it == gShaderData.end() )
		{
			Log_MsgF( gLC_ClientGraphics, "Found Invalid Shader: %s\n", name.data() );
			continue;
		}

		std::string type;

		if ( it->second.aStages & ShaderStage_Vertex )
			type += " | Vertex";

		if ( it->second.aStages & ShaderStage_Fragment )
			type += " | Fragment";

		if ( it->second.aStages & ShaderStage_Compute )
			type += " | Compute";

		Log_MsgF( gLC_ClientGraphics, "  %s%s\n", name.data(), type.data() );
	}

	Log_Msg( gLC_ClientGraphics, "\n" );
}


// --------------------------------------------------------------------------------------


std::vector< ShaderCreate_t* >& Shader_GetCreateList()
{
	static std::vector< ShaderCreate_t* > create;
	return create;
}


Handle Graphics::GetShader( std::string_view sName )
{
	auto it = gShaderNames.find( sName );
	if ( it != gShaderNames.end() )
		return it->second;

	Log_ErrorF( gLC_ClientGraphics, "Graphics_GetShader: Shader not found: %s\n", sName.data() );
	return InvalidHandle;
}


const char* Graphics::GetShaderName( Handle sShader )
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


bool Graphics_AddPipelineLayouts( std::string_view sName, PipelineLayoutCreate_t& srPipeline, EShaderFlags sFlags )
{
	srPipeline.aLayouts.reserve( srPipeline.aLayouts.capacity() + EShaderSlot_Count );
	srPipeline.aLayouts.push_back( gShaderDescriptorData.aGlobalLayout );

	auto find = gShaderDescriptorData.aPerShaderLayout.find( sName );
	if ( find == gShaderDescriptorData.aPerShaderLayout.end() )
	{
		Log_ErrorF( "No Shader Descriptor Set Layout made for shader \"%s\"\n", sName.data() );
		return false;
	}

	srPipeline.aLayouts.push_back( find->second );
}


bool Shader_CreatePipelineLayout( std::string_view sName, Handle& srLayout, FShader_GetPipelineLayoutCreate fCreate )
{
	if ( fCreate == nullptr )
	{
		Log_Error( gLC_ClientGraphics, "FShader_GetPipelineLayoutCreate is nullptr!\n" );
		return false;
	}

	PipelineLayoutCreate_t pipelineCreate{};
	if ( !Graphics_AddPipelineLayouts( sName, pipelineCreate, EShaderFlags_None ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to add Descriptor Set Layouts for Pipeline\n" );
		return false;
	}

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


bool Shader_CreateComputePipeline( ShaderCreate_t& srCreate, Handle& srPipeline, Handle& srLayout, Handle sRenderPass )
{
	if ( srCreate.apComputeCreate == nullptr )
	{
		Log_Error( gLC_ClientGraphics, "FShader_GetComputePipelineCreate is nullptr!\n" );
		return false;
	}

	ComputePipelineCreate_t pipelineCreate{};
	srCreate.apComputeCreate( pipelineCreate );

	pipelineCreate.apName          = srCreate.apName;
	pipelineCreate.aPipelineLayout = srLayout;

	if ( !render->CreateComputePipeline( srPipeline, pipelineCreate ) )
	{
		Log_ErrorF( "Failed to create Compute Pipeline for Shader \"%s\"\n", srCreate.apName );
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

	if ( !Shader_CreatePipelineLayout( srCreate.apName, shaderData.aLayout, srCreate.apLayoutCreate ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create Pipeline Layout\n" );
		return false;
	}

	if ( srCreate.aBindPoint == EPipelineBindPoint_Graphics )
	{
		if ( !Shader_CreateGraphicsPipeline( srCreate, pipeline, shaderData.aLayout, sRenderPass ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create Graphics Pipeline\n" );
			return false;
		}
	}
	else
	{
		if ( !Shader_CreateComputePipeline( srCreate, pipeline, shaderData.aLayout, sRenderPass ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create Compute Pipeline\n" );
			return false;
		}
	}

	gShaderNames[ srCreate.apName ] = pipeline;
	gShaderBindPoint[ pipeline ]    = srCreate.aBindPoint;
	gShaderVertFormat[ pipeline ]   = srCreate.aVertexFormat;

	shaderData.aFlags               = srCreate.aFlags;
	shaderData.aStages              = srCreate.aStages;
	shaderData.aDynamicState        = srCreate.aDynamicState;

	if ( srCreate.apShaderPush )
		shaderData.apPush = srCreate.apShaderPush;

	if ( srCreate.apShaderPushComp )
		shaderData.apPushComp = srCreate.apShaderPushComp;

	if ( srCreate.apMaterialData )
		shaderData.apMaterialIndex = srCreate.apMaterialData;

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
	for ( ShaderCreate_t* shaderCreate : Shader_GetCreateList() )
	{
		Handle renderPass = gGraphicsData.aRenderPassGraphics;

		if ( shaderCreate->aRenderPass == ERenderPass_Shadow )
			renderPass = gGraphicsData.aRenderPassShadow;

		if ( !Graphics_CreateShader( sRecreate, renderPass, *shaderCreate ) )
		{
			Log_ErrorF( gLC_ClientGraphics, "Failed to create shader \"%s\"\n", shaderCreate->apName );
			return false;
		}
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


bool Shader_ParseRequirements( ShaderRequirmentsList_t& srOutput )
{
	for ( ShaderCreate_t* shaderCreate : Shader_GetCreateList() )
	{
		// create the per shader descriptor sets for this shader
		ShaderSets_t& shaderSets = gShaderSets[ shaderCreate->apName ];

		// future demez here - what the hell does this do and what's the purpose of this?
		if ( shaderCreate->aBindingCount > 0 )
		{
			for ( u32 i = 0; i < shaderCreate->aBindingCount; i++ )
			{
				ShaderRequirement_t require{};
				require.aShader       = shaderCreate->apName;
				require.apBindings    = shaderCreate->apBindings;
				require.aBindingCount = shaderCreate->aBindingCount;

				srOutput.aItems.push_back( require );
			}
		}
		else
		{
			ShaderRequirement_t require{};
			require.aShader       = shaderCreate->apName;
			require.apBindings    = shaderCreate->apBindings;
			require.aBindingCount = shaderCreate->aBindingCount;

			srOutput.aItems.push_back( require );
		}
	}

	return true;
}


// Handle Shader_RegisterDescriptorData( EShaderSlot sSlot, FShader_DescriptorData* sCallback )
// {
// 
// }


bool Shader_Bind( Handle sCmd, u32 sIndex, Handle sShader )
{
	PROF_SCOPE();

	ShaderData_t* shaderData = Shader_GetData( sShader );
	if ( !shaderData )
		return false;

	if ( !render->CmdBindPipeline( sCmd, sShader ) )
		return false;

	// Bind Descriptor Sets (TODO: Keep track of what is currently bound so we don't need to rebind set 0)
	ChVector< Handle > descSets;
	descSets.reserve( 2 );

	// TODO: Should only be done once per frame
	descSets.push_back( gShaderDescriptorData.aGlobalSets.apSets[ sIndex ] );

	// AAAA
	std::string_view shaderName = gGraphics.GetShaderName( sShader );
	descSets.push_back( gShaderDescriptorData.aPerShaderSets[ shaderName ].apSets[ sIndex ] );

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


bool Shader_SetupRenderableDrawData( u32 sRenderableIndex, u32 sViewportIndex, Renderable_t* spModelDraw, ShaderData_t* spShaderData, SurfaceDraw_t& srRenderable )
{
	PROF_SCOPE();

	if ( !spShaderData )
		return false;

	if ( spShaderData->aFlags & EShaderFlags_PushConstant )
	{
		if ( !spShaderData->apPush )
			return false;

		spShaderData->apPush->apSetup( sRenderableIndex, sViewportIndex, spModelDraw, srRenderable );
	}

	return true;
}


bool Shader_PreRenderableDraw( Handle sCmd, u32 sIndex, ShaderData_t* spShaderData, SurfaceDraw_t& srRenderable )
{
	PROF_SCOPE();

	if ( spShaderData->aFlags & EShaderFlags_PushConstant )
	{
		if ( !spShaderData->apPush )
			return false;

		spShaderData->apPush->apPush( sCmd, spShaderData->aLayout, srRenderable );

		// void* data = gShaderPushData.at( &srRenderable );
		// render->CmdPushConstants( sCmd, layout, spShaderData->aStages, 0, spShaderData->aPushSize, data );
	}

	return true;
}


VertexFormat Shader_GetVertexFormat( Handle sShader )
{
	return VertexFormat_All;

#if 0
	PROF_SCOPE();

	auto it = gShaderVertFormat.find( sShader );

	if ( it == gShaderVertFormat.end() )
	{
		Log_Error( gLC_ClientGraphics, "Failed to find vertex format for shader!\n" );
		return VertexFormat_None;
	}

	return it->second;
#endif
}

