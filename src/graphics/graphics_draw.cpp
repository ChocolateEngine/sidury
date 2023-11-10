#include "core/core.h"
#include "igui.h"
#include "render/irender.h"
#include "graphics_int.h"
#include "lighting.h"
#include "debug_draw.h"
#include "mesh_builder.h"
#include "imgui/imgui.h"

#include <forward_list>
#include <stack>
#include <set>
#include <unordered_set>

// --------------------------------------------------------------------------------------

// shaders, fun
u32                                    Shader_Basic3D_UpdateMaterialData( Handle sMat );


// --------------------------------------------------------------------------------------
// Other

size_t                                 gModelDrawCalls = 0;
size_t                                 gVertsDrawn     = 0;

extern ChVector< Shader_VertexData_t > gDebugLineVerts;

CONVAR( r_vis, 1 );
CONVAR( r_vis_lock, 0 );

CONVAR( r_line_thickness, 2 );

CONVAR( r_show_draw_calls, 0 );

CONVAR( r_random_blend_shapes, 1 );
CONVAR( r_reset_blend_shapes, 0 );


bool Graphics_ViewFrustumTest( Renderable_t* spModelDraw, int sViewportIndex )
{
	PROF_SCOPE();

	if ( !spModelDraw )
		return false;

	if ( gGraphicsData.aViewData.aViewports.size() <= sViewportIndex || !r_vis || !spModelDraw->aTestVis )
		return true;

	if ( !spModelDraw->aVisible )
		return false;

	ViewportShader_t& viewInfo = gGraphicsData.aViewData.aViewports[ sViewportIndex ];

	if ( !viewInfo.aActive || !viewInfo.aAllocated )
		return false;

	Frustum_t& frustum = gGraphicsData.aViewData.aFrustums[ sViewportIndex ];

	return frustum.IsBoxVisible( spModelDraw->aAABB.aMin, spModelDraw->aAABB.aMax );
}


void Graphics::Reset()
{
	PROF_SCOPE();

	render->Reset();
}


void Graphics::NewFrame()
{
	PROF_SCOPE();

	render->NewFrame();

	Graphics_DebugDrawNewFrame();
}


// TODO: experiment with instanced drawing
void Graphics_CmdDrawSurface( Handle cmd, Model* spModel, size_t sSurface )
{
	PROF_SCOPE();

	Mesh& mesh = spModel->aMeshes[ sSurface ];

	// TODO: figure out a way to use vertex and index offsets with this vertex format stuff
	// ideally, it would be less vertex buffer binding, but would be harder to pull off
	if ( spModel->apBuffers->aIndex )
		render->CmdDraw(
		  cmd,
		  mesh.aIndexCount,
		  1,
		  mesh.aIndexOffset,
		  0 );

		// render->CmdDrawIndexed(
		//   cmd,
		//   mesh.aIndexCount,
		//   1,                  // instance count
		//   mesh.aIndexOffset,
		//   0, // mesh.aVertexOffset,
		//   0 );

	else
		render->CmdDraw(
		  cmd,
		  mesh.aVertexCount,
		  1,
		  mesh.aVertexOffset,
		  0 );

	gModelDrawCalls++;
	gVertsDrawn += mesh.aVertexCount;
}


bool Graphics_BindModel( ChHandle_t cmd, VertexFormat sVertexFormat, Model* spModel, ChHandle_t sVertexBuffer )
{
#if 0
	PROF_SCOPE();

	// Bind the mesh's vertex and index buffers

	// Get Vertex Buffers the shader wants
	// TODO: what about if we don't have an attribute the shader wants???
	ChVector< ChHandle_t > vertexBuffers;

	// lazy hack, blech
	// ChVector< ChHandle_t >& allVertexBuffers = spRenderable->aOutVertexBuffers.size() ? spRenderable->aOutVertexBuffers : spModel->apBuffers->aVertex;

	// TODO: THIS CAN BE DONE WHEN ADDING THE MODEL TO THE MAIN DRAW LIST, AND PUT IN SurfaceDraw_t
	// for ( size_t i = 0; i < spModel->apVertexData->aData.size(); i++ )
	// {
	// 	VertAttribData_t& data = spModel->apVertexData->aData[ i ];
	// 
	// 	if ( sVertexFormat & ( 1 << data.aAttrib ) )
	// 		vertexBuffers.push_back( srVertexBuffers[ i ] );
	// }

	uint64_t                offsets = 0;

	// size_t* offsets = (size_t*)CH_STACK_ALLOC( sizeof( size_t ) * vertexBuffers.size() );
	// if ( offsets == nullptr )
	// {
	// 	Log_Error( gLC_ClientGraphics, "Graphics_BindModel: Failed to allocate vertex buffer offsets!\n" );
	// 	return false;
	// }
	// 
	// // TODO: i could probably use offsets here, i imagine it might actually be faster?
	// memset( offsets, 0, sizeof( size_t ) * vertexBuffers.size() );

	render->CmdBindVertexBuffers( cmd, 0, 1, &sVertexBuffer, &offsets );

	// TODO: store index type here somewhere
	if ( spModel->apBuffers->aIndex )
		render->CmdBindIndexBuffer( cmd, spModel->apBuffers->aIndex, 0, EIndexType_U32 );

	// SHADER: update and bind per object descriptor set?

	// CH_STACK_FREE( offsets );
#endif
	return true;
}


