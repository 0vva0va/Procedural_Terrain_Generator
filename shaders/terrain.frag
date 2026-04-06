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
// -----------------------------------------------
vec3 ApplyFog(vec3 col, float t, vec3 viewDir, vec3 lightDir)
{
    float fogAmount = 1.0 - exp(-t * _FogDensity);
    float sunAmount = max(dot(viewDir, lightDir), 0.0);
    vec3  fogColor  = mix(vec3(0.5,0.6,0.7), // blue
                          vec3(1.0,0.9,0.7), // yellow
                          pow(sunAmount,8.0));
    return mix(col, fogColor, fogAmount);
}

vec3 ApplyFogSplit(vec3 col, float t)
{
    vec3 be = vec3(0.05, 0.02, 0.01)* 0.05; // Red dies fast, blue survives far
    vec3 bi = vec3(0.01, 0.03, 0.08)* 0.05; // Blue fog dominates
    vec3 fogColor = vec3(0.2,0.3,0.4);
    vec3 extColor = vec3(exp(-t * be.x), 
                        exp(-t * be.y), 
                        exp(-t * be.z));
    vec3 insColor = vec3(1.0 -exp(-t * bi.x), 
    1.0 -exp(-t * bi.y), 
    1.0 -exp(-t * bi.z));
    
    // Combine: surviving original color + accumulated fog color
    vec3 finalColor = col * extColor + fogColor * insColor;
    return finalColor;
}

vec3 ApplyFogHeight(vec3 col, float distance, float fragHeight)
{
    if (fragHeight > 10.0) return col;
    
    float fogAmount  = 1.0 - exp(-distance * _FogDensity);
          fogAmount *= 1.0 - smoothstep(0.0, 10.0, fragHeight);
    
    return mix(col, vec3(0.5,0.6,0.7), clamp(fogAmount, 0.0, 1.0));
}

vec3 ApplyFogUltimate(
    vec3  col,           // Original pixel color
    float distance,      // Distance from camera to fragment
    float fragHeight,    // World-space Y of fragment
    vec3  viewDir,       // Normalized view direction
    vec3  lightDir       // Normalized sun/light direction
) {
    // --- PARAMETERS (make these uniforms) ---
    float fogHeightMin  = 0.0;      // Fog starts at this height
    float fogHeightMax  = 10.0;     // Fog ends at this height  
    float densityScale  = _FogDensity; // Overall density multiplier
    
    // Spectral scattering coefficients (RGB = different wavelengths)
    vec3  be = vec3(0.25, 0.10, 0.05) * densityScale; // Extinction: red dies fast
    vec3  bi = vec3(0.05, 0.15, 0.40) * densityScale; // Inscattering: blue dominates
    
    vec3  fogColorBase  = vec3(0.2, 0.3, 0.4);  // Ambient fog color
    vec3  fogColorSun   = vec3(1.0, 0.9, 0.7);  // Sun-facing fog color
    float sunPower      = 8.0;                  // Sun glow tightness
    
    // --- HEIGHT ATTENUATION ---
    // Smooth height falloff: 1.0 inside fog, 0.0 above fogHeightMax
    float heightFactor = 1.0 - smoothstep(fogHeightMin, fogHeightMax, fragHeight);
    if (heightFactor <= 0.0) return col; // Early exit if above fog
    
    // Effective optical distance through fog layer
    float opticalDepth = distance * heightFactor;
    
    // --- SPECTRAL ATMOSPHERIC SCATTERING ---
    // Transmittance: how much original light survives per channel
    vec3 transmittance = vec3(
        exp(-opticalDepth * be.x),
        exp(-opticalDepth * be.y),
        exp(-opticalDepth * be.z)
    );
    
    // Extinction factor: how much original color is lost
    vec3 extinction = 1.0 - transmittance;
    
    // Inscattering factor: how much fog color is added per channel  
    vec3 inscatter = vec3(
        1.0 - exp(-opticalDepth * bi.x),
        1.0 - exp(-opticalDepth * bi.y),
        1.0 - exp(-opticalDepth * bi.z)
    );
    
    // --- DIRECTIONAL LIGHT INSCATTERING ---
    // Sun glow when looking toward light
    float sunDot = max(dot(viewDir, lightDir), 0.0);
    float sunFactor = pow(sunDot, sunPower);
    
    // Blend base fog color toward sun color based on view angle
    vec3 fogColor = mix(fogColorBase, fogColorSun, sunFactor);
    
    // --- FINAL COMPOSITION ---
    // Extinction removes original color, inscattering adds fog color
    vec3 finalColor = col * transmittance + fogColor * inscatter;
    
    return finalColor;
}

