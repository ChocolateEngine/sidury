#include "core/core.h"
#include "types/transform.h"
#include "render/irender.h"
#include "graphics.h"
#include "mesh_builder.h"

#include <glm/vec3.hpp>


#define MESH_BUILDER_USE_IND 1

// TODO:
// - Add a way to add bones and weights to the vertex
// - Add a way to add morph targets to the vertex
// - probably add a "AddBlendShape( std::string_view name )" function

	

// std::vector< Vertex >   aVertices;
// VertexFormat            aFormat = VertexFormat_None;
// IMaterial*              apMaterial = nullptr;


// ------------------------------------------------------------------------


// source uses some IVertexBuffer and IIndexBuffer class here? hmm
void MeshBuilder::Start( Model* spMesh, const char* spDebugName )
{
	apMesh = spMesh;

#ifdef _DEBUG
	apDebugName = spDebugName;
#endif

	SetSurfaceCount( 1 );
	SetCurrentSurface( 0 );
}


void MeshBuilder::End()
{
	if ( apMesh->aMeshes.size() )
	{
		Log_WarnF( gLC_ClientGraphics, "Meshes already created for Model: \"%s\"\n", apDebugName ? apDebugName : "internal" );
	}

	// apMesh->SetSurfaceCount( aSurfaces.size() );

	for ( size_t i = 0; i < aSurfaces.size(); i++ )
	{
		Surface& surf = aSurfaces[ i ];

		if ( surf.aVertices.empty() )
		{
			Log_DevF( gLC_ClientGraphics, 1, "Model Surface %zd has no vertices, skipping: \"%s\"\n", i, apDebugName ? apDebugName : "internal" );
			continue;
		}

		Mesh&         mesh     = apMesh->aMeshes.emplace_back();
		VertexData_t& vertData = mesh.aVertexData;

		vertData.aFormat       = surf.aFormat;
		vertData.aCount        = surf.aVertices.size();

		std::vector< VertexAttribute > attribs;
		for ( int attrib = 0; attrib < VertexAttribute_Count; attrib++ )
		{
			// does this format contain this attribute?
			// if so, add the attribute size to it
			if ( surf.aFormat & (1 << attrib) )
			{
				attribs.push_back( (VertexAttribute)attrib );
			}
		}

		// size_t formatSize = Graphics_GetVertexFormatSize( surf.aFormat );

		vertData.aData.resize( attribs.size() );

		size_t j = 0;
		for ( VertexAttribute attrib : attribs )
		{
			size_t size = Graphics_GetVertexAttributeSize( attrib );

			// VertAttribData_t* attribData = &vertData.aData.emplace_back();
			// vertData.aData.push_back( {} );
			VertAttribData_t& attribData = vertData.aData[j];
			// VertAttribData_t* attribData = &vertData.aData.back();

			attribData.aAttrib = attrib;
			attribData.apData = malloc( size * vertData.aCount );

			// char* data = (char*)attribData.apData;

			#define MOVE_VERT_DATA( vertAttrib, attribName, elemCount ) \
				if ( attrib == vertAttrib ) \
				{ \
					for ( size_t v = 0; v < surf.aVertices.size(); v++ ) \
					{ \
						memcpy( (float*)attribData.apData + (v * elemCount), &surf.aVertices[v].attribName, size ); \
					} \
				}

			if ( attrib == VertexAttribute_Position )
			{
				float* data = (float*)attribData.apData;
				for ( size_t v = 0; v < surf.aVertices.size(); v++ )
				{
					// memcpy( data + v * size, &surf.aVertices[v].pos, size );
					memcpy( (float*)attribData.apData + (v * 3), &surf.aVertices[v].pos, size);

					// memcpy( data, &surf.aVertices[v].pos, size );
					// data += size;
				}
			}

			// MOVE_VERT_DATA( VertexAttribute_Position, pos )
			else MOVE_VERT_DATA( VertexAttribute_Normal,   normal,   3 )
			else MOVE_VERT_DATA( VertexAttribute_Color,    color,    3 )
			else MOVE_VERT_DATA( VertexAttribute_TexCoord, texCoord, 2 )
				
			// else MOVE_VERT_DATA( VertexAttribute_MorphPos, morphPos, 3 )

			#undef MOVE_VERT_DATA
			j++;
		}

#if MESH_BUILDER_USE_IND
		// Now Copy Indices
		std::vector< u32 >& ind = apMesh->aMeshes[ i ].aIndices;
		ind.resize( surf.aIndices.size() );

		memcpy( ind.data(), surf.aIndices.data(), sizeof( u32 ) * ind.size() );
#endif

		// apMesh->SetVertexDataLocked( i, true );
		apMesh->aMeshes[ i ].aMaterial = surf.aMaterial;

		char* debugName = nullptr;
#ifdef _DEBUG
		if ( apDebugName )
		{
			size_t nameLen = strlen( apDebugName );
			debugName = new char[ nameLen + 13 ];  // MEMORY LEAK - need string memory pool
			snprintf( debugName, nameLen + 13, "%s | Surface %zd", apDebugName, i );
		}
#endif

		Graphics_CreateVertexBuffers( apMesh->aMeshes[ i ], debugName );

#if MESH_BUILDER_USE_IND
		Graphics_CreateIndexBuffer( apMesh->aMeshes[ i ], debugName );
#endif
	}
}


