# ---------------------------------------------------------------------------
#  PSP Engine - desktop editor (Windows / Linux)
#
#  Dependencies are fetched automatically with CMake FetchContent:
#      GLFW 3.4, GLM 1.0.1, glad (vendored, OpenGL 3.3 core),
#      nlohmann/json 3.11.3, Dear ImGui (docking), ImGuizmo (vendored),
#      Lua 5.4.6 (vendored).
#
#  Configure & build:
#      cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#      cmake --build build --config Release -j
#      # binary: bin/Editor/PSPEditor(.exe)
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
project(PSPEditor VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin/Editor)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config} ${CMAKE_SOURCE_DIR}/bin/Editor)
endforeach()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PSPEDITOR_FETCH_DEPS "Download dependencies with FetchContent" ON)

# ---------------------------------------------------------------------------
# Platform prerequisites
# ---------------------------------------------------------------------------
if(WIN32)
    # GLFW on Windows only needs the Win32 API + opengl32.
    set(PSPEDITOR_PLATFORM_LIBS opengl32)
else()
    find_package(Threads REQUIRED)
    find_package(OpenGL REQUIRED)
    set(PSPEDITOR_PLATFORM_LIBS OpenGL::GL Threads::Threads ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------------------
# FetchContent: GLFW, GLM, nlohmann/json
# ---------------------------------------------------------------------------
if(PSPEDITOR_FETCH_DEPS)
    include(FetchContent)
    # Keep dependencies out of the source tree, but let callers (CI, cross
    # builds for another target OS) point FetchContent elsewhere.
    if(NOT FETCHCONTENT_BASE_DIR)
        set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/.deps CACHE PATH "FetchContent cache")
    endif()

    # Build GLFW against X11 only (no wayland-scanner requirement).
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glfw)

    set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLM_ENABLE_CXX_20 OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(glm)

    set(JSON_BuildTests OFF CACHE INTERNAL "")
    FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(nlohmann_json)
else()
    find_package(glfw3 3.3 REQUIRED)
    find_package(glm REQUIRED)
    find_package(nlohmann_json REQUIRED)
endif()

# ---------------------------------------------------------------------------
# Dear ImGui (docking branch) - built as a static library manually
# ---------------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
if(NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
    include(FetchContent)
    if(NOT FETCHCONTENT_BASE_DIR)
        set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/.deps CACHE PATH "FetchContent cache")
    endif()
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.9b-docking
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(imgui)
    set(IMGUI_DIR ${imgui_SOURCE_DIR})
endif()

add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC glfw)
if(WIN32)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD2)
else()
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD2)
endif()

# ---------------------------------------------------------------------------
# glad (OpenGL 3.3 core loader, vendored)
# ---------------------------------------------------------------------------
add_library(glad STATIC ${CMAKE_SOURCE_DIR}/third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/third_party/glad/include)
if(NOT WIN32)
    # Load GL entry points from libGL at runtime (no link-time hidden symbols).
    target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------------------
