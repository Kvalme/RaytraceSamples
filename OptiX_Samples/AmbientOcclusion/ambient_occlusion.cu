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

using namespace optix;

rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );

rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );
rtDeclareVariable(unsigned int, primary_ray_type, , );
rtDeclareVariable(unsigned int, shadow_ray_type, , );
rtDeclareVariable(rtObject, top_object, , );
rtDeclareVariable(float,        scene_epsilon, , );
rtDeclareVariable(unsigned int, ao_rays_per_frame, , );
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
    prd_radiance.result = make_float4(1.0f, 0.0f, 0.0f, 1.0f);
}

RT_PROGRAM void ShadePrimaryRays()
{
    Sampler sampler;
    Sampler_Init(&sampler, launch_index.x +  launch_index.y * 1920 + frame_no);
    float3 c = make_float3(0.f, 0.f, 0.f);
    for (int a = 0; a < ao_rays_per_frame; ++a)
    {
        float2 sample = Sampler_Sample2D(&sampler);
        float3 dir = Sample_MapToHemisphere(sample, shading_normal, 0.f);

        optix::Ray ray(pos + shading_normal * 0.001f, dir, shadow_ray_type, scene_epsilon);
        rtTrace(top_object, ray, prd_radiance);
        c += color * prd_radiance.importance;
    }
    prd_radiance.result = make_float4(c.x, c.y, c.z, ao_rays_per_frame);
}

RT_PROGRAM void AnyHitShadowRay()
{
    prd_radiance.importance = 0.0f;
    rtTerminateRay();
}

RT_PROGRAM void MissHitShadowRay()
{
    prd_radiance.importance = 1.0f;
}

RT_PROGRAM void Exception()
{
  output_buffer[launch_index] = make_float4(1.0f, 0.0f, 1.0f, 0.0f);
}
