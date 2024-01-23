#include "util.h"
#include "render/irender.h"
#include "graphics_int.h"


// TODO: rename this file and shader to "standard" or "generic"?

static bool                gArgPCF                  = false;  // Args_Register( "Enable PCF Shadow Filtering", "-pcf" );

static Handle              gFallbackAO              = InvalidHandle;
static Handle              gFallbackEmissive        = InvalidHandle;

constexpr const char*      gpFallbackAOPath         = "materials/base/white.ktx";
constexpr const char*      gpFallbackEmissivePath   = "materials/base/black.ktx";


constexpr u32              CH_BASIC3D_MAX_MATERIALS = 2048;


static CreateDescBinding_t gBasic3D_Bindings[]      = {
		 { EDescriptorType_StorageBuffer, ShaderStage_Vertex | ShaderStage_Fragment, 0, CH_BASIC3D_MAX_MATERIALS },
};


struct Basic3D_Push
{
	u32 aRenderable = 0;
	u32 aMaterial   = 0;
	u32 aViewport   = 0;

	// Hack for GTX 1050 Ti until I figure this out properly
	// bool aPCF = false;

	// debugging
	u32 aDebugDraw  = 0;
};


struct Basic3D_Material
{
	int   diffuse       = 0;
	int   ao            = 0;
	int   emissive      = 0;

	float aoPower       = 1.f;
	float emissivePower = 1.f;

	bool  alphaTest     = false;
};


#define CH_SHADER_MATERIAL_VAR( type, name, desc, default ) \
	{                                                       \
		#name, desc, default, offsetof( type, name ), sizeof( type::name )        \
	}


// static ShaderMaterialVarDesc gBasic3D_MaterialVars[] = {
// 	{ "diffuse", "Diffuse Texture", "", offsetof( Basic3D_Material, diffuse ), sizeof( Basic3D_Material::diffuse ) },
// 	{ "ao", "Ambient Occlusion Texture", gpFallbackAOPath, offsetof( Basic3D_Material, ao ) },
// 	{ "emissive", "Emission Texture", gpFallbackEmissivePath, offsetof( Basic3D_Material, emissive ) },
// 
// 	{ "aoPower", "Ambient Occlusion Strength", 0.f, offsetof( Basic3D_Material, aoPower ) },
// 	{ "emissivePower", "Emission Strength", 0.f, offsetof( Basic3D_Material, emissivePower ) },
// 
// 	{ "alphaTest", "Alpha Testing", false, offsetof( Basic3D_Material, alphaTest ) },
// };


static ShaderMaterialVarDesc gBasic3D_MaterialVars[] = {
	CH_SHADER_MATERIAL_VAR( Basic3D_Material, diffuse, "Diffuse Texture", "" ),
	CH_SHADER_MATERIAL_VAR( Basic3D_Material, ao, "Ambient Occlusion Texture", gpFallbackAOPath ),
	CH_SHADER_MATERIAL_VAR( Basic3D_Material, emissive, "Emission Texture", gpFallbackEmissivePath ),

	CH_SHADER_MATERIAL_VAR( Basic3D_Material, aoPower, "Ambient Occlusion Strength", 0.f ),
	CH_SHADER_MATERIAL_VAR( Basic3D_Material, emissivePower, "Emission Strength", 0.f ),

	CH_SHADER_MATERIAL_VAR( Basic3D_Material, alphaTest, "Alpha Testing", false ),
};


// KEEP IN THE SAME ORDER AS THE MATERIAL VARS ABOVE
enum : u32
{
	EBasic3D_Diffuse,
	EBasic3D_AmbientOcclusion,
	EBasic3D_Emissive,

	EBasic3D_AmbientOcclusionPower,
	EBasic3D_EmissivePower,

	EBasic3D_AlphaTest,
};


// Material Handle, Buffer
static std::unordered_map< Handle, ChHandle_t >       gMaterialBuffers;
static std::unordered_map< Handle, u32 >              gMaterialBufferIndex;
static std::unordered_map< u32, Basic3D_Push >        gPushData;
static std::unordered_map< Handle, Basic3D_Material > gMaterialData;


static Handle                                         gStagingBuffer = InvalidHandle;