# Lua 5.4.6 (vendored, editor side - statically linked)
# ---------------------------------------------------------------------------
file(GLOB LUA_SOURCES ${CMAKE_SOURCE_DIR}/third_party/lua/src/src/*.c)
list(REMOVE_ITEM LUA_SOURCES
    ${CMAKE_SOURCE_DIR}/third_party/lua/src/src/lua.c
    ${CMAKE_SOURCE_DIR}/third_party/lua/src/src/luac.c)
add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC ${CMAKE_SOURCE_DIR}/third_party/lua/src/src)
if(NOT WIN32)
    target_compile_definitions(lua_static PUBLIC LUA_USE_LINUX)
else()
    target_compile_definitions(lua_static PUBLIC LUA_USE_WINDOWS)
endif()

# ---------------------------------------------------------------------------
# ImGuizmo (vendored, MIT)
# ---------------------------------------------------------------------------
add_library(imgui_gizmo STATIC ${CMAKE_SOURCE_DIR}/third_party/ImGuizmo/ImGuizmo.cpp)
target_include_directories(imgui_gizmo PUBLIC ${CMAKE_SOURCE_DIR}/third_party/ImGuizmo)
target_link_libraries(imgui_gizmo PUBLIC imgui)

# ---------------------------------------------------------------------------
# Editor executable
# ---------------------------------------------------------------------------
set(EDITOR_SOURCES
    Editor/Source/main.cpp
    Editor/Source/Application.cpp
    Editor/Source/EditorState.cpp
    Editor/Source/Panels.cpp
    Editor/Source/Renderer.cpp
    Editor/Source/Camera.cpp
    Editor/Source/ProjectIO.cpp
    Editor/Source/BuildPipeline.cpp
    Editor/Source/PspsdkInstaller.cpp
    Shared/Scene/SceneData.cpp
    Shared/Scene/SceneIO.cpp
    Shared/Physics/Physics.cpp
    Shared/Mesh/MeshGen.cpp
    Shared/Mesh/GltfLoader.cpp
    Shared/Scripting/LuaVM.cpp)

# Console subsystem on both platforms: the editor logs startup diagnostics
# (OpenGL version, PSPSDK detection) and the build pipeline output, which a
# WIN32 (GUI subsystem) binary would swallow.
add_executable(PSPEditor ${EDITOR_SOURCES})
set_target_properties(PSPEditor PROPERTIES OUTPUT_NAME "PSPEditor")

if(WIN32 AND MINGW)
    # Static MinGW runtime: the .exe then runs without libstdc++-6.dll,
    # libgcc_s_seh-1.dll and libwinpthread-1.dll sitting next to it.
    target_link_options(PSPEditor PRIVATE -static -static-libgcc -static-libstdc++)
endif()

target_include_directories(PSPEditor PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Editor/Source
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/glad/include)

target_link_libraries(PSPEditor PRIVATE
    imgui imgui_gizmo glad lua_static
    glfw glm::glm nlohmann_json::nlohmann_json
    ${PSPEDITOR_PLATFORM_LIBS})

if(WIN32)
    target_link_libraries(PSPEditor PRIVATE opengl32)
    target_compile_definitions(PSPEditor PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX GLFW_EXPOSE_NATIVE_WIN32)
else()
    target_compile_definitions(PSPEditor PRIVATE GLFW_EXPOSE_NATIVE_X11)
endif()

if(MSVC)
    target_compile_options(PSPEditor PRIVATE /W3 /utf-8 /MP)
else()
    target_compile_options(PSPEditor PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# GLM: matrix decomposition / intersections require the experimental extensions.
target_compile_definitions(PSPEditor PRIVATE GLM_ENABLE_EXPERIMENTAL)

# ---------------------------------------------------------------------------
# Copy the PSP runtime templates next to the editor binary so the Build
# Settings panel can find them at runtime.
# ---------------------------------------------------------------------------
add_custom_command(TARGET PSPEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin/Editor/PSPRuntime
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/PSPRuntime/Source
        ${CMAKE_SOURCE_DIR}/bin/Editor/PSPRuntime/Source
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/PSPRuntime/Include
        ${CMAKE_SOURCE_DIR}/bin/Editor/PSPRuntime/Include
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/bin/Editor/PSPRuntime/lib
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/Shared ${CMAKE_SOURCE_DIR}/bin/Editor/Shared
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/third_party/lua ${CMAKE_SOURCE_DIR}/bin/Editor/third_party/lua
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/third_party/glm ${CMAKE_SOURCE_DIR}/bin/Editor/third_party/glm
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json ${CMAKE_SOURCE_DIR}/bin/Editor/third_party/nlohmann_json
    COMMENT "Staging PSP runtime sources next to the editor")

# PSPRuntime/lib holds libpsplua.a and the Lua objects produced by
# `make -C PSPRuntime`; it is not part of the repository, so stage it only when
# the runtime has been built already (otherwise the editor builds Lua itself).
add_custom_command(TARGET PSPEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        -DPSPRUNTIME_SRC=${CMAKE_SOURCE_DIR}/PSPRuntime/lib
        -DDEST=${CMAKE_SOURCE_DIR}/bin/Editor/PSPRuntime/lib
        -P ${CMAKE_SOURCE_DIR}/cmake/StageLuaLib.cmake)

# ---------------------------------------------------------------------------
# Headless regression tests (no OpenGL / window): Lua bridge, scene IO.
#   cmake --build build --target PSPEngineTests && ctest --test-dir build
# ---------------------------------------------------------------------------
option(PSPEDITOR_BUILD_TESTS "Build headless regression tests" ON)

if(PSPEDITOR_BUILD_TESTS)
    enable_testing()
    add_executable(PSPEngineTests
        Tests/lua_vm_test.cpp
        Shared/Mesh/GltfLoader.cpp
        Shared/Scene/SceneData.cpp
        Shared/Scene/SceneIO.cpp
        Shared/Scripting/LuaVM.cpp)
    target_include_directories(PSPEngineTests PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(PSPEngineTests PRIVATE
        lua_static glm::glm nlohmann_json::nlohmann_json ${PSPEDITOR_PLATFORM_LIBS})
    target_compile_definitions(PSPEngineTests PRIVATE GLM_ENABLE_EXPERIMENTAL)
    add_test(NAME lua_vm COMMAND PSPEngineTests)

    add_executable(GltfLoaderTests Tests/gltf_test.cpp Shared/Mesh/GltfLoader.cpp)
    target_include_directories(GltfLoaderTests PRIVATE
        ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(GltfLoaderTests PRIVATE glm::glm nlohmann_json::nlohmann_json)
    target_compile_definitions(GltfLoaderTests PRIVATE GLM_ENABLE_EXPERIMENTAL)
    add_test(NAME gltf_loader
             COMMAND GltfLoaderTests ${CMAKE_SOURCE_DIR}/projects/MyPSPGame/assets/models)

    # PNG decoder used for ICON0/PIC1 (carries its own inflate implementation).
    add_executable(PngTests Tests/png_test.cpp
        Editor/Source/BuildPipeline.cpp
        Editor/Source/PspsdkInstaller.cpp
        Editor/Source/ProjectIO.cpp
        Shared/Scene/SceneData.cpp
        Shared/Scene/SceneIO.cpp
        Shared/Mesh/GltfLoader.cpp)
    target_include_directories(PngTests PRIVATE
        ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/Editor/Source
        ${CMAKE_SOURCE_DIR}/Package ${CMAKE_SOURCE_DIR}/third_party)
    target_link_libraries(PngTests PRIVATE glm::glm nlohmann_json::nlohmann_json)
    target_compile_definitions(PngTests PRIVATE GLM_ENABLE_EXPERIMENTAL)
    add_test(NAME png_decoder
             COMMAND PngTests ${CMAKE_SOURCE_DIR}/projects/MyPSPGame/assets/ICON0.PNG)
endif()

message(STATUS "PSPEditor: binary will be written to ${CMAKE_SOURCE_DIR}/bin/Editor")
