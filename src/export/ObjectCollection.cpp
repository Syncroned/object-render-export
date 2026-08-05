#include "export/ObjectCollection.hpp"

#include <Geode/Geode.hpp>
#include <matjson.hpp>

#include <algorithm>
#include <climits>
#include <fstream>

using namespace geode::prelude;
using namespace cocos2d;

namespace {

struct ObjEntry {
    GameObject* obj;
    int zLayer;
    int zOrder;
};

CCRect worldBounds(std::vector<ObjEntry> const& entries) {
    if (entries.empty()) return CCRectZero;

    float minX = 1e9f, minY = 1e9f;
    float maxX = -1e9f, maxY = -1e9f;

    for (auto& e : entries) {
        CCPoint pos = e.obj->getPosition();
        CCSize sz = e.obj->getContentSize();
        float scX = e.obj->getScaleX();
        float scY = e.obj->getScaleY();

        float hw = sz.width * scX * 0.5f;
        float hh = sz.height * scY * 0.5f;

        minX = std::min(minX, pos.x - hw);
        minY = std::min(minY, pos.y - hh);
        maxX = std::max(maxX, pos.x + hw);
        maxY = std::max(maxY, pos.y + hh);
    }

    return CCRect{minX, minY, maxX - minX, maxY - minY};
}

}

std::vector<std::pair<int, int>> computeDrawKey(CCNode* node) {
    std::vector<std::pair<int, int>> key;
    for (CCNode* n = node; n != nullptr; n = n->getParent()) {
        CCNode* p = n->getParent();
        int idx = 0;
        if (p && p->getChildren()) {
            unsigned int found = p->getChildren()->indexOfObject(n);
            idx = (found == UINT_MAX) ? 0 : static_cast<int>(found);
        }
        key.emplace_back(n->getZOrder(), idx);
    }
    std::reverse(key.begin(), key.end());
    return key;
}

std::set<int> const& get22ObjectIDs() {
    static std::set<int> ids;
    static bool loaded = false;
    if (loaded) return ids;

    auto jsonPath = Mod::get()->getResourcesDir() / "Compatibility.json";
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        geode::log::warn("Could not open Compatibility.json at {}", jsonPath.string());
        return ids;
    }
    auto result = matjson::Value::parse(file);
    if (result.isErr()) {
        geode::log::warn("Failed to parse Compatibility.json: {}",
            static_cast<std::string>(result.unwrapErr()));
        return ids;
    }
    auto& json = result.unwrap();
    auto objectsRes = json["objects"].asArray();
    if (objectsRes.isErr()) {
        geode::log::warn("Compatibility.json: no objects array");
        return ids;
    }
    for (const auto& obj : objectsRes.unwrap()) {
        auto idRes = obj["objectID"].as<int>();
        auto statusRes = obj["status"].as<std::string>();
        if (idRes.isOk() && statusRes.isOk() && statusRes.unwrap() == "Added in 2.2+") {
            ids.insert(idRes.unwrap());
        }
    }
    loaded = true;
    geode::log::info("Loaded {} 2.2+ object IDs from Compatibility.json", ids.size());
    return ids;
}

std::pair<std::vector<ObjectData>, CCRect> collectObjectData(CCArray* selected, LevelEditorLayer* lel,
    std::set<int> const& ids22) {
    std::vector<ObjectData> originalData;
    auto* effectManager = lel->m_effectManager;

    auto channelBlends = [&](int channelId) -> bool {
        if (!effectManager) return false;
        auto* ca = effectManager->getColorAction(channelId);
        return ca && ca->m_blending;
    };

    for (int i = 0; i < selected->count(); i++) {
        GameObject* obj = static_cast<GameObject*>(selected->objectAtIndex(i));
        ObjectData data;
        data.obj = obj;
        data.isVisible = obj->isVisible();
        data.opacity = obj->getOpacity();
        data.zLayer = obj->m_zLayer != ZLayer::Default
            ? (int)obj->m_zLayer : (int)obj->m_defaultZLayer;
        data.zOrder = obj->m_zOrder != 0 ? obj->m_zOrder : obj->m_defaultZOrder;
        data.uniqueID = obj->m_uniqueID;
        data.is22 = ids22.count(obj->m_objectID) > 0;
        data.drawKey = computeDrawKey(obj);
        data.position = obj->getPosition();
        data.scale = obj->getScale();
        data.rotation = obj->getRotation();

        int baseChan = obj->m_baseColor ? obj->m_baseColor->m_colorID : -1;
        int detailChan = obj->m_detailColor ? obj->m_detailColor->m_colorID : -1;
        data.shouldBlendBase = obj->m_shouldBlendBase ||
            (obj->m_baseColor && channelBlends(baseChan));
        data.shouldBlendDetail = obj->m_shouldBlendDetail ||
            (obj->m_detailColor && channelBlends(detailChan));

        data.particleUseObjectColor = obj->m_particleUseObjectColor;
        data.objectColor = {255, 255, 255};
        if (data.particleUseObjectColor && effectManager) {
            int chan = 0;
            if (obj->m_baseColor) {
                chan = obj->m_baseColor->m_colorID != 0
                    ? obj->m_baseColor->m_colorID : obj->m_baseColor->m_defaultColorID;
            } else if (obj->m_detailColor) {
                chan = obj->m_detailColor->m_colorID != 0
                    ? obj->m_detailColor->m_colorID : obj->m_detailColor->m_defaultColorID;
            }
            if (chan != 0) {
                data.objectColor = effectManager->activeColorForIndex(chan);
            }
        }

        if (!obj->m_particleString.empty()) {
            data.particleString = obj->m_particleString.c_str();
            data.particleOffset = obj->m_particleOffset;
        } else if (obj->m_particle) {
            data.particleString = GameToolbox::saveParticleToString(obj->m_particle).c_str();
            data.particleOffset = obj->m_particleOffset;
        }

        data.isParticleObject = !data.isVisible && !data.particleString.empty();

        originalData.push_back(data);
    }

    std::stable_sort(originalData.begin(), originalData.end(),
        [](ObjectData const& a, ObjectData const& b) {
            if (a.zLayer != b.zLayer) return a.zLayer < b.zLayer;
            if (a.drawKey != b.drawKey) return a.drawKey < b.drawKey;
            return a.uniqueID < b.uniqueID;
        });

    std::vector<ObjEntry> exportBoundsEntries;
    exportBoundsEntries.reserve(originalData.size());
    for (auto const& data : originalData) {
        exportBoundsEntries.push_back({data.obj, data.zLayer, data.zOrder});
    }
    CCRect exportWorldBounds = worldBounds(exportBoundsEntries);
    return {std::move(originalData), exportWorldBounds};
}
