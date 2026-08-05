#include "ui/PopupWidgets.hpp"

using namespace geode::prelude;

CCLabelBMFont* addToggleRow(CCNode* layer, char const* text, float y, CCMenuItemToggler* toggle, CCMenuItemSpriteExtra* info) {
    auto* label = CCLabelBMFont::create(text, "bigFont.fnt");
    label->setScale(0.45f);
    label->setPosition({115.f, y});
    layer->addChild(label);

    auto* toggleMenu = CCMenu::create();
    toggleMenu->setPosition({205.f, y});
    toggleMenu->addChild(toggle);
    layer->addChild(toggleMenu);

    if (info) {
        auto* infoMenu = CCMenu::create();
        infoMenu->setPosition({270.f, y});
        infoMenu->addChild(info);
        layer->addChild(infoMenu);
    }

    return label;
}

CCMenuItemSpriteExtra* makeIconButton(char const* frameName, float scale, CCObject* target, SEL_MenuHandler sel) {
    auto* sprite = CCSprite::createWithSpriteFrameName(frameName);
    sprite->setScale(scale);
    return CCMenuItemSpriteExtra::create(sprite, target, sel);
}
