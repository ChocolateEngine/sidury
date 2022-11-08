#include "game_physics.h"
#include "render/irender.h"
#include "graphics/graphics.h"
#include "graphics/mesh_builder.h"


extern IRender*          render;

IPhysicsEnvironment*     physenv    = nullptr;
Ch_IPhysics*             ch_physics = nullptr;

static Phys_DebugFuncs_t gPhysDebugFuncs;


CONVAR_CMD( phys_gravity, -800 )
{
	physenv->SetGravityZ( phys_gravity );
}


CONVAR( phys_dbg, 0 );

// constexpr glm::vec3 vec3_default( 255, 255, 255 );
// constexpr glm::vec4 vec4_default( 255, 255, 255, 255 );

constexpr glm::vec3           vec3_default( 1, 1, 1 );
constexpr glm::vec4           vec4_default( 1, 1, 1, 1 );

static Handle                 gShader_Debug     = InvalidHandle;
static Handle                 gShader_DebugLine = InvalidHandle;

static Handle                 gMatSolid         = InvalidHandle;
static Handle                 gMatWire          = InvalidHandle;

static std::unordered_map< Handle, ModelDraw_t > gModelDraw;

// ==============================================================


void Phys_DebugInit()
{
	gShader_Debug     = Graphics_GetShader( "debug" );
	gShader_DebugLine = Graphics_GetShader( "debug_line" );

	gMatSolid         = Graphics_CreateMaterial( "__phys_debug_solid", gShader_Debug );
	gMatWire          = Graphics_CreateMaterial( "__phys_debug_wire", gShader_DebugLine );

	// apMatSolid->SetShader( "basic_3d" );
}


void Phys_DrawLine( const glm::vec3& from, const glm::vec3& to, const glm::vec3& color )
{
	// Graphics_DrawLine( from, to, color );
}


void Phys_DrawTriangle(
  const glm::vec3& inV1,
  const glm::vec3& inV2,
  const glm::vec3& inV3,
  const glm::vec4& srColor )
{
	// Graphics_DrawLine( inV1, inV2, srColor );
	// Graphics_DrawLine( inV1, inV3, srColor );
	// Graphics_DrawLine( inV2, inV3, srColor );
}

// vertex_debug_t ToVertDBG( const JPH::DebugRenderer::Vertex& inVert )
// {
// 	return {
// 		.pos        = fromJolt(inVert.mPosition),
// 		.color      = fromJolt(inVert.mColor),
// 		// .texCoord   = fromJolt(inVert.mUV),
// 		// .normal     = fromJolt(inVert.mNormal),
// 	};
// }

Handle Phys_CreateTriangleBatch( const std::vector< PhysTriangle_t >& srTriangles )
{
	if ( srTriangles.empty() )
		return InvalidHandle;  // mEmptyBatch;

	Model*      model = new Model;
	// gMeshes.push_back( mesh );

	Handle      mat   = Graphics_CreateMaterial( "__phys_material", gShader_Debug );

	MeshBuilder meshBuilder;
	meshBuilder.Start( model, "_phys_triangle_batch" );
	meshBuilder.SetMaterial( mat );

	// convert vertices
	// vert.resize( inTriangleCount * 3 );
	for ( int i = 0; i < srTriangles.size(); i++ )
	{
		meshBuilder.SetPos( srTriangles[ i ].aPos[ 0 ] );
		// meshBuilder.SetNormal( fromJolt( inTriangles[i].mV[0].mNormal ) );
		// meshBuilder.SetColor( srTriangles[ i ].aPos[ 0 ].mColor ) );
		// meshBuilder.SetTexCoord( fromJolt( inTriangles[i].mV[0].mUV ) );
		meshBuilder.NextVertex();

		meshBuilder.SetPos( srTriangles[ i ].aPos[ 1 ] );
		// meshBuilder.SetNormal( fromJolt( inTriangles[i].mV[1].mNormal ) );
		// meshBuilder.SetColor( fromJolt( inTriangles[ i ].mV[ 1 ].mColor ) );
		// meshBuilder.SetTexCoord( fromJolt( inTriangles[i].mV[1].mUV ) );
		meshBuilder.NextVertex();

		meshBuilder.SetPos( srTriangles[ i ].aPos[ 2 ] );
		// meshBuilder.SetNormal( fromJolt( inTriangles[i].mV[2].mNormal ) );
		// meshBuilder.SetColor( fromJolt( inTriangles[ i ].mV[ 2 ].mColor ) );
		// meshBuilder.SetTexCoord( fromJolt( inTriangles[i].mV[2].mUV ) );
		meshBuilder.NextVertex();

		// vert[i * 3 + 0] = ToVertDBG( inTriangles[i].mV[0] );
		// vert[i * 3 + 1] = ToVertDBG( inTriangles[i].mV[1] );
		// vert[i * 3 + 2] = ToVertDBG( inTriangles[i].mV[2] );
	}

	meshBuilder.End();

	Handle handle = Graphics_AddModel( model );
	gModelDraw[ handle ];
	return handle;
}


