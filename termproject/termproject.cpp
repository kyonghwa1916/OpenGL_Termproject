#define _CRT_SECURE_NO_WARNINGS
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>
#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --- 파일 읽기 ---
char* filetobuf(const char* file) {
    FILE* fptr = fopen(file, "rb");
    if (!fptr) return NULL;
    fseek(fptr, 0, SEEK_END);
    long length = ftell(fptr);
    char* buf = (char*)malloc(length + 1);
    fseek(fptr, 0, SEEK_SET);
    fread(buf, length, 1, fptr);
    fclose(fptr);
    buf[length] = 0;
    return buf;
}

// --- 전역 변수 ---
GLuint shaderProgramID;
GLuint vertexShader, fragmentShader;
GLuint bgVAO, bgVBO;
GLuint carVAO, carVBO;
GLuint lightVAO, lightVBO;

GLuint roadTextureID, dirtTextureID;

GLuint modelLoc, viewLoc, projLoc;
GLuint useTextureLoc, isLightSourceLoc, viewPosLoc; // 쉐이더 유니폼 위치

float carX = 0.0f;
float carZ = 0.0f;
float carAngle = 0.0f; // Y축 회전 각도 (라디안)

// 키 상태 추적
bool specialKeyStates[256] = { false };

// --- 행렬 헬퍼 ---
void setIdentityMatrix(float* mat, int size) {
    for (int i = 0; i < size * size; ++i) mat[i] = 0.0f;
    for (int i = 0; i < size; ++i) mat[i * size + i] = 1.0f;
}
void makePerspectiveMatrix(float* mat, float fov, float aspect, float nearDist, float farDist) {
    setIdentityMatrix(mat, 4);
    float tanHalfFov = tanf(fov / 2.0f);
    mat[0] = 1.0f / (aspect * tanHalfFov);
    mat[5] = 1.0f / tanHalfFov;
    mat[10] = -(farDist + nearDist) / (farDist - nearDist);
    mat[11] = -1.0f;
    mat[14] = -(2.0f * farDist * nearDist) / (farDist - nearDist);
    mat[15] = 0.0f;
}
void setTranslationMatrix(float* mat, float x, float y, float z) {
    setIdentityMatrix(mat, 4);
    mat[12] = x; mat[13] = y; mat[14] = z;
}
void setRotationYMatrix(float* mat, float angle) {
    setIdentityMatrix(mat, 4);
    float c = cosf(angle);
    float s = sinf(angle);
    mat[0] = c;  mat[2] = s;
    mat[8] = -s; mat[10] = c;
}
void multiplyMatrix(float* result, const float* a, const float* b) {
    float temp[16];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            temp[i * 4 + j] = 0.0f;
            for (int k = 0; k < 4; ++k) {
                temp[i * 4 + j] += a[i * 4 + k] * b[k * 4 + j];
            }
        }
    }
    for (int i = 0; i < 16; ++i) result[i] = temp[i];
}

// --- 텍스처 로드 ---
GLuint LoadTexture(const char* filename) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cerr << "Failed to load texture: " << filename << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

// --- 쉐이더 컴파일 ---
void make_Shaders() {
    GLchar* vSrc = filetobuf("vertex.glsl");
    GLchar* fSrc = filetobuf("fragment.glsl");
    if (!vSrc || !fSrc) { std::cerr << "Shader file not found!" << std::endl; exit(1); }
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vertexShader, 1, &vSrc, NULL);
    glShaderSource(fragmentShader, 1, &fSrc, NULL);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
    shaderProgramID = glCreateProgram();
    glAttachShader(shaderProgramID, vertexShader);
    glAttachShader(shaderProgramID, fragmentShader);
    glLinkProgram(shaderProgramID);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    free(vSrc); free(fSrc);
}

