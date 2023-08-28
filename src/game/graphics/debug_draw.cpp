#include "graphics.h"
#include "graphics_int.h"
#include "debug_draw.h"
#include "types/transform.h"

// --------------------------------------------------------------------------------------
// Debug Drawing


// static MeshBuilder                               gDebugLineBuilder;
static Handle                   gDebugLineModel    = InvalidHandle;
static Handle                   gDebugLineDraw     = InvalidHandle;
static Handle                   gDebugLineMaterial = InvalidHandle;
ChVector< Shader_VertexData_t > gDebugLineVerts;
static size_t                   gDebugLineBufferSize = 0;


// --------------------------------------------------------------------------------------

CONVAR( r_debug_draw, 0 );
CONVAR( r_debug_aabb, 0 );
CONVAR( r_debug_frustums, 0 );
CONVAR( r_debug_normals, 0 );
CONVAR( r_debug_normals_len, 8 );
CONVAR( r_debug_normals_len_face, 8 );


// bool& r_debug_draw = Con_RegisterConVar_Bool( "r_debug_draw", "Enable or Disable All Debug Drawing", true );


// --------------------------------------------------------------------------------------


bool Graphics_DebugDrawInit()
{
	gDebugLineMaterial = Graphics_CreateMaterial( "__debug_line_mat", Graphics_GetShader( "debug_line" ) );
	return gDebugLineMaterial;
}


void Graphics_DebugDrawNewFrame()
{
	PROF_SCOPE();

	gDebugLineVerts.clear();

	if ( !r_debug_draw )
	{
		if ( gDebugLineModel )
		{
			Graphics_FreeModel( gDebugLineModel );
			gDebugLineModel = InvalidHandle;
		}

		if ( gDebugLineDraw )
		{
			Renderable_t* renderable = Graphics_GetRenderableData( gDebugLineDraw );

			if ( !renderable )
				return;

			// Graphics_FreeModel( renderable->aModel );
			Graphics_FreeRenderable( gDebugLineDraw );
			gDebugLineDraw = InvalidHandle;
		}

		return;
	}

	if ( !gDebugLineModel )
	{
		Model* model    = nullptr;
		gDebugLineModel = Graphics_CreateModel( &model );

		if ( !gDebugLineModel )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create Debug Line Model\n" );
			return;
		}

		model->aMeshes.resize( 1 );

		model->apVertexData = new VertexData_t;
		model->apBuffers    = new ModelBuffers_t;

		// gpDebugLineModel->apBuffers->aVertex.resize( 2, true );
		model->apVertexData->aData.resize( 2, true );
		model->apVertexData->aData[ 0 ].aAttrib = VertexAttribute_Position;
		model->apVertexData->aData[ 1 ].aAttrib = VertexAttribute_Color;

		Model_SetMaterial( gDebugLineModel, 0, gDebugLineMaterial );

		if ( gDebugLineDraw )
		{
			if ( Renderable_t* renderable = Graphics_GetRenderableData( gDebugLineDraw ) )
			{
				renderable->aModel = gDebugLineDraw;
			}
		}
	}

	if ( !gDebugLineDraw )
	{
		gDebugLineDraw = Graphics_CreateRenderable( gDebugLineModel );

		if ( !gDebugLineDraw )
			return;

		Renderable_t* renderable = Graphics_GetRenderableData( gDebugLineDraw );

		if ( !renderable )
			return;

		renderable->aTestVis    = false;
		renderable->aCastShadow = false;
		renderable->aVisible    = true;
	}
}


