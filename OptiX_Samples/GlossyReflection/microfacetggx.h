#ifndef MICROFACET_GGX_CL
#define MICROFACET_GGX_CL

#include <optix.h>
#include <optixu/optixu_matrix.h>

#define DENOM_EPS 1e-8f

/*
Microfacet GGX
*/
// Distribution fucntion
__device__
float MicrofacetDistribution_GGX_D(float roughness, float3 m)
{
    float ndotm = fabs(m.y);
    float ndotm2 = ndotm * ndotm;
    float sinmn = sqrt(1.f - clamp(ndotm * ndotm, 0.f, 1.f));
    float tanmn = ndotm > DENOM_EPS ? sinmn / ndotm : 0.f;
    float a2 = roughness * roughness;
    float denom = (PI * ndotm2 * ndotm2 * (a2 + tanmn * tanmn) * (a2 + tanmn * tanmn));
    return denom > DENOM_EPS ? (a2 / denom) : 1.f;
}

// PDF of the given direction
__device__
float MicrofacetDistribution_GGX_GetPdf(
    // Halfway vector
    float3 m,
    // Rougness
    float roughness,
    // Incoming direction
    float3 wi,
    // Outgoing direction
    float3 wo
)
{
    float mpdf = MicrofacetDistribution_GGX_D(roughness, m) * fabs(m.y);
    // See Humphreys and Pharr for derivation
    float denom = (4.f * fabs(dot(wo, m)));

    return denom > DENOM_EPS ? mpdf / denom : 0.f;
}

// Sample the distribution
__device__
void MicrofacetDistribution_GGX_SampleNormal(
    // Roughness
    float roughness,
    // Sample
    float2 sample,
    // Outgoing  direction
    float3* wh
)
{
    float r1 = sample.x;
    float r2 = sample.y;

    // Sample halfway vector first, then reflect wi around that
    float theta = atan2(roughness * sqrt(r1), sqrt(1.f - r1));
    float costheta = cos(theta);
    float sintheta = sin(theta);

    // phi = 2*PI*ksi2
    float phi = 2.f * PI * r2;
    float cosphi = cos(phi);
    float sinphi = sin(phi);

    // Calculate wh
    *wh = make_float3(sintheta * cosphi, costheta, sintheta * sinphi);
}

//
__device__
float MicrofacetDistribution_GGX_G1(float roughness, float3 v, float3 m)
{
    float ndotv = fabs(v.y);
    float mdotv = fabs(dot(m, v));

    float sinnv = sqrt(1.f - clamp(ndotv * ndotv, 0.f, 1.f));
    float tannv = ndotv > DENOM_EPS ? sinnv / ndotv : 0.f;
    float a2 = roughness * roughness;
    return 2.f / (1.f + sqrt(1.f + a2 * tannv * tannv));
}

// Shadowing function also depends on microfacet distribution
__device__
float MicrofacetDistribution_GGX_G(float roughness, float3 wi, float3 wo, float3 wh)
{
    return MicrofacetDistribution_GGX_G1(roughness, wi, wh) * MicrofacetDistribution_GGX_G1(roughness, wo, wh);
}

__device__
float3 MicrofacetGGX_Evaluate(
    // Roughness
    float roughness,
    // Color
    float3 color,
    // Incoming direction
    float3 wi,
    // Outgoing direction
    float3 wo,
    float3 normal
)
{
    // Incident and reflected zenith angles
    float costhetao = fabs(dot(wo, normal));
    float costhetai = fabs(dot(wi, normal));

    // Calc halfway vector
    float3 wh = normalize(wi + wo);

    float denom = (4.f * costhetao * costhetai);

    return denom > DENOM_EPS ? color * MicrofacetDistribution_GGX_G(roughness, wi, wo, wh) * MicrofacetDistribution_GGX_D(roughness, wh) / denom : make_float3(0.f, 0.f, 0.f);
}

__device__
float MicrofacetGGX_GetPdf(
    // Roughness
    float roughness,
    // Incoming direction
    float3 wi,
    // Outgoing direction
    float3 wo
)
{
    float3 wh = normalize(wo + wi);

    return MicrofacetDistribution_GGX_GetPdf(wh, roughness, wi, wo);
}

__device__
float3 MicrofacetGGX_Sample(
    // Roughness
    float roughness,
    // Color
    float3 color,
    // Incoming direction
    float3 wi,
    // Sample
    float2 sample,
    // Normal
    float3 normal,
    // Outgoing  direction
    float3* wo,
    // PDF at wo
    float* pdf
)
{
    float3 wh;
    MicrofacetDistribution_GGX_SampleNormal(roughness, sample, &wh);

    *wo = -wi + 2.f*fabs(dot(wi, wh)) * wh;

    *pdf = MicrofacetDistribution_GGX_GetPdf(wh, roughness, wi, *wo);

    return MicrofacetGGX_Evaluate(roughness, color, wi, *wo, normal);
}

#endif