// --- 데이터 초기화 (XYZ RGB UV Normal) ---
// 정점 데이터 구조: 3(Pos) + 3(Color) + 2(UV) + 3(Normal) = 11 floats
void initBuffer() {
    float w = 2.0f; float sw = 1.5f; float l = 300.0f; float repeat = 30.0f;
    std::vector<float> v;

    // 사각형 추가 (바닥은 Normal이 항상 0, 1, 0)
    auto addRect = [&](float x1, float z1, float x2, float z2, float y, float uMax, float vMax) {
        float ny = 1.0f; // 법선: 위쪽 방향
        // Tri 1
        v.insert(v.end(), { x1, y, z1, 1,1,1, 0.0f, vMax, 0, ny, 0 });
        v.insert(v.end(), { x2, y, z1, 1,1,1, uMax, vMax, 0, ny, 0 });
        v.insert(v.end(), { x2, y, z2, 1,1,1, uMax, 0.0f, 0, ny, 0 });
        // Tri 2
        v.insert(v.end(), { x1, y, z1, 1,1,1, 0.0f, vMax, 0, ny, 0 });
        v.insert(v.end(), { x2, y, z2, 1,1,1, uMax, 0.0f, 0, ny, 0 });
        v.insert(v.end(), { x1, y, z2, 1,1,1, 0.0f, 0.0f, 0, ny, 0 });
        };

    addRect(-w, 10.0f, w, -l, -0.5f, 1.0f, repeat); // 도로
    addRect(-w - sw, 10.0f, -w, -l, -0.5f, 1.0f, repeat); // 왼쪽 인도
    addRect(w, 10.0f, w + sw, -l, -0.5f, 1.0f, repeat); // 오른쪽 인도

    glGenVertexArrays(1, &bgVAO);
    glGenBuffers(1, &bgVBO);
    glBindVertexArray(bgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bgVBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);

    int stride = 11 * sizeof(float);
    // Attribute 0: Pos, 1: Color, 2: TexCoord, 3: Normal
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
}