void Graphics_DrawShaderRenderables( Handle cmd, size_t sIndex, Handle shader, size_t sViewIndex, ChVector< SurfaceDraw_t >& srRenderList )
{
	PROF_SCOPE();

	if ( !Shader_Bind( cmd, sIndex, shader ) )
	{
		Log_ErrorF( gLC_ClientGraphics, "Failed to bind shader: %s\n", gGraphics.GetShaderName( shader ) );
		return;
	}

	SurfaceDraw_t* prevSurface = nullptr;
	Model*         prevModel   = nullptr;

	ShaderData_t*  shaderData  = Shader_GetData( shader );
	if ( !shaderData )
		return;

	if ( shaderData->aDynamicState & EDynamicState_LineWidth )
		render->CmdSetLineWidth( cmd, r_line_thickness );

	VertexFormat vertexFormat = Shader_GetVertexFormat( shader );

	for ( uint32_t i = 0; i < srRenderList.size(); )
	{
		SurfaceDraw_t& surfaceDraw = srRenderList[ i ];

		Renderable_t* renderable = nullptr;
		if ( !gGraphicsData.aRenderables.Get( surfaceDraw.aRenderable, &renderable ) )
		{
			Log_Warn( gLC_ClientGraphics, "Draw Data does not exist for renderable!\n" );
			srRenderList.remove( i );
			continue;
		}

		// get model and check if it's nullptr
		if ( renderable->aModel == InvalidHandle )
		{
			Log_Error( gLC_ClientGraphics, "Graphics::DrawShaderRenderables: model handle is InvalidHandle\n" );
			srRenderList.remove( i );
			continue;
		}

		// get model data
		Model* model = gGraphics.GetModelData( renderable->aModel );
		if ( !model )
		{
			Log_Error( gLC_ClientGraphics, "Graphics::DrawShaderRenderables: model is nullptr\n" );
			srRenderList.remove( i );
			continue;
		}

		// make sure this model has valid vertex buffers
		if ( model->apBuffers == nullptr || model->apBuffers->aVertex == CH_INVALID_HANDLE )
		{
			Log_Error( gLC_ClientGraphics, "No Vertex/Index Buffers for Model??\n" );
			srRenderList.remove( i );
			continue;
		}

		bool bindModel = !prevSurface;

		if ( prevSurface )
		{
			// bindModel |= prevSurface->apDraw->aModel != renderable->apDraw->aModel;
			// bindModel |= prevSurface->aSurface != renderable->aSurface;

			if ( prevModel )
			{
				bindModel |= prevModel->apBuffers != model->apBuffers;
				bindModel |= prevModel->apVertexData != model->apVertexData;
			}
		}

		if ( bindModel )
		{
			prevModel   = model;
			prevSurface = &surfaceDraw;
			if ( !Graphics_BindModel( cmd, vertexFormat, model, renderable->aVertexBuffer ) )
				continue;
		}

		// NOTE: not needed if the material is the same i think
		if ( !Shader_PreRenderableDraw( cmd, sIndex, shaderData, surfaceDraw ) )
			continue;

		Graphics_CmdDrawSurface( cmd, model, surfaceDraw.aSurface );
		i++;
	}
}


// Do Rendering with shader system and user land meshes
void Graphics_RenderView( Handle cmd, size_t sIndex, size_t sViewIndex, ViewRenderList_t& srViewList )
{
	PROF_SCOPE();

	// here we go again
	static Handle skybox    = gGraphics.GetShader( "skybox" );

	bool          hasSkybox = false;

	int width = 0, height = 0;
	render->GetSurfaceSize( width, height );

	Rect2D_t rect{};
	rect.aOffset.x = 0;
	rect.aOffset.y = 0;
	rect.aExtent.x = width;
	rect.aExtent.y = height;

	render->CmdSetScissor( cmd, 0, &rect, 1 );

	// flip viewport
	Viewport_t viewPort{};
	viewPort.x        = 0.f;
	viewPort.y        = height;
	viewPort.minDepth = 0.f;
	viewPort.maxDepth = 1.f;
	viewPort.width    = width;
	viewPort.height   = height * -1.f;

	render->CmdSetViewport( cmd, 0, &viewPort, 1 );

	for ( auto& [ shader, renderList ] : srViewList.aRenderLists )
	{
		if ( shader == InvalidHandle )
		{
			Log_Warn( gLC_ClientGraphics, "Invalid Shader Handle (0) in View RenderList\n" );
			continue;
		}

		if ( shader == skybox )
		{
			hasSkybox = true;
			continue;
		}

		Graphics_DrawShaderRenderables( cmd, sIndex, shader, sViewIndex, renderList );
	}

	// Draw Skybox - and set depth for skybox
	if ( hasSkybox )
	{
		viewPort.minDepth = 0.999f;
		viewPort.maxDepth = 1.f;

		render->CmdSetViewport( cmd, 0, &viewPort, 1 );

		Graphics_DrawShaderRenderables( cmd, sIndex, skybox, sViewIndex, srViewList.aRenderLists[ skybox ] );
	}
}


void Graphics_Render( Handle sCmd, size_t sIndex, ERenderPass sRenderPass )
{
	PROF_SCOPE();

	// render->CmdBindDescriptorSets( sCmd, sIndex, EPipelineBindPoint_Graphics, PIPELINE_LAYOUT, SETS, SET_COUNT );

	// TODO: add in some dependency thing here, when you add camera's in the game, you'll need to render those first before the final viewports (VR maybe)
	for ( size_t i = 0; i < gGraphicsData.aViewRenderLists.size(); i++ )
	{
		// blech
		if ( !gGraphicsData.aViewData.aViewports[ i ].aAllocated || !gGraphicsData.aViewData.aViewports[ i ].aActive )
			continue;

		// HACK HACK !!!!
		// don't render views with shader overrides here, the only override is the shadow map shader
		// and that is rendered in a separate render pass
		if ( gGraphicsData.aViewData.aViewports[ i ].aShaderOverride )
			continue;

		Graphics_RenderView( sCmd, sIndex, i, gGraphicsData.aViewRenderLists[ i ] );
	}
}


