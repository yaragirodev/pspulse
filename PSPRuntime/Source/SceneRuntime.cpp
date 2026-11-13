// ---------------------------------------------------------------------------
//  PSP Runtime - scene runtime: math, JSON, scene parsing, physics, meshes
// ---------------------------------------------------------------------------

#include "SceneRuntime.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pspruntime {

// ==========================================================================
// Math
// ==========================================================================

Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
Vec3 operator*(const Vec3& a, float s) { return Vec3(a.x * s, a.y * s, a.z * s); }
Vec3& operator+=(Vec3& a, const Vec3& b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}
Vec3& operator-=(Vec3& a, const Vec3& b) {
    a.x -= b.x;
    a.y -= b.y;
    a.z -= b.z;
    return a;
}
Vec3 operator-(const Vec3& v) { return Vec3(-v.x, -v.y, -v.z); }
float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float Length(const Vec3& v) { return std::sqrt(Dot(v, v)); }
Vec3 Normalize(const Vec3& v) {
    float len = Length(v);
    if (len < 1e-6f) return Vec3(0.0f, 0.0f, 0.0f);
    return v * (1.0f / len);
}

Mat4 Mat4::Identity() {
    Mat4 r;
    for (int i = 0; i < 16; ++i) r.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    return r;
}

Mat4 Mat4::Multiply(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = sum;
        }
    }
    return r;
}

Mat4 Mat4::Translation(const Vec3& t) {
    Mat4 r = Identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

Mat4 Mat4::RotationEuler(const Vec3& degrees) {
    const float kDeg2Rad = 3.14159265358979323846f / 180.0f;
    float ax = degrees.x * kDeg2Rad;
    float ay = degrees.y * kDeg2Rad;
    float az = degrees.z * kDeg2Rad;
    Mat4 rx = Identity(), ry = Identity(), rz = Identity();
    float cx = std::cos(ax), sx = std::sin(ax);
    float cy = std::cos(ay), sy = std::sin(ay);
    float cz = std::cos(az), sz = std::sin(az);
    // Column-major: m[col*4 + row]
    rx.m[5] = cx;  rx.m[6] = sx;  rx.m[9] = -sx; rx.m[10] = cx;
    ry.m[0] = cy;  ry.m[2] = -sy; ry.m[8] = sy;  ry.m[10] = cy;
    rz.m[0] = cz;  rz.m[1] = sz;  rz.m[4] = -sz; rz.m[5] = cz;
    return Multiply(Multiply(ry, rx), rz);
}

Mat4 Mat4::Scale(const Vec3& s) {
    Mat4 r = Identity();
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    return r;
}

Mat4 Mat4::Perspective(float fovYDegrees, float aspect, float nearPlane, float farPlane) {
    const float kDeg2Rad = 3.14159265358979323846f / 180.0f;
    float f = 1.0f / std::tan(fovYDegrees * kDeg2Rad * 0.5f);
    Mat4 r;
    for (int i = 0; i < 16; ++i) r.m[i] = 0.0f;
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    return r;
}

Mat4 Mat4::LookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = Normalize(center - eye);
    Vec3 s = Normalize(Vec3(f.y * up.z - f.z * up.y, f.z * up.x - f.x * up.z, f.x * up.y - f.y * up.x));
    Vec3 u = Vec3(s.y * f.z - s.z * f.y, s.z * f.x - s.x * f.z, s.x * f.y - s.y * f.x);
    Mat4 r = Identity();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8] = s.z;
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -Dot(s, eye);
    r.m[13] = -Dot(u, eye);
    r.m[14] = Dot(f, eye);
    return r;
}

Vec3 Mat4::TransformPoint(const Vec3& p) const {
    return Vec3(m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
                m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
                m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]);
}

Vec3 Mat4::TransformDirection(const Vec3& d) const {
    return Vec3(m[0] * d.x + m[4] * d.y + m[8] * d.z, m[1] * d.x + m[5] * d.y + m[9] * d.z,
                m[2] * d.x + m[6] * d.y + m[10] * d.z);
}

Mat4 Transform::Matrix() const {
    return Mat4::Multiply(Mat4::Multiply(Mat4::Translation(position), Mat4::RotationEuler(rotation)),
                          Mat4::Scale(scale));
}

// ==========================================================================
// JSON
// ==========================================================================

