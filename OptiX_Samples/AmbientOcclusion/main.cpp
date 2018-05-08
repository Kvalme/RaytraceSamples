#include <iostream>
#include <fstream>

#include <memory>

#include "OpenImageIO/imageio.h"

//#include <optix.h>
#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_stream_namespace.h>
#include <array>
//#include <sutil.h>

#include "utils.h"
#include "tiny_obj_loader.h"
#include "scene.h"

using namespace tinyobj;

#define RT_CHECK_ERROR(x) assert((x) == RT_SUCCESS)

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

std::string read_ptx(const std::string fname)
{
    std::ifstream file(fname.c_str());
    if (file.good())
    {
        // Found usable source file
        std::stringstream source_buffer;
        source_buffer << file.rdbuf();
        return source_buffer.str();
    }
    return "";
}
using namespace optix;

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <rays_per_frame_per_hit>" << std::endl;
        return -1;
    }

    int w = 1920;
    int h = 1080;

    Context context = Context::create();
    context->setRayTypeCount(2);
    context->setEntryPointCount(1);
    context->setStackSize(4640);
    context["primary_ray_type"]->setUint(0);
    context["shadow_ray_type"]->setUint(1);
    context["scene_epsilon"]->setFloat(1.e-4f);

    Buffer output_buffer = context->createBuffer(RT_BUFFER_OUTPUT);
    output_buffer->setFormat(RT_FORMAT_FLOAT4);
    output_buffer->setSize(w, h);
    context["output_buffer"]->set(output_buffer);

    Program triangle_mesh_bounds = context->createProgramFromPTXFile("../cuda/draw_color.ptx", "mesh_bounds");
    Program triangle_mesh_intersect = context->createProgramFromPTXFile("../cuda/draw_color.ptx", "mesh_intersect");

    Program ray_gen_program = context->createProgramFromPTXFile("ambient_occlusion.ptx", "GenerateCameraRays");
    Program closest_hit_primary = context->createProgramFromPTXFile("ambient_occlusion.ptx", "ShadePrimaryRays");
    Program miss_hit_primary = context->createProgramFromPTXFile("ambient_occlusion.ptx", "MissHitPrimary");

//    Program miss_hit_shadow = context->createProgramFromPTXFile("ambient_occlusion.ptx", "MissHitShadow");


    context->setRayGenerationProgram(0, ray_gen_program);

    std::array<float, 3> draw_color = { 0.462f, 0.725f, 0.0f };
    context["draw_color"]->set3fv(draw_color.data());

    //Load scene
    //Scene scene;
    //scene.loadFile("../../Resources/Sponza/sponza.obj");
    //scene.loadFile("../../Resources/orig.obj");


    //Run
    context->validate();
    
    context->launch(0, w * h);


    //Read and save output buffer
    {
        void* imageData = output_buffer->map();
        
        std::vector<float> pix(w * h * 4);
        memcpy(pix.data(), imageData, w * h * 4 * sizeof(float));

        SaveImage("ambient_occlusion.jpg", pix.data(), w, h);
        
        output_buffer->unmap();
    }


    output_buffer->destroy();
    ray_gen_program->destroy();
    context->destroy();
}