Handle Phys_CreateTriangleBatchInd(
  const std::vector< PhysVertex_t >& srVerts,
  const std::vector< u32 >&          srInd )
{
	if ( srVerts.empty() || srInd.empty() )
		return InvalidHandle;

	Model* model = new Model;

	Handle mat = Graphics_CreateMaterial( "__phys_material", gShader_Debug );

	MeshBuilder meshBuilder;
	meshBuilder.Start( model, "__phys_model" );
	meshBuilder.SetMaterial( mat );

	// convert vertices
	for ( size_t i = 0; i < srInd.size(); i++ )
	{
		// pretty awful
		meshBuilder.SetPos( srVerts[ srInd[ i ] ].aPos );
		meshBuilder.SetNormal( srVerts[ srInd[ i ] ].aNorm );
		meshBuilder.SetColor( srVerts[ srInd[ i ] ].aColor );
		meshBuilder.SetTexCoord( srVerts[ srInd[ i ] ].aUV );
		meshBuilder.NextVertex();
	}

	meshBuilder.End();

	Handle handle = Graphics_AddModel( model );
	gModelDraw[ handle ];
	return handle;
}


void Phys_DrawGeometry(
  const glm::mat4& srModelMatrix,
  // const JPH::AABox& inWorldSpaceBounds,
  float            sLODScaleSq,
  const glm::vec4& srColor,
  Handle           sGeometry,
  EPhysCullMode    sCullMode,
  bool             sCastShadow,
  bool             sWireframe )
{
	Handle mat = Model_GetMaterial( sGeometry, 0 );

	Mat_SetVar( mat, "color", srColor );

	auto& modelDraw = gModelDraw[ sGeometry ];
	modelDraw.aModel = sGeometry;
	modelDraw.aModelMatrix = srModelMatrix;

	Graphics_DrawModel( &modelDraw );
}


void Phys_DrawText(
  const glm::vec3&   srPos,
  const std::string& srText,
  const glm::vec3&   srColor,
  float              sHeight )
{
}


void Phys_GetModelVerts( Handle sModel, PhysDataConvex_t& srData )
{
	Model* model = Graphics_GetModelData( sModel );

	for ( size_t s = 0; s < model->aMeshes.size(); s++ )
	{
		Mesh&  mesh     = model->aMeshes[ s ];

		auto&  ind      = mesh.aIndices;
		auto&  vertData = mesh.aVertexData;

		float* data     = nullptr;
		for ( auto& attrib : vertData.aData )
		{
			if ( attrib.aAttrib == VertexAttribute_Position )
			{
				data = (float*)attrib.apData;
				break;
			}
		}

		if ( data == nullptr )
		{
			Log_Error( "Phys_GetModelInd(): Position Vertex Data not found?\n" );
			return;
		}

		u32 newSize  = static_cast< u32 >( ind.size() );
		if ( newSize == 0 )
			newSize = srData.aVertCount;

		u32 origSize = srData.aVertCount;

		if ( srData.apVertices )
			srData.apVertices = (glm::vec3*)realloc( srData.apVertices, ( origSize + newSize ) * sizeof( glm::vec3 ) );
		else
			srData.apVertices = (glm::vec3*)malloc( ( origSize + newSize ) * sizeof( glm::vec3 ) );

		if ( ind.size() )
		{
			for ( size_t i = 0; i < ind.size(); i++ )
			{
				size_t i0 = ind[ i ] * 3;

				srData.apVertices[ origSize + i ].x = data[ i0 + 0 ];
				srData.apVertices[ origSize + i ].y = data[ i0 + 1 ];
				srData.apVertices[ origSize + i ].z = data[ i0 + 2 ];
			}

			srData.aVertCount += ind.size();
		}
		else
		{
			for ( size_t i = 0, j = 0; i < vertData.aCount * 3; j++ )
			{
				srData.apVertices[ origSize + j ].x = data[ i++ ];
				srData.apVertices[ origSize + j ].y = data[ i++ ];
				srData.apVertices[ origSize + j ].z = data[ i++ ];
			}

			srData.aVertCount += vertData.aCount;
		}
	}
}