namespace {

struct JsonParser {
    const char* p;
    const char* end;
    std::string error;

    void SkipWhitespace() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    }

    bool Fail(const char* message) {
        if (error.empty()) error = message;
        return false;
    }

    bool ParseValue(JsonValue& out, int depth) {
        if (depth > 64) return Fail("JSON nesting too deep");
        SkipWhitespace();
        if (p >= end) return Fail("unexpected end of input");
        char c = *p;
        if (c == '{') return ParseObject(out, depth);
        if (c == '[') return ParseArray(out, depth);
        if (c == '"') {
            out.type = JsonValue::String;
            return ParseString(out.text);
        }
        if (c == 't') {
            if (end - p >= 4 && std::strncmp(p, "true", 4) == 0) {
                p += 4;
                out.type = JsonValue::Bool;
                out.boolean = true;
                return true;
            }
            return Fail("invalid token");
        }
        if (c == 'f') {
            if (end - p >= 5 && std::strncmp(p, "false", 5) == 0) {
                p += 5;
                out.type = JsonValue::Bool;
                out.boolean = false;
                return true;
            }
            return Fail("invalid token");
        }
        if (c == 'n') {
            if (end - p >= 4 && std::strncmp(p, "null", 4) == 0) {
                p += 4;
                out.type = JsonValue::Null;
                return true;
            }
            return Fail("invalid token");
        }
        return ParseNumber(out);
    }

    bool ParseNumber(JsonValue& out) {
        char* endPtr = nullptr;
        double value = std::strtod(p, &endPtr);
        if (endPtr == p) return Fail("invalid number");
        p = endPtr;
        out.type = JsonValue::Number;
        out.number = value;
        return true;
    }

    bool ParseString(std::string& out) {
        if (p >= end || *p != '"') return Fail("expected string");
        ++p;
        out.clear();
        while (p < end) {
            char c = *p++;
            if (c == '"') return true;
            if (c == '\\') {
                if (p >= end) return Fail("bad escape");
                char esc = *p++;
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (end - p < 4) return Fail("bad unicode escape");
                        unsigned int code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = p[i];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= static_cast<unsigned int>(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= static_cast<unsigned int>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= static_cast<unsigned int>(h - 'A' + 10);
                            else return Fail("bad unicode escape");
                        }
                        p += 4;
                        // UTF-8 encode (BMP only, no surrogate pairs).
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        } else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: return Fail("bad escape");
                }
            } else {
                out += c;
            }
        }
        return Fail("unterminated string");
    }

    bool ParseArray(JsonValue& out, int depth) {
        out.type = JsonValue::Array;
        ++p; // [
        SkipWhitespace();
        if (p < end && *p == ']') {
            ++p;
            return true;
        }
        while (p < end) {
            JsonValue item;
            if (!ParseValue(item, depth + 1)) return false;
            out.items.push_back(item);
            SkipWhitespace();
            if (p < end && *p == ',') {
                ++p;
                continue;
            }
            if (p < end && *p == ']') {
                ++p;
                return true;
            }
            return Fail("expected ',' or ']'");
        }
        return Fail("unterminated array");
    }

    bool ParseObject(JsonValue& out, int depth) {
        out.type = JsonValue::Object;
        ++p; // {
        SkipWhitespace();
        if (p < end && *p == '}') {
            ++p;
            return true;
        }
        while (p < end) {
            SkipWhitespace();
            std::string key;
            if (!ParseString(key)) return false;
            SkipWhitespace();
            if (p >= end || *p != ':') return Fail("expected ':'");
            ++p;
            JsonValue value;
            if (!ParseValue(value, depth + 1)) return false;
            out.keys.push_back(key);
            out.values.push_back(value);
            SkipWhitespace();
            if (p < end && *p == ',') {
                ++p;
                continue;
            }
            if (p < end && *p == '}') {
                ++p;
                return true;
            }
            return Fail("expected ',' or '}'");
        }
        return Fail("unterminated object");
    }
};

} // namespace

bool ParseJson(const std::string& source, JsonValue& out, std::string& error) {
    JsonParser parser{source.c_str(), source.c_str() + source.size(), std::string()};
    out = JsonValue();
    if (!parser.ParseValue(out, 0)) {
        error = parser.error.empty() ? "invalid JSON" : parser.error;
        return false;
    }
    return true;
}

