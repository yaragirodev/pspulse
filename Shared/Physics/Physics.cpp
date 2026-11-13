// ---------------------------------------------------------------------------
//  PSP Engine - lightweight kinematic physics implementation
// ---------------------------------------------------------------------------

#include "Physics.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

namespace pspengine {

namespace {

void DecomposeWorld(const glm::mat4& world, glm::vec3& position, glm::vec3& scale) {
    glm::quat orientation;
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::decompose(world, scale, orientation, position, skew, perspective);
    scale = glm::max(glm::abs(scale), glm::vec3(0.0001f));
}

glm::vec3 ClosestPointOnAabb(const glm::vec3& p, const glm::vec3& center, const glm::vec3& half) {
    return glm::clamp(p, center - half, center + half);
}

// Sphere vs AABB. Returns true when penetrating; normal/penetration filled.
bool SphereVsAabb(const glm::vec3& sphereCenter, float radius, const glm::vec3& boxCenter,
                  const glm::vec3& half, glm::vec3& normal, float& penetration) {
    glm::vec3 closest = ClosestPointOnAabb(sphereCenter, boxCenter, half);
    glm::vec3 delta = sphereCenter - closest;
    float distSq = glm::dot(delta, delta);
    if (distSq > radius * radius) return false;
    float dist = std::sqrt(distSq);
    if (dist > 1e-5f) {
        normal = delta / dist;
        penetration = radius - dist;
    } else {
        // Center inside the box: push out along the smallest axis.
        glm::vec3 local = sphereCenter - boxCenter;
        float px = half.x - std::fabs(local.x);
        float py = half.y - std::fabs(local.y);
        float pz = half.z - std::fabs(local.z);
        if (px < py && px < pz) {
            normal = glm::vec3(local.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
            penetration = px + radius;
        } else if (py < pz) {
            normal = glm::vec3(0.0f, local.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
            penetration = py + radius;
        } else {
            normal = glm::vec3(0.0f, 0.0f, local.z >= 0.0f ? 1.0f : -1.0f);
            penetration = pz + radius;
        }
    }
    return true;
}

// AABB vs AABB: returns minimum translation vector.
bool AabbVsAabb(const glm::vec3& centerA, const glm::vec3& halfA, const glm::vec3& centerB,
                const glm::vec3& halfB, glm::vec3& normal, float& penetration) {
    glm::vec3 delta = centerB - centerA;
    glm::vec3 overlap = (halfA + halfB) - glm::abs(delta);
    if (overlap.x <= 0.0f || overlap.y <= 0.0f || overlap.z <= 0.0f) return false;

    normal = glm::vec3(0.0f);
    penetration = overlap.x;
    normal.x = delta.x >= 0.0f ? -1.0f : 1.0f; // push A away from B
    if (overlap.y < penetration) {
        penetration = overlap.y;
        normal = glm::vec3(0.0f);
        normal.y = delta.y >= 0.0f ? -1.0f : 1.0f;
    }
    if (overlap.z < penetration) {
        penetration = overlap.z;
        normal = glm::vec3(0.0f);
        normal.z = delta.z >= 0.0f ? -1.0f : 1.0f;
    }
    return true;
}

bool SphereVsSphere(const glm::vec3& centerA, float radiusA, const glm::vec3& centerB, float radiusB,
                    glm::vec3& normal, float& penetration) {
    glm::vec3 delta = centerA - centerB;
    float distSq = glm::dot(delta, delta);
    float sum = radiusA + radiusB;
    if (distSq > sum * sum) return false;
    float dist = std::sqrt(distSq);
    if (dist > 1e-5f) {
        normal = delta / dist;
        penetration = sum - dist;
    } else {
        normal = glm::vec3(0.0f, 1.0f, 0.0f);
        penetration = sum;
    }
    return true;
}

void ApplyImpulse(GameObject& object, Collider& body, const glm::vec3& normal, float penetration,
                  const glm::vec3& otherVelocity, bool staticOther) {
    // Positional correction.
    object.transform.position += normal * penetration;

    float restitution = glm::clamp(body.restitution, 0.0f, 1.0f);
    if (staticOther) {
        float vn = glm::dot(body.velocity, normal);
        if (vn < 0.0f) {
            glm::vec3 normalComponent = normal * vn;
            glm::vec3 tangent = body.velocity - normalComponent;
            body.velocity = tangent * (1.0f - glm::clamp(body.friction, 0.0f, 1.0f) * 0.25f) -
                            normalComponent * restitution;
            if (normal.y > 0.5f && std::fabs(body.velocity.y) < 0.35f) {
                body.velocity.y = 0.0f;
                object.physics.grounded = true;
            } else if (normal.y < -0.5f) {
                body.velocity.y = std::fabs(body.velocity.y) * restitution;
            }
        }
    } else {
        glm::vec3 relative = body.velocity - otherVelocity;
        float vn = glm::dot(relative, normal);
        if (vn < 0.0f) {
            float impulse = -(1.0f + restitution) * vn * 0.5f;
            body.velocity += normal * impulse;
            body.velocity -= normal * impulse * 0.5f;
            if (normal.y > 0.5f) object.physics.grounded = true;
        }
    }
    object.physics.velocity = body.velocity;
}

} // namespace

void PhysicsWorld::Sync(const Scene& scene) {
    colliders_.clear();
    colliders_.reserve(scene.objects.size());
    for (size_t i = 0; i < scene.objects.size(); ++i) {
        const GameObject& object = scene.objects[i];
        if (!object.collision.enabled && !object.physics.enabled) continue;

        Collider c;
        c.objectId = object.id;
        c.objectIndex = static_cast<int>(i);
        c.shape = object.collision.shape;
        c.isDynamic = object.physics.IsDynamic();
        c.isStatic = object.physics.enabled && (object.physics.isStatic || !c.isDynamic);
        c.velocity = object.physics.velocity;
        c.restitution = object.physics.restitution;
        c.friction = object.physics.friction;
        c.mass = glm::max(object.physics.mass, 0.01f);

        glm::vec3 worldPos;
        glm::vec3 worldScale;
        DecomposeWorld(scene.WorldMatrix(object), worldPos, worldScale);
        c.center = worldPos + object.collision.offset * worldScale;

        glm::vec3 size = object.collision.size;
        if (c.shape == CollisionShapeType::Sphere) {
            float diameter = glm::max(size.x, glm::max(size.y, size.z));
            c.radius = 0.5f * diameter * glm::max(worldScale.x, glm::max(worldScale.y, worldScale.z));
            c.halfExtents = glm::vec3(c.radius);
        } else {
            c.halfExtents = glm::max(size * 0.5f * worldScale, glm::vec3(0.01f));
            c.radius = glm::max(c.halfExtents.x, glm::max(c.halfExtents.y, c.halfExtents.z));
        }
        colliders_.push_back(c);
    }
}

void PhysicsWorld::Step(Scene& scene, float dt) {
    const float clampedDt = glm::clamp(dt, 0.0f, 0.05f);
    if (clampedDt <= 0.0f) return;

    Sync(scene);

    // Integrate.
    for (auto& collider : colliders_) {
        if (!collider.isDynamic) continue;
        GameObject* object = scene.Find(collider.objectId);
        if (!object) continue;
        if (object->physics.useGravity) {
            object->physics.velocity += scene.gravity * clampedDt;
        }
        object->transform.position += object->physics.velocity * clampedDt;
        if (object->transform.position.y < -100.0f) {
            object->transform.position.y = 8.0f;
            object->physics.velocity = glm::vec3(0.0f);
        }
        object->physics.grounded = false;
        collider.velocity = object->physics.velocity;
    }

    // Re-sync centres after integration so resolution sees fresh positions.
    Sync(scene);

    // Resolve dynamic bodies against everything.
    for (auto& body : colliders_) {
        if (!body.isDynamic) continue;
        GameObject* object = scene.Find(body.objectId);
        if (!object) continue;
        body.velocity = object->physics.velocity;

        for (auto& other : colliders_) {
            if (other.objectId == body.objectId) continue;

            glm::vec3 normal(0.0f);
            float penetration = 0.0f;
            bool collided = false;

            if (body.shape == CollisionShapeType::Sphere && other.shape == CollisionShapeType::Sphere) {
                collided = SphereVsSphere(body.center, body.radius, other.center, other.radius, normal,
                                          penetration);
            } else if (body.shape == CollisionShapeType::Sphere) {
                collided = SphereVsAabb(body.center, body.radius, other.center, other.halfExtents, normal,
                                        penetration);
            } else if (other.shape == CollisionShapeType::Sphere) {
                collided = SphereVsAabb(other.center, other.radius, body.center, body.halfExtents, normal,
                                        penetration);
                normal = -normal; // AABB body pushed away from sphere
            } else {
                collided = AabbVsAabb(body.center, body.halfExtents, other.center, other.halfExtents, normal,
                                      penetration);
            }

            if (!collided) continue;

            if (!other.isDynamic) {
                ApplyImpulse(*object, body, normal, penetration, glm::vec3(0.0f), true);
            } else if (other.isDynamic) {
                ApplyImpulse(*object, body, normal, penetration * 0.5f, other.velocity, false);
                GameObject* otherObject = scene.Find(other.objectId);
                if (otherObject) {
                    otherObject->transform.position -= normal * penetration * 0.5f;
                    otherObject->physics.velocity -= normal * 0.05f;
                }
            }
            body.center += normal * penetration;
        }
        object->physics.velocity = body.velocity;
    }
}

} // namespace pspengine
