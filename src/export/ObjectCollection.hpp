#pragma once


#include <Geode/Geode.hpp>
#include <cocos2d.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

std::vector<std::pair<int, int>> computeDrawKey(cocos2d::CCNode* node);

struct ObjectData {
    GameObject* obj;
    bool isVisible;
    GLubyte opacity;
    bool shouldBlendBase;
    bool shouldBlendDetail;
    int zLayer;
    int zOrder;
    int uniqueID;
    bool is22;
    std::vector<std::pair<int, int>> drawKey;
    std::string particleString;
    cocos2d::CCPoint particleOffset;
    cocos2d::CCPoint position;
    float scale;
    float rotation;
    bool particleUseObjectColor;
    cocos2d::ccColor3B objectColor;
    bool isParticleObject;
};

std::set<int> const& get22ObjectIDs();

std::pair<std::vector<ObjectData>, cocos2d::CCRect> collectObjectData(cocos2d::CCArray* selected,
    LevelEditorLayer* lel, std::set<int> const& ids22);
