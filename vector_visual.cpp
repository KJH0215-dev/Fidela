#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

#include "Vector3.h"
#include "Camera.h"
#include "Mesh.h"
#include "ObjParser.h"
#include "MeshDiagnostics.h"
#include "Surface.h"

namespace mat4 {

    void identity(float* m) {
        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    void multiply(const float* a, const float* b, float* out) {
        float result[16];
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                float sum = 0.0f;
                for (int k = 0; k < 4; k++) {
                    sum += a[k * 4 + row] * b[col * 4 + k];
                }
                result[col * 4 + row] = sum;
            }
        }
        for (int i = 0; i < 16; i++) out[i] = result[i];
    }

    void perspective(float fovYRadians, float aspect, float nearZ, float farZ, float* out) {
        identity(out);
        float f = 1.0f / std::tan(fovYRadians / 2.0f);
        out[0] = f / aspect;
        out[5] = f;
        out[10] = (farZ + nearZ) / (nearZ - farZ);
        out[11] = -1.0f;
        out[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
        out[15] = 0.0f;
    }
    void lookAt(const Vector3& eye, const Vector3& center, const Vector3& up, float* out) {
        Vector3 f = (center - eye).Normalize();
        Vector3 s = f.Cross(up).Normalize();
        Vector3 u = s.Cross(f);

        identity(out);
        out[0] = (float)s.x;  out[4] = (float)s.y;  out[8] = (float)s.z;
        out[1] = (float)u.x;  out[5] = (float)u.y;  out[9] = (float)u.z;
        out[2] = (float)-f.x; out[6] = (float)-f.y; out[10] = (float)-f.z;
        out[12] = (float)-s.Dot(eye);
        out[13] = (float)-u.Dot(eye);
        out[14] = (float)f.Dot(eye);
    }

}

static const char* kVertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uViewProj;
out vec3 vColor;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

static const char* kFragmentShaderSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::fprintf(stderr, "셰이더 컴파일 실패: %s\n", log);
    }
    return shader;
}

static GLuint CreateShaderProgram() {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShaderSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSrc);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::fprintf(stderr, "셰이더 링크 실패: %s\n", log);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

static void PushLine(std::vector<float>& out, const Vector3& start, const Vector3& end, const Vector3& color) {
    out.push_back((float)start.x); out.push_back((float)start.y); out.push_back((float)start.z);
    out.push_back((float)color.x); out.push_back((float)color.y); out.push_back((float)color.z);
    out.push_back((float)end.x);   out.push_back((float)end.y);   out.push_back((float)end.z);
    out.push_back((float)color.x); out.push_back((float)color.y); out.push_back((float)color.z);
}

static void PushGrid(std::vector<float>& out, float halfExtent, float spacing, const Vector3& color) {
    for (float i = -halfExtent; i <= halfExtent + 0.001f; i += spacing) {
        PushLine(out, Vector3(i, 0, -halfExtent), Vector3(i, 0, halfExtent), color);
        PushLine(out, Vector3(-halfExtent, 0, i), Vector3(halfExtent, 0, i), color);
    }
}

static void PushMeshTriangles(std::vector<float>& out, const Mesh& mesh, const Vector3& eye,
    const MeshDiagnostics& diag, bool showProblems, bool showBackface,
    int selectedFace) {
    const Vector3 selectedColor(1.0, 0.9, 0.1);
    const Vector3 meshColor(0.55, 0.60, 0.75);
    const Vector3 backfaceColor(1.0, 0.35, 0.55);
    const Vector3 problemColor(1.0, 0.1, 0.1);

    for (size_t i = 0; i < mesh.triangles.size(); i++) {
        const Triangle& tri = mesh.triangles[i];
        const Vector3& p0 = mesh.positions[tri.v0];
        const Vector3& p1 = mesh.positions[tri.v1];
        const Vector3& p2 = mesh.positions[tri.v2];

        Vector3 faceNormal;
        if (mesh.HasNormals() && tri.n0 >= 0) {
            faceNormal = mesh.normals[tri.n0];
        }
        else {
            Vector3 edge1 = p1 - p0;
            Vector3 edge2 = p2 - p0;
            faceNormal = edge1.Cross(edge2).Normalize();
        }

        Vector3 centroid = (p0 + p1 + p2) * (1.0 / 3.0);
        Vector3 viewDir = (eye - centroid).Normalize();
        double facing = faceNormal.Dot(viewDir);

        Vector3 color = meshColor;
        if ((int)i == selectedFace) {
            color = selectedColor;
        }
        else if (showProblems && i < diag.faceFlags.size() && diag.faceFlags[i] == FACE_FLIPPED) {
            color = problemColor;
        }
        else if (showBackface && facing < 0.0) {
            color = backfaceColor;
        }

        out.push_back((float)p0.x); out.push_back((float)p0.y); out.push_back((float)p0.z);
        out.push_back((float)color.x); out.push_back((float)color.y); out.push_back((float)color.z);
        out.push_back((float)p1.x); out.push_back((float)p1.y); out.push_back((float)p1.z);
        out.push_back((float)color.x); out.push_back((float)color.y); out.push_back((float)color.z);
        out.push_back((float)p2.x); out.push_back((float)p2.y); out.push_back((float)p2.z);
        out.push_back((float)color.x); out.push_back((float)color.y); out.push_back((float)color.z);
    }
}

