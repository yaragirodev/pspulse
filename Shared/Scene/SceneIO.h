#pragma once

// ---------------------------------------------------------------------------
//  PSP Engine - JSON scene & project serialisation
// ---------------------------------------------------------------------------

#include <string>

#include "SceneData.h"

namespace pspengine {

// Serialise the scene into pretty JSON text.
std::string SceneToJson(const Scene& scene);

// Parse a scene from JSON text. Returns false and fills error on failure.
bool SceneFromJson(const std::string& text, Scene& outScene, std::string& error);

// File helpers.
bool SaveSceneToFile(const Scene& scene, const std::string& path, std::string& error);
bool LoadSceneFromFile(const std::string& path, Scene& outScene, std::string& error);

// Creates the default "empty" scene used by File -> New Scene:
// a ground plane, one cube, one sphere, a sun and a camera.
Scene MakeDefaultScene();

} // namespace pspengine
