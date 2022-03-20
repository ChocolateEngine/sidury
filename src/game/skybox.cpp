#include "skybox.h"
#include "gamesystem.h"

#include "graphics/imaterialsystem.h"
#include "graphics/imaterial.h"
#include "util.h"

extern GameSystem* game;
extern IMaterialSystem* materialsystem;


CONVAR( g_skybox, 1 );


Skybox& GetSkybox()
{
	static Skybox skybox;
	return skybox;
}


enum Faces
{
	FaceTop,
	FaceBottom,
	FaceLeft,
	FaceRight,
	FaceBack,
	FaceFront,
};


const char SkyboxFaceVerts[][3] = {
	{  0,  0,  1 },  // Above
	{  0,  0, -1 },  // Below
	{ -1,  0,  0 },  // Left
	{  1,  0,  0 },  // Right
	{  0, -1,  0 },  // Behind
	{  0,  1,  0 },  // Front
};


const glm::vec3 FaceNorm[6] = {
	{  0,  0,  1 },  // Above
	{  0,  0, -1 },  // Below
	// { -1,  0,  0 },  // Left
	{ -0.25,  0,  0.3 },  // Left
	{  1,  0,  0 },  // Right
	// {  0.5,  0,  0 },  // Right - HACK FOR LIGHTING
	{  0, -1,  0 },  // Behind
	{  0,  1,  0.15 },  // Front
};


/*const glm::vec2 texCoord[2][3] = {
	// { { 1, 1 }, { 0, 1 }, { 0, 0 } },
	// { { 1, 0 }, { 1, 1 }, { 0, 0 } }
	
	//{ { 2, 2 }, { 0, 2 }, { 0, 0 } },
	//{ { 2, 0 }, { 2, 2 }, { 0, 0 } }
	
	{ { 2, 2 }, { -2, 2 }, { -2, -2 } },
	{ { 2, -2 }, { 2, 2 }, { -2, -2 } }
	
	//{ { -2, -2 }, { 0, -2 }, { 0, 0 } },
	//{ { -2, 0 }, { -2, -2 }, { 0, 0 } }

	//{ { 1, 1 }, { -1, 1 }, { -1, -1 } },
	//{ { 1, -1 }, { 1, 1 }, { -1, -1 } }

	//{ { 1, 10 }, { -10, 10 },   { -10, -10 } },
	//{ { 1, -10 },  { 10, 10 },  { -10, -10 } }
};*/


const glm::vec3 texCoord[12][3] = {
	// Bottom Face (-Z)
	// { { -1, -1 },  { 1, -1 }, { 1, 1 } },
	// { { -1, 1 }, { -1, -1 },  { 1, 1 } },

	// { { 1, 1, 1 },  { -1, 1, 1 }, { -1,  -1, 1 } },
	// // { { -1, -3 }, {  -3, -1 }, { 1,  1 } },
	// { { -1, 1, 1 }, { -1, -1, 1 },  { 1, 1, 1 } },

	// 2nd list moves it along the bottom (y+) in a corner only?
	// 3rd list moves it along x????
	{ { 2, -2, 1 /*nothing?*/ },	{ 1, -1, 4 },	{ 4, 1, 1 } },
	//{ { -1, 1, 4 },		{ -1, -1, 1 },	{ 1, 1, 1 } },
	{ { -2, 2, 4 },		{ -2, -2, 1 },	{ 2, 2, 1 } },

	// Top Face (+Z)
	{ { 1, 1,  1 },  { -1, 1, 1 }, { -1,  -1, 1 } },
	{ { 1, -1, 1 },  { 1, 1, 1 },  { -1,  -1, 1 } },

	// Left Face (-X)
	//{ { 1, 1,  1 },  { -1, 1, 1 }, { -1,  -1, 1 } },
	//{ { 1, -1, 1 },  { 1, 1, 1 },  { -1,  -1, 1 } },
	
	//{ { 4, 4,  1 },  { -4, 4, 1 }, { -4,  -4, 1 } },
	//{ { 4, -4, 1 },  { 4, 4, 1 },  { -4,  -4, 1 } },

	// { { 1, 1,  1 },  { -4, 4, 0 }, { -1, -1, 1 } },
	{ { 1, 2,  0 },  { -2, 1, -2 }, { -2, -2, 1 } },
	{ { 4, -4, 0 },  { 4, 4, 4 },  { -8,  -2, 0 } },

	// Right Face (+X)
	//{ { 1, 1,  1 },  { -1, 1, 1 }, { -1,  -1, 1 } },
	//{ { 1, -1, 1 },  { 1, 1, 1 },  { -1,  -1, 1 } },

	{ { 4, 4,  0 },  { -4, 4, 1 }, { -4,  -4, 4 } },
	{ { 4, -4, 1 },  { 4, 4, 0 },  { -4,  -4, 4 } },

	// Back Face (+Y)
	{ { 1, 1,  1 },  { -1, 1, 1 }, { -1,  -1, 1 } },
	{ { 1, -1, 1 },  { 1, 1, 1 },  { -1,  -1, 1 } },

	// Front Face (-Y)
	{ { 1, 1,  1 },  { -1, 1, 1 }, { -1,  -1, 1 } },
	{ { 1, -1, 1 },  { 1, 1, 1 },  { -1,  -1, 1 } },
};


constexpr float SKYBOX_SCALE = 100.0f;


