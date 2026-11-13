// ---------------------------------------------------------------------------
//  PSP Engine Editor - application shell implementation
// ---------------------------------------------------------------------------

#include "Application.h"

#include <cstdio>
#include <cstring>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"

#include "Shared/Scene/SceneIO.h"
#include "Shared/BuildInfo.h"
#include "PspsdkInstaller.h"

namespace pspeditor {

namespace {

void GlfwErrorCallback(int code, const char* description) {
    std::fprintf(stderr, "[GLFW] error %d: %s\n", code, description);
}

} // namespace

// --------------------------------------------------------------------------
// Init
// --------------------------------------------------------------------------
bool Application::InitWindow(std::string& error) {
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit()) {
        error = "glfwInit() failed";
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    window_ = glfwCreateWindow(1600, 920, "PSP Engine Editor", nullptr, nullptr);
    if (!window_) {
        error = "Cannot create the GLFW window (OpenGL 3.3 core is required)";
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(glfwGetProcAddress))) {
        error = "Failed to load OpenGL 3.3 entry points with glad";
        return false;
    }
    std::printf("[PSPEditor] OpenGL %s | %s\n", reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
    return true;
}

bool Application::InitImGui(std::string& error) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "pspeditor_layout.ini";

    ApplyDarkTheme();

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        error = "ImGui GLFW backend init failed";
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        error = "ImGui OpenGL3 backend init failed";
        return false;
    }
    return true;
}

void Application::ApplyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(7, 5);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.075f, 0.082f, 0.098f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.065f, 0.070f, 0.085f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.18f, 0.20f, 0.24f, 0.80f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.21f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.26f, 0.31f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.06f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.10f, 0.13f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.16f, 0.19f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.27f, 0.34f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.35f, 0.45f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.16f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.21f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.33f, 0.43f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.21f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.16f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.24f, 0.45f, 0.70f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.05f, 0.06f, 0.07f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.72f, 1.00f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(0.88f, 0.90f, 0.93f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.48f, 0.51f, 0.56f, 1.00f);
}

// --------------------------------------------------------------------------
// Run
// --------------------------------------------------------------------------
int Application::Run(const std::string& projectPath) {
    startupProjectPath_ = projectPath;
    std::string error;
    if (!InitWindow(error)) {
        std::fprintf(stderr, "[PSPEditor] %s\n", error.c_str());
        return 1;
    }
    if (!InitImGui(error)) {
        std::fprintf(stderr, "[PSPEditor] %s\n", error.c_str());
        return 1;
    }
    if (!state_.renderer.Init(error)) {
        std::fprintf(stderr, "[PSPEditor] Renderer init failed: %s\n", error.c_str());
        return 1;
    }

    state_.lua.SetLogger([this](const std::string& message) {
        state_.Log(ConsoleEntry::Level::Script, message);
    });

    // Open the project passed on the command line when present.
    if (!startupProjectPath_.empty()) {
        std::string openError;
        if (state_.OpenProject(fsx::Absolute(startupProjectPath_), openError)) {
            state_.statusMessage = "Project opened: " + state_.project.name;
            state_.statusMessageTime = 3.0f;
        } else {
            std::fprintf(stderr, "[PSPEditor] %s\n", openError.c_str());
            showStartupDialog_ = true;
        }
    }

    // Try to auto-open a project next to the executable or in the CWD.
    std::string candidate = fsx::PathJoin(fsx::ExecutableDir(), "project.json");
    if (!state_.project.valid) {
        if (fsx::FileExists(candidate)) {
            std::string openError;
            state_.OpenProject(fsx::ExecutableDir(), openError);
        } else {
            candidate = fsx::Absolute("project.json");
            if (fsx::FileExists(candidate)) {
                std::string openError;
                state_.OpenProject(fsx::ParentDir(candidate), openError);
            }
        }
    }
    if (!state_.project.valid) showStartupDialog_ = true;

    DetectPspsdkOnStartup();

    MainLoop();

    // Wait for a running toolchain download so Shutdown() can join cleanly.
    pspsdkCancel_ = true;
    if (pspsdkWorker_.joinable()) pspsdkWorker_.join();
    return 0;
}

