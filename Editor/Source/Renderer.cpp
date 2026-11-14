// ---------------------------------------------------------------------------
//  PSPulse Editor - OpenGL 3.3 scene renderer implementation
// ---------------------------------------------------------------------------

#include "Renderer.h"

#include "Shared/Mesh/GltfLoader.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include <glad/gl.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace pspeditor {

using pspulse::GameObject;
using pspulse::PrimitiveType;
using pspulse::Scene;
using pspulse::CollisionShapeType;

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

namespace {

unsigned int CompileStage(unsigned int type, const char* source, std::string& error) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        error += log;
        error += "\n";
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

glm::vec3 ColorFromObject(const GameObject& object) {
    return glm::vec3(object.color.r, object.color.g, object.color.b);
}

float HashFloat(int id, float scale) {
    unsigned int h = static_cast<unsigned int>(id * 2654435761u);
    return static_cast<float>(h % 1000) / 1000.0f * scale;
}

// BlOB (blobby) light attenuation used in the editor preview to roughly match
// the PSP GU CLAMPED lighting model.
glm::vec3 EvaluateLighting(const GameObject& object, const glm::mat3& normalMatrix,
                           const glm::vec3& worldPos, const std::vector<glm::vec3>& lightPositions,
                           const std::vector<glm::vec3>& lightColors, const glm::vec3& ambient) {
    glm::vec3 base = ColorFromObject(object);
    // A tiny per-object normal tint keeps un-lit sides readable in the editor.
    glm::vec3 lit = ambient * base;
    // Fake directional shade based on normal direction for stable readability.
    glm::vec3 n = glm::normalize(normalMatrix * glm::vec3(0.0f, 1.0f, 0.0f));
    lit *= 0.75f + 0.25f * glm::clamp(n.y, 0.0f, 1.0f);
    for (size_t i = 0; i < lightPositions.size(); ++i) {
        glm::vec3 toLight = lightPositions[i] - worldPos;
        float dist = glm::length(toLight);
        if (dist < 1e-4f) dist = 1e-4f;
        glm::vec3 l = toLight / dist;
        float atten = glm::clamp(1.0f - dist / 24.0f, 0.0f, 1.0f);
        atten *= atten;
        lit += base * lightColors[i] * atten * 0.55f;
    }
    return glm::clamp(lit, glm::vec3(0.03f), glm::vec3(1.0f));
}

} // namespace

// --------------------------------------------------------------------------
// ViewportFramebuffer
// --------------------------------------------------------------------------

void ViewportFramebuffer::Resize(int width, int height) {
    width = glm::max(width, 2);
    height = glm::max(height, 2);
    if (width == width_ && height == height_ && fbo_ != 0) return;

    Destroy();
    width_ = width;
    height_ = height;
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &colorTexture_);
    glBindTexture(GL_TEXTURE_2D, colorTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture_, 0);

    glGenRenderbuffers(1, &depthBuffer_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width_, height_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthBuffer_);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ViewportFramebuffer::Bind() const { glBindFramebuffer(GL_FRAMEBUFFER, fbo_); }
void ViewportFramebuffer::Unbind() const { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void ViewportFramebuffer::Destroy() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (colorTexture_) glDeleteTextures(1, &colorTexture_);
    if (depthBuffer_) glDeleteRenderbuffers(1, &depthBuffer_);
    fbo_ = 0;
    colorTexture_ = 0;
    depthBuffer_ = 0;
    width_ = 0;
    height_ = 0;
}

// --------------------------------------------------------------------------
// MeshGPU
// --------------------------------------------------------------------------

