#include "game_physics.h"
#include "render/irender.h"
#include "graphics/graphics.h"
#include "graphics/mesh_builder.h"


extern IRender*      render;

IPhysicsEnvironment* physenv    = nullptr;
Ch_IPhysics*         ch_physics = nullptr;

GamePhysics          gamephys;


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

static ResourceList< Model* > gModels;

// ==============================================================


void Phys_DebugInit()
{
	gMatSolid = Graphics_CreateMaterial( "__phys_debug_solid", gShader_Debug );
	gMatWire  = Graphics_CreateMaterial( "__phys_debug_wire", gShader_DebugLine );

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

	MeshBuilder meshBuilder;
	meshBuilder.Start( model );
	meshBuilder.SetMaterial( gMatSolid );

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

	return gModels.Add( model );
}


Handle Phys_CreateTriangleBatchInd(
  const std::vector< PhysVertex >& srVerts,
  const std::vector< u32 >&        srInd )
{
#if 0
	if ( inVertices == nullptr || inVertexCount == 0 || inIndices == nullptr || inIndexCount == 0 )
		return nullptr;  // mEmptyBatch;

	PhysDebugMesh* mesh = new PhysDebugMesh;
	aMeshes.push_back( mesh );

	MeshBuilder meshBuilder;
	meshBuilder.Start( matsys, mesh );
	meshBuilder.SetMaterial( apMatSolid );

	// convert vertices
	for ( int i = 0; i < inIndexCount; i++ )
	{
		meshBuilder.SetPos( fromJolt( inVertices[ inIndices[ i ] ].mPosition ) );
		// meshBuilder.SetNormal( fromJolt( inVertices[inIndices[i]].mNormal ) );
		meshBuilder.SetColor( fromJolt( inVertices[ inIndices[ i ] ].mColor ) );
		// meshBuilder.SetTexCoord( fromJolt( inVertices[inIndices[i]].mUV ) );
		meshBuilder.NextVertex();
	}

	meshBuilder.End();

	// create material
	// IMaterial* mat = matsys->CreateMaterial();
	// aMaterials.push_back( mat );
	//
	// mat->SetShader( "debug" );
	// mat->SetVar( "color", vec4_default );
	//
	// mesh->SetMaterial( 0, mat );

	return mesh;
#endif
	return InvalidHandle;
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
#if 0
	if ( inGeometry.GetPtr()->mLODs.empty() )
		return;

	// TODO: handle LODs
	auto&          lod  = inGeometry.GetPtr()->mLODs[ 0 ];

	PhysDebugMesh* mesh = (PhysDebugMesh*)lod.mTriangleBatch.GetPtr();

	if ( mesh == nullptr )
		return;

	IMaterial*         mat    = mesh->GetMaterial( 0 );
	const std::string& shader = mat->GetShaderName();

	// AAAA
	if ( inDrawMode == EDrawMode::Wireframe )
	{
		if ( shader != gShader_DebugLine )
			mat->SetShader( gShader_DebugLine );
	}
	else
	{
		if ( shader != gShader_Debug )
			mat->SetShader( gShader_Debug );
	}

	mat->SetVar(
	  "color",
	  {
		inModelColor.r,
		inModelColor.g,
		inModelColor.b,
		inModelColor.a,
	  } );

	// AWFUL
	DefaultRenderable* renderable = new DefaultRenderable;
	renderable->aMatrix           = fromJolt( inModelMatrix );
	renderable->apModel           = mesh;

	matsys->AddRenderable( renderable );
#endif
}


void Phys_DrawText(
  const glm::vec3&   srPos,
  const std::string& srText,
  const glm::vec3&   srColor,
  float              sHeight )
{
}



GamePhysics::GamePhysics()
{
}


GamePhysics::~GamePhysics()
{
	ch_physics->DestroyPhysEnv( physenv );
}


void GamePhysics::Init()
{
	GET_SYSTEM_ASSERT( ch_physics, Ch_IPhysics );

#if 0
	Phys_DebugInit();

	ch_physics->SetDebugDrawFuncs(
		Phys_DrawLine,
		Phys_DrawTriangle,
		Phys_CreateTriangleBatch,
		Phys_CreateTriangleBatchInd,
		Phys_DrawGeometry,
		Phys_DrawText
	);
#endif

	physenv = ch_physics->CreatePhysEnv();
	physenv->Init(  );
	physenv->SetGravityZ( phys_gravity );
}


void GamePhysics::SetMaxVelocities( IPhysicsObject* spPhysObj )
{
	if ( !spPhysObj )
		return;

	spPhysObj->SetMaxLinearVelocity( 50000.f );
	spPhysObj->SetMaxAngularVelocity( 50000.f );
}

