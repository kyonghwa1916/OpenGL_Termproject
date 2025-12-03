#version 330 core

layout (location = 0) in vec3 vPos;   // 위치
layout (location = 1) in vec3 vColor; // 색상
layout (location = 2) in vec2 vTexCoord; // 텍스처 좌표 (UV)

out vec3 FragColor;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(vPos, 1.0);
    FragColor = vColor;
    TexCoord = vTexCoord;
}