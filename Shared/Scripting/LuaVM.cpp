// ---------------------------------------------------------------------------
//  PSP Engine - embedded Lua 5.4 scripting implementation
// ---------------------------------------------------------------------------

#include "LuaVM.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace pspengine {

// --------------------------------------------------------------------------
// Script environment layout
//
// Every script runs inside its own environment table (sandbox). Fields:
//   __scene   -> light userdata (Scene*)
//   __objectId-> integer id of the owning GameObject
//   __vm      -> light userdata (LuaVM*)
// The environment table is also used as the script's global table via
// setfenv-style _ENV upvalue replacement.
// --------------------------------------------------------------------------

namespace {

constexpr const char* kSceneKey = "__scene";
constexpr const char* kObjectKey = "__objectId";
constexpr const char* kVmKey = "__vm";

Scene* GetScene(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), kSceneKey);
    Scene* scene = static_cast<Scene*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return scene;
}

int GetObjectId(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), kObjectKey);
    int id = static_cast<int>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    return id;
}

LuaVM* GetVM(lua_State* L) {
    lua_getfield(L, lua_upvalueindex(1), kVmKey);
    LuaVM* vm = static_cast<LuaVM*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return vm;
}

GameObject* GetSelf(lua_State* L) {
    Scene* scene = GetScene(L);
    if (!scene) return nullptr;
    return scene->Find(GetObjectId(L));
}

// -------------------------------------------------------------------------
// Transform accessors
// -------------------------------------------------------------------------
int LuaGetPosition(lua_State* L) {
    GameObject* self = GetSelf(L);
    glm::vec3 p = self ? self->transform.position : glm::vec3(0.0f);
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    lua_pushnumber(L, p.z);
    return 3;
}

int LuaSetPosition(lua_State* L) {
    GameObject* self = GetSelf(L);
    if (!self) return 0;
    float x = static_cast<float>(luaL_optnumber(L, 1, self->transform.position.x));
    float y = static_cast<float>(luaL_optnumber(L, 2, self->transform.position.y));
    float z = static_cast<float>(luaL_optnumber(L, 3, self->transform.position.z));
    self->transform.position = glm::vec3(x, y, z);
    return 0;
}

int LuaGetRotation(lua_State* L) {
    GameObject* self = GetSelf(L);
    glm::vec3 r = self ? self->transform.rotation : glm::vec3(0.0f);
    lua_pushnumber(L, r.x);
    lua_pushnumber(L, r.y);
    lua_pushnumber(L, r.z);
    return 3;
}

int LuaSetRotation(lua_State* L) {
    GameObject* self = GetSelf(L);
    if (!self) return 0;
    float x = static_cast<float>(luaL_optnumber(L, 1, self->transform.rotation.x));
    float y = static_cast<float>(luaL_optnumber(L, 2, self->transform.rotation.y));
    float z = static_cast<float>(luaL_optnumber(L, 3, self->transform.rotation.z));
    self->transform.rotation = glm::vec3(x, y, z);
    return 0;
}

int LuaGetScale(lua_State* L) {
    GameObject* self = GetSelf(L);
    glm::vec3 s = self ? self->transform.scale : glm::vec3(1.0f);
    lua_pushnumber(L, s.x);
    lua_pushnumber(L, s.y);
    lua_pushnumber(L, s.z);
    return 3;
}

int LuaSetScale(lua_State* L) {
    GameObject* self = GetSelf(L);
    if (!self) return 0;
    float x = static_cast<float>(luaL_optnumber(L, 1, self->transform.scale.x));
    float y = static_cast<float>(luaL_optnumber(L, 2, self->transform.scale.y));
    float z = static_cast<float>(luaL_optnumber(L, 3, self->transform.scale.z));
    self->transform.scale = glm::vec3(x, y, z);
    return 0;
}

int LuaGetVelocity(lua_State* L) {
    GameObject* self = GetSelf(L);
    glm::vec3 v = self ? self->physics.velocity : glm::vec3(0.0f);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    return 3;
}