void Skybox::Init()
{
	// Create Mesh (only to get the shader to draw right now, blech
	materialsystem->RegisterRenderable( this );

	std::unordered_map< vertex_cube_3d_t, uint32_t > vertIndexes;

	std::vector< vertex_cube_3d_t >& vertices = GetVertices();
	std::vector< uint32_t >&    indices  = GetIndices();

	vertex_cube_3d_t vert{};

#if 1
	auto CreateVert = [&]( const glm::vec3& pos, const glm::vec3& uv )
	{
		vert.pos = (pos * SKYBOX_SCALE);
		vert.texCoord = uv;

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

	auto CreateTri = [&]( const glm::vec3 uv[3], const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec3& pos2 )
	{
		CreateVert( pos0, uv[0] );
		CreateVert( pos1, uv[1] );
		CreateVert( pos2, uv[2] );

		//CreateVert( pos0, pos0 );
		//CreateVert( pos1, pos1 );
		//CreateVert( pos2, pos2 );
	};

	// create the skybox mesh now
	// Create Bottom Face (-Z)
	CreateTri( texCoord[0],     {  1,  1, -1 },  { -1,  1, -1 },  { -1, -1, -1 } );
	CreateTri( texCoord[1],     {  1, -1, -1 },  {  1,  1, -1 },  { -1, -1, -1 } );

	// Create Top Face (+Z)
	CreateTri( texCoord[2],     {  1,  1,  1 },  {  1, -1,  1 },  { -1, -1,  1 } );
	CreateTri( texCoord[3],     { -1,  1,  1 },  {  1,  1,  1 },  { -1, -1,  1 } );

	// Create Left Face (-X)
	CreateTri( texCoord[4],     { -1, -1, -1 },  { -1,  1, -1 },  { -1,  1,  1 } );
	CreateTri( texCoord[5],     { -1, -1,  1 },  { -1, -1, -1 },  { -1,  1,  1 } );

	// Create Right Face (+X)
	CreateTri( texCoord[6],     {  1,  1, -1 },  {  1, -1, -1 },  {  1, -1,  1 } );
	CreateTri( texCoord[7],     {  1,  1,  1 },  {  1,  1, -1 },  {  1, -1,  1 } );

	// Create Back Face (+Y)
	CreateTri( texCoord[8],     {  1,  1,  1 },  { -1,  1,  1 },  { -1,  1, -1 } );
	CreateTri( texCoord[9],     {  1,  1, -1 },  {  1,  1,  1 },  { -1,  1, -1 } );

	// Create Front Face (-Y)
	CreateTri( texCoord[10],    {  1, -1,  1 },  {  1, -1, -1 },  { -1, -1, -1 } );
	CreateTri( texCoord[11],    { -1, -1,  1 },  {  1, -1,  1 },  { -1, -1, -1 } );

	materialsystem->CreateVertexBuffer( this );
	materialsystem->CreateIndexBuffer( this );
#endif
}


void Skybox::SetSkybox( const std::string &path )
{
	aValid = false;

	SetMaterial( materialsystem->ParseMaterial( path ) );

	if ( GetMaterial() == nullptr )
		return;

	if ( GetMaterial()->GetShaderName() != "skybox" )
	{
		Print( "[Game] Skybox Material is not using skybox shader: %s\n", path.c_str() );
		return;
	}

	aValid = true;
}


CONVAR( g_skybox_ang_freeze, 0 );


void Skybox::Draw()
{
	if ( aValid && g_skybox )
		materialsystem->AddRenderable( this );

	// TESTING
	if ( !g_skybox_ang_freeze )
		GetMaterial()->SetVar( "ang", aAng );

	auto CreateTri = [&]( const glm::vec3 color, const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec3& pos2 )
	{
		graphics->DrawLine( pos0 * SKYBOX_SCALE, pos1 * SKYBOX_SCALE, color );
		graphics->DrawLine( pos0 * SKYBOX_SCALE, pos2 * SKYBOX_SCALE, color );
		graphics->DrawLine( pos1 * SKYBOX_SCALE, pos2 * SKYBOX_SCALE, color );
	};

	// create the skybox mesh now
	// Create Bottom Face (-Z)
	CreateTri( {0, 0, 1},     {  1,  1, -1 },  { -1,  1, -1 },  { -1, -1, -1 } );
	CreateTri( {0, 0, 1},     {  1, -1, -1 },  {  1,  1, -1 },  { -1, -1, -1 } );

	// Create Top Face (+Z)
	CreateTri( {0, 0, 1},     {  1,  1,  1 },  {  1, -1,  1 },  { -1, -1,  1 } );
	CreateTri( {0, 0, 1},     { -1,  1,  1 },  {  1,  1,  1 },  { -1, -1,  1 } );

	// Create Left Face (-X)
	CreateTri( {1, 0, 0},     {  1,  1, -1 },  {  1, -1, -1 },  {  1, -1,  1 } );
	CreateTri( {1, 0, 0},     {  1,  1,  1 },  {  1,  1, -1 },  {  1, -1,  1 } );

	// Create Right Face (+X)
	CreateTri( {1, 0, 0},     { -1, -1, -1 },  { -1,  1, -1 },  { -1,  1,  1 } );
	CreateTri( {1, 0, 0},     { -1, -1,  1 },  { -1, -1, -1 },  { -1,  1,  1 } );

	// Create Back Face (+Y)
	CreateTri( {0, 1, 0},     {  1,  1,  1 },  { -1,  1,  1 },  { -1,  1, -1 } );
	CreateTri( {0, 1, 0},     {  1,  1, -1 },  {  1,  1,  1 },  { -1,  1, -1 } );

	// Create Front Face (-Y)
	CreateTri( {0, 1, 0},     {  1, -1,  1 },  {  1, -1, -1 },  { -1, -1, -1 } );
	CreateTri( {0, 1, 0},     { -1, -1,  1 },  {  1, -1,  1 },  { -1, -1, -1 } );
}

