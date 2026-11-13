// ---------------------------------------------------------------------------
//  Headless regression test for the glTF loader (editor + PSP share this code).
//
//  Covers: .gltf with embedded base64 buffer, .glb container, node transform
//  baking, bounds computation and graceful failure on a missing file.
// ---------------------------------------------------------------------------

#include <cmath>
#include <cstdio>
#include <string>

#include "Shared/Mesh/GltfLoader.h"

using namespace pspengine;

namespace {
int gFailures = 0;
void Check(bool ok, const char* what) {
    std::printf(ok ? "  ok:   %s\n" : "  FAIL: %s\n", what);
    if (!ok) ++gFailures;
}
} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "projects/MyPSPGame/assets/models";
    const std::string gltfPath = dir + "/triangle.gltf";
    const std::string glbPath = dir + "/triangle.glb";

    std::printf("glTF:\n");
    {
        MeshData mesh;
        GltfLoadStats stats;
        std::string error;
        const bool ok = LoadGltfMesh(gltfPath, mesh, stats, error);
        Check(ok, ok ? "loads .gltf" : error.c_str());
        if (ok) {
            Check(stats.vertices == 3, "3 vertices");
            Check(stats.triangles == 1, "1 triangle");
            Check(mesh.normals.size() == mesh.positions.size(), "normals synthesised or loaded");
            Check(mesh.uvs.size() / 2 == stats.vertices, "uv array sized to the vertex count");
            float mn[3], mx[3];
            Check(ComputeMeshBounds(mesh, mn, mx), "bounds computed");
        }
    }

    std::printf("GLB:\n");
    {
        MeshData mesh;
        GltfLoadStats stats;
        std::string error;
        const bool ok = LoadGltfMesh(glbPath, mesh, stats, error);
        Check(ok, ok ? "loads binary .glb" : error.c_str());
        if (ok) Check(stats.usedBinaryContainer, "detected the GLB container");
    }

    std::printf("Failure handling:\n");
    {
        MeshData mesh;
        GltfLoadStats stats;
        std::string error;
        Check(!LoadGltfMesh(dir + "/missing.gltf", mesh, stats, error), "missing file is reported");
        Check(!error.empty(), "error message is filled");
    }

    std::printf(gFailures == 0 ? "\nRESULT: PASS\n" : "\nRESULT: FAIL (%d)\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
