// ---------------------------------------------------------------------------
//  PSP Engine - glTF 2.0 mesh loader (implementation)
// ---------------------------------------------------------------------------

#include "GltfLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>

#if defined(PSP_ENGINE_TINY_JSON)
// PSP runtime: reuse the dependency-free JSON DOM that already ships with the
// runtime. Pulling nlohmann/json into a 28 MB handheld is not worth ~700 KB.
#include "../Json/TinyJsonShim.h"
#else
#include <nlohmann/json.hpp>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace pspengine {

namespace {

using json = nlohmann::json;

constexpr uint32_t kGlbMagic = 0x46546C67; // "glTF"
constexpr uint32_t kGlbChunkJson = 0x4E4F534A;
constexpr uint32_t kGlbChunkBin = 0x004E4942;

// -------------------------------------------------------------------------
//  Buffer helpers
// -------------------------------------------------------------------------
bool Base64Decode(const std::string& text, std::vector<unsigned char>& out) {
    static const int8_t kTable[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63, 52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, 15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40, 41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    out.clear();
    out.reserve(text.size() * 3 / 4 + 3);
    uint32_t accumulator = 0;
    int bits = 0;
    for (unsigned char c : text) {
        const int8_t value = kTable[c];
        if (value < 0) {
            if (c == '=') break;
            continue; // whitespace / newlines in data URIs
        }
        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((accumulator >> bits) & 0xFF));
        }
    }
    return !out.empty();
}

struct BufferView {
    size_t byteOffset = 0;
    size_t byteLength = 0;
    size_t byteStride = 0; // 0 => tightly packed
};

struct Loader {
    json document;
    std::vector<std::vector<unsigned char>> buffers;
    std::vector<BufferView> views;
    MeshData mesh;
    GltfLoadStats stats;
    std::string error;

    void Warn(const std::string& message) {
        if (stats.warnings.size() < 16) stats.warnings.push_back(message);
    }

    bool Fail(const std::string& message) {
        error = message;
        return false;
    }

    // ---- buffers ---------------------------------------------------------
    bool LoadBuffers(const std::string& baseDir) {
        if (!document.contains("buffers")) return true;
        for (const auto& buffer : document["buffers"]) {
            std::vector<unsigned char> data;
            if (buffer.contains("uri")) {
                const std::string uri = buffer["uri"].get<std::string>();
                if (uri.rfind("data:", 0) == 0) {
                    const size_t comma = uri.find(',');
                    if (comma == std::string::npos) return Fail("malformed data URI in buffer");
                    if (!Base64Decode(uri.substr(comma + 1), data)) {
                        return Fail("cannot base64-decode an embedded buffer");
                    }
                } else {
                    std::string path = baseDir.empty() ? uri : baseDir + "/" + uri;
                    std::ifstream file(path, std::ios::binary);
                    if (!file) return Fail("cannot open buffer file: " + path);
                    file.seekg(0, std::ios::end);
                    const std::streamoff length = file.tellg();
                    file.seekg(0, std::ios::beg);
                    data.resize(static_cast<size_t>(std::max<std::streamoff>(0, length)));
                    if (!data.empty()) file.read(reinterpret_cast<char*>(data.data()), length);
                }
            } else {
                // GLB stored its binary chunk here before parsing started.
                if (stats.usedBinaryContainer && !buffers.empty() && buffers[0].empty()) {
                    // handled by caller injecting the chunk
                }
            }
            buffers.push_back(std::move(data));
        }
        return true;
    }

    // ---- buffer views ----------------------------------------------------
    bool LoadBufferViews() {
        if (document.contains("bufferViews")) {
            for (const auto& view : document["bufferViews"]) {
                BufferView out;
                out.byteOffset = view.value("byteOffset", 0u);
                out.byteLength = view.value("byteLength", 0u);
                out.byteStride = view.value("byteStride", 0u);
                views.push_back(out);
            }
        }
        return true;
    }

