//
//  raytracer.cpp
//  Non Euclidean
//
//  Created by Antoni Wójcik on 22/07/2020.
//  Copyright © 2020 Antoni Wójcik. All rights reserved.
//
#include <GL/glew.h>
#include "raytracer.h"

#include <vector>
#include <iostream>
#include <random>
#include <cmath>

#include "gtc/matrix_transform.hpp"

#define TRACE_KERNEL_NAME "trace"
#define RETRACE_KERNEL_NAME "retrace"
#define SCENE_KERNEL_NAME "createScene"
#define RANDOM_BUFFER_SIZE 100000
#define NUM_TRIANGLES 4

RayTracer::RayTracer(int w, int h, const char* kernel_path) : KernelGL(kernel_path), width(w), height(h) {
    try {
        createKernels();
        createGLTextures();
        createGLBuffers();
        createCLBuffers();
        setKernelArgs();
        
        scene.createScene(context, device);
    } catch(cl::Error e) {
        processError(e);
    }
}

RayTracer::~RayTracer() {
    glDeleteTextures(1, &texture_ID);
}

void RayTracer::createGLTextures() {
    glGenTextures(1, &texture_ID);
    
    glBindTexture(GL_TEXTURE_2D, texture_ID);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    
    glFinish();
}

void RayTracer::createGLBuffers() {
    image = cl::ImageGL(context, CL_MEM_READ_WRITE, GL_TEXTURE_2D, 0, texture_ID);
}

void RayTracer::createCLBuffers() {
    buff_size = 12 * sizeof(cl_float);
    camera_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, buff_size);
    
    // create the buffer of random vectors in an unit sphere
    
    size_t random_buffer_size = RANDOM_BUFFER_SIZE * 4 * sizeof(cl_float);
    
    float* random_data = new float[RANDOM_BUFFER_SIZE * 4];
    
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    std::normal_distribution<float> ndis(0.0f, 1.0f);
    
    for(int i = 0; i < RANDOM_BUFFER_SIZE; i++) {
        float x = ndis(rng), y = ndis(rng), z = ndis(rng), r = std::cbrt(dis(rng)), u = dis(rng);
        float len_inv_r = r / std::sqrt(x * x + y * y + z * z);
        x *= len_inv_r;
        y *= len_inv_r;
        z *= len_inv_r;
        random_data[3 * i]     = x;
        random_data[3 * i + 1] = y;
        random_data[3 * i + 2] = z;
        
        random_data[3 * RANDOM_BUFFER_SIZE + i] = u;
    }
    
    random_buffer = cl::Buffer(context, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, random_buffer_size, random_data);
    
    delete [] random_data;
    
    scene.loadScene("assets/scenes/scene.scene");
    
    scene.loadTextures(context, device);
    
    scene.setupBuffers(context);
}

void RayTracer::createKernels() {
    trace_kernel = cl::Kernel(program, TRACE_KERNEL_NAME);
    retrace_kernel = cl::Kernel(program, RETRACE_KERNEL_NAME);
    scene.createKernel(program, SCENE_KERNEL_NAME);
}

void RayTracer::setKernelArgs() {
    trace_kernel.setArg(0, image);
    trace_kernel.setArg(1, camera_buffer);
    trace_kernel.setArg(2, random_buffer);
    trace_kernel.setArg(3, scene.getBuffer());
    trace_kernel.setArg(4, scene.getTextures());
    retrace_kernel.setArg(0, image);
    retrace_kernel.setArg(1, image);
    retrace_kernel.setArg(2, camera_buffer);
    retrace_kernel.setArg(3, random_buffer);
    retrace_kernel.setArg(4, scene.getBuffer());
    retrace_kernel.setArg(5, scene.getTextures());
    scene.setKernelArgs();
}

/*void RayTracer::setTime(float time) {
    scene_kernel.setArg(5, time);
}*/

void RayTracer::render(const Camera* camera) {
    sample_counter = 0;
    
    try {
        std::vector<cl::Memory> mem_objs;
        mem_objs.push_back(image);
        
        cl::CommandQueue queue(context, device);
        queue.enqueueAcquireGLObjects(&mem_objs);
        queue.enqueueWriteBuffer(camera_buffer, CL_TRUE, 0, buff_size, camera->transferData());
        queue.enqueueNDRangeKernel(trace_kernel, cl::NullRange, cl::NDRange(size_t(width), size_t(height)), cl::NullRange);
        queue.enqueueReleaseGLObjects(&mem_objs);
        queue.enqueueBarrierWithWaitList();
        queue.finish();
    } catch(cl::Error e) {
        processError(e);
    }
}

void RayTracer::renderAgain(const Camera* camera) {
    sample_counter++;
    
    try {
        retrace_kernel.setArg(6, sample_counter);
        
        std::vector<cl::Memory> mem_objs;
        mem_objs.push_back(image);
        
        cl::CommandQueue queue(context, device);
        queue.enqueueAcquireGLObjects(&mem_objs);
        queue.enqueueWriteBuffer(camera_buffer, CL_TRUE, 0, buff_size, camera->transferData());
        queue.enqueueNDRangeKernel(retrace_kernel, cl::NullRange, cl::NDRange(size_t(width), size_t(height)), cl::NullRange);
        queue.enqueueReleaseGLObjects(&mem_objs);
        queue.enqueueBarrierWithWaitList();
        queue.finish();
    } catch(cl::Error e) {
        processError(e);
    }
}

void RayTracer::transferImage(Screen* screen, const char* shader_tex_id) {
    screen->shader.use();
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_ID);
    
    glUniform1i(glGetUniformLocation(screen->shader.ID, shader_tex_id), 0);
}
