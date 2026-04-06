#pragma once

#include "shader_t.h"
#include <glad/glad.h>



class Skybox {
public:
    Skybox();
    ~Skybox();

    void Draw() const;

    void BakeIrradiance(Shader& irradianceShader);
    void SetupPrefilterMap(Shader& prefilterShader);
    void SetupBrdfLUTTex(Shader& prefilterShader);
    
    GLuint GetSkyboxTex()     const;
    GLuint GetIrradianceMap() const;
    GLuint GetPrefilterMap()  const;
    GLuint GetBrdfTexture()   const;

private:
    void RenderQuad();
    void GenerateSkybox();
    void SetupIrradianceFBO();
    void UploadToGPU();

    
private:
    GLuint skyboxTex;

    GLuint skyboxVAO;
    GLuint skyboxVBO;
    GLuint quadVAO;
    GLuint quadVBO;
    
    unsigned int irradianceMap;
    unsigned int prefilterMap;
    unsigned int brdfLUTTexture;
    unsigned int captureFBO, captureRBO;
};
