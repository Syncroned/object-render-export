#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

CCLabelBMFont* addToggleRow(CCNode* layer, char const* text, float y, CCMenuItemToggler* toggle, CCMenuItemSpriteExtra* info = nullptr);
CCMenuItemSpriteExtra* makeIconButton(char const* frameName, float scale, CCObject* target, SEL_MenuHandler sel);
