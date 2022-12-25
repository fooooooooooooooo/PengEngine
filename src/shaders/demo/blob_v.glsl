#version 330 core

layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_col;

out vec4 vertex_color;

uniform vec4 color = vec4(1, 1, 1, 1);
uniform vec3 pos = vec3(0, 0, 0);
uniform vec3 scale = vec3(1, 1, 1);

void main()
{
    gl_Position = vec4(a_pos * scale + pos, 1.0);
    vertex_color = vec4(a_col, 1) * color;
}