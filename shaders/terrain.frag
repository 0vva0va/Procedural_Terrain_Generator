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
uniform vec3  _LightPos;
uniform vec3  _LightCol;
uniform float _LightIntensity;
uniform float _EnvironmentLightStrength;
uniform float _FogDensity;

float _FarPlane = 1000.0f;  

uniform vec3 _CamPos;

uniform samplerCube _IrradianceMap;  // unit 1
uniform samplerCube _ShadowMap;      // unit 2

out vec4 FragColor;


const float PI = 3.14159265359;


// ---------------------------------------------------------------------------
// Fog
// ---------------------------------------------------------------------------
vec3 ApplyFog(vec3 col, float t)
{
    float fogAmount = 1.0 - exp(-t * _FogDensity);
    vec3  fogColor  = vec3(0.5, 0.6, 0.7);
    return mix(col, fogColor, fogAmount);
}


// ---------------------------------------------------------------------------
// Point-light PCF shadow
//   Samples the depth cubemap in a small neighbourhood of directions and
//   averages the results for a soft penumbra.
// ---------------------------------------------------------------------------
float PointShadow(vec3 fragWorldPos)
{
    vec3  fragToLight  = fragWorldPos - _LightPos;
    float currentDepth = length(fragToLight);
    
    vec3  L      = normalize(-fragToLight);
    float NdotL  = max(dot(normalize(FS_Normal), L), 0.0);
    float bias   = mix(8.0, 0.5, NdotL);  
    bias        *= (currentDepth / _FarPlane);  
    bias         = max(bias, 0.1);           

    vec3 sampleOffsets[20] = vec3[](
        vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
        vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
        vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
        vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
        vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
    );

    float viewDist   = length(_CamPos - fragWorldPos);
    float diskRadius = mix(0.05, 0.3, viewDist / _FarPlane);
    
    // Rotate PCF disc by a per-fragment angle
    float angle = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) * 2.0 * PI;
    mat2 rot = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));

    float shadow = 0.0;
    for (int i = 0; i < 20; ++i)
    {
        vec3 offset = vec3(sampleOffsets[i].xy * rot, sampleOffsets[i].z);
        float closestDepth = texture(_ShadowMap, fragToLight + offset * diskRadius).r;
        closestDepth *= _FarPlane;

        if (currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    float shadowFade = 1.0 - smoothstep(0.7 * _FarPlane, _FarPlane, currentDepth);
    return (shadow / 20.0) * shadowFade;
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
    vec3  L           = normalize(_LightPos);
    vec3  H           = normalize(V + L);
    float dist        = length(_LightPos - FS_WorldPos);
    float attenuation = 1.0 / (dist * dist);
    vec3  radiance    = _LightCol * attenuation * _LightIntensity;

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
    float shadow = PointShadow(FS_WorldPos);   
    
    ambient *= (1.0 - shadow * 0.2);

    vec3 color = ambient + Lo * (1.0 - shadow);
         color = ApplyFog(color, length(_CamPos - FS_WorldPos));
         
         color = ACESFilm(color);
         color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