bool MeshGPU::Upload(const pspulse::MeshData& data) {
    Destroy();
    if (data.positions.empty() || data.indices.empty()) return false;
    indexCount_ = static_cast<unsigned int>(data.indices.size());

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    glBindVertexArray(vao_);

    std::vector<float> interleaved;
    interleaved.reserve(data.VertexCount() * 8);
    for (size_t i = 0; i < data.VertexCount(); ++i) {
        interleaved.insert(interleaved.end(), {data.positions[i * 3 + 0], data.positions[i * 3 + 1],
                                               data.positions[i * 3 + 2], data.normals[i * 3 + 0],
                                               data.normals[i * 3 + 1], data.normals[i * 3 + 2],
                                               data.uvs[i * 2 + 0], data.uvs[i * 2 + 1]});
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                 interleaved.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.indices.size() * sizeof(uint32_t)),
                 data.indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(6 * sizeof(float)));
    glBindVertexArray(0);
    return true;
}

void MeshGPU::Draw() const {
    if (!vao_) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void MeshGPU::Destroy() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
    vao_ = vbo_ = ebo_ = 0;
    indexCount_ = 0;
}

// --------------------------------------------------------------------------
// ShaderProgram
// --------------------------------------------------------------------------

bool ShaderProgram::Compile(const char* vertexSource, const char* fragmentSource, std::string& error) {
    Destroy();
    unsigned int vs = CompileStage(GL_VERTEX_SHADER, vertexSource, error);
    if (!vs) return false;
    unsigned int fs = CompileStage(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (!fs) {
        glDeleteShader(vs);
        return false;
    }
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
        error += log;
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    cache_.clear();
    return true;
}

void ShaderProgram::Use() const { glUseProgram(program_); }

void ShaderProgram::Destroy() {
    if (program_) glDeleteProgram(program_);
    program_ = 0;
    cache_.clear();
}

int ShaderProgram::Uniform(const char* name) const {
    auto it = cache_.find(name);
    if (it != cache_.end()) return it->second;
    int location = glGetUniformLocation(program_, name);
    cache_[name] = location;
    return location;
}

void ShaderProgram::SetMat4(const char* name, const glm::mat4& value) const {
    glUniformMatrix4fv(Uniform(name), 1, GL_FALSE, glm::value_ptr(value));
}
void ShaderProgram::SetMat3(const char* name, const glm::mat3& value) const {
    glUniformMatrix3fv(Uniform(name), 1, GL_FALSE, glm::value_ptr(value));
}
void ShaderProgram::SetVec3(const char* name, const glm::vec3& value) const {
    glUniform3fv(Uniform(name), 1, glm::value_ptr(value));
}
void ShaderProgram::SetVec4(const char* name, const glm::vec4& value) const {
    glUniform4fv(Uniform(name), 1, glm::value_ptr(value));
}
void ShaderProgram::SetFloat(const char* name, float value) const { glUniform1f(Uniform(name), value); }
void ShaderProgram::SetInt(const char* name, int value) const { glUniform1i(Uniform(name), value); }

// --------------------------------------------------------------------------
// LineBatch
// --------------------------------------------------------------------------

void LineBatch::Begin() { vertices_.clear(); }

void LineBatch::AddLine(const glm::vec3& a, const glm::vec3& b, const glm::vec3& color) {
    vertices_.insert(vertices_.end(), {a.x, a.y, a.z, color.r, color.g, color.b});
    vertices_.insert(vertices_.end(), {b.x, b.y, b.z, color.r, color.g, color.b});
}

void LineBatch::AddBox(const glm::mat4& transform, const glm::vec3& center, const glm::vec3& half,
                       const glm::vec3& color) {
    glm::vec3 corners[8];
    int i = 0;
    for (int x = -1; x <= 1; x += 2)
        for (int y = -1; y <= 1; y += 2)
            for (int z = -1; z <= 1; z += 2) {
                glm::vec3 local(center.x + static_cast<float>(x) * half.x,
                                center.y + static_cast<float>(y) * half.y,
                                center.z + static_cast<float>(z) * half.z);
                corners[i++] = glm::vec3(transform * glm::vec4(local, 1.0f));
            }
    // Bit index: x*4 + y*2 + z
    const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                              {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto& edge : edges) AddLine(corners[edge[0]], corners[edge[1]], color);
}

void LineBatch::AddWireSphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments) {
    for (int axis = 0; axis < 3; ++axis) {
        for (int s = 0; s < segments; ++s) {
            float a0 = (static_cast<float>(s) / segments) * 6.2831853f;
            float a1 = (static_cast<float>(s + 1) / segments) * 6.2831853f;
            glm::vec3 p0(0.0f), p1(0.0f);
            if (axis == 0) {
                p0 = {0.0f, std::cos(a0), std::sin(a0)};
                p1 = {0.0f, std::cos(a1), std::sin(a1)};
            } else if (axis == 1) {
                p0 = {std::cos(a0), 0.0f, std::sin(a0)};
                p1 = {std::cos(a1), 0.0f, std::sin(a1)};
            } else {
                p0 = {std::cos(a0), std::sin(a0), 0.0f};
                p1 = {std::cos(a1), std::sin(a1), 0.0f};
            }
            AddLine(center + p0 * radius, center + p1 * radius, color);
        }
    }
}

