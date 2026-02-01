#pragma once

#include "ECS/SystemFormat.h"

#include "assets/AssetManager.h"

#include "Engine/Camera.h"
#include "Engine/Renderer.h"
#include "Engine/SModelRenderPassModule.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class RenderSystem : public Engine::ECS::SystemBase
{
public:
    explicit RenderSystem(Engine::AssetManager *assets = nullptr)
        : m_assets(assets)
    {
        // We need Position to build a model matrix later.
        setRequiredNames({"RenderModel", "RenderAnimation", "Position"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "RenderModelSystem"; }
    void setAssetManager(Engine::AssetManager *assets) { m_assets = assets; }

    void setRenderer(Engine::Renderer *renderer) { m_renderer = renderer; }
    void setCamera(Engine::Camera *camera) { m_camera = camera; }

    void update(Engine::ECS::ArchetypeStoreManager &mgr, float dt) override
    {
        if (!m_assets || !m_renderer || !m_camera)
            return;

        auto keyFromHandle = [](const Engine::ModelHandle &h) -> uint64_t
        {
            return (static_cast<uint64_t>(h.generation) << 32) | static_cast<uint64_t>(h.id);
        };

        struct PerModelBatch
        {
            std::vector<glm::mat4> instanceWorlds;
            std::vector<glm::mat4> nodePalette; // flattened: [instance][node]
            uint32_t nodeCount = 0;

            std::vector<glm::mat4> jointPalette; // flattened: [instance][joint]
            uint32_t jointCount = 0;

            // scratch (reused per instance)
            std::vector<Engine::ModelAsset::NodeTRS> trsScratch;
            std::vector<glm::mat4> localsScratch;
            std::vector<glm::mat4> globalsScratch;
            std::vector<uint8_t> visitedScratch;

            std::vector<glm::mat4> jointsScratch;
        };

        std::unordered_map<uint64_t, PerModelBatch> batchesByModel;
        std::unordered_map<uint64_t, Engine::ModelHandle> handleByKey;

        for (const auto &ptr : mgr.stores())
        {
            if (!ptr)
                continue;

            auto &store = *ptr;
            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;
            if (!store.hasRenderModel())
                continue;
            if (!store.hasRenderAnimation())
                continue;
            if (!store.hasPosition())
                continue;

            auto &renderModels = store.renderModels();
            auto &renderAnimations = store.renderAnimations();
            auto &positions = store.positions();
            const auto &masks = store.rowMasks();
            const uint32_t n = store.size();

            for (uint32_t row = 0; row < n; ++row)
            {
                if (!masks[row].matches(required(), excluded()))
                    continue;

                const Engine::ModelHandle handle = renderModels[row].handle;
                Engine::ModelAsset *asset = m_assets->getModel(handle);
                if (!asset)
                    continue;

                const uint64_t key = keyFromHandle(handle);
                const auto &anim = renderAnimations[row];

                const auto &pos = positions[row];

                handleByKey[key] = handle;

                auto &batch = batchesByModel[key];
                if (batch.nodeCount == 0)
                {
                    batch.nodeCount = static_cast<uint32_t>(asset->nodes.size());
                    batch.nodePalette.reserve(64u * batch.nodeCount);

                    batch.jointCount = asset->totalJointCount;
                    if (batch.jointCount > 0)
                        batch.jointPalette.reserve(64u * batch.jointCount);
                }

                if (batch.nodeCount == 0)
                    continue;

                // World matrix
                auto *facings = store.hasFacing() ? &store.facings() : nullptr;
                glm::mat4 world = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, pos.z));
                
                if (facings)
                {
                    const float yaw = (*facings)[row].yaw;
                    world = glm::rotate(world, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                }

                batch.instanceWorlds.emplace_back(world);

                const uint32_t safeClip = (!asset->animClips.empty())
                                              ? std::min(anim.clipIndex, static_cast<uint32_t>(asset->animClips.size() - 1))
                                              : 0u;
                const float timeSec = (!asset->animClips.empty() && anim.playing) ? anim.timeSec : 0.0f;

                // Node palette for this instance
                asset->evaluatePoseInto(safeClip, timeSec,
                                        batch.trsScratch,
                                        batch.localsScratch,
                                        batch.globalsScratch,
                                        batch.visitedScratch);
                if (batch.globalsScratch.size() == batch.nodeCount)
                {
                    batch.nodePalette.insert(batch.nodePalette.end(), batch.globalsScratch.begin(), batch.globalsScratch.end());
                }

                // Joint palette for this instance
                if (batch.jointCount > 0 && batch.globalsScratch.size() == batch.nodeCount)
                {
                    batch.jointsScratch.assign(batch.jointCount, glm::mat4(1.0f));

                    for (const auto &skin : asset->skins)
                    {
                        if (skin.jointCount == 0)
                            continue;
                        for (uint32_t j = 0; j < skin.jointCount; ++j)
                        {
                            if (j >= skin.jointNodeIndices.size() || j >= skin.inverseBind.size())
                                continue;

                            const uint32_t nodeIx = skin.jointNodeIndices[j];
                            if (nodeIx >= batch.globalsScratch.size())
                                continue;

                            const uint32_t outIx = skin.jointBase + j;
                            if (outIx >= batch.jointsScratch.size())
                                continue;

                            batch.jointsScratch[outIx] = batch.globalsScratch[nodeIx] * skin.inverseBind[j];
                        }
                    }

                    batch.jointPalette.insert(batch.jointPalette.end(), batch.jointsScratch.begin(), batch.jointsScratch.end());
                }

                (void)asset;
            }
        }

        // Create/update passes for models that have instances this frame.
        for (auto &kv : batchesByModel)
        {
            const uint64_t key = kv.first;
            auto &batch = kv.second;
            auto &worlds = batch.instanceWorlds;
            if (worlds.empty())
                continue;

            const Engine::ModelHandle handle = handleByKey[key];

            auto it = m_passes.find(key);
            if (it == m_passes.end())
            {
                auto pass = std::make_shared<Engine::SModelRenderPassModule>();
                pass->setAssets(m_assets);
                pass->setModel(handle);
                pass->setCamera(m_camera);
                pass->setEnabled(true);
                m_renderer->registerPass(pass);
                it = m_passes.emplace(key, std::move(pass)).first;
            }

            it->second->setCamera(m_camera);
            it->second->setEnabled(true);
            it->second->setInstances(worlds.data(), static_cast<uint32_t>(worlds.size()));
            it->second->setNodePalette(batch.nodePalette.data(), static_cast<uint32_t>(worlds.size()), batch.nodeCount);

            if (batch.jointCount > 0 && batch.jointPalette.size() == worlds.size() * static_cast<size_t>(batch.jointCount))
            {
                it->second->setJointPalette(batch.jointPalette.data(), static_cast<uint32_t>(worlds.size()), batch.jointCount);
            }
        }

        // Disable passes that have no instances this frame.
        for (auto &kv : m_passes)
        {
            if (batchesByModel.find(kv.first) == batchesByModel.end())
            {
                kv.second->setEnabled(false);
            }
        }
    }

private:
    Engine::AssetManager *m_assets = nullptr; // not owned
    Engine::Renderer *m_renderer = nullptr;   // not owned
    Engine::Camera *m_camera = nullptr;       // not owned

    std::unordered_map<uint64_t, std::shared_ptr<Engine::SModelRenderPassModule>> m_passes;
};
