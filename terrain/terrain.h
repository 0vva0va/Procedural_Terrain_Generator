#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
};

struct Patch {
    int startX;
    int startZ;
    int size;

    // Index information for rendering
    uint32_t indexOffset;   // offset into the global EBO (in elements, not bytes)
    uint32_t indexCount;
};

struct Terrain_Params {
    int   octaves     = 4;
    int   seed        = 0;
    float frequency   = 0.01f;
    float amplitude   = 20.0f;
    float lacunarity  = 2.0f;
    float persistence = 0.5f;
    float hydro       = 0.5f;
    float thermal     = 0.5f;
    float fog_density = 0.0005f;
};

class Terrain {
public:
    Terrain(int size);
    ~Terrain();

    void Set_Params(const Terrain_Params& params);
    const Terrain_Params& Get_Params() const;

    void Draw() const;


private:
    void Generate_Mesh();
    void Upload_To_GPU();

    
private:
    int size;
    int patch_size = 8;

    Terrain_Params params;
    
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    uint32_t index_count = 0;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
