#pragma once

struct Shape
{
    unsigned int index_count;
    unsigned int first_index;
    unsigned int base_vertex;
    int light_id;
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
    optix::float3 position;
    optix::float3 normal;
    optix::float3 color;
    optix::float2 tex_coords;
    optix::float2 padding;
};

struct PerRayData_radiance
{
    optix::float3 result;
    float importance;
};

