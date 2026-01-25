#include "VerifyLoadSModel.h"

#include "assets/AssetManager.h"
#include "assets/MaterialAsset.h"
#include "assets/MeshAsset.h"
#include "assets/ModelAsset.h"
#include "assets/TextureAsset.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>

namespace Sample
{
    Engine::ModelHandle VerifyLoadSModel(Engine::AssetManager &assets, const char *modelPath)
    {
        if (!modelPath)
        {
            std::cerr << "[SMODEL] VerifyLoadSModel: modelPath is null\n";
            return {};
        }

        std::cout << "\n[SMODEL] Loading: " << modelPath << "\n";

        Engine::ModelHandle modelHandle = assets.loadModel(modelPath);
        if (!modelHandle.isValid())
        {
            std::cerr << "[SMODEL] loadModel failed: " << modelPath << "\n";
            return {};
        }

        Engine::ModelAsset *model = assets.getModel(modelHandle);
        if (!model)
        {
            std::cerr << "[SMODEL] getModel returned nullptr\n";
            return {};
        }

        std::cout << "[SMODEL] OK primitives=" << model->primitives.size() << "\n";

        if (model->primitives.empty())
        {
            std::cerr << "[SMODEL] Model has 0 primitives (unexpected)\n";
            return modelHandle;
        }

        // Validate primitives -> mesh + material resolves
        for (size_t i = 0; i < model->primitives.size(); ++i)
        {
            const Engine::ModelPrimitive &prim = model->primitives[i];

            Engine::MeshAsset *mesh = assets.getMesh(prim.mesh);
            if (!mesh)
            {
                std::cerr << "[SMODEL] Primitive " << i << ": mesh resolve failed\n";
                continue;
            }

            std::cout << "  Prim[" << i << "] Mesh OK"
                      << " indices=" << prim.indexCount
                      << " vb=" << (void *)mesh->getVertexBuffer()
                      << " ib=" << (void *)mesh->getIndexBuffer()
                      << "\n";

            Engine::MaterialAsset *mat = assets.getMaterial(prim.material);
            if (!mat)
            {
                std::cout << "           Material: (missing)\n";
                continue;
            }

            std::cout << "           Material OK"
                      << " baseColor=("
                      << mat->baseColorFactor[0] << ", "
                      << mat->baseColorFactor[1] << ", "
                      << mat->baseColorFactor[2] << ", "
                      << mat->baseColorFactor[3] << ")\n";

            // Optional: verify textures resolve
            if (mat->baseColorTexture.isValid())
            {
                Engine::TextureAsset *tex = assets.getTexture(mat->baseColorTexture);
                std::cout << "           BaseColorTex: " << (tex ? "OK" : "FAILED") << "\n";
            }
            if (mat->normalTexture.isValid())
            {
                Engine::TextureAsset *tex = assets.getTexture(mat->normalTexture);
                std::cout << "           NormalTex: " << (tex ? "OK" : "FAILED") << "\n";
            }
        }

        // Validate node graph if present
        if (!model->nodes.empty())
        {
            const auto &nodes = model->nodes;
            const auto &np = model->nodePrimitiveIndices;
            const uint32_t nodeCount = static_cast<uint32_t>(nodes.size());
            const uint32_t primCount = static_cast<uint32_t>(model->primitives.size());

            auto nearlyEqual = [](float a, float b)
            {
                return std::abs(a - b) < 1e-4f;
            };

            bool nodeOk = true;
            std::cout << "[SMODEL] Nodes=" << nodeCount << " NodePrimIx=" << np.size() << "\n";

            for (uint32_t i = 0; i < nodeCount; ++i)
            {
                const auto &n = nodes[i];

                // Bounds checks on hierarchy and primitive ranges
                if (n.parentIndex != ~0u && n.parentIndex >= nodeCount)
                {
                    std::cerr << "  Node[" << i << "] invalid parentIndex=" << n.parentIndex << "\n";
                    nodeOk = false;
                }
                if (n.childCount > 0)
                {
                    if (n.firstChild == ~0u || n.firstChild + n.childCount > nodeCount)
                    {
                        std::cerr << "  Node[" << i << "] invalid child range first=" << n.firstChild << " count=" << n.childCount << "\n";
                        nodeOk = false;
                    }
                }
                if (n.primitiveCount > 0)
                {
                    if (n.firstPrimitiveIndex + n.primitiveCount > np.size())
                    {
                        std::cerr << "  Node[" << i << "] invalid prim range first=" << n.firstPrimitiveIndex << " count=" << n.primitiveCount << "\n";
                        nodeOk = false;
                    }
                    for (uint32_t k = 0; k < n.primitiveCount && (n.firstPrimitiveIndex + k) < np.size(); ++k)
                    {
                        const uint32_t pidx = np[n.firstPrimitiveIndex + k];
                        if (pidx >= primCount)
                        {
                            std::cerr << "  Node[" << i << "] prim ref out of bounds: " << pidx << "\n";
                            nodeOk = false;
                        }
                    }
                }

                // Recompute expected global = parent.global * local (or local if root)
                glm::mat4 expected = n.localMatrix;
                if (n.parentIndex != ~0u && n.parentIndex < nodeCount)
                {
                    expected = nodes[n.parentIndex].globalMatrix * n.localMatrix;
                }

                // Compare to stored global
                const float *a = glm::value_ptr(expected);
                const float *b = glm::value_ptr(n.globalMatrix);
                for (int m = 0; m < 16; ++m)
                {
                    if (!nearlyEqual(a[m], b[m]))
                    {
                        std::cerr << "  Node[" << i << "] global mismatch at element " << m << " expected=" << a[m] << " got=" << b[m] << "\n";
                        nodeOk = false;
                        break;
                    }
                }
            }

            if (nodeOk)
                std::cout << "[SMODEL] Node graph validation OK\n";
        }
        else
        {
            std::cout << "[SMODEL] No nodes present (fallback primitive-only)\n";
        }

        std::cout << "[SMODEL] Verification complete \n\n";
        return modelHandle;
    }
}
