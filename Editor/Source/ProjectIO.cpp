// ---------------------------------------------------------------------------
//  PSP Engine Editor - filesystem, process & project helpers
// ---------------------------------------------------------------------------

#include "ProjectIO.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "Shared/Scene/SceneIO.h"
#include "Shared/Scene/SceneData.h"
#include "../Package/PackageInfo.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/wait.h>
#endif

namespace fs = std::filesystem;

namespace pspeditor {

namespace fsx {

std::string ExecutableDir() {
#if defined(_WIN32)
    std::array<wchar_t, 1024> buffer{};
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) return ".";
    return fs::path(std::wstring(buffer.data(), length)).parent_path().string();
#else
    std::error_code ec;
    fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (ec) return fs::current_path().string();
    return exe.parent_path().string();
#endif
}

std::string PathJoin(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return (fs::path(a) / b).string();
}

std::string Absolute(const std::string& path) {
    std::error_code ec;
    fs::path result = fs::absolute(path, ec);
    if (ec) return path;
    return result.lexically_normal().string();
}

std::string FileName(const std::string& path) { return fs::path(path).filename().string(); }

std::string ExtensionLower(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

std::string ParentDir(const std::string& path) { return fs::path(path).parent_path().string(); }

bool FileExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

bool DirectoryExists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec) && fs::is_directory(path, ec);
}

bool EnsureDirectory(const std::string& path) {
    std::error_code ec;
    fs::create_directories(path, ec);
    return !ec || DirectoryExists(path);
}

bool ReadTextFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    out = buffer.str();
    return true;
}

bool WriteTextFile(const std::string& path, const std::string& contents) {
    std::error_code ec;
    fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    return file.good();
}

bool ReadBinaryFile(const std::string& path, std::vector<unsigned char>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize size = file.tellg();
    if (size < 0) return false;
    out.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (size > 0) file.read(reinterpret_cast<char*>(out.data()), size);
    return file.good() || file.eof();
}

bool WriteBinaryFile(const std::string& path, const std::vector<unsigned char>& data) {
    std::error_code ec;
    fs::path parent = fs::path(path).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    if (!data.empty()) file.write(reinterpret_cast<const char*>(data.data()),
                                  static_cast<std::streamsize>(data.size()));
    return file.good();
}

bool CopyFile(const std::string& from, const std::string& to) {
    std::error_code ec;
    fs::path parent = fs::path(to).parent_path();
    if (!parent.empty()) fs::create_directories(parent, ec);
    fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

std::vector<DirEntry> ListDirectory(const std::string& path) {
    std::vector<DirEntry> entries;
    std::error_code ec;
    if (!fs::exists(path, ec)) return entries;
    for (const auto& item : fs::directory_iterator(path, ec)) {
        DirEntry entry;
        entry.path = item.path().string();
        entry.name = item.path().filename().string();
        entry.isDirectory = item.is_directory(ec);
        entries.push_back(entry);
    }
    std::sort(entries.begin(), entries.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory;
        return a.name < b.name;
    });
    return entries;
}

bool CopyDirectoryRecursive(const std::string& from, const std::string& to) {
    std::error_code ec;
    if (!fs::exists(from, ec)) return false;
    fs::create_directories(to, ec);
    fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    return !ec;
}

} // namespace fsx

// --------------------------------------------------------------------------
// Process helper
// --------------------------------------------------------------------------

int RunCommand(const std::string& commandLine, std::string& output) {
    output.clear();
#if defined(_WIN32)
    SECURITY_ATTRIBUTES attributes{};
    attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    attributes.bInheritHandle = TRUE;
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &attributes, 0)) return -1;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand(commandLine.begin(), commandLine.end());
    BOOL ok = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    if (!ok) {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return -1;
    }
    CloseHandle(writePipe);
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
        output.append(buffer, read);
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    CloseHandle(readPipe);
    return static_cast<int>(exitCode);
#else
    std::string full = commandLine + " 2>&1";
    FILE* pipe = popen(full.c_str(), "r");
    if (!pipe) return -1;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
    int status = pclose(pipe);
    if (status == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
#endif
}

std::string GetEnvVar(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : std::string();
}

std::string FindPspsdkBin(const std::string& explicitPspsdkPath, std::string& details) {
    details.clear();
    std::vector<std::string> candidates;
    if (!explicitPspsdkPath.empty()) candidates.push_back(explicitPspsdkPath);
    std::string env = GetEnvVar("PSPDEV");
    if (!env.empty()) candidates.push_back(env);
    candidates.push_back("/usr/local/pspdev");
    candidates.push_back("/opt/pspdev");

    for (const auto& root : candidates) {
        std::string bin = fsx::PathJoin(root, "bin");
        std::string gccName = "psp-gcc";
#if defined(_WIN32)
        gccName = "psp-gcc.exe";
#endif
        if (fsx::FileExists(fsx::PathJoin(bin, gccName))) {
            details = "Found PSPSDK: " + bin;
            return bin;
        }
    }
    details = "PSPSDK not found. Install it from https://github.com/pspdev/pspdev/releases "
              "and set PSPDEV (e.g. export PSPDEV=/usr/local/pspdev, PATH=$PSPDEV/bin:$PATH).";
    return std::string();
}

// --------------------------------------------------------------------------
// Project creation / loading
// --------------------------------------------------------------------------

namespace {

const char* kExampleScript = R"(-- spin.lua - example PSP Engine script
-- The owning object slowly spins and bobs above the ground.
local t = 0
local baseY = 0

function on_start()
    local x, y, z = position()
    baseY = y
    t = 0
end

function on_update(dt)
    t = t + dt
    local rx, ry, rz = rotation()
    set_rotation(rx, ry + dt * 45, rz)
    local x, y, z = position()
    set_position(x, baseY + math.sin(t * 2) * 0.35, z)

