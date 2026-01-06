#include <cmath>
#include <numeric>
#include <random>
#include <iostream>

#include "terrain.h"
#include <glm/glm.hpp>



std::vector<Patch> patches;


Terrain::Terrain(int size) : size(size) {
    patch_size = std::max(4, patch_size); 
    Generate_Mesh();
    Upload_To_GPU();
}

Terrain::~Terrain() {
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}

// ------------- generate simple flat mesh -------------
void Terrain::Generate_Mesh() {
    vertices.resize(size * size);
    indices.clear();
    patches.clear();

    // ----- vertices (unchanged) -----
    for (int z = 0; z < size; ++z) {
        for (int x = 0; x < size; ++x) {
            Vertex& v = vertices[z * size + x];
            v.pos     = { (float)x, 0.0f, (float)z };
            v.normal  = { 0.0f, 1.0f, 0.0f };
        }
    }

    // ----- build patches -----
    for (int pz = 0; pz < size - 1; pz += patch_size) {
        for (int px = 0; px < size - 1; px += patch_size) {

            Patch patch{};
            patch.startX = px;
            patch.startZ = pz;
            patch.size   = patch_size;

            patch.indexOffset = indices.size(); // record where this patch starts

            int endX = std::min(px + patch_size, size - 1);
            int endZ = std::min(pz + patch_size, size - 1);

            for (int z = pz; z < endZ; ++z) {
                for (int x = px; x < endX; ++x) {

                    unsigned int i = z * size + x;

                    indices.insert(indices.end(), {
                        i,
                        i + size,
                        i + 1,

                        i + 1,
                        i + size,
                        i + size + 1
                    });
                }
            }

            patch.indexCount = indices.size() - patch.indexOffset;

            patches.push_back(patch);
        }
    }

    index_count = indices.size();
}

// ------------- terrain parameter helpers -------------
void Terrain::Set_Params(const Terrain_Params& p) {
    params = p;
}

const Terrain_Params& Terrain::Get_Params() const {
    return params;
}


void Terrain::Upload_To_GPU() {
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glBindVertexArray(0);
}

void Terrain::Draw() const {
    glBindVertexArray(vao);

    for (const Patch& patch : patches) {
        glDrawElements(GL_TRIANGLES, patch.indexCount, GL_UNSIGNED_INT, (void*)(patch.indexOffset * sizeof(uint32_t))); 
    }
}
