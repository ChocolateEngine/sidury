#include "skybox.h"
#include "gamesystem.h"

#include "graphics/imaterialsystem.h"
#include "graphics/imaterial.h"
#include "graphics/meshbuilder.hpp"
#include "util.h"

extern GameSystem* game;
extern IMaterialSystem* materialsystem;


CONVAR( g_skybox, 1 );
CONVAR( g_skybox_ang_freeze, 0 );


Skybox& GetSkybox()
{
	static Skybox skybox;
	return skybox;
}


constexpr float SKYBOX_SCALE = 100.0f;
constexpr glm::vec3 vec3_zero( 0, 0, 0 );


void Skybox::Init()
{
	apModel = new Model;

	// create an empty material just to have for now
	// kind of an issue with this, funny
	IMaterial* mat = materialsystem->CreateMaterial( "skybox" );

	MeshBuilder meshBuilder;
	meshBuilder.Start( materialsystem, apModel );
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

	IMaterial* prevMat = apModel->GetMaterial( 0 );

	if ( prevMat )
	{
		apModel->SetMaterial( 0, nullptr );
		materialsystem->DeleteMaterial( prevMat );
	}

	if ( path.empty() )
		return;

	IMaterial* mat = materialsystem->ParseMaterial( path );
	if ( !mat )
		return;

	apModel->SetMaterial( 0, mat );

	if ( mat->GetShaderName() != "skybox" )
	{
		LogWarn( "[Game] Skybox Material is not using skybox shader: %s\n", path.c_str() );
		return;
	}

	aValid = true;

	mat->SetVar( MatVar_Ang, vec3_zero );
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
	if ( !aValid || g_skybox_ang_freeze || !apModel->GetMaterial( 0 ) )
		return;

	ToViewMatrixY( aMatrix, ang );
}


void Skybox::Draw()
{
	if ( aValid && g_skybox )
		materialsystem->AddRenderable( this );
}


IModel* Skybox::GetModel()
{
	return apModel;
}

const glm::mat4& Skybox::GetModelMatrix()
{
	return aMatrix;
}

// hmm, i don't like having to return something valid here
const std::vector< float >& Skybox::GetMorphWeights()
{
	return aMorphVerts;
}

