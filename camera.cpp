#include "Camera.h"
#include <cmath>

Camera::Camera()
    : yaw(0.785398f)
    , pitch(0.615479f)
    , distance(6.0f)
    , target(0.0, 0.0, 0.0)
    , up(0.0, 1.0, 0.0)
{
}

void Camera::ProcessMouseDrag(float dx, float dy) {
    const float sensitivity = 0.01f;
    yaw += dx * sensitivity;
    pitch += dy * sensitivity;

    const float limit = 1.5f;
    if (pitch > limit) pitch = limit;
    if (pitch < -limit) pitch = -limit;
}

void Camera::ProcessPan(float dx, float dy) {
    const Vector3 forward = (target - GetEyePosition()).Normalize();
    const Vector3 right = forward.Cross(up).Normalize();
    const Vector3 trueUp = right.Cross(forward).Normalize();
    const double scale = distance * 0.0015;
    target = target + right * (-dx * scale) + trueUp * (dy * scale);
}

void Camera::ProcessScroll(float yOffset) {
    distance -= yOffset * 0.5f;
    if (distance < 1.5f) distance = 1.5f;
    if (distance > 40.0f) distance = 40.0f;
}

void Camera::Reset() {
    yaw = 0.785398f;
    pitch = 0.615479f;
    distance = 6.0f;
    target = Vector3(0.0, 0.0, 0.0);
}

void Camera::FocusOn(const Vector3& point) {
    target = point;
}

Vector3 Camera::GetEyePosition() const {
    double x = distance * std::cos(pitch) * std::sin(yaw);
    double y = distance * std::sin(pitch);
    double z = distance * std::cos(pitch) * std::cos(yaw);
    return target + Vector3(x, y, z);
}

Vector3 Camera::GetTarget() const { return target; }
Vector3 Camera::GetUp() const { return up; }
float Camera::GetDistance() const { return distance; }