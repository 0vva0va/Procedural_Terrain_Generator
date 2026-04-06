#version 450 core

layout (location = 0) in vec3 aPos;

uniform mat4 _Projection;
uniform mat4 _View;


out vec3 worldPos;


void main()
{
    worldPos = aPos;  
    gl_Position =  _Projection * _View * vec4(worldPos, 1.0);
}