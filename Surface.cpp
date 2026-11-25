#include "Surface.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace {

    const double kLimit = 1.0e4;

    bool IsGood(const Vector3& p) {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
            std::fabs(p.x) < kLimit && std::fabs(p.y) < kLimit && std::fabs(p.z) < kLimit;
    }

    std::string Trim(const std::string& s) {
        size_t a = 0, b = s.size();
        while (a < b && std::isspace((unsigned char)s[a])) a++;
        while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
        return s.substr(a, b - a);
    }

    std::string Lower(std::string s) {
        for (char& c : s) c = (char)std::tolower((unsigned char)c);
        return s;
    }

    struct Preset {
        const char* name;
        int mode;
        const char* e1;
        const char* e2;
        const char* e3;
        const char* uMin; const char* uMax; const char* vMin; const char* vMax;
        float a, b, c;
        float range;
        int resolution;
    };

    const Preset kPresets[] = {
        {"[z=f] 포물면", SURFMODE_EXPLICIT, "a*x^2 + b*y^2", "", "", "", "", "", "", 0.5f, 0.5f, 1.0f, 2.0f, 40},
        {"[z=f] 안장면", SURFMODE_EXPLICIT, "a*x^2 - b*y^2", "", "", "", "", "", "", 0.5f, 0.5f, 1.0f, 2.0f, 40},
        {"[z=f] 원뿔", SURFMODE_EXPLICIT, "c*sqrt(a*x^2 + b*y^2)", "", "", "", "", "", "", 1.0f, 1.0f, 1.0f, 2.0f, 40},
        {"[z=f] 사인-코사인 파동", SURFMODE_EXPLICIT, "c*sin(a*x)*cos(b*y)", "", "", "", "", "", "", 1.0f, 1.0f, 1.0f, 3.0f, 60},
        {"[z=f] 가우시안 언덕", SURFMODE_EXPLICIT, "c*exp(-(a*x^2 + b*y^2))", "", "", "", "", "", "", 1.0f, 1.0f, 2.0f, 2.5f, 50},
        {"[z=f] 봉우리와 골짜기", SURFMODE_EXPLICIT, "x*y*exp(-(x^2 + y^2))", "", "", "", "", "", "", 1.0f, 1.0f, 1.0f, 2.5f, 60},
        {"[z=f] 반구 (정의역 밖은 비움)", SURFMODE_EXPLICIT, "sqrt(c^2 - x^2 - y^2)", "", "", "", "", "", "", 1.0f, 1.0f, 1.5f, 2.0f, 60},
        {"[z=f] 평면", SURFMODE_EXPLICIT, "a*x + b*y + c", "", "", "", "", "", "", 0.5f, 0.5f, 0.0f, 2.0f, 20},
        {"[매개변수] 타원체 (a=b=c 이면 구)", SURFMODE_PARAMETRIC,
            "a*sin(u)*cos(v)", "b*sin(u)*sin(v)", "c*cos(u)", "0", "pi", "0", "2*pi", 1.5f, 1.5f, 1.5f, 2.0f, 40},
        {"[매개변수] 원환면 (도넛)", SURFMODE_PARAMETRIC,
            "(a + b*cos(v))*cos(u)", "(a + b*cos(v))*sin(u)", "b*sin(v)", "0", "2*pi", "0", "2*pi", 2.0f, 0.7f, 1.0f, 2.0f, 60},
        {"[매개변수] 원기둥", SURFMODE_PARAMETRIC,
            "a*cos(u)", "a*sin(u)", "c*v", "0", "2*pi", "-1", "1", 1.2f, 1.0f, 2.0f, 2.0f, 40},
        {"[매개변수] 나선면 (헬리코이드)", SURFMODE_PARAMETRIC,
            "v*cos(u)", "v*sin(u)", "c*u", "0", "4*pi", "-2", "2", 1.0f, 1.0f, 0.3f, 2.0f, 60},
        {"[매개변수] 뫼비우스 띠", SURFMODE_PARAMETRIC,
            "(2 + v*cos(u/2))*cos(u)", "(2 + v*cos(u/2))*sin(u)", "v*sin(u/2)", "0", "2*pi", "-0.8", "0.8", 1.0f, 1.0f, 1.0f, 2.0f, 60},
        {"[음함수] 구 / 타원체", SURFMODE_IMPLICIT, "x^2/a^2 + y^2/b^2 + z^2/c^2 = 1", "", "", "", "", "", "", 1.5f, 1.5f, 1.5f, 2.5f, 32},
        {"[음함수] 1엽 쌍곡면", SURFMODE_IMPLICIT, "x^2/a^2 + y^2/b^2 - z^2/c^2 = 1", "", "", "", "", "", "", 1.0f, 1.0f, 1.0f, 3.0f, 36},
        {"[음함수] 2엽 쌍곡면", SURFMODE_IMPLICIT, "z^2/c^2 - x^2/a^2 - y^2/b^2 = 1", "", "", "", "", "", "", 1.0f, 1.0f, 1.0f, 3.0f, 36},
        {"[음함수] 원뿔 (양쪽)", SURFMODE_IMPLICIT, "x^2 + y^2 = c*z^2", "", "", "", "", "", "", 1.0f, 1.0f, 1.0f, 2.0f, 36},
        {"[음함수] 원환면", SURFMODE_IMPLICIT, "(sqrt(x^2 + y^2) - a)^2 + z^2 = b^2", "", "", "", "", "", "", 2.0f, 0.7f, 1.0f, 3.0f, 48},
        {"[음함수] 자이로이드", SURFMODE_IMPLICIT, "sin(x)*cos(y) + sin(y)*cos(z) + sin(z)*cos(x) = 0", "", "", "", "", "", "", 1.0f, 1.0f, 1.0f, 3.2f, 48},
        {"[음함수] 하트", SURFMODE_IMPLICIT, "(x^2 + 9/4*y^2 + z^2 - 1)^3 - x^2*z^3 - 9/80*y^2*z^3 = 0", "", "", "", "", "", "", 1.0f, 1.0f, 1.0f, 1.5f, 48},
    };
    const int kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

    void CopyText(char* dst, size_t cap, const char* src) {
        size_t i = 0;
        while (i + 1 < cap && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
        dst[i] = '\0';
    }

    bool StripLhs(const std::string& text, const char* lhsName, std::string& body, std::string& err) {
        const size_t eq = text.find('=');
        if (eq == std::string::npos) {
            body = text;
            return true;
        }
        if (text.find('=', eq + 1) != std::string::npos) {
            err = "식 오류: '=' 는 하나만 쓸 수 있어요";
            return false;
        }
        const std::string lhs = Lower(Trim(text.substr(0, eq)));
        if (lhs != lhsName) {
            err = std::string("식 오류: '=' 왼쪽은 ") + lhsName + " 이어야 해요";
            return false;
        }
        body = text.substr(eq + 1);
        return true;
    }

    bool CompileRange(const char* text, double& out, std::string& err, const char* label) {
        Expression e;
        std::string msg;
        if (!e.Compile(text, std::vector<std::string>(), msg)) {
            err = std::string(label) + " 범위: " + msg;
            return false;
        }
        out = e.Eval(nullptr);
        if (!std::isfinite(out)) {
            err = std::string(label) + " 범위 값이 올바르지 않아요";
            return false;
        }
        return true;
    }

    const std::vector<std::string>& VarsXY() {
        static const std::vector<std::string> v = { "x", "y", "a", "b", "c" };
        return v;
    }
    const std::vector<std::string>& VarsUV() {
        static const std::vector<std::string> v = { "u", "v", "a", "b", "c" };
        return v;
    }
    const std::vector<std::string>& VarsXYZ() {
        static const std::vector<std::string> v = { "x", "y", "z", "a", "b", "c" };
        return v;
    }

    void ComputeExtent(SurfaceMesh& m) {
        if (m.indices.empty()) { m.extent = 1.0; return; }
        Vector3 lo(1e300, 1e300, 1e300), hi(-1e300, -1e300, -1e300);
        for (int idx : m.indices) {
            const Vector3& q = m.points[(size_t)idx];
            lo.x = std::min(lo.x, q.x); lo.y = std::min(lo.y, q.y); lo.z = std::min(lo.z, q.z);
            hi.x = std::max(hi.x, q.x); hi.y = std::max(hi.y, q.y); hi.z = std::max(hi.z, q.z);
        }
        m.extent = std::max(hi.x - lo.x, std::max(hi.y - lo.y, hi.z - lo.z));
        if (!(m.extent > 1e-6)) m.extent = 1.0;
    }

    SurfaceMesh BuildGrid(const SurfaceModel& model, const SurfaceParams& p) {
        SurfaceMesh m;
        const int cells = std::max(4, std::min(p.resolution, 200));
        const int n = cells + 1;

        double u0, u1, v0, v1;
        model.Domain(p, u0, u1, v0, v1);

        m.points.resize((size_t)n * n);
        std::vector<char> good((size_t)n * n, 0);
        for (int i = 0; i < n; i++) {
            const double u = u0 + (u1 - u0) * (double)i / (double)cells;
            for (int j = 0; j < n; j++) {
                const double v = v0 + (v1 - v0) * (double)j / (double)cells;
                const size_t k = (size_t)i * n + j;
                m.points[k] = model.Eval(p, u, v);
                good[k] = IsGood(m.points[k]) ? 1 : 0;
            }
        }

        m.normals.assign(m.points.size(), Vector3(0.0, 0.0, 0.0));
        auto addTri = [&](int a, int b, int c) {
            if (!good[a] || !good[b] || !good[c]) return;
            m.indices.push_back(a); m.indices.push_back(b); m.indices.push_back(c);
            const Vector3 fn = (m.points[b] - m.points[a]).Cross(m.points[c] - m.points[a]);
            m.normals[a] = m.normals[a] + fn;
            m.normals[b] = m.normals[b] + fn;
            m.normals[c] = m.normals[c] + fn;
            };
        for (int i = 0; i < cells; i++) {
            for (int j = 0; j < cells; j++) {
                const int p00 = i * n + j;
                const int p10 = (i + 1) * n + j;
                const int p11 = (i + 1) * n + (j + 1);
                const int p01 = i * n + (j + 1);
                addTri(p00, p10, p11);
                addTri(p00, p11, p01);
            }
        }
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                const int k = i * n + j;
                if (!good[k]) continue;
                if (i + 1 < n && good[k + n]) { m.wire.push_back(k); m.wire.push_back(k + n); }
                if (j + 1 < n && good[k + 1]) { m.wire.push_back(k); m.wire.push_back(k + 1); }
            }
        }
        for (Vector3& nrm : m.normals) nrm = nrm.Normalize();

        m.zMin = std::numeric_limits<double>::max();
        m.zMax = -std::numeric_limits<double>::max();
        for (size_t k = 0; k < m.points.size(); k++) {
            if (!good[k]) continue;
            m.zMin = std::min(m.zMin, m.points[k].z);
            m.zMax = std::max(m.zMax, m.points[k].z);
        }
        if (m.zMin > m.zMax) { m.zMin = 0.0; m.zMax = 0.0; }
        ComputeExtent(m);
        return m;
    }

    SurfaceMesh BuildImplicit(const SurfaceModel& model, const SurfaceParams& p) {
        SurfaceMesh m;
        const int cells = std::max(8, std::min(p.resolution, 64));
        const int n = cells + 1;
        const double R = std::max(0.1, (double)p.range);
        const double step = 2.0 * R / cells;

        std::vector<double> field((size_t)n * n * n);
        auto cid = [n](int ix, int iy, int iz) { return ((size_t)iz * n + iy) * n + ix; };
        for (int iz = 0; iz < n; iz++) {
            for (int iy = 0; iy < n; iy++) {
                for (int ix = 0; ix < n; ix++) {
                    field[cid(ix, iy, iz)] = model.EvalF(p, -R + ix * step, -R + iy * step, -R + iz * step);
                }
            }
        }

        static const int kCorner[8][3] = {
            {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}, {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1}
        };
        static const int kTets[6][4] = {
            {0,5,1,6}, {0,1,2,6}, {0,2,3,6}, {0,3,7,6}, {0,7,4,6}, {0,4,5,6}
        };

        std::unordered_map<uint64_t, int> edgeVertex;
        auto vertexOnEdge = [&](size_t idA, size_t idB, const Vector3& pa, const Vector3& pb, double fa, double fb) {
            const uint64_t lo = (uint64_t)std::min(idA, idB);
            const uint64_t hi = (uint64_t)std::max(idA, idB);
            const uint64_t key = (lo << 32) | hi;
            auto it = edgeVertex.find(key);
            if (it != edgeVertex.end()) return it->second;
            const double t = fa / (fa - fb);
            const int index = (int)m.points.size();
            m.points.push_back(pa + (pb - pa) * t);
            edgeVertex[key] = index;
            return index;
            };

        for (int iz = 0; iz < cells; iz++) {
            for (int iy = 0; iy < cells; iy++) {
                for (int ix = 0; ix < cells; ix++) {
                    double f[8];
                    size_t id[8];
                    Vector3 pos[8];
                    bool anyNeg = false, anyPos = false, bad = false;
                    for (int c = 0; c < 8; c++) {
                        const int cx = ix + kCorner[c][0], cy = iy + kCorner[c][1], cz = iz + kCorner[c][2];
                        id[c] = cid(cx, cy, cz);
                        f[c] = field[id[c]];
                        pos[c] = Vector3(-R + cx * step, -R + cy * step, -R + cz * step);
                        if (!std::isfinite(f[c])) bad = true;
                        else if (f[c] < 0.0) anyNeg = true;
                        else anyPos = true;
                    }
                    if (bad || !anyNeg || !anyPos) continue;

                    for (int t = 0; t < 6; t++) {
                        int inside[4], outside[4];
                        int nin = 0, nout = 0;
                        for (int k = 0; k < 4; k++) {
                            const int c = kTets[t][k];
                            if (f[c] < 0.0) inside[nin++] = c;
                            else outside[nout++] = c;
                        }
                        if (nin == 0 || nout == 0) continue;

                        auto edge = [&](int ci, int co) {
                            return vertexOnEdge(id[ci], id[co], pos[ci], pos[co], f[ci], f[co]);
                            };

                        int quad[4];
                        int count = 0;
                        if (nin == 1) {
                            quad[0] = edge(inside[0], outside[0]);
                            quad[1] = edge(inside[0], outside[1]);
                            quad[2] = edge(inside[0], outside[2]);
                            count = 3;
                        }
                        else if (nin == 3) {
                            quad[0] = edge(inside[0], outside[0]);
                            quad[1] = edge(inside[1], outside[0]);
                            quad[2] = edge(inside[2], outside[0]);
                            count = 3;
                        }
                        else {
                            quad[0] = edge(inside[0], outside[0]);
                            quad[1] = edge(inside[0], outside[1]);
                            quad[2] = edge(inside[1], outside[1]);
                            quad[3] = edge(inside[1], outside[0]);
                            count = 4;
                        }

                        Vector3 meanIn(0, 0, 0), meanOut(0, 0, 0);
                        for (int k = 0; k < nin; k++) meanIn = meanIn + pos[inside[k]] * (1.0 / nin);
                        for (int k = 0; k < nout; k++) meanOut = meanOut + pos[outside[k]] * (1.0 / nout);
                        const Vector3 dir = meanOut - meanIn;

                        auto emit = [&](int a, int b, int c) {
                            const Vector3 fn = (m.points[b] - m.points[a]).Cross(m.points[c] - m.points[a]);
                            if (fn.Dot(dir) < 0.0) std::swap(b, c);
                            m.indices.push_back(a); m.indices.push_back(b); m.indices.push_back(c);
                            };
                        emit(quad[0], quad[1], quad[2]);
                        if (count == 4) emit(quad[0], quad[2], quad[3]);
                    }
                }
            }
        }

        m.normals.resize(m.points.size());
        m.zMin = std::numeric_limits<double>::max();
        m.zMax = -std::numeric_limits<double>::max();
        for (size_t k = 0; k < m.points.size(); k++) {
            Vector3 g = SurfaceGradient(model, p, m.points[k]).Normalize();
            if (g.Length() < 0.5) g = Vector3(0.0, 0.0, 1.0);
            m.normals[k] = g;
            m.zMin = std::min(m.zMin, m.points[k].z);
            m.zMax = std::max(m.zMax, m.points[k].z);
        }
        if (m.zMin > m.zMax) { m.zMin = 0.0; m.zMax = 0.0; }

        for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
            for (int e = 0; e < 3; e++) {
                const int a = m.indices[t + e];
                const int b = m.indices[t + (e + 1) % 3];
                if (a < b) { m.wire.push_back(a); m.wire.push_back(b); }
            }
        }
        ComputeExtent(m);
        return m;
    }

}

