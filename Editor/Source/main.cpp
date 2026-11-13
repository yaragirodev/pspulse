// ---------------------------------------------------------------------------
//  PSP Engine Editor - entry point
//
//  Cross-platform 3D scene editor for the PlayStation Portable runtime.
//  Built with GLFW + OpenGL 3.3 + Dear ImGui (docking) + ImGuizmo.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <string>

#include "Application.h"

int main(int argc, char** argv) {
    std::string projectPath;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--project" || arg == "-p") && i + 1 < argc) {
            projectPath = argv[++i];
        } else if (arg.rfind("--project=", 0) == 0) {
            projectPath = arg.substr(std::string("--project=").size());
        }
    }

    pspeditor::Application app;
    int result = app.Run(projectPath);
    app.Shutdown();

    if (result != 0) {
        std::fprintf(stderr, "PSPEditor exited with code %d\n", result);
    }
    return result;
}
