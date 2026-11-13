# PSP Engine — кроссплатформенный 3D-движок и редактор сцен для PlayStation Portable

Редактор сцен (PC, GLFW + OpenGL 3.3 + Dear ImGui docking + ImGuizmo) и рантайм
для PlayStation Portable (PSPSDK, GU/GUM, встроенный Lua 5.4). Редактор умеет
собирать готовый `EBOOT.PBP` одной кнопкой: сцена, Lua-скрипты и glTF-модели
вкомпилируются в ELF, затем `mksfoex` + `pack-pbp` собирают образ.

При первом запуске без установленного PSPSDK редактор сам предлагает скачать
официальный тулчейн pspdev (~175 МБ) и распаковывает его рядом с собой.

## Структура

```
Editor/          ПК-редактор: Application, EditorState, Panels (Outliner,
                 Inspector, Content Browser, Viewport, Build Settings),
                 Renderer, Camera, ProjectIO, BuildPipeline
PSPRuntime/      Рантайм PSP: RuntimeApp (GU/GUM-рендер, HUD, ввод),
                 SceneRuntime (JSON-сцена, физика), RuntimeLua (Lua 5.4 мост),
                 Makefile (PSPSDK), lib/libpsplua.a
Shared/          Код, общий для редактора и рантайма: Scene (GameObject,
                 Component, Transform, PhysicsBody, CollisionShape), SceneIO
                 (JSON), Scripting/LuaVM, Mesh (примитивы), Physics
Shared/Mesh/     MeshGen (примитивы) + GltfLoader (glTF 2.0: .gltf/.glb)
Shared/Json/     TinyJsonShim — JSON-фасад для PSP-сборки glTF-загрузчика
Editor/Source/   ... + PspsdkInstaller (автоустановка тулчейна)
Tests/           headless-тесты: Lua-мост, glTF-загрузчик (ctest)
third_party/     glm, nlohmann_json, dear imgui (docking), ImGuizmo, glad,
                 stb, lua 5.4.6
projects/        Проекты редактора (MyPSPGame — пример)
bin/Editor/      PSPEditor(.exe)
bin/PSPRuntime/  EBOOT.PBP (шаблон рантайма)
screenshots/     Скриншоты сборок в PPSSPP
```

## PSPSDK: автоустановка при первом запуске

Если `$PSPDEV`, `/usr/local/pspdev`, `/opt/pspdev` и `<editor>/pspsdk/pspdev`
отсутствуют, редактор показывает модальное окно с предложением скачать
официальный prebuilt-тулчейн:

* `Tools → Install PSPSDK...` — запустить установку вручную в любой момент;
* качается `pspdev-<platform>-latest.tar.gz` с github.com/pspdev/pspdev
  в `<editor>/pspsdk/cache/` (кеш переиспользуется, повторно не качается);
* распаковка в `<editor>/pspsdk/pspdev`, затем `psp-gcc --version` как проверка;
* прогресс и лог видны в окне «PSPSDK installation», установку можно отменить;
* на нативной Windows официальной сборки pspdev нет — редактор объясняет это
  и предлагает WSL2/MSYS2 вместо фиктивной установки.

Приоритет поиска: явный путь из Build Settings → `<editor>/pspsdk/pspdev` →
`$PSPDEV` → `/usr/local/pspdev` → `/opt/pspdev`.

## Сборка ПК-редактора

Нужны CMake ≥ 3.16 и компилятор C++17. Все зависимости тянутся CMake
FetchContent / system-пакетами, вручную ничего ставить не нужно.

```bash
cmake -S /home/user -B /home/user/build -DCMAKE_BUILD_TYPE=Release
cmake --build /home/user/build -j$(nproc)
```

Результат: `bin/Editor/PSPEditor` (Linux) или `bin/Editor/PSPEditor.exe`
(Windows, MSVC/MinGW). Запуск:

```bash
bin/Editor/PSPEditor                       # пустое окно редактора
bin/Editor/PSPEditor --project projects/MyPSPGame
```

## Сборка PSP-рантайма (make)

Нужен установленный PSPSDK (`$PSPDEV`, `psp-gcc`, `mksfoex`, `pack-pbp`,
`psp-fixup-imports`, `psp-strip`).

```bash
export PSPDEV=/opt/pspdev
export PATH=$PSPDEV/bin:$PATH
cd PSPRuntime && make clean && make && make dist
```

Результат: `PSPRuntime/EBOOT.PBP` и копия в `bin/PSPRuntime/EBOOT.PBP`.

## 3D-модели (glTF 2.0)

Поддерживаются `.gltf` (JSON, буферы в `data:` URI или рядом лежащие) и `.glb`
(бинарный контейнер). Draco/meshopt-сжатие не поддерживается — экспортируйте
без него.

* в Inspector у объекта есть секция **Mesh**: путь к модели, выпадающий список
  всех `.gltf/.glb` из `assets/`, и живая диагностика (вершины/треугольники/ошибки);
* модель перекрывает primitive; при ошибке объект просто не рисуется, а причина
  пишется в Console (редактор) или в отладочный оверлей (PSP);
* при сборке EBOOT все модели из `assets/**` встраиваются в ELF — игра остаётся
  одним файлом;
* в сцене путь хранится как `"mesh_path": "assets/models/robot.glb"`.

Внутренний загрузчик (`Shared/Mesh/GltfLoader.*`) общий для редактора и PSP:
на PC он использует nlohmann/json, на PSP — компактный `TinyJsonShim` поверх
собственного JSON-парсера рантайма (экономит ~600 КБ ELF).

## Сборка игры из редактора

`Tools → Build Settings` (TITLE, TITLE_ID, APP_VER, ICON0/PIC1/SND0) →
кнопка **Build EBOOT.PBP**. Пайплайн (`Editor/Source/BuildPipeline.cpp`):

