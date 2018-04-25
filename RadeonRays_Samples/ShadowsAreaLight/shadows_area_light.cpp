#include "radeon_rays.h"

#include <iostream>

#include "math/matrix.h"
#include "math/mathutils.h"
#include "utils.h"
#include "tiny_obj_loader.h"
#include "scene.h"
#include "radeon_rays_cl.h"

#include "CLWProgram.h"

#include "OpenImageIO/imageio.h"

using namespace RadeonRays;
using namespace tinyobj;

typedef struct
{
    float3 position;
    float3 intencity;
    uint32_t shape_id;
    float size;
    float2 padding;
} Light;

void GenCameraRays(CLWContext context, CLWProgram program, CLWBuffer<Params> camera_params_buffer, CLWBuffer<ray> rays_buffer,
    int w, int h)
{
    //Generate camera rays
    CLWKernel genkernel = program.GetKernel("GenerateCameraRays");
    int argid = 0;
    genkernel.SetArg(argid++, camera_params_buffer);
    genkernel.SetArg(argid++, w);
    genkernel.SetArg(argid++, h);
    genkernel.SetArg(argid++, rays_buffer);

    int globalsize = w * h;
    context.Launch1D(0, ((globalsize + 63) / 64) * 64, 64, genkernel);
}

void SaveImage(const std::string &fname, float *data, int w, int h)
{
    OIIO_NAMESPACE_USING;

    std::unique_ptr<ImageOutput> out{ ImageOutput::create(fname) };

    if (!out)
    {
        throw std::runtime_error("Can't create image file on disk");
    }

    auto fmt = TypeDesc::FLOAT;
    ImageSpec spec(w, h, 4, fmt);

    out->open(fname, spec);
    out->write_image(fmt, data);
    out->close();
}

std::vector<Light> PrepareLights(int count, Scene &scene)
{
    std::vector<Light> lights;
    for (int a = -(int)count/2; a <= (int)count/2; ++a)
    {
        Light light;
        light.position = float3(a * 2.f * 100.f + 50.f, 800.f, 0.f);
        light.intencity = float3(100.f, 100.f, 80.f);
        light.size = 100.f;
        light.shape_id = scene.meshes_.size();

        Mesh areaLight;
        areaLight.name_ = "areaLight";
        areaLight.vertices_ = 
        {
            // positions                                                                                                 normals              uv                 colors
            light.position.x + -50.f, light.position.y, light.position.z + -50.f,  0.0f, -1.0f, 0.0f,  0.0f, 0.0f, 0.0f,  1.0f, 1.0f, 0.8f,
            light.position.x + -50.f, light.position.y, light.position.z + 50.f,  0.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 0.8f,
            light.position.x + 50.f, light.position.y, light.position.z + 50.f,  0.0f, -1.0f, 0.0f,  1.0f, 1.0f, 0.0f,  1.0f, 1.0f, 0.8f,
            light.position.x + 50.f, light.position.y, light.position.z + -50.f,  0.0f, -1.0f, 0.0f,  1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 0.8f
        };

        areaLight.indices_ = { 0, 2, 1, 0, 3, 2 };
        areaLight.vertex_stride_ = 12 * sizeof(float);
        areaLight.index_stride_ = sizeof(uint32_t);
        areaLight.light_id_ = (int32_t)lights.size();
        
        scene.meshes_.push_back(areaLight);

        lights.push_back(light);
    }

    return lights;
}

