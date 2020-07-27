#version 410 core
layout (location = 0) in vec3 aPos;

out vec2 fragPos;

void main() {
    fragPos = vec2((aPos.x+1.0f)*0.5f, (aPos.y+1.0f)*0.5f); //1.0f-(aPos.y+1.0f)*0.5f //we have to substract due to how the screen coordiantes are defined
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0f);
}

