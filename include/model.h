//
//  model.h
//  Non Euclidean
//
//  Created by Antoni Wójcik on 31/07/2020.
//  Copyright © 2020 Antoni Wójcik. All rights reserved.
//

#ifndef model_h
#define model_h

#include <string>
#include <vector>
#include <iostream>

// include the Assimp library
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// include the OpenCL library (C++ binding)
#define __CL_ENABLE_EXCEPTIONS
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#include "cl2.hpp"
#include "opencl_error.h"

struct Mesh {
    cl_uint index_anchor;
    cl_uint face_count;
    
    Mesh(cl_uint index_anchor, cl_uint face_count) : index_anchor(index_anchor), face_count(face_count) {}
};

class Model {
private:
    std::vector<cl_float3> vertices;
    std::vector<cl_uint> indices;
    std::vector<Mesh> meshes;
    
    void loadModel(std::string path);
    void processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    
public:
    Model(const char* path) {
        loadModel(path);
    }
    
    inline cl_float3* getVertices() { return &(vertices[0]); }
    inline cl_uint* getIndices() { return &(indices[0]); }
    inline Mesh* getMeshes() { return &(meshes[0]); }
    
    inline size_t getVSize() { return sizeof(cl_float3) * vertices.size(); }
    inline size_t getISize() { return sizeof(cl_uint) * indices.size(); }
    inline size_t getMSize() { return sizeof(Mesh) * meshes.size(); }
};

#endif /* model_h */
