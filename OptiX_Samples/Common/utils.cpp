#include "utils.h"
#include <assert.h>
#include "CLWPlatform.h"
#include "radeon_rays_cl.h"

using namespace RadeonRays;

#define _USE_MATH_DEFINES
#include <math.h>
#define GRAD2RAD(x) {x * (float)M_PI / 180.f}

CLWContext InitCLW(int req_platform_index, int req_device_index)
{
    std::vector<CLWPlatform> platforms;

    CLWPlatform::CreateAllPlatforms(platforms);

    if (platforms.size() == 0)
    {
        throw std::runtime_error("No OpenCL platforms installed.");
    }

    if (req_platform_index >= (int)platforms.size())
        throw std::runtime_error("There is no such platform index");
    else if ((req_platform_index > 0) &&
        (req_device_index >= (int)platforms[req_platform_index].GetDeviceCount()))
        throw std::runtime_error("There is no such device index");

    bool hasprimary = false;

    int i = (req_platform_index >= 0) ? (req_platform_index) : 0;
    int d = (req_device_index >= 0) ? (req_device_index) : 0;

    int platforms_end = (req_platform_index >= 0) ?
        (req_platform_index + 1) : ((int)platforms.size());

    for (; i < platforms_end; ++i)
    {
        int device_end = 0;

        if (req_platform_index < 0 || req_device_index < 0)
            device_end = (int)platforms[i].GetDeviceCount();
        else
            device_end = req_device_index + 1;

        for (; d < device_end; ++d)
        {
            if (req_platform_index < 0)
            {
                if (platforms[i].GetDevice(d).GetType() != CL_DEVICE_TYPE_GPU)
                    continue;
            }

            return CLWContext::Create(platforms[i].GetDevice(d));
        }
    }
    throw std::runtime_error(
        "No devices was selected (probably device index type does not correspond with real device type).");

    return CLWContext();
}

RadeonRays::IntersectionApi* InitIntersectorApi(CLWContext context)
{
    return CreateFromOpenClContext(context, context.GetDevice(0).GetID(), context.GetCommandQueue(0));
}

void UploadSceneToIntersector(const Scene& scene, IntersectionApi* api)
{
    int id = 0;
    for (auto mesh : scene.meshes_)
    {
        float* vertdata = mesh.vertices_.data();
        int nvert = (int)(mesh.vertices_.size() / (mesh.vertex_stride_/sizeof(float)));
        int* indices = reinterpret_cast<int*>(mesh.indices_.data());
        int nfaces = (int)(mesh.indices_.size() / 3);
        RadeonRays::Shape* shape = api->CreateMesh(vertdata, nvert, mesh.vertex_stride_, indices, 0, nullptr, nfaces);
        shape->SetId(id++);

        assert(shape != nullptr);
        api->AttachShape(shape);
    }
}

void BuildSceneBuffers(CLWContext context, const Scene& scene, CLWBuffer<::Shape> &shapes, CLWBuffer<Vertex> &vertices, CLWBuffer<uint32_t> &indices)
{
    std::vector<::Shape> shapes_array;
    std::vector<Vertex> vertices_array;
    std::vector<uint32_t> indices_array;

    int id = 0;
    for (auto mesh : scene.meshes_)
    {
        ::Shape shape;
        shape.base_vertex = (uint32_t)(vertices_array.size());
        shape.first_index = (uint32_t)(indices_array.size());
        shape.light_id = -1;
        shape.index_count = (uint32_t)mesh.indices_.size();
        shapes_array.push_back(shape);

        indices_array.insert(indices_array.end(), mesh.indices_.begin(), mesh.indices_.end());

        for (size_t a = 0; a < mesh.vertices_.size(); a+=mesh.vertex_stride_/4)
        {
            float *data = mesh.vertices_.data() + a;
            Vertex v;
            v.position = float3(data[0], data[1], data[2]);
            v.normal = float3(data[3], data[4], data[5]);
            v.tex_coords = float2(data[6], data[7]);
            v.color = float3(data[9], data[10], data[11]);

            vertices_array.push_back(v);
        }
    }

    shapes = context.CreateBuffer<::Shape>(shapes_array.size(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, shapes_array.data());
    indices = context.CreateBuffer<uint32_t>(indices_array.size(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, indices_array.data());
    vertices = context.CreateBuffer<Vertex>(vertices_array.size(), CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, vertices_array.data());
}

Params PrepareCameraParams(float4 eye, float4 center, float4 up, float4 near_far, float2 screen_dims)
{
    Params camera_params;
    camera_params.eye = eye;
    camera_params.near_far = near_far;
    camera_params.screen_dims = float4(screen_dims.x, screen_dims.y, 1.f / screen_dims.x, 1.f / screen_dims.y);
    matrix projection = perspective_proj_fovy_lh_dx(GRAD2RAD(60), screen_dims.x / screen_dims.y, screen_dims.x, screen_dims.y);
    matrix view = lookat_lh_dx(camera_params.eye, center, up);
    camera_params.view_proj_inv = inverse(projection * view);
    return camera_params;
}