const JsonValue* JsonValue::Get(const char* key) const {
    if (type != Object) return nullptr;
    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] == key) return &values[i];
    }
    return nullptr;
}

std::string JsonValue::GetString(const char* key, const std::string& fallback) const {
    const JsonValue* v = Get(key);
    if (!v || v->type != String) return fallback;
    return v->text;
}

double JsonValue::GetNumber(const char* key, double fallback) const {
    const JsonValue* v = Get(key);
    if (!v || v->type != Number) return fallback;
    return v->number;
}

bool JsonValue::GetBool(const char* key, bool fallback) const {
    const JsonValue* v = Get(key);
    if (!v || v->type != Bool) return fallback;
    return v->boolean;
}

Vec3 JsonValue::GetVec3(const char* key, const Vec3& fallback) const {
    const JsonValue* v = Get(key);
    if (!v || v->type != Array || v->items.size() < 3) return fallback;
    return Vec3(static_cast<float>(v->items[0].AsNumber(fallback.x)),
                static_cast<float>(v->items[1].AsNumber(fallback.y)),
                static_cast<float>(v->items[2].AsNumber(fallback.z)));
}

double JsonValue::AsNumber(double fallback) const { return type == Number ? number : fallback; }
bool JsonValue::AsBool(bool fallback) const { return type == Bool ? boolean : fallback; }

// ==========================================================================
// Scene
// ==========================================================================

GameObject* Scene::Find(int id) {
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].id == id) return &objects[i];
    }
    return nullptr;
}

int Scene::IndexOf(int id) const {
    for (size_t i = 0; i < objects.size(); ++i) {
        if (objects[i].id == id) return static_cast<int>(i);
    }
    return -1;
}

Mat4 Scene::WorldMatrix(const GameObject& object) const {
    int index = IndexOf(object.id);
    if (index >= 0 && index < static_cast<int>(worldMatrices.size()) &&
        worldMatrices.size() == objects.size()) {
        return worldMatrices[static_cast<size_t>(index)];
    }
    return object.transform.Matrix();
}

void Scene::UpdateWorldMatrices() const {
    worldMatrices.resize(objects.size());
    for (size_t i = 0; i < objects.size(); ++i) {
        Mat4 local = objects[i].transform.Matrix();
        const GameObject* current = &objects[i];
        int guard = 0;
        while (current->parent != -1 && guard++ < 64) {
            const GameObject* parent = nullptr;
            for (const auto& candidate : objects) {
                if (candidate.id == current->parent) {
                    parent = &candidate;
                    break;
                }
            }
            if (!parent) break;
            local = Mat4::Multiply(parent->transform.Matrix(), local);
            current = parent;
        }
        worldMatrices[i] = local;
    }
}

