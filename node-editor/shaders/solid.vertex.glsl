#version 330 core

layout (location = 0) in vec2 Position;

uniform mat4 mvp;

void main()
{
    vec4 position = mvp * vec4(Position, 0.f, 1.f);
    gl_Position = position;
}