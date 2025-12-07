#define _CRT_SECURE_NO_WARNINGS
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>
#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>

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

// --- 게임 상태 및 전역 변수 ---
enum GameState { MENU, PLAY, GAMEOVER };
GameState currentState = MENU;
int selectedMap = 1; // 1 or 2

GLuint shaderProgramID;
GLuint vertexShader, fragmentShader;
GLuint bgVAO, bgVBO;
GLuint carVAO, carVBO;
GLuint lightVAO, lightVBO;

GLuint roadTextureID, dirtTextureID;

GLuint modelLoc, viewLoc, projLoc;
GLuint useTextureLoc, isLightSourceLoc, viewPosLoc;

// 자동차 상태
float carX = 0.0f;
float carZ = 0.0f;
float carAngle = 0.0f;

// 도로 설정
const float ROAD_WIDTH = 4.0f;       // 도로 전체 폭
const float SIDEWALK_WIDTH = 1.5f;   // 인도 폭
const float CAR_COLLISION_RADIUS = 0.5f; // 자동차 충돌 반경
const float TRACK_RADIUS = 80.0f; // 트랙의 반지름 (크기)
const int TRACK_SEGMENTS = 360;   // 원을 몇 개로 쪼갤지    
int vertexCountRoad = 0;
int vertexCountSidewalk = 0;

// 키 상태 추적
bool specialKeyStates[256] = { false };

// --- 수학 헬퍼 함수 ---
// Z 위치에 따른 도로의 중심 X 좌표를 반환 (곡선 도로 핵심 로직)
float getRoadCenterX(float z, int mapType) {
    if (mapType == 1) {
        // Map 1: 완만한 Sine 파형
        return sinf(z * 0.05f) * 10.0f;
    }
    else {
        // Map 2: 더 복잡하고 급격한 곡선
        return sinf(z * 0.1f) * 10.0f + cosf(z * 0.05f) * 5.0f;
    }
}

// 도로의 접선 각도 계산 (가로등 회전 등에 사용)
float getRoadAngle(float z, int mapType) {
    float delta = 0.1f;
    float x1 = getRoadCenterX(z, mapType);
    float x2 = getRoadCenterX(z - delta, mapType);
    return atan2f(x2 - x1, -delta); // -Z 방향이 진행 방향
}

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

// 텍스트 출력 함수
void drawString(const char* str, int x, int y) {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glUseProgram(0); // 고정 파이프라인 사용

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2i(x, y);
    for (int i = 0; str[i] != '\0'; i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, str[i]);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);
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
        std::cout << "Texture Load Failed (Use Default Color): " << filename << std::endl;
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

