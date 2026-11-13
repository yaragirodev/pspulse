// ---------------------------------------------------------------------------
//  PSP Engine Editor - "Build EBOOT.PBP" pipeline
// ---------------------------------------------------------------------------

#include "BuildPipeline.h"

#include "PspsdkInstaller.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <vector>

#include <zlib.h>

#include <glm/glm.hpp>

#include "Shared/Scene/SceneData.h"
#include "Shared/Scene/SceneIO.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

namespace pspeditor {

namespace {

// --------------------------------------------------------------------------
// Minimal PNG decoder (8-bit RGB/RGBA, no interlace) - just enough to inspect
// and downscale ICON0 / PIC1 artwork. Uses zlib's uncompress().
// --------------------------------------------------------------------------
struct DecodedImage {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

unsigned int ReadBE32(const unsigned char* p) {
    return (static_cast<unsigned int>(p[0]) << 24) | (static_cast<unsigned int>(p[1]) << 16) |
           (static_cast<unsigned int>(p[2]) << 8) | static_cast<unsigned int>(p[3]);
}

bool DecodePng(const std::vector<unsigned char>& data, DecodedImage& out, std::string& error) {
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (data.size() < 8 || std::memcmp(data.data(), signature, 8) != 0) {
        error = "not a PNG file";
        return false;
    }
    size_t pos = 8;
    int width = 0, height = 0;
    int bitDepth = 0, colorType = 0, interlace = 0;
    std::vector<unsigned char> idat;

    while (pos + 8 <= data.size()) {
        unsigned int length = ReadBE32(data.data() + pos);
        std::string type(reinterpret_cast<const char*>(data.data() + pos + 4), 4);
        pos += 8;
        if (pos + length > data.size()) break;
        const unsigned char* chunk = data.data() + pos;
        if (type == "IHDR" && length >= 13) {
            width = static_cast<int>(ReadBE32(chunk));
            height = static_cast<int>(ReadBE32(chunk + 4));
            bitDepth = chunk[8];
            colorType = chunk[9];
            interlace = chunk[12];
        } else if (type == "IDAT") {
            idat.insert(idat.end(), chunk, chunk + length);
        } else if (type == "IEND") {
            break;
        }
        pos += length + 4; // skip CRC
    }

    if (width <= 0 || height <= 0) {
        error = "missing IHDR";
        return false;
    }
    if (bitDepth != 8 || interlace != 0 || (colorType != 2 && colorType != 6)) {
        error = "only 8-bit non-interlaced RGB/RGBA PNGs are supported";
        return false;
    }
    int channels = (colorType == 6) ? 4 : 3;
    size_t stride = static_cast<size_t>(width) * channels;
    size_t rawSize = (stride + 1) * static_cast<size_t>(height);

    std::vector<unsigned char> raw(rawSize);
    if (idat.empty() || uncompress(raw.data(), reinterpret_cast<unsigned long*>(&rawSize), idat.data(),
                                   idat.size()) != 0) {
        error = "zlib decompression failed";
        return false;
    }

    std::vector<unsigned char> pixels(stride * static_cast<size_t>(height));
    const unsigned char* prev = nullptr;
    for (int y = 0; y < height; ++y) {
        const unsigned char* src = raw.data() + (stride + 1) * static_cast<size_t>(y);
        unsigned char filter = *src++;
        unsigned char* dst = pixels.data() + stride * static_cast<size_t>(y);
        for (size_t x = 0; x < stride; ++x) {
            int rawByte = src[x];
            int a = 0, b = 0, c = 0;
            if (x >= static_cast<size_t>(channels)) a = dst[x - channels];
            if (prev) b = prev[x];
            if (prev && x >= static_cast<size_t>(channels)) c = prev[x - channels];
            int value = rawByte;
            switch (filter) {
                case 0: break;
                case 1: value += a; break;
                case 2: value += b; break;
                case 3: value += (a + b) / 2; break;
                case 4: {
                    int p = a + b - c;
                    int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
                    value += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                    break;
                }
                default:
                    error = "bad PNG filter";
                    return false;
            }
            dst[x] = static_cast<unsigned char>(value & 0xFF);
        }
        prev = dst;
    }

    out.width = width;
    out.height = height;
    bool forceAlpha = false;
    for (size_t i = 0; i < pixels.size() && !forceAlpha; i += static_cast<size_t>(channels)) {
        if (channels == 4 && pixels[i + 3] != 255) forceAlpha = true;
    }
    int outChannels = forceAlpha ? 4 : 3;
    out.rgba.resize(static_cast<size_t>(width) * height * outChannels);
    for (int y = 0; y < height; ++y) {
        const unsigned char* src = pixels.data() + stride * static_cast<size_t>(y);
        unsigned char* dst = out.rgba.data() + static_cast<size_t>(width) * outChannels * y;
        for (int x = 0; x < width; ++x) {
            dst[x * outChannels + 0] = src[x * channels + 0];
            dst[x * outChannels + 1] = src[x * channels + 1];
            dst[x * outChannels + 2] = src[x * channels + 2];
            if (outChannels == 4) dst[x * outChannels + 3] = (channels == 4) ? src[x * channels + 3] : 255;
        }
    }
    return true;
}

// Bilinear downscale with alpha handling.
void ResampleImage(const DecodedImage& src, int targetW, int targetH, int channels,
                   std::vector<unsigned char>& out) {
    out.resize(static_cast<size_t>(targetW) * targetH * channels);
    for (int y = 0; y < targetH; ++y) {
        for (int x = 0; x < targetW; ++x) {
            float sx = (x + 0.5f) * src.width / targetW - 0.5f;
            float sy = (y + 0.5f) * src.height / targetH - 0.5f;
            int x0 = glm::clamp(static_cast<int>(std::floor(sx)), 0, src.width - 1);
            int y0 = glm::clamp(static_cast<int>(std::floor(sy)), 0, src.height - 1);
            int x1 = glm::clamp(x0 + 1, 0, src.width - 1);
            int y1 = glm::clamp(y0 + 1, 0, src.height - 1);
            float fx = glm::clamp(sx - x0, 0.0f, 1.0f);
            float fy = glm::clamp(sy - y0, 0.0f, 1.0f);
            int srcChannels = static_cast<int>(src.rgba.size()) / (src.width * src.height);
            for (int c = 0; c < channels; ++c) {
                auto sample = [&](int px, int py) {
                    int sc = (c == 3) ? 3 : c;
                    if (sc >= srcChannels) return 255.0f;
                    return static_cast<float>(src.rgba[(static_cast<size_t>(py) * src.width + px) * srcChannels + sc]);
                };
                float v00 = sample(x0, y0), v10 = sample(x1, y0), v01 = sample(x0, y1), v11 = sample(x1, y1);
                float v = (v00 * (1 - fx) + v10 * fx) * (1 - fy) + (v01 * (1 - fx) + v11 * fx) * fy;
                out[(static_cast<size_t>(y) * targetW + x) * channels + c] =
                    static_cast<unsigned char>(glm::clamp(v, 0.0f, 255.0f));
            }
        }
    }
}

// --------------------------------------------------------------------------
// SFO / PBP helpers
// --------------------------------------------------------------------------
std::string QuoteForShell(const std::string& text) {
    std::string out = "\"";
    for (char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    out += "\"";
    return out;
}

std::string PathQuote(const std::string& path) { return QuoteForShell(path); }

void AppendLog(std::string& log, const std::string& line) { log += line + "\n"; }

// Builds the C asset table consumed by PSPRuntime (project data is embedded in
// the ELF so the EBOOT works without extra files).
bool GenerateAssetSource(const Project& project, const std::string& outputPath,
                         const std::string& startupScript, std::string& error) {
    std::string sceneText;
    std::string scenePath = fsx::PathJoin(project.rootPath, project.package.scene);
    if (!fsx::ReadTextFile(scenePath, sceneText)) {
        error = "cannot read scene: " + scenePath;
        return false;
    }

    std::ostringstream cpp;
    cpp << "/* Auto-generated by PSP Engine Editor - do not edit. */\n";
    cpp << "#include \"AssetData.h\"\n\n";
    cpp << "static const unsigned char kSceneData[] = {";
    for (size_t i = 0; i < sceneText.size(); ++i) {
        if (i % 16 == 0) cpp << "\n    ";
        cpp << static_cast<int>(static_cast<unsigned char>(sceneText[i])) << ",";
    }
    cpp << "0};\n\n";
    std::string startup = startupScript;
    std::string startupText;
    if (!startup.empty()) {
        fsx::ReadTextFile(fsx::PathJoin(project.rootPath, startup), startupText);
    }
    // Bundle every Lua file of the project so scripts referenced by objects can
    // be resolved from the EBOOT without external files.
    std::string scriptTable;
    {
        std::vector<fsx::DirEntry> scripts = fsx::ListDirectory(fsx::PathJoin(project.rootPath, "scripts"));
        cpp << "/* PspScriptEntry comes from AssetData.h. */\n";
        int index = 0;
        for (const auto& entry : scripts) {
            if (entry.isDirectory || fsx::ExtensionLower(entry.name) != ".lua") continue;
            std::string text;
            if (!fsx::ReadTextFile(entry.path, text)) continue;
            cpp << "static const unsigned char kScript" << index << "[] = {";
            for (size_t i = 0; i < text.size(); ++i) {
                if (i % 16 == 0) cpp << "\n    ";
                cpp << static_cast<int>(static_cast<unsigned char>(text[i])) << ",";
            }
            cpp << "0};\n";
            scriptTable += "    {\"scripts/" + entry.name + "\", kScript" + std::to_string(index) +
                           ", " + std::to_string(text.size()) + "},\n";
            ++index;
        }
        cpp << "static const PspScriptEntry kScriptTable[] = {\n" << scriptTable;
        cpp << "    {0, 0, 0}\n};\n\n";
        cpp << "const PspScriptEntry* gPspScriptTable = kScriptTable;\n";
        cpp << "const unsigned int gPspScriptCount = " << index << ";\n\n";
    }

    // Embed every glTF/GLB model under assets/ so the runtime can draw meshes
    // without shipping loose files.
    {
        struct MeshFile { std::string path; std::vector<unsigned char> bytes; };
        std::vector<MeshFile> meshes;
        std::function<void(const std::string&, const std::string&)> scan = [&](const std::string& dir,
                                                                              const std::string& prefix) {
            for (const auto& entry : fsx::ListDirectory(dir)) {
                if (entry.isDirectory) {
                    scan(entry.path, prefix + entry.name + "/");
                    continue;
                }
                const std::string ext = fsx::ExtensionLower(entry.name);
                if (ext != ".gltf" && ext != ".glb") continue;
                MeshFile file;
                file.path = prefix + entry.name;
                if (fsx::ReadBinaryFile(entry.path, file.bytes) && !file.bytes.empty()) {
                    meshes.push_back(std::move(file));
                }
            }
        };
        const std::string assetsDir = fsx::PathJoin(project.rootPath, "assets");
        if (fsx::DirectoryExists(assetsDir)) scan(assetsDir, "assets/");

        cpp << "typedef struct { const char* path; const unsigned char* data; unsigned int size; } PspMeshEntryLocal;\n";
        for (size_t i = 0; i < meshes.size(); ++i) {
            cpp << "static const unsigned char kMesh" << i << "[] = {";
            for (size_t b = 0; b < meshes[i].bytes.size(); ++b) {
                if (b % 16 == 0) cpp << "\n    ";
                cpp << static_cast<int>(meshes[i].bytes[b]) << ",";
            }
            cpp << "0};\n";
        }
        cpp << "static const PspMeshEntryLocal kMeshTable[] = {\n";
        for (size_t i = 0; i < meshes.size(); ++i) {
            cpp << "    {\"" << meshes[i].path << "\", kMesh" << i << ", "
                << meshes[i].bytes.size() << "},\n";
        }
        cpp << "    {0, 0, 0}\n};\n";
        cpp << "const PspMeshEntry* gPspMeshTable = reinterpret_cast<const PspMeshEntry*>(kMeshTable);\n";
        cpp << "const unsigned int gPspMeshCount = " << meshes.size() << ";\n\n";
    }

    cpp << "static const unsigned char kStartupScriptData[] = {";
    for (size_t i = 0; i < startupText.size(); ++i) {
        if (i % 16 == 0) cpp << "\n    ";
        cpp << static_cast<int>(static_cast<unsigned char>(startupText[i])) << ",";
    }
    cpp << "0};\n\n";

    cpp << "const PspAssetBundle gPspAssetBundle = {\n";
    cpp << "    kSceneData, sizeof(kSceneData),\n";
    cpp << "    kStartupScriptData, " << startupText.size() << ",\n";
    cpp << "    " << QuoteForShell(project.package.title) << ",\n";
    cpp << "    " << QuoteForShell(startup) << "\n";
    cpp << "};\n";

    if (!fsx::WriteTextFile(outputPath, cpp.str())) {
        error = "cannot write generated asset source: " + outputPath;
        return false;
    }
    return true;
}

// Dark PSP-style placeholder icon (144x80) / background (480x272).
void WritePlaceholderPng(const std::string& path, int width, int height, bool icon) {
    std::vector<unsigned char> rgb(static_cast<size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float v = static_cast<float>(y) / height;
            unsigned char r = static_cast<unsigned char>(20 + v * 25);
            unsigned char g = static_cast<unsigned char>(32 + v * 40);
            unsigned char b = static_cast<unsigned char>(58 + v * 60);
            if (icon && x > width / 5 && x < width - width / 5 && y > height / 3 &&
                y < height - height / 3) {
                r = static_cast<unsigned char>(70);
                g = static_cast<unsigned char>(150);
                b = static_cast<unsigned char>(230);
            }
            size_t index = (static_cast<size_t>(y) * width + x) * 3;
            rgb[index + 0] = r;
            rgb[index + 1] = g;
            rgb[index + 2] = b;
        }
    }
    stbi_write_png(path.c_str(), width, height, 3, rgb.data(), width * 3);
}

} // namespace

// --------------------------------------------------------------------------
// Public helpers
// --------------------------------------------------------------------------

bool ReadPngSize(const std::string& path, int& width, int& height) {
    std::vector<unsigned char> data;
    if (!fsx::ReadBinaryFile(path, data)) return false;
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (data.size() < 24 || std::memcmp(data.data(), signature, 8) != 0) return false;
    width = static_cast<int>(ReadBE32(data.data() + 16));
    height = static_cast<int>(ReadBE32(data.data() + 20));
    return width > 0 && height > 0;
}

bool NormalizePngImage(const std::string& inputPath, const std::string& outputPath, int targetWidth,
                       int targetHeight, std::string& error) {
    error.clear();
    std::vector<unsigned char> data;
    if (!fsx::ReadBinaryFile(inputPath, data)) {
        error = "cannot read " + inputPath;
        return false;
    }
    DecodedImage image;
    if (!DecodePng(data, image, error)) {
        error = inputPath + ": " + error;
        return false;
    }

    int channels = static_cast<int>(image.rgba.size()) / (image.width * image.height);

    // Upscale-free fitting: scale down preserving aspect ratio, then pad.
    float scale = std::min(static_cast<float>(targetWidth) / image.width,
                           static_cast<float>(targetHeight) / image.height);
    int fitW = std::max(1, static_cast<int>(image.width * scale));
    int fitH = std::max(1, static_cast<int>(image.height * scale));

    std::vector<unsigned char> resized;
    ResampleImage(image, fitW, fitH, channels, resized);

    std::vector<unsigned char> canvas(static_cast<size_t>(targetWidth) * targetHeight * channels, 0);
    int offsetX = (targetWidth - fitW) / 2;
    int offsetY = (targetHeight - fitH) / 2;
    for (int y = 0; y < fitH; ++y) {
        std::memcpy(canvas.data() + ((static_cast<size_t>(y + offsetY) * targetWidth + offsetX) * channels),
                    resized.data() + static_cast<size_t>(y) * fitW * channels,
                    static_cast<size_t>(fitW) * channels);
    }

    if (!stbi_write_png(outputPath.c_str(), targetWidth, targetHeight, channels, canvas.data(),
                        targetWidth * channels)) {
        error = "cannot write " + outputPath;
        return false;
    }
    return true;
}

// --------------------------------------------------------------------------
// Build
// --------------------------------------------------------------------------

BuildResult BuildGame(const Project& project, const BuildOptions& options, std::atomic<bool>* cancel) {
    BuildResult result;
    auto aborted = [&]() { return cancel && cancel->load(); };
    auto fail = [&](const std::string& message) {
        AppendLog(result.log, "[ERROR] " + message);
        result.success = false;
        return result;
    };

    if (!project.valid) return fail("No project is open.");
    if (aborted()) return fail("Build cancelled.");

    const pspengine::PackageInfo& info = project.package;
    if (info.title.empty()) result.warnings.push_back("TITLE is empty - using 'PSP Engine Game'.");
    std::string titleId = info.titleId.empty() ? "PSPENG000" : info.titleId;
    if (titleId.size() < 4 || titleId.size() > 9) {
        return fail("TITLE_ID must be 4-9 characters (got '" + titleId + "').");
    }
    for (char c : titleId) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return fail("TITLE_ID may only contain A-Z, 0-9, '-' and '_'.");
        }
    }

