#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>




struct TerrainParams 
{
    int   octaves     = 8;
    int   seed        = 0;
    float frequency   = 0.01f;
    float amplitude   = 20.0f;
    float lacunarity  = 2.0f;
    float persistence = 0.5f;
    float hydro       = 0.5f;
    float thermal     = 0.5f;
    float fogDensity  = 0.0005f;
};


class Terrain {
public:
    Terrain(int size);
    ~Terrain();

    void SetParams(const TerrainParams& params);
    const TerrainParams& GetParams() const;

    void Draw() const;
    
    void SetupNoiseSSBO();
    void BindSSBOBuffers();
    
    GLuint terrainHeightSSBO  = 0;
    GLuint terrainNormalsSSBO = 0;


private:
    void Generate_Mesh();
    void Upload_To_GPU();

    
private:
    GLuint calcNoiseProgram;
    
    int size;

    TerrainParams params;
    
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    
    uint32_t idxCount = 0;

    std::vector<float>    vertices;
    std::vector<uint32_t> indices;
};
