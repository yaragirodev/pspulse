// ---------------------------------------------------------------------------
//  PSPulse - runtime scene model implementation
// ---------------------------------------------------------------------------

#include "SceneData.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/intersect.hpp>

namespace pspulse {

// --------------------------------------------------------------------------
// Enum <-> string
// --------------------------------------------------------------------------

static const char* kPrimitiveNames[] = {"Empty", "Cube", "Sphere", "Cone", "Cylinder", "Plane"};
static const char* kPrimitiveDisplay[] = {"Empty", "Cube", "Sphere", "Cone", "Cylinder", "Plane"};

const char* PrimitiveToString(PrimitiveType type) {
    int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(PrimitiveType::Count)) index = 0;
    return kPrimitiveNames[index];
}

PrimitiveType PrimitiveFromString(const std::string& text) {
    for (int i = 0; i < static_cast<int>(PrimitiveType::Count); ++i) {
        if (text == kPrimitiveNames[i]) return static_cast<PrimitiveType>(i);
    }
    return PrimitiveType::Empty;
}

const char* PrimitiveDisplayName(PrimitiveType type) {
    int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(PrimitiveType::Count)) index = 0;
    return kPrimitiveDisplay[index];
}

static const char* kShapeNames[] = {"Box", "Sphere"};
static const char* kShapeDisplay[] = {"Collision Box", "Collision Sphere"};

const char* CollisionShapeToString(CollisionShapeType type) {
    int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(CollisionShapeType::Count)) index = 0;
    return kShapeNames[index];
}

CollisionShapeType CollisionShapeFromString(const std::string& text) {
    for (int i = 0; i < static_cast<int>(CollisionShapeType::Count); ++i) {
        if (text == kShapeNames[i]) return static_cast<CollisionShapeType>(i);
    }
    return CollisionShapeType::Box;
}

const char* CollisionShapeDisplayName(CollisionShapeType type) {
    int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(CollisionShapeType::Count)) index = 0;
    return kShapeDisplay[index];
}

// --------------------------------------------------------------------------
// Math helpers
// --------------------------------------------------------------------------

glm::mat4 RotationMatrixEulerDegrees(const glm::vec3& eulerDeg) {
    const float deg2rad = 3.14159265358979323846f / 180.0f;
    glm::mat4 rot(1.0f);
    rot = glm::rotate(rot, eulerDeg.y * deg2rad, glm::vec3(0, 1, 0));
    rot = glm::rotate(rot, eulerDeg.x * deg2rad, glm::vec3(1, 0, 0));
    rot = glm::rotate(rot, eulerDeg.z * deg2rad, glm::vec3(0, 0, 1));
    return rot;
}

glm::vec3 EulerDegreesFromMatrix(const glm::mat4& matrix) {
    glm::vec3 scale;
    glm::quat orientation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    if (!glm::decompose(matrix, scale, orientation, translation, skew, perspective)) {
        return glm::vec3(0.0f);
    }
    glm::mat4 rot = glm::mat4_cast(orientation);
    // Inverse of Y * X * Z order:
    //   m21 = sin(x);  x = asin(m21)
    //   m20/m22 = -sin(y)cos(x)/cos(y)cos(x) -> y = atan2(-m20, m22)
    //   m01/m11 = -cos(x)sin(z)/cos(x)cos(z) -> z = atan2(-m01, m11)
    const float deg2rad = 3.14159265358979323846f / 180.0f;
    (void)deg2rad;
    const float m21 = glm::clamp(rot[1][2], -1.0f, 1.0f);
    float x = std::asin(m21);
    float y = 0.0f;
    float z = 0.0f;
    if (std::fabs(m21) < 0.9999f) {
        y = std::atan2(-rot[0][2], rot[2][2]);
        z = std::atan2(-rot[1][0], rot[1][1]);
    } else {
        // Gimbal lock - fold yaw into roll.
        z = 0.0f;
        y = std::atan2(rot[2][0], rot[0][0]);
    }
    const float rad2deg = 180.0f / 3.14159265358979323846f;
    return glm::vec3(x * rad2deg, y * rad2deg, z * rad2deg);
}

glm::mat4 Transform::Matrix() const {
    glm::mat4 m(1.0f);
    m = glm::translate(m, position);
    m = m * RotationMatrixEulerDegrees(rotation);
    m = glm::scale(m, scale);
    return m;
}

glm::vec3 Transform::Forward() const {
    glm::mat4 r = RotationMatrixEulerDegrees(rotation);
    return glm::normalize(glm::vec3(-r[0][2], -r[1][2], -r[2][2]));
}

glm::vec3 Transform::Right() const {
    glm::mat4 r = RotationMatrixEulerDegrees(rotation);
    return glm::normalize(glm::vec3(r[0][0], r[1][0], r[2][0]));
}

// --------------------------------------------------------------------------
// Scene
// --------------------------------------------------------------------------

GameObject* Scene::Find(int id) {
    for (auto& object : objects) {
        if (object.id == id) return &object;
    }
    return nullptr;
}