// Update Debug Draw Buffers
// TODO: replace this system with instanced drawing
void Graphics_UpdateDebugDraw()
{
	PROF_SCOPE();

	if ( !r_debug_draw )
		return;

	if ( r_debug_aabb || r_debug_normals )
	{
		ViewRenderList_t& viewList   = gGraphicsData.aViewRenderLists[ 0 ];

		glm::mat4         lastMatrix = glm::mat4( 0.f );
		glm::mat4         invMatrix  = glm::mat4( 1.f );

		for ( auto& [ shader, modelList ] : viewList.aRenderLists )
		{
			for ( SurfaceDraw_t& surfaceDraw : modelList )
			{
				// hack to not draw this AABB multiple times, need to change this render list system
				if ( surfaceDraw.aSurface == 0 )
				{
					// Graphics_DrawModelAABB( renderable->apDraw );

					Renderable_t* renderable = Graphics_GetRenderableData( surfaceDraw.aRenderable );

					if ( !renderable )
					{
						Log_Warn( gLC_ClientGraphics, "Draw Data does not exist for renderable!\n" );
						return;
					}

					if ( r_debug_aabb )
						Graphics_DrawBBox( renderable->aAABB.aMin, renderable->aAABB.aMax, { 1.0, 0.5, 1.0 } );

					if ( r_debug_normals )
					{
						// slight hack
						if ( lastMatrix != renderable->aModelMatrix )
						{
							lastMatrix = renderable->aModelMatrix;
							invMatrix  = glm::inverse( renderable->aModelMatrix );
						}

						Graphics_DrawNormals( renderable->aModel, invMatrix );
					}

					// ModelBBox_t& bbox = gModelBBox[ renderable->apDraw->aModel ];
					// Graphics_DrawBBox( bbox.aMin, bbox.aMax, { 1.0, 0.5, 1.0 } );
				}
			}
		}
	}

	Renderable_t* renderable = Graphics_GetRenderableData( gDebugLineDraw );

	if ( !renderable )
		return;

	if ( gDebugLineModel && gDebugLineVerts.size() )
	{
		renderable->aVisible = true;
		// Mesh& mesh = gDebugLineModel->aMeshes[ 0 ];

		Model* model = nullptr;
		if ( !gGraphicsData.aModels.Get( gDebugLineModel, &model ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to get Debug Draw Model!\n" );
			gDebugLineModel = InvalidHandle;
			return;
		}

		if ( !model->apVertexData )
			model->apVertexData = new VertexData_t;

		if ( !model->apBuffers )
			model->apBuffers = new ModelBuffers_t;

		// Is our current buffer size too small? If so, free the old ones
		if ( gDebugLineVerts.size() > gDebugLineBufferSize )
		{
			if ( model->apBuffers && model->apBuffers->aVertex != CH_INVALID_HANDLE )
			{
				render->DestroyBuffer( model->apBuffers->aVertex );
				model->apBuffers->aVertex = CH_INVALID_HANDLE;
			}

			gDebugLineBufferSize = gDebugLineVerts.size();
		}

		size_t bufferSize = sizeof( Shader_VertexData_t ) * gDebugLineVerts.size();

		// Create new Buffers if needed
		if ( model->apBuffers->aVertex == CH_INVALID_HANDLE )
			model->apBuffers->aVertex = render->CreateBuffer( "DebugLine Vertex", bufferSize, EBufferFlags_Vertex, EBufferMemory_Host );

		model->apVertexData->aCount = gDebugLineVerts.size();

		if ( model->aMeshes.empty() )
			model->aMeshes.resize( 1 );

		model->aMeshes[ 0 ].aIndexCount   = 0;
		model->aMeshes[ 0 ].aVertexOffset = 0;
		model->aMeshes[ 0 ].aVertexCount  = gDebugLineVerts.size();

		// Update the Buffers
		render->BufferWrite( model->apBuffers->aVertex, bufferSize, gDebugLineVerts.data() );
	}
	else
	{
		renderable->aVisible = false;
	}
}


// ---------------------------------------------------------------------------------------
// Debug Rendering Functions


void Graphics_DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec3& sColor )
{
	PROF_SCOPE();

	if ( !r_debug_draw || !gDebugLineModel )
		return;

	size_t index = gDebugLineVerts.size();
	gDebugLineVerts.resize( gDebugLineVerts.size() + 2 );

#if 0
	gDebugLineVerts[ index ].aPos     = sX;
	gDebugLineVerts[ index ].aColor.x = sColor.x;
	gDebugLineVerts[ index ].aColor.y = sColor.y;
	gDebugLineVerts[ index ].aColor.z = sColor.z;

	index++;
	gDebugLineVerts[ index ].aPos     = sY;
	gDebugLineVerts[ index ].aColor.x = sColor.x;
	gDebugLineVerts[ index ].aColor.y = sColor.y;
	gDebugLineVerts[ index ].aColor.z = sColor.z;

#else

	gDebugLineVerts[ index ].aPosNormX.x = sX.x;
	gDebugLineVerts[ index ].aPosNormX.y = sX.y;
	gDebugLineVerts[ index ].aPosNormX.z = sX.z;

	gDebugLineVerts[ index ].aColor.x = sColor.x;
	gDebugLineVerts[ index ].aColor.y = sColor.y;
	gDebugLineVerts[ index ].aColor.z = sColor.z;

	index++;
	gDebugLineVerts[ index ].aPosNormX.x = sY.x;
	gDebugLineVerts[ index ].aPosNormX.y = sY.y;
	gDebugLineVerts[ index ].aPosNormX.z = sY.z;

	gDebugLineVerts[ index ].aColor.x = sColor.x;
	gDebugLineVerts[ index ].aColor.y = sColor.y;
	gDebugLineVerts[ index ].aColor.z = sColor.z;
#endif
}