    std::string sdkDetails;
    std::string sdkBin = LocatePspsdk(options.pspsdkPath, sdkDetails);
    AppendLog(result.log, "[INFO] " + sdkDetails);
    if (sdkBin.empty()) {
        return fail("PSPSDK tools (psp-gcc / mksfoex / pack-pbp) were not found. " + result.log);
    }
    AppendLog(result.log, "[INFO] Using PSPSDK at " + sdkBin);
    if (aborted()) return fail("Build cancelled.");

    // Relative output directories are resolved against the project root (never
    // against the editor's working directory), so the build lands inside the
    // project no matter where the editor was launched from.
    std::string outputRel = info.outputDir.empty() ? std::string("build/pkg") : info.outputDir;
    const bool outputIsAbsolute = (!outputRel.empty() && (outputRel[0] == '/' || outputRel[0] == '\\')) ||
                                  (outputRel.size() > 1 && outputRel[1] == ':');
    if (!outputIsAbsolute) outputRel = fsx::PathJoin(project.rootPath, outputRel);
    const std::string packageDir = fsx::Absolute(outputRel);
    result.packageDir = packageDir;
    std::string ebootPath = fsx::PathJoin(packageDir, "EBOOT.PBP");
    result.ebootPath = ebootPath;

    if (!fsx::EnsureDirectory(packageDir) || !fsx::EnsureDirectory(fsx::PathJoin(packageDir, "assets")) ||
        !fsx::EnsureDirectory(fsx::PathJoin(packageDir, "scenes")) ||
        !fsx::EnsureDirectory(fsx::PathJoin(packageDir, "scripts"))) {
        return fail("Cannot create package directory: " + packageDir);
    }

