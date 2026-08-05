#pragma once

#include <cocos2d.h>

#include <vector>

namespace nodetree {

void collectSprites(cocos2d::CCNode* root, std::vector<cocos2d::CCNode*> const& skips,
    std::vector<cocos2d::CCSprite*>& out, bool includeRoot = true);

}
