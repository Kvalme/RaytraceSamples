#pragma once

struct Shape
{
    uint32_t index_count;
    uint32_t first_index;
    uint32_t base_vertex;
    int32_t light_id;
};

struct CameraParams
{
    optix::float4 eye;
    optix::float4 near_far;
    optix::float4 screen_dims;
    optix::Matrix4x4 view_proj_inv;
};

struct Vertex
{
    float3 position;
    float3 normal;
    float3 color;
    float2 tex_coords;
    float2 padding;
};