CONVAR( r_basic3d_dbg_mode, 0 );


EShaderFlags Shader_Basic3D_Flags()
{
	// return EShaderFlags_PushConstant | EShaderFlags_SlotPerShader | EShaderFlags_SlotPerObject | EShaderFlags_Lights;
	return EShaderFlags_PushConstant;
}


#if 0
bool Shader_Basic3D_CreateMaterialBuffer( Handle sMat )
{
	// IDEA: since materials shouldn't be updated very often,
	// maybe have the buffer be on the gpu (EBufferMemory_Device)?

	Handle newBuffer = render->CreateBuffer( gGraphics.Mat_GetName( sMat ), sizeof( Basic3D_Material ), EBufferFlags_Storage | EBufferFlags_TransferDst, EBufferMemory_Device );

	if ( newBuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Material Uniform Buffer\n" );
		return false;
	}

	gMaterialBuffers[ sMat ] = newBuffer;

	// TODO: this should probably be moved to the shader system
	// update the descriptor sets
	WriteDescSet_t update{};

	update.aDescSetCount = gShaderDescriptorData.aPerShaderSets[ "basic_3d" ].aCount;
	update.apDescSets    = gShaderDescriptorData.aPerShaderSets[ "basic_3d" ].apSets;

	update.aBindingCount = CH_ARR_SIZE( gBasic3D_Bindings );
	update.apBindings    = ch_calloc_count< WriteDescSetBinding_t >( update.aBindingCount );

	size_t i             = 0;
	for ( size_t binding = 0; binding < update.aBindingCount; binding++ )
	{
		update.apBindings[ i ].aBinding = gBasic3D_Bindings[ binding ].aBinding;
		update.apBindings[ i ].aType    = gBasic3D_Bindings[ binding ].aType;
		update.apBindings[ i ].aCount   = gBasic3D_Bindings[ binding ].aCount;
		i++;
	}

	update.apBindings[ 0 ].apData = ch_calloc_count< ChHandle_t >( CH_BASIC3D_MAX_MATERIALS );
	update.apBindings[ 0 ].aCount = gMaterialBuffers.size();

	// GOD
	i                             = 0;
	for ( const auto& [ mat, buffer ] : gMaterialBuffers )
	{
		gMaterialBufferIndex[ mat ]          = i;
		update.apBindings[ 0 ].apData[ i++ ] = buffer;
	}

	// update.aImages = gViewportBuffers;
	render->UpdateDescSets( &update, 1 );

	free( update.apBindings[ 0 ].apData );
	free( update.apBindings );

	return true;
}


// TODO: this doesn't handle shaders being changed on materials, or materials being freed
u32 Shader_Basic3D_UpdateMaterialData( ChHandle_t sMat )
{
	PROF_SCOPE();

	if ( sMat == 0 )
		return 0;

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
			return 0;

		// create new material data
		mat = &gMaterialData[ sMat ];
	}

	mat->diffuse       = gGraphics.Mat_GetTextureIndex( sMat, "diffuse" );
	mat->ao            = gGraphics.Mat_GetTextureIndex( sMat, "ao", gFallbackAO );
	mat->emissive      = gGraphics.Mat_GetTextureIndex( sMat, "emissive", gFallbackEmissive );

	mat->aoPower       = gGraphics.Mat_GetFloat( sMat, "aoPower", 0.f );
	mat->emissivePower = gGraphics.Mat_GetFloat( sMat, "emissivePower", 0.f );

	mat->alphaTest    = gGraphics.Mat_GetBool( sMat, "alphaTest" );

	if ( !gStagingBuffer )
		gStagingBuffer = render->CreateBuffer( "Staging Buffer", sizeof( Basic3D_Material ), EBufferFlags_Uniform | EBufferFlags_TransferSrc, EBufferMemory_Host );

	if ( gStagingBuffer == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Staging Material Uniform Buffer\n" );
		return 0;
	}

	render->BufferWrite( gStagingBuffer, sizeof( Basic3D_Material ), mat );

	// write new material data to the buffer
	ChHandle_t& buffer = gMaterialBuffers[ sMat ];

	BufferRegionCopy_t copy;
	copy.aSrcOffset = 0;
	copy.aDstOffset = 0;
	copy.aSize      = sizeof( Basic3D_Material );

	// render->MemWriteBuffer( buffer, sizeof( Basic3D_Material ), mat );
	render->BufferCopy( gStagingBuffer, buffer, &copy, 1 );

	return gMaterialBufferIndex[ sMat ];
}
#endif