static Mesh g_mesh;
static bool g_meshLoaded = false;
static int g_meshVersion = 0;
static MeshDiagnostics g_diag;
static bool g_wireframe = false;
static char g_meshPathBuf[256] = "C:\\Users\\USER\\OneDrive\\Desktop\\OBJ";
static std::string g_meshLoadError;
static bool g_showAxes = true;
static bool g_showGrid = true;
static bool g_showVectors = true;
static bool g_showMesh = true;
static bool g_showProblems = true;
static bool g_showBackface = false;
static bool g_showLeftPanel = true;
static bool g_showBottomPanel = true;

static const ImGuiWindowFlags kPanelFlags =
ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
static const ImGuiWindowFlags kStatusFlags =
ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
ImGuiWindowFlags_NoBringToFrontOnFocus;

static bool g_showSurface = false;
static SurfaceInput g_surfInput = SurfacePreset(0);
static SurfaceModel g_surfModel;
static SurfaceMesh g_surfMesh;
static bool g_surfRecompile = true;
static bool g_surfDirty = true;
static bool g_surfNeedResetPoint = true;
static std::string g_surfError;
static bool g_surfColorByHeight = true;
static bool g_surfWire = true;
static bool g_surfShowTangent = true;
static float g_surfU = 0.8f;
static float g_surfV = 0.4f;
static float g_surfT = 0.3f;
static double g_surfIntegral = 0.0;
static bool g_surfIntegralUndefined = false;

struct SurfacePoint {
    Vector3 P;
    Vector3 Pu;
    Vector3 Pv;
    Vector3 N;
};
static SurfacePoint g_surfPoint;

static Vector3 ToView(const Vector3& m) {
    return Vector3(m.x, m.z, -m.y);
}

static void ResetSurfacePoint() {
    g_surfT = 0.3f;
    if (!g_surfModel.IsReady() || g_surfModel.Mode() == SURFMODE_IMPLICIT) return;
    double u0, u1, v0, v1;
    g_surfModel.Domain(g_surfInput.params, u0, u1, v0, v1);
    g_surfU = (float)(u0 + 0.7 * (u1 - u0));
    g_surfV = (float)(v0 + 0.6 * (v1 - v0));
}

static void UpdateSurfacePoint() {
    SurfacePoint sp;
    g_surfPoint = sp;
    if (!g_surfModel.IsReady()) return;
    const SurfaceParams& p = g_surfInput.params;

    if (g_surfModel.Mode() == SURFMODE_IMPLICIT) {
        if (g_surfMesh.Empty()) return;
        const size_t count = g_surfMesh.indices.size();
        const float t = std::min(std::max(g_surfT, 0.0f), 1.0f);
        const size_t pick = (size_t)(t * (float)(count - 1));
        sp.P = g_surfMesh.points[(size_t)g_surfMesh.indices[pick]];
        sp.N = SurfaceGradient(g_surfModel, p, sp.P);
        const Vector3 n = sp.N.Normalize();
        const Vector3 helper = (std::fabs(n.z) < 0.9) ? Vector3(0, 0, 1) : Vector3(1, 0, 0);
        sp.Pu = n.Cross(helper).Normalize();
        sp.Pv = n.Cross(sp.Pu).Normalize();
        g_surfPoint = sp;
        return;
    }

    double u0, u1, v0, v1;
    g_surfModel.Domain(p, u0, u1, v0, v1);
    const double u = std::min(std::max((double)g_surfU, u0), u1);
    const double v = std::min(std::max((double)g_surfV, v0), v1);
    g_surfU = (float)u;
    g_surfV = (float)v;

    const double hu = (u1 - u0) * 1e-4;
    const double hv = (v1 - v0) * 1e-4;
    sp.P = g_surfModel.Eval(p, u, v);
    sp.Pu = (g_surfModel.Eval(p, u + hu, v) - g_surfModel.Eval(p, u - hu, v)) * (1.0 / (2.0 * hu));
    sp.Pv = (g_surfModel.Eval(p, u, v + hv) - g_surfModel.Eval(p, u, v - hv)) * (1.0 / (2.0 * hv));
    sp.N = sp.Pu.Cross(sp.Pv);
    g_surfPoint = sp;
}

static Vector3 HeightColor(double t) {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    double r, g, b;
    if (t < 0.25) { const double k = t / 0.25;          r = 0.0; g = k;       b = 1.0; }
    else if (t < 0.5) { const double k = (t - 0.25) / 0.25; r = 0.0; g = 1.0;     b = 1.0 - k; }
    else if (t < 0.75) { const double k = (t - 0.5) / 0.25;  r = k;   g = 1.0;     b = 0.0; }
    else { const double k = (t - 0.75) / 0.25; r = 1.0; g = 1.0 - k; b = 0.0; }
    return Vector3(r * 0.9, g * 0.9, b * 0.9);
}

static void PushSurfaceTriangles(std::vector<float>& out, const SurfaceMesh& m, const Vector3& eye, bool colorByHeight) {
    const size_t count = m.points.size();
    std::vector<Vector3> viewPos(count);
    std::vector<Vector3> colors(count);
    const double span = m.zMax - m.zMin;

    for (size_t k = 0; k < count; k++) {
        viewPos[k] = ToView(m.points[k]);
        const Vector3 n = ToView(m.normals[k]);
        const Vector3 viewDir = (eye - viewPos[k]).Normalize();
        const double facing = std::fabs(n.Dot(viewDir));
        const double light = 0.3 + 0.7 * facing;

        Vector3 base(0.55, 0.60, 0.75);
        if (colorByHeight && span > 1e-9) {
            base = HeightColor((m.points[k].z - m.zMin) / span);
        }
        colors[k] = base * light;
    }

    out.reserve(m.indices.size() * 6);
    for (size_t t = 0; t < m.indices.size(); t++) {
        const size_t k = (size_t)m.indices[t];
        out.push_back((float)viewPos[k].x); out.push_back((float)viewPos[k].y); out.push_back((float)viewPos[k].z);
        out.push_back((float)colors[k].x);  out.push_back((float)colors[k].y);  out.push_back((float)colors[k].z);
    }
}