void Graphics_DrawLine( const glm::vec3& sX, const glm::vec3& sY, const glm::vec4& sColor )
{
	PROF_SCOPE();

	if ( !r_debug_draw || !gDebugLineModel )
		return;

	size_t index = gDebugLineVerts.size();
	gDebugLineVerts.resize( gDebugLineVerts.size() + 2 );

	gDebugLineVerts[ index ].aPosNormX.x = sX.x;
	gDebugLineVerts[ index ].aPosNormX.y = sX.y;
	gDebugLineVerts[ index ].aPosNormX.z = sX.z;
	gDebugLineVerts[ index ].aColor      = sColor;

	index++;
	gDebugLineVerts[ index ].aPosNormX.x = sY.x;
	gDebugLineVerts[ index ].aPosNormX.y = sY.y;
	gDebugLineVerts[ index ].aPosNormX.z = sY.z;
	gDebugLineVerts[ index ].aColor      = sColor;
}


CONVAR( r_debug_axis_scale, 1 );


void Graphics_DrawAxis( const glm::vec3& sPos, const glm::vec3& sAng, const glm::vec3& sScale )
{
	if ( !r_debug_draw || !gDebugLineModel )
		return;

	glm::vec3 forward, right, up;
	Util_GetDirectionVectors( sAng, &forward, &right, &up );

	Graphics_DrawLine( sPos, sPos + ( forward * sScale.x * r_debug_axis_scale.GetFloat() ), { 1.f, 0.f, 0.f } );
	Graphics_DrawLine( sPos, sPos + ( right * sScale.y * r_debug_axis_scale.GetFloat() ), { 0.f, 1.f, 0.f } );
	Graphics_DrawLine( sPos, sPos + ( up * sScale.z * r_debug_axis_scale.GetFloat() ), { 0.f, 0.f, 1.f } );
}


void Graphics_DrawBBox( const glm::vec3& sMin, const glm::vec3& sMax, const glm::vec3& sColor )
{
	PROF_SCOPE();

	if ( !r_debug_draw || !gDebugLineModel )
		return;

	gDebugLineVerts.reserve( gDebugLineVerts.size() + 24 );

	// bottom
	Graphics_DrawLine( sMin, glm::vec3( sMax.x, sMin.y, sMin.z ), sColor );
	Graphics_DrawLine( sMin, glm::vec3( sMin.x, sMax.y, sMin.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMin.x, sMax.y, sMin.z ), glm::vec3( sMax.x, sMax.y, sMin.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMax.x, sMin.y, sMin.z ), glm::vec3( sMax.x, sMax.y, sMin.z ), sColor );

	// top
	Graphics_DrawLine( sMax, glm::vec3( sMin.x, sMax.y, sMax.z ), sColor );
	Graphics_DrawLine( sMax, glm::vec3( sMax.x, sMin.y, sMax.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMax.x, sMin.y, sMax.z ), glm::vec3( sMin.x, sMin.y, sMax.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMin.x, sMax.y, sMax.z ), glm::vec3( sMin.x, sMin.y, sMax.z ), sColor );

	// sides
	Graphics_DrawLine( sMin, glm::vec3( sMin.x, sMin.y, sMax.z ), sColor );
	Graphics_DrawLine( sMax, glm::vec3( sMax.x, sMax.y, sMin.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMax.x, sMin.y, sMin.z ), glm::vec3( sMax.x, sMin.y, sMax.z ), sColor );
	Graphics_DrawLine( glm::vec3( sMin.x, sMax.y, sMin.z ), glm::vec3( sMin.x, sMax.y, sMax.z ), sColor );
}


