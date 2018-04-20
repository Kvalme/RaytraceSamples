#include "radeon_rays.h"

#include <iostream>

#include "math/matrix.h"
#include "math/mathutils.h"
#include "utils.h"
#include "tiny_obj_loader.h"
#include "scene.h"

using namespace RadeonRays;
using namespace tinyobj;


int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <rays_per_frame_per_hit>" << std::endl;
        return -1;
    }

    IntersectionApi* intersection_api = InitIntersectorApi();

    Scene scene;
    scene.loadFile("../../Resources/Sponza/sponza.obj");

    UploadSceneToIntersector(scene, intersection_api);

    intersection_api->Commit();

    int rays_per_frame_per_hit = atoi(argv[1]);
    int w = 1920;
    int h = 1080;
    int rays_per_frame = w * h * rays_per_frame_per_hit;
    int initial_rays_count = w * h; // 1 ray per pixel

    std::cout << "Indirect rays per frame: " << rays_per_frame << " (" << rays_per_frame_per_hit << " per hit)" << std::endl;

    std::vector<ray> initial_rays(initial_rays_count);
    std::vector<ray> indirect_rays(rays_per_frame);

    Params camera_params =
        PrepareCameraParams(float4(-11.f, 111.f, -54.f), float4(-9.f, 111.f, -54.f), float4(0.f, 1.f, 0.f),
            float4(0.1f, 10000.f, (float)w, (float)h));

}
