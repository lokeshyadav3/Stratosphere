#pragma once
//
// ECSContext.h
// -------------
// Purpose:
//   - Centralize Engine-owned ECS managers under a single context object.
//   - Owned by Application; SampleApp accesses it via Application::GetECS().
//
// Managers included:
//   - ComponentRegistry: name <-> id mapping for components (data-driven).
//   - ArchetypeManager: archetype IDs keyed by component signature.
//   - ArchetypeStoreManager: lazily created SoA stores per archetype.
//   - EntitiesRecord: control-plane mapping of entity handle -> (archetypeId, row).
//   - PrefabManager: dictionary of prefabs keyed by name (SampleApp loads JSON and fills it).
//
// Notes:
//   - Engine/Application owns lifetime of ECSContext.
//   - SampleApp should perform game-specific configuration (ensure IDs, load JSON, spawn, systems).
//

#include "ECS/Components.h"       // ComponentRegistry, ComponentMask and components (Position/Velocity/Health)
#include "ECS/ArchetypeManager.h" // ArchetypeManager
#include "ECS/ArchetypeStore.h"   // ArchetypeStoreManager
#include "ECS/Entity.h"           // EntitiesRecord
#include "ECS/Prefab.h"           // PrefabManager
namespace Engine::ECS
{
    struct ECSContext
    {
        // Core managers
        ComponentRegistry components;
        ArchetypeManager archetypes;
        ArchetypeStoreManager stores;
        EntitiesRecord entities;
        PrefabManager prefabs;

        // Optional helper to reset state (typically not needed except in tests/tools).
        void Reset()
        {
            // Recreate default-constructed managers.
            *this = ECSContext{};
        }
    };
}