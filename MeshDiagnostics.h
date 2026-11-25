#pragma once
#include <vector>
#include "Mesh.h"

enum FaceFlag : unsigned char {
    FACE_OK = 0,
    FACE_FLIPPED = 1,
    FACE_DEGENERATE = 2
};

struct MeshDiagnostics {
    int totalFaces = 0;
    int suspectedFlippedFaces = 0;
    int degenerateFaces = 0;

    Vector3 boundsMin;
    Vector3 boundsMax;
    Vector3 boundsSize;
    double maxDimension = 0.0;
    bool scaleWarningTooSmall = false;
    bool scaleWarningTooLarge = false;

    std::vector<unsigned char> faceFlags;
};

MeshDiagnostics AnalyzeMesh(const Mesh& mesh);