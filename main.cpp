//
//  main.cpp
//  Non Euclidean
//
//  Created by Antoni Wójcik on 21/07/2020.
//  Copyright © 2020 Antoni Wójcik. All rights reserved.
//

#define RETINA

#define SCR_WIDTH 1200
#define SCR_HEIGHT 800

#define MIN_FRAME_TIME 0.003f
#define MAX_FRAME_COUNT 6
#define FPS_STEPS 5


#include <iostream>
#include <fstream>

// include the OpenGL libraries
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "shader.h"
#include "screen.h"
#include "camera.h"
#include "raytracer.h"


// function declarations
GLFWwindow* initialiseOpenGL();
void framebufferSizeCallback(GLFWwindow*, int, int);
void mouseCallback(GLFWwindow*, double, double);
void mouseButtonCallback(GLFWwindow*, int, int, int);
void scrollCallback(GLFWwindow*, double, double);
void processInput(GLFWwindow*, float);
void takeScreenshot(const std::string& name = "screenshot", bool show_image = false);
void countFPS(float);

#ifdef RETINA
// dimensions of the viewport (they have to be multiplied by 2 at the retina displays)
unsigned int scr_width = SCR_WIDTH*2;
unsigned int scr_height = SCR_HEIGHT*2;
#else
unsigned int scr_width = SCR_WIDTH;
unsigned int scr_height = SCR_HEIGHT;
#endif

// variables used in the main loop
bool run = true;
bool camera_in_motion = true;

// variables used in callbacks
bool mouse_hidden = true;

// camera pointer
Camera* camera;
Screen* screen;

int main(int argc, const char * argv[]) {
    GLFWwindow* window = initialiseOpenGL();
    
    camera = new Camera(60.0f, (float)scr_width / (float)scr_height, glm::vec3(0.0f), 0, 0);
    screen = new Screen("shaders/screen.vs", "shaders/screen.fs");
    RayTracer* ray_tracer = new RayTracer(SCR_WIDTH, SCR_HEIGHT, "kernels/raytracer.cl"); // FIXME: change to scr_width, scr_height to get the full resolution
    
    float last_frame_time = 0.0f;
    float delta_time = 0.0f;
    float lag = 0.0f;
    
    while(!glfwWindowShouldClose(window)) {
        float current_time = glfwGetTime();
        delta_time = current_time - last_frame_time;
        last_frame_time = current_time;
        lag += delta_time;
        
        glClearColor(0.7f, 0.8f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        processInput(window, delta_time);
        //ray_tracer->setTime(current_time);
        
        if(run) {
            if(lag >= MIN_FRAME_TIME) {
                int steps = (int)(lag / MIN_FRAME_TIME);
                lag -= steps * MIN_FRAME_TIME;
                if(steps > MAX_FRAME_COUNT) steps = MAX_FRAME_COUNT;
                
                if(camera_in_motion) {
                    ray_tracer->render(camera);
                    
                    camera_in_motion = false;
                } else ray_tracer->renderAgain(camera);
                
                //scene->iterate(steps); TODO
            }
        } else {
            lag = 0.0f;
        }
        
        ray_tracer->transferImage(screen, "image");
        screen->draw();
        
        glfwSwapBuffers(window);
        glfwPollEvents();
        glFinish();
    }
    
    delete ray_tracer;
    delete camera;
    
    glfwTerminate();
    return 0;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    scr_width = width;
    scr_height = height;
    
    camera->setSize((float)width / (float)height);
}

void processInput(GLFWwindow* window, float delta_time) {
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
    if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera->move(FORWARD, delta_time);
        camera_in_motion = true;
    }
    if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera->move(BACK, delta_time);
        camera_in_motion = true;
    }
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera->move(LEFT, delta_time);
        camera_in_motion = true;
    }
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera->move(RIGHT, delta_time);
        camera_in_motion = true;
    }
    
    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera->setFasterSpeed(true);
    else if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_RELEASE) camera->setFasterSpeed(false);
    if(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) camera->setSlowerSpeed(true);
    else if(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_RELEASE && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_RELEASE) camera->setSlowerSpeed(false);
    
    static bool taking_screenshot = false;
    
    if(glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
        if(!taking_screenshot) takeScreenshot();
        taking_screenshot = true;
    } else if(glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_RELEASE)
        taking_screenshot = false;
    
    static bool stopping = false;
    
    if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        if(!stopping) run = !run;
        stopping = true;
    } else if(glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE)
        stopping = false;
}