namespace {

PrimitiveType PrimitiveFromName(const std::string& name) {
    if (name == "Cube") return PrimCube;
    if (name == "Sphere") return PrimSphere;
    if (name == "Cone") return PrimCone;
    if (name == "Cylinder") return PrimCylinder;
    if (name == "Plane") return PrimPlane;
    return PrimEmpty;
}

bool ParseObjectJson(const JsonValue& node, GameObject& object) {
    object.id = static_cast<int>(node.GetNumber("id", 0));
    object.name = node.GetString("name", "GameObject");
    object.parent = static_cast<int>(node.GetNumber("parent", -1));
    object.primitive = PrimitiveFromName(node.GetString("primitive", "Empty"));
    object.meshPath = node.GetString("mesh_path", "");
    object.visible = node.GetBool("visible", true);
    object.script = node.GetString("script", "");

    if (const JsonValue* color = node.Get("color")) {
        if (color->type == JsonValue::Array && color->items.size() >= 3) {
            for (int i = 0; i < 3 && i < static_cast<int>(color->items.size()); ++i) {
                object.color[i] = static_cast<float>(color->items[static_cast<size_t>(i)].AsNumber(1.0));
            }
        }
    }

    if (const JsonValue* transform = node.Get("transform")) {
        object.transform.position = transform->GetVec3("position", Vec3(0, 0, 0));
        object.transform.rotation = transform->GetVec3("rotation", Vec3(0, 0, 0));
        object.transform.scale = transform->GetVec3("scale", Vec3(1, 1, 1));
    }

    if (const JsonValue* physics = node.Get("physics")) {
        object.physics.enabled = physics->GetBool("enabled", false);
        object.physics.mass = static_cast<float>(physics->GetNumber("mass", 1.0));
        object.physics.useGravity = physics->GetBool("useGravity", true);
        object.physics.isStatic = physics->GetBool("isStatic", false);
        object.physics.restitution = static_cast<float>(physics->GetNumber("restitution", 0.2));
        object.physics.friction = static_cast<float>(physics->GetNumber("friction", 0.6));
        object.physics.velocity = physics->GetVec3("velocity", Vec3(0, 0, 0));
    }
    if (const JsonValue* collision = node.Get("collision")) {
        object.collision.enabled = collision->GetBool("enabled", false);
        object.collision.shape =
            collision->GetString("shape", "Box") == "Sphere" ? ShapeSphere : ShapeBox;
        object.collision.size = collision->GetVec3("size", Vec3(1, 1, 1));
        object.collision.offset = collision->GetVec3("offset", Vec3(0, 0, 0));
    }
    if (const JsonValue* light = node.Get("light")) {
        object.light.enabled = light->GetBool("enabled", false);
        object.light.color = light->GetVec3("color", Vec3(1.0f, 0.95f, 0.85f));
        object.light.intensity = static_cast<float>(light->GetNumber("intensity", 1.0));
        object.light.range = static_cast<float>(light->GetNumber("range", 14.0));
    }
    if (const JsonValue* camera = node.Get("camera")) {
        object.camera.enabled = camera->GetBool("enabled", false);
        object.camera.fov = static_cast<float>(camera->GetNumber("fov", 60.0));
        object.camera.nearPlane = static_cast<float>(camera->GetNumber("near", 0.1));
        object.camera.farPlane = static_cast<float>(camera->GetNumber("far", 100.0));
    }
    return true;
}

} // namespace

bool ParseScene(const std::string& json, Scene& out, std::string& error) {
    JsonValue root;
    if (!ParseJson(json, root, error)) return false;
    if (root.type != JsonValue::Object) {
        error = "scene root must be an object";
        return false;
    }
    Scene scene;
    scene.name = root.GetString("name", "Scene");
    scene.ambient = root.GetVec3("ambient", Vec3(0.22f, 0.24f, 0.30f));
    scene.gravity = root.GetVec3("gravity", Vec3(0.0f, -9.81f, 0.0f));
    if (const JsonValue* objects = root.Get("objects")) {
        if (objects->type == JsonValue::Array) {
            for (const auto& item : objects->items) {
                if (item.type != JsonValue::Object) continue;
                GameObject object;
                ParseObjectJson(item, object);
                scene.objects.push_back(object);
            }
        }
    }
    scene.UpdateWorldMatrices();
    out = scene;
    return true;
}

// ==========================================================================
// Physics
// ==========================================================================

