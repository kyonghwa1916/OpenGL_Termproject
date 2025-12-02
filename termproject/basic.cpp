/**
 * ================================================================================
 *                    OpenGL 변환 종합 기본 코드 (basic.cpp)
 * ================================================================================
 *
 * 이 코드는 코딩테스트에서 사용할 수 있는 모든 변환 기능을 포함합니다.
 *
 * [포함된 기능]
 * - 기본 변환: translate, rotate, scale
 * - 뷰 변환: glm::lookAt (카메라)
 * - 투영 변환: glm::perspective
 * - 계층적 변환: 부모-자식 관계
 * - 애니메이션: 시간 기반, 사인파
 * - 입력 처리: 키보드, 마우스
 * - 충돌 감지: 경계 체크
 * - 좌표 변환: 월드 ↔ 로컬
 *
 * [키보드 조작]
 * WASD: 오브젝트 이동
 * Q/E: Y축 회전
 * R/F: 위/아래 이동
 * X/Y/Z: 해당 축 회전 토글
 * 1/2: 큐브/피라미드 선택
 * C: 카메라 공전 토글
 * H: 계층 변환 토글
 * J: 점프 애니메이션
 * P: 일시정지
 * Space: 초기화
 * ESC: 종료
 *
 * [마우스 조작]
 * 좌클릭 드래그: X/Y축 회전
 * 우클릭: 줌 인/아웃
 * ================================================================================
 */

#include <gl/glew.h>
#include <gl/freeglut.h>
#include <gl/freeglut_ext.h>
#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <vector>

// ================================================================================
// 전역 변수 - 셰이더
// ================================================================================
GLuint shaderProgramID;
GLint loc_uMVP;      // MVP 행렬 유니폼 위치
GLint loc_uColor;    // 색상 유니폼 위치

// ================================================================================
// 전역 변수 - 행렬
// ================================================================================
glm::mat4 matProj(1.0f);  // 투영 행렬
glm::mat4 matView(1.0f);  // 뷰 행렬

// ================================================================================
// 전역 변수 - VAO/VBO
// ================================================================================
GLuint cubeVAO, cubeVBO, cubeEBO;
GLuint pyramidVAO, pyramidVBO, pyramidEBO;
GLuint axesVAO, axesVBO;

// ================================================================================
// 전역 변수 - 오브젝트 상태
// ================================================================================
glm::vec3 objPos(0.0f, 0.0f, 0.0f);   // 오브젝트 위치
glm::vec3 objRot(0.0f, 0.0f, 0.0f);   // 오브젝트 회전 (X, Y, Z)
glm::vec3 objScale(1.0f, 1.0f, 1.0f); // 오브젝트 크기
int currentShape = 0;  // 0: 큐브, 1: 피라미드

// ================================================================================
// 전역 변수 - 카메라
// ================================================================================
glm::vec3 camPos(0.0f, 2.0f, 5.0f);    // 카메라 위치
glm::vec3 camTarget(0.0f, 0.0f, 0.0f); // 카메라가 바라보는 점
glm::vec3 camUp(0.0f, 1.0f, 0.0f);     // 카메라 상향 벡터
float camOrbitAngle = 0.0f;            // 카메라 공전 각도
bool camOrbitEnabled = false;          // 카메라 공전 활성화
float camOrbitRadius = 5.0f;           // 카메라 공전 반경

// ================================================================================
// 전역 변수 - 애니메이션
// ================================================================================
bool spinX = false, spinY = false, spinZ = false;  // 축별 회전 활성화
float spinSpeed = 60.0f;  // 회전 속도 (도/초)
int lastTick = 0;         // 이전 프레임 시간
bool isPaused = false;    // 일시정지

// 점프 애니메이션
bool isJumping = false;
float jumpTime = 0.0f;
float jumpHeight = 2.0f;
float baseY = 0.0f;

// 사인파 애니메이션 (팔/다리 흔들림용)
float waveTime = 0.0f;
float waveAmplitude = 30.0f;  // 도

// ================================================================================
// 전역 변수 - 계층 변환
// ================================================================================
bool hierarchyEnabled = false;  // 계층 변환 모드

// 자식 오브젝트 (부모 기준 상대 위치)
struct ChildObject {
    glm::vec3 localPos;   // 부모 기준 상대 위치
    glm::vec3 localRot;   // 로컬 회전
    glm::vec3 localScale; // 로컬 크기
    glm::vec3 color;
};

