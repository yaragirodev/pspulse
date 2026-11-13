#pragma once

// ---------------------------------------------------------------------------
//  PSP Engine - glTF 2.0 mesh loader (shared editor + PSP runtime)
//
//  Scope, deliberately kept small enough to run on a PSP:
//    * .gltf  (JSON, external or data: URI buffers)
//    * .glb   (binary container, embedded BIN chunk)
//    * POSITION / NORMAL / TEXCOORD_0 accessors (float32 + normalised ints)
//    * indexed + non-indexed primitives, node transforms baked into one mesh
//    * Draco / meshopt compression is *not* supported (export without it)
//
//  Everything is flattened into a single MeshData so a whole model becomes one
//  draw call - which is what the PSP GU pipeline wants.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

#include "MeshGen.h"

namespace pspengine {

struct GltfLoadStats {
    int meshes = 0;
    int primitives = 0;
    int nodes = 0;
    size_t vertices = 0;
    size_t triangles = 0;
    bool usedBinaryContainer = false;
    std::vector<std::string> warnings;
};

// Loads `path` and appends the flattened geometry to `out` (out is cleared
// first). Returns false and fills `error` when the file cannot be parsed.
bool LoadGltfMesh(const std::string& path, MeshData& out, GltfLoadStats& stats, std::string& error);

// Same, but from an in-memory buffer (used when assets are embedded in the EBOOT).
bool LoadGltfMeshFromMemory(const unsigned char* data, size_t size, const std::string& sourceName,
                            MeshData& out, GltfLoadStats& stats, std::string& error);

// Computes the axis-aligned bounding box of a mesh; returns false when empty.
bool ComputeMeshBounds(const MeshData& mesh, float outMin[3], float outMax[3]);

} // namespace pspengine
