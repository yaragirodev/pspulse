#pragma once

// ---------------------------------------------------------------------------
//  PSPulse Editor - free / orbit viewport camera
//
//  Controls (Unity-like):
//    RMB + drag .......... look around
//    RMB + WASD / QE ..... fly (Shift = sprint)
//    MMB / Alt+LMB drag .. orbit around focus point
//    Wheel ............... dolly
//    F ................... focus the selected object
// ---------------------------------------------------------------------------

#include <glm/glm.hpp>

#include "Renderer.h"

namespace pspeditor {

class EditorCamera {
public:
    void SetAspect(float aspect) { aspect_ = aspect; }
    void Update(float dt);
    void HandleMouseDelta(float dx, float dy, bool rightDown, bool middleDown);
    void HandleScroll(float delta);
    void FocusOn(const glm::vec3& target);
    void SetPose(const glm::vec3& position, float yawDeg, float pitchDeg);

    CameraView BuildView(float nearPlane = 0.05f, float farPlane = 400.0f) const;

    glm::vec3 Position() const { return position_; }
    float Yaw() const { return yaw_; }
    float Pitch() const { return pitch_; }
    glm::vec3 Forward() const;
    glm::vec3 Right() const;
    glm::vec3 Up() const { return glm::vec3(0.0f, 1.0f, 0.0f); }

    void SetMoveInput(bool forward, bool back, bool left, bool right, bool up, bool down, bool fast);

private:
    glm::vec3 position_{9.0f, 6.5f, 13.0f};
    float yaw_ = -34.7f;    // degrees, around Y (looking at the world origin)
    float pitch_ = -21.9f;  // degrees
    float aspect_ = 16.0f / 9.0f;
    float fov_ = 55.0f;
    glm::vec3 focus_{0.0f, 1.0f, 0.0f};
    float orbitDistance_ = 16.0f;

    bool moveForward_ = false, moveBack_ = false, moveLeft_ = false, moveRight_ = false;
    bool moveUp_ = false, moveDown_ = false, moveFast_ = false;
};

} // namespace pspeditor