namespace {

void DecomposeWorld(const Mat4& m, Vec3& position, Vec3& scale) {
    position = Vec3(m.m[12], m.m[13], m.m[14]);
    scale = Vec3(Length(Vec3(m.m[0], m.m[1], m.m[2])), Length(Vec3(m.m[4], m.m[5], m.m[6])),
                 Length(Vec3(m.m[8], m.m[9], m.m[10])));
    if (scale.x < 1e-4f) scale.x = 1e-4f;
    if (scale.y < 1e-4f) scale.y = 1e-4f;
    if (scale.z < 1e-4f) scale.z = 1e-4f;
}

bool SphereSphere(const Vec3& ca, float ra, const Vec3& cb, float rb, Vec3& normal, float& penetration) {
    Vec3 delta = ca - cb;
    float distSq = Dot(delta, delta);
    float sum = ra + rb;
    if (distSq > sum * sum) return false;
    float dist = std::sqrt(distSq);
    if (dist > 1e-5f) {
        normal = delta * (1.0f / dist);
        penetration = sum - dist;
    } else {
        normal = Vec3(0, 1, 0);
        penetration = sum;
    }
    return true;
}

bool SphereAabb(const Vec3& center, float radius, const Vec3& boxCenter, const Vec3& half, Vec3& normal,
                float& penetration) {
    Vec3 closest(center.x < boxCenter.x - half.x ? boxCenter.x - half.x
                                                 : (center.x > boxCenter.x + half.x ? boxCenter.x + half.x : center.x),
                 center.y < boxCenter.y - half.y ? boxCenter.y - half.y
                                                 : (center.y > boxCenter.y + half.y ? boxCenter.y + half.y : center.y),
                 center.z < boxCenter.z - half.z ? boxCenter.z - half.z
                                                 : (center.z > boxCenter.z + half.z ? boxCenter.z + half.z : center.z));
    Vec3 delta = center - closest;
    float distSq = Dot(delta, delta);
    if (distSq > radius * radius) return false;
    float dist = std::sqrt(distSq);
    if (dist > 1e-5f) {
        normal = delta * (1.0f / dist);
        penetration = radius - dist;
    } else {
        Vec3 local = center - boxCenter;
        float px = half.x - std::fabs(local.x);
        float py = half.y - std::fabs(local.y);
        float pz = half.z - std::fabs(local.z);
        if (px < py && px < pz) {
            normal = Vec3(local.x >= 0 ? 1.0f : -1.0f, 0, 0);
            penetration = px + radius;
        } else if (py < pz) {
            normal = Vec3(0, local.y >= 0 ? 1.0f : -1.0f, 0);
            penetration = py + radius;
        } else {
            normal = Vec3(0, 0, local.z >= 0 ? 1.0f : -1.0f);
            penetration = pz + radius;
        }
    }
    return true;
}

bool AabbAabb(const Vec3& ca, const Vec3& ha, const Vec3& cb, const Vec3& hb, Vec3& normal,
              float& penetration) {
    Vec3 delta = cb - ca;
    float ox = (ha.x + hb.x) - std::fabs(delta.x);
    float oy = (ha.y + hb.y) - std::fabs(delta.y);
    float oz = (ha.z + hb.z) - std::fabs(delta.z);
    if (ox <= 0.0f || oy <= 0.0f || oz <= 0.0f) return false;
    if (ox < oy && ox < oz) {
        normal = Vec3(delta.x >= 0 ? -1.0f : 1.0f, 0, 0);
        penetration = ox;
    } else if (oy < oz) {
        normal = Vec3(0, delta.y >= 0 ? -1.0f : 1.0f, 0);
        penetration = oy;
    } else {
        normal = Vec3(0, 0, delta.z >= 0 ? -1.0f : 1.0f);
        penetration = oz;
    }
    return true;
}

float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

} // namespace

void Physics::Sync(const Scene& scene) {
    colliders_.clear();
    scene.UpdateWorldMatrices();
    for (size_t i = 0; i < scene.objects.size(); ++i) {
        const GameObject& object = scene.objects[i];
        if (!object.collision.enabled && !object.physics.enabled) continue;
        Collider c;
        c.objectId = object.id;
        c.shape = object.collision.shape;
        c.isDynamic = object.physics.IsDynamic();
        c.velocity = object.physics.velocity;
        c.restitution = object.physics.restitution;
        c.friction = object.physics.friction;
        Vec3 position, scale;
        DecomposeWorld(scene.worldMatrices[i], position, scale);
        c.center = Vec3(position.x + object.collision.offset.x * scale.x,
                        position.y + object.collision.offset.y * scale.y,
                        position.z + object.collision.offset.z * scale.z);
        if (c.shape == ShapeSphere) {
            float diameter = object.collision.size.x;
            if (object.collision.size.y > diameter) diameter = object.collision.size.y;
            if (object.collision.size.z > diameter) diameter = object.collision.size.z;
            float maxScale = scale.x;
            if (scale.y > maxScale) maxScale = scale.y;
            if (scale.z > maxScale) maxScale = scale.z;
            c.radius = 0.5f * diameter * maxScale;
            c.halfExtents = Vec3(c.radius, c.radius, c.radius);
        } else {
            c.halfExtents = Vec3(object.collision.size.x * 0.5f * scale.x,
                                 object.collision.size.y * 0.5f * scale.y,
                                 object.collision.size.z * 0.5f * scale.z);
            if (c.halfExtents.x < 0.01f) c.halfExtents.x = 0.01f;
            if (c.halfExtents.y < 0.01f) c.halfExtents.y = 0.01f;
            if (c.halfExtents.z < 0.01f) c.halfExtents.z = 0.01f;
            c.radius = c.halfExtents.x;
            if (c.halfExtents.y > c.radius) c.radius = c.halfExtents.y;
            if (c.halfExtents.z > c.radius) c.radius = c.halfExtents.z;
        }
        colliders_.push_back(c);
    }
}

