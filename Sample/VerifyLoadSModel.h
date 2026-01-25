#pragma once

#include "assets/Handles.h"

namespace Engine
{
    class AssetManager;
}

namespace Sample
{
    // Simple integrity test for .smodel loading.
    // Returns a valid handle if loaded, otherwise an invalid handle.
    Engine::ModelHandle VerifyLoadSModel(Engine::AssetManager &assets, const char *modelPath);
}
