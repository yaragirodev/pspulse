// ---------------------------------------------------------------------------
//  PSP Runtime - Lua 5.4 scripting bridge implementation
// ---------------------------------------------------------------------------

#include "RuntimeLua.h"

#include <cstring>

#include <pspdebug.h>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace pspruntime {

namespace {

constexpr const char* kSceneKey = "__scene";
constexpr const char* kObjectKey = "__objectId";
constexpr const char* kLuaKey = "__lua";

Scene* GetScenePtr(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), kSceneKey);
    Scene* scene = static_cast<Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return scene;
}

GameObject* GetSelf(lua_State* L) {
    Scene* scene = GetScenePtr(L);
    if (!scene) return nullptr;
    lua_getfield(L, lua_upvalueindex(1), kObjectKey);
    int id = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    return scene->Find(id);
}

RuntimeLua* GetLua(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), kLuaKey);
    RuntimeLua* lua = static_cast<RuntimeLua*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return lua;
}

int LuaGetPosition(lua_State* L) {
    GameObject* self = GetSelf(L);
    Vec3 p = self ? self->transform.position : Vec3(0, 0, 0);
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    lua_pushnumber(L, p.z);
    return 3;
}

int LuaSetPosition(lua_State* L) {
    GameObject* self = GetSelf(L);
    if (!self) return 0;
    self->transform.position = Vec3(static_cast<float>(luaL_optnumber(L, 1, self->transform.position.x)),
                                    static_cast<float>(luaL_optnumber(L, 2, self->transform.position.y)),
                                    static_cast<float>(luaL_optnumber(L, 3, self->transform.position.z)));
    return 0;
}

int LuaGetRotation(lua_State* L) {
    GameObject* self = GetSelf(L);
    Vec3 r = self ? self->transform.rotation : Vec3(0, 0, 0);
    lua_pushnumber(L, r.x);
    lua_pushnumber(L, r.y);
    lua_pushnumber(L, r.z);
    return 3;
}

int LuaSetRotation(lua_State* L) {
    GameObject* self = GetSelf(L);
    if (!self) return 0;
    self->transform.rotation = Vec3(static_cast<float>(luaL_optnumber(L, 1, self->transform.rotation.x)),
                                    static_cast<float>(luaL_optnumber(L, 2, self->transform.rotation.y)),
                                    static_cast<float>(luaL_optnumber(L, 3, self->transform.rotation.z)));
    return 0;
}

int LuaGetScale(lua_State* L) {
    GameObject* self = GetSelf(L);
    Vec3 s = self ? self->transform.scale : Vec3(1, 1, 1);
    lua_pushnumber(L, s.x);
    lua_pushnumber(L, s.y);
    lua_pushnumber(L, s.z);
    return 3;
}

int LuaSetScale(lua_State* L) {
    GameObject* self = GetSelf(L);
    if (!self) return 0;
    self->transform.scale = Vec3(static_cast<float>(luaL_optnumber(L, 1, self->transform.scale.x)),
                                 static_cast<float>(luaL_optnumber(L, 2, self->transform.scale.y)),
                                 static_cast<float>(luaL_optnumber(L, 3, self->transform.scale.z)));
    return 0;
}

int LuaGetVelocity(lua_State* L) {
    GameObject* self = GetSelf(L);
    Vec3 v = self ? self->physics.velocity : Vec3(0, 0, 0);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    return 3;
}

int LuaSetVelocity(lua_State* L) {
    GameObject* self = GetSelf(L);
    if (!self) return 0;
    self->physics.velocity = Vec3(static_cast<float>(luaL_optnumber(L, 1, self->physics.velocity.x)),
                                  static_cast<float>(luaL_optnumber(L, 2, self->physics.velocity.y)),
                                  static_cast<float>(luaL_optnumber(L, 3, self->physics.velocity.z)));
    return 0;
}

int LuaOnGround(lua_State* L) {
    GameObject* self = GetSelf(L);
    lua_pushboolean(L, self && self->physics.grounded);
    return 1;
}

int LuaGetName(lua_State* L) {
    GameObject* self = GetSelf(L);
    lua_pushstring(L, self ? self->name.c_str() : "");
    return 1;
}

int LuaTime(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), "__elapsed");
    lua_pushnumber(L, lua_tonumber(L, -1));
    lua_remove(L, -2);
    return 1;
}

int LuaDeltaTime(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), "__dt");
    lua_pushnumber(L, lua_tonumber(L, -1));
    lua_remove(L, -2);
    return 1;
}

int LuaPrint(lua_State* L) {
    RuntimeLua* runtime = GetLua(L);
    int n = lua_gettop(L);
    std::string text;
    for (int i = 1; i <= n; ++i) {
        size_t len = 0;
        const char* str = luaL_tolstring(L, i, &len);
        if (i > 1) text += "\t";
        text.append(str, len);
        lua_pop(L, 1);
    }
    if (runtime) runtime->Log(text);
    return 0;
}

