#include "radeon_rays.h"
#include "utils.h"
#include "tiny_obj_loader.h"
#include "scene.h"

using namespace RadeonRays;
using namespace tinyobj;



int main(int argc, char* argv[])
{
    IntersectionApi* intersection_api = InitIntersectorApi();

    Scene scene;
    scene.loadFile("../../Resources/Sponza/sponza.obj");

    UploadSceneToIntersector(scene, intersection_api);

    intersection_api->Commit();


}