void Graphics_DoSkinning( ChHandle_t sCmd, u32 sCmdIndex )
{
#if 1
	static ChHandle_t shaderSkinning = gGraphics.GetShader( "__skinning" );

	if ( shaderSkinning == CH_INVALID_HANDLE )
	{
		Log_Error( gLC_ClientGraphics, "skinning shader not found, can't apply blend shapes and bone transforms!\n" );
		return;
	}

	if ( !Shader_Bind( sCmd, sCmdIndex, shaderSkinning ) )
	{
		Log_Error( gLC_ClientGraphics, "Failed to bind skinning shader, can't apply blend shapes and bone transforms!\n" );
		return;
	}

	ShaderData_t*    shaderSkinningData = Shader_GetData( shaderSkinning );
	IShaderPushComp* skinningPush       = shaderSkinningData->apPushComp;

	ChVector< GraphicsBufferMemoryBarrier_t > buffers;

	u32 i = 0;
	for ( ChHandle_t renderHandle : gGraphicsData.aSkinningRenderList )
	{
		Renderable_t* renderable = nullptr;
		if ( !gGraphicsData.aRenderables.Get( renderHandle, &renderable ) )
		{
			Log_Warn( gLC_ClientGraphics, "Renderable does not exist!\n" );
			continue;
		}

		// get model data
		Model* model = gGraphics.GetModelData( renderable->aModel );
		if ( !model )
		{
			Log_Error( gLC_ClientGraphics, CH_FUNC_NAME_CLASS ": model is nullptr\n" );
			continue;
		}

		// make sure this model has valid vertex buffers
		if ( model->apBuffers == nullptr || model->apBuffers->aVertex == CH_INVALID_HANDLE )
		{
			Log_Error( gLC_ClientGraphics, "No Vertex/Index Buffers for Model??\n" );
			continue;
		}

		i++;

		ShaderSkinning_Push push{};
		push.aRenderable            = CH_GET_HANDLE_INDEX( renderHandle );
		push.aSourceVertexBuffer    = Graphics_GetShaderBufferIndex( gGraphicsData.aVertexBuffers, model->apBuffers->aVertexHandle );
		push.aVertexCount           = model->apVertexData->aIndices.empty() ? model->apVertexData->aCount : model->apVertexData->aIndices.size();
		push.aBlendShapeCount       = model->apVertexData->aBlendShapeCount;
		push.aBlendShapeWeightIndex = Graphics_GetShaderBufferIndex( gGraphicsData.aBlendShapeWeightBuffers, renderable->aBlendShapeWeightsIndex );
		push.aBlendShapeDataIndex   = Graphics_GetShaderBufferIndex( gGraphicsData.aBlendShapeDataBuffers, model->apBuffers->aBlendShapeHandle );

		if ( push.aSourceVertexBuffer == UINT32_MAX || push.aBlendShapeWeightIndex == UINT32_MAX || push.aBlendShapeDataIndex == UINT32_MAX )
			continue;

		render->CmdPushConstants( sCmd, shaderSkinningData->aLayout, ShaderStage_Compute, 0, sizeof( push ), &push );
		render->CmdDispatch( sCmd, glm::max( 1U, push.aVertexCount / 64 ), 1, 1 );

#if 0
		VkBufferMemoryBarrier barrier = vkinit::buffer_barrier(matrixBuffer, _graphicsQueueFamily);
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
#endif

		GraphicsBufferMemoryBarrier_t& buffer = buffers.emplace_back();
		buffer.aSrcAccessMask                 = EGraphicsAccess_MemoryRead | EGraphicsAccess_MemoryWrite;
		buffer.aDstAccessMask                 = EGraphicsAccess_MemoryRead | EGraphicsAccess_MemoryWrite;
		buffer.aBuffer                        = Graphics_GetShaderBuffer( gGraphicsData.aVertexBuffers, renderable->aVertexIndex );
	}

	PipelineBarrier_t endBarrier{};
	// endBarrier.aSrcStageMask             = EPipelineStage_BottomOfPipe;  // EPipelineStage_ComputeShader;
	// endBarrier.aDstStageMask             = EPipelineStage_TopOfPipe;  // EPipelineStage_VertexShader;
	endBarrier.aSrcStageMask             = EPipelineStage_ComputeShader;
	endBarrier.aDstStageMask             = EPipelineStage_VertexShader;

	endBarrier.aBufferMemoryBarrierCount = buffers.size();
	endBarrier.apBufferMemoryBarriers    = buffers.data();

	render->CmdPipelineBarrier( sCmd, endBarrier );

#endif

	gGraphicsData.aSkinningRenderList.clear();
}


void Graphics_AddToViewRenderList()
{
}


void Graphics_UpdateShaderRenderableData()
{
	// TODO: THREADING

	// for each renderable
	//   get renderable gpu index
	//   write model matrix, material index, and vertex/index buffer indexes
	// 
	//   --- IDEA: Calcuate this in a computer shader, so we can do this in parallel way more than we could on the cpu ---
	//   clear previous light list
	//   for each light
	//     if renderable in light radius
	//       add to light list
	// 
	//   sort light list from smallest to greatest index
	//
}


void Graphics_UpdateCoreData()
{
}


