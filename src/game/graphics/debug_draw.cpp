#include "graphics.h"
#include "graphics_int.h"
#include "debug_draw.h"
#include "types/transform.h"

// --------------------------------------------------------------------------------------
// Debug Drawing

// static MeshBuilder                               gDebugLineBuilder;
static Handle                          gDebugLineModel    = InvalidHandle;
static Handle                          gDebugLineDraw     = InvalidHandle;
static Handle                          gDebugLineMaterial = InvalidHandle;
ChVector< glm::vec3 >                  gDebugLineVertPos;
static ChVector< glm::vec3 >           gDebugLineVertColor;
static size_t                          gDebugLineBufferSize = 0;

// --------------------------------------------------------------------------------------

extern std::vector< ViewRenderList_t > gViewRenderLists;
extern ResourceList< Model >           gModels;
// extern ResourceList< Renderable_t >    gRenderables;

// --------------------------------------------------------------------------------------

CONVAR( r_debug_draw, 0 );
CONVAR( r_debug_aabb, 0 );
CONVAR( r_debug_frustums, 0 );
CONVAR( r_debug_normals, 0 );
CONVAR( r_debug_normals_len, 8 );
CONVAR( r_debug_normals_len_face, 8 );

// --------------------------------------------------------------------------------------


bool Graphics_DebugDrawInit()
{
	gDebugLineMaterial = Graphics_CreateMaterial( "__debug_line_mat", Graphics_GetShader( "debug_line" ) );
	return gDebugLineMaterial;
}


