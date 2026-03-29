#include <glm/glm.hpp>

#include "shader_c.h"
#include "Terrain.h"


Terrain::Terrain(int size) : size(size) 
{
    Generate_Mesh();
    Upload_To_GPU();
}


Terrain::~Terrain() 
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}


// ------------- generate simple flat mesh -------------
void Terrain::Generate_Mesh() 
{
    unsigned int resolution = size;
    const unsigned int vertCountPerSide = resolution + 1;

    const float halfSize = size * 0.5f;
    const float step     = size / static_cast<float>(resolution);

    // -----------------------
    // Generate vertex positions
    // -----------------------
    vertices.reserve(vertCountPerSide * vertCountPerSide * 5);

    for (unsigned int z = 0; z < vertCountPerSide; ++z)
    {
        for (unsigned int x = 0; x < vertCountPerSide; ++x)
        {
            float xpos = x * step - halfSize;
            float zpos = z * step - halfSize;

            float u = static_cast<float>(x) / static_cast<float>(resolution);
            float v = static_cast<float>(z) / static_cast<float>(resolution);

            vertices.push_back(xpos);   // x
            vertices.push_back(0.0f);   // y 
            vertices.push_back(zpos);   // z

            vertices.push_back(u);      // u
            vertices.push_back(v);      // v
        }
    }

    // -----------------------
    // Generate triangle indices
    // -----------------------
    for (unsigned int z = 0; z < resolution; ++z)
    {
        for (unsigned int x = 0; x < resolution; ++x)
        {
            unsigned int topLeft     =  z      * vertCountPerSide + x;
            unsigned int topRight    =  topLeft + 1;
            unsigned int bottomLeft  = (z + 1) * vertCountPerSide + x;
            unsigned int bottomRight =  bottomLeft + 1;

            // First triangle
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            // Second triangle
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    idxCount = static_cast<GLsizei>(indices.size());
}

// ------------- terrain parameter helpers -------------
void Terrain::SetParams(const TerrainParams& p) { params = p; }
const TerrainParams& Terrain::GetParams() const { return params; }


void Terrain::Upload_To_GPU() 
{
    glGenVertexArrays(1, &vao);
    
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);
}


void Terrain::SetupNoiseSSBO()
{
    int vertCount = (size + 1) * (size + 1);
    glGenBuffers(1, &terrainHeightSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrainHeightSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, vertCount * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    
    glGenBuffers(1, &terrainNormalsSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, terrainNormalsSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, vertCount * sizeof(float) * 4, nullptr, GL_DYNAMIC_DRAW);
    
}


void Terrain::BindSSBOBuffers()
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, terrainHeightSSBO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, terrainNormalsSSBO);
}


void Terrain::Draw() const 
{
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, idxCount, GL_UNSIGNED_INT, 0);
}
