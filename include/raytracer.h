//
//  raytracer.h
//  Non Euclidean
//
//  Created by Antoni Wójcik on 22/07/2020.
//  Copyright © 2020 Antoni Wójcik. All rights reserved.
//

#ifndef raytracer_h
#define raytracer_h

#include "kernelgl.h"
#include "glm.hpp"
#include "screen.h"

class RayTracer : KernelGL {
private:
    int width, height;
    
    int sample_counter;
    
    cl_GLuint texture_ID;
    
    cl::Kernel trace_kernel, retrace_kernel, scene_kernel;
    cl::ImageGL image;
    cl::Buffer scene_buffer, random_buffer, camera_buffer;
    size_t image_size, buff_size;
    
    void createGLTextures();
    void createGLBuffers();
    void createCLBuffers();
    void createKernels();
    void setKernelArgs();
    
public:
    RayTracer(int w, int h, const char* kernel_path);
    ~RayTracer();

    void render(const Camera* camera);
    void renderAgain(const Camera* camera);
    void transferImage(Screen* screen, const char* shader_tex_id);
    void setTime(float time);
    void resize(int w, int h);
    
};

#endif /* raytracer_h */
