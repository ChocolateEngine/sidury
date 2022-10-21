#pragma once

#include "core/core.h"


// Vertex containing all possible values
struct MeshBuilderVertex
{
	glm::vec3   pos{};
	glm::vec3   normal{};
	glm::vec2   texCoord{};
	// glm::vec2 texCoord2;
	glm::vec3   color{};

	// only one thing for morph data at the moment
	glm::vec3   morphPos{};

	// bones and weights
	// std::vector<int> bones;
	// std::vector<float> weights;

	inline bool operator==( const MeshBuilderVertex& srOther ) const
	{
		// Guard self assignment
		if ( this == &srOther )
			return true;

		return !( std::memcmp( &pos, &srOther.pos, sizeof( MeshBuilderVertex ) ) );
	}
};


// Vertex containing all possible morph values
struct MeshBuilderMorphVertex
{
	// Vertex Index to affect
	int         vert;

	// Morph Target it came from
	int         morph;

	// Position and Normal deltas
	glm::vec3   pos{};
	glm::vec3   normal{};

	inline bool operator==( const MeshBuilderMorphVertex& srOther ) const
	{
		if ( vert != srOther.vert )
			return false;

		if ( morph != srOther.morph )
			return false;

		if ( pos != srOther.pos )
			return false;

		if ( normal != srOther.normal )
			return false;

		return true;
	}
};


// Hashing Support for this vertex struct
namespace std
{
	template<  > struct hash< MeshBuilderVertex >
	{
		size_t operator()( MeshBuilderVertex const& vertex ) const
		{
			size_t value = 0;

			// for some reason just doing hash< glm::vec3 > doesn't work anymore
			value ^= ( hash< glm::vec3::value_type >()( vertex.pos.x ) );
			value ^= ( hash< glm::vec3::value_type >()( vertex.pos.y ) );
			value ^= ( hash< glm::vec3::value_type >()( vertex.pos.z ) );

			value ^= ( hash< glm::vec3::value_type >()( vertex.color.x ) );
			value ^= ( hash< glm::vec3::value_type >()( vertex.color.y ) );
			value ^= ( hash< glm::vec3::value_type >()( vertex.color.z ) );

			value ^= ( hash< glm::vec3::value_type >()( vertex.normal.x ) );
			value ^= ( hash< glm::vec3::value_type >()( vertex.normal.y ) );
			value ^= ( hash< glm::vec3::value_type >()( vertex.normal.z ) );

			value ^= ( hash< glm::vec2::value_type >()( vertex.texCoord.x ) );
			value ^= ( hash< glm::vec2::value_type >()( vertex.texCoord.y ) );

			return value;
		}
	};
}


// TODO:
// - Add a way to add bones and weights to the vertex
// - Add a way to add morph targets to the vertex
// - probably add a "AddBlendShape( std::string_view name )" function

glm::vec3 Util_CalculatePlaneNormal( const glm::vec3& a, const glm::vec3& b, const glm::vec3& c );
// glm::vec3 Util_CalculateVertexNormal( const glm::vec3& a, const glm::vec3& b, const glm::vec3& c );

// Helper For Mesh Building on User Land
struct MeshBuilder
{
	struct BlendShape
	{
		std::string aName;
	};

	struct BlendShapeData
	{
		size_t                                aIndex;
		std::vector< MeshBuilderMorphVertex > aVertices;
		MeshBuilderVertex                     aVertex;
	};

	struct Surface
	{
		std::vector< MeshBuilderVertex >             aVertices;
		std::vector< u32 >                           aIndices;

		// uh
		std::unordered_map< MeshBuilderVertex, u32 > aVertInd;

		MeshBuilderVertex                            aVertex;
		VertexFormat                                 aFormat   = VertexFormat_None;
		Handle                                       aMaterial = InvalidHandle;

		// is this a per surface thing? i would imagine so
		std::vector< BlendShapeData >                aBlendShapes;
	};

#ifdef _DEBUG
	const char* apDebugName = nullptr;
#endif

	Model*                    apMesh = nullptr;

	std::vector< BlendShape > aBlendShapes;
	std::vector< Surface >    aSurfaces;
	size_t                    aSurf  = 0;
	Surface*                  apSurf = 0;  // pointer to current surface

	// ------------------------------------------------------------------------

	void                     Start( Model* spMesh, const char* spDebugName = nullptr );
	void                     End();
	void                     Reset();

	size_t                   GetVertexCount( size_t i ) const;
	size_t                   GetVertexCount() const;

	// ------------------------------------------------------------------------
	// Building Functions

	void                     NextVertex();

	void                     SetPos( const glm::vec3& data );
	void                     SetPos( float x, float y, float z );

	void                     SetNormal( const glm::vec3& data );
	void                     SetNormal( float x, float y, float z );

	void                     SetColor( const glm::vec3& data );
	void                     SetColor( float x, float y, float z );

	void                     SetTexCoord( const glm::vec2& data );
	void                     SetTexCoord( float x, float y );

	// ------------------------------------------------------------------------
	// Blend Shapes

	// void                     SetMorphPos( const glm::vec3& data );
	// void                     SetMorphPos( float x, float y, float z );

	// ------------------------------------------------------------------------

	void                     SetMaterial( Handle sMaterial );
	void                     SetSurfaceCount( size_t sCount );
	void                     SetCurrentSurface( size_t sIndex );
	void                     AdvanceSurfaceIndex();

	const MeshBuilderVertex& GetLastVertex();

	// ------------------------------------------------------------------------

	void                     CalculateNormals( size_t sIndex );
	void                     CalculateAllNormals();
};