// --- 맵 생성 ---
void initMapBuffer(int mapType) {
    std::vector<float> v;
    float step = 2.0f;
    float startZ = 20.0f;

    // 도로 길이 -20000으로 변경
    float endZ = -20000.0f;

    float halfW = ROAD_WIDTH / 2.0f;

    // 높이 설정
    float roadY = -0.5f;
    float walkY = -0.3f;

    for (float z = startZ; z > endZ; z -= step) {
        float zNext = z - step;

        float cxCurrent = getRoadCenterX(z, mapType);
        float cxNext = getRoadCenterX(zNext, mapType);
        float ny = 1.0f;

        float v1 = -z * 0.1f;
        float v2 = -zNext * 0.1f;

        // --- [1] 도로 (Road) ---
        // Quad 1
        v.insert(v.end(), { cxCurrent - halfW, roadY, z,      1,1,1,  0.0f, v1,   0, ny, 0 });
        v.insert(v.end(), { cxCurrent + halfW, roadY, z,      1,1,1,  1.0f, v1,   0, ny, 0 });
        v.insert(v.end(), { cxNext + halfW,    roadY, zNext,  1,1,1,  1.0f, v2,   0, ny, 0 });
        // Quad 2
        v.insert(v.end(), { cxCurrent - halfW, roadY, z,      1,1,1,  0.0f, v1,   0, ny, 0 });
        v.insert(v.end(), { cxNext + halfW,    roadY, zNext,  1,1,1,  1.0f, v2,   0, ny, 0 });
        v.insert(v.end(), { cxNext - halfW,    roadY, zNext,  1,1,1,  0.0f, v2,   0, ny, 0 });
    }

    // 도로 정점 개수 저장
    vertexCountRoad = v.size() / 11;

    for (float z = startZ; z > endZ; z -= step) {
        float zNext = z - step;
        float cxCurrent = getRoadCenterX(z, mapType);
        float cxNext = getRoadCenterX(zNext, mapType);
        float ny = 1.0f;
        float v1 = -z * 0.1f;
        float v2 = -zNext * 0.1f;

        // --- [2] 왼쪽 인도 (Sidewalk Left) ---
        // Y값을 walkY(-0.3f)로 올림
        v.insert(v.end(), { cxCurrent - halfW - SIDEWALK_WIDTH, walkY, z,      1,1,1,  0.0f, v1,   0, ny, 0 });
        v.insert(v.end(), { cxCurrent - halfW,                  walkY, z,      1,1,1,  1.0f, v1,   0, ny, 0 });
        v.insert(v.end(), { cxNext - halfW,                     walkY, zNext,  1,1,1,  1.0f, v2,   0, ny, 0 });
        v.insert(v.end(), { cxCurrent - halfW - SIDEWALK_WIDTH, walkY, z,      1,1,1,  0.0f, v1,   0, ny, 0 });
        v.insert(v.end(), { cxNext - halfW,                     walkY, zNext,  1,1,1,  1.0f, v2,   0, ny, 0 });
        v.insert(v.end(), { cxNext - halfW - SIDEWALK_WIDTH,    walkY, zNext,  1,1,1,  0.0f, v2,   0, ny, 0 });

        // --- [3] 오른쪽 인도 (Sidewalk Right) ---
        v.insert(v.end(), { cxCurrent + halfW,                  walkY, z,      1,1,1,  0.0f, v1,   0, ny, 0 });
        v.insert(v.end(), { cxCurrent + halfW + SIDEWALK_WIDTH, walkY, z,      1,1,1,  1.0f, v1,   0, ny, 0 });
        v.insert(v.end(), { cxNext + halfW + SIDEWALK_WIDTH,    walkY, zNext,  1,1,1,  1.0f, v2,   0, ny, 0 });
        v.insert(v.end(), { cxCurrent + halfW,                  walkY, z,      1,1,1,  0.0f, v1,   0, ny, 0 });
        v.insert(v.end(), { cxNext + halfW + SIDEWALK_WIDTH,    walkY, zNext,  1,1,1,  1.0f, v2,   0, ny, 0 });
        v.insert(v.end(), { cxNext + halfW,                     walkY, zNext,  1,1,1,  0.0f, v2,   0, ny, 0 });

        // --- [4] 연석 (Curb) - 도로와 인도 사이 옆면 ---
        // 왼쪽 턱 옆면
        v.insert(v.end(), { cxCurrent - halfW, roadY, z,      0.5f,0.5f,0.5f,  0.0f, 0.0f,   1, 0, 0 });
        v.insert(v.end(), { cxCurrent - halfW, walkY, z,      0.5f,0.5f,0.5f,  0.0f, 0.0f,   1, 0, 0 });
        v.insert(v.end(), { cxNext - halfW,    walkY, zNext,  0.5f,0.5f,0.5f,  0.0f, 0.0f,   1, 0, 0 });
        v.insert(v.end(), { cxCurrent - halfW, roadY, z,      0.5f,0.5f,0.5f,  0.0f, 0.0f,   1, 0, 0 });
        v.insert(v.end(), { cxNext - halfW,    walkY, zNext,  0.5f,0.5f,0.5f,  0.0f, 0.0f,   1, 0, 0 });
        v.insert(v.end(), { cxNext - halfW,    roadY, zNext,  0.5f,0.5f,0.5f,  0.0f, 0.0f,   1, 0, 0 });

        // 오른쪽 턱 옆면
        v.insert(v.end(), { cxCurrent + halfW, roadY, z,      0.5f,0.5f,0.5f,  0.0f, 0.0f,  -1, 0, 0 });
        v.insert(v.end(), { cxCurrent + halfW, walkY, z,      0.5f,0.5f,0.5f,  0.0f, 0.0f,  -1, 0, 0 });
        v.insert(v.end(), { cxNext + halfW,    walkY, zNext,  0.5f,0.5f,0.5f,  0.0f, 0.0f,  -1, 0, 0 });
        v.insert(v.end(), { cxCurrent + halfW, roadY, z,      0.5f,0.5f,0.5f,  0.0f, 0.0f,  -1, 0, 0 });
        v.insert(v.end(), { cxNext + halfW,    walkY, zNext,  0.5f,0.5f,0.5f,  0.0f, 0.0f,  -1, 0, 0 });
        v.insert(v.end(), { cxNext + halfW,    roadY, zNext,  0.5f,0.5f,0.5f,  0.0f, 0.0f,  -1, 0, 0 });
    }

    // 전체 정점 개수에서 도로 정점 개수를 뺀 것이 나머지(인도+턱) 개수
    vertexCountSidewalk = (v.size() / 11) - vertexCountRoad;

    if (bgVAO == 0) glGenVertexArrays(1, &bgVAO);
    if (bgVBO == 0) glGenBuffers(1, &bgVBO);

    glBindVertexArray(bgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bgVBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);

    int stride = 11 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float))); glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float))); glEnableVertexAttribArray(3);
}