int main(int argc, char* argv[])
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " -lc <light count> -rc <ray count per frame per light>"<< std::endl;
        return -1;
    }

    int light_count = 1;
    int rays_per_frame_per_light = 1;
    for (int a = 1; a < 5; ++a)
    {
        if (strcmp(argv[a], "-lc") == 0)
        {
            light_count = atoi(argv[++a]);
            continue;
        }
        if (strcmp(argv[a], "-rc") == 0)
        {
            rays_per_frame_per_light = atoi(argv[++a]);
            continue;
        }
    }


    CLWContext context = InitCLW(0, 0);
    IntersectionApi* intersection_api = InitIntersectorApi(context);

    Scene scene;
    scene.loadFile("../../Resources/Sponza/sponza.obj");
    //scene.loadFile("../../Resources/orig.obj");

    std::vector<Light> lights = PrepareLights(light_count, scene);

    UploadSceneToIntersector(scene, intersection_api);
    CLWBuffer<::Shape> shapes_buffer;
    CLWBuffer<Vertex> vertex_buffer;
    CLWBuffer<uint32_t> index_buffer;
    BuildSceneBuffers(context, scene, shapes_buffer, vertex_buffer, index_buffer);

    intersection_api->Commit();

    int w = 1920;
    int h = 1080;
    int shadow_rays_per_frame = w * h * rays_per_frame_per_light;
    int initial_rays_count = w * h; // 1 ray per pixel

    std::cout << "Primary rays: " << initial_rays_count << std::endl;
    std::cout << "Max shadow rays per frame: " << shadow_rays_per_frame << " (" << light_count << " lights "<< rays_per_frame_per_light<<" rays per light)" << std::endl;

    Params camera_params =
        PrepareCameraParams(float4(-11.f, 111.f, -54.f), float4(-9.f, 111.f, -54.f), float4(0.f, 1.f, 0.f),
            float4(0.1f, 10000.f), float2((float)w, (float)h));
    /*Params camera_params =
        PrepareCameraParams(float4(0.f, 1.f, 3.f), float4(0.f, 1.f, 2.f), float4(0.f, 1.f, 0.f),
            float4(0.1f, 10000.f), float2((float)w, (float)h));*/


    std::string options("-cl-mad-enable -cl-fast-relaxed-math -cl-std=CL1.2 -I .");
    CLWProgram program = CLWProgram::CreateFromFile("shadows_area_light.cl", options.c_str(), context);

    CLWBuffer<ray> primary_rays_buffer = context.CreateBuffer<ray>(initial_rays_count, CL_MEM_READ_WRITE);
    CLWBuffer<Intersection> primary_intersection_buffer = context.CreateBuffer<Intersection>(initial_rays_count, CL_MEM_READ_WRITE);

    CLWBuffer<float4> output_buffer = context.CreateBuffer<float4>(w*h, CL_MEM_READ_WRITE);
    
    CLWBuffer<Params> camera_params_buffer = context.CreateBuffer<Params>(1, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &camera_params);

    CLWBuffer<float4> color_buffer = context.CreateBuffer<float4>(w * h * rays_per_frame_per_light, CL_MEM_READ_WRITE);

    CLWBuffer<Light> lights_buffer = context.CreateBuffer<Light>(light_count, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, lights.data());

    CLWBuffer<uint32_t> shadow_rays_counter = context.CreateBuffer<uint32_t>(1, CL_MEM_READ_WRITE);
    CLWBuffer<ray> shadow_rays_buffer = context.CreateBuffer<ray>(shadow_rays_per_frame, CL_MEM_READ_WRITE);
    CLWBuffer<Intersection> shadow_intersection_buffer = context.CreateBuffer<Intersection>(shadow_rays_per_frame, CL_MEM_READ_WRITE);

    //Clear output buffer
    context.FillBuffer<float4>(0, output_buffer, float4(), output_buffer.GetElementCount());
    
    Buffer *ray_buffer = CreateFromOpenClBuffer(intersection_api, primary_rays_buffer);
    Buffer *isect_buffer = CreateFromOpenClBuffer(intersection_api, primary_intersection_buffer);
    Buffer *shadow_ray_buffer = CreateFromOpenClBuffer(intersection_api, shadow_rays_buffer);
    Buffer *shadow_isect_buffer = CreateFromOpenClBuffer(intersection_api, shadow_intersection_buffer);
    
    int frame_count = 100;
    for (int a = 0; a < frame_count; ++a)
    {
        context.FillBuffer<uint32_t>(0, shadow_rays_counter, 0, 1);

        //Gen camera rays
        GenCameraRays(context, program, camera_params_buffer, primary_rays_buffer, w, h);
        //Run intersector
        intersection_api->QueryIntersection(ray_buffer, initial_rays_count, isect_buffer, nullptr, nullptr);
        {
            //Shade and generate new rays
            CLWKernel kernel = program.GetKernel("ShadePrimaryRays");
            int argid = 0;
            kernel.SetArg(argid++, shapes_buffer);
            kernel.SetArg(argid++, vertex_buffer);
            kernel.SetArg(argid++, index_buffer);
            kernel.SetArg(argid++, shadow_rays_buffer);
            kernel.SetArg(argid++, primary_rays_buffer);
            kernel.SetArg(argid++, primary_intersection_buffer);
            kernel.SetArg(argid++, (cl_int)primary_intersection_buffer.GetElementCount());
            kernel.SetArg(argid++, output_buffer);
            kernel.SetArg(argid++, color_buffer);
            kernel.SetArg(argid++, light_count);
            kernel.SetArg(argid++, lights_buffer);
            kernel.SetArg(argid++, shadow_rays_counter);
            kernel.SetArg(argid++, rays_per_frame_per_light);
            kernel.SetArg(argid++, a);

            int globalsize = primary_rays_buffer.GetElementCount();
            context.Launch1D(0, ((globalsize + 63) / 64) * 64, 64, kernel);
        }

        //Trace shadow rays
        uint32_t shadow_rays_count;
        context.ReadBuffer<uint32_t>(0, shadow_rays_counter, &shadow_rays_count, 1).Wait();
        intersection_api->QueryIntersection(shadow_ray_buffer, shadow_rays_count, shadow_isect_buffer, nullptr, nullptr);

        //Process shadow rays
        {
            CLWKernel kernel = program.GetKernel("ProcessShadowRays");
            int argid = 0;
            kernel.SetArg(argid++, shadow_rays_buffer);
            kernel.SetArg(argid++, shadow_intersection_buffer);
            kernel.SetArg(argid++, shadow_rays_count);
            kernel.SetArg(argid++, color_buffer);
            kernel.SetArg(argid++, output_buffer);

            int globalsize = shadow_rays_count;
            context.Launch1D(0, ((globalsize + 63) / 64) * 64, 64, kernel);
        }
    }

    //Resolve image
    {
        CLWKernel kernel = program.GetKernel("Resolve");
        int argid = 0;
        kernel.SetArg(argid++, output_buffer);
        kernel.SetArg(argid++, frame_count);

        int globalsize = w * h;
        context.Launch1D(0, ((globalsize + 63) / 64) * 64, 64, kernel);
    }

    // dump image
    std::unique_ptr<float[]> image_data(new float[output_buffer.GetElementCount() * 4]);
    float4 *image;
    context.MapBuffer(0, output_buffer, CL_MAP_READ, &image).Wait();
    memcpy(image_data.get(), image, sizeof(float4) * output_buffer.GetElementCount());
    context.UnmapBuffer(0, output_buffer, image);

    SaveImage("shadows_area_light.jpg", image_data.get(), w, h);
}