const GameObject* Scene::Find(int id) const {
    for (const auto& object : objects) {
        if (object.id == id) return &object;
    }
    return nullptr;
}

int Scene::IndexOf(int id) const {
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

GameObject& Scene::AddObject(const std::string& objectName, PrimitiveType primitive) {
    GameObject object;
    object.id = AllocateId();
    object.name = objectName;
    object.primitive = primitive;
    if (primitive == PrimitiveType::Plane) {
        object.color = glm::vec4(0.55f, 0.58f, 0.62f, 1.0f);
    }
    objects.push_back(object);
    return objects.back();
}

void Scene::RemoveObject(int id) {
    // Remove descendants as well.
    std::vector<int> doomed{id};
    bool grew = true;
    while (grew) {
        grew = false;
        for (const auto& object : objects) {
            if (std::find(doomed.begin(), doomed.end(), object.id) != doomed.end()) continue;
            if (std::find(doomed.begin(), doomed.end(), object.parent) != doomed.end()) {
                doomed.push_back(object.id);
                grew = true;
            }
        }
    }
    objects.erase(std::remove_if(objects.begin(), objects.end(),
                                 [&](const GameObject& object) {
                                     return std::find(doomed.begin(), doomed.end(), object.id) != doomed.end();
                                 }),
                  objects.end());
}

bool Scene::IsDescendantOf(int childId, int ancestorId) const {
    const GameObject* object = Find(childId);
    int guard = 0;
    while (object && object->parent != -1 && guard++ < 512) {
        if (object->parent == ancestorId) return true;
        object = Find(object->parent);
    }
    return false;
}

int Scene::CloneObject(int id) {
    const GameObject* source = Find(id);
    if (!source) return -1;
    GameObject copy = *source;
    copy.id = AllocateId();
    copy.name = source->name + " (copy)";
    objects.push_back(copy);
    return copy.id;
}

void Scene::EnsureConsistentIds() {
    std::set<int> seen;
    int maxId = 0;
    for (auto& object : objects) {
        if (object.id <= 0 || seen.count(object.id)) {
            while (seen.count(nextId) || nextId <= maxId) ++nextId;
            object.id = nextId++;
        }
        seen.insert(object.id);
        maxId = std::max(maxId, object.id);
    }
    for (auto& object : objects) {
        if (object.parent != -1 && seen.count(object.parent) == 0) object.parent = -1;
        if (object.parent == object.id) object.parent = -1;
    }
    if (nextId <= maxId) nextId = maxId + 1;
}

glm::mat4 Scene::WorldMatrix(const GameObject& object) const {
    glm::mat4 matrix = object.LocalMatrix();
    const GameObject* current = &object;
    int guard = 0;
    while (current->parent != -1 && guard++ < 512) {
        const GameObject* parent = Find(current->parent);
        if (!parent) break;
        matrix = parent->LocalMatrix() * matrix;
        current = parent;
    }
    return matrix;
}

glm::mat4 Scene::WorldMatrix(int index) const {
    if (index < 0 || index >= static_cast<int>(objects.size())) return glm::mat4(1.0f);
    return WorldMatrix(objects[index]);
}

GameObject Scene::MakeLight(const std::string& objectName) {
    GameObject object;
    object.name = objectName;
    object.primitive = PrimitiveType::Empty;
    object.transform.position = glm::vec3(4.0f, 6.0f, 5.0f);
    object.light.enabled = true;
    object.light.color = glm::vec3(1.0f, 0.95f, 0.82f);
    object.light.intensity = 1.0f;
    object.light.range = 18.0f;
    return object;
}

GameObject Scene::MakeCamera(const std::string& objectName) {
    GameObject object;
    object.name = objectName;
    object.primitive = PrimitiveType::Empty;
    object.transform.position = glm::vec3(0.0f, 3.0f, 9.0f);
    object.transform.rotation = glm::vec3(-15.0f, 0.0f, 0.0f);
    object.camera.enabled = true;
    object.camera.active = true;
    return object;
}

// --------------------------------------------------------------------------
// Raycasting against analytic primitive shapes (local space)
// --------------------------------------------------------------------------

namespace {

bool RaySphere(const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& center, float radius,
               float& tOut) {
    glm::vec3 oc = origin - center;
    float b = glm::dot(oc, dir);
    float c = glm::dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0f) return false;
    float sq = std::sqrt(disc);
    float t = -b - sq;
    if (t < 0.0f) t = -b + sq;
    if (t < 0.0f) return false;
    tOut = t;
    return true;
}

// Cylinder / cone approximated as tapered capsule along local Y in [-0.5, 0.5].
bool RayTaperedCylinder(const glm::vec3& origin, const glm::vec3& dir, float rBottom, float rTop,
                        float& tOut) {
    // Sample: intersect with a few stacked cylinders.
    const int slices = 6;
    bool found = false;
    float best = 1e30f;
    for (int i = 0; i < slices; ++i) {
        float t0 = static_cast<float>(i) / slices;
        float t1 = static_cast<float>(i + 1) / slices;
        float yCenter = -0.5f + 0.5f * (t0 + t1);
        float radius = glm::mix(rBottom, rTop, (t0 + t1) * 0.5f);
        float halfH = 0.5f / slices;
        // Slab test in Y.
        if (std::fabs(dir.y) < 1e-6f) {
            if (std::fabs(origin.y - yCenter) > halfH) continue;
        }
        float ty0 = (yCenter - halfH - origin.y) / dir.y;
        float ty1 = (yCenter + halfH - origin.y) / dir.y;
        if (ty0 > ty1) std::swap(ty0, ty1);
        float tEnter = ty0;
        float tExit = ty1;
        // Infinite cylinder test in XZ.
        float a = dir.x * dir.x + dir.z * dir.z;
        float b = 2.0f * (origin.x * dir.x + origin.z * dir.z);
        float c = origin.x * origin.x + origin.z * origin.z - radius * radius;
        if (a > 1e-8f) {
            float disc = b * b - 4.0f * a * c;
            if (disc < 0.0f) continue;
            float sq = std::sqrt(disc);
            float tc0 = (-b - sq) / (2.0f * a);
            float tc1 = (-b + sq) / (2.0f * a);
            tEnter = std::max(tEnter, tc0);
            tExit = std::min(tExit, tc1);
        } else if (c > 0.0f) {
            continue;
        }
        if (tEnter > tExit || tExit < 0.0f) continue;
        float t = tEnter >= 0.0f ? tEnter : tExit;
        if (t < best) {
            best = t;
            found = true;
        }
    }
    if (found) tOut = best;
    return found;
}

} // namespace

