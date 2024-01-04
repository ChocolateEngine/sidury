#include "skybox.h"
#include "main.h"

#include "igraphics.h"
#include "mesh_builder.h"
#include "render/irender.h"
#include "util.h"


CONVAR( r_skybox, 1 );
CONVAR( r_skybox_ang_freeze, 0 );


constexpr float         SKYBOX_SCALE = 100.0f;
constexpr glm::vec3     vec3_zero( 0, 0, 0 );

static bool             gSkyboxValid  = false;
static Handle           gSkyboxDraw   = InvalidHandle;
static Handle           gSkyboxModel  = InvalidHandle;
static Handle           gSkyboxShader = InvalidHandle;


void skybox_set_dropdown(
	const std::vector< std::string >& args,  // arguments currently typed in by the user
	std::vector< std::string >& results )      // results to populate the dropdown list with
{
	for ( const auto& file : FileSys_ScanDir( "materials", ReadDir_AllPaths | ReadDir_NoDirs | ReadDir_Recursive ) )
	{
		if ( file.ends_with( ".." ) )
			continue;

		// Make sure it's a chocolate material file
		if ( !file.ends_with( ".cmt" ) )
			continue;

		if ( args.size() && !file.starts_with( args[ 0 ] ) )
			continue;

		results.push_back( file );
	}
}


CONCMD_DROP_VA( skybox_set, skybox_set_dropdown, 0, "Set the skybox material" )
{
	if ( args.empty() )
		return;

	Skybox_SetMaterial( args[ 0 ] );
}


bool Skybox_Init()
{
	Model* model    = nullptr;
	gSkyboxModel    = graphics->CreateModel( &model );
	gSkyboxShader   = graphics->GetShader( "skybox" );

	// create an empty material just to have for now
	// kind of an issue with this, funny
	Handle      mat = graphics->CreateMaterial( "__skybox", gSkyboxShader );

	MeshBuilder meshBuilder( graphics );
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
		graphics->FreeModel( gSkyboxModel );

	if ( gSkyboxDraw )
		graphics->FreeRenderable( gSkyboxDraw );

	gSkyboxModel = InvalidHandle;
	gSkyboxDraw  = InvalidHandle;
}


void Skybox_SetAng( const glm::vec3& srAng )
{
	if ( !gSkyboxModel && !Skybox_Init() )
		return;

	if ( !gSkyboxValid || r_skybox_ang_freeze || !graphics->Model_GetMaterial( gSkyboxModel, 0 ) )
		return;

	if ( Renderable_t* renderable = graphics->GetRenderableData( gSkyboxDraw ) )
	{
		Util_ToViewMatrixY( renderable->aModelMatrix, srAng );
		renderable->aModelMatrix = renderable->aModelMatrix;
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

	graphics->FreeRenderable( gSkyboxDraw );
	gSkyboxDraw    = InvalidHandle;
	gSkyboxValid   = false;

	Handle prevMat = graphics->Model_GetMaterial( gSkyboxModel, 0 );

	if ( prevMat )
	{
		graphics->Model_SetMaterial( gSkyboxModel, 0, InvalidHandle );
		graphics->FreeMaterial( prevMat );
	}

	if ( srPath.empty() )
		return;

	Handle mat = graphics->LoadMaterial( srPath );
	if ( mat == InvalidHandle )
		return;

	graphics->Model_SetMaterial( gSkyboxModel, 0, mat );

	if ( graphics->Mat_GetShader( mat ) != gSkyboxShader )
	{
		Log_WarnF( "Skybox Material is not using skybox shader: %s\n", srPath.c_str() );
		return;
	}

	gSkyboxDraw = graphics->CreateRenderable( gSkyboxModel );

	if ( !gSkyboxDraw )
	{
		Log_Error( "Failed to create skybox renderable!\n" );
		return;
	}

	if ( Renderable_t* renderable = graphics->GetRenderableData( gSkyboxDraw ) )
	{
		renderable->aCastShadow = false;
		renderable->aTestVis    = false;
		gSkyboxValid            = true;
	}

	graphics->Mat_SetVar( mat, "ang", vec3_zero );
}


const char* Skybox_GetMaterialName()
{
	if ( ( !gSkyboxModel && !Skybox_Init() ) || !gSkyboxValid )
		return nullptr;

	Handle mat = graphics->Model_GetMaterial( gSkyboxModel, 0 );
	if ( mat == CH_INVALID_HANDLE )
		return nullptr;

	return graphics->Mat_GetName( mat );
}


// TODO: how will you know which skybox is the active one?
// Same goes for the playerSpawn component
// Maybe in the game map struct, you can store a skybox entity to use the one to draw
// And you can swap that out whenever with a different skybox entity
// But what about for playerSpawn?

// Called when the component is added to this entity
void SkyboxSystem::ComponentAdded( Entity sEntity, void* spData )
{
	// Renderable_t* renderable = Ent_CreateRenderable( sEntity, gSkyboxModel );
}


// Called when the component is removed from this entity
void SkyboxSystem::ComponentRemoved( Entity sEntity, void* spData )
{
}


// Called when the component data has been updated (ONLY ON CLIENT RIGHT NOW)
void SkyboxSystem::ComponentUpdated( Entity sEntity, void* spData )
{
	// Material Change
}


void SkyboxSystem::Update()
{
	// gViewport[ 0 ]
	// Skybox_SetAng()
}

