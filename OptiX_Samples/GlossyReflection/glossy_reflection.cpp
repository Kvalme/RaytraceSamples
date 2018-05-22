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

#include "../../RadeonRays_SDK/RadeonRays/include/math/matrix.h"
#include "../../RadeonRays_SDK/RadeonRays/include/math/mathutils.h"

#include <chrono>

#define _USE_MATH_DEFINES
#include <math.h>
#define GRAD2RAD(x) {x * (float)M_PI / 180.f}


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
    try
    {
        int w = 1920;
        int h = 1080;

        Context context = Context::create();
        context->setRayTypeCount(2);
        context->setEntryPointCount(1);
        context->setStackSize(4640);
        context["primary_ray_type"]->setUint(0);
        context["indirect_ray_type"]->setUint(1);
        context["scene_epsilon"]->setFloat(1.e-4f);

        Buffer output_buffer = context->createBuffer(RT_BUFFER_OUTPUT);
        output_buffer->setFormat(RT_FORMAT_FLOAT4);
        output_buffer->setSize(w, h);
        context["output_buffer"]->set(output_buffer);

        Program triangle_mesh_bounds = context->createProgramFromPTXFile("../cuda/triangle_mesh.ptx", "mesh_bounds");
        Program triangle_mesh_intersect = context->createProgramFromPTXFile("../cuda/triangle_mesh.ptx", "mesh_intersect");

        Program ray_gen_program = context->createProgramFromPTXFile("glossy_reflection.ptx", "GenerateCameraRays");
        Program closest_hit_primary = context->createProgramFromPTXFile("glossy_reflection.ptx", "ShadePrimaryRays");
        Program miss_hit_primary = context->createProgramFromPTXFile("glossy_reflection.ptx", "MissHitPrimary");
        Program closest_hit_indirect = context->createProgramFromPTXFile("glossy_reflection.ptx", "ClosestHitIndirectRay");
        Program miss_hit_indirect = context->createProgramFromPTXFile("glossy_reflection.ptx", "MissHitIndirectRay");

        Program exception_program = context->createProgramFromPTXFile("glossy_reflection.ptx", "Exception");

        context->setRayGenerationProgram(0, ray_gen_program);
        context->setMissProgram(0, miss_hit_primary);
        context->setMissProgram(1, miss_hit_indirect);
        context->setExceptionProgram(0, exception_program);

        //Load scene
        Scene scene;
        scene.loadFile("../../Resources/Sponza/sponza.obj");
        //scene.loadFile("../../Resources/orig.obj");

        //Create optix geometry
        {
            GeometryGroup geometrygroup = context->createGeometryGroup();

            std::vector<optix::float3> vertices;
            std::vector<optix::float3> normals;
            std::vector<optix::float2> texcoord;
            std::vector<optix::float3> colors;
            std::vector<int> indices;
            std::vector<int> materials;

            for (auto mesh : scene.meshes_)
            {
                int index_offset = vertices.size();
                //copy vertex data into arrays
                for (int a = 0; a < mesh.vertices_.size(); a += (mesh.vertex_stride_ / sizeof(float)))
                {
                    vertices.push_back(
                        optix::make_float3(mesh.vertices_[a], mesh.vertices_[a + 1], mesh.vertices_[a + 2]));

                    normals.push_back(
                        optix::make_float3(mesh.vertices_[a + 3], mesh.vertices_[a + 4], mesh.vertices_[a + 5]));

                    texcoord.push_back(
                        optix::make_float2(mesh.vertices_[a + 6], mesh.vertices_[a + 7]));

                    colors.push_back(
                        optix::make_float3(mesh.vertices_[a + 9], mesh.vertices_[a + 10], mesh.vertices_[a + 11]));
                }

                //copy indices with adding index_offset
                for (auto index : mesh.indices_)
                {
                    indices.push_back(index + index_offset);
                }
            }

            int num_triangles = indices.size() / 3;

            optix::Geometry geometry = context->createGeometry();

            auto positions_buffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, vertices.size());
            auto normals_buffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, normals.size());
            auto texcoords_buffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT2, texcoord.size());
            auto colors_buffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT3, colors.size());
            auto index_buffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_INT3, num_triangles);

            //fill data
            auto fill_buffer = [&](optix::Buffer &buffer, void *src_data, uint32_t size)
            {
                void *dta = buffer->map();
                memcpy(dta, src_data, size);
                buffer->unmap();
            };

            fill_buffer(positions_buffer, vertices.data(), sizeof(optix::float3) * vertices.size());
            fill_buffer(normals_buffer, normals.data(), sizeof(optix::float3) * normals.size());
            fill_buffer(texcoords_buffer, texcoord.data(), sizeof(optix::float2) * texcoord.size());
            fill_buffer(colors_buffer, colors.data(), sizeof(optix::float3) * colors.size());
            fill_buffer(index_buffer, indices.data(), sizeof(int) * indices.size());

            geometry["vertex_buffer"]->setBuffer(positions_buffer);
            geometry["normal_buffer"]->setBuffer(normals_buffer);
            geometry["texcoord_buffer"]->setBuffer(texcoords_buffer);
            geometry["color_buffer"]->setBuffer(colors_buffer);
            geometry["index_buffer"]->setBuffer(index_buffer);
            geometry->setPrimitiveCount(num_triangles);

            geometry->setBoundingBoxProgram(triangle_mesh_bounds);
            geometry->setIntersectionProgram(triangle_mesh_intersect);

            optix::GeometryInstance instance = context->createGeometryInstance();
            instance->setGeometry(geometry);

            auto material = context->createMaterial();
            material->setClosestHitProgram(0, closest_hit_primary);
            material->setClosestHitProgram(1, closest_hit_indirect);
            instance->setMaterialCount(1);
            instance->setMaterial(0, material);

            geometrygroup->setChildCount(1);
            geometrygroup->setChild(0, instance);
            geometrygroup->setAcceleration(context->createAcceleration("Trbvh"));

            context["top_object"]->set(geometrygroup);
            context["top_shadower"]->set(geometrygroup);
        }

        //Fill camera params
        CameraParams params;
        /*PrepareCameraParams(float4(-11.f, 111.f, -54.f), float4(-9.f, 111.f, -54.f), float4(0.f, 1.f, 0.f),
            float4(0.1f, 10000.f), float2((float)w, (float)h));*/
            /*Params camera_params =
            PrepareCameraParams(float4(0.f, 1.f, 3.f), float4(0.f, 1.f, 2.f), float4(0.f, 1.f, 0.f),
            float4(0.1f, 10000.f), float2((float)w, (float)h));*/
            /*    {
                    params.eye = make_float4(0.f, 1.f, 3.f, 0.f);
                    params.near_far = make_float4(0.1f, 10000.f, 0.f, 0.f);
                    params.screen_dims = make_float4((float)w, (float)h, 1.f / (float)w, 1.f / (float)h);

                    RadeonRays::matrix projection = RadeonRays::perspective_proj_fovy_lh_dx(GRAD2RAD(60), (float)w/ (float)h, 0.1f, 10000.f);
                    RadeonRays::matrix view = RadeonRays::lookat_lh_dx(
                        RadeonRays::float4(0.f, 1.f, 3.f), RadeonRays::float4(0.f, 1.f, 2.f), RadeonRays::float4(0.f, 1.f, 0.f));
                    RadeonRays::matrix vpi = RadeonRays::inverse(projection * view);

                    params.view_proj_inv.setRow(0, make_float4(vpi.m00, vpi.m01, vpi.m02, vpi.m03));
                    params.view_proj_inv.setRow(1, make_float4(vpi.m10, vpi.m11, vpi.m12, vpi.m13));
                    params.view_proj_inv.setRow(2, make_float4(vpi.m20, vpi.m21, vpi.m22, vpi.m23));
                    params.view_proj_inv.setRow(3, make_float4(vpi.m30, vpi.m31, vpi.m32, vpi.m33));
                }*/

        {
            params.eye = make_float4(-11.f, 111.f, -54.f, 0.f);
            params.near_far = make_float4(0.1f, 10000.f, 0.f, 0.f);
            params.screen_dims = make_float4((float)w, (float)h, 1.f / (float)w, 1.f / (float)h);

            RadeonRays::matrix projection = RadeonRays::perspective_proj_fovy_lh_dx(GRAD2RAD(60), (float)w / (float)h, 0.1f, 10000.f);
            RadeonRays::matrix view = RadeonRays::lookat_lh_dx(
                RadeonRays::float4(-11.f, 111.f, -54.f), RadeonRays::float4(-9.f, 111.f, -54.f), RadeonRays::float4(0.f, 1.f, 0.f));
            RadeonRays::matrix vpi = RadeonRays::inverse(projection * view);

            params.view_proj_inv.setRow(0, make_float4(vpi.m00, vpi.m01, vpi.m02, vpi.m03));
            params.view_proj_inv.setRow(1, make_float4(vpi.m10, vpi.m11, vpi.m12, vpi.m13));
            params.view_proj_inv.setRow(2, make_float4(vpi.m20, vpi.m21, vpi.m22, vpi.m23));
            params.view_proj_inv.setRow(3, make_float4(vpi.m30, vpi.m31, vpi.m32, vpi.m33));
        }

        context["camera_params"]->setUserData(sizeof(CameraParams), &params);

        //Run

        void* imageData = output_buffer->map();
        memset(imageData, 0, w * h * sizeof(float));
        output_buffer->unmap();


        std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
        start = std::chrono::high_resolution_clock::now();

        context->validate();
        uint32_t frame_count = 100;
        for (uint32_t a = 0; a < frame_count; ++a)
        {
            context["frame_no"]->setUint(a);
            context->launch(0, w, h);
        }

        end = std::chrono::high_resolution_clock::now();
        double elapsed_s = (double)(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()) / 1000.;
        std::cout << frame_count << " frames, " << w * h << " primary rays, " << w * h << " indirect rays - " << elapsed_s << " s" << std::endl;
        std::cout << "fps: " << (frame_count / elapsed_s) << ", " << ((((w * h) + (w * h)) * frame_count) / (elapsed_s)) / 1e6 << " MRays/s" << std::endl;


        //Read and save output buffer
        {
            void* imageData = output_buffer->map();

            std::vector<float> pix(w * h * 4);
            memcpy(pix.data(), imageData, w * h * 4 * sizeof(float));

            for (int a = 0; a < pix.size(); a += 4)
            {
                pix[a] /= pix[a + 3];
                pix[a+1] /= pix[a + 3];
                pix[a+2] /= pix[a + 3];
            }

            SaveImage("glossy_reflect.jpg", pix.data(), w, h);

            output_buffer->unmap();
        }


        output_buffer->destroy();
        ray_gen_program->destroy();
        context->destroy();
    }
    catch (optix::Exception &e)
    {
        std::cerr << "Optix error: "<<e.what() << std::endl;
    }
}
