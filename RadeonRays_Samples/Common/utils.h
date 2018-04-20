#pragma once

#include "radeon_rays.h"
#include "tiny_obj_loader.h"
#include "scene.h"

RadeonRays::IntersectionApi* InitIntersectorApi();

void UploadSceneToIntersector(const Scene& scene, RadeonRays::IntersectionApi* api);


