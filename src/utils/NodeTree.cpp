#include "utils/NodeTree.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/cocos.hpp>

#include <functional>

namespace nodetree {

void collectSprites(cocos2d::CCNode* root, std::vector<cocos2d::CCNode*> const& skips,
    std::vector<cocos2d::CCSprite*>& out, bool includeRoot) {
    std::function<void(cocos2d::CCNode*)> rec = [&](cocos2d::CCNode* node) {
        if (!node) return;
        for (auto* sk : skips) {
            if (node == sk) return;
        }
        if (includeRoot || node != root) {
            if (auto* spr = geode::cast::typeinfo_cast<cocos2d::CCSprite*>(node)) {
                out.push_back(spr);
            }
        }
        if (auto* children = node->getChildren()) {
            for (unsigned int i = 0; i < children->count(); ++i) {
                rec(static_cast<cocos2d::CCNode*>(children->objectAtIndex(i)));
            }
        }
    };
    rec(root);
}

}