void LineBatch::AddCircle(const glm::vec3& center, float radius, const glm::vec3& color, int segments) {
    for (int s = 0; s < segments; ++s) {
        float a0 = (static_cast<float>(s) / segments) * 6.2831853f;
        float a1 = (static_cast<float>(s + 1) / segments) * 6.2831853f;
        glm::vec3 p0(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius);
        glm::vec3 p1(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius);
        AddLine(center + p0, center + p1, color);
    }
}

void LineBatch::End(ShaderProgram& shader) {
    if (!vao_) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
    }
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(float)),
                 vertices_.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);
    (void)shader;
}

void LineBatch::Draw(ShaderProgram& shader) const {
    if (!vao_ || vertices_.empty()) return;
    shader.Use();
    glBindVertexArray(vao_);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices_.size() / 6));
    glBindVertexArray(0);
}

void LineBatch::Destroy() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    vao_ = vbo_ = 0;
    vertices_.clear();
}

// --------------------------------------------------------------------------
// SceneRenderer
// --------------------------------------------------------------------------

namespace {

const char* kMeshVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uViewProjection;
uniform mat3 uNormalMatrix;

out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUV;

void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPos = world.xyz;
    vNormal = normalize(uNormalMatrix * aNormal);
    vUV = aUV;
    gl_Position = uViewProjection * world;
}
)";

const char* kMeshFragmentShader = R"(#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUV;

uniform vec3 uBaseColor;
uniform vec3 uAmbient;
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform float uSelected;
uniform float uGridFade;

out vec4 fragColor;

void main() {
    vec3 n = normalize(vNormal);
    vec3 viewDir = normalize(vec3(0.0, 6.0, 12.0) - vWorldPos);
    vec3 l = normalize(uLightPos - vWorldPos);
    float ndl = max(dot(n, l), 0.0);
    float dist = length(uLightPos - vWorldPos);
    float atten = clamp(1.0 - dist / 32.0, 0.0, 1.0);
    atten *= atten;

    vec3 ambient = uAmbient * uBaseColor;
    vec3 diffuse = uBaseColor * uLightColor * ndl * atten;

    // A subtle cool rim keeps silhouettes readable against the sky gradient.
    vec3 rim = uBaseColor * pow(1.0 - max(dot(n, viewDir), 0.0), 2.0) * 0.25;

    // Procedural micro-checker to give flat primitives a sense of scale.
    float checker = mod(floor(vUV.x * 8.0) + floor(vUV.y * 8.0), 2.0);
    vec3 albedo = uBaseColor * (0.94 + 0.06 * checker);

    vec3 color = ambient * 0.9 + diffuse * albedo + rim;
    color = mix(color, uBaseColor * 1.2 + vec3(0.12, 0.16, 0.22), uSelected * 0.35);
    fragColor = vec4(color, 1.0);
}
)";

const char* kLineVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;
uniform mat4 uViewProjection;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
)";

const char* kLineFragmentShader = R"(#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); }
)";

