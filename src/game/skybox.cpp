#include "skybox.h"
#include "main.h"

#include "graphics/graphics.h"
#include "graphics/mesh_builder.h"
#include "render/irender.h"
#include "util.h"


CONVAR( r_skybox, 1 );
CONVAR( r_skybox_ang_freeze, 0 );

// wtf
extern ViewportCamera_t gView;

constexpr float         SKYBOX_SCALE = 100.0f;
constexpr glm::vec3     vec3_zero( 0, 0, 0 );

static bool             gSkyboxValid  = false;
static Handle           gSkyboxDraw   = InvalidHandle;
static Handle           gSkyboxModel  = InvalidHandle;
static Handle           gSkyboxShader = InvalidHandle;


bool Skybox_Init()
{
	Model* model            = nullptr;
	gSkyboxModel            = Graphics_CreateModel( &model );

	gSkyboxShader           = Graphics_GetShader( "skybox" );

	// create an empty material just to have for now
	// kind of an issue with this, funny
	Handle      mat         = Graphics_CreateMaterial( "__skybox", gSkyboxShader );

	MeshBuilder meshBuilder;
	meshBuilder.Start( model, "__skybox_model" );
	meshBuilder.SetMaterial( mat );

	// std::unordered_map< vertex_cube_3d_t, uint32_t > vertIndexes;
	//
	// std::vector< vertex_cube_3d_t >& vertices = GetVertices();
	// std::vector< uint32_t >&    indices  = GetIndices();

	auto CreateVert = [ & ]( const glm::vec3& pos )
	{
		meshBuilder.SetPos( pos * SKYBOX_SCALE );
		meshBuilder.NextVertex();
	};

	auto CreateTri = [ & ]( const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec3& pos2 )
	{
		CreateVert( pos0 );
		CreateVert( pos1 );
		CreateVert( pos2 );
	};

	// create the skybox mesh now
	// Create Bottom Face (-Z)
	CreateTri( { 1, 1, -1 }, { -1, 1, -1 }, { -1, -1, -1 } );
	CreateTri( { 1, -1, -1 }, { 1, 1, -1 }, { -1, -1, -1 } );

	// Create Top Face (+Z)
	CreateTri( { 1, 1, 1 }, { 1, -1, 1 }, { -1, -1, 1 } );
	CreateTri( { -1, 1, 1 }, { 1, 1, 1 }, { -1, -1, 1 } );

	// Create Left Face (-X)
	CreateTri( { -1, -1, -1 }, { -1, 1, -1 }, { -1, 1, 1 } );
	CreateTri( { -1, -1, 1 }, { -1, -1, -1 }, { -1, 1, 1 } );

	// Create Right Face (+X)
	CreateTri( { 1, 1, -1 }, { 1, -1, -1 }, { 1, -1, 1 } );
	CreateTri( { 1, 1, 1 }, { 1, 1, -1 }, { 1, -1, 1 } );

	// Create Back Face (+Y)
	CreateTri( { 1, 1, 1 }, { -1, 1, 1 }, { -1, 1, -1 } );
	CreateTri( { 1, 1, -1 }, { 1, 1, 1 }, { -1, 1, -1 } );

	// Create Front Face (-Y)
	CreateTri( { 1, -1, 1 }, { 1, -1, -1 }, { -1, -1, -1 } );
	CreateTri( { -1, -1, 1 }, { 1, -1, 1 }, { -1, -1, -1 } );

	meshBuilder.End();

	return true;
}


void Skybox_Destroy()
{
	if ( gSkyboxModel )
		Graphics_FreeModel( gSkyboxModel );

	if ( gSkyboxDraw )
		Graphics_FreeRenderable( gSkyboxDraw );

	gSkyboxModel = InvalidHandle;
	gSkyboxDraw  = InvalidHandle;
}


void Skybox_SetAng( const glm::vec3& srAng )
{
	if ( !gSkyboxModel && !Skybox_Init() )
		return;

	if ( !gSkyboxValid || r_skybox_ang_freeze || !Model_GetMaterial( gSkyboxModel, 0 ) )
		return;

	if ( Renderable_t* renderable = Graphics_GetRenderableData( gSkyboxDraw ) )
	{
		Util_ToViewMatrixY( renderable->aModelMatrix, srAng );
		renderable->aModelMatrix = gView.aProjMat * renderable->aModelMatrix;
		renderable->aVisible     = r_skybox.GetBool();
	}
}


void Skybox_SetMaterial( const std::string& srPath )
{
	if ( !gSkyboxModel && !Skybox_Init() )
	{
		Log_Error( "Failed to create skybox model\n" );
		return;
	}

	Graphics_FreeRenderable( gSkyboxDraw );
	gSkyboxDraw    = InvalidHandle;
	gSkyboxValid   = false;

	Handle prevMat = Model_GetMaterial( gSkyboxModel, 0 );

	if ( prevMat )
	{
		Model_SetMaterial( gSkyboxModel, 0, InvalidHandle );
		Graphics_FreeMaterial( prevMat );
	}

	if ( srPath.empty() )
		return;

	Handle mat = Graphics_LoadMaterial( srPath );
	if ( mat == InvalidHandle )
		return;

	Model_SetMaterial( gSkyboxModel, 0, mat );

	if ( Mat_GetShader( mat ) != gSkyboxShader )
	{
		Log_WarnF( "[Game] Skybox Material is not using skybox shader: %s\n", srPath.c_str() );
		return;
	}

	gSkyboxDraw = Graphics_CreateRenderable( gSkyboxModel );

	if ( !gSkyboxDraw )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create skybox renderable!\n" );
		return;
	}

	if ( Renderable_t* renderable = Graphics_GetRenderableData( gSkyboxDraw ) )
	{
		renderable->aCastShadow = false;
		renderable->aTestVis    = false;
		gSkyboxValid            = true;
	}

	Mat_SetVar( mat, "ang", vec3_zero );
}

