#pragma once

#include <glm/glm.hpp>

class Camera {
public:
    Camera(
        glm::vec3 position = { 0.0f, 20.0f, 30.0f },
        float yaw          = -90.0f,
        float pitch        = -20.0f
    );

    void Update(float dt);

    void On_Mouse_Move(double xpos, double ypos);
    void On_Key(int key, int action);
    void Reset_Mouse();

    glm::mat4 Get_View_Matrix() const;
    glm::mat4 Get_Projection_Matrix(float aspect) const;

    float speed       = 80.0f;
    float sensitivity = 0.1f;
    float fov         = 90.0f;

    glm::vec3 position;

    
private:
    void Update_Vectors();


private:
    float yaw;
    float pitch;

    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;

    bool keys[1024]  = {};
    bool first_mouse = true;
    
    double last_x = 0.0;
    double last_y = 0.0;
};
