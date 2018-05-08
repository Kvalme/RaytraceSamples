/**********************************************************************
Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#include "scene.h"

#include <tiny_obj_loader.h>

#define _USE_MATH_DEFINES
#include <assert.h>
#include <cmath>
#include <algorithm>
#include <iostream>

class ObjKey
{
public:
    inline ObjKey()
    {
    }

    inline bool operator <(const ObjKey &other) const
    {
        if (position_index_ != other.position_index_)
            return (position_index_ < other.position_index_);
        if (normal_index_ != other.normal_index_)
            return (normal_index_ < other.normal_index_);
        if (texcoords_index_ != other.texcoords_index_)
            return (texcoords_index_ < other.texcoords_index_);
        return false;
    }

    uint32_t    position_index_;
    uint32_t    normal_index_;
    uint32_t    texcoords_index_;
};

typedef float ObjVertex[12];

// Constructor
Scene::Scene()
{
}

// Loads a file into the scene
bool Scene::loadFile(const char *filename)
{
    assert(filename);
    bool result = false;

    // Handle file type
    const std::string fileExtension = getFileExtension(filename);
    if (fileExtension == ".obj")
        result = parseObj(filename);

    return result;
}

// Parses the input .obj file
bool Scene::parseObj(const char *filename)
{
    using namespace tinyobj;

    // Compute relative path
    const char *file = std::max(strrchr(filename, '/'), strrchr(filename, '\\'));
    const std::string relativePath(filename, file ? file + 1 : filename);

    // Parse the .obj file
    attrib_t attrib;
    std::string error;
    std::vector<shape_t> shapes;
    std::vector<material_t> materials;
    auto result = LoadObj(&attrib, &shapes, &materials, &error, filename, relativePath.empty() ? nullptr : relativePath.data());
    if (!error.empty())
        std::cout << error << std::endl;
    if (!result)
        return false;

    // Create the scene data
    for (auto &shape : shapes)
    {
        // Gather the vertices
        ObjKey objKey;
        std::vector<float> vertices;
        std::vector<uint32_t> indices;
        std::map<ObjKey, uint32_t> objMap;

        uint32_t i = 0, material_id = 0;
        for (auto face = shape.mesh.num_face_vertices.begin(); face != shape.mesh.num_face_vertices.end(); i += *face, ++material_id, ++face)
        {
            // We only support triangle primitives
            if (*face != 3)
                continue;

            // Load the material information
            auto material_idx = (!shape.mesh.material_ids.empty() ? shape.mesh.material_ids[material_id] : 0xffffffffu);
            auto material = (material_idx != 0xffffffffu ? &materials[material_idx] : nullptr);

            for (auto v = 0u; v < 3u; ++v)
            {
                // Construct the lookup key
                objKey.position_index_  = shape.mesh.indices[i + v].vertex_index;
                objKey.normal_index_    = shape.mesh.indices[i + v].normal_index;
                objKey.texcoords_index_ = shape.mesh.indices[i + v].texcoord_index;

                // Look up the vertex map
                uint32_t index;
                auto it = objMap.find(objKey);
                if (it != objMap.end())
                    index = (*it).second;
                else
                {
                    // Push the vertex into memory
                    ObjVertex vertex = {};
                    for (auto p = 0u; p < 3u; ++p)
                        vertex[p] = attrib.vertices[3 * objKey.position_index_ + p];
                    if (objKey.normal_index_ != 0xffffffffu)
                        for (auto n = 0u; n < 3u; ++n)
                            vertex[n + 3] = attrib.normals[3 * objKey.normal_index_ + n];
                    if (objKey.texcoords_index_ != 0xffffffffu)
                        for (auto t = 0u; t < 2u; ++t)
                            vertex[t + 6] = attrib.texcoords[2 * objKey.texcoords_index_ + t];
                    if (material != nullptr)
                        for (auto c = 0u; c < 3u; ++c)
                            vertex[c + 9] = material->diffuse[c];
                    for (auto value : vertex)
                        vertices.push_back(value);
                    index = static_cast<uint32_t>(vertices.size() / (sizeof(vertex) / sizeof(*vertex))) - 1;
                    objMap[objKey] = index;
                }

                // Push the index into memory
                indices.push_back(index);
            }
        }

        // Create the mesh object
        meshes_.push_back(Mesh());
        Mesh &mesh = meshes_.back();
        mesh.name_ = shape.name;
        std::swap(mesh.vertices_, vertices);
        mesh.vertex_stride_ = sizeof(ObjVertex);
        std::swap(mesh.indices_, indices);
        mesh.index_stride_ = sizeof(uint32_t);
    }

    return true;
}

Mesh::Mesh()
    : vertex_stride_(0)
    , index_stride_(0)
{
}
