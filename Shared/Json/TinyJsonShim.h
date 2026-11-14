#pragma once

// ---------------------------------------------------------------------------
//  PSPulse - small json facade used by the shared glTF loader on the PSP.
//
//  The editor links nlohmann/json; the handheld links this shim, which wraps the
//  dependency-free JSON DOM that already lives in the runtime (SceneRuntime.cpp,
//  including its parser). It implements just the subset the glTF loader needs,
//  so a single GltfLoader.cpp compiles for both targets.
// ---------------------------------------------------------------------------

#if !defined(PSP_ENGINE_TINY_JSON)
#error "TinyJsonShim.h is only used for the PSP build"
#endif

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "../../PSPRuntime/Source/SceneRuntime.h"

namespace tnjson {

using Value = pspruntime::JsonValue;

class basic_json {
public:
    basic_json() = default;
    basic_json(std::nullptr_t) {}
    basic_json(const basic_json&) = default;
    basic_json(basic_json&&) = default;
    basic_json& operator=(const basic_json&) = default;
    basic_json& operator=(basic_json&&) = default;

    // Factories mirroring the nlohmann calls used by the loader.
    static basic_json object() { return basic_json(Value::Object); }
    static basic_json array() { return basic_json(Value::Array); }

    static basic_json parse(const char* begin, const char* end, void* = nullptr, bool allowExceptions = true);
    static basic_json parse(const std::string& text, void* = nullptr, bool allowExceptions = true) {
        return parse(text.data(), text.data() + text.size(), nullptr, allowExceptions);
    }

    bool is_discarded() const { return discarded_; }
    bool is_object() const { return data.type == Value::Object; }
    bool is_array() const { return data.type == Value::Array; }

    bool contains(const char* key) const { return Get(key) != nullptr; }
    const Value* Get(const char* key) const {
        return data.type == Value::Object ? data.Get(key) : nullptr;
    }

    size_t size() const {
        if (data.type == Value::Array) return data.items.size();
        if (data.type == Value::Object) return data.values.size();
        return 0;
    }

    // Element access: json_object[key] / json_array[index].
    basic_json operator[](const char* key) const {
        basic_json result;
        if (const Value* child = Get(key)) result.data = *child;
        return result;
    }
    basic_json operator[](size_t index) const {
        basic_json result;
        if (data.type == Value::Array && index < data.items.size()) result.data = data.items[index];
        return result;
    }
    basic_json operator[](int index) const { return (*this)[static_cast<size_t>(index)]; }

    // Iteration over array elements (range-for over json).
    struct Iterator {
        const std::vector<Value>* items = nullptr;
        size_t index = 0;
        basic_json operator*() const {
            basic_json element;
            element.data = (*items)[index];
            return element;
        }
        Iterator& operator++() { ++index; return *this; }
        bool operator!=(const Iterator& other) const { return index != other.index; }
    };
    Iterator begin() const { return Iterator{&data.items, 0}; }
    Iterator end() const { return Iterator{&data.items, data.items.size()}; }

    // nlohmann-style typed getters used by the loader.
    template <typename T>
    T get() const;
    template <typename T>
    T value(const char* key, const T& fallback) const;
    std::string value(const char* key, const char* fallback) const;

    std::string get_string() const { return data.text; }

    Value data;

private:
    explicit basic_json(Value::Type type) { data.type = type; }
    bool discarded_ = false;
};

template <>
inline bool basic_json::get<bool>() const { return data.AsBool(false); }
template <>
inline int basic_json::get<int>() const { return static_cast<int>(data.AsNumber(0.0)); }
template <>
inline unsigned int basic_json::get<unsigned int>() const {
    return static_cast<unsigned int>(data.AsNumber(0.0));
}
template <>
inline float basic_json::get<float>() const { return static_cast<float>(data.AsNumber(0.0)); }
template <>
inline std::string basic_json::get<std::string>() const { return data.text; }

template <typename T>
inline T basic_json::value(const char* key, const T& fallback) const {
    const Value* child = Get(key);
    if (!child) return fallback;
    return static_cast<T>(child->AsNumber(0.0));
}

// Nested object access: document.value("asset", json::object()).
template <>
inline basic_json basic_json::value<basic_json>(const char* key, const basic_json& fallback) const {
    const Value* child = Get(key);
    if (!child) return fallback;
    basic_json result;
    result.data = *child;
    return result;
}

template <>
inline std::string basic_json::value<std::string>(const char* key, const std::string& fallback) const {
    const Value* child = Get(key);
    if (!child) return fallback;
    if (child->type == Value::String) return child->text;
    return fallback;
}

// const char* fallback (e.g. value("type", "SCALAR")) resolves to std::string.
inline std::string basic_json::value(const char* key, const char* fallback) const {
    return value<std::string>(key, std::string(fallback ? fallback : ""));
}

} // namespace tnjson

namespace nlohmann {
using json = tnjson::basic_json;
}

// ---------------------------------------------------------------------------
//  Implementation of the shim's parser entry point (header-only so the PSP
//  build does not need an extra translation unit).
// ---------------------------------------------------------------------------
namespace tnjson {
inline basic_json basic_json::parse(const char* begin, const char* end, void*, bool) {
    basic_json result;
    pspruntime::JsonValue parsed;
    std::string error;
    const std::string text(begin, end);
    if (!pspruntime::ParseJson(text, parsed, error)) {
        result.discarded_ = true;
        return result;
    }
    result.data = std::move(parsed);
    return result;
}
} // namespace tnjson
