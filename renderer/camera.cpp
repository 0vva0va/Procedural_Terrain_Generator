#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>

Camera::Camera(glm::vec3 pos, float y, float p) : position(pos), yaw(y), pitch(p) {
    Update_Vectors();
    std::memset(keys, 0, sizeof(keys));
}

void Camera::On_Key(int key, int action) {
    if (key < 0 || key >= 1024) return;

    if      (action == 1) keys[key] = true;    // GLFW_PRESS 
    else if (action == 0) keys[key] = false;   // GLFW_RELEASE
        
}

void Camera::On_Mouse_Move(double xpos, double ypos) {
    if (first_mouse) {
        last_x      = xpos;
        last_y      = ypos;
        first_mouse = false;
        return;
    }

    float dx = float(xpos - last_x);
    float dy = float(last_y - ypos);

    last_x = xpos;
    last_y = ypos;

    yaw   += dx * sensitivity;
    pitch += dy * sensitivity;

    pitch = std::clamp(pitch, -89.0f, 89.0f);

    Update_Vectors();
}

void Camera::Reset_Mouse() {
    first_mouse = true;
}

void Camera::Update(float dt) {
    float velocity = speed * dt;

    if (keys['W']) position += front * velocity;
    if (keys['S']) position -= front * velocity;
    if (keys['A']) position -= right * velocity;
    if (keys['D']) position += right * velocity;

    if (keys[' ']) position.y += velocity;
    if (keys[340]) position.y -= velocity; // GLFW_KEY_LEFT_SHIFT
}

glm::mat4 Camera::Get_View_Matrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::Get_Projection_Matrix(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.0f);
}

void Camera::Update_Vectors() {
    glm::vec3 f;

    f.x   = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    f.y   = sin(glm::radians(pitch));
    f.z   = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(f);

    up    = { 0.0f, 1.0f, 0.0f };
    right = glm::normalize(glm::cross(front, up));
    up    = glm::normalize(glm::cross(right, front));
}
