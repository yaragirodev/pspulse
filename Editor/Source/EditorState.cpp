// ---------------------------------------------------------------------------
//  PSP Engine Editor - editor application state implementation
// ---------------------------------------------------------------------------

#include "EditorState.h"

#include <algorithm>
#include <thread>

#include <glm/gtc/matrix_transform.hpp>

#include "Shared/Scene/SceneIO.h"
#include "PspsdkInstaller.h"

namespace pspeditor {

using pspengine::GameObject;
using pspengine::PrimitiveType;
using pspengine::Scene;

// --------------------------------------------------------------------------
// SelectionHistory
// --------------------------------------------------------------------------
void SelectionHistory::Push(int selected) {
    if (!undo_.empty() && undo_.back() == selected) return;
    undo_.push_back(selected);
    if (undo_.size() > 128) undo_.erase(undo_.begin());
    redo_.clear();
}

bool SelectionHistory::Undo(int& outSelected) {
    if (undo_.size() < 2) return false;
    int current = undo_.back();
    undo_.pop_back();
    redo_.push_back(current);
    outSelected = undo_.back();
    return true;
}

bool SelectionHistory::Redo(int& outSelected) {
    if (redo_.empty()) return false;
    int value = redo_.back();
    redo_.pop_back();
    undo_.push_back(value);
    outSelected = value;
    return true;
}

void SelectionHistory::Clear() {
    undo_.clear();
    redo_.clear();
}

// --------------------------------------------------------------------------
// Project / scene lifecycle
// --------------------------------------------------------------------------
bool EditorState::NewProject(const std::string& rootDir, const std::string& name, std::string& error) {
    Project created;
    if (!CreateProject(rootDir, name, created, error)) return false;
    if (!OpenProject(rootDir, error)) return false;
    LogInfo("Created project '" + name + "' at " + rootDir);
    return true;
}

bool EditorState::OpenProject(const std::string& rootDir, std::string& error) {
    Project loaded;
    if (!LoadProject(rootDir, loaded, error)) return false;
    project = loaded;

    std::string sceneError;
    std::string sceneFile = ResolveProjectPath(project.package.scene);
    if (!pspengine::LoadSceneFromFile(sceneFile, scene, sceneError)) {
        LogWarning("Could not load " + sceneFile + " (" + sceneError + "), creating a new scene.");
        scene = pspengine::MakeDefaultScene();
    }
    scenePath = sceneFile;
    sceneDirty = false;
    selectionHistory.Clear();
    renderSettings.selectedIndex = -1;

    StopPlay();
    physicsWorld.Sync(scene);
    browserDirectory = "assets";
    RefreshContentBrowser();

    std::string sdkDetails;
    buildSettingsPspsdk = LocatePspsdk("", sdkDetails);
    if (buildSettingsPspsdk.empty()) {
        LogWarning(sdkDetails);
    } else {
        LogInfo(sdkDetails);
    }
    LogInfo("Opened project '" + project.name + "' (" + project.rootPath + ")");
    return true;
}

bool EditorState::SaveScene(std::string& error) {
    if (scenePath.empty()) return CreateNewScene(error);
    if (!pspengine::SaveSceneToFile(scene, scenePath, error)) return false;
    sceneDirty = false;
    LogInfo("Scene saved: " + scenePath);
    if (project.valid) {
        std::string projectError;
        SaveProject(project, projectError);
    }
    return true;
}

bool EditorState::LoadSceneFromPath(const std::string& path, std::string& error) {
    Scene loaded;
    if (!pspengine::LoadSceneFromFile(path, loaded, error)) return false;
    scene = loaded;
    scenePath = path;
    sceneDirty = false;
    selectionHistory.Clear();
    renderSettings.selectedIndex = -1;
    physicsWorld.Sync(scene);
    StopPlay();
    LogInfo("Loaded scene: " + path);
    return true;
}

bool EditorState::CreateNewScene(std::string& error) {
    scene = pspengine::MakeDefaultScene();
    if (project.valid) {
        scenePath = fsx::PathJoin(project.rootPath, "scenes/main.scene");
    } else {
        scenePath = fsx::Absolute("scenes/main.scene");
    }
    sceneDirty = true;
    selectionHistory.Clear();
    renderSettings.selectedIndex = -1;
    physicsWorld.Sync(scene);
    StopPlay();
    std::string saveError;
    SaveScene(saveError);
    (void)error;
    LogInfo("New scene created: " + scenePath);
    return true;
}

void EditorState::NewFrame(float dt) {
    timeSeconds += dt;
    ++frameCount;
    if (statusMessageTime > 0.0f) statusMessageTime -= dt;
    TickBuild();

    if (playing) {
        playTime += dt;
        // Simple script-driven play mode: run the Lua VM and physics like the
        // PSP runtime would.
        lua.SetInput(playInput);
        physicsWorld.Step(playSceneBefore, dt);
        lua.UpdateAll(playSceneBefore, dt);
        scene.objects = playSceneBefore.objects;
    }
}

void EditorState::Shutdown() {
    StopPlay();
    lua.Shutdown();
    renderer.Shutdown();
}

// --------------------------------------------------------------------------
// Selection & scene editing
// --------------------------------------------------------------------------
int EditorState::SelectedIndex() const {
    int index = renderSettings.selectedIndex;
    if (index < 0 || index >= static_cast<int>(scene.objects.size())) return -1;
    return index;
}

GameObject* EditorState::SelectedObject() {
    int index = SelectedIndex();
    if (index < 0) return nullptr;
    return &scene.objects[static_cast<size_t>(index)];
}

void EditorState::Select(int index, bool recordHistory) {
    if (index < -1 || index >= static_cast<int>(scene.objects.size())) return;
    renderSettings.selectedIndex = index;
    if (recordHistory) selectionHistory.Push(index);
}

int EditorState::FindObjectIndexById(int id) const {
    return scene.IndexOf(id);
}

void EditorState::AddPrimitive(PrimitiveType type) {
    std::string baseName = pspengine::PrimitiveDisplayName(type);
    int count = 1;
    for (const auto& object : scene.objects) {
        if (object.name.rfind(baseName, 0) == 0) ++count;
    }
    GameObject& object = scene.AddObject(baseName + " " + std::to_string(count), type);
    object.transform.position = glm::vec3(0.0f, 0.5f, 0.0f);
    if (type == PrimitiveType::Sphere) object.transform.position.y = 0.5f;
    sceneDirty = true;
    Select(scene.IndexOf(object.id));
    LogInfo("Added " + object.name);
    RefreshContentBrowser();
}

void EditorState::AddLight() {
    GameObject light = Scene::MakeLight("Sun " + std::to_string(scene.objects.size()));
    light.id = scene.AllocateId();
    scene.objects.push_back(light);
    sceneDirty = true;
    Select(static_cast<int>(scene.objects.size()) - 1);
    LogInfo("Added point light");
}

void EditorState::AddCamera() {
    GameObject cam = Scene::MakeCamera("Camera " + std::to_string(scene.objects.size()));
    cam.id = scene.AllocateId();
    scene.objects.push_back(cam);
    sceneDirty = true;
    Select(static_cast<int>(scene.objects.size()) - 1);
    LogInfo("Added camera");
}

void EditorState::DeleteSelected() {
    GameObject* object = SelectedObject();
    if (!object) return;
    std::string name = object->name;
    scene.RemoveObject(object->id);
    renderSettings.selectedIndex = -1;
    selectionHistory.Push(-1);
    sceneDirty = true;
    LogInfo("Deleted " + name);
}

void EditorState::DuplicateSelected() {
    GameObject* object = SelectedObject();
    if (!object) return;
    GameObject copy = *object;
    copy.id = scene.AllocateId();
    copy.name = object->name + " (copy)";
    if (copy.parent != -1) copy.parent = -1;
    scene.objects.push_back(copy);
    sceneDirty = true;
    Select(static_cast<int>(scene.objects.size()) - 1);
    LogInfo("Duplicated " + object->name);
}

// --------------------------------------------------------------------------
// Console
// --------------------------------------------------------------------------
void EditorState::Log(ConsoleEntry::Level level, const std::string& text) {
    ConsoleEntry entry;
    entry.level = level;
    entry.text = text;
    console.push_back(entry);
    if (console.size() > 500) console.erase(console.begin(), console.begin() + 100);
    if (level != ConsoleEntry::Level::Info) consoleOpen = true;
}

// --------------------------------------------------------------------------
// Content browser
// --------------------------------------------------------------------------
std::string EditorState::ResolveProjectPath(const std::string& relative) const {
    if (relative.empty()) return project.rootPath;
    if (relative.size() > 1 && relative[1] == ':') return relative;    // Windows absolute
    if (!relative.empty() && relative[0] == '/') return relative;      // POSIX absolute
    return fsx::PathJoin(project.rootPath, relative);
}

void EditorState::SetBrowserDirectory(const std::string& relative) {
    browserDirectory = relative;
    RefreshContentBrowser();
}

void EditorState::RefreshContentBrowser() {
    browserEntries.clear();
    if (!project.valid) return;
    std::string directory = ResolveProjectPath(browserDirectory);
    if (!fsx::DirectoryExists(directory)) return;
    for (const auto& entry : fsx::ListDirectory(directory)) {
        if (entry.name.empty() || entry.name[0] == '.') continue;
        ContentEntry content;
        content.name = entry.name;
        content.isDirectory = entry.isDirectory;
        content.relativePath = browserDirectory.empty() ? entry.name : browserDirectory + "/" + entry.name;
        browserEntries.push_back(content);
    }
}

// --------------------------------------------------------------------------
// Play mode
// --------------------------------------------------------------------------
void EditorState::StartPlay() {
    if (playing) return;
    playSceneBefore = scene;
    playing = true;
    playTime = 0.0f;
    playInput.Clear();

    std::string error;
    if (!lua.Init(error)) {
        LogError("Lua init failed: " + error);
        playing = false;
        return;
    }
    lua.ClearScripts();
    lua.SetLogger([this](const std::string& message) {
        Log(ConsoleEntry::Level::Script, message);
    });

    int loaded = 0;
    for (auto& object : scene.objects) {
        if (object.script.empty()) continue;
        std::string source;
        if (!fsx::ReadTextFile(ResolveProjectPath(object.script), source)) {
            LogWarning("Script not found: " + object.script);
            continue;
        }
        std::string scriptError;
        if (lua.AttachScript(scene, object, source, object.script, scriptError)) {
            ++loaded;
        } else {
            LogError(object.script + ": " + scriptError);
        }
    }
    lua.StartAll(scene);
    physicsWorld.Sync(scene);
    LogInfo("Play mode started (" + std::to_string(loaded) + " script(s))");
}

void EditorState::StopPlay() {
    if (!playing) return;
    playing = false;
    scene.objects = playSceneBefore.objects;
    lua.ClearScripts();
    lua.SetLogger(nullptr);
    sceneDirty = true;
    LogInfo("Play mode stopped");
}

void EditorState::BeginPlayHotReload() {}

// --------------------------------------------------------------------------
// Build
// --------------------------------------------------------------------------
void EditorState::StartBuild(const BuildOptions& options) {
    if (building) return;
    if (!project.valid) {
        LogError("No project open - create or open a project first.");
        return;
    }
    // Persist the scene before packaging.
    std::string error;
    if (!SaveScene(error)) {
        LogError("Cannot save scene before build: " + error);
        return;
    }
    building = true;
    buildFinished = false;
    cancelBuild = false;
    buildResult = BuildResult();
    LogInfo("Build started: " + project.package.title);

    Project projectCopy = project;
    BuildOptions optionsCopy = options;
    if (optionsCopy.runtimeSourceDir.empty()) {
        optionsCopy.runtimeSourceDir = fsx::PathJoin(fsx::ExecutableDir(), "PSPRuntime");
        if (!fsx::FileExists(fsx::PathJoin(optionsCopy.runtimeSourceDir, "Source/RuntimeApp.cpp"))) {
            optionsCopy.runtimeSourceDir = fsx::Absolute("PSPRuntime");
        }
    }
    projectCopy.rootPath = project.rootPath;

    std::thread([this, projectCopy, optionsCopy]() {
        BuildResult result = BuildGame(projectCopy, optionsCopy, &cancelBuild);
        buildResult = result;
        buildFinished = true;
        building = false;
    }).detach();
}

void EditorState::TickBuild() {
    if (!buildFinished) return;
    buildFinished = false;
    if (buildResult.success) {
        LogInfo("Build succeeded: " + buildResult.ebootPath);
        for (const auto& warning : buildResult.warnings) LogWarning(warning);
    } else {
        LogError("Build failed.");
        for (const auto& warning : buildResult.warnings) LogWarning(warning);
    }
    LogInfo(buildResult.log);
}

} // namespace pspeditor