std::vector<ChildObject> children = {
    {{1.5f, 0.0f, 0.0f}, {0,0,0}, {0.3f, 0.3f, 0.3f}, {1,0,0}},  // 빨강 (오른쪽)
    {{-1.5f, 0.0f, 0.0f}, {0,0,0}, {0.3f, 0.3f, 0.3f}, {0,1,0}}, // 초록 (왼쪽)
    {{0.0f, 1.5f, 0.0f}, {0,0,0}, {0.3f, 0.3f, 0.3f}, {0,0,1}},  // 파랑 (위)
    {{0.0f, -1.5f, 0.0f}, {0,0,0}, {0.3f, 0.3f, 0.3f}, {1,1,0}}, // 노랑 (아래)
};

// ================================================================================
// 전역 변수 - 마우스
// ================================================================================
bool isLeftDragging = false;
bool isRightDragging = false;
int lastMouseX = 0, lastMouseY = 0;
float mouseSensitivity = 0.5f;

// ================================================================================
// 전역 변수 - 충돌/경계
// ================================================================================
float boundaryMin = -3.0f;
float boundaryMax = 3.0f;

// ================================================================================
// 큐브 정점 데이터
// ================================================================================
static const GLfloat cubeVertices[] = {
    // 위치 (x, y, z)
    -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,  // 뒤
    -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f   // 앞
};

static const GLuint cubeIndices[] = {
    0, 3, 7,  0, 7, 4,  // 왼쪽
    0, 1, 2,  0, 2, 3,  // 뒤
    4, 5, 6,  4, 6, 7,  // 앞
    0, 4, 5,  0, 5, 1,  // 아래
    3, 2, 6,  3, 6, 7,  // 위
    1, 5, 6,  1, 6, 2   // 오른쪽
};

// ================================================================================
// 피라미드 정점 데이터
// ================================================================================
static const GLfloat pyramidVertices[] = {
    // 바닥 (사각형)
    -0.5f, 0.0f, -0.5f,
     0.5f, 0.0f, -0.5f,
     0.5f, 0.0f,  0.5f,
    -0.5f, 0.0f,  0.5f,
    // 꼭대기
     0.0f, 1.0f,  0.0f
};

static const GLuint pyramidIndices[] = {
    0, 1, 2,  0, 2, 3,  // 바닥
    0, 1, 4,            // 뒤
    1, 2, 4,            // 오른쪽
    2, 3, 4,            // 앞
    3, 0, 4             // 왼쪽
};

// ================================================================================
// 축 정점 데이터
// ================================================================================
static const GLfloat axesVertices[] = {
    // X축 (빨강)
    0.0f, 0.0f, 0.0f,   2.0f, 0.0f, 0.0f,
    // Y축 (초록)
    0.0f, 0.0f, 0.0f,   0.0f, 2.0f, 0.0f,
    // Z축 (파랑)
    0.0f, 0.0f, 0.0f,   0.0f, 0.0f, 2.0f
};

// ================================================================================
// 셰이더 소스
// ================================================================================
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

// ================================================================================
// 셰이더 컴파일 함수
// ================================================================================
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    // 컴파일 오류 체크
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "셰이더 컴파일 오류: " << infoLog << std::endl;
    }
    return shader;
}

// ================================================================================
// 셰이더 프로그램 생성
// ================================================================================
void initShaders() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    shaderProgramID = glCreateProgram();
    glAttachShader(shaderProgramID, vertexShader);
    glAttachShader(shaderProgramID, fragmentShader);
    glLinkProgram(shaderProgramID);

    // 링크 오류 체크
    GLint success;
    glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgramID, 512, NULL, infoLog);
        std::cerr << "셰이더 링크 오류: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 유니폼 위치 가져오기
    loc_uMVP = glGetUniformLocation(shaderProgramID, "uMVP");
    loc_uColor = glGetUniformLocation(shaderProgramID, "uColor");
}