void Application::MainLoop() {
    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime_);
        lastTime_ = now;
        if (dt < 0.0f) dt = 0.0f;
        if (dt > 0.25f) dt = 0.25f;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        state_.NewFrame(dt);
        DrawFrame();

        ImGui::Render();
        int width = 0, height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        glViewport(0, 0, width, height);
        glClearColor(0.045f, 0.050f, 0.060f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup);
        }

        glfwSwapBuffers(window_);
    }
}

// --------------------------------------------------------------------------
// Frame
// --------------------------------------------------------------------------
void Application::BuildDefaultLayout() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiID dockspaceId = ImGui::GetID("PSPEditorDockspace");
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

    ImGuiID center = dockspaceId;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);
    ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.45f, nullptr, &left);

    ImGui::DockBuilderDockWindow("3D Viewport", center);
    ImGui::DockBuilderDockWindow("Scene Outliner", left);
    ImGui::DockBuilderDockWindow("Content Browser", leftBottom);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Build Settings", right);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderFinish(dockspaceId);
    ImGui::DockBuilderFinish(dockspaceId);
}

void Application::DrawFrame() {
    if (!layoutInitialized_) {
        BuildDefaultLayout();
        layoutInitialized_ = true;
    }
    ImGui::DockSpaceOverViewport(ImGui::GetID("PSPEditorDockspace"), nullptr,
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    DrawMenuBar();
    DrawStartupDialog();
    DrawPspsdkPanel();

    DrawOutlinerPanel();
    DrawInspectorPanel();
    DrawContentBrowserPanel();
    DrawConsolePanel();
    DrawBuildSettingsPanel();
    DrawViewportPanel();

    HandleGlobalShortcuts();

    // Status bar.
    if (ImGui::BeginViewportSideBar("##StatusBar", nullptr, ImGuiDir_Down, ImGui::GetFrameHeight(),
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar)) {
        if (ImGui::BeginMenuBar()) {
            ImGui::TextUnformatted("PSP Engine " PSPENGINE_VERSION_STR " | Editor");
            ImGui::Separator();
            if (state_.project.valid) {
                ImGui::Text("Project: %s", state_.project.name.c_str());
            } else {
                ImGui::TextUnformatted("No project");
            }
            ImGui::Separator();
            ImGui::Text("Objects: %d", static_cast<int>(state_.scene.objects.size()));
            ImGui::Separator();
            ImGui::Text("Lua: %s", state_.playing ? "running" : "idle");
            ImGui::Separator();
            if (state_.building) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Building EBOOT...");
            } else if (state_.statusMessageTime > 0.0f) {
                ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.6f, 1.0f), "%s", state_.statusMessage.c_str());
            }
            ImGui::EndMenuBar();
        }
        ImGui::End();
    }
}

void Application::HandleGlobalShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    const bool ctrl = io.KeyCtrl;
    const bool shift = io.KeyShift;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
        std::string error;
        state_.CreateNewScene(error);
        state_.statusMessage = "New scene";
        state_.statusMessageTime = 2.0f;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        std::string error;
        if (state_.SaveScene(error)) {
            state_.statusMessage = "Scene saved";
        } else {
            state_.LogError(error);
        }
        state_.statusMessageTime = 2.0f;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) state_.DuplicateSelected();
    if (ctrl && !shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        int value = state_.renderSettings.selectedIndex;
        if (state_.selectionHistory.Undo(value)) state_.Select(value, false);
    }
    if (ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        int value = state_.renderSettings.selectedIndex;
        if (state_.selectionHistory.Redo(value)) state_.Select(value, false);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !io.WantTextInput) state_.DeleteSelected();
    if (ImGui::IsKeyPressed(ImGuiKey_W, false) && !io.WantTextInput) state_.gizmoMode = GizmoMode::Translate;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false) && !io.WantTextInput) state_.gizmoMode = GizmoMode::Rotate;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !io.WantTextInput) state_.gizmoMode = GizmoMode::Scale;
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        if (state_.playing) {
            state_.StopPlay();
        } else {
            state_.StartPlay();
        }
    }
}

