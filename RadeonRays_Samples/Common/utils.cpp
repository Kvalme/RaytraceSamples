#include "utils.h"
#include <assert.h>

using namespace RadeonRays;

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
