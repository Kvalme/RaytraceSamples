#ifndef MICROFACET_GGX_CL
#define MICROFACET_GGX_CL

#define DENOM_EPS 1e-8f

float3 IdealReflect_Sample(
    // Preprocessed shader input data
    float3 color,
    // Incoming direction
    float3 wi,
    // Normal
    float3 normal,
    // Outgoing  direction
    float3* wo,
    // PDF at wo
    float* pdf)
{
    // Mirror reflect wi
    *wo = normalize(make_float3(-wi.x, wi.y, -wi.z));

    // PDF is infinite at that point, but deltas are going to cancel out while evaluating
    // so set it to 1.f
    *pdf = 1.f;

    float coswo = fabs(dot(*wo, normal));

    // Return reflectance value
    return coswo > DENOM_EPS ? (color * (1.f / coswo)) : 0.f;
}


#endif
