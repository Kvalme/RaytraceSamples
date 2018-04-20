#pragma once

#include "radeon_rays.h"
#include "tiny_obj_loader.h"
#include "scene.h"

RadeonRays::IntersectionApi* InitIntersectorApi();

void UploadSceneToIntersector(const Scene& scene, RadeonRays::IntersectionApi* api);

struct Params
{
    RadeonRays::float4 eye;
    RadeonRays::float4 near_far_and_size;
    RadeonRays::matrix view_proj_inv;
};

Params PrepareCameraParams(RadeonRays::float4 eye, RadeonRays::float4 center, RadeonRays::float4 up, RadeonRays::float4 near_far_dim);
