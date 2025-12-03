#define _CRT_SECURE_NO_WARNINGS
#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>
#include <iostream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// [중요] stb_image 라이브러리 설정
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --- 파일 읽기 함수 ---
char* filetobuf(const char* file)
{
    FILE* fptr;
    long length;
    char* buf;
    fptr = fopen(file, "rb");
    if (!fptr) return NULL;
    fseek(fptr, 0, SEEK_END);
    length = ftell(fptr);
    buf = (char*)malloc(length + 1);
    fseek(fptr, 0, SEEK_SET);
    fread(buf, length, 1, fptr);
    fclose(fptr);
    buf[length] = 0;
    return buf;
}

// --- 전역 변수 ---
GLuint shaderProgramID;
GLuint vertexShader, fragmentShader;
GLuint bgVAO, bgVBO;       // 배경
GLuint carVAO, carVBO;     // 자동차
GLuint lightVAO, lightVBO; // 가로등

// 텍스처 ID 저장 변수
GLuint roadTextureID;
GLuint dirtTextureID;

GLuint modelLoc, viewLoc, projLoc;
GLuint useTextureLoc; // 쉐이더에게 텍스처 쓸지 말지 알려주는 변수

float carX = 0.0f;
float carZ = 0.0f;

// --- 수학 헬퍼 함수 ---
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
    mat[12] = x;
    mat[13] = y;
    mat[14] = z;
}

