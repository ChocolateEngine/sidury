#include "terrain.h"
#include "noise.h"
#include "gamesystem.h"

#include "../../chocolate/inc/core/renderer/materialsystem.h"
#include "../../chocolate/inc/shared/util.h"


VoxelWorld* voxelworld = new VoxelWorld;

extern GameSystem* game;
extern IMaterialSystem* materialsystem;


// BLECH
Material* g_matGrass = nullptr;


enum Faces
{
	FaceTop,
	FaceBottom,
	FaceLeft,
	FaceRight,
	FaceBack,
	FaceFront,
};


constexpr BlockPosS FaceDirs[6] = {
	{  0,  0,  1 },  // Above
	{  0,  0, -1 },  // Below
	{ -1,  0,  0 },  // Left
	{  1,  0,  0 },  // Right
	{  0, -1,  0 },  // Behind
	{  0,  1,  0 },  // Front
};


constexpr int FaceDirsInt[6] = {
	 1,     // Above
	-1,     // Below
	-1,     // Left
	 1,     // Right
	-1,     // Behind
	 1,     // Front
};


constexpr uchar FaceDirsVec[6] = {
	 2,     // Above
	 2,     // Below
	 0,     // Left
	 0,     // Right
	 1,     // Behind
	 1,     // Front
};


constexpr glm::vec3 FaceNorm[6] = {
	{  0,  0,  1 },  // Above
	{  0,  0, -1 },  // Below
	//{ -1,  0,  0 },  // Left
	{ -0.25,  0,  0.3 },  // Left
	{  1,  0,  0 },  // Right
	// {  0.5,  0,  0 },  // Right - HACK FOR LIGHTING
	{  0, -1,  0 },  // Behind
	{  0,  1,  0.15 },  // Front
};


constexpr glm::vec2 uvGrass[2][3] = {
	{ { 1, 1 }, { 0, 1 }, { 0, 0 } },
	{ { 1, 0 }, { 1, 1 }, { 0, 0 } }
};


void VoxelWorld::Init()
{
	// Create Base Materials
	g_matGrass = materialsystem->CreateMaterial(  );

	// Set Shaders
	g_matGrass->apShader = materialsystem->GetShader( "basic_3d" );

	// Load Base Textures
	g_matGrass->apDiffuse = materialsystem->CreateTexture( g_matGrass, "materials/models/riverhouse/dirtfloor001a.png" );

	Print( "Size Of: glm::ivec3: %u", sizeof glm::ivec3 );
	
	// Now Create the world
	CreateWorld(  );
}


void VoxelWorld::Update( float frameTime )
{
	for ( Chunk* chunk: aChunks )
	{
		//for (auto& mesh: chunk->aMeshes)
		{
			ChunkMesh* mesh = chunk->GetMesh();

			if ( mesh && mesh->aVertices.size() > 0 )
				materialsystem->AddRenderable( mesh );
		}
	}
}


void CreateTerrain( Chunk* chunk, const std::array<int, CHUNK_AREA>& heightMap, unsigned seed )
{
	for (int y = 0; y < CHUNK_SIZE; y++)
	{
		for (int x = 0; x < CHUNK_SIZE; x++)
		{
			int height = heightMap[y * CHUNK_SIZE + x];
			// int biomeVal = heightMap[y * CHUNK_SIZE + x];
			//int biomeVal = biomeMap[z * CHUNK_SIZE + x];
			//auto& biome = biomeData.getBiomeData(biomeVal > 50 ? 0 : 1);
			for (int z = 0; z < CHUNK_SIZE; z++)
			{
				int voxelZ = chunk->aPos.z * CHUNK_SIZE + y;

				if ( height < z )
				{
					chunk->SetBlockType( {x, y, z}, BlockType::Air );
					continue;
				}
				else if ( height == z )
				{
					chunk->SetBlockType( {x, y, z}, BlockType::Grass );
				}
				else
				{
					chunk->SetBlockType( {x, y, z}, BlockType::Dirt );
				}

				// chunk->SetBlockType( {x, y, z}, BlockType::Air );
				//chunk->SetBlockType( {x, y, z}, BlockType::Air );
			}
		}
	}
}