    // ---- 1. data: scene + scripts ---------------------------------------
    std::string sceneText;
    std::string sceneSource = fsx::PathJoin(project.rootPath, info.scene);
    if (!fsx::ReadTextFile(sceneSource, sceneText)) {
        return fail("Scene file not found: " + sceneSource);
    }
    pspengine::Scene scene;
    std::string sceneError;
    if (!pspengine::SceneFromJson(sceneText, scene, sceneError)) {
        return fail("Scene is invalid: " + sceneError);
    }
    fsx::WriteTextFile(fsx::PathJoin(packageDir, "scenes/" + fsx::FileName(info.scene)), sceneText);
    if (fsx::DirectoryExists(fsx::PathJoin(project.rootPath, "scripts"))) {
        fsx::CopyDirectoryRecursive(fsx::PathJoin(project.rootPath, "scripts"),
                                    fsx::PathJoin(packageDir, "scripts"));
    }
    AppendLog(result.log, "[INFO] Copied scene (" + std::to_string(scene.objects.size()) +
                              " objects) and scripts into the package.");
    if (aborted()) return fail("Build cancelled.");

    // ---- 2. XMB artwork ---------------------------------------------------
    auto resolveAsset = [&](const std::string& relative, std::string& outPath) -> bool {
        outPath.clear();
        if (relative.empty()) return false;
        std::string candidate = fsx::PathJoin(project.rootPath, relative);
        if (fsx::FileExists(candidate)) {
            outPath = candidate;
            return true;
        }
        candidate = fsx::PathJoin(project.rootPath, "assets/" + fsx::FileName(relative));
        if (fsx::FileExists(candidate)) {
            outPath = candidate;
            return true;
        }
        return false;
    };

