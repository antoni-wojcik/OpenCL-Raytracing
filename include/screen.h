//
//  screen.h
//  GPURaymarching
//
//  Created by Antoni Wójcik on 17/09/2019.
//

#ifndef screen_h
#define screen_h

#include "shader.h"

class Screen {
private:
    float vertices[12];
    unsigned int indices[6];
    unsigned int VBO, VAO, EBO;
    
public:
    Shader shader;
    
    Screen(const char* vs_path, const char* fs_path);
    ~Screen();
    
    void draw();
};

#endif /* screen_h */