// --- 텍스처 로드 함수 ---
GLuint LoadTexture(const char* filename) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // 텍스처 반복 설정 (이미지가 작아도 계속 반복되게)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // 텍스처 필터링 (깨짐 방지)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    // 이미지 로드 (Y축 반전 처리 포함)
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filename, &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "Texture Loaded: " << filename << std::endl;
    }
    else {
        std::cerr << "Failed to load texture: " << filename << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

// --- 셰이더 생성 ---
void make_vertexShaders() {
    GLchar* vertexSource = filetobuf("vertex.glsl");
    if (!vertexSource) exit(EXIT_FAILURE);
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
    free(vertexSource);
}

void make_fragmentShaders() {
    GLchar* fragmentSource = filetobuf("fragment.glsl");
    if (!fragmentSource) exit(EXIT_FAILURE);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
    free(fragmentSource);
}

GLuint make_shaderProgram() {
    GLuint shaderID = glCreateProgram();
    glAttachShader(shaderID, vertexShader);
    glAttachShader(shaderID, fragmentShader);
    glLinkProgram(shaderID);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glUseProgram(shaderID);
    return shaderID;
}

// --- 1. 배경 (텍스처 좌표 포함) ---
void initBuffer() {
    float w = 2.0f;
    float sw = 1.5f;
    float l = 300.0f;

    // 텍스처 반복 횟수 (길이가 300이니 30번 반복해서 선명하게)
    float repeat = 30.0f;

    // Vertex 구조: x, y, z (위치) | r, g, b (색상-무시됨) | u, v (텍스처좌표)
    // 텍스처 좌표(UV): (0,0) ~ (1, repeat)

    std::vector<float> v;

    // 사각형 추가 헬퍼 (텍스처 좌표 포함)
    auto addRectUV = [&](float x1, float z1, float x2, float z2, float y, float uMax, float vMax) {
        // Triangle 1
        v.insert(v.end(), { x1, y, z1, 0,0,0,  0.0f, vMax });    // 좌상단
        v.insert(v.end(), { x2, y, z1, 0,0,0,  uMax, vMax });    // 우상단
        v.insert(v.end(), { x2, y, z2, 0,0,0,  uMax, 0.0f });    // 우하단
        // Triangle 2
        v.insert(v.end(), { x1, y, z1, 0,0,0,  0.0f, vMax });    // 좌상단
        v.insert(v.end(), { x2, y, z2, 0,0,0,  uMax, 0.0f });    // 우하단
        v.insert(v.end(), { x1, y, z2, 0,0,0,  0.0f, 0.0f });    // 좌하단
        };

    // 1. 도로 (아스팔트 텍스처)
    // -2.0 ~ 2.0 (폭 4.0), 길이 300
    // 아스팔트는 가로로 1번, 세로로 repeat번 반복
    addRectUV(-w, 10.0f, w, -l, -0.5f, 1.0f, repeat);

    // 2. 왼쪽 인도 (흙 텍스처)
    addRectUV(-w - sw, 10.0f, -w, -l, -0.5f, 1.0f, repeat);

    // 3. 오른쪽 인도 (흙 텍스처)
    addRectUV(w, 10.0f, w + sw, -l, -0.5f, 1.0f, repeat);

    // *중앙선은 텍스처 대신 그냥 색상으로 그릴 것이므로 여기 넣지 않고 따로 빼거나,
    // 간단하게 도로 텍스처 위에 얇은 폴리곤을 띄워서 그림 (기존 방식 유지)

    glGenVertexArrays(1, &bgVAO);
    glGenBuffers(1, &bgVBO);
    glBindVertexArray(bgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bgVBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);

    // Stride가 8 * sizeof(float)로 변경됨 (XYZ RGB UV)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
}

// --- 2. 가로등 (Color만 사용, UV는 0) ---
void initStreetLightBuffer() {
    std::vector<float> v;
    auto addCube = [&](float x, float y, float z, float sx, float sy, float sz, float r, float g, float b) {
        float dx = sx / 2, dy = sy / 2, dz = sz / 2;
        // 8개의 데이터 (XYZ RGB UV) -> UV는 (0,0)으로 더미값 넣음
        float faces[] = {
            // Front
            x - dx, y - dy, z + dz, r,g,b, 0,0,  x + dx, y - dy, z + dz, r,g,b, 0,0,  x + dx, y + dy, z + dz, r,g,b, 0,0,
            x - dx, y - dy, z + dz, r,g,b, 0,0,  x + dx, y + dy, z + dz, r,g,b, 0,0,  x - dx, y + dy, z + dz, r,g,b, 0,0,
            // Back
            x - dx, y - dy, z - dz, r,g,b, 0,0,  x + dx, y - dy, z - dz, r,g,b, 0,0,  x + dx, y + dy, z - dz, r,g,b, 0,0,
            x - dx, y - dy, z - dz, r,g,b, 0,0,  x + dx, y + dy, z - dz, r,g,b, 0,0,  x - dx, y + dy, z - dz, r,g,b, 0,0,
            // Top
            x - dx, y + dy, z + dz, r,g,b, 0,0,  x + dx, y + dy, z + dz, r,g,b, 0,0,  x + dx, y + dy, z - dz, r,g,b, 0,0,
            x - dx, y + dy, z + dz, r,g,b, 0,0,  x + dx, y + dy, z - dz, r,g,b, 0,0,  x - dx, y + dy, z - dz, r,g,b, 0,0,
            // Bottom
            x - dx, y - dy, z + dz, r,g,b, 0,0,  x + dx, y - dy, z + dz, r,g,b, 0,0,  x + dx, y - dy, z - dz, r,g,b, 0,0,
            x - dx, y - dy, z + dz, r,g,b, 0,0,  x + dx, y - dy, z - dz, r,g,b, 0,0,  x - dx, y - dy, z - dz, r,g,b, 0,0,
            // Left
            x - dx, y - dy, z - dz, r,g,b, 0,0,  x - dx, y - dy, z + dz, r,g,b, 0,0,  x - dx, y + dy, z + dz, r,g,b, 0,0,
            x - dx, y - dy, z - dz, r,g,b, 0,0,  x - dx, y + dy, z + dz, r,g,b, 0,0,  x - dx, y + dy, z - dz, r,g,b, 0,0,
            // Right
            x + dx, y - dy, z - dz, r,g,b, 0,0,  x + dx, y - dy, z + dz, r,g,b, 0,0,  x + dx, y + dy, z + dz, r,g,b, 0,0,
            x + dx, y - dy, z - dz, r,g,b, 0,0,  x + dx, y + dy, z + dz, r,g,b, 0,0,  x + dx, y + dy, z - dz, r,g,b, 0,0,
        };
        v.insert(v.end(), std::begin(faces), std::end(faces));
        };

    addCube(0.0f, 1.5f, 0.0f, 0.2f, 3.0f, 0.2f, 0.5f, 0.5f, 0.5f);
    addCube(0.6f, 2.9f, 0.0f, 1.2f, 0.15f, 0.15f, 0.5f, 0.5f, 0.5f);
    addCube(1.1f, 2.7f, 0.0f, 0.3f, 0.3f, 0.3f, 1.0f, 1.0f, 0.5f);

    glGenVertexArrays(1, &lightVAO);
    glGenBuffers(1, &lightVBO);
    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightVBO);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
}

