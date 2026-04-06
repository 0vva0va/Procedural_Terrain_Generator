#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "Camera.h"



class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap(); 
    
    GLuint GetLightDepthMap() const;
    GLuint GetLightFBO()      const;
    GLuint GetMatricesUBO()   const;
    
    std::vector<glm::mat4> GetLightSpaceMatrices();
    glm::mat4 GetLightSpaceMatrix(const float nearPlane, const float farPlane);
    std::vector<glm::vec4> GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);

    
    unsigned int DEPTH_MAP_RES = 4096;
    std::vector<float> shadowCascadeLevels{ 1000.0f / 50.0f, 1000.0f / 25.0f, 1000.0f / 10.0f, 1000.0f / 2.0f };
    
    Camera cam;
    
    glm::vec3 lightDir;
    float camNearPlane;
    float camFarPlane;
    float fbHeight;
    float fbWidth;
    
    
private:
    void SetupDepthMapAndFBO();    
    

private:
    GLuint lightDepthMap;
    GLuint lightFBO;
    GLuint matricesUBO;
};