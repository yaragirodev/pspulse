// ---------------------------------------------------------------------------
//  PSP Engine Editor - dockable panels
//    * Scene Outliner (hierarchy, DnD-aware)
//    * Inspector (transform + every component with "Add Component")
//    * Content Browser
//    * Console
//    * 3D Viewport (FBO + ImGuizmo + picking)
//    * Build Settings (XMB metadata + "Build EBOOT.PBP")
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cstring>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuizmo.h"

#include "Application.h"
#include "Shared/Mesh/GltfLoader.h"
#include "PspsdkInstaller.h"

namespace pspeditor {

using pspengine::CollisionShapeType;
using pspengine::GameObject;
using pspengine::PrimitiveType;

namespace {

const ImGuiTreeNodeFlags kLeafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                                       ImGuiTreeNodeFlags_SpanAvailWidth;

void DrawObjectIcon(const GameObject& object) {
    if (object.light.enabled) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.35f, 1.0f), "[L]");
        return;
    }
    if (object.camera.enabled) {
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.95f, 1.0f), "[C]");
        return;
    }
    switch (object.primitive) {
        case PrimitiveType::Cube: ImGui::TextColored(ImVec4(0.85f, 0.55f, 0.35f, 1.0f), "[B]"); break;
        case PrimitiveType::Sphere: ImGui::TextColored(ImVec4(0.45f, 0.70f, 0.95f, 1.0f), "[S]"); break;
        case PrimitiveType::Cone: ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.40f, 1.0f), "[A]"); break;
        case PrimitiveType::Cylinder: ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.60f, 1.0f), "[Y]"); break;
        case PrimitiveType::Plane: ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.70f, 1.0f), "[P]"); break;
        default: ImGui::TextColored(ImVec4(0.50f, 0.50f, 0.55f, 1.0f), "[O]"); break;
    }
    ImGui::SameLine();
}

// Picks the texture id of a framebuffer colour attachment for ImGui::Image.
inline ImTextureID TextureId(unsigned int glTexture) {
    return static_cast<ImTextureID>(static_cast<uintptr_t>(glTexture));
}

} // namespace

