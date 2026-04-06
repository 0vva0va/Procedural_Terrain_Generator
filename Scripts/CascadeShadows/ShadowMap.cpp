#include <glm/gtc/matrix_transform.hpp>
#include "ShadowMap.h"



ShadowMap::ShadowMap() 
{
    SetupDepthMapAndFBO();
}


ShadowMap::~ShadowMap() 
{
    glDeleteTextures(1, &lightDepthMap);
    glDeleteFramebuffers(1, &lightFBO);
}


void ShadowMap::SetupDepthMapAndFBO()
{
    glGenTextures(1, &lightDepthMap);
    glGenFramebuffers(1, &lightFBO);

    glBindTexture(GL_TEXTURE_2D_ARRAY, lightDepthMap);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, DEPTH_MAP_RES, DEPTH_MAP_RES, int(shadowCascadeLevels.size()) + 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    
    constexpr float bordercolor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, bordercolor);
    
    glBindFramebuffer(GL_FRAMEBUFFER, lightFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, lightDepthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    glGenBuffers(1, &matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, matricesUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4x4) * 16, nullptr, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, matricesUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}


std::vector<glm::vec4> ShadowMap::GetFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
{
    const auto projview = proj * view;
    const auto inv = glm::inverse(projview);

    std::vector<glm::vec4> frustumCorners;
    for (unsigned int x = 0; x < 2; ++x)
    {
        for (unsigned int y = 0; y < 2; ++y)
        {
            for (unsigned int z = 0; z < 2; ++z)
            {
                const glm::vec4 pt = inv * glm::vec4(2.0f * x - 1.0f, 2.0f * y - 1.0f, 2.0f * z - 1.0f, 1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }

    return frustumCorners;
}


glm::mat4 ShadowMap::GetLightSpaceMatrix(const float nearPlane, const float farPlane)
{
    const auto proj = glm::perspective(glm::radians(cam.fov), (float)fbWidth / (float)fbHeight, nearPlane, farPlane);
    const auto corners = GetFrustumCornersWorldSpace(proj, cam.Get_View_Matrix());

    glm::vec3 center = glm::vec3(0, 0, 0);
    for (const auto& v : corners)
    {
        center += glm::vec3(v);
    }
    center /= corners.size();

    const auto lightView = glm::lookAt(center - lightDir, center, glm::vec3(0.0f, 1.0f, 0.0f));

    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto& v : corners)
    {
        const auto trf = lightView * v;
        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    // Tune this parameter according to the scene
    constexpr float zMult = 100.0f;
    if (minZ < 0)
    {
        minZ *= zMult;
    }
    else
    {
        minZ /= zMult;
    }
    if (maxZ < 0)
    {
        maxZ /= zMult;
    }
    else
    {
        maxZ *= zMult;
    }
    
    float texelSizeX = (maxX - minX) / (float)DEPTH_MAP_RES;
    float texelSizeY = (maxY - minY) / (float)DEPTH_MAP_RES;
    
    minX = std::floor(minX / texelSizeX) * texelSizeX;
    maxX = std::floor(maxX / texelSizeX) * texelSizeX;
    minY = std::floor(minY / texelSizeY) * texelSizeY;
    maxY = std::floor(maxY / texelSizeY) * texelSizeY;

    const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
    return lightProjection * lightView;
}


std::vector<glm::mat4> ShadowMap::GetLightSpaceMatrices()
{
    std::vector<glm::mat4> ret;
    for (size_t i = 0; i < shadowCascadeLevels.size() + 1; ++i)
    {
        if (i == 0)
        {
            ret.push_back(GetLightSpaceMatrix(camNearPlane, shadowCascadeLevels[i]));
        }
        else if (i < shadowCascadeLevels.size())
        {
            ret.push_back(GetLightSpaceMatrix(shadowCascadeLevels[i - 1], shadowCascadeLevels[i]));
        }
        else
        {
            ret.push_back(GetLightSpaceMatrix(shadowCascadeLevels[i - 1], camFarPlane));
        }
    }
    return ret;
}


GLuint ShadowMap::GetLightDepthMap() const { return lightDepthMap; }
GLuint ShadowMap::GetLightFBO()      const { return lightFBO;      }
GLuint ShadowMap::GetMatricesUBO()   const { return matricesUBO;   }