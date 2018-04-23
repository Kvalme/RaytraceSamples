#pragma once

#include "radeon_rays.h"
#include "tiny_obj_loader.h"
#include "scene.h"
#include "CLWContext.h"
#include "CLWBuffer.h"

struct Shape
{
    uint32_t index_count;
    uint32_t first_index;
    uint32_t base_vertex;
    int32_t light_id;
};

struct Vertex
{
    RadeonRays::float3 position;
    RadeonRays::float3 normal;
    RadeonRays::float3 color;
    RadeonRays::float2 tex_coords;
    RadeonRays::float2 padding;
};

CLWContext InitCLW(int req_platform_index, int req_device_index);

RadeonRays::IntersectionApi* InitIntersectorApi(CLWContext context);
void UploadSceneToIntersector(const Scene& scene, RadeonRays::IntersectionApi* api);

void BuildSceneBuffers(CLWContext context, const Scene& scene, CLWBuffer<::Shape> &shapes, CLWBuffer<Vertex> &vertices, CLWBuffer<uint32_t> &indices);


struct Params
{
    RadeonRays::float4 eye;
    RadeonRays::float4 near_far;
    RadeonRays::float4 screen_dims;
    RadeonRays::matrix view_proj_inv;
};

Params PrepareCameraParams(RadeonRays::float4 eye, RadeonRays::float4 center, RadeonRays::float4 up, 
    RadeonRays::float4 near_far, RadeonRays::float2 screen_dims);
