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

#define SIZE_EMPTY 1

//TODO: HANDLE "COUNT = 0" CASES

struct ObjectCounter {
    cl_uint sphere_count;
    cl_uint plane_count;
    cl_uint lens_count;
    cl_uint model_count;
};

inline void setupBuffer(cl::Context& context, cl::Buffer& buffer, size_t size) {
    if(size) buffer = cl::Buffer(context, CL_MEM_READ_ONLY, size);
    else buffer = cl::Buffer(context, CL_MEM_READ_ONLY, SIZE_EMPTY);
}

void SceneCreator::setupBuffers(cl::Context& context) {
    scene_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(cl_mem));
    
    setupBuffer(context, material_buffer, getMaterialSize());
    
    setupBuffer(context, sphere_buffer, getSphereSize());
    setupBuffer(context, plane_buffer, getPlaneSize());
    setupBuffer(context, lens_buffer, getLensSize());
    setupBuffer(context, model_buffer, getModelSize());
    
    setupBuffer(context, vertex_buffer, getVertexSize());
    setupBuffer(context, texture_uv_buffer, getTexUVSize());
    setupBuffer(context, index_buffer, getIndexSize());
    setupBuffer(context, mesh_buffer, getMeshSize());
}

void SceneCreator::createKernel(cl::Program& program, const char* name) {
    scene_kernel = cl::Kernel(program, name);
}

inline void writeBuffer(cl::CommandQueue& queue, cl::Buffer& buffer, size_t size, void* data_ptr) {
    if(size) queue.enqueueWriteBuffer(buffer, CL_TRUE, 0, size, data_ptr);
    //else queue.enqueueWriteBuffer(buffer, CL_TRUE, 0, size, data_ptr);
}

void SceneCreator::createScene(cl::Context& context, cl::Device& device) {
    cl::CommandQueue queue(context, device);
    writeBuffer(queue, material_buffer, getMaterialSize(), getMaterials());
    
    writeBuffer(queue, sphere_buffer, getSphereSize(), getSpheres());
    writeBuffer(queue, plane_buffer, getPlaneSize(), getPlanes());
    writeBuffer(queue, lens_buffer, getLensSize(), getLenses());
    writeBuffer(queue, model_buffer, getModelSize(), getModels());
    
    writeBuffer(queue, vertex_buffer, getVertexSize(), getVertices());
    writeBuffer(queue, texture_uv_buffer, getTexUVSize(), getTexUV());
    writeBuffer(queue, index_buffer, getIndexSize(), getIndices());
    writeBuffer(queue, mesh_buffer, getMeshSize(), getMeshes());
    
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

void SceneCreator::loadTextures(cl::Context& context, cl::Device& device) {
    if(models.size() > 0) {
        if(texture_paths.size() == 0)
            processError("ERROR: TEXTURE COUNT = 0");
        
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
            } else if(tex_width != width || tex_height != height) {
                queue.finish();
                processError("ERROR: TEXTURES HAVE DIFFERENT SIZES: TEMPLATE: " + std::to_string(tex_width) + " x " + std::to_string(tex_height) + ", TEXTURE ID(" + std::to_string(texture_ID) + "): " + std::to_string(width) + " x " + std::to_string(height));
            }
            
            if(texture_data) {
                if(channel_count == 4) {// RGBA only
                    //queue.enqueueWriteImage(texture, CL_TRUE, {0, 0, 0}, {size_t(width), size_t(height), 1}, 0, 0, texture_data);
                    queue.enqueueWriteImage(textures, CL_TRUE, {0, 0, texture_ID}, {size_t(width), size_t(height), 1}, 0, 0, texture_data);
                    
                    stbi_image_free(texture_data);
                } else {
                    queue.finish();
                    processError("ERROR: STBimage: TEXTURE HAS A WRONG FORMAT: " + std::to_string(channel_count) + " INSTEAD OF 4 (RGBA)");
                }
            } else {
                queue.finish();
                processError("ERROR: STBimage: COULD NOT FIND THE TEXTURE");
            }
        }
        
        queue.finish();
    } else {
        textures = cl::Image2DArray(context, CL_MEM_READ_ONLY, cl::ImageFormat(CL_RGBA, CL_FLOAT), SIZE_EMPTY, SIZE_EMPTY, SIZE_EMPTY, 0, 0);
    }
}

void SceneCreator::loadModel(const std::string& path, cl_uint mat_ID, const glm::mat4& transform) {
    static cl_uint mesh_count_total = 0;
    
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);
    
    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        processError("ERROR: Assimp: " + std::string(importer.GetErrorString()));
    
    if(materials.size() <= mat_ID)
        processError("ERROR: MATERIAL OF ID: " + std::to_string(mat_ID) + " DOES NOT EXIST");
    
    cl_uint mesh_count = processNode(scene->mRootNode, scene, mat_ID, transform); // TODO: NOT SURE IF THE RESULT IS CORRECT
    models.push_back((Model){mesh_count_total, mesh_count, mat_ID});
    
    mesh_count_total += mesh_count;
}

cl_uint SceneCreator::processNode(aiNode* node, const aiScene* scene, cl_uint mat_ID, const glm::mat4& transform) {
    cl_uint mesh_count = 0;
    
    for(unsigned int i = 0; i < node->mNumMeshes; i++) {
        mesh_count++;
        
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene, mat_ID, transform));
    }
    
    for(unsigned int i = 0; i < node->mNumChildren; i++) {
        mesh_count += processNode(node->mChildren[i], scene, mat_ID, transform);
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

Mesh SceneCreator::processMesh(aiMesh* mesh, const aiScene* scene, cl_uint mat_ID, const glm::mat4& transform) {
    cl_uint index_anchor = (cl_uint)indices.size();
    cl_uint vertex_anchor = (cl_uint)vertices.size();
    cl_uint face_count = 0;
    
    // load vertices, texture coords, indices
    
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
    
    // load texture
    
    cl_uint texture_ID = -1;
    
    if(materials[mat_ID].type == t_textured) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        
        unsigned int texture_count = material->GetTextureCount(aiTextureType_DIFFUSE);
        
        if(texture_count == 1) { // also check if material is of type t_textured
            aiString str;
            material->GetTexture(aiTextureType_DIFFUSE, 0, &str);
            
            bool skip = false;
            for(unsigned int j = 0; j < texture_paths.size(); j++) {
                if(std::strcmp(texture_paths[j].c_str(), str.C_Str()) == 0) {
                    texture_ID = j;
                    skip = true;
                    break;
                }
            }
            if(!skip) {
                texture_ID = (cl_uint)texture_paths.size();
                texture_paths.push_back(std::string(str.C_Str()));
            }
        } else if(texture_count == 0) {
            processError("ERROR: MESH HAS NO TEXTURE APPLIED, USE A DIFFERENT MATERIAL");
        } else if(texture_count > 1) {
            processError("ERROR: MESH HAS MORE THAN ONE TEXTURE");
        }
    }
    
    return Mesh(vertex_anchor, index_anchor, face_count, texture_ID);
}

void SceneCreator::processError(const std::string& err) {
    std::cerr << err << std::endl;
    exit(-1);
}