// ================================================================================
// VAO 초기화 - 큐브
// ================================================================================
void initCubeVAO() {
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

// ================================================================================
// VAO 초기화 - 피라미드
// ================================================================================
void initPyramidVAO() {
    glGenVertexArrays(1, &pyramidVAO);
    glGenBuffers(1, &pyramidVBO);
    glGenBuffers(1, &pyramidEBO);

    glBindVertexArray(pyramidVAO);

    glBindBuffer(GL_ARRAY_BUFFER, pyramidVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(pyramidVertices), pyramidVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pyramidEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(pyramidIndices), pyramidIndices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

// ================================================================================
// VAO 초기화 - 축
// ================================================================================
void initAxesVAO() {
    glGenVertexArrays(1, &axesVAO);
    glGenBuffers(1, &axesVBO);

    glBindVertexArray(axesVAO);

    glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), axesVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

// ================================================================================
// 변환 행렬 생성 함수 (범용)
// ================================================================================
glm::mat4 makeTransform(glm::vec3 pos, glm::vec3 rot, glm::vec3 scale) {
    glm::mat4 M(1.0f);
    M = glm::translate(M, pos);
    M = glm::rotate(M, glm::radians(rot.y), glm::vec3(0, 1, 0));  // Y축 회전
    M = glm::rotate(M, glm::radians(rot.x), glm::vec3(1, 0, 0));  // X축 회전
    M = glm::rotate(M, glm::radians(rot.z), glm::vec3(0, 0, 1));  // Z축 회전
    M = glm::scale(M, scale);
    return M;
}

// ================================================================================
// 큐브 그리기
// ================================================================================
void drawCube(const glm::mat4& model, const glm::vec3& color) {
    glm::mat4 mvp = matProj * matView * model;
    glUniformMatrix4fv(loc_uMVP, 1, GL_FALSE, &mvp[0][0]);
    glUniform3fv(loc_uColor, 1, &color[0]);

    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// ================================================================================
// 피라미드 그리기
// ================================================================================
void drawPyramid(const glm::mat4& model, const glm::vec3& color) {
    glm::mat4 mvp = matProj * matView * model;
    glUniformMatrix4fv(loc_uMVP, 1, GL_FALSE, &mvp[0][0]);
    glUniform3fv(loc_uColor, 1, &color[0]);

    glBindVertexArray(pyramidVAO);
    glDrawElements(GL_TRIANGLES, 18, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

// ================================================================================
// 축 그리기
// ================================================================================
void drawAxes() {
    glm::mat4 mvp = matProj * matView * glm::mat4(1.0f);
    glUniformMatrix4fv(loc_uMVP, 1, GL_FALSE, &mvp[0][0]);

    glBindVertexArray(axesVAO);

    // X축 (빨강)
    glUniform3f(loc_uColor, 1.0f, 0.0f, 0.0f);
    glDrawArrays(GL_LINES, 0, 2);

    // Y축 (초록)
    glUniform3f(loc_uColor, 0.0f, 1.0f, 0.0f);
    glDrawArrays(GL_LINES, 2, 2);

    // Z축 (파랑)
    glUniform3f(loc_uColor, 0.0f, 0.0f, 1.0f);
    glDrawArrays(GL_LINES, 4, 2);

    glBindVertexArray(0);
}

// ================================================================================
// 바닥 그리기
// ================================================================================
void drawFloor() {
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(8.0f, 0.1f, 8.0f));
    drawCube(model, glm::vec3(0.3f, 0.3f, 0.3f));
}

// ================================================================================
// 카메라 업데이트
// ================================================================================
void updateCamera() {
    if (camOrbitEnabled) {
        // 카메라 공전 (Y축 중심)
        float rad = glm::radians(camOrbitAngle);
        camPos.x = camTarget.x + camOrbitRadius * sin(rad);
        camPos.z = camTarget.z + camOrbitRadius * cos(rad);
    }

    // 뷰 행렬 계산
    matView = glm::lookAt(camPos, camTarget, camUp);
}

// ================================================================================
// 충돌 감지 (경계 체크)
// ================================================================================
bool checkBoundary(const glm::vec3& pos) {
    if (pos.x < boundaryMin || pos.x > boundaryMax) return true;
    if (pos.z < boundaryMin || pos.z > boundaryMax) return true;
    return false;
}

// ================================================================================
// 초기화
// ================================================================================
void resetAll() {
    objPos = glm::vec3(0.0f);
    objRot = glm::vec3(0.0f);
    objScale = glm::vec3(1.0f);

    camPos = glm::vec3(0.0f, 2.0f, 5.0f);
    camOrbitAngle = 0.0f;
    camOrbitEnabled = false;

    spinX = spinY = spinZ = false;
    isJumping = false;
    jumpTime = 0.0f;
    waveTime = 0.0f;
    isPaused = false;
    hierarchyEnabled = false;
    currentShape = 0;
}

// ================================================================================
// 장면 그리기
// ================================================================================
void drawScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    glUseProgram(shaderProgramID);

    // 카메라 업데이트
    updateCamera();

    // 축 그리기
    drawAxes();

    // 바닥 그리기
    drawFloor();

    // ============================================================
    // 메인 오브젝트 변환
    // ============================================================
    glm::mat4 parentModel = makeTransform(objPos, objRot, objScale);

    // 메인 오브젝트 그리기
    if (currentShape == 0) {
        drawCube(parentModel, glm::vec3(0.8f, 0.8f, 0.8f));  // 흰색 큐브
    } else {
        drawPyramid(parentModel, glm::vec3(0.8f, 0.6f, 0.2f));  // 주황색 피라미드
    }

    // ============================================================
    // 계층적 변환 (자식 오브젝트)
    // ============================================================
    if (hierarchyEnabled) {
        for (size_t i = 0; i < children.size(); i++) {
            ChildObject& child = children[i];

            // 자식 변환 = 부모 변환 * 로컬 변환
            glm::mat4 childModel = parentModel;

            // 사인파 애니메이션 적용 (좌우 자식만)
            float waveOffset = 0.0f;
            if (i == 0) waveOffset = sin(waveTime) * glm::radians(waveAmplitude);  // 오른쪽
            if (i == 1) waveOffset = -sin(waveTime) * glm::radians(waveAmplitude); // 왼쪽 (반대)

            childModel = glm::translate(childModel, child.localPos);
            childModel = glm::rotate(childModel, glm::radians(child.localRot.x) + waveOffset, glm::vec3(1, 0, 0));
            childModel = glm::rotate(childModel, glm::radians(child.localRot.y), glm::vec3(0, 1, 0));
            childModel = glm::scale(childModel, child.localScale);

            drawCube(childModel, child.color);
        }
    }

    glutSwapBuffers();
}

// ================================================================================
// Idle 함수 (애니메이션)
// ================================================================================
void idle() {
    if (isPaused) return;

    // 델타 타임 계산
    int now = glutGet(GLUT_ELAPSED_TIME);
    if (lastTick == 0) lastTick = now;
    float dt = (now - lastTick) / 1000.0f;
    lastTick = now;

    // 축별 자동 회전
    if (spinX) objRot.x += spinSpeed * dt;
    if (spinY) objRot.y += spinSpeed * dt;
    if (spinZ) objRot.z += spinSpeed * dt;

    // 카메라 공전
    if (camOrbitEnabled) {
        camOrbitAngle += 30.0f * dt;  // 30도/초
        if (camOrbitAngle > 360.0f) camOrbitAngle -= 360.0f;
    }

    // 점프 애니메이션 (사인파)
    if (isJumping) {
        jumpTime += dt * 3.0f;  // 속도 조절
        objPos.y = baseY + sin(jumpTime) * jumpHeight;

        // 점프 완료
        if (jumpTime >= glm::pi<float>()) {
            jumpTime = 0.0f;
            isJumping = false;
            objPos.y = baseY;
        }
    }

    // 사인파 애니메이션
    waveTime += dt * 5.0f;

    glutPostRedisplay();
}

// ================================================================================
// 키보드 콜백
// ================================================================================
void keyboard(unsigned char key, int x, int y) {
    const float moveSpeed = 0.1f;
    const float rotSpeed = 5.0f;
    glm::vec3 newPos = objPos;

    switch (key) {
    // 이동 (WASD + RF)
    case 'w': case 'W': newPos.z -= moveSpeed; break;
    case 's': case 'S': newPos.z += moveSpeed; break;
    case 'a': case 'A': newPos.x -= moveSpeed; break;
    case 'd': case 'D': newPos.x += moveSpeed; break;
    case 'r': case 'R': newPos.y += moveSpeed; break;
    case 'f': case 'F': newPos.y -= moveSpeed; break;

    // 수동 회전
    case 'q': case 'Q': objRot.y -= rotSpeed; break;
    case 'e': case 'E': objRot.y += rotSpeed; break;

    // 자동 회전 토글
    case 'x': spinX = !spinX; break;
    case 'y': spinY = !spinY; break;
    case 'z': spinZ = !spinZ; break;

    // 형태 선택
    case '1': currentShape = 0; break;  // 큐브
    case '2': currentShape = 1; break;  // 피라미드

    // 카메라 공전 토글
    case 'c': case 'C': camOrbitEnabled = !camOrbitEnabled; break;

    // 계층 변환 토글
    case 'h': case 'H': hierarchyEnabled = !hierarchyEnabled; break;

    // 점프
    case 'j': case 'J':
        if (!isJumping) {
            isJumping = true;
            jumpTime = 0.0f;
            baseY = objPos.y;
        }
        break;

    // 일시정지
    case 'p': case 'P': isPaused = !isPaused; break;

    // 초기화
    case ' ': resetAll(); break;

    // 종료
    case 27: glutLeaveMainLoop(); break;  // ESC
    }

    // 충돌 감지 적용
    if (!checkBoundary(newPos)) {
        objPos = newPos;
    }

    glutPostRedisplay();
}

// ================================================================================
// 특수키 콜백 (방향키)
// ================================================================================
void specialKeys(int key, int x, int y) {
    const float camMoveSpeed = 0.2f;

    switch (key) {
    case GLUT_KEY_UP:    camPos.y += camMoveSpeed; break;
    case GLUT_KEY_DOWN:  camPos.y -= camMoveSpeed; break;
    case GLUT_KEY_LEFT:  camPos.x -= camMoveSpeed; break;
    case GLUT_KEY_RIGHT: camPos.x += camMoveSpeed; break;
    }

    glutPostRedisplay();
}

// ================================================================================
// 마우스 버튼 콜백
// ================================================================================
void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            isLeftDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        } else {
            isLeftDragging = false;
        }
    }
    else if (button == GLUT_RIGHT_BUTTON) {
        if (state == GLUT_DOWN) {
            isRightDragging = true;
            lastMouseY = y;
        } else {
            isRightDragging = false;
        }
    }
}

// ================================================================================
// 마우스 드래그 콜백
// ================================================================================
void motion(int x, int y) {
    if (isLeftDragging) {
        // 좌클릭 드래그: 오브젝트 회전
        int dx = x - lastMouseX;
        int dy = y - lastMouseY;

        objRot.y += dx * mouseSensitivity;
        objRot.x += dy * mouseSensitivity;

        lastMouseX = x;
        lastMouseY = y;
        glutPostRedisplay();
    }
    else if (isRightDragging) {
        // 우클릭 드래그: 줌
        int dy = y - lastMouseY;
        camPos.z += dy * 0.05f;

        // 줌 제한
        if (camPos.z < 2.0f) camPos.z = 2.0f;
        if (camPos.z > 15.0f) camPos.z = 15.0f;

        lastMouseY = y;
        glutPostRedisplay();
    }
}

// ================================================================================
// 창 크기 변경 콜백
// ================================================================================
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    float aspect = (float)w / (float)h;
    matProj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
}

