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

#define TRACE_KERNEL_NAME "trace"
#define SCENE_KERNEL_NAME "createScene"
#define RANDOM_BUFFER_SIZE 100000

RayTracer::RayTracer(int w, int h, const char* kernel_path) : KernelGL(kernel_path), width(w), height(h) {
    try {
        createKernels();
        createGLTextures();
        createGLBuffers();
        createCLBuffers();
        setKernelArgs();
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
    //cl::ImageFormat image_format(CL_RGBA, CL_FLOAT);
    image = cl::ImageGL(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texture_ID);
}

void RayTracer::createCLBuffers() {
    buff_size = 12 * sizeof(cl_float);
    camera_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, buff_size);
    scene_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(cl_mem));
    
    // create the buffer of random vectors in an unit sphere
    
    size_t random_buffer_size = RANDOM_BUFFER_SIZE * 3 * sizeof(cl_float);
    
    random_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, random_buffer_size);
    
    float* random_vectors = new float[RANDOM_BUFFER_SIZE * 3];
    
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    std::normal_distribution<float> ndis(0.0f, 1.0f);
    
    for(int i = 0; i < RANDOM_BUFFER_SIZE; i++) {
        float x = ndis(rng), y = ndis(rng), z = ndis(rng), r = std::cbrt(dis(rng));
        float len_inv_r = r / std::sqrt(x * x + y * y + z * z);
        x *= len_inv_r;
        y *= len_inv_r;
        z *= len_inv_r;
        random_vectors[i]     = x;
        random_vectors[i + 1] = y;
        random_vectors[i + 2] = z;
    }
    
    cl::CommandQueue queue(context, device);
    queue.enqueueWriteBuffer(random_buffer, CL_TRUE, 0, random_buffer_size, random_vectors);
    queue.finish();
    
    delete [] random_vectors;
    
    //count_buffer = cl::Buffer(context, CL_MEM_READ_WRITE, sizeof(cl_int));
}

void RayTracer::createKernels() {
    trace_kernel = cl::Kernel(program, TRACE_KERNEL_NAME);
    scene_kernel = cl::Kernel(program, SCENE_KERNEL_NAME);
    //count_kernel = cl::Kernel(program, "test_inc");
    //start_kernel = cl::Kernel(program, "test_start");
}

void RayTracer::setKernelArgs() {
    trace_kernel.setArg(0, image);
    trace_kernel.setArg(1, camera_buffer);
    trace_kernel.setArg(2, random_buffer);
    trace_kernel.setArg(3, scene_buffer);
    scene_kernel.setArg(0, scene_buffer);
    //count_kernel.setArg(0, count_buffer);
    //start_kernel.setArg(0, count_buffer);
}

void RayTracer::setTime(float time) {
    scene_kernel.setArg(1, time);
}

void RayTracer::render(const Camera* camera) {
    try {
        std::vector<cl::Memory> mem_objs;
        mem_objs.push_back(image);
    
        cl::CommandQueue queue(context, device);
        
       /* queue.enqueueNDRangeKernel(start_kernel, cl::NullRange, cl::NDRange(size_t(1)), cl::NullRange);
        queue.enqueueNDRangeKernel(count_kernel, cl::NullRange, cl::NDRange(size_t(width), size_t(height)), cl::NullRange);
        queue.enqueueBarrierWithWaitList();
        int result;
        queue.enqueueReadBuffer(count_buffer, CL_TRUE, 0, sizeof(int), &result);
        std::cout << result << std::endl;*/
        queue.enqueueNDRangeKernel(scene_kernel, cl::NullRange, cl::NDRange(size_t(1)), cl::NullRange);
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

void RayTracer::transferImage(Screen* screen, const char* shader_tex_id) {
    screen->shader.use();
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_ID);
    
    glUniform1i(glGetUniformLocation(screen->shader.ID, shader_tex_id), 0);
}
