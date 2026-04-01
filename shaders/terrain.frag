#version 450 core

in vec3 FS_Normal;
in vec3 FS_WorldPos;

// material parameters
uniform vec3  _Albedo;
uniform float _Metallic;
uniform float _Roughness;
uniform float _AO;

vec3 GrassCol = vec3(0.247, 0.608, 0.043);

// lights
uniform vec3  _LightDir;
uniform vec3  _LightCol;
uniform float _LightIntensity;
uniform float _EnvironmentLightStrength;
uniform float _FogDensity;

float _FarPlane = 1000.0f;  
uniform mat4 _View;

uniform vec3 _CamPos;

uniform samplerCube _IrradianceMap;  // unit 1
uniform sampler2DArray _ShadowMap;   // unit 2

layout (std140) uniform LightSpaceMatrices { mat4 lightSpaceMatrices[16]; };
uniform float cascadePlaneDistances[16];
uniform int   cascadeCount;   // number of frusta - 1


out vec4 FragColor;


const float PI = 3.14159265359;



// Fog
// -----------------------------------------------
vec3 ApplyFog(vec3 col, float t)
{
    float fogAmount = 1.0 - exp(-t * _FogDensity);
    vec3  fogColor  = vec3(0.5, 0.6, 0.7);
    return mix(col, fogColor, fogAmount);
}


// Cascade Shadow Map
// ---------------------------------------------------------------------------
float SampleShadow(vec3 fragPosWorldSpace, int layer)
{
    vec4  fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
    vec3  projCoords        = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;
    if (currentDepth > 1.0) return 0.0;

    vec3  normal = normalize(FS_Normal);
    float bias   = max(0.05 * (1.0 - dot(normal, _LightDir)), 0.005);
    bias *= 1.0 / ((layer == cascadeCount ? _FarPlane : cascadePlaneDistances[layer]) * 0.5);

    float shadow    = 0.0;
    vec2  texelSize = 1.0 / vec2(textureSize(_ShadowMap, 0));
    for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y)
    {
        float pcfDepth = texture(_ShadowMap, vec3(projCoords.xy + vec2(x,y) * texelSize, layer)).r;
        shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
    }
    return shadow / 9.0;
}

float ShadowCalculation(vec3 fragPosWorldSpace)
{
    vec4  fragPosViewSpace = _View * vec4(fragPosWorldSpace, 1.0);
    float depthValue       = abs(fragPosViewSpace.z);

    int layer = cascadeCount; // default to last
    for (int i = 0; i < cascadeCount; ++i)
    {
        if (depthValue < cascadePlaneDistances[i])
        {
            layer = i;
            break;
        }
    }

    // --- sample primary layer ---
    float shadow = SampleShadow(fragPosWorldSpace, layer);

    // --- blend into next layer near the boundary ---
    const float blendRange = 0.1; // 10% of cascade depth blends into next
    float cascadeNear = (layer == 0) ? 0.0 : cascadePlaneDistances[layer - 1];
    float cascadeFar  = (layer == cascadeCount) ? _FarPlane : cascadePlaneDistances[layer];
    float cascadeLen  = cascadeFar - cascadeNear;
    float blendStart  = cascadeFar - cascadeLen * blendRange;

    if (depthValue > blendStart && layer + 1 <= cascadeCount)
    {
        float nextShadow = SampleShadow(fragPosWorldSpace, layer + 1);
        float t = (depthValue - blendStart) / (cascadeLen * blendRange);
        shadow = mix(shadow, nextShadow, t);
    }

    return shadow;
}


// ---------------------------------------------------------------------------
// PBR helpers
// ---------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) 
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


// ---------------------------------------------------------------------------
// Tonemapping
// ---------------------------------------------------------------------------
vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}


// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
void main() 
{
    vec3 N = normalize(FS_Normal);
    vec3 V = normalize(_CamPos - FS_WorldPos);

    vec3 F0 = mix(vec3(0.04), _Albedo, _Metallic);
           
    // ---- direct lighting (single point light) ----
    vec3  L           = -normalize(_LightDir);
    vec3  H           =  normalize(V + L);
    // float dist        = length(_LightPos - FS_WorldPos);
    // float attenuation = 1.0 / (dist * dist);
    // vec3  radiance    = _LightCol * attenuation * _LightIntensity;
    vec3  radiance = _LightCol * _LightIntensity;

    float NDF = DistributionGGX(N, H, _Roughness);
    float G   = GeometrySmith(N, V, L, _Roughness);
    vec3  F   = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, _Roughness);

    vec3  kS        = F;
    vec3  kD        = (vec3(1.0) - kS) * (1.0 - _Metallic);
    vec3  specular  = (NDF * G * F) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
    float NdotL     = max(dot(N, L), 0.0);

    vec3 Lo = (kD * _Albedo / PI + specular) * radiance * NdotL;

    // ---- IBL ambient ----
    vec3 kS_amb = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 kD_amb = (1.0 - kS_amb) * (1.0 - _Metallic);

    vec3 skyColor    = vec3(0.4, 0.6, 1.0);
    vec3 groundColor = vec3(0.2, 0.15, 0.1);
    float hemi       = N.y * 0.5 + 0.5;
    vec3 hemiLight   = mix(groundColor, skyColor, hemi) * _EnvironmentLightStrength;


    float lambda  = smoothstep(0.95, 0.99, N.y);
    vec3  fColMat = mix(_Albedo, GrassCol, lambda);

    vec3 diffuse = hemiLight * fColMat;
    vec3 ambient = (kD_amb * diffuse) * _AO;
         
    // ---- shadow ----
    float shadow = ShadowCalculation(FS_WorldPos);     

    vec3 color = ambient + (1.0 - shadow) * Lo;
         color = ApplyFog(color, length(_CamPos - FS_WorldPos));
         
         color = ACESFilm(color);
         color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