    -- Drive with the analog nub / d-pad when playing on the PSP.
    local ax = input.axis("X")
    if math.abs(ax) > 0.25 then
        set_position(x + ax * dt * 3, y, z)
    end
    if input.pressed("CROSS") then
        print("CROSS pressed on " .. name())
    end
end
)";

const char* kProjectMenuScript = R"(-- menu.lua - simple probe controlled by the d-pad / nub
function on_start()
    print("menu.lua started")
end

function on_update(dt)
    local x, y, z = position()
    local speed = 4 * dt
    if input.down("LEFT") or input.axis("X") < -0.3 then x = x - speed end
    if input.down("RIGHT") or input.axis("X") > 0.3 then x = x + speed end
    if input.down("UP") or input.axis("Y") > 0.3 then z = z - speed end
    if input.down("DOWN") or input.axis("Y") < -0.3 then z = z + speed end
    set_position(x * 0.98, y, z * 0.98)
end
)";

} // namespace

bool CreateProject(const std::string& rootDir, const std::string& displayName, Project& outProject,
                   std::string& error) {
    error.clear();
    Project project;
    project.rootPath = fsx::Absolute(rootDir);
    project.name = displayName;
    project.package.title = displayName;
    project.package.titleId = "PSPENG000";
    project.package.version = "1.00";

    if (!fsx::EnsureDirectory(project.rootPath) ||
        !fsx::EnsureDirectory(fsx::PathJoin(project.rootPath, "assets")) ||
        !fsx::EnsureDirectory(fsx::PathJoin(project.rootPath, "scenes")) ||
        !fsx::EnsureDirectory(fsx::PathJoin(project.rootPath, "scripts"))) {
        error = "Cannot create project directories under: " + project.rootPath;
        return false;
    }

    // project.json
    {
        std::string json = "{\n  \"name\": \"" + project.name + "\",\n  \"engine\": \"PSP Engine\",\n" +
                           "  \"version\": \"1.0.0\",\n  \"mainScene\": \"scenes/main.scene\"\n}\n";
        if (!fsx::WriteTextFile(fsx::PathJoin(project.rootPath, "project.json"), json)) {
            error = "Cannot write project.json";
            return false;
        }
    }

    // Default scene.
    pspengine::Scene scene = pspengine::MakeDefaultScene();
    std::string sceneError;
    if (!pspengine::SaveSceneToFile(scene, fsx::PathJoin(project.rootPath, "scenes/main.scene"),
                                    sceneError)) {
        error = "Cannot write default scene: " + sceneError;
        return false;
    }

    // Example scripts.
    fsx::WriteTextFile(fsx::PathJoin(project.rootPath, "scripts/spin.lua"), kExampleScript);
    fsx::WriteTextFile(fsx::PathJoin(project.rootPath, "scripts/menu.lua"), kProjectMenuScript);

    // Build settings (the example script is the default startup script).
    project.package.startupScript = "scripts/spin.lua";
    project.package.scene = "scenes/main.scene";
    project.package.icon0 = "assets/ICON0.PNG";
    project.package.pic1 = "assets/PIC1.PNG";
    fsx::WriteTextFile(fsx::PathJoin(project.rootPath, "build.json"), project.package.ToJson() + "\n");

    outProject = project;
    outProject.valid = true;
    return true;
}

bool LoadProject(const std::string& rootDir, Project& outProject, std::string& error) {
    error.clear();
    std::string projectJsonPath = fsx::PathJoin(rootDir, "project.json");
    std::string text;
    if (!fsx::ReadTextFile(projectJsonPath, text)) {
        error = "project.json not found in " + rootDir;
        return false;
    }
    Project project;
    project.rootPath = fsx::Absolute(rootDir);

    // Minimal JSON reading for the project file (name field only).
    auto extract = [&](const char* key) {
        std::string needle = std::string("\"") + key + "\"";
        size_t pos = text.find(needle);
        if (pos == std::string::npos) return std::string();
        pos = text.find(':', pos);
        if (pos == std::string::npos) return std::string();
        size_t start = text.find('"', pos);
        if (start == std::string::npos) return std::string();
        size_t end = text.find('"', start + 1);
        if (end == std::string::npos) return std::string();
        return text.substr(start + 1, end - start - 1);
    };
    project.name = extract("name");
    if (project.name.empty()) project.name = fsx::FileName(project.rootPath);

    std::string buildText;
    if (fsx::ReadTextFile(fsx::PathJoin(project.rootPath, "build.json"), buildText)) {
        project.package = pspengine::PackageInfo::FromJson(buildText);
    } else {
        project.package.title = project.name;
    }

    project.valid = true;
    outProject = project;
    return true;
}

bool SaveProject(const Project& project, std::string& error) {
    error.clear();
    if (!project.valid) {
        error = "no project loaded";
        return false;
    }
    std::string projectJson =
        "{\n  \"name\": \"" + project.name + "\",\n  \"engine\": \"PSP Engine\",\n" +
        "  \"version\": \"1.0.0\",\n  \"mainScene\": \"scenes/main.scene\"\n}\n";
    if (!fsx::WriteTextFile(fsx::PathJoin(project.rootPath, "project.json"), projectJson)) {
        error = "cannot write project.json";
        return false;
    }
    return fsx::WriteTextFile(fsx::PathJoin(project.rootPath, "build.json"),
                              project.package.ToJson() + "\n");
}

} // namespace pspeditor
