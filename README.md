# PSPulse — a cross-platform 3D engine and scene editor for the PlayStation Portable

**English** · [Русский](README.ru.md)

A scene editor for PC (GLFW + OpenGL 3.3 + Dear ImGui docking + ImGuizmo) and a
runtime for the PlayStation Portable (PSPSDK, GU/GUM, embedded Lua 5.4). The
editor packages a ready-to-run `EBOOT.PBP` with a single button press: the
scene, Lua scripts and glTF models are compiled into the ELF, then `mksfoex` and
`pack-pbp` produce the image.

On first launch without PSPSDK installed, the editor offers to download the
official pspdev toolchain (~175 MB) and unpacks it next to itself.

## Layout

```
Editor/          PC editor: Application, EditorState, Panels (Outliner,
                 Inspector, Content Browser, Viewport, Build Settings),
                 Renderer, Camera, ProjectIO, BuildPipeline
PSPRuntime/      PSP runtime: RuntimeApp (GU/GUM rendering, HUD, input),
                 SceneRuntime (JSON scene, physics), RuntimeLua (Lua 5.4
                 bridge), Makefile (PSPSDK)
Shared/          Code shared by the editor and the runtime: Scene (GameObject,
                 Component, Transform, PhysicsBody, CollisionShape), SceneIO
                 (JSON), Scripting/LuaVM, Mesh primitives, Physics
Shared/Mesh/     MeshGen (primitives) + GltfLoader (glTF 2.0: .gltf/.glb)
Shared/Json/     TinyJsonShim — a JSON facade used by the glTF loader on PSP
Editor/Source/   ... + PspsdkInstaller (toolchain auto-install)
Tests/           headless tests: Lua bridge, glTF loader, PNG (ctest)
                 + Tests/data — test fixtures
third_party/     glm, nlohmann_json, dear imgui (docking), ImGuizmo, glad,
                 stb, lua 5.4.6
projects/        Your projects (created in the editor)
bin/PSPulse/     PSPulse(.exe)
bin/PSPRuntime/  EBOOT.PBP (runtime template)
```

## PSPSDK: automatic installation on first launch

If `$PSPDEV`, `/usr/local/pspdev`, `/opt/pspdev` and `<editor>/pspsdk/pspdev`
are all missing, the editor shows a modal dialog offering to download the
official prebuilt toolchain:

* `Tools → Install PSPSDK...` starts the installation manually at any time;
* it downloads `pspdev-<platform>-latest.tar.gz` from github.com/pspdev/pspdev
  into `<editor>/pspsdk/cache/` (the cache is reused, never downloaded twice);
* it unpacks to `<editor>/pspsdk/pspdev`, then verifies with `psp-gcc --version`;
* progress and log are shown in the "PSPSDK installation" window, and the
  installation can be cancelled;
* there is no native Windows build of pspdev — the editor says so and points to
  WSL2/MSYS2 instead of pretending to install something.

Lookup order: explicit path from Build Settings → `<editor>/pspsdk/pspdev` →
`$PSPDEV` → `/usr/local/pspdev` → `/opt/pspdev`.

## Building the desktop editor

Requires CMake ≥ 3.16 and a C++17 compiler. Every dependency is pulled in by
CMake FetchContent or a system package; nothing has to be installed by hand.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The result is `bin/PSPulse/PSPulse` (Linux) or `bin/PSPulse/PSPulse.exe`
(Windows, MSVC/MinGW). Run it:

```bash
bin/PSPulse/PSPulse                       # empty editor window
bin/PSPulse/PSPulse --project projects/<your-project>
```

## Building the PSP runtime (make)

Requires an installed PSPSDK (`$PSPDEV`, `psp-gcc`, `mksfoex`, `pack-pbp`,
`psp-fixup-imports`, `psp-strip`).

```bash
export PSPDEV=/opt/pspdev
export PATH=$PSPDEV/bin:$PATH
cd PSPRuntime && make clean && make && make dist
```

The result is `PSPRuntime/EBOOT.PBP` plus a copy in `bin/PSPRuntime/EBOOT.PBP`.
`libpsplua.a` is built from the vendored Lua sources by the Makefile, so a clean
checkout works without any prebuilt artifacts.

## 3D models (glTF 2.0)

Both `.gltf` (JSON, buffers as `data:` URIs or adjacent files) and `.glb`
(binary container) are supported. Draco/meshopt compression is not supported —
export without it.

* the Inspector has a **Mesh** section per object: model path, a dropdown of
  every `.gltf/.glb` under `assets/`, and live diagnostics
  (vertices/triangles/errors);
* a model overrides the primitive; on error the object is simply not drawn and
  the reason goes to the Console (editor) or the debug overlay (PSP);
* when packaging an EBOOT every model under `assets/**` is embedded into the
  ELF — the game stays a single file;
* in a scene the path is stored as `"mesh_path": "assets/models/robot.glb"`.

The loader (`Shared/Mesh/GltfLoader.*`) is shared by the editor and the PSP: on
PC it uses nlohmann/json, on PSP a compact `TinyJsonShim` over the runtime's own
JSON parser (saves ~600 KB of ELF).

## Packaging a game from the editor

`Tools → Build Settings` (TITLE, TITLE_ID, APP_VER, ICON0/PIC1/SND0) → the
**Build EBOOT.PBP** button. The pipeline (`Editor/Source/BuildPipeline.cpp`):

1. creates `<project>/build/pkg/` (`assets/`, `scenes/`, `scripts/`),
2. validates/downscales ICON0.PNG (144×80) and PIC1.PNG (480×272), generating
   placeholders when the files are missing,
3. writes `generated_assets.cpp` — the scene and every `scripts/*.lua` are
   compiled into the binary (the EBOOT runs without external files),