RaycastHit Scene::Raycast(const glm::vec3& origin, const glm::vec3& direction) const {
    RaycastHit hit;
    hit.distance = 1e30f;
    glm::vec3 dir = glm::normalize(direction);

    for (size_t i = 0; i < objects.size(); ++i) {
        const GameObject& object = objects[i];
        if (!object.visible || object.primitive == PrimitiveType::Empty) continue;

        glm::mat4 world = WorldMatrix(object);
        glm::mat4 inv = glm::inverse(world);

        // Transform ray into object space. Non-uniform scale is handled by
        // adjusting the hit distance with the inverse scale afterwards.
        glm::vec3 localOrigin = glm::vec3(inv * glm::vec4(origin, 1.0f));
        glm::vec3 localDir = glm::vec3(inv * glm::vec4(dir, 0.0f));
        float dirLen = glm::length(localDir);
        if (dirLen < 1e-8f) continue;
        localDir /= dirLen;

        float tLocal = 0.0f;
        bool found = false;

        switch (object.primitive) {
            case PrimitiveType::Cube: {
                glm::vec3 invDir(1.0f / (std::fabs(localDir.x) > 1e-8f ? localDir.x : 1e-8f),
                                 1.0f / (std::fabs(localDir.y) > 1e-8f ? localDir.y : 1e-8f),
                                 1.0f / (std::fabs(localDir.z) > 1e-8f ? localDir.z : 1e-8f));
                glm::vec3 t0 = (glm::vec3(-0.5f) - localOrigin) * invDir;
                glm::vec3 t1 = (glm::vec3(0.5f) - localOrigin) * invDir;
                glm::vec3 tmin = glm::min(t0, t1);
                glm::vec3 tmax = glm::max(t0, t1);
                float tNear = std::max(std::max(tmin.x, tmin.y), tmin.z);
                float tFar = std::min(std::min(tmax.x, tmax.y), tmax.z);
                if (tNear <= tFar && tFar >= 0.0f) {
                    tLocal = tNear >= 0.0f ? tNear : tFar;
                    found = true;
                }
                break;
            }
            case PrimitiveType::Sphere:
                found = RaySphere(localOrigin, localDir, glm::vec3(0.0f), 0.5f, tLocal);
                break;
            case PrimitiveType::Cone:
                found = RayTaperedCylinder(localOrigin, localDir, 0.5f, 0.0f, tLocal);
                break;
            case PrimitiveType::Cylinder:
                found = RayTaperedCylinder(localOrigin, localDir, 0.5f, 0.5f, tLocal);
                break;
            case PrimitiveType::Plane: {
                if (std::fabs(localDir.y) < 1e-8f) break;
                float t = -localOrigin.y / localDir.y;
                glm::vec3 p = localOrigin + localDir * t;
                if (t >= 0.0f && std::fabs(p.x) <= 0.5f && std::fabs(p.z) <= 0.5f) {
                    tLocal = t;
                    found = true;
                }
                break;
            }
            default:
                break;
        }

        if (!found) continue;
        // localDir was normalised after the inverse transform, recover world t.
        float tWorld = tLocal / dirLen;
        if (tWorld > 0.0f && tWorld < hit.distance) {
            hit.hit = true;
            hit.distance = tWorld;
            hit.objectIndex = static_cast<int>(i);
            hit.point = origin + dir * tWorld;
        }
    }
    if (!hit.hit) hit.distance = 0.0f;
    return hit;
}

} // namespace pspulse
