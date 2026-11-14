#pragma once

// ---------------------------------------------------------------------------
//  PSPulse Editor - OpenGL 3.3 scene renderer
//
//  * renders the scene into an offscreen framebuffer that is displayed inside
//    the ImGui docking viewport,
//  * object picking via an ID-buffer pass (glReadPixels on a second FBO),
//  * grid, collision/light/camera debug visualisation.
// ---------------------------------------------------------------------------

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "Shared/Mesh/MeshGen.h"
#include "Shared/Scene/SceneData.h"

namespace pspeditor {

struct CameraView {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 position{0.0f};
};

struct RenderSettings {
    // Project root of the open project; glTF model paths are resolved against it.
    std::string projectRoot;
    bool showGrid = true;
    bool showColliders = true;
    bool showHelpers = true;
    bool showShadows = true;
    int selectedIndex = -1;
    int hoveredIndex = -1;
};

// --------------------------------------------------------------------------
class ViewportFramebuffer {
public:
    void Resize(int width, int height);
    void Bind() const;
    void Unbind() const;
    void Destroy();
    bool Valid() const { return fbo_ != 0; }
    unsigned int ColorTexture() const { return colorTexture_; }
    int Width() const { return width_; }
    int Height() const { return height_; }

private:
    unsigned int fbo_ = 0;
    unsigned int colorTexture_ = 0;
    unsigned int depthBuffer_ = 0;
    int width_ = 0;
    int height_ = 0;
};

// --------------------------------------------------------------------------
class MeshGPU {
public:
    bool Upload(const pspulse::MeshData& data);
    void Draw() const;
    void Destroy();
    bool Valid() const { return vao_ != 0; }
    unsigned int IndexCount() const { return indexCount_; }

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    unsigned int indexCount_ = 0;
};

// --------------------------------------------------------------------------
class ShaderProgram {
public:
    bool Compile(const char* vertexSource, const char* fragmentSource, std::string& error);
    void Use() const;
    void Destroy();
    int Uniform(const char* name) const;
    void SetMat4(const char* name, const glm::mat4& value) const;
    void SetMat3(const char* name, const glm::mat3& value) const;
    void SetVec3(const char* name, const glm::vec3& value) const;
    void SetVec4(const char* name, const glm::vec4& value) const;
    void SetFloat(const char* name, float value) const;
    void SetInt(const char* name, int value) const;

private:
    unsigned int program_ = 0;
    mutable std::unordered_map<std::string, int> cache_;
};

// --------------------------------------------------------------------------
class LineBatch {
public:
    void Begin();
    void AddLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color);
    void AddBox(const glm::mat4& transform, const glm::vec3& center, const glm::vec3& halfExtents,
                const glm::vec3& color);
    void AddWireSphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments = 24);
    void AddCircle(const glm::vec3& center, float radius, const glm::vec3& color, int segments = 32);
    void End(ShaderProgram& shader);
    void Draw(ShaderProgram& shader) const;
    void Destroy();

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    std::vector<float> vertices_; // pos(3) color(3)
};

// --------------------------------------------------------------------------
class SceneRenderer {
public:
    bool Init(std::string& error);
    void Shutdown();

    // Renders the scene into `target` (resized by caller).
    void RenderScene(ViewportFramebuffer& target, const pspulse::Scene& scene,
                     const CameraView& camera, const RenderSettings& settings);

    // Picks the object under the mouse. Coordinates are in viewport pixels
    // (origin bottom-left). Returns object index or -1.
    int PickObject(const pspulse::Scene& scene, const CameraView& camera, int pixelX, int pixelY,
                   int width, int height, const std::string& projectRoot = std::string());

    ViewportFramebuffer& Framebuffer() { return viewportFbo_; }
    unsigned int ColorTexture() const { return viewportFbo_.ColorTexture(); }
    int Width() const { return viewportFbo_.Width(); }
    int Height() const { return viewportFbo_.Height(); }

private:
    void EnsureMeshes();
    // Returns the GPU mesh for a glTF model path, loading it on first use.
    // nullptr when the file cannot be loaded (the error is cached).
    const MeshGPU* ModelMesh(const std::string& projectRelativePath, const std::string& projectRoot,
                             std::string* error);
    void DrawObject(const pspulse::Scene& scene, const pspulse::GameObject& object, int index,
                    const ShaderProgram& shader, bool idPass, bool isSelected,
                    const std::string& projectRoot);

    ShaderProgram meshShader_;
    ShaderProgram lineShader_;
    ShaderProgram idShader_;
    ShaderProgram shadowShader_;

    std::unordered_map<int, MeshGPU> meshes_; // key: PrimitiveType int
    // glTF models, keyed by project-relative path.
    std::unordered_map<std::string, MeshGPU> models_;
    std::unordered_map<std::string, std::string> modelErrors_;
    ViewportFramebuffer viewportFbo_;
    ViewportFramebuffer pickingFbo_;
    unsigned int shadowTexture_ = 0;
    unsigned int fullscreenQuad_ = 0;
    LineBatch debugLines_;
    LineBatch selectedLines_;
    unsigned int gridVao_ = 0;
    unsigned int gridVbo_ = 0;
    int gridVertexCount_ = 0;
    bool initialized_ = false;
};

} // namespace pspeditor