#if 0
void Graphics_UpdateShaderBufferDescriptors( ShaderBufferList_t* spBufferList, u32* sBindings, u32 sCount )
{
	if ( sCount == 0 )
		return;

	WriteDescSet_t* writes = ch_malloc_count< WriteDescSet_t >( sCount );

	for ( u32 i = 0; i < sCount; i++ )
	{
		writes[ i ].aDescSetCount = gShaderDescriptorData.aGlobalSets.aCount;
		writes[ i ].apDescSets    = gShaderDescriptorData.aGlobalSets.apSets;

		writes[ i ].aBindingCount = 1;
		writes[ i ].apBindings    = CH_STACK_NEW( WriteDescSetBinding_t, update.aBindingCount );

		// move out of here aaa
		ChVector< ChHandle_t > buffers;
		buffers.resize( spBufferList[ i ].aBuffers.size() );

		writes[ i ].apBindings[ 0 ].aBinding = sBindings[ i ];
		writes[ i ].apBindings[ 0 ].aType    = EDescriptorType_StorageBuffer;
		writes[ i ].apBindings[ 0 ].aCount   = buffers.size();
		writes[ i ].apBindings[ 0 ].apData   = buffers.data();
	}

	render->UpdateDescSets( &writes[ i ], sCount );

	for ( u32 i = 0; i < sCount; i++ )
	{
		CH_STACK_FREE( writes[ i ].apBindings );
	}

	free( writes );
}
#endif


void Graphics_UpdateShaderBufferDescriptors( ShaderBufferList_t& srBufferList, u32 sBinding )
{
	srBufferList.aDirty = false;

	WriteDescSet_t write{};

	write.aDescSetCount = gShaderDescriptorData.aGlobalSets.aCount;
	write.apDescSets    = gShaderDescriptorData.aGlobalSets.apSets;

	write.aBindingCount = 1;
	write.apBindings    = CH_STACK_NEW( WriteDescSetBinding_t, write.aBindingCount );

	ChVector< ChHandle_t > buffers;
	buffers.reserve( srBufferList.aBuffers.size() );

	for ( auto& [ handle, buffer ] : srBufferList.aBuffers )
		buffers.push_back( buffer );

	write.apBindings[ 0 ].aBinding = sBinding;
	write.apBindings[ 0 ].aType    = EDescriptorType_StorageBuffer;
	write.apBindings[ 0 ].aCount   = buffers.size();
	write.apBindings[ 0 ].apData   = buffers.data();

	render->UpdateDescSets( &write, 1 );

	CH_STACK_FREE( write.apBindings );
}


