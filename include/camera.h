//
//  camera.h
//  GPURaymarching
//
//  Created by Antoni Wójcik on 17/09/2019. 
//

#ifndef camera_h
#define camera_h

#include "glm.hpp"

enum CameraMovementDirection {
    FORWARD,
    BACK,
    LEFT,
    RIGHT
};

class Camera {
private:
    float fov, aspect;
    float speed;
    float half_height;
    float half_width;
    
    float yaw, pitch;
    
    glm::vec3 u, v, w;
    glm::vec3 position;
    glm::vec3 horizontal, vertical, lower_left_corner;
    
    void updateVectors();
    void setFov();
    
public:
    Camera(int camera_fov, float camera_aspect, const glm::vec3& pos = glm::vec3(0.0f, 0.0f, 0.0f), float y = 0.0f, float p = 0.0f);
    
    void move(CameraMovementDirection dir, float dt);
    void rotate(float x, float y);
    void zoom(float scroll);
    
    void setFasterSpeed(bool speed_up);
    void setSlowerSpeed(bool speed_down);
    void setSize(float new_aspect);
    
    /*inline glm::vec3 getLLC() const { return lower_left_corner; }
    inline glm::vec3 getPos() const { return position; }
    inline glm::vec3 getHor() const { return horizontal; }
    inline glm::vec3 getVer() const { return vertical; }*/
    
    float* transferData() const;
};

#endif /* camera_h */
