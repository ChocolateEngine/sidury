#pragma once

#if 0

#include "graphics/imesh.h"
#include "graphics/igraphics.h"

#include "../physics.h"

#include <glm/glm.hpp>

#include <unordered_map>
#include <vector>


using uchar = unsigned char;
constexpr unsigned int CHUNK_SIZE = 100;  // 100  // 20 - raised for less draw calls
constexpr unsigned int CHUNK_AREA = CHUNK_SIZE * CHUNK_SIZE;
constexpr unsigned int CHUNK_VOLUME = CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE;

constexpr unsigned int WORLD_SIZE = 1;
constexpr unsigned int WORLD_HEIGHT = 1;
constexpr unsigned int WORLD_AREA = WORLD_SIZE * WORLD_SIZE;
constexpr unsigned int WORLD_VOLUME = WORLD_SIZE * WORLD_SIZE * WORLD_HEIGHT;

// constexpr float BLOCK_SCALE = .5f;
#if BULLET_PHYSICS
constexpr float BLOCK_SCALE = 50.0f;
#else
constexpr float BLOCK_SCALE = 1.0f;
#endif

using BlockID = unsigned int;

using BlockPosType = unsigned char;
using BlockPosTypeS = char;

using BlockPos = glm::vec<3, BlockPosType, glm::defaultp>;
using BlockPosS = glm::vec<3, BlockPosTypeS, glm::defaultp>;


class Chunk;
class ChunkMesh;


enum class BlockType: unsigned char
{
	Air = 0,
	Grass,
	Dirt,
};


// uh
struct Block
{
	BlockPos aPos{};
	BlockType aType = BlockType::Air;
};


// XX bytes
class Chunk
{
public:
	size_t                  GetId(  );
	ChunkMesh*              GetMesh(  );

	void                    CreatePhysicsMesh(  );

	BlockID                 GetBlock( const BlockPos& pos );
	const BlockPos&         GetBlockPos( BlockID pos );

	BlockType               GetBlockType( BlockID id );
	void                    SetBlockType( BlockID id, BlockType type );

	BlockType               GetBlockType( const glm::ivec3& pos );
	void                    SetBlockType( const glm::ivec3& pos, BlockType type );

	// 48 bytes !!!
	Chunk*                  aAdjacentChunks[6]{};

	// could be an std::array
	std::vector< Block >    aBlocks;
	//std::vector< Block* >    aBlocks;
	// YEAH WOOOOO 4,000,072 BYTES AT 100 CHUNK_SIZE !!!!!!!
	//Block                   aBlocks[CHUNK_VOLUME];
	// Block*                  aBlocks[CHUNK_VOLUME];

	glm::ivec3              aPos;

	// TODO: figure out how to make a combined UV for all textures so that way there is less draw calls
	ChunkMesh*              aMesh = nullptr;

#if BULLET_PHYSICS
	PhysicsObject*                  apPhysObj = nullptr;
#endif
};


// Custom Chunk Mesh Class
class ChunkMesh: public IMesh
{
public:
	virtual ~ChunkMesh() {}

	// you could go overboard with this, and get rid of the 8 bytes, and search all chunks for the address of the ChunkMesh
	// but that could probably be pretty damn slow so uh
	inline glm::mat4                GetModelMatrix() override
	{
		return glm::translate( glm::vec3(apChunk->aPos * (int)CHUNK_SIZE) * BLOCK_SCALE );
	}

	Chunk*                          apChunk = nullptr;
};


class VoxelWorld
{
public:
	void Init(  );
	void Update( float frameTime );

	void CreateWorld(  );
	void CreateChunk( const glm::ivec3& pos );
	void CreateChunkMesh( Chunk* chunk );

	Chunk* GetChunk( const glm::ivec3& pos );

	std::vector< Chunk* > aChunks;
};


extern VoxelWorld* voxelworld;

#endif