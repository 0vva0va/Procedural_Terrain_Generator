#version 450 core

in vec3  v_normal;
in vec3  v_world_pos;
in float v_height;

uniform vec3 u_camera_pos;
uniform float u_fog_density;

out vec4 FragColor;


// -------------- EXPERIMENTAL FOG FUNCTION --------------
vec3 fog(vec3 col, float t, vec3 view_dir, vec3 light_dir){
    float fog_amount = 1 - exp(-t * u_fog_density);
    float sun_amount = max(dot(view_dir, light_dir), 0.0);
    vec3 fog_color   = mix(vec3(0.5, 0.6, 0.7), 
                           vec3(1.0, 0.9, 0.7),
                           pow(sun_amount, 8.0));

    return mix(col, fog_color, fog_amount);
}
// vec3 fog(vec3 col, float t){
//     //float fog_amount = 1 - exp(-t * u_fog_density);
//     vec3 be = vec3(u_fog_density);
//     vec3 bi = vec3(u_fog_density);

//     vec3 fog_color   = vec3(0.5, 0.6, 0.7);

//     //return mix(col, fog_color, fog_amount);
//     vec3 ext_col = vec3(exp(-t * be.x), exp(-t * be.y), exp(-t * be.z));
//     vec3 ins_col = vec3(exp(-t * bi.x), exp(-t * bi.y), exp(-t * bi.z));

//     vec3 final_col = col * (1.0 - ext_col) + fog_color * ins_col;

//     return final_col;
// }


// vec3 fog(vec3 col, float t){
//     float fog_amount = 1 - exp(-t * u_fog_density);
//     vec3 fog_color   = vec3(0.5, 0.6, 0.7);

//     return mix(col, fog_color, fog_amount);
// }


void main() {
    vec3 terrain_material = vec3(0.2, 0.11, 0.03);
    
    vec3 normal     = normalize(v_normal);
    vec3 sun_dir    = normalize(vec3(-0.8, 0.4, -0.3));

    float sun_light      = clamp(dot(normal, sun_dir), 0.0, 1.0);
    float sky_light      = clamp(0.5 + 0.5 * normal.y, 0.0, 1.0);
    float indirect_light = clamp(dot(normal, normalize(sun_dir * vec3(-1.0, 0.0, -1.0))), 0.0, 1.0);

    vec3 view_dir    = normalize(u_camera_pos - v_world_pos);
    //vec3 reflectDir = reflect(-lightDir, normal);
    
    
    //float spec = pow(max(dot(view_dir, reflectDir), 0.0), 32.0);
    vec3 lighting = sun_light * vec3(1.64, 1.27, 0.99);
    lighting     += sky_light * vec3(0.16, 0.20, 0.28);
    lighting     += indirect_light * vec3(0.40, 0.28, 0.20);

    vec3 col      = terrain_material * lighting;

    //col = fog(col, length(u_camera_pos - v_world_pos)); // fog
    col = fog(col, length(u_camera_pos - v_world_pos), view_dir, sun_dir); // fog
    col = pow(col, vec3(1.0 / 2.2));                    // gamma correction

    FragColor = vec4(col, 1.0);
}