float hash(vec3 p) {
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return fract(sin(p.x + p.y + p.z) * 43758.5453);
}

// Smooth 3D noise
float noise(vec3 x) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f); // Smoothstep
    
    float n = mix(
        mix(mix(hash(p + vec3(0,0,0)), hash(p + vec3(1,0,0)), f.x),
            mix(hash(p + vec3(0,1,0)), hash(p + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash(p + vec3(0,0,1)), hash(p + vec3(1,0,1)), f.x),
            mix(hash(p + vec3(0,1,1)), hash(p + vec3(1,1,1)), f.x), f.y),
        f.z
    );
    return n;
}

// FBM for wispy detail - 4 octaves
float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}
vec3 ApplyFogUltimateTimeDensity(
    vec3  col,           
    float distance,      
    float fragHeight,    
    vec3  worldPos,      // NEW: World-space position of fragment
    float time,          // NEW: Time for animation
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
    // Animate noise by scrolling in wind direction (e.g., along XZ plane)
    vec3 noisePos = worldPos * wispScale + vec3(time * wispSpeed, 0.0, time * wispSpeed * 0.5);
    
    // Sample FBM noise: range roughly 0..1, centered around 0.5
    float densityNoise = fbm(noisePos);
    
    // Option A: Modulate heightFactor for "holes" in fog (erosion)
    // heightFactor *= smoothstep(0.0, 0.4, densityNoise); 
    
    // Option B: Modulate density directly (recommended)
    // Remap noise from [0,1] to [1-wispStrength, 1+wispStrength] or [0, 1]
    float densityMod = mix(1.0 - wispStrength, 1.0, densityNoise);
    
    // Add vertical variation: fog often stratifies into layers
    float verticalWisp = sin(worldPos.y * wispScale * wispHeightAtt + time * 0.1) * 0.5 + 0.5;
    densityMod *= mix(0.7, 1.3, verticalWisp); // 30% variation by height
    
    // --- OPTICAL DEPTH WITH VARIABLE DENSITY ---
    // Approximation: density at fragment * distance through fog layer
    float opticalDepth = distance * heightFactor * densityMod;
    
    // --- SPECTRAL SCATTERING ---
    vec3 transmittance = exp(-opticalDepth * be);
    vec3 extinction = 1.0 - transmittance;
    vec3 inscatter = 1.0 - exp(-opticalDepth * bi);
    
    // --- DIRECTIONAL LIGHT INSCATTERING ---
    float sunDot = max(dot(viewDir, lightDir), 0.0);
    float sunFactor = pow(sunDot, sunPower);
    
    // Optional: Make sun glow "break through" dense wisps (light shafts)
    // Reduce inscatter in dense areas when looking toward light for god-rays
    float lightShaft = pow(densityNoise, 2.0) * sunFactor * 2.0;
    vec3 fogColor = mix(fogColorBase, fogColorSun, sunFactor + lightShaft);
    
    // --- FINAL COMPOSITION ---
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



// Main
// -----------------------------------------------
void main() 
{
    vec3 N = normalize(FS_Normal);
    vec3 V = normalize(_CamPos - FS_WorldPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), _Albedo, _Metallic);
           
    // ---- direct lighting (single point light) ----
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
