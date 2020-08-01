//
//  scene.cpp
//  Non Euclidean
//
//  Created by Antoni Wójcik on 31/07/2020.
//  Copyright © 2020 Antoni Wójcik. All rights reserved.
//

#include <stdio.h>
#include "scene.h"

void SceneCreator::setupBuffers(cl::Context& context) {
    scene_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(cl_mem));
    
    vertex_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getVertexSize());
    index_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getIndexSize());
    mesh_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getMeshSize());
    model_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getModelSize());
}

void SceneCreator::createKernel(cl::Program& program, const char* name) {
    scene_kernel = cl::Kernel(program, name);
}

void SceneCreator::createScene(cl::Context& context, cl::Device& device) {
    cl::CommandQueue queue(context, device);
    queue.enqueueWriteBuffer(vertex_buffer, CL_TRUE, 0, getVertexSize(), getVertices());
    queue.enqueueWriteBuffer(index_buffer, CL_TRUE, 0, getIndexSize(), getIndices());
    queue.enqueueWriteBuffer(mesh_buffer, CL_TRUE, 0, getMeshSize(), getMeshes());
    queue.enqueueWriteBuffer(model_buffer, CL_TRUE, 0, getModelSize(), getModels());
    
    queue.enqueueNDRangeKernel(scene_kernel, cl::NullRange, cl::NDRange(size_t(1)), cl::NullRange);
    queue.finish();
}

void SceneCreator::setKernelArgs() {
    scene_kernel.setArg(0, scene_buffer);
    scene_kernel.setArg(1, vertex_buffer);
    scene_kernel.setArg(2, index_buffer);
    scene_kernel.setArg(3, mesh_buffer);
    scene_kernel.setArg(4, model_buffer);
}

void SceneCreator::loadModel(const std::string& path, cl_uint mat_ID) {
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
    
    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR: Assimp: " << importer.GetErrorString() << std::endl;
        return;
    }
    
    static cl_uint mesh_count_total = 0;
    
    cl_uint mesh_count = processNode(scene->mRootNode, scene); // TODO: NOT SURE IF THE RESULT IS CORRECT
    models.push_back((Model){mesh_count_total, mesh_count, mat_ID});
    
    mesh_count_total += mesh_count;
}

cl_uint SceneCreator::processNode(aiNode* node, const aiScene* scene) {
    cl_uint mesh_count = 0;
    
    for(unsigned int i = 0; i < node->mNumMeshes; i++) {
        mesh_count++;
        
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }
    
    for(unsigned int i = 0; i < node->mNumChildren; i++) {
        mesh_count += processNode(node->mChildren[i], scene);
    }
    
    return mesh_count;
}

Mesh SceneCreator::processMesh(aiMesh* mesh, const aiScene* scene) {
    cl_uint index_anchor = (cl_uint)indices.size();
    cl_uint face_count = 0;
    
    for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
        cl_float3 vertex;
        vertex.x = mesh->mVertices[i].x;
        vertex.y = mesh->mVertices[i].y;
        vertex.z = mesh->mVertices[i].z;
        
        vertices.push_back(vertex);
    }
    
    for(cl_uint i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        face_count++;
        
        for(cl_uint j = 0; j < face.mNumIndices; j++) indices.push_back((cl_uint)face.mIndices[j]);
    }
    
    return Mesh(index_anchor, face_count);
}