void VoxelWorld::CreateWorld(  )
{
	float seed = GenerateSeed( "bruh" );

	auto totalStartTime = std::chrono::high_resolution_clock::now(  );
	auto startTime = totalStartTime;

	Print( "\nCreating %u Chunks - %u (%u * %u * %u) Blocks\n", WORLD_SIZE*WORLD_SIZE, CHUNK_VOLUME, CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE );

	aChunks.resize( WORLD_VOLUME );

	// Create Chunks
	for (uchar z = 0; z < 1; z++) {
		for (uchar y = 0; y < WORLD_SIZE; y++) {
			for (uchar x = 0; x < WORLD_SIZE; x++) {
				CreateChunk( {x, y, z} );
			}
		}
	}

	auto currentTime = std::chrono::high_resolution_clock::now(  );
	float time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count(  );
	Print( "Time Creating Chunks: %.5f seconds\n\n", time );

	startTime = std::chrono::high_resolution_clock::now(  );
	Print( "Creating Chunk Terrain\n" );

	for ( Chunk* chunk: aChunks )
	{
		auto heightMap = CreateChunkHeightMap( chunk->aPos, WORLD_SIZE, seed);
		int maxHeight = *std::max_element(heightMap.cbegin(), heightMap.cend());

		CreateTerrain( chunk, heightMap, seed );
	}

	currentTime = std::chrono::high_resolution_clock::now(  );
	time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count(  );
	Print( "Time Creating Chunk Terrain: %.5f seconds\n\n", time );

	startTime = std::chrono::high_resolution_clock::now(  );
	Print( "Finding Adjacent Chunk Faces\n" );

	for ( Chunk* chunk: aChunks )
	{
		for (size_t i = 0; i < 6; i++)
		{
			if ( chunk->aAdjacentChunks[i] != nullptr )
				continue;

			glm::ivec3 adjPos = chunk->aPos + glm::ivec3(FaceDirs[i]);

			Chunk* otherChunk = GetChunk( adjPos );

			if ( otherChunk )
			{
				chunk->aAdjacentChunks[i] = otherChunk;

				// BLECH
				switch( (Faces)i )
				{
					case FaceTop:
						otherChunk->aAdjacentChunks[FaceBottom] = chunk;
						break;

					/*case FaceBottom:
						otherChunk->aAdjacentChunks[FaceTop] = chunk;
						break;

					case FaceLeft:
						otherChunk->aAdjacentChunks[FaceRight] = chunk;
						break;*/

					case FaceRight:
						otherChunk->aAdjacentChunks[FaceLeft] = chunk;
						break;

					/*case FaceBack:
						otherChunk->aAdjacentChunks[FaceFront] = chunk;
						break;*/

					case FaceFront:
						otherChunk->aAdjacentChunks[FaceBack] = chunk;
						break;
				}
			}
		}

		bool anyInvalid = false;

		for (size_t i = 0; i < 6; i++)
			anyInvalid |= (chunk->aAdjacentChunks[i] != nullptr);

		if ( !anyInvalid )
			break;
	}

	currentTime = std::chrono::high_resolution_clock::now(  );
	time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count(  );
	Print( "Time Finding Adjacent Chunk Faces: %.5f seconds\n\n", time );

	startTime = std::chrono::high_resolution_clock::now(  );
	Print( "Creating Chunk Meshes\n" );

	// Now Create the models for them
	for ( Chunk* chunk: aChunks )
		CreateChunkMesh( chunk );

	currentTime = std::chrono::high_resolution_clock::now(  );
	time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count(  );
	Print( "Time Creating Chunk Meshes: %.5f seconds\n\n", time );

	time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - totalStartTime ).count(  );
	Print( "Total World Creation Time: %.5f seconds\n\n", time );
}


void VoxelWorld::CreateChunk( const glm::ivec3& pos )
{
	// make one chunk
	Chunk* chunk = new Chunk;
	//chunk->aChunkId = aChunks.size();
	chunk->aPos = pos;

	// set all adjacent chunks to be invalid
	for (size_t i = 0; i < 6; i++)
		chunk->aAdjacentChunks[i] = nullptr;

	chunk->aBlocks.resize( CHUNK_VOLUME );
	BlockID id = 0;

	for (BlockPosType z = 0; z < CHUNK_SIZE; z++)
	for (BlockPosType y = 0; y < CHUNK_SIZE; y++)
	for (BlockPosType x = 0; x < CHUNK_SIZE; x++)
		chunk->aBlocks[id++] = {{x, y, z}, BlockType::Grass};
		//chunk->aBlocks[id++] = new Block{{x, y, z}, BlockType::Grass};

	// aChunks.push_back( chunk );
	aChunks[ chunk->GetId() ] = chunk;
}


// Used to make hashing faster
struct small_vert_t
{
	glm::vec3 pos;
	glm::vec2 texCoord;
	glm::vec3 normal;

	bool operator==( const small_vert_t& other ) const
	{
		//return pos == other.pos && texCoord == other.texCoord && normal == other.normal;
		return pos == other.pos && texCoord == other.texCoord;
	}
};