void Graphics_PrepareDrawData()
{
	PROF_SCOPE();

	// fun
	static Handle        shadow_map       = gGraphics.GetShader( "__shadow_map" );
	static ShaderData_t* shadowShaderData = Shader_GetData( shadow_map );

	render->PreRenderPass();
	// Update Textures

	// if ( r_show_draw_calls )
	// {
	// 	gui->DebugMessage( "Model Draw Calls: %zd", gModelDrawCalls );
	// 	gui->DebugMessage( "Verts Drawn: %zd", gVertsDrawn );
	// 	gui->DebugMessage( "Debug Line Verts: %zd", gDebugLineVerts.size() );
	// }

	// {
	// 	PROF_SCOPE_NAMED( "Imgui Render" );
	// 	ImGui::Render();
	// }

	gModelDrawCalls = 0;
	gVertsDrawn     = 0;

	for ( const auto& mat : gGraphicsData.aDirtyMaterials )
	{
		Handle shader = gGraphics.Mat_GetShader( mat );

		// HACK HACK
		if ( gGraphics.GetShader( "basic_3d" ) == shader )
			Shader_Basic3D_UpdateMaterialData( mat );
	}

	gGraphicsData.aDirtyMaterials.clear();

	// update renderable AABB's
	for ( auto& [ renderHandle, bbox ] : gGraphicsData.aRenderAABBUpdate )
	{
		if ( Renderable_t* renderable = gGraphics.GetRenderableData( renderHandle ) )
		{
			if ( glm::length( bbox.aMin ) == 0 && glm::length( bbox.aMax ) == 0 )
			{
				Log_Warn( gLC_ClientGraphics, "Model Bounding Box not calculated, length of min and max is 0\n" );
				bbox = gGraphics.CalcModelBBox( renderable->aModel );
			}

			renderable->aAABB = gGraphics.CreateWorldAABB( renderable->aModelMatrix, bbox );
		}
	}

	gGraphicsData.aRenderAABBUpdate.clear();

	// Update Light Data
	Graphics_PrepareLights();

	// if ( gGraphicsData.aViewData.aUpdate )
	// {
	// 	gGraphicsData.aViewData.aUpdate = false;
	// 	// for ( size_t i = 0; i < gViewportBuffers.size(); i++ )
	// 	for ( size_t i = 0; i < 1; i++ )
	// 	{
	// 		render->BufferWrite( gGraphicsData.aViewData.aBuffers[ i ], sizeof( UBO_Viewport_t ), &gGraphicsData.aViewData.aViewports[ i ] );
	// 	}
	// }

	// update view frustums (CHANGE THIS, SHOULD NOT UPDATE EVERY SINGLE ONE PER FRAME  !!!!)
	if ( !r_vis_lock.GetBool() || gGraphicsData.aViewData.aFrustums.size() != gGraphicsData.aViewData.aViewports.size() )
	{
		gGraphicsData.aViewData.aFrustums.resize( gGraphicsData.aViewData.aViewports.size() );

		for ( size_t i = 0; i < gGraphicsData.aViewData.aViewports.size(); i++ )
		{
			Graphics_CreateFrustum( gGraphicsData.aViewData.aFrustums[ i ], gGraphicsData.aViewData.aViewports[ i ].aProjView );
			gGraphics.DrawFrustum( gGraphicsData.aViewData.aFrustums[ i ] );
		}
	}

	// --------------------------------------------------------------------

	bool updateShaderRenderables = gGraphicsData.aVertexBuffers.aDirty || gGraphicsData.aIndexBuffers.aDirty;
	
	// Update Vertex Buffer Array SSBO
	if ( gGraphicsData.aVertexBuffers.aDirty )
		Graphics_UpdateShaderBufferDescriptors( gGraphicsData.aVertexBuffers, CH_BINDING_VERTEX_BUFFERS );

	// Update Index Buffer Array SSBO
	if ( gGraphicsData.aIndexBuffers.aDirty )
		Graphics_UpdateShaderBufferDescriptors( gGraphicsData.aIndexBuffers, CH_BINDING_INDEX_BUFFERS );

	// --------------------------------------------------------------------

	Shader_ResetPushData();

	bool usingShadow = Graphics_IsUsingShadowMaps();

	// --------------------------------------------------------------------
	// Prepare View Render Lists

	bool visLocked   = r_vis_lock.GetBool();

	if ( !visLocked )
	{
		// Reset Render Lists
		for ( ViewRenderList_t& viewList : gGraphicsData.aViewRenderLists )
			for ( auto& [ handle, vec ] : viewList.aRenderLists )
				vec.clear();

		gGraphicsData.aViewRenderLists.resize( gGraphicsData.aViewData.aViewports.size() );
	}

	Graphics_UpdateDebugDraw();

#if 1
	ChHandle_t              shaderSkinning     = gGraphics.GetShader( "__skinning" );
	ShaderData_t*           shaderSkinningData = Shader_GetData( shaderSkinning );
#endif

	if ( ImGui::Button( "Reset Blend Shapes" ) )
		r_reset_blend_shapes.SetValue( 1 );

	u32 surfDrawIndex = 0;

	u32 imguiIndex    = 0;
	for ( uint32_t i = 0; i < gGraphicsData.aRenderables.size(); )
	{
		PROF_SCOPE_NAMED( "Update Renderables" );

		Renderable_t* renderable = nullptr;
		if ( !gGraphicsData.aRenderables.Get( gGraphicsData.aRenderables.aHandles[ i ], &renderable ) )
		{
			Log_Warn( gLC_ClientGraphics, "Renderable handle is invalid!\n" );
			gGraphicsData.aRenderables.Remove( gGraphicsData.aRenderables.aHandles[ i ] );
			continue;
		}

		if ( !renderable->aVisible )
		{
			i++;
			continue;
		}

		Model* model = gGraphics.GetModelData( renderable->aModel );
		if ( !model )
		{
			Log_Warn( gLC_ClientGraphics, "Renderable has no model!\n" );
			gGraphicsData.aRenderables.Remove( gGraphicsData.aRenderables.aHandles[ i ] );
			continue;
		}

		// update data on gpu
		// NOTE: we actually use the handle index for this and not the allocator
		// if this works well, we could just get rid of the allocator entirely and use handle indexes
		u32 renderIndex = CH_GET_HANDLE_INDEX( gGraphicsData.aRenderables.aHandles[ i ] );

		if ( renderIndex >= CH_R_MAX_RENDERABLES )
		{
			Log_WarnF( gLC_ClientGraphics, "Renderable Index %zd is greater than max shader renderable count of %zd\n", renderIndex, CH_R_MAX_RENDERABLES );
			i++;
			continue;
		}

		Shader_Renderable_t& shaderRenderable         = gGraphicsData.aRenderableData[ renderIndex ];

		// write model matrix, and vertex/index buffer indexes
		gGraphicsData.aModelMatrixData[ renderIndex ] = renderable->aModelMatrix;

		// update light lists

		if ( visLocked )
		{
			i++;
			continue;
		}

		// check if we need this in any views
		bool isVisible = false;
		for ( size_t viewIndex = 0; viewIndex < gGraphicsData.aViewData.aViewports.size(); viewIndex++ )
		{
			PROF_SCOPE_NAMED( "Viewport Testing" );

			// HACK: kind of of hack with the shader override check
			// If we don't want to cast a shadow and are in a shadowmap view, don't add to the view's render list
			if ( !renderable->aCastShadow && gGraphicsData.aViewData.aViewports[ viewIndex ].aShaderOverride )
				continue;

			// Is this model visible in this view?
			if ( !Graphics_ViewFrustumTest( renderable, viewIndex ) )
				continue;

			isVisible                  = true;
			ViewRenderList_t& viewList = gGraphicsData.aViewRenderLists[ viewIndex ];

			// Add each surface to the shader draw list
			for ( uint32_t surf = 0; surf < model->aMeshes.size(); surf++ )
			{
				Handle mat = model->aMeshes[ surf ].aMaterial;

				// TODO: add Mat_IsValid()
				if ( mat == InvalidHandle )
				{
					Log_ErrorF( gLC_ClientGraphics, "Model part \"%d\" has no material!\n", surf );
					// gModelDrawList.remove( i );
					continue;
				}

				// Handle shader = InvalidHandle;
				Handle shader = gGraphicsData.aViewData.aViewports[ viewIndex ].aShaderOverride;

				if ( !shader )
					shader = gGraphics.Mat_GetShader( mat );

				ShaderData_t* shaderData = Shader_GetData( shader );
				if ( !shaderData )
					continue;

				// add a SurfaceDraw_t to this render list
				SurfaceDraw_t& surfDraw  = gGraphicsData.aViewRenderLists[ viewIndex ].aRenderLists[ shader ].emplace_back();
				surfDraw.aRenderable     = gGraphicsData.aRenderables.aHandles[ i ];
				surfDraw.aSurface        = surf;
				surfDraw.aShaderSlot     = surfDrawIndex++;

				Shader_SetupRenderableDrawData( renderIndex, viewIndex, renderable, shaderData, surfDraw );

				if ( !renderable->aCastShadow )
					continue;

				// if ( shaderData->aFlags & EShaderFlags_Lights && usingShadow && shadowShaderData )
				// 	Shader_SetupRenderableDrawData( renderable, shadowShaderData, renderable );
				
				if ( !shaderData->apMaterialIndex )
					continue;

				// shaderSurfDraw.aMaterial = shaderData->apMaterialIndex( surfIndex, renderable, surfDraw );
			}
		}

		if ( isVisible && r_random_blend_shapes && renderable->aBlendShapeWeights.size() )
		{
			gGraphicsData.aSkinningRenderList.emplace( gGraphicsData.aRenderables.aHandles[ i ] );

			// Graphics_RenderableBlendShapesDirty;
			for ( u32 blendI = 0; blendI < renderable->aBlendShapeWeights.size(); blendI++ )
			{
				if ( r_reset_blend_shapes.GetBool() )
					renderable->aBlendShapeWeights[ blendI ] = 0.f;

				// renderable->aBlendShapeWeights[ blendI ] = RandomFloat( 0.f, 1.f );
				ImGui::PushID( imguiIndex++ );
				ImGui::SliderFloat( "##blend_shape", &renderable->aBlendShapeWeights[ blendI ], -1.f, 4.f, "%.4f", 1.f );
				ImGui::PopID();
				//renderable->aBlendShapeWeights[ blendI ] = RandomFloat( 0.f, 1.f );
			}
		}

		i++;
	}

	// --------------------------------------------------------------------
	// Prepare Skinning Compute Shader Buffers

	r_reset_blend_shapes.SetValue( 0 );

#if 1
	u32 r = 0;
	for ( ChHandle_t renderHandle : gGraphicsData.aSkinningRenderList )
	{
		Renderable_t* renderable = nullptr;
		if ( !gGraphicsData.aRenderables.Get( renderHandle, &renderable ) )
		{
			Log_Warn( gLC_ClientGraphics, "Renderable does not exist!\n" );
			continue;
		}

		r++;
		render->BufferWrite( renderable->aBlendShapeWeightsBuffer, renderable->aBlendShapeWeights.size_bytes(), renderable->aBlendShapeWeights.data() );
	}
#endif

	// --------------------------------------------------------------------
	// Update Renderables on the GPU and Calculate Light Lists

	Graphics_UpdateShaderRenderableData();

	// --------------------------------------------------------------------
	// Update Shader Draw Data
	// TODO: can this be merged into the above for loop for viewports and renderables?

#if 0
	for ( size_t viewIndex = 0; viewIndex < gGraphicsData.aViewData.aViewports.size(); viewIndex++ )
	{
		PROF_SCOPE_NAMED( "Update Shader Draw Data" );

		ViewRenderList_t& viewList = gGraphicsData.aViewRenderLists[ viewIndex ];

		for ( auto& [ shader, modelList ] : viewList.aRenderLists )
		{
			ShaderData_t* shaderData = Shader_GetData( shader );
			if ( !shaderData )
				continue;

			for ( auto& renderable : modelList )
			{
				Renderable_t* renderable = nullptr;
				if ( !gGraphicsData.aRenderables.Get( renderable.aDrawData, &renderable ) )
				{
					Log_Warn( gLC_ClientGraphics, "Draw Data does not exist for renderable!\n" );
					continue;
				}

				u64 renderableIndex = CH_GET_HANDLE_INDEX( renderable.aDrawData );
				Shader_SetupRenderableDrawData( renderableIndex, viewIndex, renderable, shaderData, renderable );

				if ( !renderable->aCastShadow )
					continue;

				// if ( shaderData->aFlags & EShaderFlags_Lights && usingShadow && shadowShaderData )
				// 	Shader_SetupRenderableDrawData( renderable, shadowShaderData, renderable );
			}
		}
	}
#endif

	// Update Core Data SSBO
#if 0
	if ( gGraphicsData.aCoreDataStaging.aDirty )
	{
		// Update Viewports (this looks stupid)
		for ( u32 i = 0; i < gGraphicsData.aViewData.aViewports.size(); i++ )
		{
			ViewportShader_t&  viewport       = gGraphicsData.aViewData.aViewports[ i ];
			Shader_Viewport_t& viewportBuffer = gGraphicsData.aViewportData[ i ];

			viewportBuffer.aProjView          = viewport.aProjView;
			viewportBuffer.aProjection        = viewport.aProjection;
			viewportBuffer.aView              = viewport.aView;
			viewportBuffer.aViewPos           = viewport.aViewPos;
			viewportBuffer.aNearZ             = viewport.aNearZ;
			viewportBuffer.aFarZ              = viewport.aFarZ;
		}

		// Update Renderable Vertex Buffer Handles
		for ( u32 i = 0; i < gGraphicsData.aRenderables.size(); )
		{
			Renderable_t* renderable = nullptr;
			if ( !gGraphicsData.aRenderables.Get( gGraphicsData.aRenderables.aHandles[ i ], &renderable ) )
			{
				Log_Warn( gLC_ClientGraphics, "Renderable handle is invalid!\n" );
				gGraphicsData.aRenderables.Remove( gGraphicsData.aRenderables.aHandles[ i ] );
				continue;
			}

			if ( !renderable->aVisible )
			{
				i++;
				continue;
			}

			gGraphicsData.aRenderableData[ i ].aVertexBuffer = Graphics_GetShaderBufferIndex( gGraphicsData.aVertexBuffers, renderable->aVertexIndex );
			gGraphicsData.aRenderableData[ i ].aIndexBuffer  = Graphics_GetShaderBufferIndex( gGraphicsData.aIndexBuffers, renderable->aIndexHandle );

			i++;
		}

		gGraphicsData.aCoreDataStaging.aDirty = false;
		render->BufferWrite( gGraphicsData.aCoreDataStaging.aStagingBuffer, sizeof( Buffer_Core_t ), &gGraphicsData.aCoreData );

		BufferRegionCopy_t copy;
		copy.aSrcOffset = 0;
		copy.aDstOffset = 0;
		copy.aSize      = sizeof( Buffer_Core_t );

		render->BufferCopyQueued( gGraphicsData.aCoreDataStaging.aStagingBuffer, gGraphicsData.aCoreDataStaging.aBuffer, &copy, 1 );
	}
#endif

	// Update Viewport SSBO
	{
		// this looks stupid
		for ( u32 i = 0; i < gGraphicsData.aViewData.aViewports.size(); i++ )
		{
			ViewportShader_t&  viewport       = gGraphicsData.aViewData.aViewports[ i ];
			Shader_Viewport_t& viewportBuffer = gGraphicsData.aViewportData[ i ];

			viewportBuffer.aProjView          = viewport.aProjView;
			viewportBuffer.aProjection        = viewport.aProjection;
			viewportBuffer.aView              = viewport.aView;
			viewportBuffer.aViewPos           = viewport.aViewPos;
			viewportBuffer.aNearZ             = viewport.aNearZ;
			viewportBuffer.aFarZ              = viewport.aFarZ;
		}

		BufferRegionCopy_t copy;
		copy.aSrcOffset = 0;
		copy.aDstOffset = 0;
		copy.aSize      = sizeof( Shader_Viewport_t ) * CH_R_MAX_VIEWPORTS;

		render->BufferWrite( gGraphicsData.aViewportStaging.aStagingBuffer, copy.aSize, gGraphicsData.aViewportData );
		render->BufferCopyQueued( gGraphicsData.aViewportStaging.aStagingBuffer, gGraphicsData.aViewportStaging.aBuffer, &copy, 1 );
	}

	// Update Renderables SSBO
	// if ( gGraphicsData.aRenderableStaging.aDirty )
	if ( updateShaderRenderables )
	{
		// Update Renderable Vertex Buffer Handles
		for ( u32 i = 0; i < gGraphicsData.aRenderables.size(); )
		{
			Renderable_t* renderable = nullptr;
			if ( !gGraphicsData.aRenderables.Get( gGraphicsData.aRenderables.aHandles[ i ], &renderable ) )
			{
				Log_Warn( gLC_ClientGraphics, "Renderable handle is invalid!\n" );
				gGraphicsData.aRenderables.Remove( gGraphicsData.aRenderables.aHandles[ i ] );
				continue;
			}

			if ( !renderable->aVisible )
			{
				i++;
				continue;
			}

			gGraphicsData.aRenderableData[ i ].aVertexBuffer = Graphics_GetShaderBufferIndex( gGraphicsData.aVertexBuffers, renderable->aVertexIndex );
			gGraphicsData.aRenderableData[ i ].aIndexBuffer  = Graphics_GetShaderBufferIndex( gGraphicsData.aIndexBuffers, renderable->aIndexHandle );

			i++;
		}

		BufferRegionCopy_t copy;
		copy.aSrcOffset = 0;
		copy.aDstOffset = 0;
		copy.aSize      = sizeof( Shader_Renderable_t ) * CH_R_MAX_RENDERABLES;

		render->BufferWrite( gGraphicsData.aRenderableStaging.aStagingBuffer, copy.aSize, gGraphicsData.aRenderableData );
		render->BufferCopyQueued( gGraphicsData.aRenderableStaging.aStagingBuffer, gGraphicsData.aRenderableStaging.aBuffer, &copy, 1 );
	}

	// Update Model Matrices SSBO
	// if ( gGraphicsData.aRenderableStaging.aDirty )
	{
		BufferRegionCopy_t copy;
		copy.aSrcOffset = 0;
		copy.aDstOffset = 0;
		copy.aSize      = sizeof( glm::mat4 ) * CH_R_MAX_RENDERABLES;

		render->BufferWrite( gGraphicsData.aModelMatrixStaging.aStagingBuffer, copy.aSize, gGraphicsData.aModelMatrixData );
		render->BufferCopyQueued( gGraphicsData.aModelMatrixStaging.aStagingBuffer, gGraphicsData.aModelMatrixStaging.aBuffer, &copy, 1 );
	}

	// Update Shader Draws Data SSBO
	// if ( gGraphicsData.aSurfaceDrawsStaging.aDirty )
	// {
	// 	gGraphicsData.aSurfaceDrawsStaging.aDirty = false;
	// 	render->BufferWrite( gGraphicsData.aSurfaceDrawsStaging.aStagingBuffer, sizeof( gGraphicsData.aSurfaceDraws ), &gGraphicsData.aSurfaceDraws );
	// 	render->BufferCopy( gGraphicsData.aSurfaceDrawsStaging.aStagingBuffer, gGraphicsData.aSurfaceDrawsStaging.aBuffer, sizeof( gGraphicsData.aSurfaceDraws ) );
	// }

	render->CopyQueuedBuffers();

	{
		PROF_SCOPE_NAMED( "Imgui Render" );
		ImGui::Render();
	}
}