    std::string iconPath = fsx::PathJoin(packageDir, "ICON0.PNG");
    std::string picPath = fsx::PathJoin(packageDir, "PIC1.PNG");
    std::string sndPath = fsx::PathJoin(packageDir, "SND0.AT3");

    std::string userIcon;
    if (resolveAsset(info.icon0, userIcon)) {
        std::string pngError;
        if (NormalizePngImage(userIcon, iconPath, 144, 80, pngError)) {
            AppendLog(result.log, "[INFO] ICON0.PNG generated from " + userIcon);
        } else {
            result.warnings.push_back("ICON0 conversion failed (" + pngError + "), placeholder used.");
            WritePlaceholderPng(iconPath, 144, 80, true);
        }
    } else {
        result.warnings.push_back("ICON0.PNG not provided - generated a placeholder.");
        WritePlaceholderPng(iconPath, 144, 80, true);
    }

    std::string userPic;
    if (resolveAsset(info.pic1, userPic)) {
        std::string pngError;
        if (NormalizePngImage(userPic, picPath, 480, 272, pngError)) {
            AppendLog(result.log, "[INFO] PIC1.PNG generated from " + userPic);
        } else {
            result.warnings.push_back("PIC1 conversion failed (" + pngError + "), placeholder used.");
            WritePlaceholderPng(picPath, 480, 272, false);
        }
    } else {
        result.warnings.push_back("PIC1.PNG not provided - generated a placeholder.");
        WritePlaceholderPng(picPath, 480, 272, false);
    }

