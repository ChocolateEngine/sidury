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

static bool             gSkyboxValid  = false;
static ModelDraw_t*     gpSkyboxDraw  = nullptr;
static Handle           gSkyboxModel  = InvalidHandle;
static Handle           gSkyboxShader = InvalidHandle;


bool Skybox_Init()
{
	Model* model              = new Model;
	gSkyboxModel              = Graphics_AddModel( model );

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
	Graphics_FreeModel( gpSkyboxDraw->aModel );
	Graphics_RemoveModelDraw( gpSkyboxDraw );
}


void Skybox_SetAng( const glm::vec3& srAng )
{
	if ( !gSkyboxValid || g_skybox_ang_freeze || !gSkyboxModel || !Model_GetMaterial( gSkyboxModel, 0 ) )
		return;

	Util_ToViewMatrixY( gpSkyboxDraw->aModelMatrix, srAng );
	gpSkyboxDraw->aModelMatrix = gView.aProjMat * gpSkyboxDraw->aModelMatrix;
}


void Skybox_SetMaterial( const std::string& srPath )
{
	Graphics_RemoveModelDraw( gpSkyboxDraw );
	gpSkyboxDraw   = nullptr;
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

	gpSkyboxDraw = Graphics_AddModelDraw( gSkyboxModel );

	if ( !gpSkyboxDraw )
	{
		Log_Error( gLC_ClientGraphics, "Failed to create skybox model draw!\n" );
		return;
	}

	gpSkyboxDraw->aCastShadow = false;
	gpSkyboxDraw->aTestVis    = false;
	gSkyboxValid              = true;

	Mat_SetVar( mat, "ang", vec3_zero );
}