int SurfacePresetCount() { return kPresetCount; }

const char* SurfacePresetName(int index) {
    if (index < 0 || index >= kPresetCount) return "";
    return kPresets[index].name;
}

SurfaceInput SurfacePreset(int index) {
    SurfaceInput in;
    if (index < 0 || index >= kPresetCount) return in;
    const Preset& p = kPresets[index];
    in.mode = p.mode;
    in.exprZ[0] = in.exprPX[0] = in.exprPY[0] = in.exprPZ[0] = in.exprF[0] = '\0';
    if (p.mode == SURFMODE_EXPLICIT) {
        CopyText(in.exprZ, sizeof(in.exprZ), p.e1);
    }
    else if (p.mode == SURFMODE_PARAMETRIC) {
        CopyText(in.exprPX, sizeof(in.exprPX), p.e1);
        CopyText(in.exprPY, sizeof(in.exprPY), p.e2);
        CopyText(in.exprPZ, sizeof(in.exprPZ), p.e3);
        CopyText(in.uMin, sizeof(in.uMin), p.uMin);
        CopyText(in.uMax, sizeof(in.uMax), p.uMax);
        CopyText(in.vMin, sizeof(in.vMin), p.vMin);
        CopyText(in.vMax, sizeof(in.vMax), p.vMax);
    }
    else {
        CopyText(in.exprF, sizeof(in.exprF), p.e1);
    }
    in.params.a = p.a;
    in.params.b = p.b;
    in.params.c = p.c;
    in.params.range = p.range;
    in.params.resolution = p.resolution;
    return in;
}