int LuaSetVelocity(lua_State* L) {
    GameObject* self = GetSelf(L);
    if (!self) return 0;
    float x = static_cast<float>(luaL_optnumber(L, 1, self->physics.velocity.x));
    float y = static_cast<float>(luaL_optnumber(L, 2, self->physics.velocity.y));
    float z = static_cast<float>(luaL_optnumber(L, 3, self->physics.velocity.z));
    self->physics.velocity = glm::vec3(x, y, z);
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

// -------------------------------------------------------------------------
// Input
// -------------------------------------------------------------------------
int LuaInputDown(lua_State* L) {
    LuaVM* vm = GetVM(L);
    InputButton button;
    if (!vm || !InputButtonFromName(luaL_checkstring(L, 1), button)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, vm->GetInput().held[static_cast<int>(button)]);
    return 1;
}

int LuaInputPressed(lua_State* L) {
    LuaVM* vm = GetVM(L);
    InputButton button;
    if (!vm || !InputButtonFromName(luaL_checkstring(L, 1), button)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, vm->GetInput().pressed[static_cast<int>(button)]);
    return 1;
}

int LuaInputAxis(lua_State* L) {
    LuaVM* vm = GetVM(L);
    const char* axis = luaL_optstring(L, 1, "X");
    if (!vm) {
        lua_pushnumber(L, 0.0);
        return 1;
    }
    lua_pushnumber(L, (axis[0] == 'Y' || axis[0] == 'y') ? vm->GetInput().analogY
                                                         : vm->GetInput().analogX);
    return 1;
}

int LuaPrint(lua_State* L) {
    LuaVM* vm = GetVM(L);
    int n = lua_gettop(L);
    std::string message;
    for (int i = 1; i <= n; ++i) {
        size_t len = 0;
        const char* text = luaL_tolstring(L, i, &len);
        if (i > 1) message += "\t";
        message.append(text, len);
        lua_pop(L, 1);
    }
    if (vm) vm->Log("[Lua] " + message);
    return 0;
}

void RunLuaChunk(lua_State* L, const char* code, const char* chunkName) {
    if (luaL_dostring(L, code) != LUA_OK) {
        lua_pop(L, 1);
    }
}

// Builds the shared `input` table inside the given table at the top of stack.
void PushInputTable(lua_State* L) {
    lua_newtable(L);
    lua_pushvalue(L, -2); // env as upvalue

    lua_pushcclosure(L, LuaInputDown, 1);
    lua_setfield(L, -2, "down");
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, LuaInputPressed, 1);
    lua_setfield(L, -2, "pressed");
    lua_pushvalue(L, -2);
    lua_pushcclosure(L, LuaInputAxis, 1);
    lua_setfield(L, -2, "axis");
    // Stack is [env, input]; publish env.input = input (pops the table).
    // NOTE: the index must be -2. A wrong (out-of-stack) index inside a
    // non-protected Lua C function does not return an error - Lua calls
    // luaG_typeerror -> abort(), which killed the editor on Play.
    lua_setfield(L, -2, "input");
}

} // namespace

// --------------------------------------------------------------------------
// InputButton helpers
// --------------------------------------------------------------------------

const char* InputButtonName(InputButton button) {
    static const char* names[] = {"UP",     "DOWN",   "LEFT",    "RIGHT", "CROSS", "CIRCLE",
                                  "SQUARE", "TRIANGLE", "LTRIGGER", "RTRIGGER", "START", "SELECT"};
    int index = static_cast<int>(button);
    if (index < 0 || index >= static_cast<int>(InputButton::Count)) return "?";
    return names[index];
}

bool InputButtonFromName(const std::string& name, InputButton& out) {
    for (int i = 0; i < static_cast<int>(InputButton::Count); ++i) {
        if (name == InputButtonName(static_cast<InputButton>(i))) {
            out = static_cast<InputButton>(i);
            return true;
        }
    }
    return false;
}

// --------------------------------------------------------------------------
// LuaVM
// --------------------------------------------------------------------------

