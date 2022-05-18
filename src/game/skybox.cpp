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
	// Create Mesh (only to get the shader to draw right now, blech
	materialsystem->RegisterRenderable( this );

	// create an empty material just to have for now
	// kind of an issue with this, funny
	IMaterial* mat = materialsystem->CreateMaterial( "skybox" );

	MeshBuilder meshBuilder;
	meshBuilder.Start( materialsystem, this );
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

	IMaterial* prevMat = GetMaterial( 0 );

	if ( prevMat )
	{
		SetMaterial( 0, nullptr );
		materialsystem->DeleteMaterial( prevMat );
	}

	if ( path.empty() )
		return;

	IMaterial* mat = materialsystem->ParseMaterial( path );
	if ( !mat )
		return;

	SetMaterial( 0, mat );

	if ( mat->GetShaderName() != "skybox" )
	{
		LogWarn( "[Game] Skybox Material is not using skybox shader: %s\n", path.c_str() );
		return;
	}

	aValid = true;

	mat->SetVar( MatVar_Ang, vec3_zero );
}


void Skybox::SetAng( const glm::vec3& ang )
{
	if ( aValid && !g_skybox_ang_freeze && GetMaterial( 0 ) )
		GetMaterial( 0 )->SetVar( MatVar_Ang, ang );
}


void Skybox::Draw()
{
	if ( aValid && g_skybox )
		materialsystem->AddRenderable( this );
}

