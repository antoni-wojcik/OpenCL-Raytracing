//
//  scene.h
//  Non Euclidean
//
//  Created by Antoni Wójcik on 31/07/2020.
//  Copyright © 2020 Antoni Wójcik. All rights reserved.
//

#ifndef scene_h
#define scene_h

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

struct Model {
    cl_uint mesh_anchor;
    cl_uint mesh_count;
    cl_uint mat_ID;
    
    Model(cl_uint mesh_anchor, cl_uint mesh_count, cl_uint mat_ID) : mesh_anchor(mesh_anchor), mesh_count(mesh_count), mat_ID(mat_ID) {}
};

class SceneCreator {
private:
    cl::Kernel scene_kernel;
    cl::Buffer scene_buffer;
    cl::Buffer vertex_buffer, index_buffer, mesh_buffer, model_buffer;
    
    Assimp::Importer importer;
    
    std::vector<cl_float3> vertices;
    std::vector<cl_uint> indices;
    std::vector<Mesh> meshes;
    std::vector<Model> models;
    
    cl_uint processNode(aiNode* node, const aiScene* scene);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
    
    inline cl_float3* getVertices() { return &(vertices[0]); }
    inline cl_uint* getIndices() { return &(indices[0]); }
    inline Mesh* getMeshes() { return &(meshes[0]); }
    inline Model* getModels() { return &(models[0]); }
    
    inline size_t getVertexSize() { return sizeof(cl_float3) * vertices.size(); }
    inline size_t getIndexSize() { return sizeof(cl_uint) * indices.size(); }
    inline size_t getMeshSize() { return sizeof(Mesh) * meshes.size(); }
    inline size_t getModelSize() { return sizeof(Model) * models.size(); }
    
public:
    void createKernel(cl::Program& program, const char* name);
    void setupBuffers(cl::Context& context);
    void setKernelArgs();
    void createScene(cl::Context& context, cl::Device& device);
    
    void loadModel(const std::string& path, cl_uint mat_ID);
    
    inline const cl::Buffer& getBuffer() { return scene_buffer; }
};

#endif /* scene_h */