LuaVM::~LuaVM() { Shutdown(); }

void LuaVM::Log(const std::string& message) const {
    if (logger_) logger_(message);
}

bool LuaVM::Init(std::string& error) {
    error.clear();
    if (L_) return true;
    L_ = luaL_newstate();
    if (!L_) {
        error = "luaL_newstate failed (out of memory?)";
        return false;
    }
    luaL_openlibs(L_);
    scripts_.clear();
    elapsed_ = 0.0f;
    lastDt_ = 0.0f;
    return true;
}

void LuaVM::Shutdown() {
    if (!L_) return;
    for (auto& script : scripts_) {
        if (script.envRef != LUA_NOREF) luaL_unref(L_, LUA_REGISTRYINDEX, script.envRef);
    }
    scripts_.clear();
    lua_close(L_);
    L_ = nullptr;
}

void LuaVM::ClearScripts() {
    if (!L_) return;
    for (auto& script : scripts_) {
        if (script.envRef != LUA_NOREF) luaL_unref(L_, LUA_REGISTRYINDEX, script.envRef);
    }
    scripts_.clear();
    lastError_.clear();
    elapsed_ = 0.0f;
}

LuaVM::ScriptInstance* LuaVM::FindByObject(std::vector<ScriptInstance>& list, int objectId) {
    for (auto& instance : list) {
        if (instance.objectId == objectId) return &instance;
    }
    return nullptr;
}

void LuaVM::PushObjectEnvironment(Scene& scene, GameObject& object) {
    lua_State* L = L_;
    lua_newtable(L); // env table

    lua_pushlightuserdata(L, &scene);
    lua_setfield(L, -2, kSceneKey);
    lua_pushinteger(L, object.id);
    lua_setfield(L, -2, kObjectKey);
    lua_pushlightuserdata(L, this);
    lua_setfield(L, -2, kVmKey);
    lua_pushnumber(L, 0.0);
    lua_setfield(L, -2, "__elapsed");
    lua_pushnumber(L, 0.0);
    lua_setfield(L, -2, "__dt");

    // Every primitive takes the env table as an upvalue so it can find the
    // owning object / scene / VM without globals.
    struct Binding {
        const char* name;
        lua_CFunction function;
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
        lua_pushvalue(L, -1); // env copy for upvalue
        lua_pushcclosure(L, binding.function, 1);
        lua_setfield(L, -2, binding.name);
    }

    // input table shares the same upvalue.
    PushInputTable(L);

    // standard library references the scripts commonly rely on
    lua_getglobal(L, "math");
    lua_setfield(L, -2, "math");
    lua_getglobal(L, "string");
    lua_setfield(L, -2, "string");
    lua_getglobal(L, "table");
    lua_setfield(L, -2, "table");
    lua_getglobal(L, "os");
    lua_setfield(L, -2, "os");
}