namespace std	//	Black magic!! don't touch!!!!
{
	template<  > struct hash< small_vert_t >
	{
		size_t operator(  )( small_vert_t const& vertex ) const
		{
			return  ( ( hash< glm::vec3 >(  )( vertex.pos ) ^
			//return  ( ( ( hash< glm::vec3 >(  )( vertex.pos ) << 1 ) ^
				//	   ( hash< glm::vec2 >(  )( vertex.texCoord ) << 1 ) ) >> 1 );
				
				// works, and probably the fastest?
					   ( hash< glm::vec3 >(  )( vertex.normal ) << 1 ))) ^
				( hash< glm::vec2 >(  )( vertex.texCoord ) << 1 );
				
				// works, but is slower
				//	   ( hash< glm::vec3 >(  )( vertex.normal ) ))) ^
				//( hash< glm::vec2 >(  )( vertex.texCoord ) );
				
				// also slow
				//	   ( hash< glm::vec3 >(  )( vertex.normal ) << 1 ) ) >> 1 ) ^
				//( hash< glm::vec2 >(  )( vertex.texCoord ) << 1 );
		}
	};
}


void VoxelWorld::CreateChunkMesh( Chunk* chunk )
{
	Print("\tChunk Mesh %u\n", chunk->GetId() );

	// Create Chunk Mesh

	ChunkMesh* mesh = new ChunkMesh;
	materialsystem->RegisterRenderable( mesh );
	game->apGraphics->InitMesh( mesh );
	mesh->apChunk = chunk;
	chunk->aMesh = mesh;
	//size_t index = 0;

	std::unordered_map< vertex_3d_t, uint32_t > vertIndexes;
	// std::unordered_map< small_vert_t, uint32_t > vertIndexes;  // 8.6 sec

	//small_vert_t vertSmall;
	vertex_3d_t vert;
	vert.color = {};

	for (BlockID i = 0; i < CHUNK_VOLUME; i++)
	{
		if ( chunk->GetBlockType(i) == BlockType::Air )
			continue;

		const BlockPosS& pos = chunk->GetBlockPos(i);
		glm::vec3 fpos = pos;

		auto CreateVert = [&]( const glm::vec3& pos, const glm::vec2& uv, const glm::vec3& norm )
		{
			//vertSmall.pos = (pos * BLOCK_SCALE) + (fpos * BLOCK_SCALE);
			//vertSmall.texCoord = uv;

			vert.pos = (pos * BLOCK_SCALE) + (fpos * BLOCK_SCALE);
			//vert.pos = vertSmall.pos;
			vert.texCoord = uv;

			//auto iterSavedIndex = vertIndexes.find( vertSmall );
			auto iterSavedIndex = vertIndexes.find( vert );

			// Do we have this vertex saved?
			if ( iterSavedIndex != vertIndexes.end() )
			{
				mesh->aIndices.push_back( iterSavedIndex->second );
				return;
			}

			//vert.pos = vertSmall.pos;
			//vert.texCoord = uv;

			mesh->aVertices.push_back( vert );
			mesh->aIndices.push_back( vertIndexes.size() );
			//vertIndexes[ vertSmall ] = vertIndexes.size();
			vertIndexes[ vert ] = vertIndexes.size();
		};

		auto CreateTri = [&]( const glm::vec2 uv[3], const glm::vec3& norm, const glm::vec3& pos0, const glm::vec3& pos1, const glm::vec3& pos2 )
		{
			//vertSmall.normal = norm;
			vert.normal = norm;

			CreateVert( pos0, uv[0], norm );
			CreateVert( pos1, uv[1], norm );
			CreateVert( pos2, uv[2], norm );
		};

		// Check for adjacent blocks (TODO: check adjacent chunks)
		auto FaceHasNoAdjacentBlock = [&]( Faces face ) -> bool
		{
			BlockPosS blockPos = pos + FaceDirs[face];
			BlockPosTypeS& blockPoint = blockPos[FaceDirsVec[face]];

			if ( blockPoint == -1 || blockPoint == CHUNK_SIZE )
			{
				if ( blockPoint == -1 )
					blockPoint = CHUNK_SIZE - 1;

				else if ( blockPoint == CHUNK_SIZE )
					blockPoint = 0;

				if ( chunk->aAdjacentChunks[face] )
					return chunk->aAdjacentChunks[face]->GetBlockType( blockPos ) == BlockType::Air;

				return true;
			}

			return ( chunk->GetBlockType( blockPos ) == BlockType::Air );
		};

		// Create Top Face (+Z)
		if ( FaceHasNoAdjacentBlock( FaceTop ) )
		{
			CreateTri( uvGrass[0], FaceNorm[FaceTop],       { 1, 1, 1 }, { 0, 1, 1 }, { 0, 0, 1 } );
			CreateTri( uvGrass[1], FaceNorm[FaceTop],       { 1, 0, 1 }, { 1, 1, 1 }, { 0, 0, 1 } );
		}

		// Create Bottom Face (-Z)
		if ( FaceHasNoAdjacentBlock( FaceBottom ) )
		{
			CreateTri( uvGrass[0], FaceNorm[FaceBottom],    { 1, 1, 0 }, { 1, 0, 0 }, { 0, 0, 0 } );
			CreateTri( uvGrass[1], FaceNorm[FaceBottom],    { 0, 1, 0 }, { 1, 1, 0 }, { 0, 0, 0 } );
		}

		// Create Left Face (-X)
		if ( FaceHasNoAdjacentBlock( FaceLeft ) )
		{
			CreateTri( uvGrass[0], FaceNorm[FaceLeft],      { 0, 1, 0 }, { 0, 0, 0 }, { 0, 0, 1 } );
			CreateTri( uvGrass[1], FaceNorm[FaceLeft],      { 0, 1, 1 }, { 0, 1, 0 }, { 0, 0, 1 } );
		}

		// Create Right Face (+X)
		if ( FaceHasNoAdjacentBlock( FaceRight ) )
		{
			CreateTri( uvGrass[0], FaceNorm[FaceRight],     { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 } );
			CreateTri( uvGrass[1], FaceNorm[FaceRight],     { 1, 0, 1 }, { 1, 0, 0 }, { 1, 1, 1 } );
		}

		// Create Back Face (+Y)
		if ( FaceHasNoAdjacentBlock( FaceBack ) )
		{
			CreateTri( uvGrass[0], FaceNorm[FaceBack],      { 1, 0, 1 }, { 0, 0, 1 }, { 0, 0, 0 } );
			CreateTri( uvGrass[1], FaceNorm[FaceBack],      { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 0 } );
		}

		// Create Front Face (-Y)
		if ( FaceHasNoAdjacentBlock( FaceFront ) )
		{
			CreateTri( uvGrass[0], FaceNorm[FaceFront],     { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 0 } );
			CreateTri( uvGrass[1], FaceNorm[FaceFront],     { 0, 1, 1 }, { 1, 1, 1 }, { 0, 1, 0 } );
		}
	}

	if ( mesh->aVertices.size() > 0 )
	{
		// Set the material, create the vertex and index buffer if used, and move to the next chunk
		mesh->apMaterial = g_matGrass;

		materialsystem->CreateVertexBuffer( mesh );

		if ( mesh->aVertices.size() != mesh->aIndices.size() )
			materialsystem->CreateIndexBuffer( mesh );
	}
	else
	{
		// well damn, no blocks at all to render
		delete mesh;
		chunk->aMesh = nullptr;
	}
}