void Graphics_DebugDrawNewFrame()
{
	PROF_SCOPE();

	gDebugLineVertPos.clear();
	gDebugLineVertColor.clear();

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
		gDebugLineModel = gModels.Create( &model );

		if ( !gDebugLineModel )
		{
			Log_Error( gLC_ClientGraphics, "Failed to create Debug Line Model\n" );
			return;
		}

		model->aMeshes.resize( 1 );

		model->apVertexData = new VertexData_t;
		model->apBuffers    = new ModelBuffers_t;

		model->apVertexData->AddRef();
		model->apBuffers->AddRef();

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
		ViewRenderList_t& viewList   = gViewRenderLists[ 0 ];

		glm::mat4         lastMatrix = glm::mat4( 0.f );
		glm::mat4         invMatrix  = glm::mat4( 1.f );

		for ( auto& [ shader, modelList ] : viewList.aRenderLists )
		{
			for ( auto& surfaceDraw : modelList )
			{
				// hack to not draw this AABB multiple times, need to change this render list system
				if ( surfaceDraw.aSurface == 0 )
				{
					// Graphics_DrawModelAABB( renderable->apDraw );

					Renderable_t* modelDraw = Graphics_GetRenderableData( surfaceDraw.aDrawData );

					if ( !modelDraw )
					{
						Log_Warn( gLC_ClientGraphics, "Draw Data does not exist for renderable!\n" );
						return;
					}

					if ( r_debug_aabb )
						Graphics_DrawBBox( modelDraw->aAABB.aMin, modelDraw->aAABB.aMax, { 1.0, 0.5, 1.0 } );

					if ( r_debug_normals )
					{
						// slight hack
						if ( lastMatrix != modelDraw->aModelMatrix )
						{
							lastMatrix = modelDraw->aModelMatrix;
							invMatrix  = glm::inverse( modelDraw->aModelMatrix );
						}

						Graphics_DrawNormals( modelDraw->aModel, invMatrix );
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

	if ( gDebugLineModel && gDebugLineVertPos.size() )
	{
		renderable->aVisible = true;
		// Mesh& mesh = gDebugLineModel->aMeshes[ 0 ];

		Model* model = nullptr;
		if ( !gModels.Get( gDebugLineModel, &model ) )
		{
			Log_Error( gLC_ClientGraphics, "Failed to get Debug Draw Model!\n" );
			gDebugLineModel = InvalidHandle;
			return;
		}

		if ( !model->apVertexData )
		{
			model->apVertexData = new VertexData_t;
			model->apVertexData->AddRef();
		}

		if ( !model->apBuffers )
		{
			model->apBuffers = new ModelBuffers_t;
			model->apBuffers->AddRef();
		}

		// Is our current buffer size too small? If so, free the old ones
		if ( gDebugLineVertPos.size() > gDebugLineBufferSize )
		{
			if ( model->apBuffers && model->apBuffers->aVertex.size() )
			{
				render->DestroyBuffer( model->apBuffers->aVertex[ 0 ] );
				render->DestroyBuffer( model->apBuffers->aVertex[ 1 ] );
				model->apBuffers->aVertex.clear();
			}

			gDebugLineBufferSize = gDebugLineVertPos.size();
		}

		size_t bufferSize = ( 3 * sizeof( float ) ) * gDebugLineVertPos.size();

		// Create new Buffers if needed
		if ( model->apBuffers->aVertex.empty() )
		{
			model->apBuffers->aVertex.resize( 2 );
			model->apBuffers->aVertex[ 0 ] = render->CreateBuffer( "DebugLine Position", bufferSize, EBufferFlags_Vertex, EBufferMemory_Host );
			model->apBuffers->aVertex[ 1 ] = render->CreateBuffer( "DebugLine Color", bufferSize, EBufferFlags_Vertex, EBufferMemory_Host );
		}

		model->apVertexData->aCount = gDebugLineVertPos.size();

		if ( model->aMeshes.empty() )
			model->aMeshes.resize( 1 );

		model->aMeshes[ 0 ].aIndexCount   = 0;
		model->aMeshes[ 0 ].aVertexOffset = 0;
		model->aMeshes[ 0 ].aVertexCount  = gDebugLineVertPos.size();

		// Update the Buffers
		render->BufferWrite( model->apBuffers->aVertex[ 0 ], bufferSize, gDebugLineVertPos.data() );
		render->BufferWrite( model->apBuffers->aVertex[ 1 ], bufferSize, gDebugLineVertColor.data() );
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

	gDebugLineVertPos.reserve( gDebugLineVertPos.size() + 2 );
	gDebugLineVertColor.reserve( gDebugLineVertColor.size() + 2 );

	gDebugLineVertPos.push_back( sX );
	gDebugLineVertPos.push_back( sY );

	gDebugLineVertColor.push_back( sColor );
	gDebugLineVertColor.push_back( sColor );
}


#if 0
void Graphics_DrawAxis( const glm::vec3& sPos, const glm::vec3& sAng, const glm::vec3& sScale )
{
	if ( !r_debug_draw || !gDebugLineModel )
		return;
}
#endif


void Graphics_DrawBBox( const glm::vec3& sMin, const glm::vec3& sMax, const glm::vec3& sColor )
{
	PROF_SCOPE();

	if ( !r_debug_draw || !gDebugLineModel )
		return;

	gDebugLineVertPos.reserve( gDebugLineVertPos.size() + 24 );
	gDebugLineVertColor.reserve( gDebugLineVertColor.size() + 24 );

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

	gDebugLineVertPos.reserve( gDebugLineVertPos.size() + 24 );
	gDebugLineVertColor.reserve( gDebugLineVertColor.size() + 24 );

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

	gDebugLineVertPos.reserve( gDebugLineVertPos.size() + 24 );
	gDebugLineVertColor.reserve( gDebugLineVertColor.size() + 24 );

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

		gDebugLineVertPos.reserve( gDebugLineVertPos.size() + ( mesh.aIndexCount * 3 ) );
		gDebugLineVertColor.reserve( gDebugLineVertColor.size() + ( mesh.aIndexCount * 3 ) );

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

				Graphics_DrawLine( posX, posY, {0.9, 0.1, 0.1} );
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

