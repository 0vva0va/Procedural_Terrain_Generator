#pragma once

#include "shader_t.h"
#include <glad/glad.h>



class Skybox {
public:
    Skybox();
    ~Skybox();

    void Draw() const;

    void  BakeIrradiance(Shader& irradianceShader);
    
    GLuint GetSkyboxTex()     const;
    GLuint GetIrradianceMap() const;

private:
    void GenerateSkybox();
    void SetupIrradianceFBO();
    void UploadToGPU();

    
private:
    GLuint skyboxTex;

    GLuint skyboxVAO;
    GLuint skyboxVBO;
    
    unsigned int irradianceMap;
    unsigned int captureFBO, captureRBO;
};
