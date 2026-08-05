#pragma once


#include "export/ObjectCollection.hpp"

#include <Geode/Geode.hpp>
#include <cocos2d.h>

#include <set>
#include <vector>

struct AdditiveParts {
    std::set<cocos2d::CCSprite*> sprites;
    std::vector<cocos2d::CCParticleSystemQuad*> particles;
    std::vector<cocos2d::CCNode*> gradients;
};

struct BorrowedNode {
    geode::Ref<cocos2d::CCNode> node;
    cocos2d::CCNode* parent;
    cocos2d::CCPoint position;
    int zOrder;
};

struct BorrowGuard {
    std::vector<BorrowedNode>& nodes;
    ~BorrowGuard();
};

cocos2d::CCSprite* buildExportSprite(std::vector<ObjectData> const& originalData, LevelEditorLayer* lel,
    std::vector<BorrowedNode>& borrowedNodes, AdditiveParts& additive);