int LuaInputDown(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), kLuaKey);
    RuntimeLua* runtime = static_cast<RuntimeLua*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    const char* name = luaL_checkstring(L, 1);
    bool down = false;
    if (runtime) {
        PadState pad = runtime->Pad();
        if (std::strcmp(name, "CROSS") == 0) down = pad.cross;
        else if (std::strcmp(name, "CIRCLE") == 0) down = pad.circle;
        else if (std::strcmp(name, "SQUARE") == 0) down = pad.square;
        else if (std::strcmp(name, "TRIANGLE") == 0) down = pad.triangle;
        else if (std::strcmp(name, "UP") == 0) down = pad.up;
        else if (std::strcmp(name, "DOWN") == 0) down = pad.down;
        else if (std::strcmp(name, "LEFT") == 0) down = pad.left;
        else if (std::strcmp(name, "RIGHT") == 0) down = pad.right;
        else if (std::strcmp(name, "LTRIGGER") == 0) down = pad.lTrigger;
        else if (std::strcmp(name, "RTRIGGER") == 0) down = pad.rTrigger;
        else if (std::strcmp(name, "START") == 0) down = pad.start;
        else if (std::strcmp(name, "SELECT") == 0) down = pad.select;
    }
    lua_pushboolean(L, down);
    return 1;
}

int LuaInputPressed(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), kLuaKey);
    RuntimeLua* runtime = static_cast<RuntimeLua*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    const char* name = luaL_checkstring(L, 1);
    bool pressed = false;
    if (runtime) {
        PadState pad = runtime->Pad();
        if (std::strcmp(name, "CROSS") == 0) pressed = pad.crossPressed;
        else if (std::strcmp(name, "CIRCLE") == 0) pressed = pad.circlePressed;
        else if (std::strcmp(name, "START") == 0) pressed = pad.startPressed;
        else if (std::strcmp(name, "SELECT") == 0) pressed = pad.selectPressed;
        else if (std::strcmp(name, "LTRIGGER") == 0) pressed = pad.lTriggerPressed;
        else if (std::strcmp(name, "RTRIGGER") == 0) pressed = pad.rTriggerPressed;
        else if (std::strcmp(name, "UP") == 0) pressed = pad.up;
        else if (std::strcmp(name, "DOWN") == 0) pressed = pad.down;
        else if (std::strcmp(name, "LEFT") == 0) pressed = pad.left;
        else if (std::strcmp(name, "RIGHT") == 0) pressed = pad.right;
    }
    lua_pushboolean(L, pressed);
    return 1;
}

int LuaInputAxis(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), kLuaKey);
    RuntimeLua* runtime = static_cast<RuntimeLua*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    const char* axis = luaL_optstring(L, 1, "X");
    float value = 0.0f;
    if (runtime) {
        PadState pad = runtime->Pad();
        value = (axis[0] == 'Y' || axis[0] == 'y') ? pad.analogY : pad.analogX;
    }
    lua_pushnumber(L, value);
    return 1;
}

} // namespace

RuntimeLua::RuntimeLua() = default;

RuntimeLua::~RuntimeLua() { Shutdown(); }

void RuntimeLua::Log(const std::string& message) {
    lastMessage_ = message;
    messages_.push_back(message);
    if (messages_.size() > 32) messages_.erase(messages_.begin());
}

bool RuntimeLua::Init() {
    if (L_) return true;
    L_ = luaL_newstate();
    if (!L_) return false;
    luaL_openlibs(L_);
    scripts_.clear();
    return true;
}

void RuntimeLua::Shutdown() {
    if (!L_) return;
    for (auto& script : scripts_) {
        if (script.envRef != LUA_NOREF) luaL_unref(L_, LUA_REGISTRYINDEX, script.envRef);
    }
    scripts_.clear();
    lua_close(L_);
    L_ = nullptr;
}

void RuntimeLua::ClearScripts() {
    if (!L_) {
        scripts_.clear();
        return;
    }
    for (auto& script : scripts_) {
        if (script.envRef != LUA_NOREF) luaL_unref(L_, LUA_REGISTRYINDEX, script.envRef);
    }
    scripts_.clear();
    elapsed_ = 0.0f;
}

