#pragma once

// ---------------------------------------------------------------------------
//  PSP Engine Editor - "Build EBOOT.PBP" pipeline
//
//  Steps executed by BuildGame():
//    1. validate build settings (title, title id, icon/pic/snd),
//    2. prepare <project>/build/pkg/ :
//           assets/, scenes/, scripts/, data/  (scene copied as data/scene.json)
//    3. validate / convert XMB artwork:
//           ICON0.PNG (144x80), PIC1.PNG (480x272)  -> downscaled+repacked if needed
//           SND0.AT3 (ATRAC3)                        -> 4-byte silent placeholder if missing
//    4. compile PSPRuntime with psp-gcc:
//           psp-gcc generated_assets.c -lpspgu -lpspgum -lpspctrl ... -> runtime.elf
//    5. mksfoex -s "TITLE" ... PARAM.SFO
//    6. pack-pbp EBOOT.PBP PARAM.SFO ICON0.PNG NULL NULL PIC1.PNG SND0.AT3 NULL runtime.elf
// ---------------------------------------------------------------------------

#include <atomic>
#include <string>
#include <vector>

#include "ProjectIO.h"

namespace pspeditor {

struct BuildOptions {
    std::string pspsdkPath;          // explicit PSPDEV override ("" == auto-detect)
    std::string runtimeSourceDir;    // absolute path to PSPRuntime/ sources
    bool skipRuntimeCompile = false; // reuse a previously compiled runtime.elf
    bool verbose = true;
};

struct BuildResult {
    bool success = false;
    int exitCode = -1;
    std::string log;
    std::string ebootPath;
    std::string packageDir;
    std::vector<std::string> warnings;
};

// Runs the full packaging pipeline synchronously (call from a worker std::thread;
// `cancel` is polled between steps).
BuildResult BuildGame(const Project& project, const BuildOptions& options,
                      std::atomic<bool>* cancel = nullptr);

// Utility used by Build Settings UI: validates a PNG against PSP XMB rules and,
// if needed, re-encodes it smaller. Returns true when the file is usable.
bool NormalizePngImage(const std::string& inputPath, const std::string& outputPath, int targetWidth,
                       int targetHeight, std::string& error);

// Reads width/height from a PNG header (no decoding required).
bool ReadPngSize(const std::string& path, int& width, int& height);

} // namespace pspeditor
