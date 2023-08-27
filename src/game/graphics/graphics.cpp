#include "core/core.h"
#include "igui.h"
#include "render/irender.h"
#include "graphics.h"
#include "graphics_int.h"
#include "lighting.h"
#include "debug_draw.h"
#include "mesh_builder.h"
#include "imgui/imgui.h"
#include "../main.h"

#include <forward_list>
#include <stack>
#include <set>
#include <unordered_set>


LOG_REGISTER_CHANNEL_EX( gLC_ClientGraphics, "ClientGraphics", LogColor::Green );

// --------------------------------------------------------------------------------------
// Interfaces

extern IGuiSystem*     gui;
extern IRender*        render;

// --------------------------------------------------------------------------------------

void                   Graphics_LoadObj( const std::string& srBasePath, const std::string& srPath, Model* spModel );
void                   Graphics_LoadGltf( const std::string& srBasePath, const std::string& srPath, const std::string& srExt, Model* spModel );
void                   Graphics_LoadGltfNew( const std::string& srBasePath, const std::string& srPath, const std::string& srExt, Model* spModel );

void                   Graphics_LoadSceneObj( const std::string& srBasePath, const std::string& srPath, Scene_t* spScene );

Handle                 CreateModelBuffer( const char* spName, void* spData, size_t sBufferSize, EBufferFlags sUsage );

// --------------------------------------------------------------------------------------
// General Rendering
// TODO: group these globals together by data commonly used together

GraphicsData_t         gGraphicsData;
ShaderDescriptorData_t gShaderDescriptorData;

// TODO: rethink this so you can have draw ordering
// use the RenderLayer idea you had

// struct RenderList_t
// {
// 	std::forward_list< SurfaceDraw_t* > aSurfaces;
// };


// static std::unordered_map<
//   Handle,
//   ChVector< SurfaceDraw_t > >
// 												 gModelDrawList;


constexpr u32          MAX_MATERIALS_BASIC3D = 500;


// --------------------------------------------------------------------------------------
// Other

const char*            gShaderCoreArrayStr[]       = {
					 "Viewport",
					 // "Renderable",

					 "LightWorld",
					 "LightPoint",
					 "LightCone",
					 "LightCapsule",
};


static_assert( CH_ARR_SIZE( gShaderCoreArrayStr ) == EShaderCoreArray_Count );


CONCMD( r_reload_textures )
{
	render->ReloadTextures();
	// Graphics_SetAllMaterialsDirty();
}


ModelBBox_t Graphics_CalcModelBBox( Handle sModel )
{
	PROF_SCOPE();

	ModelBBox_t bbox{};
	bbox.aMax = { INT_MIN, INT_MIN, INT_MIN };
	bbox.aMin = { INT_MAX, INT_MAX, INT_MAX };

	Model* model = Graphics_GetModelData( sModel );
	if ( !model )
		return bbox;

	auto*      vertData = model->apVertexData;
	glm::vec3* data     = nullptr;

	for ( auto& attrib : vertData->aData )
	{
		if ( attrib.aAttrib == VertexAttribute_Position )
		{
			data = (glm::vec3*)attrib.apData;
			break;
		}
	}

	if ( data == nullptr )
	{
		Log_Error( "Position Vertex Data not found?\n" );
		gGraphicsData.aModelBBox[ sModel ] = bbox;
		return bbox;
	}

	auto UpdateBBox = [ & ]( const glm::vec3& vertex )
	{
		bbox.aMin.x = glm::min( bbox.aMin.x, vertex.x );
		bbox.aMin.y = glm::min( bbox.aMin.y, vertex.y );
		bbox.aMin.z = glm::min( bbox.aMin.z, vertex.z );

		bbox.aMax.x = glm::max( bbox.aMax.x, vertex.x );
		bbox.aMax.y = glm::max( bbox.aMax.y, vertex.y );
		bbox.aMax.z = glm::max( bbox.aMax.z, vertex.z );
	};

	for ( Mesh& mesh : model->aMeshes )
	{
		if ( vertData->aIndices.size() )
		{
			for ( u32 i = 0; i < mesh.aIndexCount; i++ )
				UpdateBBox( data[ vertData->aIndices[ mesh.aIndexOffset + i ] ] );
		}
		else
		{
			for ( u32 i = 0; i < mesh.aVertexCount; i++ )
				UpdateBBox( data[ mesh.aVertexOffset + i ] );
		}
	}

	gGraphicsData.aModelBBox[ sModel ] = bbox;

	return bbox;
}


bool Graphics_GetModelBBox( Handle sModel, ModelBBox_t& srBBox )
{
	auto it = gGraphicsData.aModelBBox.find( sModel );
	if ( it == gGraphicsData.aModelBBox.end() )
		return false;

	srBBox = it->second;
	return true;
}


Handle Graphics_LoadModel( const std::string& srPath )
{
	PROF_SCOPE();

	// Have we loaded this model already?
	auto it = gGraphicsData.aModelPaths.find( srPath );

	if ( it != gGraphicsData.aModelPaths.end() )
	{
		// We did load this already, use that model instead
		// Increment the ref count
		Model* model = nullptr;
		if ( !gGraphicsData.aModels.Get( it->second, &model ) )
		{
			Log_Error( gLC_ClientGraphics, "Graphics_LoadModel: Model is nullptr\n" );
			return InvalidHandle;
		}

		model->AddRef();
		return it->second;
	}

	// We have not, so try to load this model in
	std::string fullPath = FileSys_FindFile( srPath );

	if ( fullPath.empty() )
	{
		Log_DevF( gLC_ClientGraphics, 1, "LoadModel: Failed to Find Model: %s\n", srPath.c_str() );
		return InvalidHandle;
	}

	std::string fileExt = FileSys_GetFileExt( srPath );

	Model* model = nullptr;
	Handle handle = InvalidHandle;

	// TODO: try to do file header checking
	if ( fileExt == "obj" )
	{
		handle = gGraphicsData.aModels.Create( &model );

		if ( handle == InvalidHandle )
		{
			Log_ErrorF( gLC_ClientGraphics, "LoadModel: Failed to Allocate Model: %s\n", srPath.c_str() );
			return InvalidHandle;
		}

		Graphics_LoadObj( srPath, fullPath, model );
	}
	else if ( fileExt == "glb" || fileExt == "gltf" )
	{
		handle = gGraphicsData.aModels.Create( &model );

		if ( handle == InvalidHandle )
		{
			Log_ErrorF( gLC_ClientGraphics, "LoadModel: Failed to Allocate Model: %s\n", srPath.c_str() );
			return InvalidHandle;
		}

		Graphics_LoadGltfNew( srPath, fullPath, fileExt, model );
	}
	else
	{
		Log_DevF( gLC_ClientGraphics, 1, "Unknown Model File Extension: %s\n", fileExt.c_str() );
		return InvalidHandle;
	}

	//sModel->aRadius = glm::distance( mesh->aMinSize, mesh->aMaxSize ) / 2.0f;

	// TODO: load in an error model here instead
	if ( model->aMeshes.empty() )
	{
		gGraphicsData.aModels.Remove( handle );
		return InvalidHandle;
	}

	// calculate a bounding box
	Graphics_CalcModelBBox( handle );

	gGraphicsData.aModelPaths[ srPath ] = handle;

	model->AddRef();

	return handle;
}


Handle Graphics_CreateModel( Model** spModel )
{
	Handle handle = gGraphicsData.aModels.Create( spModel );

	if ( handle != InvalidHandle )
		( *spModel )->AddRef();

	return handle;
}


