// ---------------------------------------------------------------------------
//  PSPulse Editor - free / orbit viewport camera
// ---------------------------------------------------------------------------

#include "Camera.h"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace pspeditor {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDeg2Rad = kPi / 180.0f;
} // namespace

glm::vec3 EditorCamera::Forward() const {
    float yaw = yaw_ * kDeg2Rad;
    float pitch = pitch_ * kDeg2Rad;
    return glm::normalize(glm::vec3(std::cos(pitch) * std::sin(yaw), std::sin(pitch),
                                    -std::cos(pitch) * std::cos(yaw)));
}

glm::vec3 EditorCamera::Right() const {
    glm::vec3 forward = Forward();
    return glm::normalize(glm::cross(forward, Up()));
}

void EditorCamera::SetPose(const glm::vec3& position, float yawDeg, float pitchDeg) {
    position_ = position;
    yaw_ = yawDeg;
    pitch_ = pitchDeg;
}

void EditorCamera::Update(float dt) {
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;

    float speed = moveFast_ ? 16.0f : 6.0f;
    glm::vec3 delta(0.0f);
    glm::vec3 forward = Forward();
    glm::vec3 right = Right();
    if (moveForward_) delta += forward;
    if (moveBack_) delta -= forward;
    if (moveRight_) delta += right;
    if (moveLeft_) delta -= right;
    if (moveUp_) delta += Up();
    if (moveDown_) delta -= Up();
    if (glm::length(delta) > 0.0f) {
        position_ += glm::normalize(delta) * speed * dt;
        orbitDistance_ = glm::length(position_ - focus_);
    }
}

void EditorCamera::HandleMouseDelta(float dx, float dy, bool rightDown, bool middleDown) {
    if (rightDown) {
        yaw_ += dx * 0.22f;
        pitch_ -= dy * 0.22f;
        pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);
    } else if (middleDown) {
        // Orbit around the focus point.
        yaw_ += dx * 0.30f;
        pitch_ -= dy * 0.30f;
        pitch_ = glm::clamp(pitch_, -89.0f, 89.0f);
        float yaw = yaw_ * kDeg2Rad;
        float pitch = pitch_ * kDeg2Rad;
        orbitDistance_ = glm::max(orbitDistance_, 1.5f);
        position_ = focus_ + glm::vec3(std::cos(pitch) * std::sin(yaw), -std::sin(pitch),
                                       -std::cos(pitch) * std::cos(yaw)) * -orbitDistance_;
    }
}

void EditorCamera::HandleScroll(float scroll) {
    glm::vec3 forward = Forward();
    float step = scroll * (moveFast_ ? 2.4f : 1.1f);
    position_ += forward * step;
    orbitDistance_ = glm::max(glm::length(position_ - focus_), 0.5f);
}

void EditorCamera::FocusOn(const glm::vec3& target) {
    glm::vec3 direction = glm::normalize(position_ - target);
    focus_ = target;
    orbitDistance_ = glm::clamp(glm::length(position_ - target), 3.0f, 30.0f);
    position_ = target + direction * orbitDistance_;
}

CameraView EditorCamera::BuildView(float nearPlane, float farPlane) const {
    CameraView view;
    view.position = position_;
    view.view = glm::lookAt(position_, position_ + Forward(), Up());
    view.projection = glm::perspective(fov_ * kDeg2Rad, aspect_ > 0.01f ? aspect_ : 1.6f, nearPlane,
                                       farPlane);
    return view;
}

void EditorCamera::SetMoveInput(bool forward, bool back, bool left, bool right, bool up, bool down,
                                bool fast) {
    moveForward_ = forward;
    moveBack_ = back;
    moveLeft_ = left;
    moveRight_ = right;
    moveUp_ = up;
    moveDown_ = down;
    moveFast_ = fast;
}

} // namespace pspeditor
