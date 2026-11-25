#pragma once
#include <vector>
#include "Vector3.h"

struct Triangle {
    int v0, v1, v2;
    int n0, n1, n2;
};
struct Mesh {
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Triangle> triangles;

    bool HasNormals() const { return !normals.empty(); }
};