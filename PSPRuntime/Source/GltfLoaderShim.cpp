// ---------------------------------------------------------------------------
//  PSP runtime glue implementation
// ---------------------------------------------------------------------------

#include "GltfLoaderShim.h"

#include <vector>

#include "../Shared/Mesh/GltfLoader.h"

bool LoadGltfMeshToGltfData(const unsigned char* data, unsigned int size, const char* sourceName,
                            pspruntime::GltfMeshData& out, std::string& error) {
    out.positions.clear();
    out.normals.clear();
    out.indices.clear();

    pspengine::MeshData mesh;
    pspengine::GltfLoadStats stats;
    if (!pspengine::LoadGltfMeshFromMemory(data, size, sourceName ? sourceName : "model", mesh, stats,
                                          error)) {
        return false;
    }

    out.positions.resize(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) out.positions[i] = mesh.positions[i];
    out.normals.resize(mesh.normals.size());
    for (size_t i = 0; i < mesh.normals.size(); ++i) out.normals[i] = mesh.normals[i];
    out.indices.resize(mesh.indices.size());
    for (size_t i = 0; i < mesh.indices.size(); ++i) out.indices[i] = mesh.indices[i];
    return true;
}
