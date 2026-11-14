# PSPulse — кроссплатформенный 3D-движок и редактор сцен для PlayStation Portable

[English](README.md) · **Русский**

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
                 Makefile (PSPSDK)
Shared/          Код, общий для редактора и рантайма: Scene (GameObject,
                 Component, Transform, PhysicsBody, CollisionShape), SceneIO
                 (JSON), Scripting/LuaVM, Mesh (примитивы), Physics
Shared/Mesh/     MeshGen (примитивы) + GltfLoader (glTF 2.0: .gltf/.glb)
Shared/Json/     TinyJsonShim — JSON-фасад для PSP-сборки glTF-загрузчика
Editor/Source/   ... + PspsdkInstaller (автоустановка тулчейна)
Tests/           headless-тесты: Lua-мост, glTF-загрузчик, PNG (ctest)
                 + Tests/data — фикстуры тестов
third_party/     glm, nlohmann_json, dear imgui (docking), ImGuizmo, glad,
                 stb, lua 5.4.6
projects/        Ваши проекты (создаются в редакторе)
bin/PSPulse/     PSPulse(.exe)
bin/PSPRuntime/  EBOOT.PBP (шаблон рантайма)
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
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Результат: `bin/PSPulse/PSPulse` (Linux) или `bin/PSPulse/PSPulse.exe`
(Windows, MSVC/MinGW). Запуск:

```bash
bin/PSPulse/PSPulse                       # пустое окно редактора
bin/PSPulse/PSPulse --project projects/<ваш-проект>
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
`libpsplua.a` собирается Makefile'ом из вендоренных исходников Lua, поэтому
чистый клон собирается без готовых артефактов.

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
| `build-windows` | MSVC (VS2022/VS2026, x64) | `PSPulse-windows-msvc` | ~2.2 МБ |
| `build-mingw` | MinGW-w64 | `PSPulse-windows-mingw` | ~5.9 МБ |

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
# bin/PSPulse/PSPulse.exe
```

### Headless-сборка игры (CI/скрипты)

```bash
./bin/PSPulse/PSPulse --project projects/<ваш-проект> --build
# projects/<ваш-проект>/build/pkg/EBOOT.PBP
```

Редактор сам находит `PSPRuntime/` рядом с бинарником, при отсутствии
`libpsplua.a` компилирует Lua 5.4 из `third_party/lua` — готовый архив для
чистого клона не нужен.

### Ограничение: PSPSDK под Windows

Prebuilt-тулчейн pspdev выпускается только под Linux (`pspdev-debian-latest.tar.gz`).
Под Windows он запускается через **WSL2** (проще всего) или MSYS2. Редактор
это учитывает: путь к `psp-gcc` можно указать вручную, а кнопка «Install
PSPSDK...» под Windows предложит WSL2.

## Релизы

Релиз собирается и публикуется автоматически (`.github/workflows/release.yml`):
каждый артефакт компилируется из исходников в этом же прогоне, ничего не
загружается вручную.

```bash
git tag v1.0.1 && git push origin v1.0.1      # релиз v1.0.1
```

Либо вручную: **Actions → Release → Run workflow** и указать версию
(без тега тоже работает). Публикуются только если все сборки прошли.

| Файл | Платформа |
|------|-----------|
| `PSPulse-<ver>-windows-x64.exe` | Windows x64, MSVC, статический CRT |
| `PSPulse-<ver>-windows-x64-mingw.exe` | Windows x64, MinGW |
| `PSPulse-<ver>-linux-x64` | Linux x64 (нужен glibc ≥ 2.34, т.е. Ubuntu 22.04+) |
| `PSPulse-<ver>-runtime-template.EBOOT.PBP` | PSP, шаблон рантайма |

Плюс `CHECKSUMS.txt` с SHA-256 каждого файла. Версия в бинарнике берётся из
git-тега (`PSPulse version: X.Y.Z` при конфигурации), поэтому версия в окне
редактора совпадает с релизом.

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

Разработка велась с реальными сборками и запусками, проверки ниже прогонялись
на каждом этапе:

* ПК-редактор запускается под Xvfb (программный OpenGL), все панели,
  докинг и ImGuizmo работают.
* `bin/PSPRuntime/EBOOT.PBP`, собранный через `make`, запускается в PPSSPP:
  сцена с сеткой, HUD и счётчиком FPS.
* EBOOT, собранный **из GUI редактора** (Build Settings → Build EBOOT.PBP) —
  сцена проекта плюс работающий Lua-скрипт.
* glTF-модели (`.gltf` и `.glb`) рисуются и в редакторе, и в PPSSPP.
* Модальное окно автоустановки PSPSDK, скачивание и распаковка тулчейна.
* Сборка EBOOT через **только что установленный** тулчейн (без `$PSPDEV`).
* **Чистый клон:** EBOOT собирается без готового `libpsplua.a` (редактор
  компилирует Lua сам) и загружается в PPSSPP с `LUA 1`; CI-EBOOT побайтно
  совпадает с локальным.
* **GitHub Actions:** Windows (MSVC + MinGW), Linux и PSP runtime — зелёные;
  Windows-бинарники запускаются без VC++ Redistributable.
* Тесты `lua_vm`, `gltf_loader`, `png_decoder` проходят (`ctest`).
