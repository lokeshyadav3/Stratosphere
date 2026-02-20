#pragma once
/*
  Query.h
  -------
  Purpose:
    - Provide a compiled ECS query: required/excluded masks + cached matching archetype IDs.

  Notes:
    - Queries cache matching archetype IDs to avoid scanning all stores.
    - Queries can optionally track dirty rows (bitset per matching store) so systems can update incrementally.
    - QueryManager incrementally updates store lists when new archetype stores are created.
*/

#include <cstdint>
#include <unordered_map>
#include <vector>
#include "ECS/Components.h"

namespace Engine::ECS
{
    using QueryId = uint32_t;

    struct Query
    {
        ComponentMask required;
        ComponentMask excluded;
        std::vector<uint32_t> matchingArchetypeIds;

        // Dirty tracking (optional)
        bool dirtyEnabled = false;
        ComponentMask dirtyComponents;

        // For O(1) lookup of a matching archetype index.
        std::unordered_map<uint32_t, uint32_t> archetypeToMatchIndex;

        // Parallel to matchingArchetypeIds: bitset per matching archetype.
        // Row i is dirty if (dirtyBits[matchIdx][i/64] & (1ull<<(i%64))) != 0.
        std::vector<std::vector<uint64_t>> dirtyBits;
    };
}
