#pragma once

// ---------------------------------------------------------------------------
//  PSP Engine Editor - editor application state
// ---------------------------------------------------------------------------

#include <atomic>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "BuildPipeline.h"
#include "Camera.h"
#include "ProjectIO.h"
#include "Renderer.h"
#include "Shared/Physics/Physics.h"
#include "Shared/Scene/SceneData.h"
#include "Shared/Scripting/LuaVM.h"

namespace pspeditor {

struct ConsoleEntry {
    enum class Level { Info, Warning, Error, Script };
    Level level = Level::Info;
    std::string text;
};

struct ContentEntry {
    std::string name;
    std::string relativePath;
    bool isDirectory = false;
};

struct SelectionHistory {
    void Push(int selected);
    bool Undo(int& outSelected);
    bool Redo(int& outSelected);
    void Clear();

private:
    std::vector<int> undo_;
    std::vector<int> redo_;
};

enum class GizmoMode { Translate = 0, Rotate, Scale };

class EditorState {
public:
    // ---- lifecycle ---------------------------------------------------------
    bool NewProject(const std::string& rootDir, const std::string& name, std::string& error);
    bool OpenProject(const std::string& rootDir, std::string& error);
    bool SaveScene(std::string& error);
    bool LoadSceneFromPath(const std::string& path, std::string& error);
    bool CreateNewScene(std::string& error);

    void NewFrame(float dt);
    void Shutdown();

    // ---- scene editing ------------------------------------------------------
    pspengine::GameObject* SelectedObject();
    int SelectedIndex() const;
    void Select(int index, bool recordHistory = true);
    void AddPrimitive(pspengine::PrimitiveType type);
    void AddLight();
    void AddCamera();
    void DeleteSelected();
    void DuplicateSelected();
    int FindObjectIndexById(int id) const;

    // ---- console ------------------------------------------------------------
    void Log(ConsoleEntry::Level level, const std::string& text);
    void LogInfo(const std::string& text) { Log(ConsoleEntry::Level::Info, text); }
    void LogWarning(const std::string& text) { Log(ConsoleEntry::Level::Warning, text); }
    void LogError(const std::string& text) { Log(ConsoleEntry::Level::Error, text); }

    // ---- content browser ----------------------------------------------------
    void RefreshContentBrowser();
    const std::vector<ContentEntry>& BrowserEntries() const { return browserEntries; }
    const std::string& BrowserDirectory() const { return browserDirectory; }
    void SetBrowserDirectory(const std::string& relative);
    std::string ResolveProjectPath(const std::string& relative) const;

    // ---- play mode ----------------------------------------------------------
    void StartPlay();
    void StopPlay();

    // ---- build --------------------------------------------------------------
    void StartBuild(const BuildOptions& options);
    void TickBuild();

    // ---- state (public, panel-oriented) -------------------------------------
    Project project;
    pspengine::Scene scene;
    pspengine::PhysicsWorld physicsWorld;
    pspengine::LuaVM lua;
    SceneRenderer renderer;
    EditorCamera camera;
    RenderSettings renderSettings;
    GizmoMode gizmoMode = GizmoMode::Translate;
    bool gizmoLocalSpace = false;

    std::string scenePath;
    bool sceneDirty = false;
    SelectionHistory selectionHistory;
    std::vector<ConsoleEntry> console;
    ConsoleEntry::Level consoleLevel = ConsoleEntry::Level::Info;

    // Play mode
    bool playing = false;
    pspengine::Scene playSceneBefore;
    float playTime = 0.0f;
    pspengine::InputState playInput;

    // Build state
    std::atomic<bool> building{false};
    std::atomic<bool> buildFinished{false};
    std::atomic<bool> cancelBuild{false};
    BuildResult buildResult;
    std::string buildSettingsPspsdk;
    bool buildSettingsOpen = false;
    bool consoleOpen = true;

    // Content browser
    std::string browserDirectory = "assets";
    std::string browserSelection;
    std::vector<ContentEntry> browserEntries;

    // UI
    bool viewportFocused = false;
    bool showMetrics = false;
    bool showDemo = false;
    std::string statusMessage;
    float statusMessageTime = 0.0f;
    double timeSeconds = 0.0;
    int frameCount = 0;

private:
    void BeginPlayHotReload();
};

} // namespace pspeditor