    const unsigned char* ViewData(int viewIndex, size_t offset, size_t size) {
        if (viewIndex < 0 || viewIndex >= static_cast<int>(views.size())) return nullptr;
        const BufferView& view = views[viewIndex];
        const json& viewJson = document["bufferViews"][viewIndex];
        const int bufferIndex = viewJson.value("buffer", -1);
        if (bufferIndex < 0 || bufferIndex >= static_cast<int>(buffers.size())) return nullptr;
        const auto& buffer = buffers[bufferIndex];
        const size_t start = view.byteOffset + offset;
        if (start + size > buffer.size()) return nullptr;
        return buffer.data() + start;
    }

    // ---- accessors -------------------------------------------------------
    static int ComponentSize(int componentType) {
        switch (componentType) {
            case 5120: case 5121: return 1;                  // BYTE / UNSIGNED_BYTE
            case 5122: case 5123: return 2;                  // SHORT / UNSIGNED_SHORT
            case 5125: case 5126: return 4;                  // UNSIGNED_INT / FLOAT
            default: return 0;
        }
    }

    static int ComponentCount(const std::string& type) {
        if (type == "SCALAR") return 1;
        if (type == "VEC2") return 2;
        if (type == "VEC3") return 3;
        if (type == "VEC4") return 4;
        if (type == "MAT4") return 16;
        return 0;
    }

    // Reads a float component, normalising integer types per glTF spec.
    static float ReadComponent(const unsigned char* p, int componentType, bool normalized) {
        switch (componentType) {
            case 5126: { float v; std::memcpy(&v, p, 4); return v; }
            case 5121: return normalized ? p[0] / 255.0f : static_cast<float>(p[0]);
            case 5120: return normalized ? std::max(-1.0f, p[0] / 127.0f) : static_cast<float>(static_cast<int8_t>(p[0]));
            case 5123: { uint16_t v; std::memcpy(&v, p, 2); return normalized ? v / 65535.0f : static_cast<float>(v); }
            case 5122: { int16_t v; std::memcpy(&v, p, 2); return normalized ? std::max(-1.0f, v / 32767.0f) : static_cast<float>(v); }
            case 5125: { uint32_t v; std::memcpy(&v, p, 4); return static_cast<float>(v); }
            default: return 0.0f;
        }
    }

    // Reads an index component as uint32.
    static uint32_t ReadIndex(const unsigned char* p, int componentType) {
        switch (componentType) {
            case 5121: return p[0];
            case 5123: { uint16_t v; std::memcpy(&v, p, 2); return v; }
            case 5125: { uint32_t v; std::memcpy(&v, p, 4); return v; }
            default: return 0;
        }
    }

    // Appends `components` floats from accessor `index` into `out`.
    bool ReadFloatAccessor(int index, int components, std::vector<float>& out) {
        if (!document.contains("accessors") || index < 0 ||
            index >= static_cast<int>(document["accessors"].size())) {
            return Fail("accessor " + std::to_string(index) + " is out of range");
        }
        const json& accessor = document["accessors"][index];
        const int count = accessor.value("count", 0);
        if (count <= 0) return true;

        const std::string type = accessor.value("type", "SCALAR");
        const int declared = ComponentCount(type);
        const int componentType = accessor.value("componentType", 5126);
        const bool normalized = accessor.value("normalized", false);
        const int componentSize = ComponentSize(componentType);
        if (componentSize == 0) return Fail("unsupported accessor componentType");
        if (declared < components) return Fail("accessor " + std::to_string(index) + " has too few components");

        // Sparse accessors are rare; support them rather than silently misread.
        if (accessor.contains("sparse")) {
            Warn("sparse accessors are not supported - using the base values");
        }
        if (!accessor.contains("bufferView")) return true; // all zeros per spec

        const int viewIndex = accessor["bufferView"].get<int>();
        const size_t accessorOffset = accessor.value("byteOffset", 0u);
        const BufferView& view = views[viewIndex];
        const size_t elementSize = static_cast<size_t>(componentSize) * declared;
        const size_t stride = view.byteStride ? view.byteStride : elementSize;

        out.resize(out.size() + static_cast<size_t>(count) * components);
        float* destination = out.data() + out.size() - static_cast<size_t>(count) * components;

        const unsigned char* base = ViewData(viewIndex, accessorOffset, count * stride);
        if (!base) return Fail("accessor " + std::to_string(index) + " points outside its buffer");

        for (int i = 0; i < count; ++i) {
            const unsigned char* element = base + static_cast<size_t>(i) * stride;
            for (int c = 0; c < components; ++c) {
                destination[i * components + c] =
                    ReadComponent(element + static_cast<size_t>(c) * componentSize, componentType, normalized);
            }
        }
        return true;
    }