// 큐브 생성 함수 (법선 벡터 포함)
void initCubeObj(GLuint* vao, GLuint* vbo, bool isCar) {
    std::vector<float> v;

    // 원통(실린더) 생성 헬퍼 - X축 방향으로 뻗은 실린더
    auto addCylinder = [&](float x, float y, float z, float radius, float height, float r, float g, float b) {
        const int segments = 16; // 원의 분할 수
        const float PI = 3.141592f;

        // 옆면 (측면)
        for (int i = 0; i < segments; ++i) {
            float angle1 = (float)i / segments * 2.0f * PI;
            float angle2 = (float)(i + 1) / segments * 2.0f * PI;

            float y1 = cosf(angle1) * radius;
            float z1 = sinf(angle1) * radius;
            float y2 = cosf(angle2) * radius;
            float z2 = sinf(angle2) * radius;

            // 법선 벡터 (방사형)
            float ny1 = cosf(angle1);
            float nz1 = sinf(angle1);
            float ny2 = cosf(angle2);
            float nz2 = sinf(angle2);

            // 삼각형 1
            v.insert(v.end(), {x - height/2, y + y1, z + z1, r, g, b, 0, 0, 0, ny1, nz1});
            v.insert(v.end(), {x + height/2, y + y1, z + z1, r, g, b, 0, 0, 0, ny1, nz1});
            v.insert(v.end(), {x + height/2, y + y2, z + z2, r, g, b, 0, 0, 0, ny2, nz2});

            // 삼각형 2
            v.insert(v.end(), {x - height/2, y + y1, z + z1, r, g, b, 0, 0, 0, ny1, nz1});
            v.insert(v.end(), {x + height/2, y + y2, z + z2, r, g, b, 0, 0, 0, ny2, nz2});
            v.insert(v.end(), {x - height/2, y + y2, z + z2, r, g, b, 0, 0, 0, ny2, nz2});
        }

        // 왼쪽 원판 (법선: -1, 0, 0)
        for (int i = 0; i < segments; ++i) {
            float angle1 = (float)i / segments * 2.0f * PI;
            float angle2 = (float)(i + 1) / segments * 2.0f * PI;

            float y1 = cosf(angle1) * radius;
            float z1 = sinf(angle1) * radius;
            float y2 = cosf(angle2) * radius;
            float z2 = sinf(angle2) * radius;

            v.insert(v.end(), {x - height/2, y, z, r, g, b, 0, 0, -1, 0, 0});
            v.insert(v.end(), {x - height/2, y + y2, z + z2, r, g, b, 0, 0, -1, 0, 0});
            v.insert(v.end(), {x - height/2, y + y1, z + z1, r, g, b, 0, 0, -1, 0, 0});
        }

        // 오른쪽 원판 (법선: 1, 0, 0)
        for (int i = 0; i < segments; ++i) {
            float angle1 = (float)i / segments * 2.0f * PI;
            float angle2 = (float)(i + 1) / segments * 2.0f * PI;

            float y1 = cosf(angle1) * radius;
            float z1 = sinf(angle1) * radius;
            float y2 = cosf(angle2) * radius;
            float z2 = sinf(angle2) * radius;

            v.insert(v.end(), {x + height/2, y, z, r, g, b, 0, 0, 1, 0, 0});
            v.insert(v.end(), {x + height/2, y + y1, z + z1, r, g, b, 0, 0, 1, 0, 0});
            v.insert(v.end(), {x + height/2, y + y2, z + z2, r, g, b, 0, 0, 1, 0, 0});
        }
    };

    // 큐브 생성 헬퍼 (각 면마다 법선 벡터 다르게 설정)
    auto addFace = [&](float x, float y, float z, float sx, float sy, float sz, float r, float g, float b) {
        float dx = sx / 2, dy = sy / 2, dz = sz / 2;
        // 각 면의 법선 벡터 (Front, Back, Top, Bottom, Left, Right)
        float normals[6][3] = { {0,0,1}, {0,0,-1}, {0,1,0}, {0,-1,0}, {-1,0,0}, {1,0,0} };
        // 정점 위치 
        float pos[6][4][3] = {
            {{x - dx, y - dy, z + dz}, {x + dx, y - dy, z + dz}, {x + dx, y + dy, z + dz}, {x - dx, y + dy, z + dz}}, // Front
            {{x - dx, y - dy, z - dz}, {x + dx, y - dy, z - dz}, {x + dx, y + dy, z - dz}, {x - dx, y + dy, z - dz}}, // Back
            {{x - dx, y + dy, z + dz}, {x + dx, y + dy, z + dz}, {x + dx, y + dy, z - dz}, {x - dx, y + dy, z - dz}}, // Top
            {{x - dx, y - dy, z + dz}, {x + dx, y - dy, z + dz}, {x + dx, y - dy, z - dz}, {x - dx, y - dy, z - dz}}, // Bottom
            {{x - dx, y - dy, z - dz}, {x - dx, y - dy, z + dz}, {x - dx, y + dy, z + dz}, {x - dx, y + dy, z - dz}}, // Left
            {{x + dx, y - dy, z - dz}, {x + dx, y - dy, z + dz}, {x + dx, y + dy, z + dz}, {x + dx, y + dy, z - dz}}  // Right
        };

        int indices[] = { 0, 1, 2, 0, 2, 3 }; // 삼각형 2개 인덱스
        for (int i = 0; i < 6; ++i) { // 6면 루프
            for (int j = 0; j < 6; ++j) { // 정점 6개
                int idx = indices[j];
                v.push_back(pos[i][idx][0]); v.push_back(pos[i][idx][1]); v.push_back(pos[i][idx][2]); // Pos
                v.push_back(r); v.push_back(g); v.push_back(b); // Color
                v.push_back(0.0f); v.push_back(0.0f); // UV (사용 안 함)
                v.push_back(normals[i][0]); v.push_back(normals[i][1]); v.push_back(normals[i][2]); // Normal
            }
        }
        };

    if (isCar) {
        // 자동차 모델 - 차체(body) + 캐빈(cabin) + 바퀴(wheels)
        // Z 좌표를 반전시켜 자동차가 -Z 방향을 향하도록 수정

        // 1. 차체 하부 (Lower Body) - 빨간색
        addFace(0.0f, 0.0f, 0.0f, 0.8f, 0.25f, 1.2f, 0.8f, 0.1f, 0.1f);

        // 2. 캐빈 (Cabin/Roof) - 약간 어두운 빨간색, 뒤쪽에 배치
        addFace(0.0f, 0.25f, 0.15f, 0.6f, 0.3f, 0.6f, 0.6f, 0.05f, 0.05f);

        // 3. 본넷/후드 (Hood) - 앞쪽 경사
        addFace(0.0f, 0.05f, -0.5f, 0.6f, 0.15f, 0.4f, 0.9f, 0.15f, 0.15f);

        // 4. 바퀴 4개 - 검은색 원통 형태
        float wheelRadius = 0.15f;
        float wheelWidth = 0.12f;
        float wheelBaseX = 0.45f; // 차체 폭의 바깥쪽
        float wheelBaseZ = 0.4f;  // 앞뒤 간격

        // 왼쪽 앞 바퀴 (원통)
        addCylinder(-wheelBaseX, -0.125f + wheelRadius, -wheelBaseZ, wheelRadius, wheelWidth, 0.1f, 0.1f, 0.1f);
        // 오른쪽 앞 바퀴 (원통)
        addCylinder(wheelBaseX, -0.125f + wheelRadius, -wheelBaseZ, wheelRadius, wheelWidth, 0.1f, 0.1f, 0.1f);
        // 왼쪽 뒤 바퀴 (원통)
        addCylinder(-wheelBaseX, -0.125f + wheelRadius, wheelBaseZ, wheelRadius, wheelWidth, 0.1f, 0.1f, 0.1f);
        // 오른쪽 뒤 바퀴 (원통)
        addCylinder(wheelBaseX, -0.125f + wheelRadius, wheelBaseZ, wheelRadius, wheelWidth, 0.1f, 0.1f, 0.1f);

        // 5. 앞 유리창 (Windshield) - 하늘색 (유리 느낌)
        addFace(0.0f, 0.3f, -0.15f, 0.55f, 0.2f, 0.15f, 0.3f, 0.5f, 0.7f);

        // 6. 헤드라이트 2개 - 노란색
        addFace(-0.25f, 0.0f, -0.62f, 0.12f, 0.08f, 0.04f, 1.0f, 1.0f, 0.6f);
        addFace(0.25f, 0.0f, -0.62f, 0.12f, 0.08f, 0.04f, 1.0f, 1.0f, 0.6f);
    }
    else { // 가로등 모델
        addFace(0.0f, 1.5f, 0.0f, 0.2f, 3.0f, 0.2f, 0.5f, 0.5f, 0.5f); // 기둥
        addFace(0.6f, 2.9f, 0.0f, 1.2f, 0.15f, 0.15f, 0.5f, 0.5f, 0.5f); // 팔
        addFace(1.1f, 2.7f, 0.0f, 0.3f, 0.3f, 0.3f, 1.0f, 1.0f, 0.5f); // 전구
    }

    glGenVertexArrays(1, vao);
    glGenBuffers(1, vbo);
    glBindVertexArray(*vao);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);

    int stride = 11 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
}