void Phys_GetModelTris( Handle sModel, std::vector< PhysTriangle_t >& srTris )
{
	Model* model = Graphics_GetModelData( sModel );

	for ( size_t s = 0; s < model->aMeshes.size(); s++ )
	{
		Mesh&  mesh     = model->aMeshes[ s ];

		auto&  ind      = mesh.aIndices;
		auto&  vertData = mesh.aVertexData;

		float* data     = nullptr;
		for ( auto& attrib : vertData.aData )
		{
			if ( attrib.aAttrib == VertexAttribute_Position )
			{
				data = (float*)attrib.apData;
				break;
			}
		}

		// shouldn't be using this function if we have indices lol
		// could even calculate them in GetModelInd as well
		if ( ind.size() )
		{
			for ( size_t i = 0; i < ind.size(); i += 3 )
			{
				size_t    i0   = ind[ i + 0 ] * 3;
				size_t    i1   = ind[ i + 1 ] * 3;
				size_t    i2   = ind[ i + 2 ] * 3;

				auto& tri = srTris.emplace_back();

				tri.aPos[ 0 ] = { data[ i0 ], data[ i0 + 1 ], data[ i0 + 2 ] };
				tri.aPos[ 1 ] = { data[ i1 ], data[ i1 + 1 ], data[ i1 + 2 ] };
				tri.aPos[ 2 ] = { data[ i2 ], data[ i2 + 1 ], data[ i2 + 2 ] };

				// srTris.emplace_back( vec0, vec1, vec2 );
			}
		}
		else
		{
			for ( size_t i = 0; i < vertData.aCount * 3; )
			{
				auto&     tri  = srTris.emplace_back();

				tri.aPos[ 0 ] = { data[ i++ ], data[ i++ ], data[ i++ ] };
				tri.aPos[ 1 ] = { data[ i++ ], data[ i++ ], data[ i++ ] };
				tri.aPos[ 2 ] = { data[ i++ ], data[ i++ ], data[ i++ ] };

				// srTris.emplace_back( vec0, vec1, vec2 );
			}
		}
	}
}


void Phys_GetModelInd( Handle sModel, PhysDataConcave_t& srData )
{
	u32    origSize = srData.aVertCount;
	Model* model    = Graphics_GetModelData( sModel );

	for ( size_t s = 0; s < model->aMeshes.size(); s++ )
	{
		Mesh&  mesh     = model->aMeshes[ s ];

		auto&  ind      = mesh.aIndices;
		auto&  vertData = mesh.aVertexData;

		float* data     = nullptr;
		for ( auto& attrib : vertData.aData )
		{
			if ( attrib.aAttrib == VertexAttribute_Position )
			{
				data = (float*)attrib.apData;
				break;
			}
		}

		if ( data == nullptr )
		{
			Log_Error( "Phys_GetModelInd(): Position Vertex Data not found?\n" );
			return;
		}

		origSize = srData.aVertCount;

		if ( srData.apVertices )
			srData.apVertices = (glm::vec3*)realloc( srData.apVertices, ( origSize + vertData.aCount ) * sizeof( glm::vec3 ) );
		else
			srData.apVertices = (glm::vec3*)malloc( ( origSize + vertData.aCount ) * sizeof( glm::vec3 ) );

		for ( u32 i = 0, j = 0; j < vertData.aCount; i += 3, j++ )
		{
			// srVerts.emplace_back( data[ i + 0 ], data[ i + 1 ], data[ i + 2 ] );

			srData.apVertices[ origSize + j ].x = data[ i + 0 ];
			srData.apVertices[ origSize + j ].y = data[ i + 1 ];
			srData.apVertices[ origSize + j ].z = data[ i + 2 ];

			// srVerts.push_back( what );
		}

		srData.aVertCount += vertData.aCount;
		u32 indSize = srData.aTriCount;

		if ( srData.aTris )
			srData.aTris = (PhysIndexedTriangle_t*)realloc( srData.aTris, ( indSize + ( ind.size() / 3 ) ) * sizeof( PhysIndexedTriangle_t ) );
		else
			srData.aTris = (PhysIndexedTriangle_t*)malloc( ( indSize + ind.size() ) * sizeof( PhysIndexedTriangle_t ) );

		for ( u32 i = 0, j = 0; i < ind.size(); j++ )
		{
			// auto& tri     = srConvexData.emplace_back();

			srData.aTris[ indSize + j ].aPos[ 0 ] = origSize + ind[ i++ ];
			srData.aTris[ indSize + j ].aPos[ 1 ] = origSize + ind[ i++ ];
			srData.aTris[ indSize + j ].aPos[ 2 ] = origSize + ind[ i++ ];

			// srInd.emplace_back(
			//   origSize + ind[ i + 0 ],
			//   origSize + ind[ i + 1 ],
			//   origSize + ind[ i + 2 ] );
		}

		srData.aTriCount += ind.size() / 3;
	}
}


void Phys_Init()
{
	Phys_DebugInit();

	gPhysDebugFuncs.apDrawLine          = Phys_DrawLine;
	gPhysDebugFuncs.apDrawTriangle      = Phys_DrawTriangle;
	gPhysDebugFuncs.apCreateTriBatch    = Phys_CreateTriangleBatch;
	gPhysDebugFuncs.apCreateTriBatchInd = Phys_CreateTriangleBatchInd;
	gPhysDebugFuncs.apDrawGeometry      = Phys_DrawGeometry;
	gPhysDebugFuncs.apDrawText          = Phys_DrawText;

	ch_physics->SetDebugDrawFuncs( gPhysDebugFuncs );

	physenv = ch_physics->CreatePhysEnv();
	physenv->Init(  );
	physenv->SetGravityZ( phys_gravity );
}


void Phys_Shutdown()
{
	ch_physics->DestroyPhysEnv( physenv );
}


void Phys_SetMaxVelocities( IPhysicsObject* spPhysObj )
{
	if ( !spPhysObj )
		return;

	spPhysObj->SetMaxLinearVelocity( 50000.f );
	spPhysObj->SetMaxAngularVelocity( 50000.f );
}

