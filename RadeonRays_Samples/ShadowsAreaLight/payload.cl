#ifndef PAYLOAD_CL
#define PAYLOAD_CL

typedef struct
{
    float4 o;
    float4 d;
    int2 extra;
    int2 padding;
} Ray;

typedef struct
{
    float4 m0;
    float4 m1;
    float4 m2;
    float4 m3;
} matrix4x4;

typedef struct
{
    float4 eye;
    float4 near_far;
    float4 screen_dims;
    matrix4x4 view_proj_inv;
} CameraParams;

typedef struct
{
    uint index_count;
    uint first_index;
    uint base_vertex;
    int light_id;
} Shape;

typedef struct
{
    float3 position;
    float3 normal;
    float3 color;
    float2 tex_coords;
    float2 padding;
}  Vertex;

typedef struct
{
    float3 position;
    float3 intencity;
    uint shape_id;
    float size;
    float2 padding;

} Light;


#endif