    bool ReadIndexAccessor(int index, std::vector<uint32_t>& out) {
        if (!document.contains("accessors") || index < 0 ||
            index >= static_cast<int>(document["accessors"].size())) {
            return Fail("index accessor is out of range");
        }
        const json& accessor = document["accessors"][index];
        const int count = accessor.value("count", 0);
        if (count <= 0) return true;
        if (!accessor.contains("bufferView")) return true;

        const int componentType = accessor.value("componentType", 5123);
        const int componentSize = ComponentSize(componentType);
        if (componentSize == 0) return Fail("unsupported index componentType");

        const int viewIndex = accessor["bufferView"].get<int>();
        const size_t accessorOffset = accessor.value("byteOffset", 0u);
        const BufferView& view = views[viewIndex];
        const size_t stride = view.byteStride ? view.byteStride : static_cast<size_t>(componentSize);

        out.resize(out.size() + static_cast<size_t>(count));
        uint32_t* destination = out.data() + out.size() - count;

        const unsigned char* base = ViewData(viewIndex, accessorOffset, count * stride);
        if (!base) return Fail("index accessor points outside its buffer");
        for (int i = 0; i < count; ++i) {
            destination[i] = ReadIndex(base + static_cast<size_t>(i) * stride, componentType);
        }
        return true;
    }

    // ---- node traversal --------------------------------------------------
    // Flattens every mesh instance into one MeshData, baking node transforms.
    bool AppendNode(int nodeIndex, const glm::mat4& parentTransform, int depth) {
        if (depth > 64) return Fail("glTF node hierarchy is too deep (cycle?)");
        const json& node = document["nodes"][nodeIndex];
        ++stats.nodes;

        glm::mat4 local(1.0f);
        if (node.contains("matrix")) {
            const auto& m = node["matrix"];
            if (m.size() == 16) {
                for (int column = 0; column < 4; ++column) {
                    for (int row = 0; row < 4; ++row) {
                        local[column][row] = m[column * 4 + row].get<float>();
                    }
                }
            }
        } else {
            glm::vec3 translation(0.0f);
            glm::vec3 scale(1.0f);
            glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
            if (node.contains("translation")) {
                const auto& t = node["translation"];
                translation = glm::vec3(t[0].get<float>(), t[1].get<float>(), t[2].get<float>());
            }
            if (node.contains("scale")) {
                const auto& s = node["scale"];
                scale = glm::vec3(s[0].get<float>(), s[1].get<float>(), s[2].get<float>());
            }
            if (node.contains("rotation")) {
                const auto& r = node["rotation"];
                rotation = glm::quat(r[3].get<float>(), r[0].get<float>(), r[1].get<float>(),
                                     r[2].get<float>());
            }
            local = glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) *
                    glm::scale(glm::mat4(1.0f), scale);
        }
        const glm::mat4 world = parentTransform * local;

