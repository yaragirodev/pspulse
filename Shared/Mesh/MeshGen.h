#pragma once

// ---------------------------------------------------------------------------
//  PSP Engine - procedural mesh generation (editor viewport + glTF-free assets)
// ---------------------------------------------------------------------------

#include <cstdint>
#include <vector>

#include "../Scene/SceneData.h"

namespace pspengine {

struct MeshData {
    std::vector<float> positions; // xyz
    std::vector<float> normals;   // xyz
    std::vector<float> uvs;       // uv
    std::vector<uint32_t> indices;

    size_t VertexCount() const { return positions.size() / 3; }
};

// Generates a unit primitive (1x1x1 cube, diameter 1 sphere, height 1 cone...).
MeshData GeneratePrimitive(PrimitiveType type, int segments = 32);

// Simple flat grid lines for the viewport ground grid.
void GenerateGrid(float halfExtent, float cellSize, std::vector<float>& outPositions);

} // namespace pspengine