Chunk* VoxelWorld::GetChunk( const glm::ivec3& pos )
{
	// why is this slower than the for loop????
	/*size_t id = (pos.z * (WORLD_AREA) + pos.y * WORLD_SIZE + pos.x);

	if ( id < WORLD_VOLUME )
		return aChunks[id];*/

	for (Chunk* chunk: aChunks)
	{
		if ( chunk->aPos == pos )
			return chunk;
	}

	return nullptr;
};


// ===================================================================


size_t Chunk::GetId(  )
{
	return aPos.z * (WORLD_AREA) + aPos.y * WORLD_SIZE + aPos.x;
}


ChunkMesh* Chunk::GetMesh(  )
{
	return aMesh;
}


// NOTE: this can return numbers greater than a short so uh, yeah
BlockID Chunk::GetBlock( const BlockPos& pos )
{
	return pos.z * (CHUNK_AREA) + pos.y * CHUNK_SIZE + pos.x;
}


const BlockPos& Chunk::GetBlockPos( BlockID id )
{
	return aBlocks[id].aPos;
	//return aBlocks[id]->aPos;
}


// TODO: have this grab from adjacent chunks when needed
BlockType Chunk::GetBlockType( const glm::ivec3& pos )
{
	return aBlocks[ GetBlock(pos) ].aType;
}


BlockType Chunk::GetBlockType( BlockID pos )
{
	return aBlocks[pos].aType;
}


void Chunk::SetBlockType( const glm::ivec3& pos, BlockType type )
{
	aBlocks[ GetBlock(pos) ].aType = type;
}


void Chunk::SetBlockType( BlockID id, BlockType type )
{
	aBlocks[ id ].aType = type;
}