struct Issue {
    int face;
    unsigned char type;
};
static std::vector<Issue> g_issues;
static int g_selectedFace = -1;
static std::vector<std::string> g_log;
static bool g_logScrollToBottom = false;

static void Log(const std::string& message) {
    g_log.push_back(message);
    if (g_log.size() > 200) {
        g_log.erase(g_log.begin());
    }
    g_logScrollToBottom = true;
}

static void TryLoadMesh(const std::string& path) {
    Mesh loaded;
    std::string err;
    if (LoadObj(path, loaded, err)) {
        g_mesh = loaded;
        g_meshLoaded = true;
        g_meshVersion++;
        g_meshLoadError.clear();
        g_diag = AnalyzeMesh(g_mesh);

        g_issues.clear();
        for (size_t i = 0; i < g_diag.faceFlags.size(); i++) {
            if (g_diag.faceFlags[i] != FACE_OK) {
                g_issues.push_back(Issue{ (int)i, g_diag.faceFlags[i] });
            }
        }
        g_selectedFace = -1;

        Log("[정보] 불러옴: " + path + " (정점 " + std::to_string(g_mesh.positions.size())
            + ", 삼각형 " + std::to_string(g_mesh.triangles.size()) + ")");
        if (g_issues.empty()) {
            Log("[정보] 심각한 문제 없음");
        }
        if (g_diag.suspectedFlippedFaces > 0) {
            Log("[문제] 뒤집힘 면 " + std::to_string(g_diag.suspectedFlippedFaces) + "개");
        }
        if (g_diag.degenerateFaces > 0) {
            Log("[문제] 퇴화 삼각형 " + std::to_string(g_diag.degenerateFaces) + "개");
        }
        size_t copyLen = path.size();
        if (copyLen > sizeof(g_meshPathBuf) - 1) {
            copyLen = sizeof(g_meshPathBuf) - 1;
        }
        std::memcpy(g_meshPathBuf, path.data(), copyLen);
        g_meshPathBuf[copyLen] = '\0';
    }
    else {
        g_meshLoaded = false;
        g_meshLoadError = err;
        g_issues.clear();
        g_selectedFace = -1;
        Log("[오류] " + err);
    }
}

static void DropCallback(GLFWwindow* window, int count, const char** paths) {
    (void)window;
    if (count > 0) {
        TryLoadMesh(paths[0]);
    }
}

static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    Camera* cam = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (cam && !ImGui::GetIO().WantCaptureMouse) {
        cam->ProcessScroll((float)yoffset);
    }
}