void Physics::Step(Scene& scene, float dt) {
    if (dt <= 0.0f) return;
    if (dt > 0.05f) dt = 0.05f;

    Sync(scene);

    for (size_t i = 0; i < colliders_.size(); ++i) {
        if (!colliders_[i].isDynamic) continue;
        GameObject* object = scene.Find(colliders_[i].objectId);
        if (!object) continue;
        if (object->physics.useGravity) object->physics.velocity += scene.gravity * dt;
        object->transform.position += object->physics.velocity * dt;
        if (object->transform.position.y < -60.0f) {
            object->transform.position.y = 8.0f;
            object->physics.velocity = Vec3(0, 0, 0);
        }
        object->physics.grounded = false;
    }

    Sync(scene);

    for (size_t i = 0; i < colliders_.size(); ++i) {
        if (!colliders_[i].isDynamic) continue;
        GameObject* body = scene.Find(colliders_[i].objectId);
        if (!body) continue;
        Vec3 velocity = body->physics.velocity;

        for (size_t j = 0; j < colliders_.size(); ++j) {
            if (i == j) continue;
            Vec3 normal;
            float penetration = 0.0f;
            bool hit = false;
            if (colliders_[i].shape == ShapeSphere && colliders_[j].shape == ShapeSphere) {
                hit = SphereSphere(colliders_[i].center, colliders_[i].radius, colliders_[j].center,
                                   colliders_[j].radius, normal, penetration);
            } else if (colliders_[i].shape == ShapeSphere) {
                hit = SphereAabb(colliders_[i].center, colliders_[i].radius, colliders_[j].center,
                                 colliders_[j].halfExtents, normal, penetration);
            } else if (colliders_[j].shape == ShapeSphere) {
                hit = SphereAabb(colliders_[j].center, colliders_[j].radius, colliders_[i].center,
                                 colliders_[i].halfExtents, normal, penetration);
                normal = normal * -1.0f;
            } else {
                hit = AabbAabb(colliders_[i].center, colliders_[i].halfExtents, colliders_[j].center,
                               colliders_[j].halfExtents, normal, penetration);
            }
            if (!hit) continue;

            body->transform.position += normal * penetration;
            colliders_[i].center += normal * penetration;

            if (!colliders_[j].isDynamic) {
                float vn = Dot(velocity, normal);
                if (vn < 0.0f) {
                    float restitution = Clamp01(body->physics.restitution);
                    Vec3 normalComponent = normal * vn;
                    Vec3 tangent = velocity - normalComponent;
                    velocity = tangent * (1.0f - Clamp01(body->physics.friction) * 0.2f) +
                               normalComponent * -restitution;
                    if (normal.y > 0.5f && std::fabs(velocity.y) < 0.4f) {
                        velocity.y = 0.0f;
                        body->physics.grounded = true;
                    }
                }
            } else {
                GameObject* other = scene.Find(colliders_[j].objectId);
                if (other) {
                    other->transform.position -= normal * (penetration * 0.5f);
                    float vn = Dot(velocity - colliders_[j].velocity, normal);
                    if (vn < 0.0f) {
                        velocity = velocity - normal * (vn * 0.6f);
                        other->physics.velocity = other->physics.velocity + normal * (vn * 0.4f);
                    }
                }
            }
        }
        body->physics.velocity = velocity;
    }

    scene.UpdateWorldMatrices();
}

// ==========================================================================
// Primitive meshes (triangles with per-vertex normals -> GU flat shading)
// ==========================================================================

namespace {

constexpr float kPi = 3.14159265358979323846f;

Vec3 FaceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 ab = b - a;
    Vec3 ac = c - a;
    return Normalize(Vec3(ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z,
                          ab.x * ac.y - ab.y * ac.x));
}

} // namespace

