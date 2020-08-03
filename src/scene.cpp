//
//  scene.cpp
//  Non Euclidean
//
//  Created by Antoni Wójcik on 31/07/2020.
//  Copyright © 2020 Antoni Wójcik. All rights reserved.
//

#include <stdio.h>
#include <cmath>
#include "scene.h"

// include the STB library to read texture files
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

//TODO: HANDLE "COUNT = 0" CASES

struct ObjectCounter {
    cl_uint sphere_count;
    cl_uint plane_count;
    cl_uint lens_count;
    cl_uint model_count;
};

void SceneCreator::setupBuffers(cl::Context& context) {
    scene_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(cl_mem));
    
    material_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getMaterialSize());
    
    sphere_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getSphereSize());
    plane_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getPlaneSize());
    lens_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getLensSize());
    
    vertex_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getVertexSize());
    texture_uv_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getTexUVSize());
    index_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getIndexSize());
    mesh_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getMeshSize());
    model_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, getModelSize());
}

void SceneCreator::createKernel(cl::Program& program, const char* name) {
    scene_kernel = cl::Kernel(program, name);
}

void SceneCreator::createScene(cl::Context& context, cl::Device& device) {
    cl::CommandQueue queue(context, device);
    queue.enqueueWriteBuffer(material_buffer, CL_TRUE, 0, getMaterialSize(), getMaterials());
    
    queue.enqueueWriteBuffer(sphere_buffer, CL_TRUE, 0, getSphereSize(), getSpheres());
    queue.enqueueWriteBuffer(plane_buffer, CL_TRUE, 0, getPlaneSize(), getPlanes());
    queue.enqueueWriteBuffer(lens_buffer, CL_TRUE, 0, getLensSize(), getLenses());
    queue.enqueueWriteBuffer(model_buffer, CL_TRUE, 0, getModelSize(), getModels());
    
    queue.enqueueWriteBuffer(vertex_buffer, CL_TRUE, 0, getVertexSize(), getVertices());
    queue.enqueueWriteBuffer(texture_uv_buffer, CL_TRUE, 0, getTexUVSize(), getTexUV());
    queue.enqueueWriteBuffer(index_buffer, CL_TRUE, 0, getIndexSize(), getIndices());
    queue.enqueueWriteBuffer(mesh_buffer, CL_TRUE, 0, getMeshSize(), getMeshes());
    
    queue.enqueueNDRangeKernel(scene_kernel, cl::NullRange, cl::NDRange(size_t(1)), cl::NullRange);
    queue.finish();
}

void SceneCreator::setKernelArgs() {
    scene_kernel.setArg(0, scene_buffer);
    scene_kernel.setArg(1, material_buffer);
    scene_kernel.setArg(2, sphere_buffer);
    scene_kernel.setArg(3, plane_buffer);
    scene_kernel.setArg(4, lens_buffer);
    scene_kernel.setArg(5, vertex_buffer);
    scene_kernel.setArg(6, texture_uv_buffer);
    scene_kernel.setArg(7, index_buffer);
    scene_kernel.setArg(8, mesh_buffer);
    scene_kernel.setArg(9, model_buffer);
    
    ObjectCounter obj_counter;
    obj_counter.sphere_count = (cl_uint)spheres.size();
    obj_counter.plane_count = (cl_uint)planes.size();
    obj_counter.lens_count = (cl_uint)lenses.size();
    obj_counter.model_count = (cl_uint)models.size();
    
    scene_kernel.setArg(10, obj_counter);
}

void SceneCreator::addMaterial(MatType type, const cl_float3& color, cl_float extra_data) {
    materials.push_back(Material(type, color, extra_data));
}

void SceneCreator::addSphere(const cl_float3& pos, cl_float r, cl_uint mat_ID) {
    spheres.push_back(Sphere(pos, r, mat_ID));
}

void SceneCreator::addPlane(const cl_float3& pos, const cl_float3& normal, cl_uint mat_ID) {
    planes.push_back(Plane(pos, normal, mat_ID));
}

void SceneCreator::addLens(const cl_float3& pos, const cl_float3& normal, cl_float r1, cl_float r2, cl_float h, uint mat_ID) {
    assert(r1 >= h && r2 >= h);
    
    Lens lens;
    lens.pos = pos;
    
    cl_float temp = (cl_float)std::sqrt(r1 * r1 - h * h);
    lens.p1.x = pos.x + normal.x * temp;
    lens.p1.y = pos.y + normal.y * temp;
    lens.p1.z = pos.z + normal.z * temp;
    
    temp = (cl_float)std::sqrt(r2 * r2 - h * h);
    lens.p2.x = pos.x - normal.x * temp;
    lens.p2.y = pos.y - normal.y * temp;
    lens.p2.z = pos.z - normal.z * temp;
    
    lens.r1 = r1;
    lens.r2 = r2;
    lens.mat_ID = mat_ID;
    
    lenses.push_back(lens);
}

void SceneCreator::addTexture(const std::string& path) {
    texture_paths.push_back(path);
}

