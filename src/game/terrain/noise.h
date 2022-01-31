#pragma once

#if 0

#include <stdint.h>
#include <inttypes.h>

/*typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef float f32;
typedef double f64;

typedef f32 (*FNoise)(void *p, f32 s, f32 x, f32 z);

struct Noise
{
    u8 params[512];
    FNoise compute;
};

// Octave noise with n octaves and seed offset o
// Maximum amplitude is 2^0 + 2^1 + 2^2 ... 2^n = 2^(n+1) - 1
// i.e. for octave 8, values range between [-511, 511]
struct Octave
{
    s32 n, o;
};

// Combined noise where compute(x, z) = n.compute(x + m.compute(x, z), z)
struct Combined
{
    struct Noise *n, *m;
};

struct Basic
{
    s32 o;
};

struct ExpScale
{
    struct Noise *n;
    f32 exp, scale;
};

Noise octave(s32 n, s32 o);
Noise combined(struct Noise *n, struct Noise *m);
Noise basic(s32 o);
Noise expscale(struct Noise *n, f32 exp, f32 scale);*/


struct NoiseOptions
{
    int octaves;
    float amplitude;
    float smoothness;
    float roughness;
    float offset;
};


std::array<int, CHUNK_AREA> CreateChunkHeightMap( const glm::ivec3& position, int worldSize, int seed );
float GenerateSeed( const std::string& input );

float perlin( float x, float y );

#endif