#include <iostream>

#include "math/matrix.h"
#include "math/mathutils.h"
#include "utils.h"
#include "tiny_obj_loader.h"
#include "scene.h"
#include "radeon_rays_cl.h"

#include "CLWProgram.h"

#include "OpenImageIO/imageio.h"

#include <chrono>

using namespace RadeonRays;
using namespace tinyobj;

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

int main(int argc, char* argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: " << argv[0] << " <rays_per_frame_per_hit>" << std::endl;
            return -1;
        }

        CLWContext context = InitCLW(1, 0);
        std::cout << "CLW done" << std::endl;

        IntersectionApi* intersection_api = InitIntersectorApi(context);

        Scene scene;
        scene.loadFile("../../Resources/Sponza/sponza.obj");
        //scene.loadFile("../../Resources/orig.obj");

        UploadSceneToIntersector(scene, intersection_api);
        CLWBuffer<::Shape> shapes_buffer;
        CLWBuffer<Vertex> vertex_buffer;
        CLWBuffer<uint32_t> index_buffer;
        BuildSceneBuffers(context, scene, shapes_buffer, vertex_buffer, index_buffer);

        intersection_api->Commit();

        int ao_rays_per_frame_per_hit = atoi(argv[1]);
        int w = 1920;
        int h = 1080;
        int ao_rays_per_frame = w * h * ao_rays_per_frame_per_hit;
        int initial_rays_count = w * h; // 1 ray per pixel

        std::cout << "Primary rays: " << initial_rays_count << std::endl;
        std::cout << "Max ao rays per frame: " << ao_rays_per_frame << " (" << ao_rays_per_frame_per_hit << " per hit)" << std::endl;

        Params camera_params =
            PrepareCameraParams(float4(-11.f, 111.f, -54.f), float4(-9.f, 111.f, -54.f), float4(0.f, 1.f, 0.f),
                float4(0.1f, 10000.f), float2((float)w, (float)h));
        /*Params camera_params =
            PrepareCameraParams(float4(0.f, 1.f, 3.f), float4(0.f, 1.f, 2.f), float4(0.f, 1.f, 0.f),
                float4(0.1f, 10000.f), float2((float)w, (float)h));*/


        std::string options("-cl-mad-enable -cl-fast-relaxed-math -cl-std=CL1.2 -I .");
        CLWProgram program = CLWProgram::CreateFromFile("ambient_occlusion.cl", options.c_str(), context);

        CLWBuffer<ray> primary_rays_buffer = context.CreateBuffer<ray>(initial_rays_count, CL_MEM_READ_WRITE);
        CLWBuffer<Intersection> primary_intersection_buffer = context.CreateBuffer<Intersection>(initial_rays_count, CL_MEM_READ_WRITE);

        CLWBuffer<ray> ao_rays_buffer = context.CreateBuffer<ray>(ao_rays_per_frame, CL_MEM_READ_WRITE);
        CLWBuffer<int> ao_hit_result = context.CreateBuffer<int>(ao_rays_per_frame, CL_MEM_READ_WRITE);
        CLWBuffer<Intersection> ao_intersection_buffer = context.CreateBuffer<Intersection>(ao_rays_per_frame, CL_MEM_READ_WRITE);
        CLWBuffer<uint32_t> max_ao_rays = context.CreateBuffer<uint32_t>(1, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &ao_rays_per_frame);
        CLWBuffer<uint32_t> ao_rays_counter = context.CreateBuffer<uint32_t>(1, CL_MEM_READ_WRITE);

        CLWBuffer<float4> output_buffer = context.CreateBuffer<float4>(w*h, CL_MEM_READ_WRITE);

        CLWBuffer<Params> camera_params_buffer = context.CreateBuffer<Params>(1, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, &camera_params);

        CLWBuffer<float4> color_buffer = context.CreateBuffer<float4>(w*h, CL_MEM_READ_WRITE);

        //Clear output buffer
        context.FillBuffer<float4>(0, output_buffer, float4(), output_buffer.GetElementCount());


        Buffer *ray_buffer = CreateFromOpenClBuffer(intersection_api, primary_rays_buffer);
        Buffer *isect_buffer = CreateFromOpenClBuffer(intersection_api, primary_intersection_buffer);
        Buffer *ao_ray_buffer = CreateFromOpenClBuffer(intersection_api, ao_rays_buffer);
        Buffer *ao_isect_buffer = CreateFromOpenClBuffer(intersection_api, ao_intersection_buffer);
        Buffer *ao_hit_result_buffer = CreateFromOpenClBuffer(intersection_api, ao_hit_result);

        int frame_count = 100;
        std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
        start = std::chrono::high_resolution_clock::now();

        for (int a = 0; a < frame_count; ++a)
        {
            context.FillBuffer<uint32_t>(0, ao_rays_counter, 0, 1);

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
                kernel.SetArg(argid++, ao_rays_buffer);
                kernel.SetArg(argid++, primary_rays_buffer);
                kernel.SetArg(argid++, primary_intersection_buffer);
                kernel.SetArg(argid++, (cl_int)primary_intersection_buffer.GetElementCount());
                kernel.SetArg(argid++, output_buffer);
                kernel.SetArg(argid++, color_buffer);
                kernel.SetArg(argid++, ao_rays_counter);
                kernel.SetArg(argid++, max_ao_rays);
                kernel.SetArg(argid++, ao_rays_per_frame_per_hit);
                kernel.SetArg(argid++, a);

                int globalsize = primary_rays_buffer.GetElementCount();
                context.Launch1D(0, ((globalsize + 63) / 64) * 64, 64, kernel);
            }

            //Trace AO rays
            uint32_t ao_rays_count;
            context.ReadBuffer<uint32_t>(0, ao_rays_counter, &ao_rays_count, 1).Wait();
            //intersection_api->QueryIntersection(ao_ray_buffer, ao_rays_count, ao_isect_buffer, nullptr, nullptr);
            intersection_api->QueryOcclusion(ao_ray_buffer, ao_rays_count, ao_hit_result_buffer, nullptr, nullptr);
            //Process AO
            {
                CLWKernel kernel = program.GetKernel("ProcessAO");
                int argid = 0;
                kernel.SetArg(argid++, ao_rays_buffer);
                kernel.SetArg(argid++, ao_hit_result);
                kernel.SetArg(argid++, ao_rays_count);
                kernel.SetArg(argid++, color_buffer);
                kernel.SetArg(argid++, output_buffer);

                int globalsize = ao_rays_count;
                context.Launch1D(0, ((globalsize + 63) / 64) * 64, 64, kernel);
            }
        }

        end = std::chrono::high_resolution_clock::now();
        double elapsed_s = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) / 1000.;
        std::cout << frame_count << " frames, " << initial_rays_count << " primary rays, " << ao_rays_per_frame << " indirect rays - " << elapsed_s << " s" << std::endl;
        std::cout << "fps: " << (frame_count / elapsed_s) << ", " << (((initial_rays_count + ao_rays_per_frame) * frame_count) / (elapsed_s)) / 1e6 << " MRays/s" << std::endl;


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

        SaveImage("ambient_occlusion.jpg", image_data.get(), w, h);
    }
    catch (CLWException& e)
    {
        std::cerr << e.what() << std::endl;
    }
}
