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

#define KERNEL_NAME "trace"

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
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL); // for single-channel textures
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL); // for float textures
    
    glFinish();
}

void RayTracer::createGLBuffers() {
    //cl::ImageFormat image_format(CL_RGBA, CL_FLOAT);
    image = cl::ImageGL(context, CL_MEM_WRITE_ONLY, GL_TEXTURE_2D, 0, texture_ID);
}

void RayTracer::createCLBuffers() {
    buff_size = 12 * sizeof(cl_float);
    camera_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, buff_size);
}

void RayTracer::createKernels() {
    kernel = cl::Kernel(program, KERNEL_NAME);
}

void RayTracer::setKernelArgs() {
    kernel.setArg(0, image);
    kernel.setArg(1, camera_buffer);
}

void RayTracer::setTime(float time) {
    kernel.setArg(2, time);
}

void RayTracer::render(const Camera* camera) {
    try {
        std::vector<cl::Memory> mem_objs;
        mem_objs.push_back(image);
    
        cl::CommandQueue queue(context, device);
        
        queue.enqueueAcquireGLObjects(&mem_objs);
        queue.enqueueWriteBuffer(camera_buffer, CL_TRUE, 0, buff_size, camera->transferData());
        queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(size_t(width), size_t(height)), cl::NullRange);
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
