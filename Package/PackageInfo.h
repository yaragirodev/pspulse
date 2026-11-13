#pragma once

// ---------------------------------------------------------------------------
//  PSP Engine - game package description (Build Settings -> EBOOT.PBP)
//
//  Used by the editor (Build Settings panel + packaging pipeline) and by the
//  PSP runtime (compiled into the EBOOT so it can read its own metadata).
// ---------------------------------------------------------------------------

#include <string>

#include <nlohmann/json.hpp>

namespace pspengine {

struct PackageInfo {
    // XMB metadata.
    std::string title = "PSP Engine Game";
    std::string titleId = "PSPENG000"; // 4..9 chars, [A-Z0-9]
    std::string developer = "PSP Engine";
    std::string version = "1.00";
    std::string parentalLevel = "1";

    // PNG/AT3 art (project-relative paths, may be empty).
    std::string icon0 = "assets/ICON0.PNG"; // 144x80
    std::string pic1 = "assets/PIC1.PNG";    // 480x272
    std::string snd0 = "";                   // assets/SND0.AT3

    // Scene & scripts.
    std::string scene = "scenes/main.scene";
    std::string startupScript = "";

    // Packaging.
    std::string outputDir = "build/pkg";
    bool embedAssets = true;

    std::string ToJson() const {
        nlohmann::json j;
        j["title"] = title;
        j["titleId"] = titleId;
        j["developer"] = developer;
        j["version"] = version;
        j["parentalLevel"] = parentalLevel;
        j["icon0"] = icon0;
        j["pic1"] = pic1;
        j["snd0"] = snd0;
        j["scene"] = scene;
        j["startupScript"] = startupScript;
        j["outputDir"] = outputDir;
        j["embedAssets"] = embedAssets;
        return j.dump(2);
    }

    static PackageInfo FromJson(const std::string& text) {
        PackageInfo info;
        nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
        if (j.is_discarded() || !j.is_object()) return info;
        auto str = [&](const char* key, std::string& out) {
            auto it = j.find(key);
            if (it != j.end() && it->is_string()) out = it->get<std::string>();
        };
        auto boolean = [&](const char* key, bool& out) {
            auto it = j.find(key);
            if (it != j.end() && it->is_boolean()) out = it->get<bool>();
        };
        str("title", info.title);
        str("titleId", info.titleId);
        str("developer", info.developer);
        str("version", info.version);
        str("parentalLevel", info.parentalLevel);
        str("icon0", info.icon0);
        str("pic1", info.pic1);
        str("snd0", info.snd0);
        str("scene", info.scene);
        str("startupScript", info.startupScript);
        str("outputDir", info.outputDir);
        boolean("embedAssets", info.embedAssets);
        return info;
    }
};

} // namespace pspengine
