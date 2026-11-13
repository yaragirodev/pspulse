// ---------------------------------------------------------------------------
//  PSP Engine Editor - PSPSDK auto-installation (implementation)
// ---------------------------------------------------------------------------

#include "PspsdkInstaller.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "ProjectIO.h"

namespace pspeditor {

namespace {

std::string TrimNewlines(const std::string& text) {
    std::string out = text;
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
    while (!out.empty() && (out.front() == '\n' || out.front() == '\r' || out.front() == ' ')) out.erase(out.begin());
    return out;
}

bool CommandExists(const std::string& name) {
#if defined(_WIN32)
    std::string output;
    return RunCommand("where " + name + " >nul 2>nul", output) == 0;
#else
    std::string output;
    return RunCommand("command -v " + name + " >/dev/null 2>&1", output) == 0;
#endif
}

// Picks the downloader available on this machine.
enum class Downloader { Curl, Wget, PowerShell, None };

Downloader PickDownloader() {
    if (CommandExists("curl")) return Downloader::Curl;
    if (CommandExists("wget")) return Downloader::Wget;
#if defined(_WIN32)
    if (CommandExists("powershell")) return Downloader::PowerShell;
#endif
    return Downloader::None;
}

std::string Quote(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
}

// Extracts `archive` into `destination` (created if missing).
bool ExtractArchive(const std::string& archive, const std::string& destination, std::string& log) {
    if (!fsx::EnsureDirectory(destination)) {
        log += "cannot create " + destination + "\n";
        return false;
    }
    std::string command;
    std::string output;
    int code = -1;
    const std::string ext = fsx::ExtensionLower(archive);
    if (ext == ".zip") {
        command = "unzip -q -o " + Quote(archive) + " -d " + Quote(destination);
        if (CommandExists("unzip")) {
            code = RunCommand(command, output);
#if defined(_WIN32)
        } else {
            code = RunCommand("powershell -NoProfile -Command \"Expand-Archive -Force -LiteralPath " +
                                  Quote(archive) + " -DestinationPath " + Quote(destination) + "\"",
                              output);
#endif
        }
    } else {
        command = "tar -xzf " + Quote(archive) + " -C " + Quote(destination);
        code = RunCommand(command, output);
    }
    if (!output.empty()) log += output;
    return code == 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Platform / asset resolution
// ---------------------------------------------------------------------------
bool ResolvePspsdkAsset(PlatformAsset& asset, std::string& reason) {
    asset = PlatformAsset{};
    reason.clear();

    // pspdev publishes rolling "latest" prebuilt tarballs per host OS.
    const std::string base = "https://github.com/pspdev/pspdev/releases/latest/download/";
#if defined(_WIN32)
    // pspdev does not publish native Windows builds: the recommended setup is
    // WSL2 or MSYS2. Point the user at the official instructions instead of
    // pretending to install something that will not run.
    reason =
        "The official PSPSDK has no native Windows build. Install it inside WSL2 "
        "(https://github.com/pspdev/pspdev#windows-wsl) or MSYS2 "
        "(pacman -S mingw-w64-x86_64-pspdev), then point PSPDEV at it in Build Settings.";
    return false;
#elif defined(__APPLE__)
    #if defined(__aarch64__) || defined(__arm64__)
        asset.archiveName = "pspdev-macos-latest-arm64.tar.gz";
    #else
        asset.archiveName = "pspdev-macos-15-intel-x86_64.tar.gz";
    #endif
#else
    // Linux: pick the build whose glibc is old enough for this host. The
    // debian build is the most portable one and works on Arch, Ubuntu, Fedora
    // etc. when glibc >= 2.28.
    asset.archiveName = "pspdev-debian-latest.tar.gz";
    std::string output;
    if (RunCommand("ldd --version 2>&1 | head -n1", output) == 0) {
        // Nothing to parse strictly: the debian asset is the safe default.
    }
#endif
    asset.url = base + asset.archiveName;
    return true;
}

std::string ManagedPspsdkRoot() {
    return fsx::PathJoin(fsx::PathJoin(fsx::ExecutableDir(), "pspsdk"), "pspdev");
}

bool IsPspsdkBinDir(const std::string& path) {
    if (path.empty()) return false;
#if defined(_WIN32)
    const std::string suffix = ".exe";
#else
    const std::string suffix;
#endif
    return fsx::FileExists(fsx::PathJoin(path, "psp-gcc" + suffix)) &&
           fsx::FileExists(fsx::PathJoin(path, "mksfoex" + suffix)) &&
           fsx::FileExists(fsx::PathJoin(path, "pack-pbp" + suffix));
}

bool IsPspsdkRoot(const std::string& root) {
    if (root.empty()) return false;
    const std::string bin = IsPspsdkBinDir(root) ? root : fsx::PathJoin(root, "bin");
#if defined(_WIN32)
    const std::string suffix = ".exe";
#else
    const std::string suffix;
#endif
    return fsx::FileExists(fsx::PathJoin(bin, "psp-gcc" + suffix)) &&
           fsx::FileExists(fsx::PathJoin(bin, "mksfoex" + suffix)) &&
           fsx::FileExists(fsx::PathJoin(bin, "pack-pbp" + suffix));
}

namespace {

// Accepts both a PSPDEV root (/opt/pspdev) and a bin directory (/opt/pspdev/bin);
// the editor stores the bin directory in its settings, users usually paste the root.
std::string NormalizeSdkRoot(const std::string& path) {
    if (path.empty()) return path;
    if (IsPspsdkRoot(path)) return path;
    if (fsx::FileName(path) == "bin") {
        const std::string parent = fsx::ParentDir(path);
        if (IsPspsdkRoot(parent)) return parent;
    }
    return path;
}

} // namespace

std::string LocatePspsdk(const std::string& explicitPath, std::string& details) {
    details.clear();
    std::vector<std::string> candidates;
    if (!explicitPath.empty()) candidates.push_back(NormalizeSdkRoot(explicitPath));

    // A managed install lives under <editor>/pspsdk/pspdev and is checked before
    // $PSPDEV so that a stale environment variable cannot shadow it.
    const std::string managed = ManagedPspsdkRoot();
    if (IsPspsdkRoot(managed)) candidates.push_back(managed);

    const std::string env = GetEnvVar("PSPDEV");
    if (!env.empty()) candidates.push_back(env);

    candidates.push_back("/usr/local/pspdev");
    candidates.push_back("/opt/pspdev");
#if defined(_WIN32)
    candidates.push_back("C:/pspdev");
#endif

    for (const auto& root : candidates) {
        if (IsPspsdkRoot(root)) {
            // The caller always receives a bin directory (never bin/bin).
            const std::string bin = IsPspsdkBinDir(root) ? root : fsx::PathJoin(root, "bin");
            details = "Found PSPSDK: " + bin;
            return bin;
        }
    }
    details = "PSPSDK not found (looked in $PSPDEV, /usr/local/pspdev, /opt/pspdev and " + managed +
              "). Install it from Tools -> Install PSPSDK, or point Build Settings at an existing one.";
    return std::string();
}

std::vector<std::string> PspsdkManualInstallHints() {
    return {
        "Official prebuilt toolchain: https://github.com/pspdev/pspdev/releases (pspdev-debian-latest.tar.gz)",
        "Linux:  tar -xzf pspdev-debian-latest.tar.gz -C /opt && export PSPDEV=/opt/pspdev && export PATH=$PSPDEV/bin:$PATH",
        "Windows (WSL2): https://github.com/pspdev/pspdev#windows-wsl",
        "Windows (MSYS2): pacman -S mingw-w64-x86_64-pspdev",
        "macOS:  tar -xzf pspdev-macos-*.tar.gz -C /usr/local && export PSPDEV=/usr/local/pspdev",
        "After installing, restart the editor or set the path in Tools -> Build Settings.",
    };
}

// ---------------------------------------------------------------------------
// Installation
// ---------------------------------------------------------------------------
InstallResult InstallPspsdk(const InstallOptions& options,
                            const std::function<void(float, const std::string&)>& onProgress,
                            std::atomic<bool>* cancel) {
    InstallResult result;
    auto report = [&](float fraction, const std::string& message) {
        if (onProgress) onProgress(fraction, message);
    };
    auto aborted = [&]() { return cancel && cancel->load(); };

    if (options.assetUrl.empty() || options.archiveName.empty()) {
        result.error = "No PSPSDK download is available for this platform.";
        return result;
    }

    const Downloader downloader = PickDownloader();
    if (downloader == Downloader::None) {
        result.error =
            "Neither curl nor wget is available - install one of them, then retry "
            "(curl is packaged as 'curl' on every distro).";
        return result;
    }

    // Keep the 174 MB tarball outside the binary tree so rebuilds do not copy it.
    const std::string cacheDir = fsx::PathJoin(fsx::PathJoin(fsx::ExecutableDir(), "pspsdk"), "cache");
    if (!fsx::EnsureDirectory(cacheDir)) {
        result.error = "Cannot create download directory: " + cacheDir;
        return result;
    }
    const std::string archivePath = fsx::PathJoin(cacheDir, options.archiveName);

    bool downloaded = fsx::FileExists(archivePath);
    if (downloaded) {
        result.log += "[INFO] Using cached archive " + archivePath + "\n";
        report(0.35f, "Using cached PSPSDK archive");
    } else {
        report(-1.0f, "Downloading PSPSDK (~175 MB, this can take a few minutes)");
        std::string command;
        std::string output;
        int code = -1;
#if defined(_WIN32)
        if (downloader == Downloader::Curl) {
            command = "curl -L --fail -o " + Quote(archivePath) + " " + Quote(options.assetUrl);
            code = RunCommand(command, output);
        } else {
            std::string ps = "powershell -NoProfile -Command \"Invoke-WebRequest -Uri '" + options.assetUrl +
                             "' -OutFile '" + archivePath + "'\"";
            code = RunCommand(ps, output);
        }
#else
        if (downloader == Downloader::Curl) {
            command = "curl -L --fail --retry 3 --connect-timeout 30 -o " + Quote(archivePath) + " " +
                      Quote(options.assetUrl);
            code = RunCommand(command, output);
        } else {
            command = "wget -q --show-progress --timeout=30 -O " + Quote(archivePath) + " " +
                      Quote(options.assetUrl);
            code = RunCommand(command, output);
        }
#endif
        if (!output.empty()) result.log += output;
        if (code != 0 || !fsx::FileExists(archivePath)) {
            result.error = "Download failed (exit " + std::to_string(code) +
                           "). Check your internet connection / proxy, or install PSPSDK manually.";
            if (fsx::FileExists(archivePath)) {
                std::remove(archivePath.c_str()); // never keep a half-written archive
            }
            return result;
        }
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "[INFO] Downloaded %s\n", options.archiveName.c_str());
        result.log += buffer;
    }
    if (aborted()) {
        result.error = "Installation cancelled.";
        return result;
    }

    report(-1.0f, "Unpacking toolchain");
    const std::string targetRoot = options.targetRoot.empty() ? ManagedPspsdkRoot() : options.targetRoot;
    const std::string parentDir = fsx::ParentDir(targetRoot);
    if (!ExtractArchive(archivePath, parentDir, result.log)) {
        result.error = "Cannot unpack " + archivePath + " into " + parentDir;
        return result;
    }

    // The archives always contain a top-level "pspdev/" directory.
    const std::string unpacked = fsx::PathJoin(parentDir, "pspdev");
    if (!fsx::DirectoryExists(unpacked)) {
        result.error = "Archive layout is unexpected: " + unpacked + " is missing.";
        return result;
    }
    if (unpacked != targetRoot && !fsx::DirectoryExists(targetRoot)) {
        result.error = "Unpacked to " + unpacked + " but expected " + targetRoot;
        return result;
    }

    if (!IsPspsdkRoot(targetRoot)) {
        result.error = "Toolchain unpacked but psp-gcc / mksfoex / pack-pbp are missing in " +
                       fsx::PathJoin(targetRoot, "bin");
        return result;
    }

    // Smoke-test the freshly installed compiler.
    report(0.9f, "Verifying the toolchain");
    std::string output;
    const int versionCode =
        RunCommand(Quote(fsx::PathJoin(fsx::PathJoin(targetRoot, "bin"), "psp-gcc")) + " --version", output);
    if (versionCode != 0) {
        result.error = "psp-gcc cannot be executed: " + TrimNewlines(output) +
                       " (missing 32-bit runtime libraries? try 'pacman -S lib32-glibc' / 'apt install libc6:i386').";
        return result;
    }
    result.log += "[INFO] " + TrimNewlines(output.substr(0, output.find('\n'))) + "\n";

    result.success = true;
    result.pspsdkBin = fsx::PathJoin(targetRoot, "bin");
    report(1.0f, "PSPSDK installed");
    return result;
}

} // namespace pspeditor
