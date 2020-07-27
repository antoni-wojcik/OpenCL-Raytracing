//
//  camera.cpp
//  Non Euclidean
//
//  Created by Antoni Wójcik on 22/07/2020.
//  Copyright © 2020 Antoni Wójcik. All rights reserved.
//

#include "camera.h"

#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include <iostream>

#define CAMERA_SPEED_SLOW 0.3f
#define CAMERA_SPEED_NORMAL 1.0f
#define CAMERA_SPEED_FAST 5.0f
#define MOUSE_SENSITIVITY 0.2f
#define ZOOM_MIN 90.0f
#define ZOOM_MAX 10.0f
#define ZOOM_SPEED 0.5f
#define UP_DIR glm::vec3(0.0f, -1.0f, 0.0f)

    
void Camera::updateVectors() {
    float rad_pitch = glm::radians(pitch), rad_yaw = glm::radians(yaw);
    w.x = glm::cos(rad_pitch) * glm::sin(rad_yaw);
    w.y = glm::sin(rad_pitch);
    w.z = glm::cos(rad_pitch) * glm::cos(rad_yaw);
    w = normalize(w);
    u = normalize(cross(w, UP_DIR));
    v = cross(u, w);
    lower_left_corner = w-(half_width*u + half_height*v);
    horizontal = 2.0f*half_width*u;
    vertical = 2.0f*half_height*v;
}

void Camera::setFov() {
    float angle = fov * M_PI * 0.0055556f; // 1/180
    half_height = tan(angle * 0.5f);
    half_width = aspect * half_height;
    updateVectors();
}

Camera::Camera(int camera_fov, float camera_aspect, const glm::vec3& pos, float y, float p) : fov(camera_fov), aspect(camera_aspect), position(pos), yaw(y), pitch(p), speed(CAMERA_SPEED_SLOW) {
    float angle = fov*M_PI/180.0f;
    half_height = tan(angle*0.5f);
    half_width = aspect * half_height;
    updateVectors();
}

void Camera::move(CameraMovementDirection dir, float dt) {
    float ds = speed * dt;
    if(dir == FORWARD) position += w * ds;
    else if(dir == BACK) position -= w * ds;
    else if(dir == LEFT) position -= u * ds;
    else if(dir == RIGHT) position += u * ds;
}

void Camera::rotate(float x, float y) {
    yaw += x * MOUSE_SENSITIVITY * fov/ZOOM_MAX;
    pitch += y * MOUSE_SENSITIVITY * fov/ZOOM_MAX;
    //constrain the yaw
    if(pitch > 89.0f) pitch = 89.0f;
    else if(pitch < -89.0f) pitch = -89.0f;
    //constranin the pitch
    yaw = fmod(yaw, 360.0f);
    updateVectors();
}

void Camera::zoom(float scroll) {
    fov += scroll * ZOOM_SPEED;
    if(fov < ZOOM_MAX) fov = ZOOM_MAX;
    else if(fov > ZOOM_MIN) fov = ZOOM_MIN;
    setFov();
}

void Camera::setFasterSpeed(bool speed_up) {
    if(speed_up) speed = CAMERA_SPEED_FAST;
    else speed = CAMERA_SPEED_NORMAL;
}

void Camera::setSlowerSpeed(bool speed_down) {
    if(speed_down) speed = CAMERA_SPEED_SLOW;
    else speed = CAMERA_SPEED_NORMAL;
}

void Camera::setSize(float new_aspect) {
    aspect = new_aspect;
    setFov();
}

float* Camera::transferData() const {
    static float data[12];
    
    data[0] = position.x;
    data[1] = position.y;
    data[2] = position.z;
    data[3] = lower_left_corner.x;
    data[4] = lower_left_corner.y;
    data[5] = lower_left_corner.z;
    data[6] = horizontal.x;
    data[7] = horizontal.y;
    data[8] = horizontal.z;
    data[9] = vertical.x;
    data[10] = vertical.y;
    data[11] = vertical.z;
    
    return data;
    
    /*static float data[12] = {
        position.x, position.y, position.z,
        lower_left_corner.x, lower_left_corner.y, lower_left_corner.z,
        horizontal.x, horizontal.y, horizontal.z,
        vertical.x, vertical.y, vertical.z
    };*/
}
