#version 450 core

in vec3 worldPos;

uniform samplerCube environmentMap;

out vec4 FragColor;

const float PI = 3.14159265359;

void main()
{
    vec3 N = normalize(worldPos);

    // Build a TBN frame around N
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));

    vec3 irradiance = vec3(0.0);
    float sampleDelta = 0.025; // lower = more samples, slower
    float nrSamples = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            // Spherical to cartesian (tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi),
                                     sin(theta) * sin(phi),
                                     cos(theta));
            // Tangent space to world space
            vec3 sampleVec = tangentSample.x * right
                           + tangentSample.y * up
                           + tangentSample.z * N;

            irradiance += texture(environmentMap, sampleVec).rgb
                        * cos(theta) * sin(theta);
            nrSamples++;
        }
    }

    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    FragColor = vec4(irradiance, 1.0);
}