#pragma once
#include "Vector3.h"

class Camera {
public:
    Camera();
    void ProcessMouseDrag(float dx, float dy);
    void ProcessPan(float dx, float dy);
    void ProcessScroll(float yOffset);
    void Reset();
    void FocusOn(const Vector3& point);

    Vector3 GetEyePosition() const;
    Vector3 GetTarget() const;
    Vector3 GetUp() const;
    float GetDistance() const;

private:
    float yaw;
    float pitch;
    float distance;
    Vector3 target;
    Vector3 up;
};