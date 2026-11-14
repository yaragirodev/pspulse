#pragma once

// ---------------------------------------------------------------------------
//  PSPulse Editor - application shell (GLFW + OpenGL 3.3 + ImGui docking)
// ---------------------------------------------------------------------------

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "EditorState.h"
#include "PspsdkInstaller.h"

struct GLFWwindow;

namespace pspeditor {

class Application {
public:
    int Run(const std::string& projectPath = std::string());
    void Shutdown();

private:
    bool InitWindow(std::string& error);
    bool InitImGui(std::string& error);
    void MainLoop();
    void DrawFrame();
    void HandleGlobalShortcuts();
    void ApplyDarkTheme();
    void DrawMenuBar();
    void DrawViewportPanel();
    void DrawOutlinerPanel();
    void DrawInspectorPanel();
    void DrawContentBrowserPanel();
    void DrawConsolePanel();
    void DrawBuildSettingsPanel();
    void DrawPspsdkPanel();
    void DrawSceneCameraControl(float dt);
    void DrawStartupDialog();
    void BuildDefaultLayout();
    std::string startupProjectPath_;

    GLFWwindow* window_ = nullptr;
    EditorState state_;
    float lastTime_ = 0.0f;
    bool showStartupDialog_ = false;
    bool layoutInitialized_ = false;
    std::string startupError_;
    std::string pendingProjectPath_;

    // --- PSPSDK auto-installation state -----------------------------------
    // Pending offer, shown as a modal on the very first launch when no
    // toolchain is present:
    bool pspsdkOfferPending_ = false;
    bool pspsdkInstallRunning_ = false;
    bool pspsdkInstallFinished_ = false;
    bool pspsdkOfferDismissed_ = false;
    std::thread pspsdkWorker_;
    std::atomic<bool> pspsdkCancel_{false};
    std::atomic<float> pspsdkProgress_{-1.0f};
    std::mutex pspsdkLogMutex_;
    std::string pspsdkStatusLine_;
    std::string pspsdkLog_;
    bool pspsdkWindowOpen_ = true;

    // Launches the background install (idempotent).
    void StartPspsdkInstall();
    // Called once at startup: locate the SDK, queue the install offer or print
    // manual instructions.
    void DetectPspsdkOnStartup();
    char newProjectName_[128] = "My PSP Game";
    char newProjectPath_[512] = "projects/MyPSPGame";
};

} // namespace pspeditor
