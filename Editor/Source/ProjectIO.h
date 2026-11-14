#pragma once

// ---------------------------------------------------------------------------
//  PSPulse Editor - filesystem, process & project helpers
//  (std::filesystem based, works on Windows and Linux)
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

#include "../Package/PackageInfo.h"

namespace pspeditor {

struct Project {
    std::string rootPath;   // absolute project directory
    std::string name = "Untitled Project";
    pspulse::PackageInfo package;
    bool valid = false;
};

namespace fsx {

std::string ExecutableDir();
std::string PathJoin(const std::string& a, const std::string& b);
std::string Absolute(const std::string& path);
std::string FileName(const std::string& path);
std::string ExtensionLower(const std::string& path);
std::string ParentDir(const std::string& path);
bool FileExists(const std::string& path);
bool DirectoryExists(const std::string& path);
bool EnsureDirectory(const std::string& path);
bool ReadTextFile(const std::string& path, std::string& out);
bool WriteTextFile(const std::string& path, const std::string& contents);
bool ReadBinaryFile(const std::string& path, std::vector<unsigned char>& out);
bool WriteBinaryFile(const std::string& path, const std::vector<unsigned char>& data);
bool CopyFileTo(const std::string& from, const std::string& to);

struct DirEntry {
    std::string name;
    std::string path;
    bool isDirectory = false;
};

std::vector<DirEntry> ListDirectory(const std::string& path);
// Recursively copies `from` -> `to` (creates directories as needed).
bool CopyDirectoryRecursive(const std::string& from, const std::string& to);

} // namespace fsx

// Creates a brand-new project on disk (assets/, scenes/, scripts/, project.json,
// build.json, an example scene and an example Lua script).
bool CreateProject(const std::string& rootDir, const std::string& displayName, Project& outProject,
                   std::string& error);

// Loads an existing project (reads project.json and build.json).
bool LoadProject(const std::string& rootDir, Project& outProject, std::string& error);

// Persists the project metadata back to project.json.
bool SaveProject(const Project& project, std::string& error);

// Runs a shell command, capturing stdout+stderr into `output`.
// Returns the process exit code (-1 when the process could not be started).
int RunCommand(const std::string& commandLine, std::string& output);

// Locates psp-gcc / mksfo / pack-pbp. Returns the bin directory or "".
std::string FindPspsdkBin(const std::string& explicitPspsdkPath, std::string& details);
std::string GetEnvVar(const std::string& name);

} // namespace pspeditor
