#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 _Projection;
uniform mat4 _View;

out vec3 TexCoords;


void main()
{
    TexCoords   = aPos;
    vec4 pos    = _Projection * _View * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
} 