void RuntimeLua::PushEnvironment(Scene& scene, int objectId) {
    lua_State* L = L_;
    scene_ = &scene;
    lua_newtable(L);
    lua_pushlightuserdata(L, &scene);
    lua_setfield(L, -2, kSceneKey);
    lua_pushinteger(L, objectId);
    lua_setfield(L, -2, kObjectKey);
    lua_pushlightuserdata(L, this);
    lua_setfield(L, -2, kLuaKey);
    lua_pushnumber(L, 0.0);
    lua_setfield(L, -2, "__elapsed");
    lua_pushnumber(L, 0.0);
    lua_setfield(L, -2, "__dt");

    struct Binding {
        const char* name;
        lua_CFunction fn;
    };
    const Binding bindings[] = {
        {"position", LuaGetPosition},     {"set_position", LuaSetPosition},
        {"rotation", LuaGetRotation},     {"set_rotation", LuaSetRotation},
        {"scale", LuaGetScale},           {"set_scale", LuaSetScale},
        {"velocity", LuaGetVelocity},     {"set_velocity", LuaSetVelocity},
        {"on_ground", LuaOnGround},       {"name", LuaGetName},
        {"time", LuaTime},                {"dt", LuaDeltaTime},
        {"print", LuaPrint},
    };
    for (const auto& binding : bindings) {
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, binding.fn, 1);
        lua_setfield(L, -2, binding.name);
    }

    // input table: built on the stack, then published as env.input.
    lua_newtable(L);
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, LuaInputDown, 1);
    lua_setfield(L, -2, "down");
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, LuaInputPressed, 1);
    lua_setfield(L, -2, "pressed");
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, LuaInputAxis, 1);
    lua_setfield(L, -2, "axis");
    lua_setfield(L, -2, "input"); // env.input = <the table just built>

    lua_getglobal(L, "math");
    lua_setfield(L, -2, "math");
    lua_getglobal(L, "string");
    lua_setfield(L, -2, "string");
    lua_getglobal(L, "table");
    lua_setfield(L, -2, "table");
}

bool RuntimeLua::LoadScript(Scene& scene, int objectId, const std::string& source,
                            const std::string& name, std::string& error) {
    if (!L_) {
        error = "Lua VM not initialised";
        return false;
    }
    PushEnvironment(scene, objectId);
    int envRef = luaL_ref(L_, LUA_REGISTRYINDEX);

    lua_rawgeti(L_, LUA_REGISTRYINDEX, envRef);
    lua_getglobal(L_, "load");
    lua_pushlstring(L_, source.c_str(), source.size());
    lua_pushstring(L_, name.c_str());
    lua_pushliteral(L_, "t");
    lua_pushvalue(L_, -5);
    if (lua_pcall(L_, 4, 1, 0) != LUA_OK) {
        const char* message = lua_tostring(L_, -1);
        error = std::string("load: ") + (message ? message : "?");
        lua_pop(L_, 2);
        luaL_unref(L_, LUA_REGISTRYINDEX, envRef);
        return false;
    }
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
        const char* message = lua_tostring(L_, -1);
        error = std::string("run: ") + (message ? message : "?");
        lua_pop(L_, 2);
        luaL_unref(L_, LUA_REGISTRYINDEX, envRef);
        return false;
    }
    lua_pop(L_, 1); // env

    Binding binding;
    binding.objectId = objectId;
    binding.envRef = envRef;
    lua_rawgeti(L_, LUA_REGISTRYINDEX, envRef);
    lua_getfield(L_, -1, "on_update");
    if (lua_isfunction(L_, -1)) {
        binding.updateRef = luaL_ref(L_, LUA_REGISTRYINDEX);
    } else {
        binding.updateRef = LUA_NOREF;
        lua_pop(L_, 1);
    }
    lua_getfield(L_, -1, "on_start");
    if (lua_isfunction(L_, -1)) {
        binding.startRef = luaL_ref(L_, LUA_REGISTRYINDEX);
    } else {
        binding.startRef = LUA_NOREF;
        lua_pop(L_, 1);
    }
    lua_pop(L_, 1);
    scripts_.push_back(binding);
    return true;
}

void RuntimeLua::StartAll(Scene& scene) {
    if (!L_) return;
    for (auto& script : scripts_) {
        if (script.started || script.failed) continue;
        script.started = true;
        if (script.startRef == LUA_NOREF) continue;
        lua_rawgeti(L_, LUA_REGISTRYINDEX, script.startRef);
        if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
            Log(std::string("on_start error: ") + (lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "?"));
            script.failed = true;
            lua_pop(L_, 1);
        }
    }
    (void)scene;
}

void RuntimeLua::UpdateAll(Scene& scene, float dt) {
    if (!L_) return;
    elapsed_ += dt;
    for (auto& script : scripts_) {
        lua_rawgeti(L_, LUA_REGISTRYINDEX, script.envRef);
        lua_pushnumber(L_, static_cast<lua_Number>(elapsed_));
        lua_setfield(L_, -2, "__elapsed");
        lua_pushnumber(L_, static_cast<lua_Number>(dt));
        lua_setfield(L_, -2, "__dt");
        lua_pop(L_, 1);
    }
    StartAll(scene);

    for (auto& script : scripts_) {
        if (script.failed || script.updateRef == LUA_NOREF) continue;
        lua_rawgeti(L_, LUA_REGISTRYINDEX, script.updateRef);
        lua_pushnumber(L_, static_cast<lua_Number>(dt));
        if (lua_pcall(L_, 1, 0, 0) != LUA_OK) {
            Log(std::string("on_update error: ") + (lua_tostring(L_, -1) ? lua_tostring(L_, -1) : "?"));
            script.failed = true;
            lua_pop(L_, 1);
        }
    }
}

} // namespace pspruntime
