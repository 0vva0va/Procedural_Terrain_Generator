#version 450 core


in  vec4  gFragPos;


uniform vec3  _LightPos;
uniform float _FarPlane;


void main()
{
    float dist = length(gFragPos.xyz - _LightPos);

    gl_FragDepth = dist / _FarPlane;
}
