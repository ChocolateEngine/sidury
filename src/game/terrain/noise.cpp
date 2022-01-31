#if 0

#include "terrain.h"
#include "noise.h"

#include <glm/gtc/noise.hpp>

#include <iostream>
#include <cstdint>
#include <cstring>

/*
float trilinearInterpolation(float blf, float blb, float brf, float brb,
                                float tlf, float tlb, float trf, float trb,
                                const glm::vec3 &point)
{
    return (blf * (1 - point.x) * (1 - point.y) * (1 - point.z)) +
            (brf * point.x * (1 - point.y) * (1 - point.z)) +
            (blb * (1 - point.x) * point.y * (1 - point.z)) +
            (tlf * (1 - point.x) * (1 - point.y) * point.z) +
            (trf * point.x * (1 - point.y) * point.z) +
            (tlb * (1 - point.x) * point.y * point.z) +
            (brb * point.x * point.y * (1 - point.z)) +
            (trb * point.x * point.y * point.z);
}
*/


// THANKS! Karasa and K.jpg for help with this algo
float rounded(const glm::vec2& coord)
{
    auto bump = [](float t) { return glm::max(0.0f, 1.0f - std::pow(t, 6.0f)); };
    float b = bump(coord.x) * bump(coord.y);
    return b * 0.9f;
}

float getNoiseAt(const glm::vec2& voxelPosition, const glm::vec2& chunkPosition,
                    const NoiseOptions& options, int seed)
{
    // Get voxel X/Z positions
    float voxelX = voxelPosition.x + chunkPosition.x * CHUNK_SIZE;
    float voxelZ = voxelPosition.y + chunkPosition.y * CHUNK_SIZE;

    // Begin iterating through the octaves
    float value = 0;
    float accumulatedAmps = 0;
    for (int i = 0; i < options.octaves; i++) {
        float frequency = glm::pow(2.0f, i);
        float amplitude = glm::pow(options.roughness, i);

        float x = voxelX * frequency / options.smoothness;
        float y = voxelZ * frequency / options.smoothness;

        float noise = glm::simplex(glm::vec3{seed + x, seed + y, seed});
        noise = (noise + 1.0f) / 2.0f;
        value += noise * amplitude;
        accumulatedAmps += amplitude;
    }
    return value / accumulatedAmps;
}


std::array<int, CHUNK_AREA> CreateChunkHeightMap( const glm::ivec3& position, int worldSize, int seed )
{
    const float WOLRD_SIZE = static_cast<float>(worldSize) * CHUNK_SIZE;

    NoiseOptions firstNoise;
    firstNoise.amplitude = 105;
    firstNoise.octaves = 6;
    firstNoise.smoothness = 205.f;
    firstNoise.roughness = 0.58f;
    firstNoise.offset = 18;

    NoiseOptions secondNoise;
    secondNoise.amplitude = 20;
    secondNoise.octaves = 4;
    secondNoise.smoothness = 200;
    secondNoise.roughness = 0.45f;
    secondNoise.offset = 0;

    glm::vec2 chunkXY = {position.x, position.y};

    std::array<int, CHUNK_AREA> heightMap;
    for (int y = 0; y < CHUNK_SIZE; y++)
    {
        for (int x = 0; x < CHUNK_SIZE; x++)
        {
            float bx = x + position.x * CHUNK_SIZE;
            float by = y + position.y * CHUNK_SIZE;

            glm::vec2 coord = (glm::vec2{bx, by} - WOLRD_SIZE / 2.0f) / WOLRD_SIZE * 2.0f;

            auto noise = getNoiseAt({x, y}, chunkXY, firstNoise, seed);
            auto noise2 = getNoiseAt({x, y}, {position.x, position.y}, secondNoise, seed);
            auto island = rounded(coord) * 1.25;
            float result = noise * noise2;

            heightMap[y * CHUNK_SIZE + x] =
                static_cast<int>((result * firstNoise.amplitude + firstNoise.offset) * island) - 5;
        }
    }

    return heightMap;
}


float GenerateSeed( const std::string& input )
{
    std::hash<std::string> strHash;

    float seedFloat;
    uint32_t hash = strHash(input);
    std::memcpy(&seedFloat, &hash, sizeof(float));
    return seedFloat;
}

#endif