        if (node.contains("mesh") && !node.value("skipMesh", false)) {
            if (!AppendMesh(node["mesh"].get<int>(), world)) return false;
        }
        if (node.contains("children")) {
            for (const auto& child : node["children"]) {
                if (!AppendNode(child.get<int>(), world, depth + 1)) return false;
            }
        }
        return true;
    }

    bool AppendMesh(int meshIndex, const glm::mat4& transform) {
        if (meshIndex < 0 || meshIndex >= static_cast<int>(document["meshes"].size())) {
            return Fail("mesh index out of range");
        }
        ++stats.meshes;
        const json& meshJson = document["meshes"][meshIndex];
        if (!meshJson.contains("primitives")) return true;

        // Normals need the inverse-transpose; scale is uniform in practice but
        // handle the general case with a proper 3x3 normal matrix.
        const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

        for (const auto& primitive : meshJson["primitives"]) {
            const int mode = primitive.value("mode", 4); // 4 == TRIANGLES
            if (mode != 4) {
                Warn("skipping primitive with mode " + std::to_string(mode) + " (only TRIANGLES)");
                continue;
            }
            if (!primitive.contains("attributes")) continue;
            const json& attributes = primitive["attributes"];
            if (!attributes.contains("POSITION")) {
                Warn("skipping primitive without POSITION");
                continue;
            }
            ++stats.primitives;

            std::vector<float> positions;
            std::vector<float> normals;
            std::vector<float> uvs;
            if (!ReadFloatAccessor(attributes["POSITION"].get<int>(), 3, positions)) return false;
            if (attributes.contains("NORMAL")) {
                if (!ReadFloatAccessor(attributes["NORMAL"].get<int>(), 3, normals)) return false;
            }
            if (attributes.contains("TEXCOORD_0")) {
                if (!ReadFloatAccessor(attributes["TEXCOORD_0"].get<int>(), 2, uvs)) return false;
            }

            const size_t vertexCount = positions.size() / 3;
            if (vertexCount == 0) continue;
            const uint32_t baseVertex = static_cast<uint32_t>(mesh.positions.size() / 3);

            for (size_t i = 0; i < vertexCount; ++i) {
                const glm::vec4 p(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2], 1.0f);
                const glm::vec4 world = transform * p;
                mesh.positions.push_back(world.x);
                mesh.positions.push_back(world.y);
                mesh.positions.push_back(world.z);

                if (normals.size() >= (i + 1) * 3) {
                    const glm::vec3 n(normals[i * 3 + 0], normals[i * 3 + 1], normals[i * 3 + 2]);
                    glm::vec3 transformed = normalMatrix * n;
                    const float length = glm::length(transformed);
                    if (length > 1e-6f) transformed /= length;
                    mesh.normals.push_back(transformed.x);
                    mesh.normals.push_back(transformed.y);
                    mesh.normals.push_back(transformed.z);
                } else {
                    mesh.normals.push_back(0.0f);
                    mesh.normals.push_back(0.0f);
                    mesh.normals.push_back(1.0f);
                }

                if (uvs.size() >= (i + 1) * 2) {
                    mesh.uvs.push_back(uvs[i * 2 + 0]);
                    mesh.uvs.push_back(uvs[i * 2 + 1]);
                } else {
                    mesh.uvs.push_back(0.0f);
                    mesh.uvs.push_back(0.0f);
                }
            }

            if (primitive.contains("indices")) {
                std::vector<uint32_t> indices;
                if (!ReadIndexAccessor(primitive["indices"].get<int>(), indices)) return false;
                for (uint32_t index : indices) {
                    if (index >= vertexCount) continue; // malformed data guard
                    mesh.indices.push_back(baseVertex + index);
                }
            } else {
                for (uint32_t i = 0; i < vertexCount; ++i) mesh.indices.push_back(baseVertex + i);
            }
        }
        return true;
    }
};

// Injects the GLB BIN chunk as buffer 0 when the JSON declares one without a URI.
std::vector<unsigned char> gGlbBinChunk;

std::string DirectoryOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash);
}

