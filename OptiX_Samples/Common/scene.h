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

#pragma once

#include <stdint.h>
#include <vector>

// Forward declarations
class Mesh;

class Scene
{
    // Non-copyable
    Scene(const Scene &) = delete;
    Scene &operator =(const Scene &) = delete;

public:
    Scene();

    bool loadFile(const char *filename);

    // Scene data
    std::vector<Mesh>   meshes_;

protected:
    static inline const char *getFileExtension(const char *filename)
    {
        const char *fileExtension = strrchr(filename, '.');
        return (fileExtension ? fileExtension : filename);
    }

    bool parseObj(const char *filename);
};

class Mesh
{
public:
    Mesh();

    std::string             name_;
    std::vector<float>      vertices_;
    uint32_t                vertex_stride_;
    std::vector<uint32_t>   indices_;
    uint32_t                index_stride_;
    int32_t                 light_id_ = -1;
};
