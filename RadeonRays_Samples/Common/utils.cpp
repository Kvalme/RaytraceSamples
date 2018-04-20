#include "utils.h"
#include <assert.h>

using namespace RadeonRays;

#define _USE_MATH_DEFINES
#include <math.h>
#define GRAD2RAD(x) {x * (float)M_PI / 180.f}


RadeonRays::IntersectionApi* InitIntersectorApi()
{
    int nativeidx = -1;
    // Always use OpenCL
    IntersectionApi::SetPlatform(DeviceInfo::kOpenCL);
    for (auto idx = 0U; idx < IntersectionApi::GetDeviceCount(); ++idx)
    {
        DeviceInfo devinfo;
        IntersectionApi::GetDeviceInfo(idx, devinfo);

        if (devinfo.type == DeviceInfo::kGpu && nativeidx == -1)
        {
            nativeidx = idx;
        }
    }
    assert(nativeidx != -1);
    IntersectionApi* api = IntersectionApi::Create(nativeidx);
    return api;
}

void UploadSceneToIntersector(const Scene& scene, IntersectionApi* api)
{
    int id = 0;
    for (auto mesh : scene.meshes_)
    {
        float* vertdata = mesh.vertices_.data();
        int nvert = mesh.vertices_.size() / (mesh.vertex_stride_/4);
        int* indices = reinterpret_cast<int*>(mesh.indices_.data());
        int nfaces = mesh.indices_.size() / 3;
        Shape* shape = api->CreateMesh(vertdata, nvert, mesh.vertex_stride_, indices, mesh.index_stride_, nullptr, nfaces);

        assert(shape != nullptr);
        api->AttachShape(shape);
        shape->SetId(id++);
    }
}

Params PrepareCameraParams(float4 eye, float4 center, float4 up, float4 near_far_dim)
{
    Params camera_params;
    camera_params.eye = eye;
    camera_params.near_far_and_size = near_far_dim;
    matrix projection = perspective_proj_fovy_lh_dx(GRAD2RAD(60), near_far_dim.z / near_far_dim.w, near_far_dim.x, near_far_dim.y);
    matrix view = lookat_lh_dx(camera_params.eye, center, up);
    camera_params.view_proj_inv = inverse(projection * view);
    return camera_params;
}