1. создаёт `<project>/build/pkg/` (`assets/`, `scenes/`, `scripts/`),
2. проверяет/масштабирует ICON0.PNG (144×80) и PIC1.PNG (480×272),
   генерирует заглушки, если файлов нет,
3. пишет `generated_assets.cpp` — сцена + все `scripts/*.lua` вкомпилируются
   в бинарь (EBOOT работает без внешних файлов),
4. `psp-g++ -c` по каждому модулю рантайма → `runtime.elf`,
5. `psp-fixup-imports runtime.elf` (обязательный шаг PSPSDK; без него EBOOT
   падает на старте с Bad Execution Address),
6. `mksfoex -s BOOTABLE=1 -s TITLEID=... -d APP_VER=...`,
7. `psp-strip` + `pack-pbp EBOOT.PBP PARAM.SFO ICON0 NULL NULL PIC1 SND0 <elf> NULL`.

Относительные пути (`build/pkg`) резолвятся от корня проекта, а не от CWD.

## Сборка под Windows

### Готовые бинарники (GitHub Actions)

`.github/workflows/windows.yml` собирает редактор двумя тулчейнами на нативном
`windows-latest` и публикует артефакты:

| Job | Тулчейн | Артефакт | Размер |
|-----|---------|----------|--------|
| `build-windows` | MSVC (VS2022/VS2026, x64) | `PSPEditor-windows-x64` | ~2.2 МБ |
| `build-mingw` | MinGW-w64 | `PSPEditor-windows-mingw` | ~5.9 МБ |

Оба бинарника самодостаточны: CRT слинкован статически
(`CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` для MSVC,
`-static -static-libgcc -static-libstdc++` для MinGW), поэтому зависимости —
только `KERNEL32/USER32/GDI32/SHELL32` и не нужен ни VC++ Redistributable,
ни `libstdc++-6.dll`. Генератор VS определяется через `vswhere`
(17 → 2022, 18 → 2026), потому что `windows-latest` переехал на VS 2026.

### Сборка вручную

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
# bin/Editor/PSPEditor.exe
```

### Headless-сборка игры (CI/скрипты)

```bash
./bin/Editor/PSPEditor --project projects/MyPSPGame --build
# projects/MyPSPGame/build/pkg/EBOOT.PBP
```

Редактор сам находит `PSPRuntime/` рядом с бинарником, при отсутствии
`libpsplua.a` компилирует Lua 5.4 из `third_party/lua` — готовый архив для
чистого клона не нужен.

### Ограничение: PSPSDK под Windows

Prebuilt-тулчейн pspdev выпускается только под Linux (`pspdev-debian-latest.tar.gz`).
Под Windows он запускается через **WSL2** (проще всего) или MSYS2. Редактор
это учитывает: путь к `psp-gcc` можно указать вручную, а кнопка «Install
PSPSDK...» под Windows предложит WSL2.

## Устранение неполадок

* **PSPSDK не найден** — редактор всё равно собирается; в Build Settings можно
  указать путь к `PSPSDK bin`. При сборке выводится подсказка по установке.
* **EBOOT зависает на чёрном экране** — проверьте, что линковка идёт через
  `psp-g++` (C++ CRT регистрирует статические конструкторы) и что после линковки
  выполнен `psp-fixup-imports`.
* **Lua: `attempt to index a nil value (global 'input')`** — таблица `input`
  должна присваиваться окружению (`env.input = ...`), а не оставаться на стеке.

## Тесты

```bash
cmake --build build -j && cd build && ctest --output-on-failure
```

* `lua_vm` — Lua-мост: `input.axis/pressed`, вызовы `on_start/on_update`,
  запись в transform. Ловит падения вида «PANIC: unprotected error in call to
  Lua API» (некорректный индекс стека в C-функции → `abort()`).
* `gltf_loader` — загрузка `.gltf`/`.glb`, встраивание трансформа узла,
  bounds, аккуратная ошибка на отсутствующем файле.
* `png_decoder` — собственный inflate (RFC 1950/1951) без zlib: декодирование
  PNG и пересжатие ICON0 до 144x80. Без него Windows-сборка требует системный
  zlib, а на PSP уезжали бы чёрные иконки.

## Проверено

* `bin/Editor/PSPEditor` запускается под Xvfb, все панели и ImGuizmo работают.
* `bin/PSPRuntime/EBOOT.PBP` — сцена + HUD в PPSSPP (скриншот
  `screenshots/psp_make_build_eboot.png`).
* `projects/MyPSPGame/build/pkg/EBOOT.PBP`, собранный **из GUI редактора** —
  сцена проекта + работающий Lua-скрипт (скриншот
  `screenshots/psp_gui_build_eboot.png`).
* glTF-модели (.gltf и .glb) рисуются и в редакторе, и на PSP:
  `screenshots/editor_gltf_models.png`, `screenshots/psp_gltf_models.png`.
* Модальное окно автоустановки PSPSDK: `screenshots/pspsdk_first_run_offer.png`,
  процесс установки: `screenshots/pspsdk_install_progress.png`.
* Сборка EBOOT через **только что установленный** тулчейн (без `$PSPDEV`)
  проверена в PPSSPP — сцена с моделями рендерится.
* **Чистый клон:** EBOOT, собранный без готового `libpsplua.a`, загружается в
  PPSSPP — `FPS 62 OBJ 10 LUA 1` (`screenshots/psp_clean_checkout_build.png`).
  CI-EBOOT при этом побайтно совпадает с локальным (`md5 717d457c…`).
* **GitHub Actions:** все 4 job'а зелёные (Windows MSVC, Windows MinGW,
  Linux, PSP runtime); артефакты `PSPEditor.exe` запускаются без
  Redistributable.