void Graphics::Present()
{
	PROF_SCOPE();

	// render->LockGraphicsMutex();
	render->WaitForQueues();
	render->ResetCommandPool();

	Graphics_FreeQueuedResources();

	Graphics_PrepareDrawData();

	ChHandle_t* commandBuffers     = gGraphicsData.aCommandBuffers;
	size_t      commandBufferCount = gGraphicsData.aCommandBufferCount;

	u32         imageIndex         = render->GetFlightImageIndex();

	// For each framebuffer, begin a primary
	// command buffer, and record the commands.
	// for ( size_t cmdIndex = 0; cmdIndex < commandBufferCount; cmdIndex++ )
	{
		size_t cmdIndex = imageIndex;
		PROF_SCOPE_NAMED( "Primary Command Buffer" );

		ChHandle_t c = commandBuffers[ cmdIndex ];

		render->BeginCommandBuffer( c );

		// Animate Materials in a Compute Shader
		// Run Skinning Compute Shader
		Graphics_DoSkinning( c, cmdIndex );

		// Draw Shadow Maps
		Graphics_DrawShadowMaps( c, cmdIndex );

		// ----------------------------------------------------------
		// Main RenderPass

		RenderPassBegin_t renderPassBegin{};
		renderPassBegin.aRenderPass  = gGraphicsData.aRenderPassGraphics;
		renderPassBegin.aFrameBuffer = gGraphicsData.aBackBuffer[ cmdIndex ];
		renderPassBegin.aClear.resize( 2 );
		renderPassBegin.aClear[ 0 ].aColor   = { 0.f, 0.f, 0.f, 0.f };
		renderPassBegin.aClear[ 0 ].aIsDepth = false;
		renderPassBegin.aClear[ 1 ].aColor   = { 0.f, 0.f, 0.f, 1.f };
		renderPassBegin.aClear[ 1 ].aIsDepth = true;

		render->BeginRenderPass( c, renderPassBegin );  // VK_SUBPASS_CONTENTS_INLINE

		Graphics_Render( c, cmdIndex, ERenderPass_Graphics );

		// Run Bloom Compute Shader

		render->DrawImGui( ImGui::GetDrawData(), c );

		render->EndRenderPass( c );

		render->EndCommandBuffer( c );
	}

	render->Present( imageIndex );
	// render->UnlockGraphicsMutex();
}


