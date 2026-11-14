// ---------------------------------------------------------------------------
//  PSPulse - procedural mesh generation
// ---------------------------------------------------------------------------

#include "MeshGen.h"

#include <cmath>

namespace pspulse {

namespace {

constexpr float kPi = 3.14159265358979323846f;

void PushVertex(MeshData& mesh, float px, float py, float pz, float nx, float ny, float nz, float u,
                float v) {
    mesh.positions.push_back(px);
    mesh.positions.push_back(py);
    mesh.positions.push_back(pz);
    mesh.normals.push_back(nx);
    mesh.normals.push_back(ny);
    mesh.normals.push_back(nz);
    mesh.uvs.push_back(u);
    mesh.uvs.push_back(v);
}

void BuildCube(MeshData& mesh) {
    struct Face {
        float normal[3];
        float corners[4][3];
    };
    const Face faces[6] = {
        // +X
        {{1, 0, 0}, {{0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}}},
        // -X
        {{-1, 0, 0}, {{-0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, -0.5f}}},
        // +Y
        {{0, 1, 0}, {{-0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}}},
        // -Y
        {{0, -1, 0}, {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, 0.5f}}},
        // +Z
        {{0, 0, 1}, {{-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}}},
        // -Z
        {{0, 0, -1}, {{0.5f, -0.5f, -0.5f}, {-0.5f, -0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}}},
    };
    const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (const auto& face : faces) {
        uint32_t base = static_cast<uint32_t>(mesh.VertexCount());
        for (int i = 0; i < 4; ++i) {
            PushVertex(mesh, face.corners[i][0], face.corners[i][1], face.corners[i][2], face.normal[0],
                       face.normal[1], face.normal[2], uv[i][0], uv[i][1]);
        }
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
}

void BuildSphere(MeshData& mesh, int segments) {
    const int stacks = segments;
    const int slices = segments * 2;
    for (int i = 0; i <= stacks; ++i) {
        float v = static_cast<float>(i) / stacks;
        float phi = v * kPi;
        for (int j = 0; j <= slices; ++j) {
            float u = static_cast<float>(j) / slices;
            float theta = u * 2.0f * kPi;
            float x = std::sin(phi) * std::cos(theta) * 0.5f;
            float y = std::cos(phi) * 0.5f;
            float z = std::sin(phi) * std::sin(theta) * 0.5f;
            float nx = x * 2.0f, ny = y * 2.0f, nz = z * 2.0f;
            PushVertex(mesh, x, y, z, nx, ny, nz, u, 1.0f - v);
        }
    }
    const int row = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            uint32_t a = static_cast<uint32_t>(i * row + j);
            uint32_t b = a + 1;
            uint32_t c = a + static_cast<uint32_t>(row);
            uint32_t d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
        }
    }
}

void BuildCone(MeshData& mesh, int segments) {
    const int slices = segments * 2;
    // Side.
    for (int j = 0; j < slices; ++j) {
        float u0 = static_cast<float>(j) / slices;
        float u1 = static_cast<float>(j + 1) / slices;
        float a0 = u0 * 2.0f * kPi;
        float a1 = u1 * 2.0f * kPi;
        float x0 = std::cos(a0) * 0.5f, z0 = std::sin(a0) * 0.5f;
        float x1 = std::cos(a1) * 0.5f, z1 = std::sin(a1) * 0.5f;
        // Normals of a cone side.
        auto sideNormal = [](float x, float z) {
            float len = std::sqrt(x * x + z * z);
            if (len < 1e-5f) return std::pair<float, float>{0.0f, 0.0f};
            return std::pair<float, float>{x / len * 0.894f, z / len * 0.894f};
        };
        auto n0 = sideNormal(x0, z0);
        auto n1 = sideNormal(x1, z1);
        uint32_t base = static_cast<uint32_t>(mesh.VertexCount());
        PushVertex(mesh, x0, -0.5f, z0, n0.first, 0.447f, n0.second, u0, 0.0f);
        PushVertex(mesh, x1, -0.5f, z1, n1.first, 0.447f, n1.second, u1, 0.0f);
        PushVertex(mesh, 0.0f, 0.5f, 0.0f, (n0.first + n1.first) * 0.5f, 0.447f,
                   (n0.second + n1.second) * 0.5f, (u0 + u1) * 0.5f, 1.0f);
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2});
        // Bottom cap.
        uint32_t capBase = static_cast<uint32_t>(mesh.VertexCount());
        PushVertex(mesh, 0.0f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f);
        PushVertex(mesh, x1, -0.5f, z1, 0.0f, -1.0f, 0.0f, u1, 0.0f);
        PushVertex(mesh, x0, -0.5f, z0, 0.0f, -1.0f, 0.0f, u0, 0.0f);
        mesh.indices.insert(mesh.indices.end(), {capBase, capBase + 1, capBase + 2});
    }
}

