#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;
in vec2 TexCoord;

out vec4 out_Color;

uniform sampler2D outTexture;
uniform int useTexture; 
uniform int isLightSource; // 1이면 빛 계산 안 함 (전구 자체는 항상 밝게)
uniform vec3 viewPos;      // 카메라 위치 (반사광 계산용)

// 가로등 (Point Light) 구조체
struct PointLight {
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
};

// 성능을 위해 주변 4개의 가로등만 계산
#define NR_POINT_LIGHTS 4
uniform PointLight pointLights[NR_POINT_LIGHTS];

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objectColor);

void main()
{
    // 텍스처가 있으면 텍스처 색, 없으면 버텍스 색 사용
    vec3 objectColor;
    if (useTexture == 1) {
        objectColor = texture(outTexture, TexCoord).rgb;
    } else {
        objectColor = Color;
    }

    // 전구(Light Source) 자체는 빛 계산 없이 항상 밝게 표시
    if (isLightSource == 1) {
        out_Color = vec4(objectColor, 1.0);
        return;
    }

    // --- 조명 계산 시작 ---
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    // 1. 은은한 환경광 (Ambient) - 너무 어둡지 않게
    vec3 ambient = 0.1 * vec3(1.0, 1.0, 1.0) * objectColor;
    
    // 2. 가로등 빛 (Diffuse + Specular) 합산
    vec3 result = ambient;
    for(int i = 0; i < NR_POINT_LIGHTS; i++)
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir, objectColor);

    out_Color = vec4(result, 1.0);
}

// 개별 조명 계산 함수
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objectColor)
{
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse (확산광)
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular (반사광 - 반짝임)
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32); // 32는 반짝임 강도
    
    // 거리 감쇠 (멀어질수록 어두워짐)
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
    // 최종 합산
    vec3 diffuse = light.color * diff * objectColor;
    vec3 specular = vec3(0.5, 0.5, 0.5) * spec; // 반사광은 흰색 계열

    return (diffuse + specular) * attenuation;
}