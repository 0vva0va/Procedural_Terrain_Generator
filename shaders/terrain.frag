#version 450 core

in vec3 FS_Normal;
in vec3 FS_WorldPos;

// material parameters
uniform vec3  _Albedo;
uniform float _Metallic;
uniform float _Roughness;
uniform float _AO;

vec3 GrassCol = vec3(0.247, 0.608, 0.043);

// light settings
uniform vec3  _LightDir;
uniform vec3  _LightCol;
uniform float _LightIntensity;
uniform float _EnvironmentLightStrength;

// fog settings
uniform vec3  _BE;
uniform vec3  _BI;
uniform vec3  _FogColorBase;
uniform vec3  _FogColorSun;
uniform float _SunPower;
uniform float _FogDensity;
uniform float _FogHeightMin;
uniform float _FogHeightMax;
uniform float _WispScale;
uniform float _WispSpeed;
uniform float _WispStrength;
uniform float _WispHeightAtt;



float _FarPlane = 1000.0f;  
uniform vec4 _View;
uniform vec3 _CamPos;
uniform float _Time;

uniform samplerCube _IrradianceMap;  // unit 1
uniform samplerCube _PrefilterMap;   // unit 2
uniform sampler2D   _BrdfLUT;        // unit 3

uniform sampler2DArray _ShadowMap;   // unit 4

// shadow settings
layout (std140) uniform _LightSpaceMatrices { mat4 lightSpaceMatrices[16]; };
uniform float _CascadePlaneDistances[16];
uniform int   _CascadeCount; 


out vec4 FragColor;


const float PI = 3.14159265359;



// Fog
// ----
vec3 ApplyFog(
    vec3  col,           
    float distance,      
    float fragHeight,    
    vec3  worldPos,     
    float time,         
    vec3  viewDir,       
    vec3  lightDir       
) {
    // --- PARAMETERS ---
    float fogHeightMin = _FogHeightMin;      
    float fogHeightMax = _FogHeightMax;     
    float densityScale = _FogDensity; 
    
    float wispScale     = _WispScale;     // Spatial scale of fog blobs (smaller = bigger blobs)
    float wispSpeed     = _WispSpeed;     // Drift speed
    float wispStrength  = _WispStrength;  // How much density varies (0 = constant, 1 = full variation)
    float wispHeightAtt = _WispHeightAtt; // How much vertical variation differs from horizontal
    
    vec3  be = _BE * densityScale; 
    vec3  bi = _BI * densityScale; 
    
    vec3  fogColorBase = _FogColorBase;  
    vec3  fogColorSun  = _FogColorSun;  
    float sunPower     = _SunPower;                  
    
    // --- HEIGHT ATTENUATION ---
    float heightFactor = 1.0 - smoothstep(fogHeightMin, fogHeightMax, fragHeight);
    if (heightFactor <= 0.0) return col; 
    
    // --- NON-CONSTANT DENSITY (WISPY FOG) ---
    vec3 noisePos = worldPos * wispScale + vec3(time * wispSpeed, 0.0, time * wispSpeed * 0.5);
    
    float densityNoise = fbm(noisePos);
    float densityMod   = mix(1.0 - wispStrength, 1.0, densityNoise);
    
    float verticalWisp = sin(worldPos.y * wispScale * wispHeightAtt + time * 0.1) * 0.5 + 0.5;
    
    densityMod *= mix(0.7, 1.3, verticalWisp);
    
    // --- OPTICAL DEPTH WITH VARIABLE DENSITY ---
    float opticalDepth = distance * heightFactor * densityMod;
    
    // --- SPECTRAL SCATTERING ---
    vec3 transmittance = exp(-opticalDepth * be);
    vec3 extinction    = 1.0 - transmittance;
    vec3 inscatter     = 1.0 - exp(-opticalDepth * bi);
    
    // --- DIRECTIONAL LIGHT INSCATTERING ---
    float sunDot    = max(dot(viewDir, lightDir), 0.0);
    float sunFactor = pow(sunDot, sunPower);
    
    float lightShaft = pow(densityNoise, 2.0) * sunFactor * 2.0;
    vec3 fogColor    = mix(fogColorBase, fogColorSun, sunFactor + lightShaft);
    
    vec3 finalColor = col * transmittance + fogColor * inscatter;
    
    return finalColor;
}


