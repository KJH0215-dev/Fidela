#include "ObjParser.h"
#include <fstream>
#include <sstream>
#include <vector>

namespace {
    void ParseFaceVertex(const std::string& token, int& posIndex, int& normIndex) {
        posIndex = -1;
        normIndex = -1;

        size_t firstSlash = token.find('/');
        if (firstSlash == std::string::npos) {
            posIndex = std::stoi(token) - 1;
            return;
        }

        posIndex = std::stoi(token.substr(0, firstSlash)) - 1;

        size_t secondSlash = token.find('/', firstSlash + 1);
        if (secondSlash == std::string::npos) {
            return;
        }

        std::string normPart = token.substr(secondSlash + 1);
        if (!normPart.empty()) {
            normIndex = std::stoi(normPart) - 1;
        }
    }

}

bool LoadObj(const std::string& path, Mesh& outMesh, std::string& errorMessage) {
    std::ifstream file(path);
    if (!file.is_open()) {
        errorMessage = "파일을 열 수 없습니다: " + path;
        return false;
    }

    outMesh.positions.clear();
    outMesh.normals.clear();
    outMesh.triangles.clear();

    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        lineNumber++;
        std::istringstream iss(line);
        std::string tag;
        iss >> tag;

        if (tag == "v") {
            double x, y, z;
            iss >> x >> y >> z;
            outMesh.positions.push_back(Vector3(x, y, z));
        }
        else if (tag == "vn") {
            double x, y, z;
            iss >> x >> y >> z;
            outMesh.normals.push_back(Vector3(x, y, z));
        }
        else if (tag == "f") {
            std::vector<std::string> tokens;
            std::string tok;
            while (iss >> tok) {
                tokens.push_back(tok);
            }

            if (tokens.size() < 3) {
                errorMessage = "잘못된 f 라인 (정점 3개 미만), 줄 " + std::to_string(lineNumber);
                return false;
            }

            std::vector<int> posIdx(tokens.size());
            std::vector<int> normIdx(tokens.size());
            for (size_t i = 0; i < tokens.size(); i++) {
                ParseFaceVertex(tokens[i], posIdx[i], normIdx[i]);
            }

            for (size_t i = 1; i + 1 < tokens.size(); i++) {
                Triangle tri;
                tri.v0 = posIdx[0];     tri.v1 = posIdx[i];     tri.v2 = posIdx[i + 1];
                tri.n0 = normIdx[0];    tri.n1 = normIdx[i];    tri.n2 = normIdx[i + 1];
                outMesh.triangles.push_back(tri);
            }
        }
    }

    if (outMesh.positions.empty()) {
        errorMessage = "정점 데이터가 없습니다: " + path;
        return false;
    }

    return true;
}