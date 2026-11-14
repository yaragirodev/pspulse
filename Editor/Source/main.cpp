// ---------------------------------------------------------------------------
//  PSPulse Editor - entry point
//
//  Cross-platform 3D scene editor for the PlayStation Portable runtime.
//  Built with GLFW + OpenGL 3.3 + Dear ImGui (docking) + ImGuizmo.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>

#include "Application.h"
#include "BuildPipeline.h"
#include "ProjectIO.h"

// Headless packaging: `PSPulse --project <dir> --build`. Used by CI to smoke
// test the whole EBOOT pipeline without an OpenGL context.
static int RunHeadlessBuild(const std::string& projectPath) {
    pspeditor::Project project;
    std::string error;
    if (!pspeditor::LoadProject(projectPath, project, error)) {
        std::fprintf(stderr, "Cannot open project '%s': %s\n", projectPath.c_str(), error.c_str());
        return 2;
    }
    // BuildOptions defaults are fine: BuildGame() locates PSPRuntime/ next to
    // the executable (CMake stages it there) or in the working directory.
    const pspeditor::BuildResult result = pspeditor::BuildGame(project, pspeditor::BuildOptions{});
    std::printf("%s\n", result.log.c_str());
    for (const auto& warning : result.warnings) std::printf("[WARNING] %s\n", warning.c_str());
    if (!result.success) {
        std::fprintf(stderr, "Build failed (exit code %d)\n", result.exitCode);
        return 3;
    }
    std::printf("EBOOT.PBP: %s\n", result.ebootPath.c_str());
    return 0;
}

int main(int argc, char** argv) {
    std::string projectPath;
    bool headlessBuild = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--project" || arg == "-p") && i + 1 < argc) {
            projectPath = argv[++i];
        } else if (arg.rfind("--project=", 0) == 0) {
            projectPath = arg.substr(std::string("--project=").size());
        } else if (arg == "--build") {
            headlessBuild = true;
        }
    }

    if (headlessBuild) {
        if (projectPath.empty()) {
            std::fprintf(stderr, "--build requires --project <dir>\n");
            return 2;
        }
        return RunHeadlessBuild(projectPath);
    }

    pspeditor::Application app;
    int result = app.Run(projectPath);
    app.Shutdown();

    if (result != 0) {
        std::fprintf(stderr, "PSPulse exited with code %d\n", result);
    }
    return result;
}
