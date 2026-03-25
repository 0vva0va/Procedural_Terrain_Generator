#pragma once

#include <glad/glad.h>



class Skybox {
public:
    Skybox();
    ~Skybox();

    void Draw() const;

    GLuint GetSkyboxTex() const;

private:
    void  GenerateSkybox();
    void  UploadToGPU();

    
private:
    GLuint skyboxTex;

    GLuint skyboxVAO;
    GLuint skyboxVBO;
};
