#pragma once
#include <string>
#include <vector>
#include "Vector3.h"
#include "Expression.h"

enum SurfaceMode {
    SURFMODE_EXPLICIT = 0,
    SURFMODE_PARAMETRIC = 1,
    SURFMODE_IMPLICIT = 2
};

struct SurfaceParams {
    float a = 1.0f;
    float b = 1.0f;
    float c = 1.0f;
    float range = 2.0f;
    int resolution = 40;
};

struct SurfaceInput {
    int mode = SURFMODE_EXPLICIT;
    char exprZ[256] = "a*x^2 + b*y^2";
    char exprPX[256] = "";
    char exprPY[256] = "";
    char exprPZ[256] = "";
    char exprF[256] = "";
    char uMin[32] = "0";
    char uMax[32] = "2*pi";
    char vMin[32] = "0";
    char vMax[32] = "pi";
    SurfaceParams params;
};

int SurfacePresetCount();
const char* SurfacePresetName(int index);
SurfaceInput SurfacePreset(int index);
SurfaceInput SurfaceDefaultFor(int mode);

class SurfaceModel {
public:
    bool Compile(const SurfaceInput& input, std::string& errorMessage);

    bool IsReady() const { return ready_; }
    int Mode() const { return mode_; }

    Vector3 Eval(const SurfaceParams& p, double u, double v) const;
    double EvalF(const SurfaceParams& p, double x, double y, double z) const;
    void Domain(const SurfaceParams& p, double& u0, double& u1, double& v0, double& v1) const;

private:
    bool ready_ = false;
    int mode_ = SURFMODE_EXPLICIT;
    Expression ez_, px_, py_, pz_, f_;
    double u0_ = 0.0, u1_ = 1.0, v0_ = 0.0, v1_ = 1.0;
};

struct SurfaceMesh {
    std::vector<Vector3> points;
    std::vector<Vector3> normals;
    std::vector<int> indices;
    std::vector<int> wire;
    double zMin = 0.0;
    double zMax = 0.0;
    double extent = 1.0;
    bool Empty() const { return indices.empty(); }
};

SurfaceMesh BuildSurface(const SurfaceModel& model, const SurfaceParams& p);

double SurfaceIntegral(const SurfaceModel& model, const SurfaceParams& p, bool* hadUndefined);

Vector3 SurfaceGradient(const SurfaceModel& model, const SurfaceParams& p, const Vector3& at);

bool ExportSurfaceObj(const SurfaceMesh& mesh, const std::string& path, std::string& errorMessage);