    std::string userSnd;
    if (resolveAsset(info.snd0, userSnd)) {
        if (!fsx::CopyFile(userSnd, sndPath)) return fail("Cannot copy SND0: " + userSnd);
        AppendLog(result.log, "[INFO] SND0.AT3 copied from " + userSnd);
    } else {
        if (fsx::FileExists(sndPath)) {
            result.warnings.push_back("SND0.AT3 not provided - reusing previous sound.");
        } else {
            std::vector<unsigned char> silent = {0x52, 0x49, 0x46, 0x46};
            silent.resize(1024, 0);
            fsx::WriteBinaryFile(sndPath, silent);
            result.warnings.push_back(
                "SND0.AT3 not provided - a silent placeholder was generated (replace it with a "
                "real ATRAC3 file for music).");
        }
    }
    if (aborted()) return fail("Build cancelled.");

    // ---- 3. compile the runtime -------------------------------------------
    // Resolve the runtime template sources: explicit option -> next to the
    // editor binary (CMake stages them there) -> the CWD (developer runs).
    std::string runtimeDir = options.runtimeSourceDir;
    if (runtimeDir.empty()) {
        const std::string beside = fsx::PathJoin(fsx::ExecutableDir(), "PSPRuntime");
        if (fsx::FileExists(fsx::PathJoin(beside, "Source/RuntimeApp.cpp"))) {
            runtimeDir = beside;
        } else if (fsx::FileExists(fsx::Absolute("PSPRuntime/Source/RuntimeApp.cpp"))) {
            runtimeDir = fsx::Absolute("PSPRuntime");
        }
    }
    if (!fsx::FileExists(fsx::PathJoin(runtimeDir, "Source/RuntimeApp.cpp"))) {
        return fail("PSPRuntime sources not found (looked in '" + runtimeDir + "'). "
                    "The editor binary must sit next to its PSPRuntime/ directory.");
    }

