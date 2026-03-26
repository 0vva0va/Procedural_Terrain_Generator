#version 450 core

in vec3  FS_Normal;
in vec3  FS_WorldPos;

// material parameters
uniform vec3  _Albedo;
uniform float _Metallic;
uniform float _Roughness;
uniform float _AO;

// lights
uniform vec3  _LightPos;
uniform vec3  _LightCol;
uniform float _LightIntensity;
uniform float _EnvironmentLightStrength;

uniform vec3  _CamPos;

uniform samplerCube _IrradianceMap;

out vec4 FragColor;


const float PI = 3.14159265359;


float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}


vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}


void main() 
{
    vec3 N = normalize(FS_Normal);
    vec3 V = normalize(_CamPos - FS_WorldPos);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, _Albedo, _Metallic);
	           
    // reflectance equation
    vec3 Lo = vec3(0.0);
    vec3 L = normalize(_LightPos - FS_WorldPos);
    vec3 H = normalize(V + L);
    float distance    = length(_LightPos - FS_WorldPos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance     = _LightCol * attenuation * _LightIntensity;        
        
    // cook-torrance brdf
    float NDF = DistributionGGX(N, H, _Roughness);        
    float G   = GeometrySmith(N, V, L, _Roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);       
        
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - _Metallic;	  
        
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;  
            
    // add to outgoing radiance Lo
    float NdotL = max(dot(N, L), 0.0);                
    Lo += (kD * _Albedo / PI + specular) * radiance * NdotL; 

    kS  = fresnelSchlick(max(dot(N, V), 0.0), F0);
    kD  = 1.0 - kS;
    kD *= 1.0 - _Metallic;	  
    vec3 irradiance  = texture(_IrradianceMap, N).rgb;
         irradiance *= _EnvironmentLightStrength;
    vec3 diffuse    = irradiance * _Albedo;
    vec3 ambient    = (kD * diffuse) * _AO;
    
    vec3 color = ambient + Lo;
	
    color = ACESFilm(color);
    color = pow(color, vec3(1.0/2.2));  
   
    FragColor = vec4(color, 1.0);
}