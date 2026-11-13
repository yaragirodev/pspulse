#pragma once

// ---------------------------------------------------------------------------
//  PSP Engine - lightweight kinematic physics (shared by editor & PSP runtime)
//
//  The solver is intentionally small (PSP class CPU, no external deps):
//    * gravity integration for dynamic bodies,
//    * collision resolution against static colliders,
//    * box (AABB) and sphere collision shapes,
//    * restitution + friction, grounded detection (lets scripts call on_ground()).
//
//  Shapes are treated as world-space AABBs / spheres, which is plenty for a
//  blockout toolkit and keeps the PSP frame budget intact.
// ---------------------------------------------------------------------------

#include "../Scene/SceneData.h"

namespace pspengine {

struct Collider {
    int objectId = -1;
    int objectIndex = -1;
    CollisionShapeType shape = CollisionShapeType::Box;
    glm::vec3 center{0.0f};    // world space
    glm::vec3 halfExtents{0.5f};
    float radius = 0.5f;
    bool isStatic = false;
    bool isDynamic = false;
    glm::vec3 velocity{0.0f};
    float restitution = 0.2f;
    float friction = 0.6f;
    float mass = 1.0f;
};

class PhysicsWorld {
public:
    // Rebuilds the collider list from the scene (call once per frame or after
    // structural edits).
    void Sync(const Scene& scene);

    // Fixed or variable timestep integration. dt is clamped internally.
    void Step(Scene& scene, float dt);

    const std::vector<Collider>& Colliders() const { return colliders_; }

private:
    void ResolveStatic(Scene& scene, Collider& body);
    void ResolveDynamic(Scene& scene, Collider& a, Collider& b);

    std::vector<Collider> colliders_;
};

} // namespace pspengine