SurfaceInput SurfaceDefaultFor(int mode) {
    for (int i = 0; i < kPresetCount; i++) {
        if (kPresets[i].mode == mode) return SurfacePreset(i);
    }
    return SurfaceInput();
}

bool SurfaceModel::Compile(const SurfaceInput& input, std::string& errorMessage) {
    errorMessage.clear();

    if (input.mode == SURFMODE_EXPLICIT) {
        std::string body;
        if (!StripLhs(input.exprZ, "z", body, errorMessage)) return false;
        Expression e;
        if (!e.Compile(body, VarsXY(), errorMessage)) return false;
        ez_ = e;
    }
    else if (input.mode == SURFMODE_PARAMETRIC) {
        Expression ex, ey, ez;
        std::string err;
        if (!ex.Compile(input.exprPX, VarsUV(), err)) { errorMessage = "x(u,v): " + err; return false; }
        if (!ey.Compile(input.exprPY, VarsUV(), err)) { errorMessage = "y(u,v): " + err; return false; }
        if (!ez.Compile(input.exprPZ, VarsUV(), err)) { errorMessage = "z(u,v): " + err; return false; }
        double a0, a1, b0, b1;
        if (!CompileRange(input.uMin, a0, errorMessage, "u 최솟값")) return false;
        if (!CompileRange(input.uMax, a1, errorMessage, "u 최댓값")) return false;
        if (!CompileRange(input.vMin, b0, errorMessage, "v 최솟값")) return false;
        if (!CompileRange(input.vMax, b1, errorMessage, "v 최댓값")) return false;
        if (!(a0 < a1) || !(b0 < b1)) {
            errorMessage = "범위 오류: 최솟값은 최댓값보다 작아야 해요";
            return false;
        }
        px_ = ex; py_ = ey; pz_ = ez;
        u0_ = a0; u1_ = a1; v0_ = b0; v1_ = b1;
    }
    else {
        const std::string text = input.exprF;
        const size_t eq = text.find('=');
        std::string combined;
        if (eq == std::string::npos) {
            combined = text;
        }
        else {
            if (text.find('=', eq + 1) != std::string::npos) {
                errorMessage = "식 오류: '=' 는 하나만 쓸 수 있어요";
                return false;
            }
            const std::string lhs = Trim(text.substr(0, eq));
            const std::string rhs = Trim(text.substr(eq + 1));
            if (lhs.empty() || rhs.empty()) {
                errorMessage = "식 오류: '=' 양쪽에 식이 있어야 해요";
                return false;
            }
            combined = "(" + lhs + ") - (" + rhs + ")";
        }
        Expression e;
        if (!e.Compile(combined, VarsXYZ(), errorMessage)) return false;
        f_ = e;
    }

    mode_ = input.mode;
    ready_ = true;
    return true;
}