// ==========================================================================
//  Scene Outliner
// ==========================================================================
void Application::DrawOutlinerPanel() {
    if (!ImGui::Begin("Scene Outliner")) {
        ImGui::End();
        return;
    }

    if (ImGui::Button("+ Cube")) state_.AddPrimitive(PrimitiveType::Cube);
    ImGui::SameLine();
    if (ImGui::Button("+ Sphere")) state_.AddPrimitive(PrimitiveType::Sphere);
    ImGui::SameLine();
    if (ImGui::Button("+ Light")) state_.AddLight();
    ImGui::SameLine();
    if (ImGui::Button("+ Camera")) state_.AddCamera();

    ImGui::Separator();
    if (state_.scene.objects.empty()) {
        ImGui::TextDisabled("Scene is empty - add a primitive above.");
    }

    auto drawChildren = [&](auto&& self, int parentId) -> void {
        for (size_t i = 0; i < state_.scene.objects.size(); ++i) {
            GameObject& object = state_.scene.objects[i];
            if (object.parent != parentId) continue;

            ImGui::PushID(object.id);
            bool hasChildren = false;
            for (const auto& candidate : state_.scene.objects) {
                if (candidate.parent == object.id) {
                    hasChildren = true;
                    break;
                }
            }
            ImGuiTreeNodeFlags flags = hasChildren ? ImGuiTreeNodeFlags_OpenOnArrow |
                                                         ImGuiTreeNodeFlags_SpanAvailWidth
                                                   : kLeafFlags;
            if (static_cast<int>(i) == state_.renderSettings.selectedIndex)
                flags |= ImGuiTreeNodeFlags_Selected;

            DrawObjectIcon(object);
            bool open = ImGui::TreeNodeEx("##node", flags, "%s", object.name.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                state_.Select(static_cast<int>(i));
            }
            if (!object.visible) {
                ImGui::SameLine();
                ImGui::TextDisabled("(hidden)");
            }
            if (!object.script.empty()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.55f, 1.0f), "<lua>");
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PSP_OBJECT")) {
                    if (payload->DataSize == sizeof(int)) {
                        int draggedId = *static_cast<const int*>(payload->Data);
                        if (draggedId != object.id && !state_.scene.IsDescendantOf(object.id, draggedId)) {
                            GameObject* dragged = state_.scene.Find(draggedId);
                            if (dragged) {
                                dragged->parent = object.id;
                                state_.sceneDirty = true;
                            }
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            if (ImGui::BeginDragDropSource()) {
                int id = object.id;
                ImGui::SetDragDropPayload("PSP_OBJECT", &id, sizeof(int));
                ImGui::Text("Reparent %s", object.name.c_str());
                ImGui::EndDragDropSource();
            }

            ImGui::PopID();
            if (open) {
                if (hasChildren) self(self, object.id);
                if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) ImGui::TreePop();
            }
        }
    };
    drawChildren(drawChildren, -1);

    // Root items whose parent was deleted.
    ImGui::Separator();
    if (ImGui::Button("Unparent selected") ) {
        GameObject* object = state_.SelectedObject();
        if (object) {
            object->parent = -1;
            state_.sceneDirty = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) state_.DeleteSelected();

    ImGui::End();
}

// ==========================================================================
//  Inspector
// ==========================================================================
void Application::DrawInspectorPanel() {
    if (!ImGui::Begin("Inspector")) {
        ImGui::End();
        return;
    }
    GameObject* object = state_.SelectedObject();
    if (!object) {
        ImGui::TextDisabled("Nothing selected.");
        ImGui::Separator();
        ImGui::TextWrapped("Select an object in the viewport or outliner to edit its transform and "
                           "components.");
        ImGui::End();
        return;
    }

    char nameBuffer[128];
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", object->name.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        object->name = nameBuffer;
        state_.sceneDirty = true;
    }
    ImGui::Checkbox("Visible", &object->visible);
    ImGui::SameLine();
    ImGui::TextDisabled("id %d", object->id);

    // ---- Transform ------------------------------------------------------
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;
        changed |= ImGui::DragFloat3("Position", glm::value_ptr(object->transform.position), 0.05f);
        changed |= ImGui::DragFloat3("Rotation", glm::value_ptr(object->transform.rotation), 0.5f,
                                     -360.0f, 360.0f, "%.1f deg");
        changed |= ImGui::DragFloat3("Scale", glm::value_ptr(object->transform.scale), 0.02f, 0.001f,
                                     1000.0f);
        ImGui::ColorEdit4("Color", glm::value_ptr(object->color));
        if (changed) state_.sceneDirty = true;

        if (object->primitive != PrimitiveType::Empty) {
            int primitive = static_cast<int>(object->primitive);
            const char* primitives[] = {"Empty", "Cube", "Sphere", "Cone", "Cylinder", "Plane"};
            if (ImGui::Combo("Primitive", &primitive, primitives, IM_ARRAYSIZE(primitives))) {
                object->primitive = static_cast<PrimitiveType>(primitive);
                state_.sceneDirty = true;
            }
        }
        if (ImGui::Button("Reset Transform")) {
            object->transform.position = glm::vec3(0.0f);
            object->transform.rotation = glm::vec3(0.0f);
            object->transform.scale = glm::vec3(1.0f);
            state_.sceneDirty = true;
        }
    }

    // ---- Mesh (glTF model) -----------------------------------------------
    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        char meshBuffer[256];
        std::snprintf(meshBuffer, sizeof(meshBuffer), "%s", object->meshPath.c_str());
        ImGui::SetNextItemWidth(-90.0f);
        if (ImGui::InputText("Model (glTF)", meshBuffer, sizeof(meshBuffer))) {
            object->meshPath = meshBuffer;
            state_.sceneDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear##mesh")) {
            object->meshPath.clear();
            state_.sceneDirty = true;
        }

        // Quick pick: every .gltf/.glb found in the project's assets/ folder.
        std::vector<std::string> models;
        std::string assetsDir = fsx::PathJoin(state_.project.rootPath, "assets");
        if (fsx::DirectoryExists(assetsDir)) {
            for (const auto& entry : fsx::ListDirectory(assetsDir)) {
                const std::string ext = fsx::ExtensionLower(entry.name);
                if (!entry.isDirectory && (ext == ".gltf" || ext == ".glb")) {
                    models.push_back("assets/" + entry.name);
                } else if (entry.isDirectory) {
                    for (const auto& sub : fsx::ListDirectory(entry.path)) {
                        const std::string subExt = fsx::ExtensionLower(sub.name);
                        if (!sub.isDirectory && (subExt == ".gltf" || subExt == ".glb")) {
                            models.push_back("assets/" + entry.name + "/" + sub.name);
                        }
                    }
                }
            }
        }
        if (!models.empty()) {
            if (ImGui::BeginCombo("Available", object->meshPath.empty() ? "(primitive)" : object->meshPath.c_str())) {
                for (const auto& candidate : models) {
                    const bool selected = candidate == object->meshPath;
                    if (ImGui::Selectable(candidate.c_str(), selected)) {
                        object->meshPath = candidate;
                        state_.sceneDirty = true;
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::TextDisabled("Drop .gltf/.glb files into assets/ to pick them here.");
        }

        // Diagnostics: report the loader's verdict for the current model.
        if (!object->meshPath.empty()) {
            pspengine::MeshData probe;
            pspengine::GltfLoadStats stats;
            std::string error;
            const std::string fullPath = fsx::PathJoin(state_.project.rootPath, object->meshPath);
            if (pspengine::LoadGltfMesh(fullPath, probe, stats, error)) {
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.6f, 1.0f), "%d verts / %d tris / %d mesh(es)",
                                   static_cast<int>(stats.vertices), static_cast<int>(stats.triangles),
                                   stats.meshes);
                for (const auto& warning : stats.warnings) ImGui::TextDisabled("! %s", warning.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.4f, 1.0f), "%s", error.c_str());
            }
        }
    }

    // ---- Lua script ------------------------------------------------------
    if (ImGui::CollapsingHeader("Lua Script", ImGuiTreeNodeFlags_DefaultOpen)) {
        char scriptBuffer[256];
        std::snprintf(scriptBuffer, sizeof(scriptBuffer), "%s", object->script.c_str());
        if (ImGui::InputText("Script path", scriptBuffer, sizeof(scriptBuffer))) {
            object->script = scriptBuffer;
            state_.sceneDirty = true;
        }
        ImGui::TextDisabled("Callbacks: on_start(), on_update(dt)");
        if (ImGui::Button("Open in Content Browser") && !object->script.empty()) {
            state_.SetBrowserDirectory(fsx::ParentDir(object->script));
            state_.browserSelection = fsx::FileName(object->script);
        }
    }

    // ---- PhysicsBody -----------------------------------------------------
    if (ImGui::CollapsingHeader("Physics Body", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Enable Physics", &object->physics.enabled)) state_.sceneDirty = true;
        ImGui::BeginDisabled(!object->physics.enabled);
        ImGui::DragFloat("Mass", &object->physics.mass, 0.05f, 0.0f, 500.0f);
        ImGui::Checkbox("Use Gravity", &object->physics.useGravity);
        ImGui::Checkbox("Static", &object->physics.isStatic);
        ImGui::SliderFloat("Restitution", &object->physics.restitution, 0.0f, 1.0f);
        ImGui::SliderFloat("Friction", &object->physics.friction, 0.0f, 1.0f);
        ImGui::DragFloat3("Velocity", glm::value_ptr(object->physics.velocity), 0.1f);
        ImGui::EndDisabled();
    }

    // ---- Collision shape -------------------------------------------------
    if (ImGui::CollapsingHeader("Collision Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox("Enable Collision", &object->collision.enabled)) state_.sceneDirty = true;
        ImGui::BeginDisabled(!object->collision.enabled);
        int shape = static_cast<int>(object->collision.shape);
        const char* shapes[] = {"Box", "Sphere"};
        if (ImGui::Combo("Shape", &shape, shapes, IM_ARRAYSIZE(shapes))) {
            object->collision.shape = static_cast<CollisionShapeType>(shape);
            state_.sceneDirty = true;
        }
        if (object->collision.shape == CollisionShapeType::Sphere) {
            float diameter = object->collision.size.x;
            if (ImGui::DragFloat("Diameter", &diameter, 0.02f, 0.01f, 100.0f)) {
                object->collision.size = glm::vec3(diameter);
                state_.sceneDirty = true;
            }
        } else {
            if (ImGui::DragFloat3("Size", glm::value_ptr(object->collision.size), 0.02f, 0.01f, 200.0f))
                state_.sceneDirty = true;
        }
        ImGui::DragFloat3("Offset", glm::value_ptr(object->collision.offset), 0.02f);
        ImGui::EndDisabled();
    }

    // ---- Light -----------------------------------------------------------
    if (ImGui::CollapsingHeader("Light", object->light.enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        if (ImGui::Checkbox("Enable Light", &object->light.enabled)) state_.sceneDirty = true;
        ImGui::BeginDisabled(!object->light.enabled);
        ImGui::ColorEdit3("Light color", glm::value_ptr(object->light.color));
        ImGui::DragFloat("Intensity", &object->light.intensity, 0.02f, 0.0f, 10.0f);
        ImGui::DragFloat("Range", &object->light.range, 0.1f, 0.1f, 200.0f);
        ImGui::EndDisabled();
    }

    // ---- Camera ----------------------------------------------------------
    if (ImGui::CollapsingHeader("Camera", object->camera.enabled ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        if (ImGui::Checkbox("Enable Camera", &object->camera.enabled)) state_.sceneDirty = true;
        ImGui::BeginDisabled(!object->camera.enabled);
        ImGui::DragFloat("FOV", &object->camera.fov, 0.5f, 10.0f, 170.0f);
        ImGui::DragFloat("Near", &object->camera.nearPlane, 0.01f, 0.01f, 10.0f);
        ImGui::DragFloat("Far", &object->camera.farPlane, 1.0f, 1.0f, 1000.0f);
        if (ImGui::Checkbox("Mark as game camera", &object->camera.active)) {
            if (object->camera.active) {
                for (auto& other : state_.scene.objects) {
                    if (other.id != object->id) other.camera.active = false;
                }
            }
            state_.sceneDirty = true;
        }
        ImGui::EndDisabled();
    }

    ImGui::Separator();
    if (ImGui::Button("Add Component...")) ImGui::OpenPopup("add_component");
    if (ImGui::BeginPopup("add_component")) {
        if (ImGui::MenuItem("Physics Body", nullptr, object->physics.enabled)) {
            object->physics.enabled = true;
            state_.sceneDirty = true;
        }
        if (ImGui::MenuItem("Collision Shape", nullptr, object->collision.enabled)) {
            object->collision.enabled = true;
            state_.sceneDirty = true;
        }
        if (ImGui::MenuItem("Point Light", nullptr, object->light.enabled)) {
            object->light.enabled = true;
            state_.sceneDirty = true;
        }
        if (ImGui::MenuItem("Camera", nullptr, object->camera.enabled)) {
            object->camera.enabled = true;
            state_.sceneDirty = true;
        }
        ImGui::Separator();
        ImGui::TextDisabled("Extend in Inspector + SceneData.h");
        ImGui::EndPopup();
    }

    ImGui::End();
}

// ==========================================================================
//  Content Browser
// ==========================================================================
void Application::DrawContentBrowserPanel() {
    if (!ImGui::Begin("Content Browser")) {
        ImGui::End();
        return;
    }
    if (!state_.project.valid) {
        ImGui::TextDisabled("Open or create a project to browse its files.");
        ImGui::End();
        return;
    }

    // Breadcrumbs.
    if (ImGui::SmallButton("Project")) state_.SetBrowserDirectory("");
    std::string accumulated;
    size_t start = 0;
    while (start <= state_.browserDirectory.size()) {
        size_t slash = state_.browserDirectory.find('/', start);
        std::string part = state_.browserDirectory.substr(
            start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!part.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("/");
            ImGui::SameLine();
            accumulated += (accumulated.empty() ? "" : "/") + part;
            if (ImGui::SmallButton(part.c_str())) state_.SetBrowserDirectory(accumulated);
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) state_.RefreshContentBrowser();
    ImGui::Separator();

    for (const auto& entry : state_.BrowserEntries()) {
        ImGui::PushID(entry.relativePath.c_str());
        bool selected = state_.browserSelection == entry.relativePath;

        if (entry.isDirectory) {
            ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.35f, 1.0f), "[D]");
        } else {
            std::string ext = fsx::ExtensionLower(entry.name);
            ImVec4 color(0.75f, 0.78f, 0.82f, 1.0f);
            if (ext == ".lua") color = ImVec4(0.45f, 0.85f, 0.55f, 1.0f);
            else if (ext == ".scene" || ext == ".json") color = ImVec4(0.55f, 0.75f, 0.95f, 1.0f);
            else if (ext == ".png") color = ImVec4(0.95f, 0.60f, 0.75f, 1.0f);
            else if (ext == ".at3" || ext == ".mp3") color = ImVec4(0.85f, 0.70f, 0.95f, 1.0f);
            ImGui::TextColored(color, "[F]");
        }
        ImGui::SameLine();

        if (ImGui::Selectable(entry.name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
            state_.browserSelection = entry.relativePath;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (entry.isDirectory) {
                    state_.SetBrowserDirectory(entry.relativePath);
                } else if (fsx::ExtensionLower(entry.name) == ".scene") {
                    std::string error;
                    if (state_.LoadSceneFromPath(state_.ResolveProjectPath(entry.relativePath), error)) {
                        state_.statusMessage = "Scene loaded: " + entry.name;
                        state_.statusMessageTime = 2.0f;
                    } else {
                        state_.LogError(error);
                    }
                }
            }
        }

        // Drag & drop a script onto an object in the outliner / inspector.
        if (!entry.isDirectory) {
            if (ImGui::BeginDragDropSource()) {
                std::string payload = entry.relativePath;
                ImGui::SetDragDropPayload("PSP_ASSET", payload.c_str(), payload.size() + 1);
                ImGui::Text("Assign %s", entry.name.c_str());
                ImGui::EndDragDropSource();
            }
        }
        if (ImGui::BeginPopupContextItem("content_context")) {
            if (!entry.isDirectory && fsx::ExtensionLower(entry.name) == ".scene") {
                if (ImGui::MenuItem("Load scene")) {
                    std::string error;
                    state_.LoadSceneFromPath(state_.ResolveProjectPath(entry.relativePath), error);
                }
            }
            if (ImGui::MenuItem("Set as ICON0/PIC1...") && !entry.isDirectory &&
                fsx::ExtensionLower(entry.name) == ".png") {
                if (entry.name == "PIC1.PNG" || entry.name == "PIC1.png")
                    state_.project.package.pic1 = entry.relativePath;
                else
                    state_.project.package.icon0 = entry.relativePath;
                state_.LogInfo("XMB artwork set to " + entry.relativePath);
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::End();
}

// ==========================================================================
//  Console
// ==========================================================================
void Application::DrawConsolePanel() {
    if (!state_.consoleOpen) return;
    if (!ImGui::Begin("Console", &state_.consoleOpen)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("Clear")) state_.console.clear();
    ImGui::SameLine();
    ImGui::TextDisabled("%d messages", static_cast<int>(state_.console.size()));
    ImGui::Separator();
    ImGui::BeginChild("console_scroll", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& entry : state_.console) {
        ImVec4 color(0.80f, 0.83f, 0.87f, 1.0f);
        const char* prefix = "   ";
        switch (entry.level) {
            case ConsoleEntry::Level::Warning:
                color = ImVec4(0.98f, 0.78f, 0.30f, 1.0f);
                prefix = "W: ";
                break;
            case ConsoleEntry::Level::Error:
                color = ImVec4(1.00f, 0.42f, 0.38f, 1.0f);
                prefix = "E: ";
                break;
            case ConsoleEntry::Level::Script:
                color = ImVec4(0.50f, 0.90f, 0.60f, 1.0f);
                prefix = "L: ";
                break;
            default:
                break;
        }
        ImGui::TextColored(color, "%s%s", prefix, entry.text.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

// ==========================================================================
//  3D Viewport
// ==========================================================================
void Application::DrawViewportPanel() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool open = ImGui::Begin("3D Viewport");
    ImGui::PopStyleVar();
    if (!open) {
        ImGui::End();
        return;
    }

    ImVec2 panelSize = ImGui::GetContentRegionAvail();
    const int width = std::max(4, static_cast<int>(panelSize.x));
    const int height = std::max(4, static_cast<int>(panelSize.y));

    // glTF model paths are project-relative; the renderer resolves them here.
    state_.renderSettings.projectRoot = state_.project.rootPath;

    // Render the scene into the offscreen framebuffer.
    state_.camera.SetAspect(static_cast<float>(width) / static_cast<float>(height));
    CameraView view = state_.camera.BuildView();
    state_.renderer.Framebuffer().Resize(width, height);
    state_.renderer.RenderScene(state_.renderer.Framebuffer(), state_.scene, view,
                                state_.renderSettings);

    ImVec2 imageOrigin = ImGui::GetCursorScreenPos();
    ImGui::Image(TextureId(state_.renderer.ColorTexture()), ImVec2(static_cast<float>(width),
                                                                   static_cast<float>(height)),
                 ImVec2(0, 1), ImVec2(1, 0));
    state_.viewportFocused = ImGui::IsItemHovered() || ImGui::IsWindowFocused();

    // ---- picking --------------------------------------------------------
    if (state_.viewportFocused && ImGui::IsItemHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
        ImVec2 mouse = ImGui::GetIO().MousePos;
        int pixelX = static_cast<int>(mouse.x - imageOrigin.x);
        int pixelY = static_cast<int>(mouse.y - imageOrigin.y);
        // glReadPixels uses a bottom-left origin.
        int flippedY = height - 1 - pixelY;
        int index = state_.renderer.PickObject(state_.scene, view, pixelX, flippedY, width, height,
                                               state_.project.rootPath);
        state_.Select(index);
        state_.renderSettings.hoveredIndex = index;
    }

    float dt = ImGui::GetIO().DeltaTime;
    DrawSceneCameraControl(dt);

    // ---- ImGuizmo -------------------------------------------------------
    GameObject* selected = state_.SelectedObject();
    if (selected) {
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetRect(imageOrigin.x, imageOrigin.y, static_cast<float>(width),
                          static_cast<float>(height));

        glm::mat4 model = state_.scene.WorldMatrix(*selected);
        glm::mat4 viewMatrix = view.view;
        glm::mat4 projection = view.projection;

        ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
        if (state_.gizmoMode == GizmoMode::Rotate) operation = ImGuizmo::ROTATE;
        if (state_.gizmoMode == GizmoMode::Scale) operation = ImGuizmo::SCALE;

        ImGuizmo::MODE mode = state_.gizmoLocalSpace ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

        ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projection), operation, mode,
                             glm::value_ptr(model));

        if (ImGuizmo::IsUsing()) {
            glm::mat4 parentWorld(1.0f);
            if (selected->parent != -1) {
                const GameObject* parent = state_.scene.Find(selected->parent);
                if (parent) parentWorld = state_.scene.WorldMatrix(*parent);
            }
            glm::mat4 local = glm::inverse(parentWorld) * model;
            glm::vec3 translation, scale, skew;
            glm::quat rotation;
            glm::vec4 perspective;
            if (glm::decompose(local, scale, rotation, translation, skew, perspective)) {
                selected->transform.position = translation;
                selected->transform.scale = scale;
                glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
                if (operation == ImGuizmo::ROTATE) {
                    selected->transform.rotation =
                        pspengine::EulerDegreesFromMatrix(rotationMatrix);
                }
                state_.sceneDirty = true;
            }
        }

        // A small position/rotation readout overlay.
        ImGui::SetCursorScreenPos(ImVec2(imageOrigin.x + 10, imageOrigin.y + 10));
        ImGui::BeginGroup();
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "%s", selected->name.c_str());
        ImGui::Text("P: %.2f %.2f %.2f", selected->transform.position.x, selected->transform.position.y,
                    selected->transform.position.z);
        ImGui::Text("R: %.1f %.1f %.1f", selected->transform.rotation.x, selected->transform.rotation.y,
                    selected->transform.rotation.z);
        ImGui::EndGroup();
    }

    // ---- viewport toolbar ----------------------------------------------
    ImGui::SetCursorScreenPos(ImVec2(imageOrigin.x + 10, imageOrigin.y + height - 34));
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (ImGui::Button("Move (W)")) state_.gizmoMode = GizmoMode::Translate;
    ImGui::SameLine();
    if (ImGui::Button("Rotate (E)")) state_.gizmoMode = GizmoMode::Rotate;
    ImGui::SameLine();
    if (ImGui::Button("Scale (R)")) state_.gizmoMode = GizmoMode::Scale;
    ImGui::SameLine();
    ImGui::Checkbox("Local", &state_.gizmoLocalSpace);
    ImGui::SameLine();
    if (ImGui::Button(state_.playing ? "Stop (F5)" : "Play (F5)")) {
        if (state_.playing) state_.StopPlay(); else state_.StartPlay();
    }
    ImGui::PopStyleVar();
    ImGui::EndGroup();

    // HUD: camera info + controls hint.
    ImGui::SetCursorScreenPos(ImVec2(imageOrigin.x + width - 400, imageOrigin.y + 10));
    ImGui::BeginGroup();
    glm::vec3 camPos = state_.camera.Position();
    ImGui::TextDisabled("Cam %.1f %.1f %.1f", camPos.x, camPos.y, camPos.z);
    ImGui::TextDisabled("RMB look | RMB+WASD fly | MMB orbit | wheel zoom | F focus");
    ImGui::EndGroup();

    ImGui::End();
}

// ==========================================================================
//  Build Settings
// ==========================================================================
void Application::DrawBuildSettingsPanel() {
    if (!state_.buildSettingsOpen) return;
    ImGui::SetNextWindowSize(ImVec2(620, 640), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Build Settings", &state_.buildSettingsOpen)) {
        ImGui::End();
        return;
    }

    pspengine::PackageInfo& info = state_.project.package;

    ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "XMB (PSP System Menu)");
    ImGui::Separator();

    char title[128];
    std::snprintf(title, sizeof(title), "%s", info.title.c_str());
    if (ImGui::InputText("TITLE", title, sizeof(title))) info.title = title;

    char titleId[16];
    std::snprintf(titleId, sizeof(titleId), "%s", info.titleId.c_str());
    if (ImGui::InputText("TITLE_ID", titleId, sizeof(titleId))) info.titleId = titleId;
    ImGui::SameLine();
    ImGui::TextDisabled("(4-9 chars)");

    char developer[96];
    std::snprintf(developer, sizeof(developer), "%s", info.developer.c_str());
    if (ImGui::InputText("Developer", developer, sizeof(developer))) info.developer = developer;

    char version[16];
    std::snprintf(version, sizeof(version), "%s", info.version.c_str());
    if (ImGui::InputText("Version", version, sizeof(version))) info.version = version;

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "Artwork & Sound");
    ImGui::Separator();

    auto assetRow = [&](const char* label, const char* hint, std::string& value) {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
        if (ImGui::InputText(label, buffer, sizeof(buffer))) value = buffer;
        ImGui::SameLine();
        ImGui::TextDisabled("%s", hint);
    };
    assetRow("ICON0.PNG", "144x80 XMB icon", info.icon0);
    assetRow("PIC1.PNG", "480x272 background", info.pic1);
    assetRow("SND0.AT3", "ATRAC3 background music", info.snd0);

    if (!info.icon0.empty()) {
        std::string path = state_.ResolveProjectPath(info.icon0);
        if (!fsx::FileExists(path)) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "  ICON0 not found: %s", path.c_str());
        }
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), "Package");
    ImGui::Separator();
    assetRow("Scene", "scene file", info.scene);
    assetRow("Output dir", "build output", info.outputDir);

    char sdk[512];
    std::snprintf(sdk, sizeof(sdk), "%s", state_.buildSettingsPspsdk.c_str());
    if (ImGui::InputText("PSPSDK bin", sdk, sizeof(sdk))) state_.buildSettingsPspsdk = sdk;
    ImGui::SameLine();
    if (ImGui::SmallButton("Detect")) {
        std::string details;
        state_.buildSettingsPspsdk = LocatePspsdk("", details);
        state_.LogInfo(details.empty() ? "PSPSDK not found." : details);
    }

    ImGui::Separator();
    ImGui::BeginDisabled(state_.building);
    if (ImGui::Button("Build EBOOT.PBP", ImVec2(200, 40))) {
        BuildOptions options;
        options.pspsdkPath = state_.buildSettingsPspsdk;
        options.runtimeSourceDir = fsx::PathJoin(fsx::ExecutableDir(), "PSPRuntime");
        if (!fsx::FileExists(fsx::PathJoin(options.runtimeSourceDir, "Source/RuntimeApp.cpp"))) {
            options.runtimeSourceDir = fsx::Absolute("PSPRuntime");
        }
        state_.StartBuild(options);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (state_.building) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Building...");
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) state_.cancelBuild = true;
    } else if (state_.buildResult.success) {
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.6f, 1.0f), "EBOOT.PBP ready");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", state_.buildResult.ebootPath.c_str());
    } else if (!state_.buildResult.log.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "Last build failed");
    }

    ImGui::BeginChild("build_log", ImVec2(0, 220), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& warning : state_.buildResult.warnings) {
        ImGui::TextColored(ImVec4(0.98f, 0.78f, 0.30f, 1.0f), "W: %s", warning.c_str());
    }
    if (!state_.buildResult.log.empty()) ImGui::TextUnformatted(state_.buildResult.log.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::End();
}

} // namespace pspeditor