void Graphics_DrawProjView( const glm::mat4& srProjView )
{
	PROF_SCOPE();

	if ( !r_debug_draw || !gDebugLineModel )
		return;

	glm::mat4 inv = glm::inverse( srProjView );

	// Calculate Frustum Points
	glm::vec3 v[ 8u ];
	for ( int i = 0; i < 8; i++ )
	{
		glm::vec4 ff = inv * gFrustumFaceData[ i ];
		v[ i ].x     = ff.x / ff.w;
		v[ i ].y     = ff.y / ff.w;
		v[ i ].z     = ff.z / ff.w;
	}

	gDebugLineVerts.reserve( gDebugLineVerts.size() + 24 );

	Graphics_DrawLine( v[ 0 ], v[ 1 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 0 ], v[ 2 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 3 ], v[ 1 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 3 ], v[ 2 ], glm::vec3( 1, 1, 1 ) );

	Graphics_DrawLine( v[ 4 ], v[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 4 ], v[ 6 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 7 ], v[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 7 ], v[ 6 ], glm::vec3( 1, 1, 1 ) );

	Graphics_DrawLine( v[ 0 ], v[ 4 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 1 ], v[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 3 ], v[ 7 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( v[ 2 ], v[ 6 ], glm::vec3( 1, 1, 1 ) );
}


void Graphics_DrawFrustum( const Frustum_t& srFrustum )
{
	PROF_SCOPE();

	if ( !r_debug_draw || !gDebugLineModel || !r_debug_frustums )
		return;

	gDebugLineVerts.reserve( gDebugLineVerts.size() + 24 );

	Graphics_DrawLine( srFrustum.aPoints[ 0 ], srFrustum.aPoints[ 1 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 0 ], srFrustum.aPoints[ 2 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 3 ], srFrustum.aPoints[ 1 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 3 ], srFrustum.aPoints[ 2 ], glm::vec3( 1, 1, 1 ) );

	Graphics_DrawLine( srFrustum.aPoints[ 4 ], srFrustum.aPoints[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 4 ], srFrustum.aPoints[ 6 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 7 ], srFrustum.aPoints[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 7 ], srFrustum.aPoints[ 6 ], glm::vec3( 1, 1, 1 ) );

	Graphics_DrawLine( srFrustum.aPoints[ 0 ], srFrustum.aPoints[ 4 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 1 ], srFrustum.aPoints[ 5 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 3 ], srFrustum.aPoints[ 7 ], glm::vec3( 1, 1, 1 ) );
	Graphics_DrawLine( srFrustum.aPoints[ 2 ], srFrustum.aPoints[ 6 ], glm::vec3( 1, 1, 1 ) );
}


void Graphics_DrawNormals( Handle sModel, const glm::mat4& srMatrix )
{
	PROF_SCOPE();

	if ( !r_debug_draw || !gDebugLineModel || !r_debug_normals )
		return;

	Model*    model = Graphics_GetModelData( sModel );

	// TODO: use this for physics materials later on
	for ( size_t s = 0; s < model->aMeshes.size(); s++ )
	{
		Mesh&      mesh     = model->aMeshes[ s ];

		auto&      vertData = model->apVertexData;

		glm::vec3* pos      = nullptr;
		glm::vec3* normals  = nullptr;

		for ( auto& attrib : vertData->aData )
		{
			if ( attrib.aAttrib == VertexAttribute_Position )
			{
				pos = static_cast< glm::vec3* >( attrib.apData );
			}
			if ( attrib.aAttrib == VertexAttribute_Normal )
			{
				normals = static_cast< glm::vec3* >( attrib.apData );
			}

			if ( pos && normals )
				break;
		}

		if ( pos == nullptr || normals == nullptr )
		{
			// Log_Error( "Graphics_DrawNormals(): Position Vertex Data not found?\n" );
			return;
		}

		gDebugLineVerts.reserve( gDebugLineVerts.size() + ( mesh.aIndexCount * 3 ) );

		u32 j = 0;
		for ( u32 i = 0; i < mesh.aIndexCount; )
		{
			u32       idx[ 3 ];
			glm::vec3 v[ 3 ];
			glm::vec3 n[ 3 ];

			idx[ 0 ] = vertData->aIndices[ mesh.aIndexOffset + i++ ];
			idx[ 1 ] = vertData->aIndices[ mesh.aIndexOffset + i++ ];
			idx[ 2 ] = vertData->aIndices[ mesh.aIndexOffset + i++ ];

			v[ 0 ]   = pos[ idx[ 0 ] ];
			v[ 1 ]   = pos[ idx[ 1 ] ];
			v[ 2 ]   = pos[ idx[ 2 ] ];

			n[ 0 ]   = normals[ idx[ 0 ] ];
			n[ 1 ]   = normals[ idx[ 1 ] ];
			n[ 2 ]   = normals[ idx[ 2 ] ];

			// Calculate face normal
			glm::vec3 normal = glm::cross( ( v[ 1 ] - v[ 0 ] ), ( v[ 2 ] - v[ 0 ] ) );
			float     len    = glm::length( normal );

			// Make sure we don't have any 0 lengths
			if ( len == 0.f )
			{
				// Log_Warn( "Graphics_DrawNormals(): Face Normal of 0?\n" );
				continue;
			}

			// Draw Vertex Normals
			for ( int vi = 0; vi < 3; vi++ )
			{
				glm::vec4 v4( v[ vi ], 1 );
				glm::vec4 n4( n[ vi ], 1 );

				glm::vec3   posX    = v4 * srMatrix;
				glm::vec3   normMat = n4 * srMatrix;

				// glm::vec3 forward;
				// Util_GetDirectionVectors( )

				glm::vec3 posY = posX + ( normMat * r_debug_normals_len.GetFloat() );

				// protoTransform.aPos, protoTransform.aPos + ( forward * r_proto_line_dist2.GetFloat() )

				Graphics_DrawLine( posX, posY, {0.9, 0.1, 0.1, 1.f} );
			}

			// Draw Face Normal
			// glm::vec4 normal4( normal, 1 );
			// glm::vec3 posX = normal4 * srMatrix;
			// glm::vec3 posY = posX * r_debug_normals_len_face.GetFloat();
			// 
			// Graphics_DrawLine( posX, posY, normal );
			j++;
		}
	}
}