bool ParseDocument(const unsigned char* data, size_t size, const std::string& sourceName,
                   bool allowGlb, json& document, std::vector<unsigned char>& binChunk,
                   GltfLoadStats& stats, std::string& error) {
    if (!data || size == 0) {
        error = sourceName + ": empty file";
        return false;
    }

    const bool looksLikeGlb = size >= 12 && std::memcmp(data, &kGlbMagic, 4) == 0;
    if (!allowGlb && looksLikeGlb) {
        error = sourceName + ": GLB containers are not supported in this context";
        return false;
    }

    if (looksLikeGlb) {
        stats.usedBinaryContainer = true;
        uint32_t totalLength = 0, version = 0;
        std::memcpy(&totalLength, data + 8, 4);
        std::memcpy(&version, data + 4, 4);
        if (version != 2) {
            error = sourceName + ": unsupported GLB version " + std::to_string(version);
            return false;
        }
        const size_t limit = std::min<size_t>(size, totalLength == 0 ? size : totalLength);
        size_t offset = 12;
        bool haveJson = false;
        while (offset + 8 <= limit) {
            uint32_t chunkLength = 0, chunkType = 0;
            std::memcpy(&chunkLength, data + offset, 4);
            std::memcpy(&chunkType, data + offset + 4, 4);
            offset += 8;
            if (offset + chunkLength > limit) break;
            if (chunkType == kGlbChunkJson) {
                document = json::parse(reinterpret_cast<const char*>(data + offset),
                                       reinterpret_cast<const char*>(data + offset) + chunkLength,
                                       nullptr, false);
                haveJson = true;
            } else if (chunkType == kGlbChunkBin) {
                binChunk.assign(data + offset, data + offset + chunkLength);
            }
            offset += chunkLength;
            if (offset % 4 != 0) offset += 4 - (offset % 4);
        }
        if (!haveJson || document.is_discarded()) {
            error = sourceName + ": GLB has no valid JSON chunk";
            return false;
        }
    } else {
        document = json::parse(reinterpret_cast<const char*>(data),
                               reinterpret_cast<const char*>(data) + size, nullptr, false);
        if (document.is_discarded()) {
            error = sourceName + ": not a valid glTF JSON document";
            return false;
        }
    }

    if (!document.contains("asset") || document.value("asset", json::object()).value("version", "") != "2.0") {
        error = sourceName + ": only glTF 2.0 is supported";
        return false;
    }
    return true;
}

bool FinishLoad(Loader& loader, MeshData& out, GltfLoadStats& stats, std::string& error,
                const std::string& sourceName) {
    // Scene roots: prefer the default scene, else every node that is nobody's child.
    std::vector<int> roots;
    if (loader.document.contains("scenes")) {
        const int sceneIndex = loader.document.value("scene", 0);
        if (sceneIndex >= 0 && sceneIndex < static_cast<int>(loader.document["scenes"].size())) {
            const json& scene = loader.document["scenes"][sceneIndex];
            if (scene.contains("nodes")) {
                for (const auto& node : scene["nodes"]) roots.push_back(node.get<int>());
            }
        }
    }
    if (roots.empty()) {
        const int nodeCount = loader.document.contains("nodes")
                                  ? static_cast<int>(loader.document["nodes"].size())
                                  : 0;
        std::vector<bool> isChild(static_cast<size_t>(nodeCount), false);
        for (int i = 0; i < nodeCount; ++i) {
            const json& node = loader.document["nodes"][i];
            if (node.contains("children")) {
                for (const auto& child : node["children"]) {
                    const int index = child.get<int>();
                    if (index >= 0 && index < nodeCount) isChild[static_cast<size_t>(index)] = true;
                }
            }
        }
        for (int i = 0; i < nodeCount; ++i) {
            if (!isChild[static_cast<size_t>(i)]) roots.push_back(i);
        }
        // Files without nodes still have meshes; load them at the origin.
        if (roots.empty() && loader.document.contains("meshes")) {
            for (int i = 0; i < static_cast<int>(loader.document["meshes"].size()); ++i) {
                if (!loader.AppendMesh(i, glm::mat4(1.0f))) {
                    error = loader.error;
                    return false;
                }
            }
        }
    }

    for (int root : roots) {
        if (root < 0 || root >= static_cast<int>(loader.document["nodes"].size())) continue;
        if (!loader.AppendNode(root, glm::mat4(1.0f), 0)) {
            error = loader.error;
            return false;
        }
    }

    if (loader.mesh.positions.empty()) {
        error = sourceName + ": the model contains no triangle geometry";
        return false;
    }

    // Counters live inside Loader (nodes/primitives/meshes/warnings): publish
    // them only now, after the node graph has actually been traversed.
    stats.meshes = loader.stats.meshes;
    stats.primitives = loader.stats.primitives;
    stats.nodes = loader.stats.nodes;
    stats.warnings = loader.stats.warnings;
    stats.vertices = loader.mesh.positions.size() / 3;
    stats.triangles = loader.mesh.indices.size() / 3;
    out = std::move(loader.mesh);
    return true;
}

} // namespace

