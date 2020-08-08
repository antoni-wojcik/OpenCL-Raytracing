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

#include "glm.hpp"

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

enum MatType { t_refractive, t_reflective, t_dielectric, t_diffuse, t_textured, t_light };

struct Material {
    MatType type;
    cl_float3 color;
    cl_float extra_data; // refractive index etc
    
    Material(MatType type, const cl_float3& color, cl_float extra_data) : type(type), color(color), extra_data(extra_data) {}
    Material() {}
};

struct Sphere {
    cl_float3 pos;
    cl_float r;
    cl_uint mat_ID;
    
    Sphere(const cl_float3& pos, cl_float r, cl_uint mat_ID) : pos(pos), r(r), mat_ID(mat_ID) {}
};

struct Plane {
    cl_float3 pos;
    cl_float3 normal;
    cl_uint mat_ID;
    
    Plane(const cl_float3& pos, const cl_float3& normal, cl_uint mat_ID) : pos(pos), normal(normal), mat_ID(mat_ID) {}
};

struct Lens {
    cl_float3 pos; // pos of the centre
    cl_float3 p1; // both curvatures
    cl_float3 p2;
    cl_float r1;
    cl_float r2;
    cl_uint mat_ID;
};

struct Mesh {
    cl_uint vertex_anchor;
    cl_uint index_anchor;
    cl_uint face_count;
    cl_uint texture_ID;
    
    Mesh(cl_uint vertex_anchor, cl_uint index_anchor, cl_uint face_count, cl_uint texture_ID) : vertex_anchor(vertex_anchor), index_anchor(index_anchor), face_count(face_count), texture_ID(texture_ID) {}
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
    
    //cl_uint texture_count = 0;
    
    cl::Buffer scene_buffer;
    cl::Buffer material_buffer;
    cl::Buffer sphere_buffer, plane_buffer, lens_buffer;
    cl::Buffer vertex_buffer, texture_uv_buffer, index_buffer, mesh_buffer, model_buffer;
    cl::Image2DArray textures;
    
    std::vector<Material> materials;
    
    std::vector<Sphere> spheres;
    std::vector<Plane> planes;
    std::vector<Lens> lenses;
    std::vector<Model> models;
    
    std::vector<cl_float3> vertices;
    std::vector<cl_float2> texture_uv;
    std::vector<cl_uint> indices;
    std::vector<Mesh> meshes;
    
    std::vector<std::string> texture_paths;
    
    Assimp::Importer importer;
    
    cl_uint processNode(aiNode* node, const aiScene* scene, cl_uint mat_ID, const glm::mat4& transform);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene, cl_uint mat_ID, const glm::mat4& transform);
    
    inline Material* getMaterials() { return &(materials[0]); }
    inline Sphere* getSpheres() { return &(spheres[0]); }
    inline Plane* getPlanes() { return &(planes[0]); }
    inline Lens* getLenses() { return &(lenses[0]); }
    inline cl_float3* getVertices() { return &(vertices[0]); }
    inline cl_float2* getTexUV() { return &(texture_uv[0]); }
    inline cl_uint* getIndices() { return &(indices[0]); }
    inline Mesh* getMeshes() { return &(meshes[0]); }
    inline Model* getModels() { return &(models[0]); }
    
    inline size_t getMaterialSize() const { return sizeof(Material) * materials.size(); }
    inline size_t getSphereSize() const { return sizeof(Sphere) * spheres.size(); }
    inline size_t getPlaneSize() const { return sizeof(Plane) * planes.size(); }
    inline size_t getLensSize() const { return sizeof(Lens) * lenses.size(); }
    inline size_t getVertexSize() const { return sizeof(cl_float3) * vertices.size(); }
    inline size_t getTexUVSize() const { return sizeof(cl_float2) * texture_uv.size(); }
    inline size_t getIndexSize() const { return sizeof(cl_uint) * indices.size(); }
    inline size_t getMeshSize() const { return sizeof(Mesh) * meshes.size(); }
    inline size_t getModelSize() const { return sizeof(Model) * models.size(); }
    
public:
    void createKernel(cl::Program& program, const char* name);
    void setupBuffers(cl::Context& context);
    void setKernelArgs();
    void createScene(cl::Context& context, cl::Device& device);
    
    void addMaterial(MatType type, const cl_float3& color, cl_float extra_data);
    
    void addSphere(const cl_float3& pos, cl_float r, cl_uint mat_ID);
    void addPlane(const cl_float3& pos, const cl_float3& normal, cl_uint mat_ID);
    void addLens(const cl_float3& pos, const cl_float3& normal, cl_float r1, cl_float r2, cl_float h, uint mat_ID);
    void loadModel(const std::string& path, cl_uint mat_ID, const glm::mat4& transform = glm::mat4(1.0f));
    
    void loadTextures(cl::Context& context, cl::Device& device);
    
    void loadScene(const std::string& path);
    
    inline cl::Buffer& getBuffer() { return scene_buffer; }
    inline cl::Image2DArray& getTextures() { return textures; }
};

#endif /* scene_h */
