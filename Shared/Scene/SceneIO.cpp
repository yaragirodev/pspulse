// ---------------------------------------------------------------------------
//  PSPulse - JSON scene & project serialisation
// ---------------------------------------------------------------------------

#include "SceneIO.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "../BuildInfo.h"

using nlohmann::json;

namespace pspulse {

namespace {

json Vec3ToJson(const glm::vec3& v) { return json::array({v.x, v.y, v.z}); }
json Vec4ToJson(const glm::vec4& v) { return json::array({v.x, v.y, v.z, v.w}); }

glm::vec3 Vec3FromJson(const json& j, const glm::vec3& fallback) {
    if (!j.is_array() || j.size() < 3) return fallback;
    return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

glm::vec4 Vec4FromJson(const json& j, const glm::vec4& fallback) {
    if (!j.is_array() || j.size() < 4) return fallback;
    return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
}

template <typename T>
T ValueOr(const json& parent, const char* key, T fallback) {
    auto it = parent.find(key);
    if (it == parent.end() || it->is_null()) return fallback;
    try {
        return it->get<T>();
    } catch (...) {
        return fallback;
    }
}

json SerializeTransform(const Transform& t) {
    return json{{"position", Vec3ToJson(t.position)},
                {"rotation", Vec3ToJson(t.rotation)},
                {"scale", Vec3ToJson(t.scale)}};
}

Transform DeserializeTransform(const json& j) {
    Transform t;
    if (!j.is_object()) return t;
    if (j.contains("position")) t.position = Vec3FromJson(j["position"], t.position);
    if (j.contains("rotation")) t.rotation = Vec3FromJson(j["rotation"], t.rotation);
    if (j.contains("scale")) t.scale = Vec3FromJson(j["scale"], t.scale);
    return t;
}

json SerializeObject(const GameObject& o) {
    json physics = {{"enabled", o.physics.enabled},
                    {"mass", o.physics.mass},
                    {"useGravity", o.physics.useGravity},
                    {"isStatic", o.physics.isStatic},
                    {"restitution", o.physics.restitution},
                    {"friction", o.physics.friction},
                    {"velocity", Vec3ToJson(o.physics.velocity)}};

    json collision = {{"enabled", o.collision.enabled},
                      {"shape", CollisionShapeToString(o.collision.shape)},
                      {"size", Vec3ToJson(o.collision.size)},
                      {"offset", Vec3ToJson(o.collision.offset)}};

    json light = {{"enabled", o.light.enabled},
                  {"color", Vec3ToJson(o.light.color)},
                  {"intensity", o.light.intensity},
                  {"range", o.light.range}};

    json camera = {{"enabled", o.camera.enabled},
                   {"fov", o.camera.fov},
                   {"near", o.camera.nearPlane},
                   {"far", o.camera.farPlane},
                   {"active", o.camera.active}};

    return json{{"id", o.id},
                {"name", o.name},
                {"parent", o.parent},
                {"primitive", PrimitiveToString(o.primitive)},
                {"mesh_path", o.meshPath},
                {"visible", o.visible},
                {"color", Vec4ToJson(o.color)},
                {"transform", SerializeTransform(o.transform)},
                {"script", o.script},
                {"physics", physics},
                {"collision", collision},
                {"light", light},
                {"camera", camera}};
}

GameObject DeserializeObject(const json& j) {
    GameObject o;
    o.id = ValueOr<int>(j, "id", 0);
    o.name = ValueOr<std::string>(j, "name", "GameObject");
    o.parent = ValueOr<int>(j, "parent", -1);
    o.primitive = PrimitiveFromString(ValueOr<std::string>(j, "primitive", "Empty"));
    if (j.contains("mesh_path")) o.meshPath = ValueOr<std::string>(j, "mesh_path", "");
    o.visible = ValueOr<bool>(j, "visible", true);
    if (j.contains("color")) o.color = Vec4FromJson(j["color"], o.color);
    if (j.contains("transform")) o.transform = DeserializeTransform(j["transform"]);
    o.script = ValueOr<std::string>(j, "script", "");

    if (j.contains("physics") && j["physics"].is_object()) {
        const json& p = j["physics"];
        o.physics.enabled = ValueOr<bool>(p, "enabled", false);
        o.physics.mass = ValueOr<float>(p, "mass", 1.0f);
        o.physics.useGravity = ValueOr<bool>(p, "useGravity", true);
        o.physics.isStatic = ValueOr<bool>(p, "isStatic", false);
        o.physics.restitution = ValueOr<float>(p, "restitution", 0.2f);
        o.physics.friction = ValueOr<float>(p, "friction", 0.6f);
        if (p.contains("velocity")) o.physics.velocity = Vec3FromJson(p["velocity"], o.physics.velocity);
    }
    if (j.contains("collision") && j["collision"].is_object()) {
        const json& c = j["collision"];
        o.collision.enabled = ValueOr<bool>(c, "enabled", false);
        o.collision.shape = CollisionShapeFromString(ValueOr<std::string>(c, "shape", "Box"));
        if (c.contains("size")) o.collision.size = Vec3FromJson(c["size"], o.collision.size);
        if (c.contains("offset")) o.collision.offset = Vec3FromJson(c["offset"], o.collision.offset);
    }
    if (j.contains("light") && j["light"].is_object()) {
        const json& l = j["light"];
        o.light.enabled = ValueOr<bool>(l, "enabled", false);
        if (l.contains("color")) o.light.color = Vec3FromJson(l["color"], o.light.color);
        o.light.intensity = ValueOr<float>(l, "intensity", 1.0f);
        o.light.range = ValueOr<float>(l, "range", 14.0f);
    }
    if (j.contains("camera") && j["camera"].is_object()) {
        const json& c = j["camera"];
        o.camera.enabled = ValueOr<bool>(c, "enabled", false);
        o.camera.fov = ValueOr<float>(c, "fov", 60.0f);
        o.camera.nearPlane = ValueOr<float>(c, "near", 0.1f);
        o.camera.farPlane = ValueOr<float>(c, "far", 100.0f);
        o.camera.active = ValueOr<bool>(c, "active", false);
    }
    return o;
}

} // namespace

std::string SceneToJson(const Scene& scene) {
    json root;
    root["format"] = PSPULSE_SCENE_FORMAT;
    root["formatVersion"] = PSPULSE_SCENE_VERSION;
    root["engine"] = PSPULSE_VERSION_STR;
    root["name"] = scene.name;
    root["version"] = scene.version;
    root["ambient"] = Vec3ToJson(scene.ambient);
    root["gravity"] = Vec3ToJson(scene.gravity);
    root["nextId"] = scene.nextId;

    json objects = json::array();
    for (const auto& object : scene.objects) objects.push_back(SerializeObject(object));
    root["objects"] = objects;

    return root.dump(2);
}

bool SceneFromJson(const std::string& text, Scene& outScene, std::string& error) {
    error.clear();
    json root;
    try {
        root = json::parse(text);
    } catch (const std::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }
    if (!root.is_object()) {
        error = "Scene root must be a JSON object.";
        return false;
    }
    Scene scene;
    scene.name = ValueOr<std::string>(root, "name", "Untitled Scene");
    scene.version = ValueOr<int>(root, "version", 1);
    if (root.contains("ambient")) scene.ambient = Vec3FromJson(root["ambient"], scene.ambient);
    if (root.contains("gravity")) scene.gravity = Vec3FromJson(root["gravity"], scene.gravity);
    scene.nextId = ValueOr<int>(root, "nextId", 1);
    scene.objects.clear();
    if (root.contains("objects") && root["objects"].is_array()) {
        for (const auto& item : root["objects"]) {
            if (!item.is_object()) continue;
            scene.objects.push_back(DeserializeObject(item));
        }
    }
    scene.EnsureConsistentIds();
    outScene = std::move(scene);
    return true;
}

bool SaveSceneToFile(const Scene& scene, const std::string& path, std::string& error) {
    error.clear();
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        error = "Cannot open file for writing: " + path;
        return false;
    }
    file << SceneToJson(scene);
    if (!file.good()) {
        error = "Write error: " + path;
        return false;
    }
    return true;
}

bool LoadSceneFromFile(const std::string& path, Scene& outScene, std::string& error) {
    error.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        error = "Cannot open file: " + path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return SceneFromJson(buffer.str(), outScene, error);
}

Scene MakeDefaultScene() {
    Scene scene;
    scene.name = "Main Scene";

    // Ground.
    GameObject& ground = scene.AddObject("Ground", PrimitiveType::Plane);
    ground.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    ground.transform.scale = glm::vec3(20.0f, 1.0f, 20.0f);
    ground.color = glm::vec4(0.42f, 0.45f, 0.50f, 1.0f);
    ground.physics.enabled = true;
    ground.physics.isStatic = true;
    ground.collision.enabled = true;
    ground.collision.shape = CollisionShapeType::Box;
    ground.collision.size = glm::vec3(20.0f, 0.1f, 20.0f);
    ground.collision.offset = glm::vec3(0.0f, -0.05f, 0.0f);

    // Boulder.
    GameObject& cube = scene.AddObject("Cube", PrimitiveType::Cube);
    cube.transform.position = glm::vec3(-1.6f, 3.0f, 0.0f);
    cube.transform.rotation = glm::vec3(15.0f, 30.0f, 0.0f);
    cube.color = glm::vec4(0.88f, 0.52f, 0.30f, 1.0f);
    cube.physics.enabled = true;
    cube.physics.mass = 2.0f;
    cube.collision.enabled = true;
    cube.collision.shape = CollisionShapeType::Box;
    cube.collision.size = glm::vec3(1.0f, 1.0f, 1.0f);

    // Sphere with the example Lua script.
    GameObject& sphere = scene.AddObject("Sphere", PrimitiveType::Sphere);
    sphere.transform.position = glm::vec3(1.4f, 4.0f, 0.0f);
    sphere.color = glm::vec4(0.35f, 0.62f, 0.95f, 1.0f);
    sphere.physics.enabled = true;
    sphere.physics.mass = 1.0f;
    sphere.physics.restitution = 0.55f;
    sphere.collision.enabled = true;
    sphere.collision.shape = CollisionShapeType::Sphere;
    sphere.collision.size = glm::vec3(1.0f, 1.0f, 1.0f);
    sphere.script = "scripts/spin.lua";

    // Cone spinning on top of a cylinder, both parented to an empty pivot.
    GameObject& pivot = scene.AddObject("Totem", PrimitiveType::Empty);
    pivot.transform.position = glm::vec3(3.6f, 0.0f, -2.2f);

    GameObject& column = scene.AddObject("Column", PrimitiveType::Cylinder);
    column.transform.position = glm::vec3(3.6f, 1.0f, -2.2f);
    column.transform.scale = glm::vec3(1.2f, 2.0f, 1.2f);
    column.color = glm::vec4(0.55f, 0.80f, 0.62f, 1.0f);
    column.parent = pivot.id;

    GameObject& cap = scene.AddObject("Cone", PrimitiveType::Cone);
    cap.transform.position = glm::vec3(3.6f, 2.6f, -2.2f);
    cap.transform.scale = glm::vec3(1.4f, 1.2f, 1.4f);
    cap.color = glm::vec4(0.95f, 0.86f, 0.35f, 1.0f);
    cap.parent = pivot.id;

    // Light & camera.
    GameObject sun = Scene::MakeLight("Sun");
    sun.id = scene.AllocateId();
    scene.objects.push_back(sun);

    GameObject camera = Scene::MakeCamera("Main Camera");
    camera.id = scene.AllocateId();
    scene.objects.push_back(camera);

    scene.EnsureConsistentIds();
    return scene;
}

} // namespace pspulse
