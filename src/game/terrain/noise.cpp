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



#include <math.h>

/* Function to linearly interpolate between a0 and a1
* Weight w should be in the range [0.0, 1.0]
*/
float interpolate(float a0, float a1, float w)
{
    /* // You may want clamping by inserting:
    * if (0.0 > w) return a0;
    * if (1.0 < w) return a1;
    */
    return (a1 - a0) * w + a0;
    /* // Use this cubic interpolation [[Smoothstep]] instead, for a smooth appearance:
    * return (a1 - a0) * (3.0 - w * 2.0) * w * w + a0;
    *
    * // Use [[Smootherstep]] for an even smoother result with a second derivative equal to zero on boundaries:
    * return (a1 - a0) * ((w * (w * 6.0 - 15.0) + 10.0) * w * w * w) + a0;
    */
}

typedef struct {
    float x, y;
} vector2;

/* Create pseudorandom direction vector
*/
vector2 randomGradient(int ix, int iy)
{
    // No precomputed gradients mean this works for any number of grid coordinates
    const unsigned w = 8 * sizeof(unsigned);
    const unsigned s = w / 2; // rotation width
    unsigned a = ix, b = iy;
    a *= 3284157443; b ^= a << s | a >> w-s;
    b *= 1911520717; a ^= b << s | b >> w-s;
    a *= 2048419325;
    float random = a * (3.14159265 / ~(~0u >> 1)); // in [0, 2*Pi]
    vector2 v;
    v.x = sin(random); v.y = cos(random);
    return v;
}

// Computes the dot product of the distance and gradient vectors.
float dotGridGradient(int ix, int iy, float x, float y)
{
    // Get gradient from integer coordinates
    vector2 gradient = randomGradient(ix, iy);

    // Compute the distance vector
    float dx = x - (float)ix;
    float dy = y - (float)iy;

    // Compute the dot-product
    return (dx*gradient.x + dy*gradient.y);
}

// Compute Perlin noise at coordinates x, y
float perlin(float x, float y)
{
    // Determine grid cell coordinates
    int x0 = (int)x;
    int x1 = x0 + 1;
    int y0 = (int)y;
    int y1 = y0 + 1;

    // Determine interpolation weights
    // Could also use higher order polynomial/s-curve here
    float sx = x - (float)x0;
    float sy = y - (float)y0;

    // Interpolate between grid point gradients
    float n0, n1, ix0, ix1, value;

    n0 = dotGridGradient(x0, y0, x, y);
    n1 = dotGridGradient(x1, y0, x, y);
    ix0 = interpolate(n0, n1, sx);

    n0 = dotGridGradient(x0, y1, x, y);
    n1 = dotGridGradient(x1, y1, x, y);
    ix1 = interpolate(n0, n1, sx);

    value = interpolate(ix0, ix1, sy);
    return value;
}