void Mesh::PushTriangle(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& na, const Vec3& nb,
                        const Vec3& nc, unsigned int color) {
    // Per-vertex normals are kept in the signature so curved primitives can be
    // uploaded with GU_NORMAL_32BITF later on; the current pipeline lights via
    // GU per-object materials (sceGuModelColor), which is the cheapest path on
    // the PSP's GE.
    (void)na;
    (void)nb;
    (void)nc;
    const Vec3 positions[3] = {a, b, c};
    for (int i = 0; i < 3; ++i) {
        MeshVertex v;
        v.color = color;
        v.x = positions[i].x;
        v.y = positions[i].y;
        v.z = positions[i].z;
        vertices.push_back(v);
    }
    ++triangleCount;
}

namespace {

void AddTri(Mesh& mesh, const Vec3& a, const Vec3& b, const Vec3& c, unsigned int color,
            bool normalizeObjectSpace = true) {
    Vec3 n = FaceNormal(a, b, c);
    if (normalizeObjectSpace) {
        // Per-vertex normals for smooth shading on curved primitives are
        // approximated by the face normal (GU flat shading, PSP-friendly).
    }
    mesh.PushTriangle(a, b, c, n, n, n, color);
}

void BuildCube(Mesh& mesh, unsigned int color) {
    const Vec3 v[8] = {
        Vec3(-0.5f, -0.5f, -0.5f), Vec3(0.5f, -0.5f, -0.5f), Vec3(0.5f, 0.5f, -0.5f),
        Vec3(-0.5f, 0.5f, -0.5f),  Vec3(-0.5f, -0.5f, 0.5f), Vec3(0.5f, -0.5f, 0.5f),
        Vec3(0.5f, 0.5f, 0.5f),    Vec3(-0.5f, 0.5f, 0.5f)};
    const int quads[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {3, 2, 6, 7},
                             {0, 1, 5, 4}, {5, 1, 2, 6}, {0, 4, 7, 3}};
    for (const auto& q : quads) {
        AddTri(mesh, v[q[0]], v[q[1]], v[q[2]], color);
        AddTri(mesh, v[q[0]], v[q[2]], v[q[3]], color);
    }
}

void BuildSphere(Mesh& mesh, unsigned int color, int segments = 12) {
    int stacks = segments;
    int slices = segments * 2;
    for (int i = 0; i < stacks; ++i) {
        float phi0 = static_cast<float>(i) / stacks * kPi;
        float phi1 = static_cast<float>(i + 1) / stacks * kPi;
        for (int j = 0; j < slices; ++j) {
            float theta0 = static_cast<float>(j) / slices * 2.0f * kPi;
            float theta1 = static_cast<float>(j + 1) / slices * 2.0f * kPi;
            Vec3 p00(std::sin(phi0) * std::cos(theta0) * 0.5f, std::cos(phi0) * 0.5f,
                     std::sin(phi0) * std::sin(theta0) * 0.5f);
            Vec3 p01(std::sin(phi0) * std::cos(theta1) * 0.5f, std::cos(phi0) * 0.5f,
                     std::sin(phi0) * std::sin(theta1) * 0.5f);
            Vec3 p10(std::sin(phi1) * std::cos(theta0) * 0.5f, std::cos(phi1) * 0.5f,
                     std::sin(phi1) * std::sin(theta0) * 0.5f);
            Vec3 p11(std::sin(phi1) * std::cos(theta1) * 0.5f, std::cos(phi1) * 0.5f,
                     std::sin(phi1) * std::sin(theta1) * 0.5f);
            AddTri(mesh, p00, p10, p01, color);
            AddTri(mesh, p01, p10, p11, color);
        }
    }
}

void BuildCone(Mesh& mesh, unsigned int color, int segments = 16) {
    Vec3 tip(0.0f, 0.5f, 0.0f);
    for (int j = 0; j < segments; ++j) {
        float a0 = static_cast<float>(j) / segments * 2.0f * kPi;
        float a1 = static_cast<float>(j + 1) / segments * 2.0f * kPi;
        Vec3 b0(std::cos(a0) * 0.5f, -0.5f, std::sin(a0) * 0.5f);
        Vec3 b1(std::cos(a1) * 0.5f, -0.5f, std::sin(a1) * 0.5f);
        AddTri(mesh, b0, b1, tip, color);
        AddTri(mesh, Vec3(0.0f, -0.5f, 0.0f), b1, b0, color);
    }
}

void BuildCylinder(Mesh& mesh, unsigned int color, int segments = 16) {
    for (int j = 0; j < segments; ++j) {
        float a0 = static_cast<float>(j) / segments * 2.0f * kPi;
        float a1 = static_cast<float>(j + 1) / segments * 2.0f * kPi;
        Vec3 b0(std::cos(a0) * 0.5f, -0.5f, std::sin(a0) * 0.5f);
        Vec3 b1(std::cos(a1) * 0.5f, -0.5f, std::sin(a1) * 0.5f);
        Vec3 t0(std::cos(a0) * 0.5f, 0.5f, std::sin(a0) * 0.5f);
        Vec3 t1(std::cos(a1) * 0.5f, 0.5f, std::sin(a1) * 0.5f);
        AddTri(mesh, b0, t0, b1, color);
        AddTri(mesh, b1, t0, t1, color);
        AddTri(mesh, Vec3(0.0f, 0.5f, 0.0f), t0, t1, color);
        AddTri(mesh, Vec3(0.0f, -0.5f, 0.0f), b1, b0, color);
    }
}

void BuildPlane(Mesh& mesh, unsigned int color) {
    Vec3 a(-0.5f, 0.0f, -0.5f), b(0.5f, 0.0f, -0.5f), c(0.5f, 0.0f, 0.5f), d(-0.5f, 0.0f, 0.5f);
    AddTri(mesh, a, c, b, color);
    AddTri(mesh, a, d, c, color);
}

} // namespace