    const std::string buildDir = fsx::PathJoin(packageDir, "obj");
    fsx::EnsureDirectory(buildDir);
    // Embed the startup script (explicit setting, else the first scene object's
    // script) so the EBOOT runs Lua without external files.
    std::string startupScript = info.startupScript;
    if (startupScript.empty()) {
        for (const auto& object : scene.objects) {
            if (!object.script.empty()) {
                startupScript = object.script;
                break;
            }
        }
    }
    if (!startupScript.empty()) {
        AppendLog(result.log, "[INFO] Embedding startup script: " + startupScript);
    }

    // The runtime sources include Shared/... headers, so the project needs a
    // copy of the shared tree next to it (created on first build).
    // Shared/ and third_party/ live next to PSPRuntime/ in the engine checkout,
    // and also next to the editor binary because CMake stages them there.
    auto stageTree = [&](const char* name) {
        const std::string to = fsx::PathJoin(project.rootPath, name);
        if (fsx::DirectoryExists(to)) return;
        const std::string candidates[] = {
            fsx::PathJoin(fsx::ParentDir(runtimeDir), name),
            fsx::PathJoin(fsx::ExecutableDir(), name),
            fsx::Absolute(name),
        };
        for (const auto& from : candidates) {
            if (!fsx::DirectoryExists(from)) continue;
            if (fsx::CopyDirectoryRecursive(from, to)) {
                AppendLog(result.log, std::string("[INFO] Staged ") + name + "/ into the project from " + from);
                return;
            }
        }
        AppendLog(result.log, std::string("[WARN] Could not stage ") + name + "/ into the project.");
    };
    stageTree("third_party");
    stageTree("Shared");

    const std::string generatedAssets = fsx::PathJoin(buildDir, "generated_assets.cpp");
    if (!GenerateAssetSource(project, generatedAssets, startupScript, sceneError)) return fail(sceneError);

#if defined(_WIN32)
    std::string gxxRaw = fsx::PathJoin(sdkBin, "psp-g++.exe");
    std::string gccRaw = fsx::PathJoin(sdkBin, "psp-gcc.exe");
#else
    std::string gxxRaw = fsx::PathJoin(sdkBin, "psp-g++");
    std::string gccRaw = fsx::PathJoin(sdkBin, "psp-gcc");
#endif
    // psp-g++ must drive the link: only it pulls in the C++ startup files
    // (crtbegin/crtend), which register static constructors. Linking with the
    // plain C driver yields an EBOOT that silently halts before the first frame.
    std::string gxx = PathQuote(gxxRaw);
    std::string gcc = PathQuote(gccRaw);
    std::string elfPath = fsx::PathJoin(buildDir, "runtime.elf");
    std::vector<std::string> sources = {generatedAssets,
                                        fsx::PathJoin(runtimeDir, "Source/RuntimeApp.cpp"),
                                        fsx::PathJoin(runtimeDir, "Source/SceneRuntime.cpp"),
                                        fsx::PathJoin(runtimeDir, "Source/RuntimeLua.cpp"),
                                        fsx::PathJoin(fsx::ParentDir(runtimeDir), "Shared/Mesh/GltfLoader.cpp"),
                                        fsx::PathJoin(runtimeDir, "Source/GltfLoaderShim.cpp")};

    // PSPSDK locations: <PSPDEV>/psp/include (SDK headers) + <PSPSDK>/include
    // (the sdk dir); libraries live in <PSPDEV>/psp/lib.
    // sdkBin is always a bin directory; PSPDEV is its parent. LocatePspsdk()
    // normalises any spelling of the path (root or bin) before we get here.
    const std::string pspdevRoot = fsx::FileName(sdkBin) == "bin" ? fsx::ParentDir(sdkBin) : sdkBin;
    std::string pspSdkPath;
    {
        std::string output;
        if (RunCommand(PathQuote(fsx::PathJoin(sdkBin, "psp-config")) + " --pspsdk-path", output) == 0) {
            while (!output.empty() && (output.back() == '\n' || output.back() == '\r'))
                output.pop_back();
            pspSdkPath = output;
        }
    }
    if (pspSdkPath.empty()) pspSdkPath = fsx::PathJoin(pspdevRoot, "psp/sdk");
    AppendLog(result.log, "[INFO] PSPSDK headers: " + pspSdkPath + " | PSPDEV: " + pspdevRoot);