4. `psp-g++ -c` per runtime module → `runtime.elf`,
5. `psp-fixup-imports runtime.elf` (a mandatory PSPSDK step; without it the
   EBOOT crashes on startup with "Bad Execution Address"),
6. `mksfoex -s BOOTABLE=1 -s TITLEID=... -d APP_VER=...`,
7. `psp-strip` + `pack-pbp EBOOT.PBP PARAM.SFO ICON0 NULL NULL PIC1 SND0 <elf> NULL`.

Relative paths (`build/pkg`) resolve against the project root, not the CWD.

## Building on Windows

### Prebuilt binaries (GitHub Actions)

`.github/workflows/windows.yml` builds the editor with two toolchains on a
native `windows-latest` runner and publishes the artifacts:

| Job | Toolchain | Artifact | Size |
|-----|-----------|----------|------|
| `build-windows` | MSVC (VS2022/VS2026, x64) | `PSPulse-windows-msvc` | ~2.2 MB |
| `build-mingw` | MinGW-w64 | `PSPulse-windows-mingw` | ~5.9 MB |

Both binaries are self-contained: the CRT is linked statically
(`CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` for MSVC,
`-static -static-libgcc -static-libstdc++` for MinGW), so the only dependencies
are `KERNEL32/USER32/GDI32/SHELL32` — neither the VC++ Redistributable nor
`libstdc++-6.dll` is needed. The Visual Studio generator is detected through
`vswhere` (17 → 2022, 18 → 2026), because `windows-latest` has moved to VS 2026.

### Building manually

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
# bin/PSPulse/PSPulse.exe
```

### Headless game build (CI/scripts)

```bash
./bin/PSPulse/PSPulse --project projects/<your-project> --build
# projects/<your-project>/build/pkg/EBOOT.PBP
```

The editor locates `PSPRuntime/` next to its own binary, and when `libpsplua.a`
is missing it compiles Lua 5.4 from `third_party/lua` — a clean checkout needs
no prebuilt archive.

### Limitation: PSPSDK on Windows

The prebuilt pspdev toolchain is published for Linux only
(`pspdev-debian-latest.tar.gz`). On Windows it runs through **WSL2** (simplest)
or MSYS2. The editor accounts for this: the `psp-gcc` path can be set manually,
and "Install PSPSDK..." on Windows points at WSL2.

## Releases

Releases are built and published automatically
(`.github/workflows/release.yml`): every artifact is compiled from source in the
same run, nothing is uploaded by hand.

```bash
git tag v1.0.1 && git push origin v1.0.1      # releases v1.0.1
```

Or manually: **Actions → Release → Run workflow** and type a version (this works
without a tag too). A release is published only if every build succeeded.

| File | Platform |
|------|----------|
| `PSPulse-<ver>-windows-x64.exe` | Windows x64, MSVC, static CRT |
| `PSPulse-<ver>-windows-x64-mingw.exe` | Windows x64, MinGW |
| `PSPulse-<ver>-linux-x64` | Linux x64 (needs glibc ≥ 2.34, i.e. Ubuntu 22.04+) |
| `PSPulse-<ver>-runtime-template.EBOOT.PBP` | PSP, runtime template |

Plus `CHECKSUMS.txt` with a SHA-256 for every file. The version inside the
binary comes from the git tag (`PSPulse version: X.Y.Z` at configure time), so
the editor's About line always matches the release.

## Troubleshooting

* **PSPSDK not found** — the editor still builds; the `PSPSDK bin` path can be
  set in Build Settings. The build then prints an installation hint.
* **EBOOT hangs on a black screen** — make sure the link runs through `psp-g++`
  (the C++ CRT registers static constructors) and that `psp-fixup-imports` was
  executed after linking.
* **Lua: `attempt to index a nil value (global 'input')`** — the `input` table
  has to be assigned into the environment (`env.input = ...`) instead of being
  left on the stack.

## Tests

```bash
cmake --build build -j && cd build && ctest --output-on-failure
```

* `lua_vm` — the Lua bridge: `input.axis/pressed`, `on_start`/`on_update`
  calls, transform writes. Catches crashes like "PANIC: unprotected error in
  call to Lua API" (a wrong stack index in a C function → `abort()`).
* `gltf_loader` — loads `.gltf`/`.glb`, applies node transforms, checks bounds,
  and reports a clean error for a missing file.
* `png_decoder` — the built-in inflate (RFC 1950/1951) with no zlib: decodes a
  PNG and re-encodes ICON0 at 144x80. Without it the Windows build would need a
  system zlib, and the PSP would get black icon artwork.

## Verified

Development was driven by real builds and real runs; every item below was
checked on the machine, not assumed:

* the desktop editor starts under Xvfb (software OpenGL) with all panels,
  docking and ImGuizmo working;
* `bin/PSPRuntime/EBOOT.PBP` built via `make` boots in PPSSPP: grid scene, HUD
  and FPS counter;
* an EBOOT built **from the editor GUI** (Build Settings → Build EBOOT.PBP)
  shows the project scene plus a working Lua script;
* glTF models (`.gltf` and `.glb`) render both in the editor and in PPSSPP;
* the PSPSDK auto-install dialog, download and unpack;
* an EBOOT built with the **freshly installed** toolchain (no `$PSPDEV` set);
* **clean clone:** an EBOOT builds without a prebuilt `libpsplua.a` (the editor
  compiles Lua itself) and boots in PPSSPP with `LUA 1`; the CI EBOOT is
  byte-identical to the local one;
* **GitHub Actions:** Windows (MSVC + MinGW), Linux and the PSP runtime are all
  green, and the Windows binaries start without the VC++ Redistributable;
* the `lua_vm`, `gltf_loader` and `png_decoder` tests pass (`ctest`).
