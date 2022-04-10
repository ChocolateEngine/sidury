#include "skybox.h"
#include "gamesystem.h"

#include "graphics/imaterialsystem.h"
#include "graphics/imaterial.h"
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

	std::unordered_map< vertex_cube_3d_t, uint32_t > vertIndexes;

	std::vector< vertex_cube_3d_t >& vertices = GetVertices();
	std::vector< uint32_t >&    indices  = GetIndices();

	vertex_cube_3d_t vert{};

	auto CreateVert = [&]( const glm::vec3& pos )
	{
		vert.pos = pos * SKYBOX_SCALE;

		auto iterSavedIndex = vertIndexes.find(vert);

		// Do we have this vertex saved?
		if ( iterSavedIndex != vertIndexes.end() )
		{
			GetIndices().push_back( iterSavedIndex->second );
			return;
		}

		vertices.push_back( vert );
		indices.push_back( vertIndexes.size() );
		vertIndexes[ vert ] = vertIndexes.size();
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

	materialsystem->CreateVertexBuffer( this );
	materialsystem->CreateIndexBuffer( this );
}


void Skybox::SetSkybox( const std::string &path )
{
	aValid = false;

	if ( GetMaterial() )
		materialsystem->DeleteMaterial( GetMaterial() );

	SetMaterial( materialsystem->ParseMaterial( path ) );

	if ( GetMaterial() == nullptr )
		return;

	if ( GetMaterial()->GetShaderName() != "skybox" )
	{
		Print( "[Game] Skybox Material is not using skybox shader: %s\n", path.c_str() );
		return;
	}

	aValid = path != "";

	GetMaterial()->SetVar( "ang", vec3_zero );
}


void Skybox::SetAng( const glm::vec3& ang )
{
	if ( !g_skybox_ang_freeze && GetMaterial() )
		GetMaterial()->SetVar( "ang", ang );
}


void Skybox::Draw()
{
	if ( aValid && g_skybox )
		materialsystem->AddRenderable( this );
}