// 키 상태에 따라 자동차 업데이트
void updateCar() {
    float speed = 0.3f;
    float rotSpeed = 0.02f; // 회전 속도 (라디안)

    // 자동차의 전방 벡터 계산 (자동차는 로컬 -Z 방향을 바라봄)
    float forwardX = sinf(carAngle);
    float forwardZ = -cosf(carAngle);

    // 전진/후진
    if (specialKeyStates[GLUT_KEY_UP]) {
        carX += speed * forwardX;
        carZ += speed * forwardZ;
    }
    if (specialKeyStates[GLUT_KEY_DOWN]) {
        carX -= speed * forwardX;
        carZ -= speed * forwardZ;
    }

    // 회전
    if (specialKeyStates[GLUT_KEY_LEFT]) {
        carAngle -= rotSpeed;
    }
    if (specialKeyStates[GLUT_KEY_RIGHT]) {
        carAngle += rotSpeed;
    }
}

GLvoid drawScene() {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f); // 밤하늘 배경
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgramID);

    // [중요] 카메라 위치 전송 (반사광 계산용)
    glUniform3f(viewPosLoc, carX, 1.5f, carZ + 8.0f);

    float view[16];
    setIdentityMatrix(view, 4);
    view[12] = -carX; view[13] = -1.5f; view[14] = -(carZ + 8.0f);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);
    float projection[16];
    makePerspectiveMatrix(projection, 3.141592f / 4.0f, 800.0f / 600.0f, 0.1f, 300.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

    // --- 동적 조명 시스템 ---
    // 가로등 간격은 20.0f입니다. 자동차(carZ) 위치를 기준으로 가장 가까운 가로등 인덱스를 찾습니다.
    int centerIdx = (int)(abs(carZ) / 20.0f);
    int lightCount = 0;

    char uniformName[64];
    // 차 주변 가로등 4개만 활성화 (앞뒤로)
    for (int i = centerIdx - 1; i <= centerIdx + 2; ++i) {
        if (lightCount >= 4) break;
        float lightZ = -(float)i * 20.0f;

        // 쉐이더의 pointLights[0], pointLights[1]... 에 값 넣기
        sprintf(uniformName, "pointLights[%d].position", lightCount);
        // 왼쪽 가로등의 전구 위치 (-2.5(인도) + 1.1(팔끝))
        glUniform3f(glGetUniformLocation(shaderProgramID, uniformName), -1.4f, 2.7f, lightZ);

        sprintf(uniformName, "pointLights[%d].color", lightCount);
        glUniform3f(glGetUniformLocation(shaderProgramID, uniformName), 1.0f, 0.9f, 0.6f); // 따뜻한 노란빛

        // 빛의 감쇠(Attenuation) 설정 - 거리에 따라 어두워짐
        sprintf(uniformName, "pointLights[%d].constant", lightCount);
        glUniform1f(glGetUniformLocation(shaderProgramID, uniformName), 1.0f);
        sprintf(uniformName, "pointLights[%d].linear", lightCount);
        glUniform1f(glGetUniformLocation(shaderProgramID, uniformName), 0.09f);
        sprintf(uniformName, "pointLights[%d].quadratic", lightCount);
        glUniform1f(glGetUniformLocation(shaderProgramID, uniformName), 0.032f);

        lightCount++;
    }

    float model[16];
    setIdentityMatrix(model, 4);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);

    // 1. 배경 그리기 (Texture ON, Light Source OFF)
    glUniform1i(useTextureLoc, 1);
    glUniform1i(isLightSourceLoc, 0); // 빛을 받아야 함

    glBindVertexArray(bgVAO);
    glBindTexture(GL_TEXTURE_2D, roadTextureID);
    glDrawArrays(GL_TRIANGLES, 0, 6); // 도로
    glBindTexture(GL_TEXTURE_2D, dirtTextureID);
    glDrawArrays(GL_TRIANGLES, 6, 12); // 인도

    // 2. 가로등 그리기
    glUniform1i(useTextureLoc, 0);
    glBindVertexArray(lightVAO);
    for (float z = 0.0f; z > -300.0f; z -= 20.0f) {
        // (1) 기둥과 팔 (빛 받음)
        glUniform1i(isLightSourceLoc, 0);

        // 왼쪽
        setTranslationMatrix(model, -2.5f, -0.5f, z);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glDrawArrays(GL_TRIANGLES, 0, 72); // 기둥+팔 (36*2)

        // 오른쪽 (대칭)
        setTranslationMatrix(model, 2.5f, -0.5f, z);
        model[0] = -1.0f; // X축 반전
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glDrawArrays(GL_TRIANGLES, 0, 72);

        // (2) 전구 (스스로 빛남 - 그림자 안 짐)
        glUniform1i(isLightSourceLoc, 1);

        // 왼쪽 전구
        setTranslationMatrix(model, -2.5f, -0.5f, z);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glDrawArrays(GL_TRIANGLES, 72, 36);

        // 오른쪽 전구
        setTranslationMatrix(model, 2.5f, -0.5f, z);
        model[0] = -1.0f;
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glDrawArrays(GL_TRIANGLES, 72, 36);
    }

    // 3. 자동차 (빛 받음)
    glUniform1i(isLightSourceLoc, 0);
    float rot[16];
    setRotationYMatrix(rot, carAngle);
    // 회전 행렬에 이동 포함 (해당 위치에서 회전)
    rot[12] = carX;
    rot[13] = -0.25f;
    rot[14] = carZ;
    for (int i = 0; i < 16; ++i) model[i] = rot[i];
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
    glBindVertexArray(carVAO);
    glDrawArrays(GL_TRIANGLES, 0, 984); // 차체 부품 + 원통 바퀴 4개

    glutSwapBuffers();
}