int main() {
    if (!glfwInit()) {
        std::fprintf(stderr, "GLFW 초기화 실패\n");
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1100, 900, "Vector Inspector - Step 1", nullptr, nullptr);
    if (!window) {
        std::fprintf(stderr, "창 생성 실패\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "GLEW 초기화 실패\n");
        return -1;
    }
    Camera camera;
    glfwSetWindowUserPointer(window, &camera);
    glfwSetScrollCallback(window, ScrollCallback);
    glfwSetDropCallback(window, DropCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\malgun.ttf",
        18.0f,
        nullptr,
        io.Fonts->GetGlyphRangesKorean()
    );

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    GLuint program = CreateShaderProgram();
    GLint uViewProjLoc = glGetUniformLocation(program, "uViewProj");

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint meshVao, meshVbo;
    glGenVertexArrays(1, &meshVao);
    glGenBuffers(1, &meshVbo);
    glBindVertexArray(meshVao);
    glBindBuffer(GL_ARRAY_BUFFER, meshVbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glEnable(GL_DEPTH_TEST);

    static float aVals[3] = { 1.0f, 0.0f, 0.0f };
    static float bVals[3] = { 0.0f, 1.0f, 0.0f };

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            static double lastMouseX = 0.0, lastMouseY = 0.0;
            static bool firstSample = true;

            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);

            if (firstSample) {
                lastMouseX = mouseX;
                lastMouseY = mouseY;
                firstSample = false;
            }

            double deltaX = mouseX - lastMouseX;
            double deltaY = mouseY - lastMouseY;
            lastMouseX = mouseX;
            lastMouseY = mouseY;

            bool leftButtonDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

            bool panButtonDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS ||
                glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
            bool shiftDown = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

            if (!ImGui::GetIO().WantCaptureMouse && (panButtonDown || (leftButtonDown && shiftDown))) {
                camera.ProcessPan((float)deltaX, (float)deltaY);
            }
            else if (leftButtonDown && !ImGui::GetIO().WantCaptureMouse) {
                camera.ProcessMouseDrag((float)deltaX, (float)deltaY);
            }
        }

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        const float menuHeight = ImGui::GetFrameHeight();
        const float statusHeight = 32.0f;
        const float leftWidth = g_showLeftPanel ? 340.0f : 0.0f;
        const float bottomHeight = g_showBottomPanel ? 230.0f : 0.0f;
        const float bodyTop = menuHeight;
        const float bodyBottom = displaySize.y - statusHeight;

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("파일(F)")) {
                if (ImGui::MenuItem("OBJ 불러오기")) {
                    TryLoadMesh(g_meshPathBuf);
                }
                if (ImGui::MenuItem("종료")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("보기(V)")) {
                ImGui::MenuItem("도구 상자", nullptr, &g_showLeftPanel);
                ImGui::MenuItem("오류 목록 / 출력", nullptr, &g_showBottomPanel);
                ImGui::Separator();
                ImGui::MenuItem("와이어프레임", nullptr, &g_wireframe);
                ImGui::MenuItem("문제 면 빨간색 표시", nullptr, &g_showProblems);
                ImGui::MenuItem("축", nullptr, &g_showAxes);
                ImGui::MenuItem("그리드", nullptr, &g_showGrid);
                ImGui::MenuItem("곡면", nullptr, &g_showSurface);
                ImGui::Separator();
                if (ImGui::MenuItem("카메라 초기화")) {
                    camera.Reset();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        if (g_showLeftPanel) {
            ImGui::SetNextWindowPos(ImVec2(0.0f, bodyTop));
            ImGui::SetNextWindowSize(ImVec2(leftWidth, bodyBottom - bodyTop));
            ImGui::Begin("도구 상자", nullptr, kPanelFlags);
            ImGui::PushTextWrapPos(0.0f);

            if (ImGui::CollapsingHeader("벡터 계산기", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat3("Vector A", aVals, 0.05f);
                ImGui::DragFloat3("Vector B", bVals, 0.05f);

                Vector3 av(aVals[0], aVals[1], aVals[2]);
                Vector3 bv(bVals[0], bVals[1], bVals[2]);
                Vector3 crossAB = av.Cross(bv);

                ImGui::Separator();
                ImGui::Text("Dot(A, B)   = %.3f", av.Dot(bv));
                ImGui::Text("Cross(A, B) = (%.3f, %.3f, %.3f)", crossAB.x, crossAB.y, crossAB.z);
                ImGui::Text("|A| = %.3f   |B| = %.3f", av.Length(), bv.Length());
            }

            if (ImGui::CollapsingHeader("곡면 시각화", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("곡면 표시", &g_showSurface);
                if (g_showSurface) {
                    SurfaceInput& in = g_surfInput;
                    SurfaceParams& sp = in.params;

                    if (ImGui::BeginCombo("예시", "고르면 식이 채워져요")) {
                        for (int k = 0; k < SurfacePresetCount(); k++) {
                            if (ImGui::Selectable(SurfacePresetName(k))) {
                                in = SurfacePreset(k);
                                g_surfRecompile = true;
                                g_surfNeedResetPoint = true;
                                g_surfWire = (in.mode != SURFMODE_IMPLICIT);
                            }
                        }
                        ImGui::EndCombo();
                    }

                    int pickedMode = in.mode;
                    ImGui::RadioButton("z=f(x,y)", &pickedMode, SURFMODE_EXPLICIT);
                    ImGui::SameLine();
                    ImGui::RadioButton("매개변수", &pickedMode, SURFMODE_PARAMETRIC);
                    ImGui::SameLine();
                    ImGui::RadioButton("F(x,y,z)=0", &pickedMode, SURFMODE_IMPLICIT);
                    if (pickedMode != in.mode) {
                        in = SurfaceDefaultFor(pickedMode);
                        g_surfRecompile = true;
                        g_surfNeedResetPoint = true;
                        g_surfWire = (in.mode != SURFMODE_IMPLICIT);
                    }

                    bool textChanged = false;
                    if (in.mode == SURFMODE_EXPLICIT) {
                        ImGui::Text("z =");
                        ImGui::SetNextItemWidth(-1.0f);
                        if (ImGui::InputText("##exprZ", in.exprZ, sizeof(in.exprZ))) textChanged = true;
                    }
                    else if (in.mode == SURFMODE_PARAMETRIC) {
                        ImGui::Text("x(u,v) =");
                        ImGui::SetNextItemWidth(-1.0f);
                        if (ImGui::InputText("##exprPX", in.exprPX, sizeof(in.exprPX))) textChanged = true;
                        ImGui::Text("y(u,v) =");
                        ImGui::SetNextItemWidth(-1.0f);
                        if (ImGui::InputText("##exprPY", in.exprPY, sizeof(in.exprPY))) textChanged = true;
                        ImGui::Text("z(u,v) =");
                        ImGui::SetNextItemWidth(-1.0f);
                        if (ImGui::InputText("##exprPZ", in.exprPZ, sizeof(in.exprPZ))) textChanged = true;

                        ImGui::Text("u:");
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(80.0f);
                        if (ImGui::InputText("##uMin", in.uMin, sizeof(in.uMin))) textChanged = true;
                        ImGui::SameLine();
                        ImGui::Text("~");
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(80.0f);
                        if (ImGui::InputText("##uMax", in.uMax, sizeof(in.uMax))) textChanged = true;

                        ImGui::Text("v:");
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(80.0f);
                        if (ImGui::InputText("##vMin", in.vMin, sizeof(in.vMin))) textChanged = true;
                        ImGui::SameLine();
                        ImGui::Text("~");
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(80.0f);
                        if (ImGui::InputText("##vMax", in.vMax, sizeof(in.vMax))) textChanged = true;
                    }
                    else {
                        ImGui::Text("F(x,y,z) = 0   (예: x^2 + y^2 + z^2 = 4)");
                        ImGui::SetNextItemWidth(-1.0f);
                        if (ImGui::InputText("##exprF", in.exprF, sizeof(in.exprF))) textChanged = true;
                    }
                    if (textChanged) {
                        g_surfRecompile = true;
                    }

                    if (!g_surfError.empty()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_surfError.c_str());
                    }
                    else if (g_surfModel.IsReady() && !g_surfDirty && g_surfMesh.Empty()) {
                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                            "그릴 수 있는 부분이 없어요. 범위를 늘리거나 식을 확인해 보세요.");
                    }

                    if (ImGui::TreeNode("식 입력 도움말")) {
                        ImGui::TextWrapped("+ - * / ^ ( ) 를 쓸 수 있고, 2x 처럼 곱셈 기호는 생략해도 돼요.");
                        ImGui::TextWrapped("상수: pi, e");
                        ImGui::TextWrapped("함수: sin cos tan asin acos atan sinh cosh tanh exp ln log(상용로그) sqrt cbrt abs sign floor ceil, pow(a,b) atan2(y,x) min(a,b) max(a,b)");
                        ImGui::TextWrapped("a, b, c 는 아래 슬라이더 값이에요. 식 안에 넣으면 값을 바꿔 가며 볼 수 있어요.");
                        ImGui::TextWrapped("z=f(x,y): x, y 사용 / 매개변수: u, v 사용 / 음함수: x, y, z 사용 (= 로 양변을 적으면 돼요)");
                        ImGui::TreePop();
                    }

                    bool changed = false;
                    if (ImGui::SliderFloat("a", &sp.a, -3.0f, 3.0f)) changed = true;
                    if (ImGui::SliderFloat("b", &sp.b, -3.0f, 3.0f)) changed = true;
                    if (ImGui::SliderFloat("c", &sp.c, -3.0f, 3.0f)) changed = true;
                    if (in.mode != SURFMODE_PARAMETRIC) {
                        if (ImGui::SliderFloat("범위 R", &sp.range, 0.5f, 6.0f)) changed = true;
                    }
                    const int maxResolution = (in.mode == SURFMODE_IMPLICIT) ? 64 : 120;
                    if (sp.resolution > maxResolution) { sp.resolution = maxResolution; changed = true; }
                    if (ImGui::SliderInt("해상도", &sp.resolution, 8, maxResolution)) changed = true;
                    if (changed) {
                        g_surfDirty = true;
                    }

                    ImGui::Checkbox("높이별 색상", &g_surfColorByHeight);
                    ImGui::SameLine();
                    ImGui::Checkbox("격자선", &g_surfWire);
                    ImGui::Checkbox("접평면 / 법선 벡터", &g_surfShowTangent);

                    if (g_surfModel.IsReady()) {
                        ImGui::Separator();
                        const int mode = g_surfModel.Mode();
                        if (mode == SURFMODE_IMPLICIT) {
                            ImGui::SliderFloat("표면 위 점 고르기", &g_surfT, 0.0f, 1.0f);
                        }
                        else {
                            double u0, u1, v0, v1;
                            g_surfModel.Domain(sp, u0, u1, v0, v1);
                            ImGui::SliderFloat(mode == SURFMODE_EXPLICIT ? "x" : "u", &g_surfU, (float)u0, (float)u1);
                            ImGui::SliderFloat(mode == SURFMODE_EXPLICIT ? "y" : "v", &g_surfV, (float)v0, (float)v1);
                        }
                        UpdateSurfacePoint();
                        const SurfacePoint& pt = g_surfPoint;

                        ImGui::Text("점 P = (%.3f, %.3f, %.3f)", pt.P.x, pt.P.y, pt.P.z);
                        if (mode == SURFMODE_IMPLICIT) {
                            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f),
                                "기울기 grad F = (%.3f, %.3f, %.3f)", pt.N.x, pt.N.y, pt.N.z);
                            ImGui::TextWrapped("기울기 벡터는 곡면에 수직이에요 (초록 화살표).");
                        }
                        else {
                            if (mode == SURFMODE_EXPLICIT) {
                                ImGui::Text("fx = %.3f    fy = %.3f", pt.Pu.z, pt.Pv.z);
                                ImGui::TextWrapped("접평면: z = %.3f + %.3f*(x - %.3f) + %.3f*(y - %.3f)",
                                    pt.P.z, pt.Pu.z, pt.P.x, pt.Pv.z, pt.P.y);
                            }
                            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Pu = (%.3f, %.3f, %.3f)", pt.Pu.x, pt.Pu.y, pt.Pu.z);
                            ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f), "Pv = (%.3f, %.3f, %.3f)", pt.Pv.x, pt.Pv.y, pt.Pv.z);
                            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "법선 = Pu x Pv = (%.3f, %.3f, %.3f)", pt.N.x, pt.N.y, pt.N.z);
                        }
                        if (mode == SURFMODE_EXPLICIT) {
                            ImGui::Text("이중적분 (x,y: -R ~ R 정사각형) = %.4f", g_surfIntegral);
                            if (g_surfIntegralUndefined) {
                                ImGui::TextWrapped("(식이 정의되지 않는 구간은 빼고 계산했어요)");
                            }
                        }
                    }

                    ImGui::Separator();
                    if (ImGui::Button("OBJ로 내보내기 (surface.obj)")) {
                        std::string err;
                        if (ExportSurfaceObj(g_surfMesh, "surface.obj", err)) {
                            Log("[정보] 내보냄: surface.obj (점 " + std::to_string(g_surfMesh.points.size())
                                + "개, 삼각형 " + std::to_string(g_surfMesh.indices.size() / 3) + "개, 블렌더용 Z-up)");
                        }
                        else {
                            Log("[오류] " + err);
                        }
                    }
                }
            }

            if (ImGui::CollapsingHeader("메쉬 불러오기", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped("OBJ 파일을 이 창에 드래그해서 놓으면 바로 열립니다.");
                ImGui::InputText("OBJ 경로", g_meshPathBuf, sizeof(g_meshPathBuf));
                if (ImGui::Button("불러오기")) {
                    TryLoadMesh(g_meshPathBuf);
                }
                ImGui::SameLine();
                if (ImGui::Button("카메라 초기화")) {
                    camera.Reset();
                }
                if (!g_meshLoadError.empty()) {
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", g_meshLoadError.c_str());
                }
            }

            if (ImGui::CollapsingHeader("표시 옵션", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("와이어프레임", &g_wireframe);
                ImGui::Checkbox("메쉬", &g_showMesh);
                ImGui::SameLine();
                ImGui::Checkbox("벡터(A/B/외적)", &g_showVectors);
                ImGui::Checkbox("축", &g_showAxes);
                ImGui::SameLine();
                ImGui::Checkbox("그리드", &g_showGrid);
                ImGui::Checkbox("문제 면 빨간색 표시", &g_showProblems);
                ImGui::Checkbox("카메라 반대 면(분홍) 표시", &g_showBackface);
            }

            if (g_meshLoaded && ImGui::CollapsingHeader("메쉬 정보 / 진단 요약", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("정점 개수: %zu", g_mesh.positions.size());
                ImGui::Text("삼각형 개수: %zu", g_mesh.triangles.size());
                ImGui::Text("노멀 포함: %s", g_mesh.HasNormals() ? "예" : "아니오 (외적으로 계산함)");
                if (g_showBackface) {
                    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.55f, 1), "분홍색 면 = 카메라 반대쪽을 향한 면 (실시간, 카메라에 따라 바뀜)");
                }

                ImGui::Separator();
                ImGui::Text("진단 요약 (카메라와 무관, 메쉬 자체 기준)");
                ImGui::TextColored(ImVec4(1, 0.1f, 0.1f, 1), "빨간 면 = 뒤집힘 / 빨간 선 = 퇴화 삼각형");

                if (g_diag.suspectedFlippedFaces == 0 && g_diag.degenerateFaces == 0) {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1), "심각한 문제 없음");
                }

                if (g_diag.suspectedFlippedFaces > 0) {
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                        "뒤집힘 면: %d / %d", g_diag.suspectedFlippedFaces, g_diag.totalFaces);
                }
                else {
                    ImGui::Text("뒤집힘 면: 0 / %d", g_diag.totalFaces);
                }

                if (g_diag.degenerateFaces > 0) {
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1),
                        "퇴화 삼각형(면적 0): %d", g_diag.degenerateFaces);
                }
                else {
                    ImGui::Text("퇴화 삼각형(면적 0): 0");
                }

                ImGui::Text("바운딩 박스: (%.3f, %.3f, %.3f)",
                    g_diag.boundsSize.x, g_diag.boundsSize.y, g_diag.boundsSize.z);

                if (g_diag.scaleWarningTooSmall) {
                    ImGui::TextColored(ImVec4(1, 0.7f, 0.2f, 1),
                        "가장 긴 변이 %.4f — 단위 착오 의심 (블렌더 스케일 Apply 확인)", g_diag.maxDimension);
                }
                else if (g_diag.scaleWarningTooLarge) {
                    ImGui::TextColored(ImVec4(1, 0.7f, 0.2f, 1),
                        "가장 긴 변이 %.1f — 비정상적으로 큼, 단위 착오 의심", g_diag.maxDimension);
                }
            }

            ImGui::PopTextWrapPos();
            ImGui::End();
        }

        if (g_showBottomPanel) {
            ImGui::SetNextWindowPos(ImVec2(leftWidth, bodyBottom - bottomHeight));
            ImGui::SetNextWindowSize(ImVec2(displaySize.x - leftWidth, bottomHeight));
            ImGui::Begin("오류 목록 / 출력", nullptr, kPanelFlags);
            if (ImGui::BeginTabBar("panelTabs")) {
                char errTabLabel[64];
                std::snprintf(errTabLabel, sizeof(errTabLabel), "오류 목록 (%zu)###errTab", g_issues.size());
                if (ImGui::BeginTabItem(errTabLabel)) {
                    if (!g_meshLoaded) {
                        ImGui::TextDisabled("OBJ를 불러오면 문제 목록이 표시됩니다.");
                    }
                    else if (g_issues.empty()) {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1), "심각한 문제 없음");
                    }
                    else {
                        if (ImGui::Button("선택 해제")) {
                            g_selectedFace = -1;
                        }
                        ImGui::SameLine();
                        ImGui::TextDisabled("행을 클릭하면 해당 면으로 이동합니다.");

                        if (ImGui::BeginTable("issueTable", 3,
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
                            ImGui::TableSetupScrollFreeze(0, 1);
                            ImGui::TableSetupColumn("종류", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                            ImGui::TableSetupColumn("삼각형 번호", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                            ImGui::TableSetupColumn("설명", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableHeadersRow();

                            ImGuiListClipper clipper;
                            clipper.Begin((int)g_issues.size());
                            while (clipper.Step()) {
                                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                                    const Issue& issue = g_issues[row];
                                    const bool flipped = (issue.type == FACE_FLIPPED);

                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();
                                    ImGui::PushID(row);
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.35f, 1.0f));
                                    bool clicked = ImGui::Selectable(
                                        flipped ? "뒤집힘" : "퇴화 삼각형",
                                        g_selectedFace == issue.face,
                                        ImGuiSelectableFlags_SpanAllColumns);
                                    ImGui::PopStyleColor();
                                    ImGui::PopID();

                                    if (clicked) {
                                        g_selectedFace = issue.face;
                                        const Triangle& t = g_mesh.triangles[issue.face];
                                        Vector3 center = (g_mesh.positions[t.v0] + g_mesh.positions[t.v1]
                                            + g_mesh.positions[t.v2]) * (1.0 / 3.0);
                                        camera.FocusOn(center);
                                    }

                                    ImGui::TableNextColumn();
                                    ImGui::Text("%d", issue.face);
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted(flipped
                                        ? "이웃 면과 방향이 어긋남 (뒤집힌 면)"
                                        : "면적이 0에 가까움");
                                }
                            }
                            ImGui::EndTable();
                        }
                    }
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("출력")) {
                    if (ImGui::Button("지우기")) {
                        g_log.clear();
                    }
                    ImGui::BeginChild("logScroll", ImVec2(0, 0));
                    for (const std::string& line : g_log) {
                        ImGui::TextUnformatted(line.c_str());
                    }
                    if (g_logScrollToBottom) {
                        ImGui::SetScrollHereY(1.0f);
                        g_logScrollToBottom = false;
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        ImGui::SetNextWindowPos(ImVec2(0.0f, bodyBottom));
        ImGui::SetNextWindowSize(ImVec2(displaySize.x, statusHeight));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.38f, 0.68f, 1.0f));
        ImGui::Begin("##statusBar", nullptr, kStatusFlags);
        if (g_meshLoaded) {
            ImGui::Text("로드됨: %s    정점 %zu    삼각형 %zu    문제 %zu",
                g_meshPathBuf, g_mesh.positions.size(), g_mesh.triangles.size(), g_issues.size());
        }
        else {
            ImGui::Text("준비");
        }
        ImGui::End();
        ImGui::PopStyleColor();

        Vector3 a(aVals[0], aVals[1], aVals[2]);
        Vector3 b(bVals[0], bVals[1], bVals[2]);
        Vector3 cross = a.Cross(b);

        std::vector<float> lines;
        Vector3 origin(0, 0, 0);
        Vector3 gray(0.5, 0.5, 0.5);

        float axisLen = camera.GetDistance() * 1.5f;
        if (g_showAxes) {
            PushLine(lines, Vector3(-axisLen, 0, 0), Vector3(axisLen, 0, 0), gray);
            PushLine(lines, Vector3(0, -axisLen, 0), Vector3(0, axisLen, 0), gray);
            PushLine(lines, Vector3(0, 0, -axisLen), Vector3(0, 0, axisLen), gray);
        }

        if (g_showGrid) {
            float rawSpacing = axisLen / 20.0f;
            float magnitude = std::pow(10.0f, std::floor(std::log10(rawSpacing)));
            float residual = rawSpacing / magnitude;
            float spacing;
            if (residual < 1.5f)      spacing = 1.0f * magnitude;
            else if (residual < 3.5f) spacing = 2.0f * magnitude;
            else if (residual < 7.5f) spacing = 5.0f * magnitude;
            else                      spacing = 10.0f * magnitude;

            Vector3 gridColor(0.22, 0.22, 0.26);
            PushGrid(lines, axisLen, spacing, gridColor);
        }

        if (g_showVectors) {
            PushLine(lines, origin, a, Vector3(1, 0.2, 0.2));
            PushLine(lines, origin, b, Vector3(0.2, 0.4, 1));
            PushLine(lines, origin, cross, Vector3(0.2, 1, 0.3));
        }

        if (g_meshLoaded && g_showMesh && g_showProblems) {
            const Vector3 red(1.0, 0.1, 0.1);
            const Vector3 yellow(1.0, 0.9, 0.1);
            for (size_t i = 0; i < g_mesh.triangles.size() && i < g_diag.faceFlags.size(); i++) {
                if (g_diag.faceFlags[i] != FACE_DEGENERATE) continue;
                const Triangle& t = g_mesh.triangles[i];
                const Vector3& p0 = g_mesh.positions[t.v0];
                const Vector3& p1 = g_mesh.positions[t.v1];
                const Vector3& p2 = g_mesh.positions[t.v2];
                const Vector3& lineColor = ((int)i == g_selectedFace) ? yellow : red;
                PushLine(lines, p0, p1, lineColor);
                PushLine(lines, p1, p2, lineColor);
                PushLine(lines, p2, p0, lineColor);
            }
        }

        if (g_showSurface && g_surfRecompile) {
            std::string err;
            if (g_surfModel.Compile(g_surfInput, err)) {
                g_surfError.clear();
                g_surfDirty = true;
                if (g_surfNeedResetPoint) {
                    ResetSurfacePoint();
                    g_surfNeedResetPoint = false;
                }
            }
            else {
                g_surfError = err;
            }
            g_surfRecompile = false;
        }
        if (g_showSurface && g_surfModel.IsReady() && g_surfDirty) {
            g_surfMesh = BuildSurface(g_surfModel, g_surfInput.params);
            bool undefinedPart = false;
            g_surfIntegral = SurfaceIntegral(g_surfModel, g_surfInput.params, &undefinedPart);
            g_surfIntegralUndefined = undefinedPart;
            g_surfDirty = false;
        }
        if (g_showSurface && g_surfModel.IsReady()) {
            UpdateSurfacePoint();
        }

        if (g_showSurface && g_surfWire && !g_surfMesh.Empty()) {
            const Vector3 wireColor(0.04, 0.05, 0.07);
            for (size_t k = 0; k + 1 < g_surfMesh.wire.size(); k += 2) {
                PushLine(lines,
                    ToView(g_surfMesh.points[(size_t)g_surfMesh.wire[k]]),
                    ToView(g_surfMesh.points[(size_t)g_surfMesh.wire[k + 1]]),
                    wireColor);
            }
        }

        if (g_showSurface && g_surfShowTangent && g_surfModel.IsReady() && !g_surfMesh.Empty()) {
            const SurfacePoint& pt = g_surfPoint;
            const Vector3 un = pt.Pu.Normalize();
            const Vector3 vn = pt.Pv.Normalize();
            const Vector3 nn = pt.N.Normalize();
            const double ext = g_surfMesh.extent;
            const double half = 0.10 * ext;
            const Vector3 c00 = pt.P + un * (-half) + vn * (-half);
            const Vector3 c10 = pt.P + un * half + vn * (-half);
            const Vector3 c11 = pt.P + un * half + vn * half;
            const Vector3 c01 = pt.P + un * (-half) + vn * half;
            const Vector3 patchColor(0.95, 0.85, 0.3);
            PushLine(lines, ToView(c00), ToView(c10), patchColor);
            PushLine(lines, ToView(c10), ToView(c11), patchColor);
            PushLine(lines, ToView(c11), ToView(c01), patchColor);
            PushLine(lines, ToView(c01), ToView(c00), patchColor);
            PushLine(lines, ToView(pt.P), ToView(pt.P + un * (0.18 * ext)), Vector3(1.0, 0.2, 0.2));
            PushLine(lines, ToView(pt.P), ToView(pt.P + vn * (0.18 * ext)), Vector3(0.2, 0.4, 1.0));
            PushLine(lines, ToView(pt.P), ToView(pt.P + nn * (0.25 * ext)), Vector3(0.2, 1.0, 0.3));
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, lines.size() * sizeof(float), lines.data(), GL_DYNAMIC_DRAW);

        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float scaleX = (displaySize.x > 0.0f) ? (float)w / displaySize.x : 1.0f;
        const float scaleY = (displaySize.y > 0.0f) ? (float)h / displaySize.y : 1.0f;
        const float sceneBottomY = bodyBottom - bottomHeight;
        int vpX = (int)(leftWidth * scaleX);
        int vpY = (int)((displaySize.y - sceneBottomY) * scaleY);
        int vpW = (int)((displaySize.x - leftWidth) * scaleX);
        int vpH = (int)((sceneBottomY - menuHeight) * scaleY);
        if (vpW < 1) vpW = 1;
        if (vpH < 1) vpH = 1;
        glViewport(vpX, vpY, vpW, vpH);

        float proj[16], view[16], viewProj[16];
        mat4::perspective(1.0f, (float)vpW / (float)vpH, 0.1f, 100.0f, proj);
        mat4::lookAt(camera.GetEyePosition(), camera.GetTarget(), camera.GetUp(), view);
        mat4::multiply(proj, view, viewProj);

        glUseProgram(program);
        glUniformMatrix4fv(uViewProjLoc, 1, GL_FALSE, viewProj);

        glBindVertexArray(vao);
        glDrawArrays(GL_LINES, 0, (GLsizei)(lines.size() / 6));

        if (g_meshLoaded && g_showMesh) {
            static int cacheVersion = -1, cacheSel = -2;
            static bool cacheProblems = false, cacheBack = false;
            static Vector3 cacheEye(0, 0, 0);
            static GLsizei cacheCount = 0;
            const Vector3 eyeNow = camera.GetEyePosition();
            bool eyeChanged = g_showBackface && (eyeNow - cacheEye).Length() > 1e-9;
            if (cacheVersion != g_meshVersion || cacheSel != g_selectedFace ||
                cacheProblems != g_showProblems || cacheBack != g_showBackface || eyeChanged) {
                std::vector<float> meshVerts;
                meshVerts.reserve(g_mesh.triangles.size() * 18);
                PushMeshTriangles(meshVerts, g_mesh, eyeNow, g_diag, g_showProblems, g_showBackface, g_selectedFace);
                glBindBuffer(GL_ARRAY_BUFFER, meshVbo);
                glBufferData(GL_ARRAY_BUFFER, meshVerts.size() * sizeof(float), meshVerts.data(), GL_DYNAMIC_DRAW);
                cacheCount = (GLsizei)(meshVerts.size() / 6);
                cacheVersion = g_meshVersion; cacheSel = g_selectedFace;
                cacheProblems = g_showProblems; cacheBack = g_showBackface; cacheEye = eyeNow;
            }
            glPolygonMode(GL_FRONT_AND_BACK, g_wireframe ? GL_LINE : GL_FILL);
            glBindVertexArray(meshVao);
            glDrawArrays(GL_TRIANGLES, 0, cacheCount);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        if (g_showSurface && !g_surfMesh.Empty()) {
            std::vector<float> surfVerts;
            PushSurfaceTriangles(surfVerts, g_surfMesh, camera.GetEyePosition(), g_surfColorByHeight);

            glBindBuffer(GL_ARRAY_BUFFER, meshVbo);
            glBufferData(GL_ARRAY_BUFFER, surfVerts.size() * sizeof(float), surfVerts.data(), GL_DYNAMIC_DRAW);

            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.0f, 1.0f);
            glBindVertexArray(meshVao);
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(surfVerts.size() / 6));
            glDisable(GL_POLYGON_OFFSET_FILL);
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &meshVao);
    glDeleteBuffers(1, &meshVbo);
    glDeleteProgram(program);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}