// --------------------------------------------------------------------------
// Menu bar
// --------------------------------------------------------------------------
void Application::DrawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
            std::string error;
            state_.CreateNewScene(error);
        }
        if (ImGui::MenuItem("Open Project...")) {
            showStartupDialog_ = true;
        }
        if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
            std::string error;
            if (state_.SaveScene(error)) state_.statusMessage = "Scene saved";
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) glfwSetWindowShouldClose(window_, GLFW_TRUE);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Duplicate", "Ctrl+D")) state_.DuplicateSelected();
        if (ImGui::MenuItem("Delete", "Del")) state_.DeleteSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Translate (W)", nullptr, state_.gizmoMode == GizmoMode::Translate)) state_.gizmoMode = GizmoMode::Translate;
        if (ImGui::MenuItem("Rotate (E)", nullptr, state_.gizmoMode == GizmoMode::Rotate)) state_.gizmoMode = GizmoMode::Rotate;
        if (ImGui::MenuItem("Scale (R)", nullptr, state_.gizmoMode == GizmoMode::Scale)) state_.gizmoMode = GizmoMode::Scale;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("GameObject")) {
        if (ImGui::MenuItem("Cube")) state_.AddPrimitive(pspengine::PrimitiveType::Cube);
        if (ImGui::MenuItem("Sphere")) state_.AddPrimitive(pspengine::PrimitiveType::Sphere);
        if (ImGui::MenuItem("Cone")) state_.AddPrimitive(pspengine::PrimitiveType::Cone);
        if (ImGui::MenuItem("Cylinder")) state_.AddPrimitive(pspengine::PrimitiveType::Cylinder);
        if (ImGui::MenuItem("Plane")) state_.AddPrimitive(pspengine::PrimitiveType::Plane);
        if (ImGui::MenuItem("Empty")) state_.AddPrimitive(pspengine::PrimitiveType::Empty);
        ImGui::Separator();
        if (ImGui::MenuItem("Sun Light")) state_.AddLight();
        if (ImGui::MenuItem("Camera")) state_.AddCamera();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Tools")) {
        if (ImGui::MenuItem("Build Settings...")) state_.buildSettingsOpen = true;
        if (ImGui::MenuItem("Console")) state_.consoleOpen = true;
        ImGui::Separator();
        const bool sdkReady = IsPspsdkRoot(fsx::ParentDir(state_.buildSettingsPspsdk));
        if (ImGui::MenuItem("Install PSPSDK...", nullptr, false, !pspsdkInstallRunning_)) {
            pspsdkWindowOpen_ = true;
            pspsdkInstallFinished_ = false;
            pspsdkLog_.clear();
            pspsdkOfferPending_ = true;
        }
        if (!sdkReady) {
            ImGui::TextDisabled("  PSPSDK: not installed");
        } else {
            ImGui::TextDisabled("  PSPSDK: %s", state_.buildSettingsPspsdk.c_str());
        }
        ImGui::Separator();
        if (ImGui::MenuItem(state_.playing ? "Stop (F5)" : "Play (F5)")) {
            if (state_.playing) state_.StopPlay(); else state_.StartPlay();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Grid", nullptr, &state_.renderSettings.showGrid);
        ImGui::MenuItem("Colliders", nullptr, &state_.renderSettings.showColliders);
        ImGui::MenuItem("Lights & cameras", nullptr, &state_.renderSettings.showHelpers);
        ImGui::Separator();
        ImGui::MenuItem("Metrics", nullptr, &state_.showMetrics);
        ImGui::MenuItem("ImGui demo", nullptr, &state_.showDemo);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

// --------------------------------------------------------------------------
// Startup dialog
// --------------------------------------------------------------------------
void Application::DrawStartupDialog() {
    if (showStartupDialog_) {
        ImGui::OpenPopup("Welcome to PSP Engine");
        showStartupDialog_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(560, 300), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Welcome to PSP Engine", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Welcome to the PSP Engine editor. Create a new project or open an "
                           "existing project folder.");
        ImGui::Separator();
        ImGui::InputText("Project name", newProjectName_, sizeof(newProjectName_));
        ImGui::InputText("Project path", newProjectPath_, sizeof(newProjectPath_));
        ImGui::TextDisabled("The path is relative to the editor executable.");

        if (ImGui::Button("Create Project", ImVec2(160, 0))) {
            std::string path = fsx::Absolute(newProjectPath_);
            std::string error;
            if (state_.NewProject(path, newProjectName_, error)) {
                state_.statusMessage = "Project created";
                state_.statusMessageTime = 3.0f;
                pendingProjectPath_ = path;
                ImGui::CloseCurrentPopup();
            } else {
                state_.LogError(error);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Project...", ImVec2(160, 0))) {
            std::string path = fsx::Absolute(newProjectPath_);
            std::string error;
            if (state_.OpenProject(path, error)) {
                state_.statusMessage = "Project opened";
                state_.statusMessageTime = 3.0f;
                ImGui::CloseCurrentPopup();
            } else {
                state_.LogError(error);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Use Empty Scene", ImVec2(160, 0))) {
            state_.project.valid = false;
            state_.CreateNewScene(startupError_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::Separator();
        if (!startupError_.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.35f, 1.0f), "%s", startupError_.c_str());
        }
        ImGui::EndPopup();
    }
}

// --------------------------------------------------------------------------
// Camera control in the viewport
// --------------------------------------------------------------------------
void Application::DrawSceneCameraControl(float dt) {
    ImGuiIO& io = ImGui::GetIO();
    if (!state_.viewportFocused) return;

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
        state_.camera.HandleMouseDelta(io.MouseDelta.x, io.MouseDelta.y,
                                       ImGui::IsMouseDown(ImGuiMouseButton_Right),
                                       ImGui::IsMouseDown(ImGuiMouseButton_Middle));
    }
    if (state_.viewportFocused) {
        state_.camera.HandleScroll(io.MouseWheel);
    }
    const bool fast = io.KeyShift;
    const bool right = ImGui::IsMouseDown(ImGuiMouseButton_Right);
    state_.camera.SetMoveInput(
        right && ImGui::IsKeyDown(ImGuiKey_W), right && ImGui::IsKeyDown(ImGuiKey_S),
        right && ImGui::IsKeyDown(ImGuiKey_A), right && ImGui::IsKeyDown(ImGuiKey_D),
        right && ImGui::IsKeyDown(ImGuiKey_E), right && ImGui::IsKeyDown(ImGuiKey_Q), fast);
    state_.camera.Update(dt);

    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        pspengine::GameObject* object = state_.SelectedObject();
        if (object) {
            state_.camera.FocusOn(glm::vec3(state_.scene.WorldMatrix(*object)[3]));
        }
    }

    // Play-mode keyboard mapping (mirrors the PSP pad).
    if (state_.playing) {
        state_.playInput.Clear();
        state_.playInput.held[static_cast<int>(pspengine::InputButton::Cross)] =
            ImGui::IsKeyDown(ImGuiKey_Space);
        state_.playInput.held[static_cast<int>(pspengine::InputButton::Left)] =
            ImGui::IsKeyDown(ImGuiKey_LeftArrow);
        state_.playInput.held[static_cast<int>(pspengine::InputButton::Right)] =
            ImGui::IsKeyDown(ImGuiKey_RightArrow);
        state_.playInput.held[static_cast<int>(pspengine::InputButton::Up)] =
            ImGui::IsKeyDown(ImGuiKey_UpArrow);
        state_.playInput.held[static_cast<int>(pspengine::InputButton::Down)] =
            ImGui::IsKeyDown(ImGuiKey_DownArrow);
        state_.playInput.analogX = (state_.playInput.held[static_cast<int>(pspengine::InputButton::Right)] ? 1.0f : 0.0f) -
                                   (state_.playInput.held[static_cast<int>(pspengine::InputButton::Left)] ? 1.0f : 0.0f);
        state_.playInput.pressed[static_cast<int>(pspengine::InputButton::Cross)] =
            ImGui::IsKeyPressed(ImGuiKey_Space, false);
    }
}

} // namespace pspeditor