// --- 3. 자동차 데이터 (Color만 사용) ---
void initCarBuffer() {
    float s = 0.25f; float r = 1.0f, g = 0.2f, b = 0.2f;
    std::vector<float> v;
    // 육면체 만들기 (XYZ RGB UV)
    auto addFace = [&](float* pts) {
        for (int i = 0; i < 18; i += 3) { // 6 points * 3 coords
            v.push_back(pts[i]); v.push_back(pts[i + 1]); v.push_back(pts[i + 2]); // XYZ
            v.push_back(r); v.push_back(g); v.push_back(b); // RGB
            v.push_back(0.0f); v.push_back(0.0f); // UV (Dummy)
        }
        };
    // (간단화를 위해 기존 정점 데이터에 UV 0,0만 추가하는 로직으로 대체 가능하지만,
    //  여기선 코드가 길어지므로 개념적으로만 설명: XYZRGB -> XYZRGBUV 변환 필요)
    //  직접 데이터 입력 (축약함)
    float vertices[] = {
        // Front (XYZ RGB UV)
        -s, -s,  s, r,g,b, 0,0,  s, -s,  s, r,g,b, 0,0,  s,  s,  s, r,g,b, 0,0,
        -s, -s,  s, r,g,b, 0,0,  s,  s,  s, r,g,b, 0,0, -s,  s,  s, r,g,b, 0,0,
        // ... (나머지 면들도 0,0 UV 추가해야 함. 생략 없이 아래에 풀 버전)
        // Back
        -s, -s, -s, r,g,b, 0,0,  s, -s, -s, r,g,b, 0,0,  s,  s, -s, r,g,b, 0,0,
        -s, -s, -s, r,g,b, 0,0,  s,  s, -s, r,g,b, 0,0, -s,  s, -s, r,g,b, 0,0,
        // Top
       -s,  s,  s, r,g,b, 0,0,  s,  s,  s, r,g,b, 0,0,  s,  s, -s, r,g,b, 0,0,
       -s,  s,  s, r,g,b, 0,0,  s,  s, -s, r,g,b, 0,0, -s,  s, -s, r,g,b, 0,0,
       // Bottom
      -s, -s,  s, r,g,b, 0,0,  s, -s,  s, r,g,b, 0,0,  s, -s, -s, r,g,b, 0,0,
      -s, -s,  s, r,g,b, 0,0,  s, -s, -s, r,g,b, 0,0, -s, -s, -s, r,g,b, 0,0,
      // Left
     -s, -s, -s, r,g,b, 0,0, -s, -s,  s, r,g,b, 0,0, -s,  s,  s, r,g,b, 0,0,
     -s, -s, -s, r,g,b, 0,0, -s,  s,  s, r,g,b, 0,0, -s,  s, -s, r,g,b, 0,0,
     // Right
     s, -s, -s, r,g,b, 0,0,  s, -s,  s, r,g,b, 0,0,  s,  s,  s, r,g,b, 0,0,
     s, -s, -s, r,g,b, 0,0,  s,  s,  s, r,g,b, 0,0,  s,  s, -s, r,g,b, 0,0,
    };

    glGenVertexArrays(1, &carVAO);
    glGenBuffers(1, &carVBO);
    glBindVertexArray(carVAO);
    glBindBuffer(GL_ARRAY_BUFFER, carVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
}

// --- 그리기 함수 ---
GLvoid drawScene() {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shaderProgramID);

    float view[16];
    setIdentityMatrix(view, 4);
    view[12] = -carX; view[13] = -1.5f; view[14] = -(carZ + 4.0f);
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, view);

    float projection[16];
    makePerspectiveMatrix(projection, 3.141592f / 4.0f, 800.0f / 600.0f, 0.1f, 300.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, projection);

    float model[16];
    setIdentityMatrix(model, 4);
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);

    // [1] 배경 그리기 (텍스처 사용)
    glUniform1i(useTextureLoc, 1); // 텍스처 사용 ON

    glBindVertexArray(bgVAO);

    // 1-1. 도로 그리기 (0~5 정점: 아스팔트)
    glBindTexture(GL_TEXTURE_2D, roadTextureID);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // 1-2. 인도 그리기 (6~17 정점: 흙)
    glBindTexture(GL_TEXTURE_2D, dirtTextureID);
    glDrawArrays(GL_TRIANGLES, 6, 12);

    // [2] 텍스처 끄기 (가로등, 자동차는 단순 색상)
    glUniform1i(useTextureLoc, 0); // 텍스처 사용 OFF

    // 2. 가로등 그리기
    glBindVertexArray(lightVAO);
    for (float z = 0.0f; z > -300.0f; z -= 20.0f) {
        setTranslationMatrix(model, -2.5f, -0.5f, z);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glDrawArrays(GL_TRIANGLES, 0, 108);

        setTranslationMatrix(model, 2.5f, -0.5f, z);
        model[0] = -1.0f;
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);
        glDrawArrays(GL_TRIANGLES, 0, 108);
    }

    // 3. 자동차 그리기
    setTranslationMatrix(model, carX, -0.25f, carZ);
    model[0] = 1.0f;
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, model);

    glBindVertexArray(carVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glutSwapBuffers();
}

