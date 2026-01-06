#version 450 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;

uniform mat4 MVP;
uniform mat4 Model;

// ---- terrain params -----
uniform int   u_octaves;
uniform int   u_seed;
uniform float u_frequency;
uniform float u_amplitude;
uniform float u_lacunarity;
uniform float u_persistance;
uniform float u_hydro_strength;
uniform float u_thermal_strength;

out vec3  v_normal;
out vec3  v_world_pos;
out float v_height;



// ------------- Perlin noise helpers -------------
int hash(int x, int seed) {
    x += seed * 0x27d4eb2d;       
    x ^= x >> 16;                   
    x *= 0x45d9f3b;                   
    x ^= x >> 16;                     
    return x & 255;                  
}

float fade(float t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float fade_deriv(float t) {
    return 30.0 * t * t * (t - 1.0) * (t - 1.0);
}

float lerp(float a, float b, float t) {
    return a + t * (b - a);
}


float grad(int hash, float x, float y) {
    int h   = hash & 7;
    float u = (h < 4) ? x : y;
    float v = (h < 4) ? y : x;

    return ((h & 1) != 0 ? -u : u) + ((h & 2) != 0 ? -v : v);
}

float grad_x(int hash, float x, float y) {
    int h = hash & 7;

    if (h < 4) return ((h & 1) != 0 ? -1.0 : 1.0);
    else       return 0.0;
    
}

float grad_y(int hash, float x, float y) {
    int h = hash & 7;

    if (h < 4) return ((h & 2) != 0 ? -1.0 : 1.0);
    else       return ((h & 1) != 0 ? -1.0 : 1.0);

}


vec3 perlin_deriv(vec2 p) {
    ivec2 pi = ivec2(floor(p)) & 255;
    vec2  pf = fract(p);

    float u     = fade(pf.x);
    float v     = fade(pf.y);
    float du_dx = fade_deriv(pf.x);
    float dv_dy = fade_deriv(pf.y);

    int aa = hash(hash(pi.x, u_seed) + pi.y, u_seed);
    int ab = hash(hash(pi.x, u_seed) + pi.y + 1, u_seed);
    int ba = hash(hash(pi.x + 1, u_seed) + pi.y, u_seed);
    int bb = hash(hash(pi.x + 1, u_seed) + pi.y + 1, u_seed);

    float a = grad(aa, pf.x, pf.y);
    float b = grad(ba, pf.x - 1.0, pf.y);
    float c = grad(ab, pf.x, pf.y - 1.0);
    float d = grad(bb, pf.x - 1.0, pf.y - 1.0);

    float ax = grad_x(aa, pf.x, pf.y);
    float bx = grad_x(ba, pf.x - 1.0, pf.y);
    float cx = grad_x(ab, pf.x, pf.y - 1.0);
    float dx = grad_x(bb, pf.x - 1.0, pf.y - 1.0);

    float ay = grad_y(aa, pf.x, pf.y);
    float by = grad_y(ba, pf.x - 1.0, pf.y);
    float cy = grad_y(ab, pf.x, pf.y - 1.0);
    float dy = grad_y(bb, pf.x - 1.0, pf.y - 1.0);

    float lerp_ab   = lerp(a, b, u);
    float lerp_ab_x = lerp(ax, bx, u) + (b - a) * du_dx;
    float lerp_cd   = lerp(c, d, u);
    float lerp_cd_x = lerp(cx, dx, u) + (d - c) * du_dx;

    float height  = lerp(lerp_ab, lerp_cd, v);
    float deriv_x = lerp(lerp_ab_x, lerp_cd_x, v);

    float lerp_ab_y = lerp(ay, by, u);
    float lerp_cd_y = lerp(cy, dy, u);
    float deriv_y   = lerp(lerp_ab_y, lerp_cd_y, v) + (lerp_cd - lerp_ab) * dv_dy;

    return vec3(height, deriv_x, deriv_y);
}

vec3 fbm_erosion_deriv(vec2 p) {
    vec3 result  = vec3(0.0); // x = h, y = dh/dx, z = dh/dy
    vec2 gradSum = vec2(0.0); // running gradient

    float freq = u_frequency;
    float amp  = u_amplitude;

    for (int i = 0; i < u_octaves; i++) {
        vec3 noise = perlin_deriv(p * freq);

        // noise derivatives -> world space
        vec2 dn = noise.yz * freq;

        gradSum += dn * amp;

        // hydro erosion influence
        float slope = length(gradSum);  // slope for erosion
        float hydro = 1.0 / (1.0 + slope * u_hydro_strength);

        // thermal erosion influence
        float curvature = length(dn - gradSum);
        float thermal   = 1.0 - exp(-curvature * u_thermal_strength);

        float erosion   = hydro * (1.0 - thermal);
        float octaveAmp = amp   * erosion;

        // height
        result.x += noise.x * octaveAmp;

        // derivatives
        result.y += dn.x * octaveAmp;
        result.z += dn.y * octaveAmp;

        freq *= u_lacunarity;
        amp  *= u_persistance;
    }
    return result;
}


void main() {
    vec3 pos = inPos;

    vec3 h_deriv = fbm_erosion_deriv(pos.xz);
    
    pos.y    = h_deriv.x;
    v_height = pos.y;


    vec3 n = normalize(vec3(-h_deriv.y, 1.0, -h_deriv.z));

    v_normal    = mat3(Model) * n;
    v_world_pos = vec3(Model * vec4(pos, 1.0));
    gl_Position = MVP * vec4(pos, 1.0);
}