const char* kIdVertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPosition;
uniform mat4 uModel;
uniform mat4 uViewProjection;
void main() { gl_Position = uViewProjection * uModel * vec4(aPosition, 1.0); }
)";

const char* kIdFragmentShader = R"(#version 330 core
uniform vec3 uIdColor;
out vec4 fragColor;
void main() { fragColor = vec4(uIdColor, 1.0); }
)";

} // namespace

bool SceneRenderer::Init(std::string& error) {
    if (!meshShader_.Compile(kMeshVertexShader, kMeshFragmentShader, error)) return false;
    if (!lineShader_.Compile(kLineVertexShader, kLineFragmentShader, error)) return false;
    if (!idShader_.Compile(kIdVertexShader, kIdFragmentShader, error)) return false;

    // Coloured grid: built once into a vertex buffer (pos + colour).
    {
        std::vector<float> gridPositions;
        pspulse::GenerateGrid(20.0f, 1.0f, gridPositions);
        gridVertexCount_ = static_cast<int>(gridPositions.size() / 3);

        std::vector<float> interleaved;
        interleaved.reserve(gridPositions.size() * 2);
        for (int i = 0; i < gridVertexCount_; ++i) {
            float x = gridPositions[i * 3 + 0];
            float y = gridPositions[i * 3 + 1];
            float z = gridPositions[i * 3 + 2];
            bool isAxisX = std::fabs(z) < 0.001f && std::fabs(x) > 0.001f;
            bool isAxisZ = std::fabs(x) < 0.001f && std::fabs(z) > 0.001f;
            bool major = (std::fabs(std::fmod(std::fabs(x) < std::fabs(z) ? z : x, 5.0f)) < 0.001f);
            glm::vec3 color(0.21f, 0.23f, 0.27f);
            if (major) color = glm::vec3(0.29f, 0.31f, 0.36f);
            if (isAxisX) color = glm::vec3(0.52f, 0.18f, 0.20f);
            if (isAxisZ) color = glm::vec3(0.16f, 0.30f, 0.60f);
            interleaved.insert(interleaved.end(), {x, y, z, color.r, color.g, color.b});
        }

        glGenVertexArrays(1, &gridVao_);
        glGenBuffers(1, &gridVbo_);
        glBindVertexArray(gridVao_);
        glBindBuffer(GL_ARRAY_BUFFER, gridVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                     interleaved.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              reinterpret_cast<void*>(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    initialized_ = true;
    return true;
}

void SceneRenderer::Shutdown() {
    meshShader_.Destroy();
    lineShader_.Destroy();
    idShader_.Destroy();
    for (auto& kv : meshes_) kv.second.Destroy();
    meshes_.clear();
    for (auto& kv : models_) kv.second.Destroy();
    models_.clear();
    modelErrors_.clear();
    viewportFbo_.Destroy();
    pickingFbo_.Destroy();
    debugLines_.Destroy();
    selectedLines_.Destroy();
    if (gridVao_) glDeleteVertexArrays(1, &gridVao_);
    if (gridVbo_) glDeleteBuffers(1, &gridVbo_);
    if (shadowTexture_) glDeleteTextures(1, &shadowTexture_);
    gridVao_ = gridVbo_ = 0;
    shadowTexture_ = 0;
    initialized_ = false;
}

const MeshGPU* SceneRenderer::ModelMesh(const std::string& projectRelativePath,
                                       const std::string& projectRoot, std::string* error) {
    if (projectRelativePath.empty()) return nullptr;
    auto cached = models_.find(projectRelativePath);
    if (cached != models_.end()) {
        if (cached->second.Valid()) return &cached->second;
        if (error) {
            const auto reason = modelErrors_.find(projectRelativePath);
            *error = reason != modelErrors_.end() ? reason->second : "model failed to load";
        }
        return nullptr;
    }

    std::string fullPath = projectRelativePath;
    const bool isAbsolute = !fullPath.empty() && (fullPath[0] == '/' || fullPath[0] == '\\');
    if (!isAbsolute && !projectRoot.empty()) fullPath = projectRoot + "/" + projectRelativePath;

    pspulse::MeshData data;
    pspulse::GltfLoadStats stats;
    std::string loadError;
    if (!pspulse::LoadGltfMesh(fullPath, data, stats, loadError)) {
        modelErrors_[projectRelativePath] = loadError;
        if (error) *error = loadError;
        models_.emplace(projectRelativePath, MeshGPU{}); // remember the failure
        return nullptr;
    }

    MeshGPU& gpu = models_[projectRelativePath];
    if (!gpu.Upload(data)) {
        modelErrors_[projectRelativePath] = "GPU upload failed";
        if (error) *error = "GPU upload failed";
        return nullptr;
    }
    modelErrors_.erase(projectRelativePath);
    return &gpu;
}

void SceneRenderer::EnsureMeshes() {
    for (int i = 0; i <= static_cast<int>(PrimitiveType::Plane); ++i) {
        if (meshes_.count(i)) continue;
        PrimitiveType type = static_cast<PrimitiveType>(i);
        pspulse::MeshData data = pspulse::GeneratePrimitive(type, 24);
        meshes_[i].Upload(data);
    }
}

void SceneRenderer::DrawObject(const Scene& scene, const GameObject& object, int index,
                               const ShaderProgram& shader, bool idPass, bool isSelected,
                               const std::string& projectRoot) {
    // glTF models win over the primitive; a failed load falls back to nothing so
    // the viewport never dies because of one broken asset.
    const MeshGPU* model = nullptr;
    if (!object.meshPath.empty()) {
        model = ModelMesh(object.meshPath, projectRoot, nullptr);
        if (!model) return;
    }
    const pspulse::MeshData* unused = nullptr;
    (void)unused;
    MeshGPU* primitiveMesh = nullptr;
    if (!model) {
        if (object.primitive == PrimitiveType::Empty) return;
        auto it = meshes_.find(static_cast<int>(object.primitive));
        if (it == meshes_.end() || !it->second.Valid()) return;
        primitiveMesh = &it->second;
    }
    const MeshGPU& mesh = model ? *model : *primitiveMesh;

    glm::mat4 world = scene.WorldMatrix(object);
    shader.SetMat4("uModel", world);
    if (idPass) {
        int id = index + 1;
        glm::vec3 idColor(static_cast<float>(id & 0xFF) / 255.0f,
                          static_cast<float>((id >> 8) & 0xFF) / 255.0f,
                          static_cast<float>((id >> 16) & 0xFF) / 255.0f);
        shader.SetVec3("uIdColor", idColor);
    } else {
        shader.SetMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(world))));
        shader.SetVec3("uBaseColor", glm::vec3(object.color.r, object.color.g, object.color.b));
        shader.SetFloat("uSelected", isSelected ? 1.0f : 0.0f);
    }
    mesh.Draw();
}