bool LuaVM::AttachScript(Scene& scene, GameObject& object, const std::string& source,
                         const std::string& chunkName, std::string& error) {
    error.clear();
    if (!L_) {
        error = "Lua VM is not initialised.";
        return false;
    }

    if (ScriptInstance* existing = FindByObject(scripts_, object.id)) {
        if (existing->envRef != LUA_NOREF) luaL_unref(L_, LUA_REGISTRYINDEX, existing->envRef);
        scripts_.erase(scripts_.begin() + (existing - scripts_.data()));
    }

    PushObjectEnvironment(scene, object);
    int envRef = luaL_ref(L_, LUA_REGISTRYINDEX); // pops env table

    // Compile the chunk with the environment table as its _ENV upvalue,
    // exactly like the classic setfenv() sandbox (Lua 5.4 style).
    lua_rawgeti(L_, LUA_REGISTRYINDEX, envRef);           // [env]
    lua_getglobal(L_, "load");                            // [env, load]
    lua_pushlstring(L_, source.c_str(), source.size());   // [env, load, src]
    lua_pushstring(L_, chunkName.c_str());                // [env, load, src, name]
    lua_pushliteral(L_, "t");                             // [env, load, src, name, mode]
    lua_pushvalue(L_, -5);                                // [env, load, src, name, mode, env]
    if (lua_pcall(L_, 4, 1, 0) != LUA_OK) {               // [env, function?]
        const char* message = lua_tostring(L_, -1);
        error = std::string("load failed: ") + (message ? message : "unknown error");
        lua_pop(L_, 2);
        luaL_unref(L_, LUA_REGISTRYINDEX, envRef);
        return false;
    }
    if (!lua_isfunction(L_, -1)) {
        error = "load did not return a function (source is not valid Lua?)";
        lua_pop(L_, 2);
        luaL_unref(L_, LUA_REGISTRYINDEX, envRef);
        return false;
    }
    if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {               // [env]
        const char* message = lua_tostring(L_, -1);
        error = std::string("script error: ") + (message ? message : "unknown error");
        lua_pop(L_, 2);
        luaL_unref(L_, LUA_REGISTRYINDEX, envRef);
        return false;
    }
    lua_pop(L_, 1); // env

    ScriptInstance instance;
    instance.objectId = object.id;
    instance.envRef = envRef;
    instance.path = chunkName;

    // Fetch callbacks.
    lua_rawgeti(L_, LUA_REGISTRYINDEX, envRef);
    lua_getfield(L_, -1, "on_update");
    if (lua_isfunction(L_, -1)) {
        instance.updateRef = luaL_ref(L_, LUA_REGISTRYINDEX);
    } else {
        instance.updateRef = LUA_NOREF;
        lua_pop(L_, 1);
    }
    lua_getfield(L_, -1, "on_start");
    if (lua_isfunction(L_, -1)) {
        instance.startRef = luaL_ref(L_, LUA_REGISTRYINDEX);
    } else {
        instance.startRef = LUA_NOREF;
        lua_pop(L_, 1);
    }
    lua_pop(L_, 1); // env

    scripts_.push_back(instance);
    return true;
}

void LuaVM::StartAll(Scene& scene) {
    if (!L_) return;
    for (auto& script : scripts_) {
        if (script.started || script.failed) continue;
        if (script.startRef == LUA_NOREF) {
            script.started = true;
            continue;
        }
        lua_rawgeti(L_, LUA_REGISTRYINDEX, script.startRef);
        if (lua_pcall(L_, 0, 0, 0) != LUA_OK) {
            const char* message = lua_tostring(L_, -1);
            lastError_ = std::string("on_start error: ") + (message ? message : "?");
            Log(lastError_);
            script.failed = true;
            lua_pop(L_, 1);
            continue;
        }
        script.started = true;
    }
    (void)scene;
}

void LuaVM::UpdateAll(Scene& scene, float dt) {
    if (!L_) return;
    lastDt_ = dt;
    elapsed_ += dt;

    // Refresh per-script timing globals.
    for (auto& script : scripts_) {
        lua_rawgeti(L_, LUA_REGISTRYINDEX, script.envRef);
        lua_pushnumber(L_, static_cast<lua_Number>(elapsed_));
        lua_setfield(L_, -2, "__elapsed");
        lua_pushnumber(L_, static_cast<lua_Number>(dt));
        lua_setfield(L_, -2, "__dt");
        lua_pop(L_, 1);
    }

    StartAll(scene);

    int failures = 0;
    for (auto& script : scripts_) {
        if (script.failed || script.updateRef == LUA_NOREF) continue;
        lua_rawgeti(L_, LUA_REGISTRYINDEX, script.updateRef);
        lua_pushnumber(L_, static_cast<lua_Number>(dt));
        if (lua_pcall(L_, 1, 0, 0) != LUA_OK) {
            const char* message = lua_tostring(L_, -1);
            lastError_ = std::string("on_update error: ") + (message ? message : "?");
            Log(lastError_);
            lua_pop(L_, 1);
            script.failed = true;
            ++failures;
        }
    }
    if (failures == 0 && lastError_.rfind("on_update", 0) == 0) lastError_.clear();
}

} // namespace pspengine