namespace pspeditor {

// --------------------------------------------------------------------------
// Shutdown: release every GPU/OS resource in reverse order of creation.
// --------------------------------------------------------------------------
void Application::Shutdown() {
    state_.Shutdown();
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

} // namespace pspeditor

// ---------------------------------------------------------------------------
//  PSPSDK detection / auto-installation UI
// ---------------------------------------------------------------------------

namespace pspeditor {

void Application::DetectPspsdkOnStartup() {
    std::string details;
    const std::string sdkBin = LocatePspsdk(state_.buildSettingsPspsdk, details);
    state_.buildSettingsPspsdk = sdkBin;

    if (!sdkBin.empty()) {
        state_.LogInfo(details);
        pspsdkOfferPending_ = false;
        return;
    }

    state_.LogWarning(details);
    PlatformAsset asset;
    std::string reason;
    if (!ResolvePspsdkAsset(asset, reason)) {
        // Unsupported host (e.g. native Windows): explain instead of offering.
        for (const auto& hint : PspsdkManualInstallHints()) state_.LogInfo(hint);
        state_.LogWarning(reason);
        pspsdkOfferPending_ = false;
        return;
    }

    // First launch without a toolchain: offer to install it right away.
    pspsdkOfferPending_ = true;
    pspsdkLog_.clear();
    pspsdkStatusLine_.clear();
    pspsdkInstallFinished_ = false;
}

void Application::StartPspsdkInstall() {
    if (pspsdkInstallRunning_) return;

    PlatformAsset asset;
    std::string reason;
    if (!ResolvePspsdkAsset(asset, reason)) {
        state_.LogWarning(reason);
        return;
    }

    pspsdkInstallRunning_ = true;
    pspsdkInstallFinished_ = false;
    pspsdkProgress_ = -1.0f;
    pspsdkCancel_ = false;
    pspsdkLog_.clear();
    pspsdkStatusLine_ = "Starting...";
    state_.LogInfo("Installing PSPSDK to " + ManagedPspsdkRoot() + " ...");

    const std::string targetRoot = ManagedPspsdkRoot();
    pspsdkWorker_ = std::thread([this, asset, targetRoot]() {
        InstallOptions options;
        options.targetRoot = targetRoot;
        options.assetUrl = asset.url;
        options.archiveName = asset.archiveName;

        InstallResult result = InstallPspsdk(
            options,
            [this](float fraction, const std::string& message) {
                pspsdkProgress_ = fraction;
                std::lock_guard<std::mutex> lock(pspsdkLogMutex_);
                pspsdkStatusLine_ = message;
            },
            &pspsdkCancel_);

        {
            std::lock_guard<std::mutex> lock(pspsdkLogMutex_);
            pspsdkLog_ = result.log;
            pspsdkStatusLine_ = result.success ? "PSPSDK installed successfully."
                                               : ("Installation failed: " + result.error);
        }
        pspsdkInstallRunning_ = false;
        pspsdkInstallFinished_ = true;

        if (result.success) {
            state_.buildSettingsPspsdk = result.pspsdkBin;
            state_.LogInfo("[OK] PSPSDK installed: " + result.pspsdkBin);
        } else {
            state_.LogWarning("[ERROR] " + result.error);
            for (const auto& hint : PspsdkManualInstallHints()) state_.LogInfo(hint);
        }
    });
}

void Application::DrawPspsdkPanel() {
    // Consume the worker's log once it finished.
    if (pspsdkInstallFinished_ && !pspsdkInstallRunning_) {
        std::lock_guard<std::mutex> lock(pspsdkLogMutex_);
        if (!pspsdkLog_.empty()) {
            for (const auto& line : [&] {
                     std::vector<std::string> lines;
                     std::string current;
                     for (char c : pspsdkLog_) {
                         if (c == '\n') { if (!current.empty()) lines.push_back(current); current.clear(); }
                         else current += c;
                     }
                     if (!current.empty()) lines.push_back(current);
                     return lines;
                 }()) {
                if (line.rfind("[INFO]", 0) == 0) state_.LogInfo(line);
                else if (line.rfind("[ERROR]", 0) == 0) state_.LogWarning(line);
            }
            pspsdkLog_.clear();
        }
    }

    // ---- Modal offer on first launch -------------------------------------
    if (pspsdkOfferPending_) {
        ImGui::OpenPopup("Install PSPSDK?");
        pspsdkOfferPending_ = false;
    }
    ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Install PSPSDK?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        PlatformAsset asset;
        std::string reason;
        ResolvePspsdkAsset(asset, reason);

        ImGui::TextWrapped(
            "No PlayStation Portable toolchain (PSPSDK) was found on this machine.");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "The editor can download the official prebuilt toolchain from github.com/pspdev "
            "(~175 MB) and unpack it next to the editor:");
        ImGui::BulletText("%s", ManagedPspsdkRoot().c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("Without it you can still edit scenes, but \"Build EBOOT.PBP\" will not work.");
        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::Button("Download and install (~175 MB)", ImVec2(320, 0))) {
            ImGui::CloseCurrentPopup();
            pspsdkWindowOpen_ = true;
            StartPspsdkInstall();
        }
        ImGui::SameLine();
        if (ImGui::Button("Not now", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            pspsdkOfferDismissed_ = true;
            state_.LogInfo("PSPSDK installation skipped. Tools -> Install PSPSDK to run it later.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Manual instructions", ImVec2(180, 0))) {
            for (const auto& hint : PspsdkManualInstallHints()) state_.LogInfo(hint);
            pspsdkWindowOpen_ = true;
        }
        ImGui::EndPopup();
    }

    if (!pspsdkInstallRunning_ && !pspsdkInstallFinished_) return;
    if (!pspsdkWindowOpen_) return;

    ImGui::SetNextWindowSize(ImVec2(700, 320), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("PSPSDK installation##pspsdk", &pspsdkWindowOpen_)) {
        std::lock_guard<std::mutex> lock(pspsdkLogMutex_);
        ImGui::TextUnformatted(pspsdkStatusLine_.c_str());
        const float progress = pspsdkProgress_.load();
        if (progress >= 0.0f) {
            ImGui::ProgressBar(progress, ImVec2(-1, 0));
        } else if (pspsdkInstallRunning_) {
            ImGui::ProgressBar(-1.0f * static_cast<float>(ImGui::GetTime()), ImVec2(-1, 0), "");
        }
        if (pspsdkInstallRunning_ && ImGui::Button("Cancel")) {
            pspsdkCancel_ = true;
        }
        ImGui::Separator();
        ImGui::BeginChild("##pspsdklog", ImVec2(0, 0), ImGuiChildFlags_Borders);
        ImGui::TextUnformatted(pspsdkLog_.c_str());
        ImGui::EndChild();
    }
    ImGui::End();
}

} // namespace pspeditor
