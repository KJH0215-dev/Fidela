#include "MeshDiagnostics.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    struct EdgeRef {
        unsigned long long key;
        int face;
        bool forward;
    };

    struct Neighbor {
        int face;
        bool consistent;
    };

    std::vector<int> WeldVertices(const Mesh& mesh) {
        const size_t n = mesh.positions.size();
        std::vector<int> order(n);
        for (size_t i = 0; i < n; i++) order[i] = (int)i;
        std::sort(order.begin(), order.end(), [&](int a, int b) {
            const Vector3& pa = mesh.positions[a];
            const Vector3& pb = mesh.positions[b];
            if (pa.x != pb.x) return pa.x < pb.x;
            if (pa.y != pb.y) return pa.y < pb.y;
            if (pa.z != pb.z) return pa.z < pb.z;
            return a < b;
            });
        std::vector<int> canon(n);
        for (size_t i = 0; i < n; i++) {
            if (i > 0) {
                const Vector3& pa = mesh.positions[order[i]];
                const Vector3& pb = mesh.positions[order[i - 1]];
                if (pa.x == pb.x && pa.y == pb.y && pa.z == pb.z) {
                    canon[order[i]] = canon[order[i - 1]];
                    continue;
                }
            }
            canon[order[i]] = order[i];
        }
        return canon;
    }

    void FindFlippedFaces(const Mesh& mesh, std::vector<unsigned char>& flags, int& flippedCount) {
        const int faceCount = (int)mesh.triangles.size();
        const int vertexCount = (int)mesh.positions.size();
        std::vector<int> canon = WeldVertices(mesh);

        std::vector<EdgeRef> edges;
        edges.reserve((size_t)faceCount * 3);
        for (int f = 0; f < faceCount; f++) {
            if (flags[f] != FACE_OK) continue;
            const Triangle& t = mesh.triangles[f];
            if (t.v0 < 0 || t.v1 < 0 || t.v2 < 0 || t.v0 >= vertexCount || t.v1 >= vertexCount || t.v2 >= vertexCount) continue;
            const int v[3] = { canon[t.v0], canon[t.v1], canon[t.v2] };
            for (int k = 0; k < 3; k++) {
                const int a = v[k];
                const int b = v[(k + 1) % 3];
                if (a == b) continue;
                const unsigned long long lo = (unsigned long long)(a < b ? a : b);
                const unsigned long long hi = (unsigned long long)(a < b ? b : a);
                edges.push_back({ (lo << 32) | hi, f, a < b });
            }
        }
        std::sort(edges.begin(), edges.end(), [](const EdgeRef& x, const EdgeRef& y) { return x.key < y.key; });

        std::vector<std::vector<Neighbor>> adjacency((size_t)faceCount);
        std::vector<unsigned char> hasOpenEdge((size_t)faceCount, 0);
        for (size_t i = 0; i < edges.size();) {
            size_t j = i;
            while (j < edges.size() && edges[j].key == edges[i].key) j++;
            const size_t count = j - i;
            if (count == 1) {
                hasOpenEdge[edges[i].face] = 1;
            }
            else if (count == 2) {
                const EdgeRef& e0 = edges[i];
                const EdgeRef& e1 = edges[i + 1];
                const bool consistent = (e0.forward != e1.forward);
                adjacency[e0.face].push_back({ e1.face, consistent });
                adjacency[e1.face].push_back({ e0.face, consistent });
            }
            i = j;
        }

        std::vector<signed char> sign((size_t)faceCount, 0);
        std::vector<int> queue;
        for (int seed = 0; seed < faceCount; seed++) {
            if (sign[seed] != 0 || flags[seed] != FACE_OK || adjacency[seed].empty()) continue;

            queue.clear();
            queue.push_back(seed);
            sign[seed] = 1;
            for (size_t head = 0; head < queue.size(); head++) {
                const int f = queue[head];
                for (const Neighbor& nb : adjacency[f]) {
                    if (sign[nb.face] != 0) continue;
                    sign[nb.face] = nb.consistent ? sign[f] : (signed char)(-sign[f]);
                    queue.push_back(nb.face);
                }
            }

            const Vector3 origin = mesh.positions[mesh.triangles[seed].v0];
            double volume = 0.0;
            long long signSum = 0;
            bool closed = true;
            Vector3 lo = origin, hi = origin;
            for (int f : queue) {
                const Triangle& t = mesh.triangles[f];
                const Vector3 a = mesh.positions[t.v0] - origin;
                const Vector3 b = mesh.positions[t.v1] - origin;
                const Vector3 c = mesh.positions[t.v2] - origin;
                volume += sign[f] * a.Dot(b.Cross(c)) / 6.0;
                signSum += sign[f];
                if (hasOpenEdge[f]) closed = false;
                const Vector3& p = mesh.positions[t.v0];
                if (p.x < lo.x) lo.x = p.x;
                if (p.y < lo.y) lo.y = p.y;
                if (p.z < lo.z) lo.z = p.z;
                if (p.x > hi.x) hi.x = p.x;
                if (p.y > hi.y) hi.y = p.y;
                if (p.z > hi.z) hi.z = p.z;
            }
            const Vector3 size = hi - lo;
            double extent = size.x;
            if (size.y > extent) extent = size.y;
            if (size.z > extent) extent = size.z;

            int outwardSign = (signSum >= 0) ? 1 : -1;
            if (closed && std::fabs(volume) > 1e-9 * extent * extent * extent) {
                outwardSign = (volume > 0.0) ? 1 : -1;
            }

            for (int f : queue) {
                if (sign[f] != outwardSign) {
                    flags[f] = FACE_FLIPPED;
                    flippedCount++;
                }
            }
        }
    }
}