void mouseCallback(GLFWwindow* window, double pos_x, double pos_y) {
    static float mouse_last_x;
    static float mouse_last_y;
    
    static bool mouse_first_check = true;
    if(mouse_first_check) {
        mouse_last_x = pos_x;
        mouse_last_y = pos_y;
        mouse_first_check = false;
    }
    
    float offset_x = pos_x - mouse_last_x;
    float offset_y = mouse_last_y - pos_y;
    
    mouse_last_x = pos_x;
    mouse_last_y = pos_y;
    
    if(mouse_hidden) {
        camera->rotate(offset_x, -offset_y);
        camera_in_motion = true;
    }
}

void scrollCallback(GLFWwindow* window, double offset_x, double offset_y) {
    camera->zoom(offset_y);
    camera_in_motion = true;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if(mouse_hidden) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        else glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        
        mouse_hidden = !mouse_hidden;
    }
}

void countFPS(float delta_time) {
    static float fps_sum = 0.0f;
    static int fps_steps_counter = 0;
    
    // count fps
    if(fps_steps_counter == FPS_STEPS) {
        fps_steps_counter = 0;
        fps_sum = 0.0f;
    }
    fps_sum += delta_time;
    fps_steps_counter++;
}

GLFWwindow* initialiseOpenGL() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    
    GLFWwindow* window;
    
    #ifdef RETINA
    window = glfwCreateWindow(scr_width/2, scr_height/2, "Non-Euclidean", NULL, NULL);
    #else
    window = glfwCreateWindow(scr_width, scr_height, "Non-Euclidean", NULL, NULL);
    #endif
    if(window == NULL) {
        std::cout << "ERROR: OpenGL: Failed to create GLFW window" << std::endl;
        glfwTerminate();
        exit(-1);
    }
    
    glfwMakeContextCurrent(window);
    
    // set the callbacks
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    
    // tell GLFW to capture the mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    if(glewInit() != GLEW_OK) {
        std::cerr << "ERROR: OpenGL: Failed to initialize GLEW" << std::endl;
        glGetError();
        exit(-1);
    }
    
    return window;
}

void takeScreenshot(const std::string& name, bool show_image) {
    static int photo_count = 0;
    std::string name_count = name + std::to_string(photo_count);
    std::cout << "Taking screenshot: " << name_count << ".tga" << ", dimensions: " << scr_width << ", " << scr_height << std::endl;
    short TGA_header[] = {0, 2, 0, 0, 0, 0, (short)scr_width, (short)scr_height, 24};
    char* pixel_data = new char[3 * scr_width * scr_height]; //there are 3 colors (RGB) for each pixel
    std::ofstream file("screenshots/" + name_count + ".tga", std::ios::out | std::ios::binary);
    if(!pixel_data || !file) {
        std::cerr << "ERROR: COULD NOT TAKE THE SCREENSHOT" << std::endl;
        exit(-1);
    }
    
    glFinish();
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, scr_width, scr_height, GL_BGR, GL_UNSIGNED_BYTE, pixel_data);
    glFinish();
    
    file.write((char*)TGA_header, 9 * sizeof(short));
    file.write(pixel_data, 3 * scr_width * scr_height);
    file.close();
    delete [] pixel_data;
    
    if(show_image) {
        std::cout << "Opening the screenshot" << std::endl;
        std::system(("open " + name_count + ".tga").c_str());
    }
    photo_count++;
}