    // psp-gcc's own internal headers (stddef.h, ...) are not in its default
    // search path when invoked with an absolute path.
    std::string pspInternalIncludes;
    {
        std::string output;
        if (RunCommand(PathQuote(fsx::PathJoin(sdkBin, "psp-gcc")) +
                           " -print-search-dirs 2>/dev/null | head -n1",
                       output) == 0) {
            // Nothing needed here: psp-gcc resolves its internal includes itself
            // as long as -nostdinc is not used.
        }
        // Explicit fallback for the hosted GCC layout used by pspdev releases.
        const std::string gccVersionDir = pspdevRoot + "/lib/gcc/psp";
        for (const auto& entry : fsx::ListDirectory(gccVersionDir)) {
            if (entry.isDirectory) {
                pspInternalIncludes += " -I" + PathQuote(entry.path + "/include");
            }
        }
        if (pspInternalIncludes.empty()) pspInternalIncludes = " -I" + PathQuote(pspdevRoot + "/lib/gcc/psp/include");
    }

    std::string cxxFlags =
        "-std=c++17 -O2 -G0 -Wall -DLUA_32BITS -D_PSP_FW_VERSION=600 -DPSP_ENGINE_TINY_JSON -I" +
        PathQuote(fsx::PathJoin(runtimeDir, "Source")) + " -I" + PathQuote(fsx::PathJoin(runtimeDir, "Include")) +
        " -I" + PathQuote(fsx::PathJoin(fsx::ParentDir(runtimeDir), "Shared")) + " -I" +
        PathQuote(fsx::PathJoin(project.rootPath, "third_party/lua/src/src")) + " -I" +
        PathQuote(fsx::PathJoin(fsx::ParentDir(runtimeDir), "third_party/glm")) + " -I" +
        PathQuote(fsx::PathJoin(pspdevRoot, "psp/include")) + " -I" + PathQuote(pspSdkPath + "/include") +
        " " + pspInternalIncludes;

    // Library list mirrors PSPSDK's build.mak user-mode defaults, which is exactly
    // what PSPRuntime/Makefile links against. Both builds therefore link
    // identically; adding -lpspkernel here (as an earlier revision did) results in
    // an EBOOT that boots to a black screen.
    std::string linkFlags =
        "-L" + PathQuote(pspdevRoot + "/psp/lib") + " -L" + PathQuote(pspSdkPath + "/lib") +
        " -Wl,-zmax-page-size=128 -lpspgu -lpspgum -lpsprtc -lstdc++ -lm"
        " -lpspdebug -lpspdisplay -lpspge -lpspctrl -lpspnet -lpspnet_apctl";
    std::string luaLib;
    const std::string luaCandidates[] = {fsx::PathJoin(runtimeDir, "lib/libpsplua.a"),
                                         fsx::PathJoin(project.rootPath, "PSPRuntime/lib/libpsplua.a"),
                                         fsx::Absolute("PSPRuntime/lib/libpsplua.a")};
    for (const auto& candidate : luaCandidates) {
        if (fsx::FileExists(candidate)) {
            luaLib = candidate;
            break;
        }
    }
    std::string libs;
    if (!luaLib.empty()) {
        libs = PathQuote(luaLib) + " -L" + PathQuote(fsx::ParentDir(luaLib));
        AppendLog(result.log, "[INFO] Linking Lua 5.4: " + luaLib);
    } else {
        result.warnings.push_back("libpsplua.a missing - Lua scripts will be disabled in the EBOOT.");
    }

