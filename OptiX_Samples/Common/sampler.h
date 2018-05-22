#ifndef SAMPLER_CL
#define SAMPLER_CL

#include <optix.h>
#include <optixu/optixu_matrix.h>

using namespace optix;

#define PI 3.14159265358979323846f

typedef struct _Sampler
{
    uint index;
    uint dimension;
    uint scramble;
    uint padding;
} Sampler;

__device__
uint WangHash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

/// Return random unsigned
__device__
uint UniformSampler_SampleUint(Sampler* sampler)
{
    sampler->index = WangHash(1664525U * sampler->index + 1013904223U);
    return sampler->index;
}

/// Return random float
__device__
float UniformSampler_Sample1D(Sampler* sampler)
{
    return ((float)UniformSampler_SampleUint(sampler)) / 0xffffffffU;

}

__device__
void Sampler_Init(Sampler* sampler, uint seed)
{
    sampler->index = seed;
    sampler->scramble = 0;
    sampler->dimension = 0;
}

__device__
float2 Sampler_Sample2D(Sampler* sampler)
{
    float2 sample;
    sample.x = UniformSampler_Sample1D(sampler);
    sample.y = UniformSampler_Sample1D(sampler);
    return sample;
}

__device__
float Sampler_Sample1D(Sampler* sampler)
{
    return UniformSampler_Sample1D(sampler);
}

__device__
float3 GetOrthoVector(float3 n)
{
    float3 p;

    if (fabs(n.z) > 0.f) {
        float k = sqrt(n.y*n.y + n.z*n.z);
        p.x = 0; p.y = -n.z / k; p.z = n.y / k;
    }
    else {
        float k = sqrt(n.x*n.x + n.y*n.y);
        p.x = n.y / k; p.y = -n.x / k; p.z = 0;
    }

    return normalize(p);
}

/// Sample hemisphere with cos weight
__device__
float3 Sample_MapToHemisphere(
    // Sample
    float2 sample,
    // Hemisphere normal
    float3 n,
    // Cos power
    float e
)
{
    // Construct basis
    float3 u = GetOrthoVector(n);
    float3 v = cross(u, n);
    u = cross(n, v);

    // Calculate 2D sample
    float r1 = sample.x;
    float r2 = sample.y;

    // Transform to spherical coordinates
    float sinpsi = sin(2 * PI*r1);
    float cospsi = cos(2 * PI*r1);
    float costheta = pow(1.f - r2, 1.f / (e + 1.f));
    float sintheta = sqrt(1.f - costheta * costheta);

    // Return the result
    return normalize(u * sintheta * cospsi + v * sintheta * sinpsi + n * costheta);
}


#endif
