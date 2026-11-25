#pragma once
#include <cmath>

class Vector3 {
public:
    double x;
    double y;
    double z;

    Vector3() : x(0.0), y(0.0), z(0.0) {}
    Vector3(double nx, double ny, double nz) : x(nx), y(ny), z(nz) {}

    double Dot(const Vector3& b) const {
        return (x * b.x) + (y * b.y) + (z * b.z);
    }

    Vector3 Cross(const Vector3& b) const {
        return Vector3(
            (y * b.z) - (z * b.y),
            (z * b.x) - (x * b.z),
            (x * b.y) - (y * b.x)
        );
    }

    double Length() const {
        return std::sqrt((x * x) + (y * y) + (z * z));
    }

    Vector3 Normalize() const {
        double len = Length();
        if (len == 0.0) {
            return Vector3(0.0, 0.0, 0.0);
        }
        return Vector3(x / len, y / len, z / len);
    }

    Vector3 operator+(const Vector3& b) const {
        return Vector3(x + b.x, y + b.y, z + b.z);
    }

    Vector3 operator-(const Vector3& b) const {
        return Vector3(x - b.x, y - b.y, z - b.z);
    }

    Vector3 operator*(double scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }
};