#pragma once
/*
  SystemFormat.h
  --------------
  Purpose (Engine-side):
    - Define a minimal, type-agnostic "system format" so gameplay systems in SampleApp
      can be written consistently and operate over the engine ECS managers.
    - Keep it simple: systems declare required/excluded component masks, then implement update().

  Notes:
    - The Engine owns the ECS managers (ComponentRegistry, ArchetypeStoreManager, etc.).
    - Application should expose these managers to SampleApp (e.g., via getters or an ECSContext).
    - SampleApp constructs systems that follow this format and calls update() each frame.

  You can evolve this into a scheduler later.
*/

#include "ECS/Components.h"     // ComponentRegistry, ComponentMask
#include "ECS/ArchetypeStore.h" // ArchetypeStoreManager

namespace Engine::ECS
{
    // A generic, minimal interface for gameplay systems.
    // Game programmers implement:
    //  - buildMasks(ComponentRegistry&) to set required/excluded based on component names
    //  - update(ArchetypeStoreManager&, float dt) to run logic on matching stores
    struct IGameplaySystem
    {
        virtual ~IGameplaySystem() = default;

        // Called once after construction to resolve required/excluded masks from names via registry.
        virtual void buildMasks(ComponentRegistry &registry) = 0;

        // Per-frame update; dt = seconds since last frame.
        virtual void update(ArchetypeStoreManager &stores, float dt) = 0;

        // Optional: name for logging.
        virtual const char *name() const { return "UnnamedSystem"; }
    };

    // A tiny helper that many systems will follow:
    // - Maintain required/excluded masks internally.
    // - Provide setRequiredNames/setExcludedNames convenience.
    class SystemBase : public IGameplaySystem
    {
    public:
        void setRequiredNames(const std::vector<std::string> &names) { m_requiredNames = names; }
        void setExcludedNames(const std::vector<std::string> &names) { m_excludedNames = names; }

        void buildMasks(ComponentRegistry &registry) override
        {
            // Build required mask from names
            m_required = ComponentMask{};
            for (const auto &n : m_requiredNames)
            {
                uint32_t id = registry.ensureId(n);
                m_required.set(id);
            }
            // Build excluded mask from names (optional)
            m_excluded = ComponentMask{};
            for (const auto &n : m_excludedNames)
            {
                uint32_t id = registry.ensureId(n);
                m_excluded.set(id);
            }
        }

    protected:
        // Accessors for derived systems
        const ComponentMask &required() const { return m_required; }
        const ComponentMask &excluded() const { return m_excluded; }

    private:
        std::vector<std::string> m_requiredNames;
        std::vector<std::string> m_excludedNames;
        ComponentMask m_required;
        ComponentMask m_excluded;
    };

} // namespace Engine::ECS