void BuildCylinder(MeshData& mesh, int segments) {
    const int slices = segments * 2;
    for (int j = 0; j < slices; ++j) {
        float u0 = static_cast<float>(j) / slices;
        float u1 = static_cast<float>(j + 1) / slices;
        float a0 = u0 * 2.0f * kPi;
        float a1 = u1 * 2.0f * kPi;
        float x0 = std::cos(a0) * 0.5f, z0 = std::sin(a0) * 0.5f;
        float x1 = std::cos(a1) * 0.5f, z1 = std::sin(a1) * 0.5f;
        uint32_t base = static_cast<uint32_t>(mesh.VertexCount());
        PushVertex(mesh, x0, -0.5f, z0, std::cos(a0), 0.0f, std::sin(a0), u0, 0.0f);
        PushVertex(mesh, x1, -0.5f, z1, std::cos(a1), 0.0f, std::sin(a1), u1, 0.0f);
        PushVertex(mesh, x1, 0.5f, z1, std::cos(a1), 0.0f, std::sin(a1), u1, 1.0f);
        PushVertex(mesh, x0, 0.5f, z0, std::cos(a0), 0.0f, std::sin(a0), u0, 1.0f);
        mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});

        uint32_t top = static_cast<uint32_t>(mesh.VertexCount());
        PushVertex(mesh, 0.0f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f);
        PushVertex(mesh, x0, 0.5f, z0, 0.0f, 1.0f, 0.0f, u0, 1.0f);
        PushVertex(mesh, x1, 0.5f, z1, 0.0f, 1.0f, 0.0f, u1, 1.0f);
        mesh.indices.insert(mesh.indices.end(), {top, top + 1, top + 2});

        uint32_t bottom = static_cast<uint32_t>(mesh.VertexCount());
        PushVertex(mesh, 0.0f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f);
        PushVertex(mesh, x1, -0.5f, z1, 0.0f, -1.0f, 0.0f, u1, 0.0f);
        PushVertex(mesh, x0, -0.5f, z0, 0.0f, -1.0f, 0.0f, u0, 0.0f);
        mesh.indices.insert(mesh.indices.end(), {bottom, bottom + 1, bottom + 2});
    }
}

void BuildPlane(MeshData& mesh) {
    const float s = 0.5f;
    uint32_t base = 0;
    PushVertex(mesh, -s, 0.0f, s, 0, 1, 0, 0, 1);
    PushVertex(mesh, s, 0.0f, s, 0, 1, 0, 1, 1);
    PushVertex(mesh, s, 0.0f, -s, 0, 1, 0, 1, 0);
    PushVertex(mesh, -s, 0.0f, -s, 0, 1, 0, 0, 0);
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

} // namespace

MeshData GeneratePrimitive(PrimitiveType type, int segments) {
    MeshData mesh;
    switch (type) {
        case PrimitiveType::Cube:
            BuildCube(mesh);
            break;
        case PrimitiveType::Sphere:
            BuildSphere(mesh, segments);
            break;
        case PrimitiveType::Cone:
            BuildCone(mesh, segments);
            break;
        case PrimitiveType::Cylinder:
            BuildCylinder(mesh, segments);
            break;
        case PrimitiveType::Plane:
            BuildPlane(mesh);
            break;
        default:
            // Empty objects render as a small wire gizmo in the editor only.
            BuildCube(mesh);
            break;
    }
    return mesh;
}

void GenerateGrid(float halfExtent, float cellSize, std::vector<float>& outPositions) {
    outPositions.clear();
    int steps = static_cast<int>(halfExtent / cellSize);
    for (int i = -steps; i <= steps; ++i) {
        float p = static_cast<float>(i) * cellSize;
        // Lines along Z.
        outPositions.insert(outPositions.end(), {-halfExtent, 0.0f, p, halfExtent, 0.0f, p});
        // Lines along X.
        outPositions.insert(outPositions.end(), {p, 0.0f, -halfExtent, p, 0.0f, halfExtent});
    }
}

} // namespace pspulse
