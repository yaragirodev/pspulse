#pragma once

// ---------------------------------------------------------------------------
//  PSPulse - runtime scene model (shared between editor and PSP runtime)
//
//  The data model is deliberately plain-old-data so that it can be:
//    * edited through Dear ImGui in the desktop editor,
//    * serialised to / from JSON (Shared/Scene/SceneIO.cpp),
//    * stepped by the shared toy physics (Shared/Physics/Physics.cpp),
//    * streamed straight into the PSP GU renderer.
//
//  Extending the engine with a new component type means:
//    1. add a POD struct below and a member to GameObject,
//    2. add (de)serialisation in SceneIO.cpp,
//    3. add an inspector block in Editor/Source/Panels.cpp,
//    4. add the runtime behaviour in PSPRuntime/Source/RuntimeApp.cpp.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace pspulse {

// --------------------------------------------------------------------------
// Enumerations
// --------------------------------------------------------------------------

enum class PrimitiveType {
    Empty = 0,
    Cube,
    Sphere,
    Cone,
    Cylinder,
    Plane,
    Count
};

enum class CollisionShapeType {
    Box = 0,
    Sphere,
    Count
};

const char* PrimitiveToString(PrimitiveType type);
PrimitiveType PrimitiveFromString(const std::string& text);
const char* PrimitiveDisplayName(PrimitiveType type);

const char* CollisionShapeToString(CollisionShapeType type);
CollisionShapeType CollisionShapeFromString(const std::string& text);
const char* CollisionShapeDisplayName(CollisionShapeType type);

// --------------------------------------------------------------------------
// Components
// --------------------------------------------------------------------------

struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f}; // Euler angles, degrees, applied Y * X * Z
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    glm::mat4 Matrix() const;
    glm::vec3 Forward() const;
    glm::vec3 Right() const;
};

struct PhysicsBodyComponent {
    bool enabled = false;
    float mass = 1.0f;
    bool useGravity = true;
    bool isStatic = false;
    float restitution = 0.2f;
    float friction = 0.6f;
    glm::vec3 velocity{0.0f, 0.0f, 0.0f};

    // Runtime-only state (never serialised).
    bool grounded = false;

    bool IsDynamic() const { return enabled && !isStatic; }
};

struct CollisionShapeComponent {
    bool enabled = false;
    CollisionShapeType shape = CollisionShapeType::Box;
    glm::vec3 size{1.0f, 1.0f, 1.0f};   // full extents (box) or diameter (sphere)
    glm::vec3 offset{0.0f, 0.0f, 0.0f};
};

struct LightComponent {
    bool enabled = false;
    glm::vec3 color{1.0f, 0.95f, 0.85f};
    float intensity = 1.0f;
    float range = 14.0f;
};

struct CameraComponent {
    bool enabled = false;
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    bool active = false;
};

// --------------------------------------------------------------------------
// GameObject
// --------------------------------------------------------------------------

struct GameObject {
    int id = 0;
    std::string name = "GameObject";
    int parent = -1; // -1 == scene root
    PrimitiveType primitive = PrimitiveType::Empty;
    // Project-relative path to a glTF model ("" == use the primitive above).
    // Supported: .gltf and .glb, no Draco/meshopt compression.
    std::string meshPath;
    bool visible = true;
    glm::vec4 color{0.82f, 0.84f, 0.90f, 1.0f};

    Transform transform;
    std::string script; // project-relative path to a .lua file ("" == none)

    PhysicsBodyComponent physics;
    CollisionShapeComponent collision;
    LightComponent light;
    CameraComponent camera;

    // Convenience helpers used by editor gizmo + runtime hierarchy.
    glm::mat4 LocalMatrix() const { return transform.Matrix(); }
};

// --------------------------------------------------------------------------
// Scene
// --------------------------------------------------------------------------

struct RaycastHit {
    bool hit = false;
    int objectIndex = -1;
    float distance = 0.0f;
    glm::vec3 point{0.0f, 0.0f, 0.0f};
};

struct Scene {
    std::string name = "Untitled Scene";
    int version = 1;
    glm::vec3 ambient{0.22f, 0.24f, 0.30f};
    glm::vec3 gravity{0.0f, -9.81f, 0.0f};
    std::vector<GameObject> objects;
    int nextId = 1;

    // ---- object management -------------------------------------------------
    GameObject* Find(int id);
    const GameObject* Find(int id) const;
    int IndexOf(int id) const;

    GameObject& AddObject(const std::string& name, PrimitiveType primitive);
    int AllocateId() { return nextId++; }
    void RemoveObject(int id);
    bool IsDescendantOf(int childId, int ancestorId) const;
    int CloneObject(int id);
    void EnsureConsistentIds();

    // ---- hierarchy helpers -------------------------------------------------
    glm::mat4 WorldMatrix(const GameObject& object) const;
    glm::mat4 WorldMatrix(int index) const;

    // ---- ray casting (editor picking) --------------------------------------
    RaycastHit Raycast(const glm::vec3& origin, const glm::vec3& direction) const;

    // ---- object factories ---------------------------------------------------
    static GameObject MakeLight(const std::string& name);
    static GameObject MakeCamera(const std::string& name);
};

// Rotation helpers shared by editor and runtime.
glm::mat4 RotationMatrixEulerDegrees(const glm::vec3& eulerDeg);
glm::vec3 EulerDegreesFromMatrix(const glm::mat4& matrix);

} // namespace pspulse