void SceneCreator::loadTextures(cl::Context& context, cl::Device& device) {
    if(texture_paths.size() == 0) {
        std::cerr << "ERROR: TEXTURE COUNT = 0" << std::endl;
        exit(-1);
    }
    
    int tex_width = 0, tex_height = 0;
    
    cl::CommandQueue queue(context, device);
    
    for(unsigned int texture_ID = 0; texture_ID < texture_paths.size(); texture_ID++) {
        int width, height;
        int channel_count;
        
        float* texture_data = stbi_loadf(texture_paths[texture_ID].c_str(), &width, &height, &channel_count, 0);
        
        if(texture_ID == 0) {
            tex_width = width;
            tex_height = height;
    
            textures = cl::Image2DArray(context, CL_MEM_READ_ONLY, cl::ImageFormat(CL_RGBA, CL_FLOAT), texture_paths.size(), width, height, 0, 0);
        } else if(texture_ID >= texture_paths.size()) {
            std::cerr << "ERROR: TOO MANY TEXTURES" << std::endl;
            exit(-1);
        } else if(tex_width != width || tex_height != height) {
            std::cerr << "ERROR: TEXTURES HAVE DIFFERENT SIZES: TEMPLATE: " << tex_width << " x " << tex_height << ", TEXTURE ID(" << texture_ID << "): " << width << " x " << height << std::endl;
            exit(-1);
        }
        
        if(texture_data) {
            if(channel_count == 4) {// RGBA only
                //queue.enqueueWriteImage(texture, CL_TRUE, {0, 0, 0}, {size_t(width), size_t(height), 1}, 0, 0, texture_data);
                queue.enqueueWriteImage(textures, CL_TRUE, {0, 0, texture_ID}, {size_t(width), size_t(height), 1}, 0, 0, texture_data);
                
                stbi_image_free(texture_data);
            } else {
                std::cerr << "ERROR: STBimage: TEXTURE HAS A WRONG FORMAT: " << channel_count << " INSTEAD OF 4" << std::endl;
                exit(-1);
            }
        } else {
            std::cerr << "ERROR: STBimage: COULD NOT FIND THE TEXTURE" << std::endl;
            exit(-1);
        }
    }
    
    queue.finish();
}

void SceneCreator::loadModel(const std::string& path, cl_uint mat_ID, cl_uint texture_ID, const glm::mat4& transform) {
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
    
    if(texture_ID >= texture_paths.size()) {
        std::cerr << "ERROR: TOO HIGH TEXTURE_ID: " << texture_ID << std::endl;
        exit(-1);
    }
    
    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR: Assimp: " << importer.GetErrorString() << std::endl;
        return;
    }
    
    static cl_uint mesh_count_total = 0;
    
    cl_uint mesh_count = processNode(scene->mRootNode, scene, transform); // TODO: NOT SURE IF THE RESULT IS CORRECT
    models.push_back((Model){mesh_count_total, mesh_count, mat_ID, texture_ID});
    
    mesh_count_total += mesh_count;
}

cl_uint SceneCreator::processNode(aiNode* node, const aiScene* scene, const glm::mat4& transform) {
    cl_uint mesh_count = 0;
    
    for(unsigned int i = 0; i < node->mNumMeshes; i++) {
        mesh_count++;
        
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene, transform));
    }
    
    for(unsigned int i = 0; i < node->mNumChildren; i++) {
        mesh_count += processNode(node->mChildren[i], scene, transform);
    }
    
    return mesh_count;
}

inline cl_float3 transformVertex(const aiVector3D& vertex, const glm::mat4& transform) {
    cl_float3 temp;
    temp.x = transform[0][0] * vertex.x + transform[1][0] * vertex.y + transform[2][0] * vertex.z + transform[3][0];
    temp.y = transform[0][1] * vertex.x + transform[1][1] * vertex.y + transform[2][1] * vertex.z + transform[3][1];
    temp.z = transform[0][2] * vertex.x + transform[1][2] * vertex.y + transform[2][2] * vertex.z + transform[3][2];
    return temp;
}

Mesh SceneCreator::processMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& transform) {
    cl_uint index_anchor = (cl_uint)indices.size();
    cl_uint vertex_anchor = (cl_uint)vertices.size();
    //cl_uint texture_uv_anchor = (cl_uint)texture_uv.size();
    cl_uint face_count = 0;
    
    for(unsigned int i = 0; i < mesh->mNumVertices; i++) {
        cl_float3 vertex = transformVertex(mesh->mVertices[i], transform);
        
        vertices.push_back(vertex);
        
        if(mesh->mTextureCoords[0]) { // does the mesh contain texture coordinates?
            cl_float2 uv;
            uv.x = mesh->mTextureCoords[0][i].x;
            uv.y = mesh->mTextureCoords[0][i].y;
            
            texture_uv.push_back(uv);
        }
    }
    
    for(cl_uint i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        face_count++;
        
        for(cl_uint j = 0; j < face.mNumIndices; j++) indices.push_back((cl_uint)face.mIndices[j]);
    }
    
    return Mesh(vertex_anchor, index_anchor, face_count);
}