MeshDiagnostics AnalyzeMesh(const Mesh& mesh) {
    MeshDiagnostics diag;
    diag.totalFaces = (int)mesh.triangles.size();
    diag.faceFlags.assign(mesh.triangles.size(), FACE_OK);

    if (mesh.positions.empty()) {
        return diag;
    }

    Vector3 minP = mesh.positions[0];
    Vector3 maxP = mesh.positions[0];
    for (const Vector3& p : mesh.positions) {
        if (p.x < minP.x) minP.x = p.x;
        if (p.y < minP.y) minP.y = p.y;
        if (p.z < minP.z) minP.z = p.z;
        if (p.x > maxP.x) maxP.x = p.x;
        if (p.y > maxP.y) maxP.y = p.y;
        if (p.z > maxP.z) maxP.z = p.z;
    }
    diag.boundsMin = minP;
    diag.boundsMax = maxP;
    diag.boundsSize = maxP - minP;
    diag.maxDimension = diag.boundsSize.x;
    if (diag.boundsSize.y > diag.maxDimension) diag.maxDimension = diag.boundsSize.y;
    if (diag.boundsSize.z > diag.maxDimension) diag.maxDimension = diag.boundsSize.z;

    diag.scaleWarningTooSmall = diag.maxDimension < 0.01;
    diag.scaleWarningTooLarge = diag.maxDimension > 1000.0;

    const double degenerateAreaEpsilon = 1e-8;
    const int vertexCount = (int)mesh.positions.size();

    for (size_t i = 0; i < mesh.triangles.size(); i++) {
        const Triangle& tri = mesh.triangles[i];
        if (tri.v0 < 0 || tri.v1 < 0 || tri.v2 < 0 || tri.v0 >= vertexCount || tri.v1 >= vertexCount || tri.v2 >= vertexCount) {
            continue;
        }
        const Vector3& p0 = mesh.positions[tri.v0];
        const Vector3& p1 = mesh.positions[tri.v1];
        const Vector3& p2 = mesh.positions[tri.v2];

        double area = (p1 - p0).Cross(p2 - p0).Length() * 0.5;
        if (area < degenerateAreaEpsilon) {
            diag.degenerateFaces++;
            diag.faceFlags[i] = FACE_DEGENERATE;
        }
    }

    FindFlippedFaces(mesh, diag.faceFlags, diag.suspectedFlippedFaces);

    return diag;
}