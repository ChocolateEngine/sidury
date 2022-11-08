#include "skybox.h"
#include "main.h"

#include "graphics/graphics.h"
#include "graphics/mesh_builder.h"
#include "render/irender.h"
#include "util.h"


CONVAR( g_skybox, 1 );
CONVAR( g_skybox_ang_freeze, 0 );

// wtf
extern ViewportCamera_t gView;

constexpr float         SKYBOX_SCALE = 100.0f;
constexpr glm::vec3     vec3_zero( 0, 0, 0 );

static bool             gSkyboxValid = false;
static ModelDraw_t      gSkyboxDraw{};
static Handle           gSkyboxShader = InvalidHandle;


bool Skybox_Init()
{
	Model* model       = new Model;
	gSkyboxDraw.aModel = Graphics_AddModel( model );

	gSkyboxShader      = Graphics_GetShader( "skybox" );

	// create an empty material just to have for now
	// kind of an issue with this, funny
	Handle      mat    = Graphics_CreateMaterial( "__skybox", gSkyboxShader );

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
	Graphics_FreeModel( gSkyboxDraw.aModel );
}


void Skybox_Draw()
{
	if ( gSkyboxValid && g_skybox )
		Graphics_DrawModel( &gSkyboxDraw );
}


void Skybox_SetAng( const glm::vec3& srAng )
{
	if ( !gSkyboxValid || g_skybox_ang_freeze || gSkyboxDraw.aModel == InvalidHandle || Model_GetMaterial( gSkyboxDraw.aModel, 0 ) == InvalidHandle )
		return;

	Util_ToViewMatrixY( gSkyboxDraw.aModelMatrix, srAng );
	gSkyboxDraw.aModelMatrix = gView.aProjMat * gSkyboxDraw.aModelMatrix;
}


void Skybox_SetMaterial( const std::string& srPath )
{
	gSkyboxValid = false;

	Handle prevMat = Model_GetMaterial( gSkyboxDraw.aModel, 0 );

	if ( prevMat )
	{
		Model_SetMaterial( gSkyboxDraw.aModel, 0, InvalidHandle );
		Graphics_FreeMaterial( prevMat );
	}

	if ( srPath.empty() )
		return;

	Handle mat = Graphics_LoadMaterial( srPath );
	if ( mat == InvalidHandle )
		return;

	Model_SetMaterial( gSkyboxDraw.aModel, 0, mat );

	if ( Mat_GetShader( mat ) != gSkyboxShader )
	{
		Log_WarnF( "[Game] Skybox Material is not using skybox shader: %s\n", srPath.c_str() );
		return;
	}

	gSkyboxValid = true;

	Mat_SetVar( mat, "ang", vec3_zero );
}

