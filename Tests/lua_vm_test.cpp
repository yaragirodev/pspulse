// ---------------------------------------------------------------------------
//  Headless regression test for the shared Lua bridge (no GL, no window).
//
//  Guards the class of bugs that used to abort the editor on "Play":
//    * env.input must exist and be callable (input.axis / input.pressed),
//    * transform accessors must round-trip,
//    * on_start / on_update must run without touching an invalid stack slot.
// ---------------------------------------------------------------------------

#include <cstdio>
#include <cstring>
#include <string>

#include "Shared/Scene/SceneData.h"
#include "Shared/Scripting/LuaVM.h"

using namespace pspulse;

namespace {

int gFailures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        std::printf("  FAIL: %s\n", what);
        ++gFailures;
    } else {
        std::printf("  ok:   %s\n", what);
    }
}

} // namespace

int main() {
    Scene scene;
    scene.name = "LuaVMTest";

    GameObject& cube = scene.AddObject("Cube", PrimitiveType::Cube);
    cube.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);

    LuaVM vm;
    std::string error;
    if (!vm.Init(error)) {
        std::printf("FAIL: LuaVM::Init -> %s\n", error.c_str());
        return 1;
    }

    InputState input;
    input.held[static_cast<int>(InputButton::Cross)] = true;
    input.pressed[static_cast<int>(InputButton::Cross)] = true;
    input.analogX = 0.5f;
    vm.SetInput(input);

    const char* script =
        "local pressed = 0\n"
        "function on_start()\n"
        "  set_position(4, 5, 6)\n"
        "end\n"
        "function on_update(dt)\n"
        "  local ax = input.axis('X')\n"
        "  if input.pressed('CROSS') then pressed = pressed + 1 end\n"
        "  local x, y, z = position()\n"
        "  set_position(x + ax * dt, y, z)\n"
        "end\n"
        "function get_pressed() return pressed end\n";

    std::printf("AttachScript:\n");
    bool attached = vm.AttachScript(scene, cube, script, "tests/input.lua", error);
    Check(attached, attached ? "script attached" : error.c_str());
    if (!attached) {
        vm.Shutdown();
        return 1;
    }

    vm.UpdateAll(scene, 0.5f); // runs on_start + on_update

    std::printf("Behaviour:\n");
    // on_start set the position, on_update moved x by axis*dt = 0.5*0.5 = 0.25
    Check(cube.transform.position.x > 4.2f && cube.transform.position.x < 4.3f,
          "on_start + on_update mutated the transform through the Lua bridge");
    Check(!vm.HasErrors(), vm.HasErrors() ? vm.LastError().c_str() : "no Lua errors reported");

    vm.UpdateAll(scene, 0.5f); // second tick: no crash, no double on_start
    Check(!vm.HasErrors(), vm.HasErrors() ? vm.LastError().c_str() : "second update tick is clean");

    vm.ClearScripts();
    vm.Shutdown();

    std::printf(gFailures == 0 ? "\nRESULT: PASS\n" : "\nRESULT: FAIL (%d)\n", gFailures);
    return gFailures == 0 ? 0 : 1;
}
