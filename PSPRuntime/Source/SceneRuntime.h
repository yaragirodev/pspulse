#pragma once

// ---------------------------------------------------------------------------
//  PSP Runtime - scene runtime (own minimal JSON reader + math + physics)
//  Deliberately dependency-free so it builds with plain psp-gcc.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

namespace pspruntime {

// ------------------------------------------------------------------------
// Math (identical conventions to the editor: right-handed, Y up, degrees for
// Euler angles, rotation order Y * X * Z).
// ------------------------------------------------------------------------
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float ax, float ay, float az) : x(ax), y(ay), z(az) {}
};

Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& a, float s);
Vec3& operator+=(Vec3& a, const Vec3& b);
Vec3& operator-=(Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& v);
float Dot(const Vec3& a, const Vec3& b);
float Length(const Vec3& v);
Vec3 Normalize(const Vec3& v);

struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    static Mat4 Identity();
    static Mat4 Multiply(const Mat4& a, const Mat4& b);
    static Mat4 Translation(const Vec3& t);
    static Mat4 RotationEuler(const Vec3& degrees); // Y * X * Z
    static Mat4 Scale(const Vec3& s);
    static Mat4 Perspective(float fovYDegrees, float aspect, float nearPlane, float farPlane);
    static Mat4 LookAt(const Vec3& eye, const Vec3& center, const Vec3& up);
    Vec3 TransformPoint(const Vec3& p) const;
    Vec3 TransformDirection(const Vec3& d) const;
    Vec3 TranslationPart() const { return Vec3(m[12], m[13], m[14]); }
};

// ------------------------------------------------------------------------
// Tiny JSON DOM (objects, arrays, strings, numbers, bool, null)
// ------------------------------------------------------------------------
struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    Type type = Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<JsonValue> items;
    std::vector<std::string> keys;
    std::vector<JsonValue> values;

    const JsonValue* Get(const char* key) const;
    std::string GetString(const char* key, const std::string& fallback = "") const;
    double GetNumber(const char* key, double fallback = 0.0) const;
    bool GetBool(const char* key, bool fallback = false) const;
    Vec3 GetVec3(const char* key, const Vec3& fallback = Vec3()) const;
    double AsNumber(double fallback = 0.0) const;
    bool AsBool(bool fallback = false) const;
    const std::string& AsString() const { return text; }
};

bool ParseJson(const std::string& source, JsonValue& out, std::string& error);

// ------------------------------------------------------------------------
// Scene model (runtime mirror of Shared/Scene/SceneData.h)
// ------------------------------------------------------------------------
enum PrimitiveType { PrimEmpty = 0, PrimCube, PrimSphere, PrimCone, PrimCylinder, PrimPlane };
enum CollisionShapeType { ShapeBox = 0, ShapeSphere };

struct Transform {
    Vec3 position;
    Vec3 rotation;
    Vec3 scale = Vec3(1.0f, 1.0f, 1.0f);
    Mat4 Matrix() const;
};

struct PhysicsBody {
    bool enabled = false;
    float mass = 1.0f;
    bool useGravity = true;
    bool isStatic = false;
    float restitution = 0.2f;
    float friction = 0.6f;
    Vec3 velocity;
    bool grounded = false;
    bool IsDynamic() const { return enabled && !isStatic; }
};

struct CollisionShape {
    bool enabled = false;
    CollisionShapeType shape = ShapeBox;
    Vec3 size = Vec3(1.0f, 1.0f, 1.0f);
    Vec3 offset;
};

struct LightComponent {
    bool enabled = false;
    Vec3 color = Vec3(1.0f, 0.95f, 0.85f);
    float intensity = 1.0f;
    float range = 14.0f;
};

struct CameraComponent {
    bool enabled = false;
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

struct GameObject {
    int id = 0;
    std::string name;
    int parent = -1;
    PrimitiveType primitive = PrimEmpty;
    std::string meshPath;   // project-relative glTF/GLB reference ("" == primitive)
    bool visible = true;
    float color[4] = {0.82f, 0.84f, 0.90f, 1.0f};
    Transform transform;
    std::string script;
    PhysicsBody physics;
    CollisionShape collision;
    LightComponent light;
    CameraComponent camera;
};

struct Scene {
    std::string name = "Scene";
    Vec3 ambient = Vec3(0.22f, 0.24f, 0.30f);
    Vec3 gravity = Vec3(0.0f, -9.81f, 0.0f);
    std::vector<GameObject> objects;

    GameObject* Find(int id);
    Mat4 WorldMatrix(const GameObject& object) const;
    int IndexOf(int id) const;
    void UpdateWorldMatrices() const;
    mutable std::vector<Mat4> worldMatrices;
};

bool ParseScene(const std::string& json, Scene& out, std::string& error);

// ------------------------------------------------------------------------
// Physics (order-of-magnitude richer solver than the editor debug one:
// sphere/AABB against static world + dynamic-vs-dynamic separation).
// ------------------------------------------------------------------------
struct Collider {
    int objectId = -1;
    CollisionShapeType shape = ShapeBox;
    Vec3 center;
    Vec3 halfExtents = Vec3(0.5f, 0.5f, 0.5f);
    float radius = 0.5f;
    bool isDynamic = false;
    Vec3 velocity;
    float restitution = 0.2f;
    float friction = 0.6f;
};

class Physics {
public:
    void Sync(const Scene& scene);
    void Step(Scene& scene, float dt);
    int ColliderCount() const { return static_cast<int>(colliders_.size()); }

private:
    std::vector<Collider> colliders_;
};

// Primitive mesh generation shared with the GU renderer.
// GU vertex layout: attributes must follow the GU order (uv -> color -> normal
// -> position). We only pass GU_COLOR_8888 | GU_VERTEX_32BITF, so the struct
// starts with the colour.
struct MeshVertex {
    unsigned int color;
    float x, y, z;
};

struct Mesh {
    std::vector<MeshVertex> vertices;
    int triangleCount = 0;
    void PushTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& na, const Vec3& nb,
                      const Vec3& nc, unsigned int color);
};

void BuildPrimitiveMesh(PrimitiveType type, unsigned int color, Mesh& out);

// Converts glTF geometry (positions/normals/indices, already flattened and
// transformed by the loader) into GU triangles with the object's tint applied
// and a cheap top-down lighting term baked per vertex.
struct GltfMeshData {
    std::vector<float> positions;   // xyz
    std::vector<float> normals;     // xyz
    std::vector<unsigned int> indices;
};
void BuildMeshFromGltf(const GltfMeshData& source, unsigned int color, Mesh& out);

} // namespace pspruntime