void Graphics_FreeModel( Handle shModel )
{
	PROF_SCOPE();

	if ( shModel == InvalidHandle )
		return;

	// HACK HACK PERF: we have to wait for queues to finish, so we could just free this model later
	// maybe right before the next draw?
	render->WaitForQueues();

	// use smart pointer for apVertexData and apBuffers?
	// though with the resource system, you can't do that, darn
	// you need to use placement new there

	// prototyping idea
	// 
	// Resource_GetData( gModels, &model );
	// Resource_IncrementRefCount( gModels, &model );
	// 

	Model* model = nullptr;
	if ( !gGraphicsData.aModels.Get( shModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_FreeModel: Model is nullptr\n" );
		return;
	}

	//if ( model->apVertexData )
	//{
	//	model->apVertexData->Release();
	//}
	//
	//if ( model->apBuffers )
	//{
	//	model->apBuffers->Release();
	// 	for ( auto& buf : model->apBuffers->aVertex )
	// 		render->DestroyBuffer( buf );
	// 
	// 	if ( model->apBuffers->aIndex )
	// 		render->DestroyBuffer( model->apBuffers->aIndex );
	// 
	// 	model->apBuffers->aVertex.clear();
	// 	model->apBuffers->aIndex = InvalidHandle;
	//}

	model->aRefCount--;
	if ( model->aRefCount == 0 )
	{
		// TODO: QUEUE THIS MODEL FOR DELETION, DON'T DELETE THIS NOW

		// Free Materials attached to this model
		for ( Mesh& mesh : model->aMeshes )
		{
			if ( mesh.aMaterial )
				Graphics_FreeMaterial( mesh.aMaterial );
		}

		// Free Vertex Data
		if ( model->apVertexData )
		{
			delete model->apVertexData;
		}

		// Free Vertex and Index Buffers
		if ( model->apBuffers )
		{
			delete model->apBuffers;
		}
		
		// If this model was loaded from disk, remove the stored model path
		for ( auto& [ path, modelHandle ] : gGraphicsData.aModelPaths )
		{
			if ( modelHandle == shModel )
			{
				gGraphicsData.aModelPaths.erase( path );
				break;
			}
		}

		gGraphicsData.aModels.Remove( shModel );
		gGraphicsData.aModelBBox.erase( shModel );
	}
}


Model* Graphics_GetModelData( Handle shModel )
{
	PROF_SCOPE();

	Model* model = nullptr;
	if ( !gGraphicsData.aModels.Get( shModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_GetModelData: Model is nullptr\n" );
		return nullptr;
	}

	return model;
}


std::string_view Graphics_GetModelPath( Handle sModel )
{
	for ( auto& [ path, modelHandle ] : gGraphicsData.aModelPaths )
	{
		if ( modelHandle == sModel )
		{
			return path;
		}
	}

	return "";
}


void Model_SetMaterial( Handle shModel, size_t sSurface, Handle shMat )
{
	Model* model = nullptr;
	if ( !gGraphicsData.aModels.Get( shModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Model_SetMaterial: Model is nullptr\n" );
		return;
	}

	if ( sSurface > model->aMeshes.size() )
	{
		Log_ErrorF( gLC_ClientGraphics, "Model_SetMaterial: surface is out of range: %zu (Surface Count: %zu)\n", sSurface, model->aMeshes.size() );
		return;
	}

	model->aMeshes[ sSurface ].aMaterial = shMat;
}


Handle Model_GetMaterial( Handle shModel, size_t sSurface )
{
	Model* model = nullptr;
	if ( !gGraphicsData.aModels.Get( shModel, &model ) )
	{
		Log_Error( gLC_ClientGraphics, "Model_GetMaterial: Model is nullptr\n" );
		return InvalidHandle;
	}

	if ( sSurface >= model->aMeshes.size() )
	{
		Log_ErrorF( gLC_ClientGraphics, "Model_GetMaterial: surface is out of range: %zu (Surface Count: %zu)\n", sSurface, model->aMeshes.size() );
		return InvalidHandle;
	}

	return model->aMeshes[ sSurface ].aMaterial;
}


// ---------------------------------------------------------------------------------------
// Scenes


Handle Graphics_LoadScene( const std::string& srPath )
{
	// Have we loaded this scene already?
	auto it = gGraphicsData.aScenePaths.find( srPath );

	if ( it != gGraphicsData.aScenePaths.end() )
	{
		// We have, use that scene instead
		return it->second;
	}

	// We have not, so try to load this model in
	std::string fullPath = FileSys_FindFile( srPath );

	if ( fullPath.empty() )
	{
		Log_ErrorF( gLC_ClientGraphics, "LoadScene: Failed to Find Scene: %s\n", srPath.c_str() );
		return InvalidHandle;
	}

	std::string fileExt = FileSys_GetFileExt( srPath );

	// Scene_t*    scene   = new Scene_t;
	Scene_t*    scene   = nullptr;
	Handle      handle  = InvalidHandle;

	// TODO: try to do file header checking
	if ( fileExt == "obj" )
	{
		handle = gGraphicsData.aScenes.Create( &scene );
		
		if ( handle == InvalidHandle )
		{
			Log_ErrorF( gLC_ClientGraphics, "LoadScene: Failed to Allocate Scene: %s\n", srPath.c_str() );
			return InvalidHandle;
		}

		memset( &scene->aModels, 0, sizeof( scene->aModels ) );
		Graphics_LoadSceneObj( srPath, fullPath, scene );
	}
	// else if ( fileExt == "glb" || fileExt == "gltf" )
	// {
	// 	// handle = gScenes.Add( scene );
	// 	// Graphics_LoadGltf( srPath, fullPath, fileExt, model );
	// }
	else
	{
		Log_DevF( gLC_ClientGraphics, 1, "Unknown Model File Extension: %s\n", fileExt.c_str() );
		return InvalidHandle;
	}

	//sModel->aRadius = glm::distance( mesh->aMinSize, mesh->aMaxSize ) / 2.0f;

	// TODO: load in an error model here instead
	if ( scene->aModels.empty() )
	{
		gGraphicsData.aScenes.Remove( handle );
		// delete scene;
		return InvalidHandle;
	}

	// Calculate Bounding Boxes for Models
	for ( const auto& modelHandle : scene->aModels )
	{
		Graphics_CalcModelBBox( modelHandle );
	}

	gGraphicsData.aScenePaths[ srPath ] = handle;
	return handle;
}


void Graphics_FreeScene( Handle sScene )
{
	// HACK HACK PERF: we have to wait for queues to finish, so we could just free this model later
	// maybe right before the next draw?
	render->WaitForQueues();

	Scene_t* scene = nullptr;
	if ( !gGraphicsData.aScenes.Get( sScene, &scene ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_FreeScene: Failed to find Scene\n" );
		return;
	}

	for ( auto& model : scene->aModels )
	{
		Graphics_FreeModel( model );
	}
	
	for ( auto& [ path, sceneHandle ] : gGraphicsData.aScenePaths )
	{
		if ( sceneHandle == sScene )
		{
			gGraphicsData.aScenePaths.erase( path );
			break;
		}
	}

	// delete scene;
	gGraphicsData.aScenes.Remove( sScene );
}


SceneDraw_t* Graphics_AddSceneDraw( Handle sScene )
{
	if ( !sScene )
		return nullptr;

	Scene_t* scene = nullptr;
	if ( !gGraphicsData.aScenes.Get( sScene, &scene ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_DrawScene: Failed to find Scene\n" );
		return nullptr;
	}

	SceneDraw_t* sceneDraw = new SceneDraw_t;
	sceneDraw->aScene      = sScene;
	sceneDraw->aDraw.resize( scene->aModels.size() );

	for ( uint32_t i = 0; i < scene->aModels.size(); i++ )
	{
		sceneDraw->aDraw[ i ] = Graphics_CreateRenderable( scene->aModels[ i ] );
	}

	return sceneDraw;
}


void Graphics_RemoveSceneDraw( SceneDraw_t* spScene )
{
	if ( !spScene )
		return;

	// Scene_t* scene = nullptr;
	// if ( !gScenes.Get( spScene->aScene, &scene ) )
	// {
	// 	Log_Error( gLC_ClientGraphics, "Graphics_DrawScene: Failed to find Scene\n" );
	// 	return;
	// }

	for ( auto& modelDraw : spScene->aDraw )
	{
		Graphics_FreeRenderable( modelDraw );
	}

	delete spScene;
}


size_t Graphics_GetSceneModelCount( Handle sScene )
{
	Scene_t* scene = nullptr;
	if ( !gGraphicsData.aScenes.Get( sScene, &scene ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_GetSceneModelCount: Failed to find Scene\n" );
		return 0;
	}

	return scene->aModels.size();
}


Handle Graphics_GetSceneModel( Handle sScene, size_t sIndex )
{
	Scene_t* scene = nullptr;
	if ( !gGraphicsData.aScenes.Get( sScene, &scene ) )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_GetSceneModel: Failed to find Scene\n" );
		return 0;
	}

	if ( sIndex >= scene->aModels.size() )
	{
		Log_Error( gLC_ClientGraphics, "Graphics_GetSceneModel: Index out of range\n" );
		return InvalidHandle;
	}

	return scene->aModels[ sIndex ];
}


// ---------------------------------------------------------------------------------------


ChHandle_t Graphics_LoadTexture( ChHandle_t& srHandle, const std::string& srTexturePath, const TextureCreateData_t& srCreateData )
{
	//gGraphicsData.aTexturesDirty = true;
	return render->LoadTexture( srHandle, srTexturePath, srCreateData );
}


ChHandle_t Graphics_CreateTexture( const TextureCreateInfo_t& srTextureCreateInfo, const TextureCreateData_t& srCreateData )
{
	//gGraphicsData.aTexturesDirty = true;
	return render->CreateTexture( srTextureCreateInfo, srCreateData );
}


void Graphics_FreeTexture( ChHandle_t shTexture )
{
	//gGraphicsData.aTexturesDirty = true;
	render->FreeTexture( shTexture );
}


void Graphics_UpdateTextureIndices()
{
}


int Graphics_GetTextureIndex( ChHandle_t shTexture )
{
	PROF_SCOPE();

	if ( shTexture == CH_INVALID_HANDLE )
		return 0;

	// if ( gGraphicsData.aTexturesDirty )
	// 	Graphics_UpdateTextureIndices();
	// 
	// gGraphicsData.aTexturesDirty = false;

	return render->GetTextureIndex( shTexture );
}


void Graphics_CalcTextureIndices()
{
}


// ---------------------------------------------------------------------------------------


void Graphics_DestroyRenderPasses()
{
	Graphics_DestroyShadowRenderPass();
}


bool Graphics_CreateRenderPasses()
{
	// Shadow Map Render Pass
	if ( !Graphics_CreateShadowRenderPass() )
	{
		Log_Error( "Failed to create Shadow Map Render Pass\n" );
		return false;
	}

	return true;
}


#if 0
bool Graphics_CreateVariableDescLayout( CreateDescLayout_t& srCreate, Handle& srLayout, Handle* spSets, u32 sSetCount, const char* spSetName, int sCount )
{
	srLayout = render->CreateDescLayout( srCreate );

	if ( srLayout == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create variable descriptor layout\n" );
		return false;
	}

	AllocVariableDescLayout_t allocLayout{};
	allocLayout.apName    = spSetName;
	allocLayout.aLayout   = srLayout;
	allocLayout.aCount    = sCount;
	allocLayout.aSetCount = sSetCount;

	if ( !render->AllocateVariableDescLayout( allocLayout, spSets ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to allocate variable descriptor layout\n" );
		return false;
	}

	return true;
}
#endif


bool Graphics_CreateDescLayout( CreateDescLayout_t& srCreate, Handle& srLayout, Handle* spSets, u32 sSetCount, const char* spSetName )
{
	srLayout = render->CreateDescLayout( srCreate );

	if ( srLayout == InvalidHandle )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create descriptor layout\n" );
		return false;
	}

	AllocDescLayout_t allocLayout{};
	allocLayout.apName    = spSetName;
	allocLayout.aLayout   = srLayout;
	// allocLayout.aCount    = sCount;
	allocLayout.aSetCount = sSetCount;

	if ( !render->AllocateDescLayout( allocLayout, spSets ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to allocate descriptor layout\n" );
		return false;
	}

	return true;
}


// bool Graphics_CreateVariableUniformLayout( ShaderDescriptor_t& srBuffer, const char* spLayoutName, const char* spSetName, int sCount )
// {
// 	return Graphics_CreateVariableDescLayout( EDescriptorType_UniformBuffer, srBuffer, spLayoutName, spSetName, sCount );
// }


// bool Graphics_CreateVariableStorageLayout( ShaderDescriptor_t& srBuffer, const char* spLayoutName, const char* spSetName, int sCount )
// {
// 	return Graphics_CreateVariableDescLayout( EDescriptorType_StorageBuffer, srBuffer, spLayoutName, spSetName, sCount );
// }


bool Graphics_CreateShaderBuffers( EBufferFlags sFlags, std::vector< Handle >& srBuffers, const char* spBufferName, size_t sBufferSize )
{
	// create buffers for it
	for ( size_t i = 0; i < srBuffers.size(); i++ )
	{
		Handle buffer = render->CreateBuffer( spBufferName, sBufferSize, sFlags, EBufferMemory_Host );

		if ( buffer == InvalidHandle )
		{
			Log_Error( gLC_ClientGraphics, "Failed to Create Light Uniform Buffer\n" );
			return false;
		}

		srBuffers[ i ] = buffer;
	}

	// update the descriptor sets
	// WriteDescSet_t update{};
	// update.apDescSets    = srUniform.apSets;
	// update.aDescSetCount = srUniform.aCount;
	// update.aType    = sType;
	// update.apData = srBuffers.data();
	// render->UpdateDescSet( update );

	return true;
}


bool Graphics_CreateUniformBuffers( std::vector< Handle >& srBuffers, const char* spBufferName, size_t sBufferSize )
{
	return Graphics_CreateShaderBuffers( EDescriptorType_UniformBuffer, srBuffers, spBufferName, sBufferSize );
}


bool Graphics_CreateStorageBuffers( std::vector< Handle >& srBuffers, const char* spBufferName, size_t sBufferSize )
{
	return Graphics_CreateShaderBuffers( EDescriptorType_StorageBuffer, srBuffers, spBufferName, sBufferSize );
}


// IDEA:
// 
// Make a global variable descriptor set that will store all textures, viewports, lights, etc.
// binding 0 will be textures, 1 for viewports, 2 for lights, etc.
// 
// We could have a per shader variable set with all materials for it, like we did originally
// 


static void Graphics_AllocateShaderArray( ShaderArrayAllocator_t& srAllocator, u32 sCount )
{
	srAllocator.aAllocated = sCount;
	srAllocator.aUsed      = 0;
	srAllocator.apFree     = ch_calloc_count< u32 >( sCount );

	// Fill the free list with indexes
	// for ( u32 index = srAllocator.aAllocated - 1, slot = 0; index > 0; --index, ++slot )
	// 	srAllocator.apFree[ slot ] = index;

	for ( u32 index = 0; index < srAllocator.aAllocated; index++ )
		srAllocator.apFree[ index ] = index;
}


// writes data in regions to a staging host buffer, and then copies those regions to the device buffer
void Graphics_WriteDeviceBufferRegions()
{
}


bool Graphics_CreateDescriptorSets( ShaderRequirmentsList_t& srRequire )
{
	// ------------------------------------------------------
	// Create Core Data Array Slots

	Graphics_AllocateShaderArray( gGraphicsData.aFreeSurfaceDraws, CH_R_MAX_SURFACE_DRAWS );

	Graphics_AllocateShaderArray( gGraphicsData.aCoreDataSlots[ EShaderCoreArray_Viewports ], CH_R_MAX_VIEWPORTS );

	Graphics_AllocateShaderArray( gGraphicsData.aCoreDataSlots[ EShaderCoreArray_LightWorld ], CH_R_MAX_LIGHT_TYPE );
	Graphics_AllocateShaderArray( gGraphicsData.aCoreDataSlots[ EShaderCoreArray_LightPoint ], CH_R_MAX_LIGHT_TYPE );
	Graphics_AllocateShaderArray( gGraphicsData.aCoreDataSlots[ EShaderCoreArray_LightCone ], CH_R_MAX_LIGHT_TYPE );
	Graphics_AllocateShaderArray( gGraphicsData.aCoreDataSlots[ EShaderCoreArray_LightCapsule ], CH_R_MAX_LIGHT_TYPE );
	
	// ------------------------------------------------------
	// Create Core Data Buffer

	gGraphicsData.aCoreDataStaging.aStagingBuffer = render->CreateBuffer( "Core Staging Buffer", sizeof( Buffer_Core_t ), EBufferFlags_TransferSrc, EBufferMemory_Host );
	gGraphicsData.aCoreDataStaging.aBuffer        = render->CreateBuffer( "Core Buffer", sizeof( Buffer_Core_t ), EBufferFlags_Storage | EBufferFlags_TransferDst, EBufferMemory_Device );

	if ( !gGraphicsData.aCoreDataStaging.aBuffer || !gGraphicsData.aCoreDataStaging.aStagingBuffer )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Core Shader Storage Buffer\n" );
		return false;
	}
	
	// ------------------------------------------------------
	// Create SurfaceDraw Buffer

	gGraphicsData.aSurfaceDrawsStaging.aStagingBuffer = render->CreateBuffer( "Surface Draw Staging Buffer", sizeof( gGraphicsData.aSurfaceDraws ), EBufferFlags_TransferSrc, EBufferMemory_Host );
	gGraphicsData.aSurfaceDrawsStaging.aBuffer        = render->CreateBuffer( "Surface Draw Buffer", sizeof( gGraphicsData.aSurfaceDraws ), EBufferFlags_Storage | EBufferFlags_TransferDst, EBufferMemory_Device );

	if ( !gGraphicsData.aSurfaceDrawsStaging.aBuffer || !gGraphicsData.aSurfaceDrawsStaging.aStagingBuffer )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create SurfaceDraw Shader Storage Buffer\n" );
		return false;
	}

	// ------------------------------------------------------
	// Create Core Descriptor Set
	{
		CreateDescLayout_t createLayout{};
		createLayout.apName                      = "Global Layout";

		CreateDescBinding_t& texBinding          = createLayout.aBindings.emplace_back();
		texBinding.aBinding                      = CH_BINDING_TEXTURES;
		texBinding.aCount                        = CH_R_MAX_TEXTURES;
		texBinding.aStages                       = ShaderStage_All;
		texBinding.aType                         = EDescriptorType_CombinedImageSampler;

		CreateDescBinding_t& validation          = createLayout.aBindings.emplace_back();
		validation.aBinding                      = CH_BINDING_CORE;
		validation.aCount                        = 1;
		validation.aStages                       = ShaderStage_All;
		validation.aType                         = EDescriptorType_StorageBuffer;

		CreateDescBinding_t& surfaceDraws        = createLayout.aBindings.emplace_back();
		surfaceDraws.aBinding                    = CH_BINDING_SURFACE_DRAWS;
		surfaceDraws.aCount                      = 1;
		surfaceDraws.aStages                     = ShaderStage_All;
		surfaceDraws.aType                       = EDescriptorType_StorageBuffer;

		// TODO: this is for 2 swap chain images, but the swap chain image count could be different
		gShaderDescriptorData.aGlobalSets.aCount = 2;
		gShaderDescriptorData.aGlobalSets.apSets = ch_calloc_count< ChHandle_t >( gShaderDescriptorData.aGlobalSets.aCount );

		if ( !Graphics_CreateDescLayout( createLayout, gShaderDescriptorData.aGlobalLayout, gShaderDescriptorData.aGlobalSets.apSets, 2, "Global Sets" ) )
			return false;

		// update the descriptor sets
		WriteDescSet_t update{};

		update.aDescSetCount = gShaderDescriptorData.aGlobalSets.aCount;
		update.apDescSets    = gShaderDescriptorData.aGlobalSets.apSets;

		update.aBindingCount = static_cast< u32 >( createLayout.aBindings.size() );
		update.apBindings    = ch_calloc_count< WriteDescSetBinding_t >( update.aBindingCount );

		size_t i             = 0;
		for ( const CreateDescBinding_t& binding : createLayout.aBindings )
		{
			update.apBindings[ i ].aBinding = binding.aBinding;
			update.apBindings[ i ].aType    = binding.aType;
			update.apBindings[ i ].aCount   = binding.aCount;
			i++;
		}

		update.apBindings[ CH_BINDING_TEXTURES ].apData      = ch_calloc_count< ChHandle_t >( CH_R_MAX_TEXTURES );
		update.apBindings[ CH_BINDING_CORE ].apData          = &gGraphicsData.aCoreDataStaging.aBuffer;
		update.apBindings[ CH_BINDING_SURFACE_DRAWS ].apData = &gGraphicsData.aSurfaceDrawsStaging.aBuffer;

		// update.aImages = gViewportBuffers;
		render->UpdateDescSets( &update, 1 );

		free( update.apBindings[ CH_BINDING_TEXTURES ].apData );
		free( update.apBindings );
	}

	render->SetTextureDescSet( gShaderDescriptorData.aGlobalSets.apSets, gShaderDescriptorData.aGlobalSets.aCount, 0 );

	// ------------------------------------------------------
	// Create Per Shader Descriptor Sets
	{
		for ( ShaderRequirement_t& requirement : srRequire.aItems )
		{
			CreateDescLayout_t createLayout{};
			createLayout.apName = requirement.aShader.data();
			createLayout.aBindings.resize( requirement.aBindingCount );

			for ( u32 i = 0; i < requirement.aBindingCount; i++ )
			{
				createLayout.aBindings[ i ] = requirement.apBindings[ i ];
			}

			ShaderDescriptor_t& descriptor = gShaderDescriptorData.aPerShaderSets[ requirement.aShader ];
			descriptor.aCount              = 2;
			descriptor.apSets              = ch_calloc_count< ChHandle_t >( descriptor.aCount );

			if ( !Graphics_CreateDescLayout( createLayout, gShaderDescriptorData.aPerShaderLayout[ requirement.aShader ], descriptor.apSets, descriptor.aCount, "Shader Sets" ) )
			{
				Log_ErrorF( "Failed to Create Descriptor Set Layout for shader \"%s\"\n", requirement.aShader.data() );
				return false;
			}
		}
	}

	return true;
}


u32 Graphics_AllocateCoreSlot( EShaderCoreArray sSlot )
{
	if ( sSlot >= EShaderCoreArray_Count )
	{
		Log_ErrorF( gLC_ClientGraphics, "Invalid core shader data array (%zd), only %d arrays\n", sSlot, EShaderCoreArray_Count );
		return CH_SHADER_CORE_SLOT_INVALID;
	}

	ShaderArrayAllocator_t& allocator = gGraphicsData.aCoreDataSlots[ sSlot ];

	if ( allocator.aUsed == allocator.aAllocated )
	{
		Log_ErrorF( gLC_ClientGraphics, "Out of slots for allocating core shader data for %s type, max of %zd\n",
			gShaderCoreArrayStr[ sSlot ], allocator.aAllocated );

		return CH_SHADER_CORE_SLOT_INVALID;
	}

	CH_ASSERT( allocator.apFree );

	// Get the base of this free list
	u32 index = allocator.apFree[ 0 ];
	allocator.aUsed++;

	CH_ASSERT( index != CH_SHADER_CORE_SLOT_INVALID );

	// shift everything down by one
	memcpy( &allocator.apFree[ 0 ], &allocator.apFree[ 1 ], sizeof( u32 ) * ( allocator.aAllocated - 1 ) );

	// mark the very end of the list as invalid
	allocator.apFree[ allocator.aAllocated - 1 ] = CH_SHADER_CORE_SLOT_INVALID;

	return index;
}


void Graphics_FreeCoreSlot( EShaderCoreArray sSlot, u32 sIndex )
{
	if ( sSlot >= EShaderCoreArray_Count )
	{
		Log_ErrorF( gLC_ClientGraphics, "Invalid core shader data array (%d), only %d arrays\n", sSlot, EShaderCoreArray_Count );
		return;
	}

	ShaderArrayAllocator_t& allocator = gGraphicsData.aCoreDataSlots[ sSlot ];

	if ( allocator.aUsed == 0 )
	{
		Log_ErrorF( gLC_ClientGraphics, "No slots in use in core shader data for %s type, can't free this slot\n", gShaderCoreArrayStr[ sSlot ] );
		return;
	}

	if ( allocator.aAllocated >= sIndex )
	{
		Log_ErrorF( gLC_ClientGraphics, "Core shader data slot index is greater than amount allocated for %s type, max of %zd, tried to free index %zd\n",
		            gShaderCoreArrayStr[ sSlot ], allocator.aAllocated );
		return;
	}

	CH_ASSERT( allocator.apFree );
	CH_ASSERT( allocator.apFree[ allocator.aAllocated - allocator.aUsed ] == CH_SHADER_CORE_SLOT_INVALID );

	// write this free index
	allocator.apFree[ allocator.aAllocated - allocator.aUsed ] == sIndex;
	allocator.aUsed--;
}


void Graphics_OnResetCallback( ERenderResetFlags sFlags )
{
	gGraphicsData.aBackBuffer[ 0 ] = render->GetBackBufferColor();
	gGraphicsData.aBackBuffer[ 1 ] = render->GetBackBufferDepth();

	if ( gGraphicsData.aBackBuffer[ 0 ] == InvalidHandle || gGraphicsData.aBackBuffer[ 1 ] == InvalidHandle )
	{
		Log_Fatal( gLC_ClientGraphics, "Failed to get Back Buffer Handles!\n" );
		return;
	}

	// actually stupid, they are HANDLES, YOU SHOULDN'T NEED NEW ONES
	// only exception if we are in msaa now or not, blech
	render->GetBackBufferTextures( &gGraphicsData.aBackBufferTex[ 0 ], &gGraphicsData.aBackBufferTex[ 1 ], &gGraphicsData.aBackBufferTex[ 2 ] );

	int width, height;
	render->GetSurfaceSize( width, height );
	gGraphicsData.aViewData.aViewports[ 0 ].aSize = { width, height };

	if ( sFlags & ERenderResetFlags_MSAA )
	{
		Graphics_DestroyRenderPasses();

		if ( !Graphics_CreateRenderPasses() )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create render passes\n" );
			return;
		}

		render->ShutdownImGui();
		if ( !render->InitImGui( gGraphicsData.aRenderPassGraphics ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to re-init ImGui for Vulkan\n" );
			return;
		}

		if ( !Graphics_ShaderInit( true ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to Recreate Shaders!\n" );
			return;
		}
	}
}


bool Graphics_Init()
{
	gGraphicsData.aCommandBufferCount = render->GetCommandBufferHandles( nullptr );

	if ( gGraphicsData.aCommandBufferCount < 1 )
	{
		Log_Fatal( gLC_ClientGraphics, "Failed to get render command buffers!\n" );
		return false;
	}

	gGraphicsData.aCommandBuffers = ch_malloc_count< Handle >( gGraphicsData.aCommandBufferCount );
	render->GetCommandBufferHandles( gGraphicsData.aCommandBuffers );

	render->SetResetCallback( Graphics_OnResetCallback );

	render->GetBackBufferTextures( &gGraphicsData.aBackBufferTex[ 0 ], &gGraphicsData.aBackBufferTex[ 1 ], &gGraphicsData.aBackBufferTex[ 2 ] );

	// TODO: the backbuffer should probably be created in game code
	gGraphicsData.aBackBuffer[ 0 ] = render->GetBackBufferColor();
	gGraphicsData.aBackBuffer[ 1 ] = render->GetBackBufferDepth();

	if ( gGraphicsData.aBackBuffer[ 0 ] == InvalidHandle || gGraphicsData.aBackBuffer[ 1 ] == InvalidHandle )
	{
		Log_Fatal( gLC_ClientGraphics, "Failed to get Back Buffer Handles!\n" );
		return false;
	}

	if ( !Graphics_CreateRenderPasses() )
	{
		return false;
	}

	// Get information about the shaders we need for creating descriptor sets
	ShaderRequirmentsList_t shaderRequire{};
	if ( !Shader_ParseRequirements( shaderRequire ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Parse Shader Requirements!\n" );
		return false;
	}

	if ( !Graphics_CreateDescriptorSets( shaderRequire ) )
	{
		return false;
	}

	if ( !Graphics_ShaderInit( false ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to Create Shaders!\n" );
		return false;
	}

	if ( !Graphics_DebugDrawInit() )
		return false;

	// TEMP: make a world light
	// gpWorldLight = Graphics_CreateLight( ELightType_Directional );
	// gpWorldLight->aColor = { 1.0, 1.0, 1.0, 1.0 };
	// gpWorldLight->aColor = { 0.1, 0.1, 0.1 };

	return render->InitImGui( gGraphicsData.aRenderPassGraphics );
	// return render->InitImGui( gGraphicsData.aRenderPassGraphics );
}


void Graphics_Shutdown()
{
	// TODO: Free Descriptor Set allocations

	if ( gGraphicsData.aCommandBuffers )
		free( gGraphicsData.aCommandBuffers );

	gGraphicsData.aCommandBuffers = nullptr;

	for ( u32 i = 0; i < EShaderCoreArray_Count; i++ )
	{
		if ( gGraphicsData.aCoreDataSlots[ i ].apFree )
			free( gGraphicsData.aCoreDataSlots[ i ].apFree );

		gGraphicsData.aCoreDataSlots[ i ].apFree     = nullptr;
		gGraphicsData.aCoreDataSlots[ i ].aAllocated = 0;
		gGraphicsData.aCoreDataSlots[ i ].aUsed      = 0;
	}

	if ( gGraphicsData.aFreeSurfaceDraws.apFree )
		free( gGraphicsData.aFreeSurfaceDraws.apFree );
}


// https://iquilezles.org/articles/frustumcorrect/
bool Frustum_t::IsBoxVisible( const glm::vec3& sMin, const glm::vec3& sMax ) const
{
	PROF_SCOPE();

	// Check Box Outside/Inside of Frustum
	for ( int i = 0; i < EFrustum_Count; i++ )
	{
		if ( ( glm::dot( aPlanes[ i ], glm::vec4( sMin.x, sMin.y, sMin.z, 1.0f ) ) < 0.0 ) &&
		     ( glm::dot( aPlanes[ i ], glm::vec4( sMax.x, sMin.y, sMin.z, 1.0f ) ) < 0.0 ) &&
		     ( glm::dot( aPlanes[ i ], glm::vec4( sMin.x, sMax.y, sMin.z, 1.0f ) ) < 0.0 ) &&
		     ( glm::dot( aPlanes[ i ], glm::vec4( sMax.x, sMax.y, sMin.z, 1.0f ) ) < 0.0 ) &&
		     ( glm::dot( aPlanes[ i ], glm::vec4( sMin.x, sMin.y, sMax.z, 1.0f ) ) < 0.0 ) &&
		     ( glm::dot( aPlanes[ i ], glm::vec4( sMax.x, sMin.y, sMax.z, 1.0f ) ) < 0.0 ) &&
		     ( glm::dot( aPlanes[ i ], glm::vec4( sMin.x, sMax.y, sMax.z, 1.0f ) ) < 0.0 ) &&
		     ( glm::dot( aPlanes[ i ], glm::vec4( sMax.x, sMax.y, sMax.z, 1.0f ) ) < 0.0 ) )
		{
			return false;
		}
	}

	// Check Frustum Outside/Inside Box
	for ( int j = 0; j < 3; j++ )
	{
		int out = 0;
		for ( int i = 0; i < 8; i++ )
		{
			if ( aPoints[ i ][ j ] > sMax[ j ] )
				out++;
		}

		if ( out == 8 )
			return false;

		out = 0;
		for ( int i = 0; i < 8; i++ )
		{
			if ( aPoints[ i ][ j ] < sMin[ j ] )
				out++;
		}

		if ( out == 8 )
			return false;
	}

	return true;
}


void Graphics_CreateFrustum( Frustum_t& srFrustum, const glm::mat4& srViewMat )
{
	PROF_SCOPE();

	glm::mat4 m                          = glm::transpose( srViewMat );
	glm::mat4 inv                        = glm::inverse( srViewMat );

	srFrustum.aPlanes[ EFrustum_Left ]   = m[ 3 ] + m[ 0 ];
	srFrustum.aPlanes[ EFrustum_Right ]  = m[ 3 ] - m[ 0 ];
	srFrustum.aPlanes[ EFrustum_Bottom ] = m[ 3 ] + m[ 1 ];
	srFrustum.aPlanes[ EFrustum_Top ]    = m[ 3 ] - m[ 1 ];
	srFrustum.aPlanes[ EFrustum_Near ]   = m[ 3 ] + m[ 2 ];
	srFrustum.aPlanes[ EFrustum_Far ]    = m[ 3 ] - m[ 2 ];

	// Calculate Frustum Points
	for ( int i = 0; i < 8; i++ )
	{
		glm::vec4 ff             = inv * gFrustumFaceData[ i ];
		srFrustum.aPoints[ i ].x = ff.x / ff.w;
		srFrustum.aPoints[ i ].y = ff.y / ff.w;
		srFrustum.aPoints[ i ].z = ff.z / ff.w;
	}
}


Frustum_t Graphics_CreateFrustum( const glm::mat4& srViewInfo )
{
	Frustum_t frustum;
	Graphics_CreateFrustum( frustum, srViewInfo );
	return frustum;
}


ModelBBox_t Graphics_CreateWorldAABB( glm::mat4& srMatrix, const ModelBBox_t& srBBox )
{
	PROF_SCOPE();

	glm::vec4 corners[ 8 ];

	// Fill array with the corners of the AABB 
	corners[ 0 ] = { srBBox.aMin.x, srBBox.aMin.y, srBBox.aMin.z, 1.f };
	corners[ 1 ] = { srBBox.aMin.x, srBBox.aMin.y, srBBox.aMax.z, 1.f };
	corners[ 2 ] = { srBBox.aMin.x, srBBox.aMax.y, srBBox.aMin.z, 1.f };
	corners[ 3 ] = { srBBox.aMax.x, srBBox.aMin.y, srBBox.aMin.z, 1.f };
	corners[ 4 ] = { srBBox.aMin.x, srBBox.aMax.y, srBBox.aMax.z, 1.f };
	corners[ 5 ] = { srBBox.aMax.x, srBBox.aMin.y, srBBox.aMax.z, 1.f };
	corners[ 6 ] = { srBBox.aMax.x, srBBox.aMax.y, srBBox.aMin.z, 1.f };
	corners[ 7 ] = { srBBox.aMax.x, srBBox.aMax.y, srBBox.aMax.z, 1.f };

	glm::vec3 globalMin;
	glm::vec3 globalMax;

	// Transform all of the corners, and keep track of the greatest and least
	// values we see on each coordinate axis.
	for ( int i = 0; i < 8; i++ )
	{
		glm::vec3 transformed = srMatrix * corners[ i ];

		if ( i > 0 )
		{
			globalMin = glm::min( globalMin, transformed );
			globalMax = glm::max( globalMax, transformed );
		}
		else
		{
			globalMin = transformed;
			globalMax = transformed;
		}
	}

	ModelBBox_t aabb( globalMin, globalMax );
	return aabb;
}


ChHandle_t Graphics_CreateRenderable( ChHandle_t sModel )
{
	Model* model = nullptr;
	if ( !gGraphicsData.aModels.Get( sModel, &model ) )
	{
		Log_Warn( gLC_ClientGraphics, "Renderable has no model!\n" );
		return InvalidHandle;
	}

	Log_Dev( gLC_ClientGraphics, 1, "Created Renderable\n" );

	Renderable_t* modelDraw  = nullptr;
	ChHandle_t    drawHandle = InvalidHandle;

	if ( !( drawHandle = gGraphicsData.aRenderables.Create( &modelDraw ) ) )
	{
		Log_ErrorF( gLC_ClientGraphics, "Failed to create Renderable_t\n" );
		return InvalidHandle;
	}

	modelDraw->aModel       = sModel;
	modelDraw->aModelMatrix = glm::identity< glm::mat4 >();
	modelDraw->aTestVis     = true;
	modelDraw->aCastShadow  = true;
	modelDraw->aVisible     = true;

	// memset( &modelDraw->aAABB, 0, sizeof( ModelBBox_t ) );
	// Graphics_UpdateModelAABB( modelDraw );
	modelDraw->aAABB        = Graphics_CreateWorldAABB( modelDraw->aModelMatrix, gGraphicsData.aModelBBox[ modelDraw->aModel ] );

	modelDraw->aBlendShapeWeights.resize( model->apVertexData->aBlendShapeCount );
	modelDraw->aVertexBuffers.resize( model->apBuffers->aVertex.size() );

	if ( modelDraw->aBlendShapeWeights.size() )
	{
		// we need new vertex buffers for the modified vertices
		for ( size_t j = 0; j < model->apVertexData->aData.size(); j++ )
		{
			size_t     bufferSize   = Graphics_GetVertexAttributeSize( model->apVertexData->aData[ j ].aAttrib ) * model->apVertexData->aCount;

			ChHandle_t deviceBuffer = render->CreateBuffer(
			  "output renderable vertices idfk",
			  bufferSize,
			  EBufferFlags_Storage | EBufferFlags_Vertex | EBufferFlags_TransferDst,
			  EBufferMemory_Device );

			BufferRegionCopy_t copy;
			copy.aSrcOffset = 0;
			copy.aDstOffset = 0;
			copy.aSize      = bufferSize;

			render->BufferCopyQueued( model->apBuffers->aVertex[ j ], deviceBuffer, &copy, 1 );
			
			modelDraw->aVertexBuffers[ j ] = deviceBuffer;
		}

		// Now Create a Blend Shape Weights Storage Buffer
		modelDraw->aBlendShapeWeightsBuffer = render->CreateBuffer(
		  "BlendShape Weights",
		  sizeof( float ) * modelDraw->aBlendShapeWeights.size(),
		  EBufferFlags_Storage,
		  EBufferMemory_Host );
	}
	else
	{
		for ( size_t j = 0; j < model->apBuffers->aVertex.size(); j++ )
		{
			modelDraw->aVertexBuffers[ j ] = model->apBuffers->aVertex[ j ];
		}
	}

	return drawHandle;
}


Renderable_t* Graphics_GetRenderableData( Handle sRenderable )
{
	PROF_SCOPE();

	Renderable_t* renderable = nullptr;
	if ( !gGraphicsData.aRenderables.Get( sRenderable, &renderable ) )
	{
		Log_Warn( gLC_ClientGraphics, "Failed to find Renderable!\n" );
		return nullptr;
	}

	return renderable;
}


void Graphics_FreeRenderable( Handle sRenderable )
{
	// TODO: QUEUE THIS RENDERABLE FOR DELETION, DON'T DELETE THIS NOW, SAME WITH MODELS, MATERIALS, AND TEXTURES!!!

	if ( !sRenderable )
		return;

	Renderable_t* renderable = nullptr;
	if ( !gGraphicsData.aRenderables.Get( sRenderable, &renderable ) )
	{
		Log_Warn( gLC_ClientGraphics, "Failed to find Renderable to delete!\n" );
		return;
	}

	// HACK - REMOVE WHEN WE ADD QUEUED DELETION FOR ASSETS
	render->WaitForQueues();

	if ( renderable->aBlendShapeWeightsBuffer )
	{
		render->DestroyBuffer( renderable->aBlendShapeWeightsBuffer );
		renderable->aBlendShapeWeightsBuffer = CH_INVALID_HANDLE;

		// If we have a blend shape weights buffer, then we have custom vertex buffers for this renderable
		for ( size_t j = 0; j < renderable->aVertexBuffers.size(); j++ )
		{
			render->DestroyBuffer( renderable->aVertexBuffers[ j ] );
		}
	}

	renderable->aVertexBuffers.clear();

	gGraphicsData.aRenderables.Remove( sRenderable );
}


void Graphics_UpdateRenderableAABB( Handle sRenderable )
{
	PROF_SCOPE();

	if ( !sRenderable )
		return;

	if ( Renderable_t* renderable = Graphics_GetRenderableData( sRenderable ) )
		gGraphicsData.aRenderAABBUpdate.emplace( sRenderable, gGraphicsData.aModelBBox[ renderable->aModel ] );
}


void Graphics_ConsolidateRenderables()
{
	gGraphicsData.aRenderables.Consolidate();
}


// ---------------------------------------------------------------------------------------
// Vertex Format/Attributes


GraphicsFmt Graphics_GetVertexAttributeFormat( VertexAttribute attrib )
{
	switch ( attrib )
	{
		default:
			Log_ErrorF( gLC_ClientGraphics, "GetVertexAttributeFormat: Invalid VertexAttribute specified: %d\n", attrib );
			return GraphicsFmt::INVALID;

		case VertexAttribute_Position:
			return GraphicsFmt::RGB323232_SFLOAT;

		// NOTE: could be smaller probably
		case VertexAttribute_Normal:
			return GraphicsFmt::RGB323232_SFLOAT;

		case VertexAttribute_Color:
			return GraphicsFmt::RGB323232_SFLOAT;

		case VertexAttribute_TexCoord:
			return GraphicsFmt::RG3232_SFLOAT;

		// case VertexAttribute_MorphPos:
		// 	return GraphicsFmt::RGB323232_SFLOAT;
	}
}


size_t Graphics_GetVertexAttributeTypeSize( VertexAttribute attrib )
{
	GraphicsFmt format = Graphics_GetVertexAttributeFormat( attrib );

	switch ( format )
	{
		default:
			Log_ErrorF( gLC_ClientGraphics, "GetVertexAttributeTypeSize: Invalid DataFormat specified from vertex attribute: %d\n", format );
			return 0;

		case GraphicsFmt::INVALID:
			return 0;

		case GraphicsFmt::RGB323232_SFLOAT:
		case GraphicsFmt::RG3232_SFLOAT:
			return sizeof( float );
	}
}


size_t Graphics_GetVertexAttributeSize( VertexAttribute attrib )
{
	GraphicsFmt format = Graphics_GetVertexAttributeFormat( attrib );

	switch ( format )
	{
		default:
			Log_ErrorF( gLC_ClientGraphics, "GetVertexAttributeSize: Invalid DataFormat specified from vertex attribute: %d\n", format );
			return 0;

		case GraphicsFmt::INVALID:
			return 0;

		case GraphicsFmt::RGB323232_SFLOAT:
			return ( 3 * sizeof( float ) );

		case GraphicsFmt::RG3232_SFLOAT:
			return ( 2 * sizeof( float ) );
	}
}


size_t Graphics_GetVertexFormatSize( VertexFormat format )
{
	size_t size = 0;

	for ( int attrib = 0; attrib < VertexAttribute_Count; attrib++ )
	{
		// does this format contain this attribute?
		// if so, add the attribute size to it
		if ( format & ( 1 << attrib ) )
			size += Graphics_GetVertexAttributeSize( (VertexAttribute)attrib );
	}

	return size;
}


void Graphics_GetVertexBindingDesc( VertexFormat format, std::vector< VertexInputBinding_t >& srAttrib )
{
	u32 binding = 0;

	for ( u8 attrib = 0; attrib < VertexAttribute_Count; attrib++ )
	{
		// does this format contain this attribute?
		// if so, add this attribute to the vector
		if ( format & ( 1 << attrib ) )
		{
			srAttrib.emplace_back(
			  binding++,
			  (u32)Graphics_GetVertexAttributeSize( (VertexAttribute)attrib ),
			  false );
		}
	}
}


void Graphics_GetVertexAttributeDesc( VertexFormat format, std::vector< VertexInputAttribute_t >& srAttrib )
{
	u32  location   = 0;
	u32  binding    = 0;
	u32  offset     = 0;

	for ( u8 attrib = 0; attrib < VertexAttribute_Count; attrib++ )
	{
		// does this format contain this attribute?
		// if so, add this attribute to the vector
		if ( format & ( 1 << attrib ) )
		{
			srAttrib.emplace_back(
			  location++,
			  binding++,
			  Graphics_GetVertexAttributeFormat( (VertexAttribute)attrib ),
			  0  // no offset
			);
		}
	}
}


const char* Graphics_GetVertexAttributeName( VertexAttribute attrib )
{
	switch ( attrib )
	{
		default:
		case VertexAttribute_Count:
			return "ERROR";

		case VertexAttribute_Position:
			return "Position";

		case VertexAttribute_Normal:
			return "Normal";

		case VertexAttribute_TexCoord:
			return "TexCoord";

		case VertexAttribute_Color:
			return "Color";
	}
}


// ---------------------------------------------------------------------------------------
// Buffers

// sBufferSize is sizeof(element) * count
Handle CreateModelBuffer( const char* spName, void* spData, size_t sBufferSize, EBufferFlags sUsage )
{
	PROF_SCOPE();

	Handle stagingBuffer = render->CreateBuffer( "Staging Model Buffer", sBufferSize, sUsage | EBufferFlags_TransferSrc, EBufferMemory_Host );

	// Copy Data to Buffer
	render->BufferWrite( stagingBuffer, sBufferSize, spData );

	Handle deviceBuffer = render->CreateBuffer( spName, sBufferSize, sUsage | EBufferFlags_TransferDst, EBufferMemory_Device );

	// Copy Local Buffer data to Device
	BufferRegionCopy_t copy;
	copy.aSrcOffset = 0;
	copy.aDstOffset = 0;
	copy.aSize      = sBufferSize;

	render->BufferCopy( stagingBuffer, deviceBuffer, &copy, 1 );

	render->DestroyBuffer( stagingBuffer );

	return deviceBuffer;
}


void Graphics_CreateBlendShapeBuffer( ChHandle_t& srBuffer, VertexData_t* spVertexData, VertFormatData_t& srVertexFormatData, const char* spDebugName )
{
	PROF_SCOPE();

	if ( spVertexData == nullptr || spVertexData->aCount == 0 )
	{
		Log_Warn( gLC_ClientGraphics, "Trying to create Vertex Buffers for mesh with no vertices!\n" );
		return;
	}

	srBuffer = CreateModelBuffer(
	  spDebugName ? spDebugName : "MORPHS",
	  srVertexFormatData.apData,
	  Graphics_GetVertexFormatSize( srVertexFormatData.aFormat ) * spVertexData->aCount * spVertexData->aBlendShapeCount,
	  EBufferFlags_Storage );
}


void Graphics_CreateVertexBuffers( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName )
{
	PROF_SCOPE();

	if ( spVertexData == nullptr || spVertexData->aCount == 0 )
	{
		Log_Warn( gLC_ClientGraphics, "Trying to create Vertex Buffers for mesh with no vertices!\n" );
		return;
	}

	if ( spBuffer == nullptr )
	{
		Log_Warn( gLC_ClientGraphics, "Graphics_CreateVertexBuffers: ModelBuffers_t is nullptr!\n" );
		return;
	}

	// Get Attributes the shader wants
	// TODO: what about if we don't have an attribute the shader wants???
	// maybe create a temporary empty buffer full of zeros? idk
	std::vector< VertAttribData_t* > attribs;

	for ( size_t j = 0; j < spVertexData->aData.size(); j++ )
	{
		VertAttribData_t& data = spVertexData->aData[ j ];

		// if ( shaderFormat & ( 1 << data.aAttrib ) )
		attribs.push_back( &data );
	}

	spBuffer->aVertex.resize( attribs.size() );

	for ( size_t j = 0; j < attribs.size(); j++ )
	{
		auto& data       = attribs[ j ];
		char* bufferName = nullptr;

#ifdef _DEBUG
		if ( spDebugName )
		{
			const char* attribName = Graphics_GetVertexAttributeName( data->aAttrib );

			size_t      len        = strlen( spDebugName ) + strlen( attribName );
			bufferName             = new char[ len + 9 ];  // MEMORY LEAK - need string memory pool

			snprintf( bufferName, len + 9, "VB | %s | %s", attribName, spDebugName );
		}
#endif

		Handle buffer = CreateModelBuffer(
		  bufferName ? bufferName : "VB",
		  data->apData,
		  Graphics_GetVertexAttributeSize( data->aAttrib ) * spVertexData->aCount,
		  EBufferFlags_Storage | EBufferFlags_Vertex | EBufferFlags_TransferSrc );

		spBuffer->aVertex[ j ] = buffer;
	}

	// Handle Blend Shapes
	
	// TODO: this expects each vertex attribute to have it's own vertex buffer
	// but we want the blend shapes to all be in one huge storage buffer
	// what this expects is a buffer for each vertex attribute, for each blend shape
	// so we would need like (blend shape count) * (vertex attribute count) buffers for blend shapes,
	// that's not happening and needs to change

	if ( spVertexData->aBlendShapeCount == 0 )
		return;

	spBuffer->aMorphs = CreateModelBuffer(
	  spDebugName ? spDebugName : "MORPHS",
	  spVertexData->aBlendShapeData.apData,
	  Graphics_GetVertexFormatSize( spVertexData->aBlendShapeData.aFormat ) * spVertexData->aCount * spVertexData->aBlendShapeCount,
	  EBufferFlags_Storage );
}


void Graphics_CreateIndexBuffer( ModelBuffers_t* spBuffer, VertexData_t* spVertexData, const char* spDebugName )
{
	PROF_SCOPE();

	char* bufferName = nullptr;

	if ( spVertexData->aIndices.empty() )
	{
		Log_Warn( gLC_ClientGraphics, "Trying to create Index Buffer for mesh with no indices!\n" );
		return;
	}

#ifdef _DEBUG
	if ( spDebugName )
	{
		size_t len = strlen( spDebugName );
		bufferName = new char[ len + 6 ];  // MEMORY LEAK - need string memory pool

		snprintf( bufferName, len + 6, "IB | %s", spDebugName );
	}
#endif

	spBuffer->aIndex = CreateModelBuffer(
	  bufferName ? bufferName : "IB",
	  spVertexData->aIndices.data(),
	  // sizeof( u32 ) * spVertexData->aIndices.size(),
	  spVertexData->aIndices.size_bytes(),
	  EBufferFlags_Index );
}


#if 0
void Graphics_CreateModelBuffers( ModelBuffers_t* spBuffers, VertexData_t* spVertexData, bool sCreateIndex, const char* spDebugName )
{
	if ( !spBuffers )
	{
		Log_Error( gLC_ClientGraphics, "ModelBuffers_t is nullptr\n" );
		return;
	}
	else if ( spBuffers->aVertex.size() )
	{
		Log_Error( gLC_ClientGraphics, "Model Vertex Buffers already created\n" );
		return;
	}

	char* debugName = nullptr;
#ifdef _DEBUG
	if ( spDebugName )
	{
		size_t nameLen = strlen( spDebugName );
		debugName      = new char[ nameLen ];  // MEMORY LEAK - need string memory pool
		snprintf( debugName, nameLen, "%s", spDebugName );
	}
#endif

	Graphics_CreateVertexBuffers( spBuffers, spVertexData, debugName );

	if ( sCreateIndex )
		Graphics_CreateIndexBuffer( spBuffers, spVertexData, debugName );
}
#endif
