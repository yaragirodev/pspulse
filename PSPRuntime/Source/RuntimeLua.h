#pragma once

// ---------------------------------------------------------------------------
//  PSP Runtime - Lua 5.4 scripting bridge
//
//  Mirrors Shared/Scripting/LuaVM.h but uses the statically linked
//  libpsplua.a that the build system produces from third_party/lua.
//
//  Script API (the calling game object is implicit):
//      on_start(), on_update(dt)
//      position()/set_position(), rotation()/set_rotation(),
//      scale()/set_scale(), velocity()/set_velocity(),
//      on_ground(), name(), time(), dt(), print(...)
//      input.down("CROSS"), input.pressed("CROSS"), input.axis("X")
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

#include "SceneRuntime.h"

struct lua_State;

namespace pspruntime {

struct PadState {
    bool up = false, down = false, left = false, right = false;
    bool cross = false, circle = false, square = false, triangle = false;
    bool lTrigger = false, rTrigger = false;
    bool start = false, select = false;
    bool crossPressed = false, circlePressed = false, startPressed = false, selectPressed = false,
         lTriggerPressed = false, rTriggerPressed = false;
    float analogX = 0.0f;
    float analogY = 0.0f;
};

class RuntimeLua {
public:
    RuntimeLua();
    ~RuntimeLua();

    bool Init();
    void Shutdown();

    // Compiles and runs a script chunk bound to the object id.
    bool LoadScript(Scene& scene, int objectId, const std::string& source, const std::string& name,
                    std::string& error);

    void StartAll(Scene& scene);
    void UpdateAll(Scene& scene, float dt);

    void ClearScripts();
    void SetScene(Scene& scene) { scene_ = &scene; }

    void SetPad(const PadState& pad) { pad_ = pad; }
    const PadState& Pad() const { return pad_; }
    int ScriptCount() const { return static_cast<int>(scripts_.size()); }

    // Most recent messages from print() (ring buffer for the on-screen console).
    const char* LastMessage() const { return lastMessage_.c_str(); }
    const std::vector<std::string>& Messages() const { return messages_; }

    // Routes a script print() to the on-screen console ring buffer.
    void Log(const std::string& message);

private:
    struct Binding {
        int objectId = -1;
        int envRef = -2;
        int startRef = -2;
        int updateRef = -2;
        bool started = false;
        bool failed = false;
    };

    void PushEnvironment(Scene& scene, int objectId);

    lua_State* L_ = nullptr;
    Scene* scene_ = nullptr;
    std::vector<Binding> scripts_;
    PadState pad_;
    std::string lastMessage_;
    std::vector<std::string> messages_;
    float elapsed_ = 0.0f;
};

} // namespace pspruntime
