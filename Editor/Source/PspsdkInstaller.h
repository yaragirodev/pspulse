#pragma once

// ---------------------------------------------------------------------------
//  PSP Engine Editor - PSPSDK auto-installation
//
//  On the very first launch the editor looks for a working PSPSDK. When none is
//  found it offers to download the official prebuilt toolchain from GitHub
//  (pspdev/pspdev releases) and unpack it next to the editor:
//
//      <editor>/pspsdk/pspdev/bin/psp-gcc        (Linux / macOS)
//      <editor>\pspsdk\pspdev\bin\psp-gcc.exe    (Windows)
//
//  The toolchain is a 170-230 MB tarball, so the download runs on a worker
//  thread while the UI keeps rendering; progress is polled from ImGui.
// ---------------------------------------------------------------------------

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace pspeditor {

struct PlatformAsset {
    std::string url;          // direct download URL of the release tarball
    std::string archiveName;  // file name inside the download cache
};

// Resolves the release asset matching the current OS/arch.
// Returns false + a human readable reason when the platform is unsupported.
bool ResolvePspsdkAsset(PlatformAsset& asset, std::string& reason);

// Directory that holds the toolchain when the editor installs it itself.
std::string ManagedPspsdkRoot();

// Scans (in order): explicit path -> $PSPDEV -> managed install -> common
// system locations. Extends the editor's FindPspsdkBin with the managed root.
std::string LocatePspsdk(const std::string& explicitPath, std::string& details);

// Returns true when a usable toolchain lives at <root>/bin (psp-gcc + mksfoex
// + pack-pbp present). IsPspsdkBinDir() checks a bin directory directly.
bool IsPspsdkRoot(const std::string& root);
bool IsPspsdkBinDir(const std::string& path);

// Downloads and unpacks the toolchain. Blocking - call from a worker thread.
//   onProgress(fraction 0..1 or -1 when indeterminate, message)
struct InstallOptions {
    std::string targetRoot;   // where the toolchain ends up (ManagedPspsdkRoot())
    std::string assetUrl;     // from ResolvePspsdkAsset
    std::string archiveName;  // from ResolvePspsdkAsset
    std::string proxyHint;    // reserved; honours $https_proxy through curl/wget
};

struct InstallResult {
    bool success = false;
    std::string pspsdkBin;    // <root>/bin on success
    std::string log;
    std::string error;
};

InstallResult InstallPspsdk(const InstallOptions& options,
                            const std::function<void(float, const std::string&)>& onProgress,
                            std::atomic<bool>* cancel);

// Human readable hints printed to the Console when the toolchain is missing.
std::vector<std::string> PspsdkManualInstallHints();

} // namespace pspeditor