// Cascade Shadow Map
// ---------------------------------------------------------------------------
float SampleShadow(vec3 fragPosWorldSpace, int layer, vec3 L)
{
    vec4  fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
    vec3  projCoords        = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    float currentDepth = projCoords.z;
    if (currentDepth > 1.0) return 0.0;

    vec3 normal = normalize(FS_Normal);
    float bias  = max(0.05 * (1.0 - dot(normal, L)), 0.005);
          bias *= 1.0 / ((layer == _CascadeCount ? _FarPlane : _CascadePlaneDistances[layer]) * 0.5);

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


float ShadowCalculation(vec3 fragPosWorldSpace, vec3 L)
{
    vec4  fragPosViewSpace = _View * vec4(fragPosWorldSpace, 1.0);
    float depthValue       = abs(fragPosViewSpace.z);

    int layer = _CascadeCount; 
    for (int i = 0; i < _CascadeCount; ++i)
    {
        if (depthValue < _CascadePlaneDistances[i])
        {
            layer = i;
            break;
        }
    }

    // --- sample primary layer ---
    float shadow = SampleShadow(fragPosWorldSpace, layer, L);

    // --- blend into next layer near the boundary ---
    const float blendRange = 0.1; // 10% of cascade depth blends into next
    float cascadeNear = (layer == 0) ? 0.0 : _CascadePlaneDistances[layer - 1];
    float cascadeFar  = (layer == _CascadeCount) ? _FarPlane : _CascadePlaneDistances[layer];
    float cascadeLen  = cascadeFar - cascadeNear;
    float blendStart  = cascadeFar - cascadeLen * blendRange;

    if (depthValue > blendStart && layer + 1 <= _CascadeCount)
    {
        float nextShadow = SampleShadow(fragPosWorldSpace, layer + 1, L);
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

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) 
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}



// Tonemapping
// --------------------
vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}



void main() 
{
    vec3 N = normalize(FS_Normal);
    vec3 V = normalize(_CamPos - FS_WorldPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), _Albedo, _Metallic);
           
    // ---- direct lighting ----
    vec3  L = -normalize(_LightDir);
    vec3  H =  normalize(V + L);
    
    vec3  radiance = _LightCol * _LightIntensity;

    float NDF = DistributionGGX(N, H, _Roughness);
    float G   = GeometrySmith(N, V, L, _Roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);
    
    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
    vec3  specular    = numerator / denominator;
    
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * ( .0 - _Metallic);
         
    float NdotL = max(dot(N, L), 0.0); 

    vec3 Lo = (kD * _Albedo / PI + specular) * radiance * NdotL;

    
    // ---- IBL ambient ----
    vec3 kS_amb = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, _Roughness);
    vec3 kD_amb = (1.0 - kS_amb) * (1.0 - _Metallic);

    float lambda  = smoothstep(0.95, 0.99, N.y);
    vec3  fColMat = mix(_Albedo, GrassCol, lambda);

    vec3 irradiance = texture(_IrradianceMap, N).rgb * _EnvironmentLightStrength;
    vec3 diffuse    = irradiance * fColMat;
    
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredCol = textureLod(_PrefilterMap, R, _Roughness * MAX_REFLECTION_LOD).rgb;    
    vec2 brdf           = texture(_BrdfLUT, vec2(max(dot(N, V), 0.0), _Roughness)).rg;
    vec3 specularAmb    = prefilteredCol * (F * brdf.x + brdf.y);
    
    vec3 ambient = (kD_amb * diffuse + specularAmb) * _AO;
         
    
    // ---- Shadow ----
    float shadow = ShadowCalculation(FS_WorldPos, L);     

    
    // ---- Final Colour ----
    vec3 color = ambient + (1.0 - shadow) * Lo;
         color = ApplyFogUltimateTimeDensity(color, length(_CamPos - FS_WorldPos), FS_WorldPos.y, FS_WorldPos, _Time, normalize(vec3(_CamPos - FS_WorldPos)), normalize(_LightDir));
         
         color = ACESFilm(color);
         color = pow(color, vec3(1.0 / 2.2));

         
    FragColor = vec4(color, 1.0);
}
