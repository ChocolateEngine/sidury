#include "skybox.h"
#include "main.h"

#include "graphics/graphics.h"
#include "graphics/mesh_builder.h"
#include "util.h"

extern GameSystem* game;


CONVAR( g_skybox, 1 );
CONVAR( g_skybox_ang_freeze, 0 );

static Handle gSkyboxShader = InvalidHandle;

Skybox& GetSkybox()
{
	static Skybox skybox;
	return skybox;
}


constexpr float SKYBOX_SCALE = 100.0f;
constexpr glm::vec3 vec3_zero( 0, 0, 0 );


void Skybox::Init()
{
	Model* model    = new Model;
	aModel          = Graphics_AddModel( model );

	gSkyboxShader = Graphics_GetShader( "skybox" );

	// create an empty material just to have for now
	// kind of an issue with this, funny
	Handle      mat = Graphics_CreateMaterial( "__skybox", gSkyboxShader );

	MeshBuilder meshBuilder;
	meshBuilder.Start( model, "__skybox_model" );
	meshBuilder.SetMaterial( mat );

	// std::unordered_map< vertex_cube_3d_t, uint32_t > vertIndexes;
	// 
	// std::vector< vertex_cube_3d_t >& vertices = GetVertices();
	// std::vector< uint32_t >&    indices  = GetIndices();

	auto CreateVert = [&]( const glm::vec3& pos )
	{
		meshBuilder.SetPos( pos * SKYBOX_SCALE );
		meshBuilder.NextVertex();
	};

	auto CreateTri = [&]( const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec3& pos2 )
	{
		CreateVert( pos0 );
		CreateVert( pos1 );
		CreateVert( pos2 );
	};

	// create the skybox mesh now
	// Create Bottom Face (-Z)
	CreateTri( {  1,  1, -1 },  { -1,  1, -1 },  { -1, -1, -1 } );
	CreateTri( {  1, -1, -1 },  {  1,  1, -1 },  { -1, -1, -1 } );

	// Create Top Face (+Z)
	CreateTri( {  1,  1,  1 },  {  1, -1,  1 },  { -1, -1,  1 } );
	CreateTri( { -1,  1,  1 },  {  1,  1,  1 },  { -1, -1,  1 } );

	// Create Left Face (-X)
	CreateTri( { -1, -1, -1 },  { -1,  1, -1 },  { -1,  1,  1 } );
	CreateTri( { -1, -1,  1 },  { -1, -1, -1 },  { -1,  1,  1 } );

	// Create Right Face (+X)
	CreateTri( {  1,  1, -1 },  {  1, -1, -1 },  {  1, -1,  1 } );
	CreateTri( {  1,  1,  1 },  {  1,  1, -1 },  {  1, -1,  1 } );

	// Create Back Face (+Y)
	CreateTri( {  1,  1,  1 },  { -1,  1,  1 },  { -1,  1, -1 } );
	CreateTri( {  1,  1, -1 },  {  1,  1,  1 },  { -1,  1, -1 } );

	// Create Front Face (-Y)
	CreateTri( {  1, -1,  1 },  {  1, -1, -1 },  { -1, -1, -1 } );
	CreateTri( { -1, -1,  1 },  {  1, -1,  1 },  { -1, -1, -1 } );
	
	meshBuilder.End();
}


// TODO: i really need to look into speeding up const char*
// cause this is a bit annoying
static std::string MatVar_Ang = "ang";


void Skybox::SetSkybox( const std::string &path )
{
	aValid = false;

	Handle prevMat = Model_GetMaterial( aModel, 0 );

	if ( prevMat )
	{
		Model_SetMaterial( aModel, 0, InvalidHandle );
		Graphics_FreeMaterial( prevMat );
	}

	if ( path.empty() )
		return;

	Handle mat = Graphics_LoadMaterial( path );
	if ( mat == InvalidHandle )
		return;

	Model_SetMaterial( aModel, 0, mat );

	if ( Mat_GetShader( mat ) != gSkyboxShader )
	{
		Log_WarnF( "[Game] Skybox Material is not using skybox shader: %s\n", path.c_str() );
		return;
	}

	aValid = true;

	Mat_SetVar( mat, MatVar_Ang, vec3_zero );
}


#define VIEWMAT_ANG( axis ) glm::vec3(viewMatrix[0][axis], viewMatrix[1][axis], viewMatrix[2][axis])

/* Y Up version of the ViewMatrix */
inline void ToViewMatrixY( glm::mat4& viewMatrix, const glm::vec3& ang )
{
#if 0
	viewMatrix = glm::eulerAngleYZX(
		glm::radians(ang[YAW]),
		glm::radians(ang[ROLL]),
		glm::radians(ang[PITCH])
	);
#else
	/* Y Rotation - YAW (Mouse X for Y up) */
	viewMatrix = glm::rotate( glm::radians(ang[YAW]), vec_right );

	/* X Rotation - PITCH (Mouse Y) */
	viewMatrix = glm::rotate( viewMatrix, glm::radians(ang[PITCH]), VIEWMAT_ANG(0) );

	/* Z Rotation - ROLL */
	viewMatrix = glm::rotate( viewMatrix, glm::radians(ang[ROLL]), VIEWMAT_ANG(2) );
#endif
}

#undef VIEWMAT_ANG


void Skybox::SetAng( const glm::vec3& ang )
{
	if ( !aValid || g_skybox_ang_freeze || Model_GetMaterial( aModel, 0 ) == InvalidHandle )
		return;

	ToViewMatrixY( aMatrix, ang );
}


void Skybox::Draw()
{
	// if ( aValid && g_skybox )
	// 	Graphics_DrawModel( {aModel, aMatrix} );
}