static bool Shader_Basic3D_Init()
{
	TextureCreateData_t createData{};
	createData.aFilter = EImageFilter_Nearest;
	createData.aUsage  = EImageUsage_Sampled;

	// create fallback textures
	//gGraphics.LoadTexture( gFallbackAO, gpFallbackAOPath, createData );
	//gGraphics.LoadTexture( gFallbackEmissive, gpFallbackEmissivePath, createData );

	return true;
}


static void Shader_Basic3D_Destroy()
{
	//gGraphics.FreeTexture( gFallbackAO );
	//gGraphics.FreeTexture( gFallbackEmissive );
	
	if ( gStagingBuffer )
		render->DestroyBuffer( gStagingBuffer );

	for ( auto& [ buffer, material ] : gMaterialData )
		render->DestroyBuffer( buffer );

	gStagingBuffer = InvalidHandle;
	gMaterialData.clear();

	gGraphics.SetAllMaterialsDirty();
}


static void Shader_Basic3D_GetPipelineLayoutCreate( PipelineLayoutCreate_t& srPipeline )
{
	// NOTE: maybe create the descriptor set layout for this shader here, then add it? idk

	srPipeline.aPushConstants.push_back( { ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Basic3D_Push ) } );
}


static void Shader_Basic3D_GetGraphicsPipelineCreate( GraphicsPipelineCreate_t& srGraphics )
{
	srGraphics.aShaderModules.push_back( { ShaderStage_Vertex, "shaders/basic3d.vert.spv", "main" } );
	srGraphics.aShaderModules.push_back( { ShaderStage_Fragment, "shaders/basic3d.frag.spv", "main" } );

	srGraphics.aColorBlendAttachments.push_back( { false } ); 

	srGraphics.aPrimTopology   = EPrimTopology_Tri;
	srGraphics.aDynamicState   = EDynamicState_Viewport | EDynamicState_Scissor;
	srGraphics.aCullMode       = ECullMode_Back;
}


#if 0
static u32 Shader_Basic3D_GetMaterialIndex( u32 sRenderableIndex, Renderable_t* spModelDraw, SurfaceDraw_t& srDrawInfo )
{
	Handle mat = gGraphics.Model_GetMaterial( spModelDraw->aModel, srDrawInfo.aSurface );

	if ( !mat )
		return 0;

	auto it = gMaterialBufferIndex.find( mat );
	if ( it == gMaterialBufferIndex.end() )
	{
		return Shader_Basic3D_UpdateMaterialData( mat );
	}

	return it->second;
}


void Basic3D_UpdateMaterial( ChHandle_t sMat, void* spData )
{
	Basic3D_Material* mat = static_cast< Basic3D_Material* >( spData );

	// TODO: do i stick with this? or have the shader system handle it, to automatically handle fallbacks?
	mat->diffuse          = gGraphics.Mat_GetTextureIndex( sMat, "diffuse" );
	mat->ao               = gGraphics.Mat_GetTextureIndex( sMat, "ao", gFallbackAO );
	mat->emissive         = gGraphics.Mat_GetTextureIndex( sMat, "emissive", gFallbackEmissive );

	mat->aoPower          = gGraphics.Mat_GetFloat( sMat, "aoPower", 0.f );
	mat->emissivePower    = gGraphics.Mat_GetFloat( sMat, "emissivePower", 0.f );

	mat->alphaTest       = gGraphics.Mat_GetBool( sMat, "alphaTest" );
}
#endif


// TODO: Move this push constant management into the shader system
// we should only need the SetupPushData function
static void Shader_Basic3D_ResetPushData()
{
	gPushData.clear();
}


