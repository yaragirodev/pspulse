#pragma once

// ---------------------------------------------------------------------------
//  PSP runtime glue: turns the shared glTF loader's MeshData into the
//  dependency-free GltfMeshData consumed by SceneRuntime's GU renderer.
//
//  The shared loader (Shared/Mesh/GltfLoader.cpp) is the exact same code the
//  editor uses, so models look identical in both places.
// ---------------------------------------------------------------------------

#include <string>

#include "SceneRuntime.h"

namespace pspulse {
struct MeshData;
}

pspruntime::GltfMeshData* LoadGltfIntoRuntimeMesh();

// Parses an embedded .gltf/.glb blob and fills `out`.
bool LoadGltfMeshToGltfData(const unsigned char* data, unsigned int size, const char* sourceName,
                            pspruntime::GltfMeshData& out, std::string& error);