void SceneRenderer::RenderScene(ViewportFramebuffer& target, const Scene& scene, const CameraView& camera,
                                const RenderSettings& settings) {
    if (!initialized_) return;
    EnsureMeshes();

    target.Resize(glm::max(target.Width(), 2), glm::max(target.Height(), 2));
    if (!target.Valid()) return;
    target.Bind();
    glViewport(0, 0, target.Width(), target.Height());
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glClearColor(0.09f, 0.10f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ---- opaque primitives -------------------------------------------------

    glm::vec3 lightPos(6.0f, 10.0f, 8.0f);
    glm::vec3 lightColor(1.0f, 0.96f, 0.9f);
    for (const auto& object : scene.objects) {
        if (object.light.enabled) {
            lightPos = glm::vec3(scene.WorldMatrix(object)[3]);
            lightColor = object.light.color * object.light.intensity;
            break;
        }
    }

    meshShader_.Use();
    meshShader_.SetMat4("uViewProjection", camera.projection * camera.view);
    meshShader_.SetVec3("uAmbient", scene.ambient + glm::vec3(0.08f));
    meshShader_.SetVec3("uLightPos", lightPos);
    meshShader_.SetVec3("uLightColor", lightColor);

    for (size_t i = 0; i < scene.objects.size(); ++i) {
        const GameObject& object = scene.objects[i];
        if (!object.visible) continue;
        DrawObject(scene, object, static_cast<int>(i), meshShader_, false,
                   static_cast<int>(i) == settings.selectedIndex, settings.projectRoot);
    }

    // ---- grid --------------------------------------------------------------
    if (settings.showGrid) {
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        lineShader_.Use();
        lineShader_.SetMat4("uViewProjection", camera.projection * camera.view);
        glBindVertexArray(gridVao_);
        glDrawArrays(GL_LINES, 0, gridVertexCount_);
        glBindVertexArray(0);
        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
    }

    // ---- debug helpers ------------------------------------------------------
    if (settings.showColliders || settings.showHelpers) {
        debugLines_.Begin();
        for (size_t i = 0; i < scene.objects.size(); ++i) {
            const GameObject& object = scene.objects[i];
            glm::mat4 world = scene.WorldMatrix(object);
            glm::vec3 position(world[3]);
            if (settings.showColliders && object.collision.enabled) {
                glm::vec3 half = object.collision.size * 0.5f;
                if (object.collision.shape == CollisionShapeType::Box) {
                    debugLines_.AddBox(glm::mat4(1.0f), position + object.collision.offset,
                                       half, {0.20f, 0.92f, 0.45f});
                } else {
                    float radius = glm::max(half.x, glm::max(half.y, half.z));
                    debugLines_.AddWireSphere(position + object.collision.offset, radius,
                                              {0.20f, 0.92f, 0.45f});
                }
            }
            if (settings.showHelpers) {
                if (object.light.enabled) {
                    debugLines_.AddWireSphere(position, 0.35f, object.light.color);
                    for (int r = 0; r < 8; ++r) {
                        float a = static_cast<float>(r) / 8.0f * 6.28318f;
                        glm::vec3 dir(std::cos(a), -0.35f, std::sin(a));
                        debugLines_.AddLine(position, position + dir * 1.1f, object.light.color);
                    }
                }
                if (object.camera.enabled) {
                    debugLines_.AddBox(glm::mat4(1.0f), position, glm::vec3(0.25f), {0.85f, 0.85f, 0.30f});
                    glm::vec3 forward = glm::normalize(glm::vec3(-world[2]));
                    debugLines_.AddLine(position, position + forward * 2.0f, {0.85f, 0.85f, 0.30f});
                }
            }
        }
        debugLines_.End(lineShader_);
        debugLines_.Draw(lineShader_);
    }

    target.Unbind();
}

int SceneRenderer::PickObject(const Scene& scene, const CameraView& camera, int pixelX, int pixelY,
                              int width, int height, const std::string& projectRoot) {
    if (!initialized_ || width < 2 || height < 2) return -1;
    EnsureMeshes();
    pickingFbo_.Resize(width, height);
    pickingFbo_.Bind();
    glViewport(0, 0, width, height);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    idShader_.Use();
    idShader_.SetMat4("uViewProjection", camera.projection * camera.view);
    for (size_t i = 0; i < scene.objects.size(); ++i) {
        if (!scene.objects[i].visible) continue;
        DrawObject(scene, scene.objects[i], static_cast<int>(i), idShader_, true, false, projectRoot);
    }

    unsigned char pixel[4] = {0, 0, 0, 0};
    glReadPixels(pixelX, pixelY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    pickingFbo_.Unbind();
    int id = pixel[0] | (pixel[1] << 8) | (pixel[2] << 16);
    if (id <= 0) return -1;
    int index = id - 1;
    if (index < 0 || index >= static_cast<int>(scene.objects.size())) return -1;
    return index;
}

} // namespace pspeditor