    const bool compile = !options.skipRuntimeCompile;
    if (compile) {
        // Compile each translation unit (keeps error logs readable).
        struct Unit {
            std::string name;
            std::string command;
        };
        std::vector<Unit> units;
        std::vector<std::string> objects;
        for (const auto& source : sources) {
            std::string object = fsx::PathJoin(buildDir, fsx::FileName(source) + ".o");
            objects.push_back(object);
            units.push_back({fsx::FileName(source),
                             gcc + " " + cxxFlags + " -c " + PathQuote(source) + " -o " + PathQuote(object)});
        }
        std::string linkLine = gxx;
        for (const auto& object : objects) linkLine += " " + PathQuote(object);
        linkLine += " " + libs + " " + linkFlags + " -o " + PathQuote(elfPath);
        units.push_back({"link", linkLine});
        // PSPSDK's build.mak always post-processes the ELF with psp-fixup-imports;
        // without it the import stubs point at garbage and the EBOOT crashes at
        // startup with "Bad Execution Address".
#if defined(_WIN32)
        units.push_back({"psp-fixup-imports",
                         PathQuote(fsx::PathJoin(sdkBin, "psp-fixup-imports.exe")) + " " + PathQuote(elfPath)});
#else
        units.push_back({"psp-fixup-imports",
                         PathQuote(fsx::PathJoin(sdkBin, "psp-fixup-imports")) + " " + PathQuote(elfPath)});
#endif

        for (const auto& unit : units) {
            if (aborted()) return fail("Build cancelled.");
            AppendLog(result.log, "$ " + unit.command);
            std::string output;
            int code = RunCommand(unit.command, output);
            if (!output.empty()) AppendLog(result.log, output);
            if (code != 0) {
                result.exitCode = code;
                return fail("PSP runtime compilation failed at step: " + unit.name);
            }
        }
        AppendLog(result.log, "[INFO] Compiled runtime.elf with psp-gcc.");
    } else {
        if (!fsx::FileExists(elfPath)) return fail("skipRuntimeCompile set but runtime.elf does not exist.");
        AppendLog(result.log, "[INFO] Reusing existing runtime.elf (skipRuntimeCompile).");
    }
    if (aborted()) return fail("Build cancelled.");

    // ---- 4. PARAM.SFO ------------------------------------------------------
    const std::string sfoPath = fsx::PathJoin(packageDir, "PARAM.SFO");
    std::string sfoCommand = PathQuote(fsx::PathJoin(sdkBin, "mksfoex")) + " -s \"BOOTABLE=1\" -s " +
                             QuoteForShell("TITLEID=" + titleId) + " -d " +
                             QuoteForShell("APP_VER=" + info.version) + " -s " +
                             QuoteForShell("PARENTAL_LEVEL=" + info.parentalLevel) + " " +
                             QuoteForShell(info.title.empty() ? std::string("PSP Engine Game") : info.title) +
                             " " + PathQuote(sfoPath);
    AppendLog(result.log, "$ " + sfoCommand);
    {
        std::string output;
        int code = RunCommand(sfoCommand, output);
        if (!output.empty()) AppendLog(result.log, output);
        if (code != 0) {
            result.exitCode = code;
            return fail("mksfoex failed.");
        }
    }

    // ---- 5. EBOOT.PBP ------------------------------------------------------
    std::string pack = PathQuote(fsx::PathJoin(sdkBin, "pack-pbp"));
    // PBP section order: PARAM.SFO ICON0.PNG ICON1.PMF PIC0.PNG PIC1.PNG
    // SND0.AT3 DATA.PSP DATA.PSAR. DATA.PSP takes the (stripped) ELF - the same
    // plain-executable layout PSPSDK's build.mak produces for BUILD_PRX=0.
    const std::string strippedElf = fsx::PathJoin(buildDir, "runtime_strip.elf");
    {
        std::string stripCommand = PathQuote(fsx::PathJoin(sdkBin, "psp-strip")) + " " +
                                   PathQuote(elfPath) + " -o " + PathQuote(strippedElf);
        std::string output;
        int code = RunCommand(stripCommand, output);
        if (code != 0 || !fsx::FileExists(strippedElf)) {
            AppendLog(result.log, "[WARN] psp-strip failed, packing the unstripped ELF.");
            result.warnings.push_back("psp-strip failed - the EBOOT is larger than necessary.");
        }
    }
    const std::string packElf = fsx::FileExists(strippedElf) ? strippedElf : elfPath;
    std::string packCommand = pack + " " + PathQuote(ebootPath) + " " + PathQuote(sfoPath) + " " +
                              PathQuote(iconPath) + " NULL NULL " + PathQuote(picPath) + " " +
                              PathQuote(sndPath) + " " + PathQuote(packElf) + " NULL";
    AppendLog(result.log, "$ " + packCommand);
    {
        std::string output;
        int code = RunCommand(packCommand, output);
        if (!output.empty()) AppendLog(result.log, output);
        if (code != 0) {
            result.exitCode = code;
            return fail("pack-pbp failed.");
        }
    }

    if (!fsx::FileExists(ebootPath)) return fail("EBOOT.PBP was not produced.");

    // Copy the finished artifacts one level up for convenience: bin/PSPRuntime.
    result.exitCode = 0;
    result.success = true;
    AppendLog(result.log, "[OK] EBOOT.PBP created: " + ebootPath);
    return result;
}

} // namespace pspeditor