// 큐브/오브젝트 생성 함수
void initCubeObj(GLuint* vao, GLuint* vbo, bool isCar) {
    std::vector<float> v;

    // 원통 헬퍼
    auto addCylinder = [&](float x, float y, float z, float radius, float height, float r, float g, float b) {
        const int segments = 16; const float PI = 3.141592f;
        for (int i = 0; i < segments; ++i) {
            float angle1 = (float)i / segments * 2.0f * PI;
            float angle2 = (float)(i + 1) / segments * 2.0f * PI;
            float y1 = cosf(angle1) * radius; float z1 = sinf(angle1) * radius;
            float y2 = cosf(angle2) * radius; float z2 = sinf(angle2) * radius;
            float ny1 = cosf(angle1); float nz1 = sinf(angle1);
            float ny2 = cosf(angle2); float nz2 = sinf(angle2);

            v.insert(v.end(), { x - height / 2, y + y1, z + z1, r, g, b, 0, 0, 0, ny1, nz1 });
            v.insert(v.end(), { x + height / 2, y + y1, z + z1, r, g, b, 0, 0, 0, ny1, nz1 });
            v.insert(v.end(), { x + height / 2, y + y2, z + z2, r, g, b, 0, 0, 0, ny2, nz2 });
            v.insert(v.end(), { x - height / 2, y + y1, z + z1, r, g, b, 0, 0, 0, ny1, nz1 });
            v.insert(v.end(), { x + height / 2, y + y2, z + z2, r, g, b, 0, 0, 0, ny2, nz2 });
            v.insert(v.end(), { x - height / 2, y + y2, z + z2, r, g, b, 0, 0, 0, ny2, nz2 });
        }
        };

    // 면 생성 헬퍼
    auto addFace = [&](float x, float y, float z, float sx, float sy, float sz, float r, float g, float b) {
        float dx = sx / 2, dy = sy / 2, dz = sz / 2;
        float normals[6][3] = { {0,0,1}, {0,0,-1}, {0,1,0}, {0,-1,0}, {-1,0,0}, {1,0,0} };
        float pos[6][4][3] = {
            {{x - dx, y - dy, z + dz}, {x + dx, y - dy, z + dz}, {x + dx, y + dy, z + dz}, {x - dx, y + dy, z + dz}},
            {{x - dx, y - dy, z - dz}, {x + dx, y - dy, z - dz}, {x + dx, y + dy, z - dz}, {x - dx, y + dy, z - dz}},
            {{x - dx, y + dy, z + dz}, {x + dx, y + dy, z + dz}, {x + dx, y + dy, z - dz}, {x - dx, y + dy, z - dz}},
            {{x - dx, y - dy, z + dz}, {x + dx, y - dy, z + dz}, {x + dx, y - dy, z - dz}, {x - dx, y - dy, z - dz}},
            {{x - dx, y - dy, z - dz}, {x - dx, y - dy, z + dz}, {x - dx, y + dy, z + dz}, {x - dx, y + dy, z - dz}},
            {{x + dx, y - dy, z - dz}, {x + dx, y - dy, z + dz}, {x + dx, y + dy, z + dz}, {x + dx, y + dy, z - dz}}
        };
        int indices[] = { 0, 1, 2, 0, 2, 3 };
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                int idx = indices[j];
                v.push_back(pos[i][idx][0]); v.push_back(pos[i][idx][1]); v.push_back(pos[i][idx][2]);
                v.push_back(r); v.push_back(g); v.push_back(b);
                v.push_back(0.0f); v.push_back(0.0f);
                v.push_back(normals[i][0]); v.push_back(normals[i][1]); v.push_back(normals[i][2]);
            }
        }
        };

    if (isCar) {
        addFace(0.0f, 0.0f, 0.0f, 0.8f, 0.25f, 1.2f, 0.8f, 0.1f, 0.1f);
        addFace(0.0f, 0.25f, 0.15f, 0.6f, 0.3f, 0.6f, 0.6f, 0.05f, 0.05f);
        addFace(0.0f, 0.05f, -0.5f, 0.6f, 0.15f, 0.4f, 0.9f, 0.15f, 0.15f);
        float wR = 0.15f, wW = 0.12f, wX = 0.45f, wZ = 0.4f;
        addCylinder(-wX, -0.125f + wR, -wZ, wR, wW, 0.1f, 0.1f, 0.1f);
        addCylinder(wX, -0.125f + wR, -wZ, wR, wW, 0.1f, 0.1f, 0.1f);
        addCylinder(-wX, -0.125f + wR, wZ, wR, wW, 0.1f, 0.1f, 0.1f);
        addCylinder(wX, -0.125f + wR, wZ, wR, wW, 0.1f, 0.1f, 0.1f);
        addFace(0.0f, 0.3f, -0.15f, 0.55f, 0.2f, 0.15f, 0.3f, 0.5f, 0.7f);
        addFace(-0.25f, 0.0f, -0.62f, 0.12f, 0.08f, 0.04f, 1.0f, 1.0f, 0.6f);
        addFace(0.25f, 0.0f, -0.62f, 0.12f, 0.08f, 0.04f, 1.0f, 1.0f, 0.6f);
    }
    else { // 가로등
        addFace(0.0f, 1.5f, 0.0f, 0.2f, 3.0f, 0.2f, 0.5f, 0.5f, 0.5f);
        addFace(0.6f, 2.9f, 0.0f, 1.2f, 0.15f, 0.15f, 0.5f, 0.5f, 0.5f);
        addFace(1.1f, 2.7f, 0.0f, 0.3f, 0.3f, 0.3f, 1.0f, 1.0f, 0.5f);
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

// --- 게임 초기화 ---
void initGame(int map) {
    selectedMap = map;
    carX = getRoadCenterX(0.0f, map); // 도로 중앙에서 시작
    carZ = 0.0f;
    carAngle = 0.0f;
    initMapBuffer(map);
    currentState = PLAY;
}

// 키 상태에 따라 자동차 업데이트 및 충돌 체크
void updateCar() {
    if (currentState != PLAY) return;

    float speed = 0.3f;
    float rotSpeed = 0.02f;

    float forwardX = sinf(carAngle);
    float forwardZ = -cosf(carAngle);

    if (specialKeyStates[GLUT_KEY_UP]) {
        carX += speed * forwardX;
        carZ += speed * forwardZ;
    }
    if (specialKeyStates[GLUT_KEY_DOWN]) {
        carX -= speed * forwardX;
        carZ -= speed * forwardZ;
    }
    if (specialKeyStates[GLUT_KEY_LEFT]) {
        carAngle -= rotSpeed;
    }
    if (specialKeyStates[GLUT_KEY_RIGHT]) {
        carAngle += rotSpeed;
    }

    // --- 충돌 체크 (Collision Detection) ---
    float roadCenter = getRoadCenterX(carZ, selectedMap);
    float limit = (ROAD_WIDTH / 2.0f) - CAR_COLLISION_RADIUS;

    // 도로 중심과의 거리 계산
    if (abs(carX - roadCenter) > limit) {
        // 도로를 벗어남 -> 인도 충돌
        currentState = GAMEOVER;
    }
}

GLvoid drawScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (currentState == MENU) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        drawString("=== Select Map ===", 320, 350);
        drawString("Press '1' for Map 1 (Gentle Curve)", 250, 300);
        drawString("Press '2' for Map 2 (Complex Curve)", 250, 270);
        glutSwapBuffers();
        return;
    }
    else if (currentState == GAMEOVER) {
        glClearColor(0.3f, 0.0f, 0.0f, 1.0f);
    }
    else {
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    }

    glUseProgram(shaderProgramID);

    // --- [1] 카메라 설정 ---
    // 자동차 뒤쪽에서 바라보는 좌표 계산
    float camDist = 10.0f;
    float camHeight = 5.0f;
    float eyeX = carX - camDist * sinf(carAngle);
    float eyeZ = carZ - camDist * (-cosf(carAngle)); 

    float eyeY = camHeight;
    float targetX = carX;
    float targetY = 0.0f;
    float targetZ = carZ;

    glUniform3f(viewPosLoc, eyeX, eyeY, eyeZ);

    // gluLookAt을 사용해 View Matrix 생성 후 Shader로 전송
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    gluLookAt(eyeX, eyeY, eyeZ, targetX, targetY, targetZ, 0, 1, 0);

    float view[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, view); // 계산된 행렬 가져오기
    glPopMatrix(); // 스택 복구

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);

    // Projection Matrix
    float projection[16];
    makePerspectiveMatrix(projection, 3.141592f / 4.0f, 800.0f / 600.0f, 0.1f, 300.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

    // --- [조명 설정] ---
    int centerIdx = (int)(abs(carZ) / 20.0f);
    int lightCount = 0;
    char uniformName[64];
    for (int i = centerIdx - 1; i <= centerIdx + 2; ++i) {
        if (lightCount >= 4) break;
        float lightZ = -(float)i * 20.0f;
        float lightX = getRoadCenterX(lightZ, selectedMap) - 2.5f + 1.1f;
        sprintf(uniformName, "pointLights[%d].position", lightCount);
        glUniform3f(glGetUniformLocation(shaderProgramID, uniformName), lightX, 2.7f, lightZ);
        sprintf(uniformName, "pointLights[%d].color", lightCount);
        glUniform3f(glGetUniformLocation(shaderProgramID, uniformName), 1.0f, 0.9f, 0.6f);
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

    // --- [2] 배경 그리기 ---
    glUniform1i(useTextureLoc, 1);
    glUniform1i(isLightSourceLoc, 0);

    glBindVertexArray(bgVAO);

    // 1) 도로 그리기 
    glBindTexture(GL_TEXTURE_2D, roadTextureID);
    glDrawArrays(GL_TRIANGLES, 0, vertexCountRoad);

    // 2) 인도 그리기
    glBindTexture(GL_TEXTURE_2D, dirtTextureID);
    glDrawArrays(GL_TRIANGLES, vertexCountRoad, vertexCountSidewalk);

    // --- [3] 가로등 ---
    glUniform1i(useTextureLoc, 0);
    glBindVertexArray(lightVAO);
    for (float z = 20.0f; z > -20000.0f; z -= 20.0f) {

        float cx = getRoadCenterX(z, selectedMap);
        float angle = getRoadAngle(z, selectedMap);
        float tx = cx - (ROAD_WIDTH / 2.0f) - 0.5f;

        // 기둥
        glUniform1i(isLightSourceLoc, 0);
        setTranslationMatrix(model, tx, -0.5f, z);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glDrawArrays(GL_TRIANGLES, 0, 72);

        // 전구
        glUniform1i(isLightSourceLoc, 1);
        setTranslationMatrix(model, tx, -0.5f, z);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glDrawArrays(GL_TRIANGLES, 72, 36);
    }

    // --- [4] 자동차 (기존 유지) ---
    glUniform1i(isLightSourceLoc, 0);
    float rot[16];
    setRotationYMatrix(rot, carAngle);
    rot[12] = carX; rot[13] = -0.25f; rot[14] = carZ;
    for (int i = 0; i < 16; ++i) model[i] = rot[i];
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
    glBindVertexArray(carVAO);
    glDrawArrays(GL_TRIANGLES, 0, 984);

    if (currentState == GAMEOVER) {
        drawString("GAME OVER", 350, 300);
        drawString("Press 'R' to Restart", 320, 270);
    }

    glutSwapBuffers();
}

GLvoid Reshape(int w, int h) { glViewport(0, 0, w, h); }
void Keyboard(unsigned char key, int x, int y) {
    if (key == 'q' || key == 'Q' || key == 27) exit(0);

    if (currentState == MENU) {
        if (key == '1') initGame(1);
        if (key == '2') initGame(2);
    }
    else if (currentState == GAMEOVER) {
        if (key == 'r' || key == 'R') {
            currentState = MENU;
        }
    }
}

void SpecialKeyboard(int key, int x, int y) { specialKeyStates[key] = true; }
void SpecialKeyboardUp(int key, int x, int y) { specialKeyStates[key] = false; }

void Timer(int value) {
    updateCar();
    glutPostRedisplay();
    glutTimerFunc(16, Timer, 0);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Curved Road Racing");

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

    // 기본 버퍼 초기화 (메뉴 화면용 더미 데이터 혹은 초기값)
    initCubeObj(&lightVAO, &lightVBO, false);
    initCubeObj(&carVAO, &carVBO, true);

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Keyboard);
    glutSpecialFunc(SpecialKeyboard);
    glutSpecialUpFunc(SpecialKeyboardUp);
    glutTimerFunc(16, Timer, 0);

    glutMainLoop();
    return 0;
}