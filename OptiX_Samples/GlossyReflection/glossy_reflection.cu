/**********************************************************************
Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#include <optix.h>
#include <optixu/optixu_matrix.h>
#include <utils.h>
#include <sampler.h>
#include "microfacetggx.h"

using namespace optix;

rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );

rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );
rtDeclareVariable(unsigned int, primary_ray_type, , );
rtDeclareVariable(unsigned int, indirect_ray_type, , );
rtDeclareVariable(rtObject, top_object, , );
rtDeclareVariable(float,        scene_epsilon, , );
rtDeclareVariable(unsigned int, frame_no,,);

rtDeclareVariable(CameraParams, camera_params, , );

rtDeclareVariable(PerRayData_radiance, prd_radiance, rtPayload, );

rtDeclareVariable(float3, texcoord,         attribute texcoord, ); 
rtDeclareVariable(float3, geometric_normal, attribute geometric_normal, ); 
rtDeclareVariable(float3, shading_normal,   attribute shading_normal, ); 
rtDeclareVariable(float3, color, attribute color, );
rtDeclareVariable(float3, pos, attribute pos, );


rtDeclareVariable(float3, back_hit_point,   attribute back_hit_point, ); 
rtDeclareVariable(float3, front_hit_point,  attribute front_hit_point, ); 


rtBuffer<float4, 2>   output_buffer;

#define DENOM_EPS 1e-8f
#define ROUGHNESS 0.01f

RT_PROGRAM void GenerateCameraRays()
{
    // Get hold of the pixel
    float2 pixelPos = make_float2(launch_index.x, launch_index.y);
    // Convert to world space position
    float2 ndc = 2.0f * (pixelPos + 0.5f) * make_float2(camera_params.screen_dims.z, camera_params.screen_dims.w) - 1.0f;

    float2 ndc2 = ndc * make_float2(1.0f, -1.0f);

    float4 homogeneous = camera_params.view_proj_inv * make_float4(ndc2.x, ndc2.y, 0.0f, 1.0f);
    homogeneous.x /= homogeneous.w; // projection divide
    homogeneous.y /= homogeneous.w; // projection divide
    homogeneous.z /= homogeneous.w; // projection divide
    homogeneous.w /= homogeneous.w; // projection divide

    float3 eye = make_float3(camera_params.eye.x, camera_params.eye.y, camera_params.eye.z);

    // Create the camera ray
    optix::Ray ray(eye, normalize(make_float3(homogeneous.x, homogeneous.y, homogeneous.z) - eye), primary_ray_type, scene_epsilon);

    PerRayData_radiance prd;
    prd.importance = 1.f;

    rtTrace(top_object, ray, prd);

    // Write the ray out to memory
    output_buffer[launch_index] += prd.result;
}

RT_PROGRAM void MissHitPrimary()
{
    prd_radiance.result = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
}

RT_PROGRAM void ShadePrimaryRays()
{
    float pdf;
    float3 bxdf;
    float3 wo;
    float3 wi = -normalize(ray.direction);

    Sampler sampler;
    Sampler_Init(&sampler, launch_index.x + launch_index.y * 1920 + frame_no);;

    float2 sample = Sampler_Sample2D(&sampler);

    bxdf = MicrofacetGGX_Sample(ROUGHNESS, color, wi, sample, shading_normal, &wo, &pdf);

    optix::Ray indirect_ray(pos + shading_normal * 0.001f, normalize(wo), indirect_ray_type, scene_epsilon);

    prd_radiance.result = make_float4(bxdf / pdf, 1.0f);
    
    PerRayData_radiance prd;
    rtTrace(top_object, indirect_ray, prd);

    prd_radiance.result = prd.result;
}

RT_PROGRAM void ClosestHitIndirectRay()
{
    float pdf;
    float3 bxdf;
    float3 wo;
    float3 wi = -normalize(ray.direction);

    Sampler sampler;
    Sampler_Init(&sampler, launch_index.x + launch_index.y * 1920 + frame_no);;

    float2 sample = Sampler_Sample2D(&sampler);


    bxdf = MicrofacetGGX_Sample(ROUGHNESS, color, wi, sample, shading_normal, &wo, &pdf);
    prd_radiance.result = make_float4(bxdf / pdf, 2.0f);
}

RT_PROGRAM void MissHitIndirectRay()
{
    prd_radiance.result = make_float4(1.f, 0.f, 0.f, 1.0f);
}

RT_PROGRAM void Exception()
{
  output_buffer[launch_index] = make_float4(1.0f, 0.0f, 1.0f, 0.0f);
}

/*
    GLOBAL Shape const* restrict shapes,
    GLOBAL Vertex const* restrict vertices,
    GLOBAL uint const* restrict indices,
    GLOBAL Ray* restrict output_rays,
    GLOBAL Ray* restrict input_rays,
    GLOBAL Intersection const* restrict isects,
    int intersection_count,
    GLOBAL float4* restrict output,
    GLOBAL float4* restrict color_buffer,
    volatile GLOBAL uint * restrict ao_rays_counter,
    GLOBAL uint const* restrict max_output_rays,
    int ao_rays_per_frame,
    int frame_no
)
{
    // Get hold of the pixel
    const int gid = get_global_id(0);
    const int pixel_id = input_rays[gid].padding.x;

    const Intersection hit = isects[gid];
    if (gid < intersection_count)
    {
        // Miss
        if (hit.shapeid == INVALID_IDX)
        {
            output[pixel_id] = (float4)(0.0f, 0.0f, 0.0f, 1.0f);
            return;
        }

        Shape shape = shapes[hit.shapeid];
        Vertex v0 = vertices[shape.base_vertex + indices[shape.first_index + 3 * hit.primid + 0]];
        Vertex v1 = vertices[shape.base_vertex + indices[shape.first_index + 3 * hit.primid + 1]];
        Vertex v2 = vertices[shape.base_vertex + indices[shape.first_index + 3 * hit.primid + 2]];

        float3 color = (1.0f - hit.uvwt.x - hit.uvwt.y) * v0.color + hit.uvwt.x * v1.color + hit.uvwt.y * v2.color;
        float3 pos = (1.0f - hit.uvwt.x - hit.uvwt.y) * v0.position + hit.uvwt.x * v1.position + hit.uvwt.y * v2.position;
        float3 normal = (1.0f - hit.uvwt.x - hit.uvwt.y) * v0.normal + hit.uvwt.x * v1.normal + hit.uvwt.y * v2.normal;

        // Write color to output buffer
        color_buffer[pixel_id] = (float4)(color, 1.0f);

        Sampler sampler;
        Sampler_Init(&sampler, gid + frame_no);

        //Get location index
        int ray_idx = atomic_add(ao_rays_counter, ao_rays_per_frame);
        if (ray_idx + ao_rays_per_frame < *max_output_rays)
        {
            for (int a = 0; a < ao_rays_per_frame; ++a)
            {
                float2 sample = Sampler_Sample2D(&sampler);
                float3 dir = Sample_MapToHemisphere(sample, normal, 0.f);

                Ray ray;
                ray.o = (float4)(pos + normal * 0.001f, 100000.f);
                ray.d = (float4)(dir, 0.f);
                ray.extra.x = 0xffffffff;
                ray.extra.y = 0xffffffff;
                ray.padding.x = pixel_id;
                output_rays[ray_idx + a] = ray;
            }
        }
    }
}

KERNEL
void ProcessAO(
    GLOBAL Ray* restrict input_rays,
    GLOBAL Intersection const* restrict isects,
    int intersection_count,
    GLOBAL float4* restrict color_buffer,
    GLOBAL float4* restrict output
)
{
    // Get hold of the pixel
    const int gid = get_global_id(0);
    const int pixel_id = input_rays[gid].padding.x;

    const Intersection hit = isects[gid];
    if (gid < intersection_count)
    {
        // Miss
        if (hit.shapeid == INVALID_IDX)
        {
            output[pixel_id] += color_buffer[pixel_id];
            return;
        }
        else
        {
            output[pixel_id] += (float4)(0.0f, 0.0f, 0.0f, 1.0f);
            return;
        }
    }
}

KERNEL
void Resolve(
    GLOBAL float4* restrict output,
    int frame_count
)
{
    // Get hold of the pixel
    const int gid = get_global_id(0);
    output[gid] /= output[gid].w > 0.9f ? output[gid].w : 1.0f;
}



*/
