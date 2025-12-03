#version 330 core

layout (location = 0) in vec3 vPos;       // 위치
layout (location = 1) in vec3 vColor;     // 색상
layout (location = 2) in vec2 vTexCoord;  // 텍스처 좌표
layout (location = 3) in vec3 vNormal;    // [NEW] 법선 벡터 (빛 계산용)

out vec3 FragPos;   // 프래그먼트의 월드 좌표
out vec3 Normal;    // 법선 벡터
out vec3 Color;     // 색상
out vec2 TexCoord;  // 텍스처 좌표

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 월드 좌표 계산 (조명 계산은 월드 좌표계에서 수행)
    FragPos = vec3(model * vec4(vPos, 1.0));
    
    // 법선 벡터 변환 (비균등 스케일링 대응을 위해 Normal Matrix 사용)
    Normal = mat3(transpose(inverse(model))) * vNormal;
    
    Color = vColor;
    TexCoord = vTexCoord;
    
    gl_Position = projection * view * vec4(FragPos, 1.0);
}