bool LoadGltfMeshFromMemory(const unsigned char* data, size_t size, const std::string& sourceName,
                            MeshData& out, GltfLoadStats& stats, std::string& error) {
    out = MeshData{};
    stats = GltfLoadStats{};
    error.clear();

    Loader loader;
    std::vector<unsigned char> binChunk;
    if (!ParseDocument(data, size, sourceName, /*allowGlb=*/true, loader.document, binChunk, stats, error)) {
        return false;
    }
    loader.stats.usedBinaryContainer = stats.usedBinaryContainer;
    if (!loader.LoadBufferViews()) {
        error = loader.error;
        return false;
    }
    if (!loader.LoadBuffers(std::string())) {
        error = loader.error;
        return false;
    }
    if (!binChunk.empty()) {
        if (loader.buffers.empty()) loader.buffers.push_back(std::move(binChunk));
        else if (loader.buffers[0].empty()) loader.buffers[0] = std::move(binChunk);
    }
    return FinishLoad(loader, out, stats, error, sourceName);
}

bool LoadGltfMesh(const std::string& path, MeshData& out, GltfLoadStats& stats, std::string& error) {
    out = MeshData{};
    stats = GltfLoadStats{};
    error.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff length = file.tellg();
    file.seekg(0, std::ios::beg);
    if (length <= 0) {
        error = path + ": empty file";
        return false;
    }
    std::vector<unsigned char> data(static_cast<size_t>(length));
    if (!file.read(reinterpret_cast<char*>(data.data()), length)) {
        error = path + ": read error";
        return false;
    }
    file.close();

    Loader loader;
    std::vector<unsigned char> binChunk;
    if (!ParseDocument(data.data(), data.size(), path, /*allowGlb=*/true, loader.document, binChunk, stats,
                       error)) {
        return false;
    }
    loader.stats.usedBinaryContainer = stats.usedBinaryContainer;
    if (!loader.LoadBufferViews()) {
        error = loader.error;
        return false;
    }
    if (!loader.LoadBuffers(DirectoryOf(path))) {
        error = loader.error;
        return false;
    }
    if (!binChunk.empty()) {
        if (loader.buffers.empty()) loader.buffers.push_back(std::move(binChunk));
        else if (loader.buffers[0].empty()) loader.buffers[0] = std::move(binChunk);
    }
    return FinishLoad(loader, out, stats, error, path);
}

bool ComputeMeshBounds(const MeshData& mesh, float outMin[3], float outMax[3]) {
    if (mesh.positions.size() < 3) return false;
    for (int i = 0; i < 3; ++i) {
        outMin[i] = 1e30f;
        outMax[i] = -1e30f;
    }
    for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
        for (int c = 0; c < 3; ++c) {
            outMin[c] = std::min(outMin[c], mesh.positions[i + c]);
            outMax[c] = std::max(outMax[c], mesh.positions[i + c]);
        }
    }
    return true;
}

} // namespace pspengine
