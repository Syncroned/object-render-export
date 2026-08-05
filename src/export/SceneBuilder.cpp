#include "export/SceneBuilder.hpp"

#include "utils/NodeTree.hpp"

#include <Geode/Geode.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>
#include <climits>
#include <sstream>
#include <utility>
#include <vector>

using namespace geode::prelude;
using namespace cocos2d;

namespace {

struct DrawItem {
    CCNode* node;
    CCSprite* sprite;
    ccBlendFunc func;
    bool useBlend;
    int zLayer;
    int group;
    int zOrder;
    int subOrder;
    std::vector<std::pair<int, int>> drawKey;
    bool isSprite;
    bool isParticle;
    bool isGradient;
};

constexpr ccBlendFunc c_additive = {GL_SRC_ALPHA, GL_ONE};

void setNormalBlend(CCSprite* s) {
    bool premultiplied = s->getTexture() && s->getTexture()->hasPremultipliedAlpha();
    s->setBlendFunc({static_cast<GLenum>(premultiplied ? GL_ONE : GL_SRC_ALPHA), GL_ONE_MINUS_SRC_ALPHA});
}

CCSprite* cloneObjects(std::vector<ObjectData> const& originalData, LevelEditorLayer* lel,
    AdditiveParts& additive, std::vector<DrawItem>& drawItems, bool& haveRef, CCPoint& refEditorPos,
    CCPoint& refLocalPos) {
    auto* ui = lel->m_editorUI;

    std::ostringstream buffer;
    for (const auto& data : originalData) {
        if (data.isVisible && !data.isParticleObject) {
            buffer << data.obj->getSaveString(lel).c_str() << ";";
        }
    }

    CCArray* objArray = CCArray::create();
    CCSprite* sprite = ui->spriteFromObjectString(buffer.str(), false, false, INT_MAX, objArray, nullptr, nullptr);
    lel->updateObjectColors(objArray);

    int visibleIndex = 0;
    for (const auto& data : originalData) {
        if (!data.isVisible || data.isParticleObject || visibleIndex >= objArray->count()) continue;

        GameObject* gameObject = static_cast<GameObject*>(objArray->objectAtIndex(visibleIndex));

        if (!haveRef) {
            CCNode* cloneParent = gameObject->getParent();
            CCPoint cloneWorld = cloneParent
                ? cloneParent->convertToWorldSpace(gameObject->getPosition())
                : gameObject->getPosition();
            refLocalPos = sprite->convertToNodeSpace(cloneWorld);
            refEditorPos = data.position;
            haveRef = true;
        }

        gameObject->setOpacity(data.opacity);

        bool baseBlend = data.shouldBlendBase;
        bool detailBlend = data.shouldBlendDetail;
        bool hasBaseColor = gameObject->m_baseColor != nullptr;
        bool hasDetailColor = gameObject->m_detailColor != nullptr;
        bool hasTwoChannels = hasBaseColor && hasDetailColor
            && gameObject->m_colorSprite != nullptr;

        bool mainBlend = hasBaseColor ? baseBlend : detailBlend;

        int mainGroup = mainBlend ? 0 : 1;
        if (hasTwoChannels && baseBlend && !detailBlend) mainGroup = 2;
        int detailGroup = detailBlend ? 0 : 1;
        if (!data.is22) {
            mainGroup += 3;
            detailGroup += 3;
        }

        std::vector<CCSprite*> baseSprites;
        nodetree::collectSprites(gameObject,
            {gameObject->m_colorSprite, gameObject->m_glowSprite}, baseSprites, false);
        for (auto* s : baseSprites) {
            if (mainBlend) {
                s->setBlendFunc(c_additive);
                additive.sprites.insert(s);
            } else {
                setNormalBlend(s);
            }
        }

        if (gameObject->m_colorSprite) {
            std::vector<CCSprite*> detailSprites;
            nodetree::collectSprites(gameObject->m_colorSprite, {}, detailSprites, true);
            for (auto* s : detailSprites) {
                if (detailBlend) {
                    s->setBlendFunc(c_additive);
                    additive.sprites.insert(s);
                } else {
                    setNormalBlend(s);
                }
            }
        }

        if (gameObject->m_glowSprite) {
            std::vector<CCSprite*> glowSprites;
            nodetree::collectSprites(gameObject->m_glowSprite, {}, glowSprites, true);
            additive.sprites.insert(glowSprites.begin(), glowSprites.end());
        }

        drawItems.push_back({gameObject, nullptr, c_additive, mainBlend, data.zLayer, mainGroup, data.zOrder, 0,
            data.drawKey, false, false, false});

        bool split = hasTwoChannels && baseBlend != detailBlend;
        if (split) {
            drawItems.push_back({gameObject->m_colorSprite, gameObject->m_colorSprite, c_additive, detailBlend,
                data.zLayer, detailGroup, data.zOrder, 1, data.drawKey, true, false, false});
        }

        visibleIndex++;
    }

    return sprite;
}

void addParticles(std::vector<ObjectData> const& originalData, CCSprite* sprite, std::vector<DrawItem>& drawItems,
    bool haveRef, CCPoint refEditorPos, CCPoint refLocalPos) {
    for (const auto& data : originalData) {
        if (data.particleString.empty()) continue;

        auto* particle = GameToolbox::particleFromString(data.particleString, nullptr, false);
        if (!particle) continue;

        particle->retain();

        CCPoint localPos;
        if (haveRef) {
            localPos = refLocalPos + (data.position + data.particleOffset - refEditorPos);
        } else {
            localPos = sprite->getContentSize() / 2 + data.particleOffset;
        }
        particle->setPosition(localPos);
        particle->setScale(data.scale);
        particle->setRotation(data.rotation);

        if (data.particleUseObjectColor) {
            ccColor4F sc = particle->getStartColor();
            sc.r = data.objectColor.r / 255.0f;
            sc.g = data.objectColor.g / 255.0f;
            sc.b = data.objectColor.b / 255.0f;
            particle->setStartColor(sc);
            ccColor4F ec = particle->getEndColor();
            ec.r = data.objectColor.r / 255.0f;
            ec.g = data.objectColor.g / 255.0f;
            ec.b = data.objectColor.b / 255.0f;
            particle->setEndColor(ec);
        }

        particle->resetSystem();
        float particleDuration = particle->getDuration();
        int steps = 60;
        if (particleDuration > 0 && particleDuration < 60.0f) {
            steps = static_cast<int>(particleDuration * 60.0f);
        }
        for (int s = 0; s < steps; ++s) {
            particle->update(1.0f / 60.0f);
        }

        bool particleAdditive = particle->isBlendAdditive();
        bool particlePremultiplied = particle->getTexture()
            && particle->getTexture()->hasPremultipliedAlpha();
        ccBlendFunc pFunc = {
            static_cast<GLenum>(particlePremultiplied ? GL_ONE : GL_SRC_ALPHA),
            static_cast<GLenum>(particleAdditive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA)
        };
        particle->setBlendFunc(pFunc);

        int group = (particleAdditive ? 0 : 1) + (data.is22 ? 0 : 3);
        drawItems.push_back({particle, nullptr, pFunc, particleAdditive,
            data.zLayer, group, data.zOrder, 2, data.drawKey, false, true, false});
    }
}

void addGradients(LevelEditorLayer* lel, std::vector<BorrowedNode>& borrowedNodes,
    std::vector<DrawItem>& drawItems, bool includeGradients) {
    if (!includeGradients || !lel->m_gradientLayers) return;

    for (auto [key, gl] : CCDictionaryExt<int, GJGradientLayer*>(lel->m_gradientLayers)) {
        if (!gl || !gl->isVisible()) continue;

        CCNode* glParent = gl->getParent();
        if (!glParent) continue;

        borrowedNodes.push_back({gl, glParent, gl->getPosition(), gl->getZOrder()});

        ccBlendFunc gFunc = gl->getBlendFunc();
        bool gAdditive = gFunc.dst == GL_ONE;

        drawItems.push_back({gl, nullptr, gFunc, gAdditive,
            gl->m_blendingLayer, gAdditive ? 0 : 1, gl->getZOrder(), 3,
            computeDrawKey(gl), false, false, true});
    }
}

void attachInDrawOrder(std::vector<DrawItem>& drawItems, CCSprite* sprite, AdditiveParts& additive) {
    std::stable_sort(drawItems.begin(), drawItems.end(),
        [](DrawItem const& a, DrawItem const& b) {
            if (a.zLayer != b.zLayer) return a.zLayer < b.zLayer;
            if (a.group != b.group) return a.group < b.group;
            if (a.zOrder != b.zOrder) return a.zOrder < b.zOrder;
            if (a.drawKey != b.drawKey) return a.drawKey < b.drawKey;
            return a.subOrder < b.subOrder;
        });

    int drawZ = 0;
    for (auto& item : drawItems) {
        if (item.isSprite) {
            CCSprite* s = item.sprite;
            CCNode* parent = s->getParent();
            if (!parent) {
                if (item.useBlend) s->setBlendFunc(item.func);
                continue;
            }

            CCPoint worldPos = parent->convertToWorldSpace(s->getPosition());
            CCPoint localPos = sprite->convertToNodeSpace(worldPos);

            s->retain();
            s->removeFromParent();
            s->setPosition(localPos);
            if (item.useBlend) {
                s->setBlendFunc(item.func);
                additive.sprites.insert(s);
            } else {
                setNormalBlend(s);
                additive.sprites.erase(s);
            }
            sprite->addChild(s, drawZ);
            s->release();
        } else if (item.isParticle) {
            auto* particle = static_cast<CCParticleSystemQuad*>(item.node);
            particle->setZOrder(drawZ);
            if (item.useBlend) {
                particle->setBlendFunc(item.func);
                additive.particles.push_back(particle);
            }
            if (particle->getParent() != sprite) {
                sprite->addChild(particle, drawZ);
            }
        } else if (item.isGradient) {
            CCNode* g = item.node;
            CCNode* parent = g->getParent();
            CCPoint worldPos = parent
                ? parent->convertToWorldSpace(g->getPosition())
                : g->getPosition();
            CCPoint localPos = sprite->convertToNodeSpace(worldPos);

            g->retain();
            g->removeFromParent();
            g->setPosition(localPos);
            sprite->addChild(g, drawZ);
            g->release();

            if (item.useBlend) additive.gradients.push_back(g);
        } else {
            item.node->setZOrder(drawZ);
            if (auto* spr = geode::cast::typeinfo_cast<CCSprite*>(item.node)) {
                if (item.useBlend) {
                    spr->setBlendFunc(item.func);
                    additive.sprites.insert(spr);
                } else {
                    setNormalBlend(spr);
                    additive.sprites.erase(spr);
                }
            }
        }
        ++drawZ;
    }
}

}

BorrowGuard::~BorrowGuard() {
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        if (!it->parent) continue;
        it->node->removeFromParent();
        it->node->setPosition(it->position);
        it->parent->addChild(it->node, it->zOrder);
    }
    nodes.clear();
}

CCSprite* buildExportSprite(std::vector<ObjectData> const& originalData, LevelEditorLayer* lel,
    std::vector<BorrowedNode>& borrowedNodes, AdditiveParts& additive, bool includeGradients) {
    std::vector<DrawItem> drawItems;

    bool haveRef = false;
    CCPoint refEditorPos, refLocalPos;

    CCSprite* sprite = cloneObjects(originalData, lel, additive, drawItems, haveRef, refEditorPos, refLocalPos);
    addParticles(originalData, sprite, drawItems, haveRef, refEditorPos, refLocalPos);
    addGradients(lel, borrowedNodes, drawItems, includeGradients);
    attachInDrawOrder(drawItems, sprite, additive);

    return sprite;
}
