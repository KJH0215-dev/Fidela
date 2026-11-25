#pragma once
#include <string>
#include "Mesh.h"

bool LoadObj(const std::string& path, Mesh& outMesh, std::string& errorMessage);