GLvoid Reshape(int w, int h) { glViewport(0, 0, w, h); }
void Keyboard(unsigned char key, int x, int y) { if (key == 'q' || key == 'Q' || key == 27) exit(0); glutPostRedisplay(); }
void SpecialKeyboard(int key, int x, int y) {
    float speed = 0.5f;
    switch (key) {
    case GLUT_KEY_UP:    carZ -= speed; break;
    case GLUT_KEY_DOWN:  carZ += speed; break;
    case GLUT_KEY_LEFT:  carX -= speed; break;
    case GLUT_KEY_RIGHT: carX += speed; break;
    }
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowPosition(100, 100);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Final Project - Textured Road");

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) exit(EXIT_FAILURE);
    glEnable(GL_DEPTH_TEST);

    make_vertexShaders();
    make_fragmentShaders();
    shaderProgramID = make_shaderProgram();

    modelLoc = glGetUniformLocation(shaderProgramID, "model");
    viewLoc = glGetUniformLocation(shaderProgramID, "view");
    projLoc = glGetUniformLocation(shaderProgramID, "projection");
    useTextureLoc = glGetUniformLocation(shaderProgramID, "useTexture");

    // [중요] 텍스처 로드 (파일 이름이 정확해야 함)
    roadTextureID = LoadTexture("road.png");
    dirtTextureID = LoadTexture("dirt.png");

    initBuffer();
    initStreetLightBuffer();
    initCarBuffer();

    glutDisplayFunc(drawScene);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(Keyboard);
    glutSpecialFunc(SpecialKeyboard);

    glutMainLoop();
    return 0;
}