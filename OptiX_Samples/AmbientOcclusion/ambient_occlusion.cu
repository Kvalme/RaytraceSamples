#include <optix.h>
#include <optixu/optixu_math_namespace.h>
#include <scene.h>

using namespace optix;

rtDeclareVariable(uint2, launch_index, rtLaunchIndex, );

rtDeclareVariable(optix::Ray, ray, rtCurrentRay, );

rtDeclareVariable(float4, camera_eye, , );
rtDeclareVariable(float4, camera_near_far, , );
rtDeclareVariable(float4, camera_screen_dims, , );
rtDeclareVariable(float4, eye, , );

optix::Matrix4x4 view_proj_inv;


rtBuffer<float4, 2>   output_buffer;

rtDeclareVariable(CameraParams, )

/*


rtDeclareVariable(float3,                draw_color, , );

RT_PROGRAM void draw_solid_color()
{
  output_buffer[launch_index] = make_float4(draw_color, 0.f);
}
*/

rtDeclareVariable()

RT_PROGRAM void GenerateCameraRays()
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


RT_PROGRAM void ShadePrimaryRays(
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