GLvoid Reshape(int w, int h) { glViewport(0, 0, w, h); }
void Keyboard(unsigned char key, int x, int y) {
    if (key == 'q' || key == 'Q' || key == 27) exit(0);
}

// 특수 키 눌림 처리
void SpecialKeyboard(int key, int x, int y) {
    specialKeyStates[key] = true;
}

// 특수 키 떼어짐 처리
void SpecialKeyboardUp(int key, int x, int y) {
    specialKeyStates[key] = false;
}

// 타이머 함수 - 일정한 간격으로 호출
void Timer(int value) {
    updateCar();
    glutPostRedisplay();
    glutTimerFunc(16, Timer, 0); // 약 60fps (16ms)
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Final Project - Street Lights");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) exit(EXIT_FAILURE);
    glEnable(GL_DEPTH_TEST);

    make_Shaders();

    modelLoc = glGetUniformLocation(shaderProgramID, "model");
    viewLoc = glGetUniformLocation(shaderProgramID, "view");
    projLoc = glGetUniformLocation(shaderProgramID, "projection");
    useTextureLoc = glGetUniformLocation(shaderProgramID, "useTexture");
    isLightSourceLoc = glGetUniformLocation(shaderProgramID, "isLightSource");
    viewPosLoc = glGetUniformLocation(shaderProgramID, "viewPos");

    roadTextureID = LoadTexture("road.png");
    dirtTextureID = LoadTexture("dirt.png");

    initBuffer();
    initCubeObj(&lightVAO, &lightVBO, false); // 가로등
    initCubeObj(&carVAO, &carVBO, true);      // 차

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Keyboard);
    glutSpecialFunc(SpecialKeyboard);
    glutSpecialUpFunc(SpecialKeyboardUp);
    glutTimerFunc(16, Timer, 0); // 타이머 시작

    glutMainLoop();
    return 0;
}