Vector3 SurfaceModel::Eval(const SurfaceParams& p, double u, double v) const {
    const double vars[5] = { u, v, p.a, p.b, p.c };
    if (mode_ == SURFMODE_PARAMETRIC) {
        return Vector3(px_.Eval(vars), py_.Eval(vars), pz_.Eval(vars));
    }
    return Vector3(u, v, ez_.Eval(vars));
}

double SurfaceModel::EvalF(const SurfaceParams& p, double x, double y, double z) const {
    const double vars[6] = { x, y, z, p.a, p.b, p.c };
    return f_.Eval(vars);
}

void SurfaceModel::Domain(const SurfaceParams& p, double& u0, double& u1, double& v0, double& v1) const {
    if (mode_ == SURFMODE_PARAMETRIC) {
        u0 = u0_; u1 = u1_; v0 = v0_; v1 = v1_;
    }
    else {
        u0 = v0 = -(double)p.range;
        u1 = v1 = (double)p.range;
    }
}

SurfaceMesh BuildSurface(const SurfaceModel& model, const SurfaceParams& p) {
    if (!model.IsReady()) return SurfaceMesh();
    if (model.Mode() == SURFMODE_IMPLICIT) return BuildImplicit(model, p);
    return BuildGrid(model, p);
}

double SurfaceIntegral(const SurfaceModel& model, const SurfaceParams& p, bool* hadUndefined) {
    if (hadUndefined) *hadUndefined = false;
    if (!model.IsReady() || model.Mode() != SURFMODE_EXPLICIT) return 0.0;

    double u0, u1, v0, v1;
    model.Domain(p, u0, u1, v0, v1);
    const int steps = 200;
    const double du = (u1 - u0) / steps;
    const double dv = (v1 - v0) / steps;
    double sum = 0.0;
    for (int i = 0; i < steps; i++) {
        const double u = u0 + (i + 0.5) * du;
        for (int j = 0; j < steps; j++) {
            const double v = v0 + (j + 0.5) * dv;
            const double z = model.Eval(p, u, v).z;
            if (std::isfinite(z) && std::fabs(z) < kLimit) sum += z;
            else if (hadUndefined) *hadUndefined = true;
        }
    }
    return sum * du * dv;
}