ChHandle_t Graphics_CreateRenderPass( EDescriptorType sType )
{
	RenderPassData_t passData{};

	return CH_INVALID_HANDLE;
}


#if 0
void Graphics_UpdateRenderPass( ChHandle_t sRenderPass )
{
	// update the descriptor sets
	WriteDescSet_t update{};

	update.aDescSets.push_back( gShaderDescriptorData.aPerPassSets[ sRenderPass ][ 0 ] );
	update.aDescSets.push_back( gShaderDescriptorData.aPerPassSets[ sRenderPass ][ 1 ] );

	update.aType    = EDescriptorType_StorageBuffer;
	update.aBuffers = gViewportBuffers;
	render->UpdateDescSet( update );
}


void Graphics_UpdateRenderPassBuffers( ERenderPass sRenderPass )
{
	// update the descriptor sets
	WriteDescSet_t update{};

	update.aDescSets.push_back( gShaderDescriptorData.aPerPassSets[ sRenderPass ][ 0 ] );
	update.aDescSets.push_back( gShaderDescriptorData.aPerPassSets[ sRenderPass ][ 1 ] );

	update.aType    = EDescriptorType_StorageBuffer;
	update.aBuffers = gViewportBuffers;
	render->UpdateDescSet( update );
}
#endif


u32 Graphics::CreateViewport( ViewportShader_t** spViewport )
{
	u32 index = Graphics_AllocateShaderSlot( gGraphicsData.aViewportSlots, "Viewports" );

	if ( index == UINT32_MAX )
	{
		Log_Error( gLC_ClientGraphics, "Failed to allocate viewport\n" );
		return index;
	}

	if ( index + 1 > gGraphicsData.aViewData.aViewports.size() )
		gGraphicsData.aViewData.aViewports.resize( index + 1 );

	if ( spViewport )
	{
		( *spViewport )             = &gGraphicsData.aViewData.aViewports[ index ];
		( *spViewport )->aAllocated = true;
		( *spViewport )->aActive    = true;
	}
	else
	{
		gGraphicsData.aViewData.aViewports[ index ].aAllocated = true;
	}

	return index;
}