void BuildPrimitiveMesh(PrimitiveType type, unsigned int color, Mesh& out) {
    out.vertices.clear();
    out.triangleCount = 0;
    switch (type) {
        case PrimCube: BuildCube(out, color); break;
        case PrimSphere: BuildSphere(out, color); break;
        case PrimCone: BuildCone(out, color); break;
        case PrimCylinder: BuildCylinder(out, color); break;
        case PrimPlane: BuildPlane(out, color); break;
        default: break;
    }
}

// ------------------------------------------------------------------------
//  glTF geometry -> GU triangles
// ------------------------------------------------------------------------
namespace {
// Same packing as RuntimeApp's ColorRGBA (GU_COLOR_8888: RGBA in little endian).
unsigned int PackColor(float r, float g, float b, float a) {
    auto clamp8 = [](float v) -> unsigned int {
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        return static_cast<unsigned int>(v * 255.0f + 0.5f);
    };
    return clamp8(r) | (clamp8(g) << 8) | (clamp8(b) << 16) | (clamp8(a) << 24);
}
} // namespace

void BuildMeshFromGltf(const GltfMeshData& source, unsigned int color, Mesh& out) {
    out.vertices.clear();
    out.triangleCount = 0;
    const size_t vertexCount = source.positions.size() / 3;
    if (vertexCount == 0) return;

    const float baseR = static_cast<float>((color >> 0) & 0xFF) / 255.0f;
    const float baseG = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
    const float baseB = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
    const float baseA = static_cast<float>((color >> 24) & 0xFF) / 255.0f;

    auto shade = [&](size_t index) -> unsigned int {
        // Simple hemispherical term: normals pointing at the sky get full
        // brightness, downward faces are darkened. Enough depth for a model
        // without paying for per-vertex normals in the GU stream.
        float ny = 0.0f;
        if (source.normals.size() >= (index + 1) * 3) ny = source.normals[index * 3 + 1];
        const float lit = 0.62f + 0.38f * (ny * 0.5f + 0.5f);
        return PackColor(baseR * lit, baseG * lit, baseB * lit, baseA);
    };

    auto push = [&](size_t index) {
        if (index >= vertexCount) return;
        MeshVertex v;
        v.color = shade(index);
        v.x = source.positions[index * 3 + 0];
        v.y = source.positions[index * 3 + 1];
        v.z = source.positions[index * 3 + 2];
        out.vertices.push_back(v);
    };

    if (source.indices.empty()) {
        for (size_t i = 0; i + 2 < vertexCount; i += 3) {
            push(i);
            push(i + 1);
            push(i + 2);
            ++out.triangleCount;
        }
        return;
    }

    for (size_t i = 0; i + 2 < source.indices.size(); i += 3) {
        push(source.indices[i]);
        push(source.indices[i + 1]);
        push(source.indices[i + 2]);
        ++out.triangleCount;
    }
}

} // namespace pspruntime
