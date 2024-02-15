#include "main.h"
#include "entity_editor.h"
#include "mesh_builder.h"
#include "gizmos.h"


static ChHandle_t gGizmoTranslationModel = CH_INVALID_HANDLE;

constexpr float   CH_GIZMO_SCALE         = 1.f;


static ChHandle_t CreateAxisMaterial( const char* name, ChHandle_t shader, glm::vec3 color )
{
	ChHandle_t mat = graphics->CreateMaterial( name, shader );

	if ( mat == CH_INVALID_HANDLE )
		return CH_INVALID_HANDLE;

	graphics->Mat_SetVar( mat, "color", { color.x, color.y, color.z, 1 } );
	return mat;
}


bool Gizmo_BuildTranslationMesh()
{
	ChHandle_t shader = graphics->GetShader( "gizmo" );

	if ( shader == CH_INVALID_HANDLE )
	{
		Log_ErrorF( "Failed to find gizmo shader!\n" );
		return false;
	}

	Model* model           = nullptr;
	gGizmoTranslationModel = graphics->CreateModel( &model );

	ChHandle_t  matX       = CreateAxisMaterial( "gizmo_translate_x", shader, {1, 0, 0} );
	ChHandle_t  matY       = CreateAxisMaterial( "gizmo_translate_y", shader, {0, 1, 0} );
	ChHandle_t  matZ       = CreateAxisMaterial( "gizmo_translate_z", shader, {0, 0, 1} );

	if ( !matX || !matY || !matZ )
	{
		Log_ErrorF( "Failed to make axis material for translation gizmo!\n" );
		return false;
	}

	MeshBuilder meshBuilder( graphics );
	meshBuilder.Start( model, "gizmo_translation" );
	meshBuilder.SetSurfaceCount( 3 );

	// std::unordered_map< vertex_cube_3d_t, uint32_t > vertIndexes;
	//
	// std::vector< vertex_cube_3d_t >& vertices = GetVertices();
	// std::vector< uint32_t >&    indices  = GetIndices();

	auto CreateVert = [ & ]( const glm::vec3& pos )
	{
		meshBuilder.SetPos( pos * CH_GIZMO_SCALE );
		meshBuilder.NextVertex();
	};

	auto CreateTri = [ & ]( const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec3& pos2 )
	{
		CreateVert( pos0 );
		CreateVert( pos1 );
		CreateVert( pos2 );
	};
	
	// -------------------------------------------------------------------------------------
	// X - AXIS
	
	meshBuilder.SetCurrentSurface( 0 );
	meshBuilder.SetMaterial( matX );

	// create the main rectangle mesh now
	// Create Bottom Face (-Z)
	CreateTri( { 15, 1, -1 }, { 15, -1, -1 }, { -1, -1, -1 } );
	CreateTri( { -1, 1, -1 }, { 15, 1, -1 }, { -1, -1, -1 } );

	// Create Top Face (+Z)
	CreateTri( { 15, 1, 1 }, { 1, 1, 1 }, { 1, -1, 1 } );
	CreateTri( { 15, -1, 1 }, { 15, 1, 1 }, { 1, -1, 1 } );

	// Create Left Face (-X)
	// CreateTri( { -1, 15, -1 }, { -1, -1, -1 }, { -1, -1, 1 } );
	// CreateTri( { -1, 15, 1 }, { -1, 1, -1 }, { -1, -1, 1 } );
	
	// Create Right Face (+X)
	// CreateTri( { 15, -1, -1 }, { 15, 1, -1 }, { 15, 1, 1 } );
	// CreateTri( { 15, -1, 1 }, { 15, -1, -1 }, { 15, 1, 1 } );
	
	// Create Back Face (+Y)
	CreateTri( { 1, 1, 1 }, { 15, 1, 1 }, { 15, 1, -1 } );
	CreateTri( { 1, 1, -1 }, { 1, 1, 1 }, { 15, 1, -1 } );
	
	// Create Front Face (-Y)
	CreateTri( { -1, -1, 1 }, { -1, -1, -1 }, { 15, -1, -1 } );
	CreateTri( { 15, -1, 1 }, { -1, -1, 1 }, { 15, -1, -1 } );

	// create the triangle
	// Create Bottom Face
	CreateTri( { 15, 2, -2 }, { 15, -2, -2 }, { 15, -2, 2 } );
	CreateTri( { 15, 2, 2 }, { 15, 2, -2 }, { 15, -2, 2 } );
	
	CreateTri( { 15, 2, 2 },   { 15, -2, 2 },  { 20, 0, 0 } );      // Front Triangle
	CreateTri( { 15, -2, -2 }, { 15,  2, -2 }, { 20, 0, 0 } );  // Back Triangle
	CreateTri( { 15, -2, 2 },  { 15, -2, -2 }, { 20, 0, 0 } );    // Left Triangle
	CreateTri( { 15, 2, -2 },  { 15,  2, 2 },  { 20, 0, 0 } );    // Right Triangle
	
	// -------------------------------------------------------------------------------------
	// Y - AXIS
	
	meshBuilder.SetCurrentSurface( 1 );
	meshBuilder.SetMaterial( matY );
	
	// create the main rectangle mesh now
	// Create Bottom Face (-Z)
	CreateTri( { 1, 15, -1 }, { 1, -1, -1 }, { -1, -1, -1 } );
	CreateTri( { -1, 15, -1 }, { 1, 15, -1 }, { -1, -1, -1 } );
	
	// Create Top Face (+Z)
	CreateTri( { 1, 15, 1 }, { -1, 15, 1 }, { -1, -1, 1 } );
	CreateTri( { 1, -1, 1 }, { 1, 15, 1 }, { -1, -1, 1 } );
	
	// Create Left Face (-X)
	CreateTri( { -1, 15, -1 }, { -1, -1, -1 }, { -1, -1, 1 } );
	CreateTri( { -1, 15, 1 }, { -1, 15, -1 }, { -1, -1, 1 } );
	
	// Create Right Face (+X)
	CreateTri( { 1, -1, -1 }, { 1, 15, -1 }, { 1, 15, 1 } );
	CreateTri( { 1, -1, 1 }, { 1, -1, -1 }, { 1, 15, 1 } );
	
	// Create Back Face (+Y)
	// CreateTri( { -1, 15, 1 }, { 1, 15, 1 }, { 1, 15, -1 } );
	// CreateTri( { -1, 15, -1 }, { -1, 15, 1 }, { 1, 15, -1 } );
	
	// Create Front Face (-Y)
	CreateTri( { -1, -1, 1 }, { -1, -1, -1 }, { 1, -1, -1 } );
	CreateTri( { 1, -1, 1 }, { -1, -1, 1 }, { 1, -1, -1 } );
	
	// create the triangle
	// Create Bottom Face
	CreateTri( { 2, 15, 2 }, { -2, 15, 2 }, { -2, 15, -2 } );
	CreateTri( { 2, 15, -2 }, { 2, 15, 2 }, { -2, 15, -2 } );
	
	CreateTri( { 2, 15, -2 }, { -2, 15, -2 }, { 0, 20, 0 } );  // Front Triangle
	CreateTri( { -2, 15, 2 }, { 2, 15, 2 }, { 0, 20, 0 } );  // Back Triangle
	CreateTri( { -2, 15, -2 }, { -2, 15, 2 }, { 0, 20, 0 } );  // Left Triangle
	CreateTri( { 2, 15, 2 }, { 2, 15, -2 }, { 0, 20, 0 } );  // Right Triangle

	// -------------------------------------------------------------------------------------
	// Z - AXIS

	meshBuilder.SetCurrentSurface( 2 );
	meshBuilder.SetMaterial( matZ );
	
	// create the main rectangle mesh now
	// Create Bottom Face (-Z)
	// CreateTri( { 1, 1, -1 }, { 1, -1, -1 }, { -1, -1, -1 } );
	// CreateTri( { -1, 1, -1 }, { 1, 1, -1 }, { -1, -1, -1 } );
	
	// Create Top Face (+Z)
	// // CreateTri( { 1, 1, 15 }, { -1, 1, 15 }, { -1, -1, 15 } );
	// // CreateTri( { 1, -1, 15 }, { 1, 1, 15 }, { -1, -1, 15 } );
	
	// Create Left Face (-X)
	CreateTri( { -1, 1, 1 }, { -1, -1, -1 }, { -1, -1, 15 } );
	CreateTri( { -1, 1, 15 }, { -1, 1, 1 }, { -1, -1, 15 } );
	
	// Create Right Face (+X)
	CreateTri( { 1, -1, -1 }, { 1, 1, -1 }, { 1, 1, 15 } );
	CreateTri( { 1, -1, 15 }, { 1, -1, -1 }, { 1, 1, 15 } );
	
	// Create Back Face (+Y)
	CreateTri( { -1, 1, 15 }, { 1, 1, 15 }, { 1, 1, -1 } );
	CreateTri( { -1, 1, -1 }, { -1, 1, 15 }, { 1, 1, -1 } );
	
	// Create Front Face (-Y)
	CreateTri( { -1, -1, 15 }, { -1, -1, -1 }, { 1, -1, -1 } );
	CreateTri( { 1, -1, 15 }, { -1, -1, 15 }, { 1, -1, -1 } );
	
	// create the triangle
	// Create Bottom Face
	CreateTri( { 2, -2, 15 }, { -2, -2, 15 }, { -2, 2, 15 } );
	CreateTri( { 2, 2, 15 }, { 2, -2, 15 }, { -2, 2, 15 } );
	
	CreateTri( { 2, 2, 15 }, { -2, 2, 15 }, { 0, 0, 20 } );    // Front Triangle
	CreateTri( { -2, -2, 15 }, { 2, -2, 15 }, { 0, 0, 20 } );  // Back Triangle
	CreateTri( { -2, 2, 15 }, { -2, -2, 15 }, { 0, 0, 20 } );   // Left Triangle
	CreateTri( { 2, -2, 15 }, { 2, 2, 15 }, { 0, 0, 20 } );   // Right Triangle

	// -------------------------------------------------------------------------------------
	
	// TODO: plane axis and center box for screen transform

	meshBuilder.End();

	gEditorRenderables.gizmoTranslation = graphics->CreateRenderable( gGizmoTranslationModel );

	if ( !gEditorRenderables.gizmoTranslation )
	{
		Log_Error( "Failed to Create Translation Gizmo Renderable!\n" );
		return false;
	}

	if ( Renderable_t* renderable = graphics->GetRenderableData( gEditorRenderables.gizmoTranslation ) )
	{
		graphics->SetRenderableDebugName( gEditorRenderables.gizmoTranslation, "gizmo_translation" );

		renderable->aCastShadow = false;
		renderable->aTestVis    = false;
	}

	// AABB's for selecting an axis
	gEditorRenderables.baseTranslateX.min = {1, 1, -1};
	gEditorRenderables.baseTranslateX.max = {17, -1, 1};

	gEditorRenderables.baseTranslateY.min = {-1, 1, -1};
	gEditorRenderables.baseTranslateY.max = {1, 17, 1};

	gEditorRenderables.baseTranslateZ.min = {-1, -1, 1};
	gEditorRenderables.baseTranslateZ.max = {1, 1, 17};

	return true;
}


bool Gizmo_Init()
{
	if ( !Gizmo_BuildTranslationMesh() )
	{
		return false;
	}

	return true;
}


void Gizmo_Shutdown()
{
}


void Gizmo_Draw()
{
}


EGizmoMode Gizmo_GetMode()
{
	return EGizmoMode_Translation;
}


void Gizmo_SetMode( EGizmoMode mode )
{
}


void Gizmo_SetMatrix( glm::mat4& mat )
{
}

