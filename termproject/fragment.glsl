#version 330 core

in vec3 FragColor;
in vec2 TexCoord;

out vec4 out_Color;

uniform sampler2D outTexture; // 텍스처 데이터
uniform int useTexture;       // 텍스처 사용 여부 (1: 사용, 0: 사용안함-색상만)

void main()
{
    if (useTexture == 1) {
        // 텍스처 색상 가져오기
        out_Color = texture(outTexture, TexCoord);
    } else {
        // 텍스처 없이 색상만 사용 (자동차, 가로등 등)
        out_Color = vec4(FragColor, 1.0);
    }
}