Vector3 SurfaceGradient(const SurfaceModel& model, const SurfaceParams& p, const Vector3& at) {
    const double h = 1e-4;
    const double gx = (model.EvalF(p, at.x + h, at.y, at.z) - model.EvalF(p, at.x - h, at.y, at.z)) / (2.0 * h);
    const double gy = (model.EvalF(p, at.x, at.y + h, at.z) - model.EvalF(p, at.x, at.y - h, at.z)) / (2.0 * h);
    const double gz = (model.EvalF(p, at.x, at.y, at.z + h) - model.EvalF(p, at.x, at.y, at.z - h)) / (2.0 * h);
    return Vector3(gx, gy, gz);
}

bool ExportSurfaceObj(const SurfaceMesh& mesh, const std::string& path, std::string& errorMessage) {
    if (mesh.Empty()) {
        errorMessage = "내보낼 곡면이 없습니다.";
        return false;
    }
    std::ofstream file(path);
    if (!file.is_open()) {
        errorMessage = "파일을 만들 수 없습니다: " + path;
        return false;
    }

    std::vector<int> remap(mesh.points.size(), -1);
    int next = 0;
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const Vector3& a = mesh.points[mesh.indices[t]];
        const Vector3& b = mesh.points[mesh.indices[t + 1]];
        const Vector3& c = mesh.points[mesh.indices[t + 2]];
        if ((b - a).Cross(c - a).Length() < 1e-12) continue;
        for (int k = 0; k < 3; k++) {
            const int idx = mesh.indices[t + k];
            if (remap[idx] < 0) remap[idx] = next++;
        }
    }

    file << "# surface exported by Vector Inspector (z-up)\n";
    file.setf(std::ios::fixed);
    file.precision(6);
    std::vector<int> order((size_t)next);
    for (size_t k = 0; k < remap.size(); k++) {
        if (remap[k] >= 0) order[(size_t)remap[k]] = (int)k;
    }
    for (int k = 0; k < next; k++) {
        const Vector3& pt = mesh.points[(size_t)order[(size_t)k]];
        file << "v " << pt.x << " " << pt.y << " " << pt.z << "\n";
    }
    for (size_t t = 0; t + 2 < mesh.indices.size(); t += 3) {
        const int i0 = mesh.indices[t], i1 = mesh.indices[t + 1], i2 = mesh.indices[t + 2];
        if (remap[i0] < 0 || remap[i1] < 0 || remap[i2] < 0) continue;
        const Vector3& a = mesh.points[i0];
        const Vector3& b = mesh.points[i1];
        const Vector3& c = mesh.points[i2];
        if ((b - a).Cross(c - a).Length() < 1e-12) continue;
        file << "f " << (remap[i0] + 1) << " " << (remap[i1] + 1) << " " << (remap[i2] + 1) << "\n";
    }
    return true;
}