void Graphics::FreeViewport( u32 sViewportIndex )
{
	Graphics_FreeShaderSlot( gGraphicsData.aViewportSlots, "Viewports", sViewportIndex );

	if ( sViewportIndex >= gGraphicsData.aViewData.aViewports.size() )
	{
		Log_ErrorF( gLC_ClientGraphics, "Invalid Viewport Index to Free, only %zd allocated, tried to free slot %zd\n",
		            gGraphicsData.aViewData.aViewports.size(), sViewportIndex );

		return;
	}

	memset( &gGraphicsData.aViewData.aViewports[ sViewportIndex ], 0, sizeof( ViewportShader_t ) );
}


ViewportShader_t* Graphics::GetViewportData( u32 sViewportIndex )
{
	if ( sViewportIndex >= gGraphicsData.aViewData.aViewports.size() )
	{
		Log_ErrorF( "Invalid Viewport Index: %zd, only allocated %zd\n", sViewportIndex, gGraphicsData.aViewData.aViewports.size() );
		return nullptr;
	}

	ViewportShader_t* viewport = &gGraphicsData.aViewData.aViewports[ sViewportIndex ];

	if ( !viewport->aAllocated )
		return nullptr;

	return viewport;
}


void Graphics::SetViewportUpdate( bool sUpdate )
{
	gGraphicsData.aCoreDataStaging.aDirty = sUpdate;
}


void Graphics_PushViewInfo( const ViewportShader_t& srViewInfo )
{
	// gViewportStack.push( srViewInfo );
}


void Graphics_PopViewInfo()
{
	// if ( gViewportStack.empty() )
	// {
	// 	Log_Error( "Misplaced View Info Pop!\n" );
	// 	return;
	// }
	// 
	// gViewportStack.pop();
}


// ViewportShader_t& Graphics_GetViewInfo()
// {
// 	// if ( gViewportStack.empty() )
// 	// 	return gViewport[ 0 ];
// 	// 
// 	// return gViewportStack.top();
// }