static void Shader_Basic3D_SetupPushData( u32 sSurfaceIndex, u32 sViewportIndex, Renderable_t* spModelDraw, SurfaceDraw_t& srDrawInfo, ShaderMaterialData* spMaterialData )
{
	PROF_SCOPE();

	Basic3D_Push& push = gPushData[ srDrawInfo.aShaderSlot ];
	// push.aModelMatrix  = spModelDraw->aModelMatrix;
	push.aRenderable   = CH_GET_HANDLE_INDEX( srDrawInfo.aRenderable );
	push.aMaterial     = spMaterialData->matIndex;
	push.aViewport     = sViewportIndex;

	push.aDebugDraw    = r_basic3d_dbg_mode;
	// push.aPCF          = gArgPCF;
}


//static void Shader_Basic3D_SetupPushData2( u32 sSurfaceIndex, u32 sViewportIndex, Renderable_t* spModelDraw, SurfaceDraw_t& srDrawInfo, void* spData )
//{
//	PROF_SCOPE();
//
//	Basic3D_Push* push = static_cast< Basic3D_Push* >( spData );
//
//	// push->aModelMatrix  = spModelDraw->aModelMatrix;
//	push->aRenderable  = CH_GET_HANDLE_INDEX( srDrawInfo.aRenderable );
//	push->aMaterial    = Shader_Basic3D_GetMaterialIndex( sSurfaceIndex, spModelDraw, srDrawInfo );
//	push->aViewport    = sViewportIndex;
//
//	push->aDebugDraw   = r_basic3d_dbg_mode;
//	// push->aPCF          = gArgPCF;
//}


static void Shader_Basic3D_PushConstants( Handle cmd, Handle sLayout, SurfaceDraw_t& srDrawInfo )
{
	PROF_SCOPE();

	Basic3D_Push& push = gPushData.at( srDrawInfo.aShaderSlot );
	render->CmdPushConstants( cmd, sLayout, ShaderStage_Vertex | ShaderStage_Fragment, 0, sizeof( Basic3D_Push ), &push );
}


static IShaderPush gShaderPush_Basic3D = {
	.apReset = Shader_Basic3D_ResetPushData,
	.apSetup = Shader_Basic3D_SetupPushData,
	.apPush  = Shader_Basic3D_PushConstants,
};


//static IShaderMaterial gShaderMaterial_Basic3D = {
//	.apAdd    = Basic3D_AddMaterial,
//	.apRemove = Basic3D_RemoveMaterial,
//	.apUpdate = Basic3D_UpdateMaterial,
//};


// TODO: some binding list saying which ones are materials? or only have one option for saying which binding is a material
ShaderCreate_t gShaderCreate_Basic3D = {
	.apName               = "basic_3d",
	.aStages              = ShaderStage_Vertex | ShaderStage_Fragment,
	.aBindPoint           = EPipelineBindPoint_Graphics,
	.aFlags               = Shader_Basic3D_Flags(),
	.aDynamicState        = EDynamicState_Viewport | EDynamicState_Scissor,
	.aVertexFormat        = VertexFormat_Position | VertexFormat_Normal | VertexFormat_TexCoord,

	.apInit               = Shader_Basic3D_Init,
	.apDestroy            = Shader_Basic3D_Destroy,
	.apLayoutCreate       = Shader_Basic3D_GetPipelineLayoutCreate,
	.apGraphicsCreate     = Shader_Basic3D_GetGraphicsPipelineCreate,

	// .aPushSize         = sizeof( Basic3D_Push ),
	// .apPushSetup       = Shader_Basic3D_SetupPushData2,

	.apShaderPush         = &gShaderPush_Basic3D,

	// .apMaterialData     = Shader_Basic3D_GetMaterialIndex,

	// .apUpdateMaterial   = Basic3D_UpdateMaterial,
	.aMaterialSize        = sizeof( Basic3D_Material ),
	.aUseMaterialBuffer   = true,
	.aMaterialBufferIndex = 0,

	.apBindings           = gBasic3D_Bindings,
	.aBindingCount        = CH_ARR_SIZE( gBasic3D_Bindings ),

	.apMaterialVars       = gBasic3D_MaterialVars,
	.aMaterialVarCount    = CH_ARR_SIZE( gBasic3D_MaterialVars ),
};


CH_REGISTER_SHADER( gShaderCreate_Basic3D );