void MeshBuilder::Reset()
{
	apMesh = nullptr;
	aSurfaces.clear();
	aSurf = 0;
	apSurf = 0;
}


// ------------------------------------------------------------------------


size_t MeshBuilder::GetVertexCount( size_t i ) const
{
	Assert( i > aSurfaces.size() );
	return aSurfaces[i].aVertices.size();
}


size_t MeshBuilder::GetVertexCount() const
{
	Assert( aSurfaces.size() );
	return aSurfaces[aSurf].aVertices.size();
}


// constexpr size_t GetVertexSize()
// {
// 	return aVertexSize;
// }


// ------------------------------------------------------------------------
// Building Functions


void MeshBuilder::NextVertex()
{
	Assert( aSurfaces.size() );
	Assert( apSurf );

	if ( apSurf->aVertices.empty() )
	{
		apSurf->aVertices.push_back( apSurf->aVertex );
		apSurf->aIndices.push_back( 0 );

		apSurf->aVertInd[apSurf->aVertex] = 0;

		return;
	}

#if MESH_BUILDER_USE_IND
	auto it = apSurf->aVertInd.find( apSurf->aVertex );

	// not a unique vertex
	if ( it != apSurf->aVertInd.end() )
	{
		apSurf->aIndices.push_back( it->second );
		return;
	}
#endif

	// this is a unique vertex, up the index
	apSurf->aVertices.push_back( apSurf->aVertex );

#if MESH_BUILDER_USE_IND
	u32 indCount = apSurf->aVertInd.size();
#else
	u32 indCount = apSurf->aIndices.size();
#endif

	apSurf->aIndices.push_back( indCount );
	apSurf->aVertInd[apSurf->aVertex] = indCount;
}


#define VERT_EMPTY_CHECK() \
	Assert( aSurfaces.size() ); \
	Assert( apSurf );


void MeshBuilder::SetPos( const glm::vec3& data )
{
	VERT_EMPTY_CHECK();

	apSurf->aFormat |= VertexFormat_Position;
	apSurf->aVertex.pos = data;
}


void MeshBuilder::SetPos( float x, float y, float z )
{
	VERT_EMPTY_CHECK();

	apSurf->aFormat |= VertexFormat_Position;
	apSurf->aVertex.pos.x = x;
	apSurf->aVertex.pos.y = y;
	apSurf->aVertex.pos.z = z;
}


void MeshBuilder::SetNormal( const glm::vec3& data )
{
	VERT_EMPTY_CHECK();

	apSurf->aFormat |= VertexFormat_Normal;
	apSurf->aVertex.normal = data;
}


void MeshBuilder::SetNormal( float x, float y, float z )
{
	VERT_EMPTY_CHECK();

	apSurf->aFormat |= VertexFormat_Normal;
	apSurf->aVertex.normal.x = x;
	apSurf->aVertex.normal.y = y;
	apSurf->aVertex.normal.z = z;
}


void MeshBuilder::SetColor( const glm::vec3& data )
{
	VERT_EMPTY_CHECK();

	apSurf->aFormat |= VertexFormat_Color;
	apSurf->aVertex.color = data;
}


void MeshBuilder::SetColor( float x, float y, float z )
{
	VERT_EMPTY_CHECK();

	apSurf->aFormat |= VertexFormat_Color;
	apSurf->aVertex.color.x = x;
	apSurf->aVertex.color.y = y;
	apSurf->aVertex.color.z = z;
}


