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

#include <../Common/common.cl>
#include <payload.cl>
#include <../Common/utils.cl>
#include <../Common/isect.cl>
#include <../Common/sampler.cl>

KERNEL
void GenerateCameraRays(
    GLOBAL CameraParams const* restrict camera_params,
    int output_width,
    int output_height,
    GLOBAL Ray* restrict rays
)
{
    // Get hold of the pixel
    const int gid = get_global_id(0);
    
    float2 pixelPos = (float2)(gid % output_width, gid / output_width);

    // Convert to world space position
    float2 ndc = 2.0f * (pixelPos + 0.5f) * camera_params->screen_dims.zw - 1.0f;

    float4 homogeneous = matrix_mul_vector4(camera_params->view_proj_inv, (float4)(ndc * (float2)(1.0f, -1.0f), 0.0f, 1.0f));
    homogeneous.xyz /= homogeneous.w; // projection divide
    

    // Create the camera ray
    Ray ray;

    ray.d = (float4)(normalize(homogeneous.xyz - camera_params->eye.xyz), 0.0f);
    ray.o = camera_params->eye;
    ray.o.w = 100000.f;
    ray.extra.x = 0xffffffff;
    ray.extra.y = 0xffffffff;
    ray.padding.x = gid;


    // Write the ray out to memory
    rays[gid] = ray;
}


KERNEL
void ShadePrimaryRays(
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
        float3 pos = (1.0f -hit.uvwt.x - hit.uvwt.y) * v0.position + hit.uvwt.x * v1.position + hit.uvwt.y * v2.position;
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
    output[gid] /=  output[gid].w > 0.9f ? output[gid].w : 1.0f;
}



