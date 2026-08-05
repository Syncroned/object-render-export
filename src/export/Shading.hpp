#pragma once


#include <Geode/Geode.hpp>
#include <cocos2d.h>

void applyShadersGPU(geode::Ref<cocos2d::CCImage>& img, ShaderLayer* shaderLayer,
    cocos2d::CCArray* selectedObjects, cocos2d::CCRect const& worldBounds);
