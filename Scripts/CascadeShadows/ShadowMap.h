#pragma once

#include "Camera.h"
#include <glad/glad.h>



class ShadowMap {
public:
    ShadowMap();
    ~ShadowMap(); 
    
    GLuint depthMapFBO;
    GLuint depthMap;
    
    unsigned int SHADOW_WIDTH = 4096;
    unsigned int SHADOW_HEIGHT = 4096;
    
    
private:
    void SetupDepthMapAndFBO();    
};