void MeshBuilder::SetTexCoord( const glm::vec2& data )
{
	VERT_EMPTY_CHECK();

	apSurf->aFormat |= VertexFormat_TexCoord;
	apSurf->aVertex.texCoord = data;
}


void MeshBuilder::SetTexCoord( float x, float y )
{
	VERT_EMPTY_CHECK();

	apSurf->aFormat |= VertexFormat_TexCoord;
	apSurf->aVertex.texCoord.x = x;
	apSurf->aVertex.texCoord.y = y;
}


// ------------------------------------------------------------------------
// Blend Shapes


// inline void MeshBuilder::SetMorphPos( const glm::vec3& data )
// {
// 	VERT_EMPTY_CHECK();
// 
// 	apSurf->aFormat |= VertexFormat_MorphPos;
// 	apSurf->aVertex.morphPos = data;
// }
// 
// 
// inline void MeshBuilder::SetMorphPos( float x, float y, float z )
// {
// 	VERT_EMPTY_CHECK();
// 
// 	apSurf->aFormat |= VertexFormat_MorphPos;
// 	apSurf->aVertex.morphPos.x = x;
// 	apSurf->aVertex.morphPos.y = y;
// 	apSurf->aVertex.morphPos.z = z;
// }


// ------------------------------------------------------------------------


void MeshBuilder::SetMaterial( Handle sMaterial )
{
	Assert( aSurfaces.size() );
	Assert( apSurf );

	apSurf->aMaterial = sMaterial;
}


void MeshBuilder::SetSurfaceCount( size_t sCount )
{
	aSurfaces.resize( sCount );

	// kind of a hack? not really supposed to do this but im lazy
	// apMesh->SetSurfaceCount( aSurfaces.size() );
}
	

void MeshBuilder::SetCurrentSurface( size_t sIndex )
{
	Assert( sIndex < aSurfaces.size() );

	aSurf = sIndex;
	apSurf = &aSurfaces[sIndex];
}


void MeshBuilder::AdvanceSurfaceIndex()
{
	aSurf++;
	Assert( aSurf < aSurfaces.size() );
	apSurf = &aSurfaces[aSurf];
}


const MeshBuilderVertex& MeshBuilder::GetLastVertex()
{
	Assert( aSurfaces.size() );
	Assert( apSurf );
	return *( apSurf->aVertices.end() );
}


// ------------------------------------------------------------------------


glm::vec3 Util_CalculatePlaneNormal( const glm::vec3& a, const glm::vec3& b, const glm::vec3& c )
{
	// glm::vec3 v1( b.x - a.x, b.y - a.y, b.z - a.z );
	// glm::vec3 v2( c.x - a.x, c.y - a.y, c.z - a.z );
	glm::vec3 v1( b - a );
	glm::vec3 v2( c - a );

	return glm::cross( v1, v2 );
}


#if 0
inline glm::vec3 CalculateVertexNormal( const glm::vec3& a, const glm::vec3& b, const glm::vec3& c )
{
	glm::vec3 v1( b - a );
	glm::vec3 v2( c - a );

	glm::vec3 faceNorm = glm::cross( v1, v2 );

	glm::vec3 temp = faceNorm / glm::length( faceNorm );
}
#endif


void MeshBuilder::CalculateNormals( size_t sIndex )
{
	Assert( aSurfaces.size() );

	Surface& surf = aSurfaces[sIndex];
	VertexData_t& vertData = apMesh->aMeshes[ sIndex ].aVertexData;

#if 0
	vec3_t v1 = ( vec3_t ){ c->x - b->x, c->y - b->y, c->z - b->z };
	vec3_t v2 = ( vec3_t ){ d->x - b->x, d->y - b->y, d->z - b->z };

	vec3_cross( &a->aNormal, &v1, &v2 );

	a->aDistance = vec3_dot( &a->aNormal, b );
#endif

	// http://web.missouri.edu/~duanye/course/cs4610-spring-2017/assignment/ComputeVertexNormal.pdf

	size_t faceI = 0;
	for ( size_t v = 0, faceI = 0; v < surf.aVertices.size(); v++, faceI += 3 )
	{
	}
}


void MeshBuilder::CalculateAllNormals()
{
	for ( size_t i = 0; i < aSurfaces.size(); i++ )
	{
		CalculateNormals( i );
	}
}


#undef VERT_EMPTY_CHECK


