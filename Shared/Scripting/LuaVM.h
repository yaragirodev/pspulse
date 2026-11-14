#pragma once

// ---------------------------------------------------------------------------
//  PSPulse - embedded Lua 5.4 scripting (shared editor + PSP runtime)
//
//  Lua API exposed to every script (the calling GameObject is `self`):
//
//      function on_start()            -- called once
//      function on_update(dt)         -- called every frame
//
//      position() -> x, y, z          set_position(x, y, z)
//      rotation() -> rx, ry, rz       set_rotation(rx, ry, rz)   (degrees)
//      scale()    -> sx, sy, sz       set_scale(sx, sy, sz)
//      velocity() -> x, y, z          set_velocity(x, y, z)
//      on_ground() -> boolean
//      name()      -> string
//      dt()        -> last delta time, time() -> seconds since start
//
//      input.down("CROSS")            held this frame
//      input.pressed("CROSS")         went down this frame
//      input.axis("X") -> -1..1       analog nub (X / Y)
//      input.down("UP"|"DOWN"|"LEFT"|"RIGHT"|"CROSS"|"CIRCLE"|"SQUARE"|
//                 "TRIANGLE"|"LTRIGGER"|"RTRIGGER"|"START"|"SELECT")
//
//      print(...)                     routed to the editor Console / PSP debug
// ---------------------------------------------------------------------------

#include <functional>
#include <string>
#include <vector>

#include "../Scene/SceneData.h"

struct lua_State;

namespace pspulse {

enum class InputButton {
    Up = 0,
    Down,
    Left,
    Right,
    Cross,
    Circle,
    Square,
    Triangle,
    LTrigger,
    RTrigger,
    Start,
    Select,
    Count
};

struct InputState {
    bool held[static_cast<int>(InputButton::Count)] = {};
    bool pressed[static_cast<int>(InputButton::Count)] = {};
    float analogX = 0.0f;
    float analogY = 0.0f;

    void Clear() {
        for (int i = 0; i < static_cast<int>(InputButton::Count); ++i) held[i] = pressed[i] = false;
        analogX = analogY = 0.0f;
    }
};

const char* InputButtonName(InputButton button);
bool InputButtonFromName(const std::string& name, InputButton& out);

class LuaVM {
public:
    using LogFunction = std::function<void(const std::string&)>;

    LuaVM() = default;
    ~LuaVM();

    LuaVM(const LuaVM&) = delete;
    LuaVM& operator=(const LuaVM&) = delete;

    void SetLogger(LogFunction logger) { logger_ = std::move(logger); }
    void Log(const std::string& message) const;

    bool Init(std::string& error);
    void Shutdown();

    // Registers a script for an object. `source` is the Lua chunk text.
    bool AttachScript(Scene& scene, GameObject& object, const std::string& source,
                      const std::string& chunkName, std::string& error);

    // Drops all registered scripts (e.g. on scene reload / Play mode stop).
    void ClearScripts();

    void StartAll(Scene& scene);
    void UpdateAll(Scene& scene, float dt);

    void SetInput(const InputState& state) { input_ = state; }
    const InputState& GetInput() const { return input_; }

    int ScriptCount() const { return static_cast<int>(scripts_.size()); }
    bool HasErrors() const { return !lastError_.empty(); }
    const std::string& LastError() const { return lastError_; }

private:
    struct ScriptInstance {
        int objectId = -1;
        int envRef = -2;       // LUA_NOREF
        int updateRef = -2;    // LUA_NOREF
        int startRef = -2;     // LUA_NOREF
        std::string path;
        bool started = false;
        bool failed = false;
    };

    void PushObjectEnvironment(Scene& scene, GameObject& object);
    static ScriptInstance* FindByObject(std::vector<ScriptInstance>& list, int objectId);

    lua_State* L_ = nullptr;
    std::vector<ScriptInstance> scripts_;
    InputState input_;
    LogFunction logger_;
    std::string lastError_;
    float elapsed_ = 0.0f;
    float lastDt_ = 0.0f;
};

} // namespace pspulse