// ================================================================================
// 메인 함수
// ================================================================================
int main(int argc, char** argv) {
    // GLUT 초기화
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL Basic Transformations");

    // GLEW 초기화
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW 초기화 실패" << std::endl;
        return -1;
    }

    // OpenGL 설정
    glEnable(GL_DEPTH_TEST);  // 깊이 테스트

    // 초기화
    initShaders();
    initCubeVAO();
    initPyramidVAO();
    initAxesVAO();

    // 콜백 등록
    glutDisplayFunc(drawScene);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutIdleFunc(idle);

    std::cout << "===============================================" << std::endl;
    std::cout << "OpenGL 변환 기본 코드" << std::endl;
    std::cout << "===============================================" << std::endl;
    std::cout << "[키보드]" << std::endl;
    std::cout << "WASD: 이동, R/F: 위/아래" << std::endl;
    std::cout << "Q/E: Y축 회전" << std::endl;
    std::cout << "X/Y/Z: 자동 회전 토글" << std::endl;
    std::cout << "1/2: 큐브/피라미드 선택" << std::endl;
    std::cout << "C: 카메라 공전, H: 계층변환" << std::endl;
    std::cout << "J: 점프, P: 일시정지" << std::endl;
    std::cout << "Space: 초기화, ESC: 종료" << std::endl;
    std::cout << "[마우스]" << std::endl;
    std::cout << "좌클릭 드래그: 회전" << std::endl;
    std::cout << "우클릭 드래그: 줌" << std::endl;
    std::cout << "방향키: 카메라 이동" << std::endl;
    std::cout << "===============================================" << std::endl;

    glutMainLoop();

    return 0;
}
