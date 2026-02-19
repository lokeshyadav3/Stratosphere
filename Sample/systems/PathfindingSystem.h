#pragma once
/*
  PathfindingSystem
  -----------------
  Purpose:
    - Finds A* path for units that have a MoveTarget but no valid Path.
    - Updates Path component with waypoints.
    - Only runs when needed (MoveTarget set, Path invalid).
*/

#include "ECS/SystemFormat.h"
#include "NavGrid.h"
#include <queue>
#include <unordered_map>
#include <cmath>
#include <algorithm>

struct Node
{
    int x, z;
    float gCost;
    float hCost;
    int parentX, parentZ;

    float fCost() const { return gCost + hCost; }

    bool operator>(const Node &other) const
    {
        return fCost() > other.fCost();
    }
};

class PathfindingSystem : public Engine::ECS::SystemBase
{
public:
    PathfindingSystem(const NavGrid *grid)
        : m_grid(grid)
    {
        setRequiredNames({"Position", "MoveTarget", "Path"});
        setExcludedNames({"Disabled", "Dead", "Obstacle"});
    }

    const char *name() const override { return "PathfindingSystem"; }

    void update(Engine::ECS::ArchetypeStoreManager &mgr, float dt) override
    {
        if (!m_grid)
            return;

        for (const auto &ptr : mgr.stores())
        {
            if (!ptr)
                continue;
            auto &store = *ptr;

            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;

            auto &positions = store.positions();
            auto &targets = store.moveTargets();
            auto &paths = store.paths();
            const auto &masks = store.rowMasks();
            const uint32_t n = store.size();

            for (uint32_t i = 0; i < n; ++i)
            {
                if (!masks[i].matches(required(), excluded()))
                    continue;

                auto &pos = positions[i];
                auto &tgt = targets[i];
                auto &path = paths[i];

                if (!tgt.active)
                {
                    path.valid = false;
                    continue;
                }

                // If path is already valid and we haven't reached the end, skip replanning
                if (path.valid && path.current < path.count)
                    continue;
                
                // If path became invalid (e.g. forced replan), or finished but target is still active and far away?
                // Actually, SteeringSystem clears tgt.active when arrived.
                // So if tgt.active is true, we must assume we haven't arrived.
                // If path.valid is true, we are following it. 
                // We only need to plan if !path.valid. 
                
                // But wait, if we finished the path (current >= count), we might still be far from target 
                // if the path was partial? Our A* will try to reach the target exactly.
                
                if (path.valid && path.current >= path.count)
                {
                   // Path finished. Let Steering handle final approach.
                   // But if expected behavior is to just steer straight after path end, 
                   // we don't need to do anything here.
                   continue;
                }

                // Plan path!
                runAStar(pos, tgt, path);
            }
        }
    }

private:
    const NavGrid *m_grid;

    // Heuristic: Octile distance
    float heuristic(int x1, int z1, int x2, int z2)
    {
        int dx = std::abs(x1 - x2);
        int dz = std::abs(z1 - z2);
        return static_cast<float>(std::max(dx, dz)) + 0.414f * std::min(dx, dz);
    }

    void runAStar(const Engine::ECS::Position &startPos, const Engine::ECS::MoveTarget &target, Engine::ECS::Path &outPath)
    {
        int startX = m_grid->worldToGridX(startPos.x);
        int startZ = m_grid->worldToGridZ(startPos.z);
        int targetX = m_grid->worldToGridX(target.x);
        int targetZ = m_grid->worldToGridZ(target.z);

        // Bounds check target; clamp to grid if needed
        targetX = std::max(0, std::min(m_grid->width - 1, targetX));
        targetZ = std::max(0, std::min(m_grid->height - 1, targetZ));

        // If start is same as target (grid-wise), no path needed (direct steering handles it)
        if (startX == targetX && startZ == targetZ)
        {
            outPath.valid = true;
            outPath.count = 0;
            outPath.current = 0;
            return;
        }
        
        // Setup A*
        // Use a flat array for closed set / g-scores if grid is small enough?
        // Grid is ~122k cells. Vector valid/visited flags is fast.
        static std::vector<float> gScores;
        static std::vector<int> cameFrom; // stores parent index: z * width + x
        static std::vector<bool> inOpenSet;

        int gridSize = m_grid->width * m_grid->height;
        if (gScores.size() < static_cast<size_t>(gridSize))
        {
            gScores.resize(gridSize);
            cameFrom.resize(gridSize);
            inOpenSet.resize(gridSize);
        }

        std::fill(gScores.begin(), gScores.end(), 1e9f); // Infinity
        std::fill(cameFrom.begin(), cameFrom.end(), -1);
        std::fill(inOpenSet.begin(), inOpenSet.end(), false);

        auto getIdx = [&](int x, int z) { return z * m_grid->width + x; };

        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openSet;
        
        int startIdx = getIdx(startX, startZ);
        gScores[startIdx] = 0.0f;
        
        Node startNode;
        startNode.x = startX;
        startNode.z = startZ;
        startNode.gCost = 0.0f;
        startNode.hCost = heuristic(startX, startZ, targetX, targetZ);
        startNode.parentX = -1; 
        startNode.parentZ = -1;
        
        openSet.push(startNode);
        inOpenSet[startIdx] = true;

        bool found = false;
        int closestX = startX;
        int closestZ = startZ;
        float minH = startNode.hCost;

        int nodesExplored = 0;
        const int MAX_NODES = 2000; // Bail out if path too long/complex

        while (!openSet.empty())
        {
            Node current = openSet.top();
            openSet.pop();
            inOpenSet[getIdx(current.x, current.z)] = false; // It's closed now

            if (nodesExplored++ > MAX_NODES)
                break;

            if (current.x == targetX && current.z == targetZ)
            {
                found = true;
                break;
            }

            if (current.hCost < minH)
            {
                minH = current.hCost;
                closestX = current.x;
                closestZ = current.z;
            }

            // Neighbors
            int dxAddr[] = {0, 0, -1, 1, -1, -1, 1, 1};
            int dzAddr[] = {-1, 1, 0, 0, -1, 1, -1, 1};
            float costs[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.414f, 1.414f, 1.414f, 1.414f};

            for (int i = 0; i < 8; ++i)
            {
                int nx = current.x + dxAddr[i];
                int nz = current.z + dzAddr[i];

                if (!m_grid->isValid(nx, nz)) continue;
                if (!m_grid->isWalkable(nx, nz)) continue;

                // Diagonal check: prevent cutting corners
                // RELAXED: Allow corner cutting because we have massive inflation (3.5m).
                /*
                if (i >= 4)
                {
                    if (!m_grid->isWalkable(current.x, nz) || !m_grid->isWalkable(nx, current.z))
                        continue;
                }
                */

                float newG = current.gCost + costs[i];
                int nIdx = getIdx(nx, nz);

                if (newG < gScores[nIdx])
                {
                    gScores[nIdx] = newG;
                    cameFrom[nIdx] = getIdx(current.x, current.z);
                    
                    Node neighbor;
                    neighbor.x = nx;
                    neighbor.z = nz;
                    neighbor.gCost = newG;
                    neighbor.hCost = heuristic(nx, nz, targetX, targetZ);
                    neighbor.parentX = current.x;
                    neighbor.parentZ = current.z;
                    openSet.push(neighbor);
                    inOpenSet[nIdx] = true;
                }
            }
        }

        // Reconstruct path
        // If not found, path to closest
        int currX = found ? targetX : closestX;
        int currZ = found ? targetZ : closestZ;

        std::vector<std::pair<int, int>> pathNodes;
        int currIdx = getIdx(currX, currZ);
        
        while (currIdx != startIdx)
        {
            pathNodes.push_back({currX, currZ});
            int pIdx = cameFrom[currIdx];
            // Decode parent
            currX = pIdx % m_grid->width;
            currZ = pIdx / m_grid->width;
            currIdx = pIdx;
            
            // Safety break
            if (pathNodes.size() > 200) break;
        }

        // Simplify: just reverse
        std::reverse(pathNodes.begin(), pathNodes.end());

        // Fill component
        outPath.count = 0;
        outPath.current = 0;
        // Skip first node if it's the start node (usually A* doesn't include start in cameFrom chain unless we add it, here we stop at startIdx)
        
        // String Pulling / Path Smoothing
        // 1. Combine start + pathNodes into a single list
        std::vector<std::pair<int, int>> allNodes;
        allNodes.reserve(pathNodes.size() + 1);
        allNodes.push_back({startX, startZ});
        allNodes.insert(allNodes.end(), pathNodes.begin(), pathNodes.end());

        // 2. Greedy simplification
        std::vector<std::pair<int, int>> smoothed;
        smoothed.reserve(allNodes.size());
        
        size_t currentIdx = 0;
        
        while (currentIdx < allNodes.size() - 1)
        {
            size_t nextIdx = currentIdx + 1;
            
            // Try to look as far ahead as possible
            for (size_t i = currentIdx + 2; i < allNodes.size(); ++i)
            {
                float x0 = m_grid->gridToWorldX(allNodes[currentIdx].first);
                float z0 = m_grid->gridToWorldZ(allNodes[currentIdx].second);
                float x1 = m_grid->gridToWorldX(allNodes[i].first);
                float z1 = m_grid->gridToWorldZ(allNodes[i].second);
                
                if (!m_grid->lineCheck(x0, z0, x1, z1))
                {
                    break; 
                }
                nextIdx = i;
            }
            
            smoothed.push_back(allNodes[nextIdx]);
            currentIdx = nextIdx;
        }

        // Fill component
        outPath.count = 0;
        outPath.current = 0;
        
        for (const auto& p : smoothed)
        {
            if (outPath.count >= Engine::ECS::Path::MAX_WAYPOINTS) break;
            outPath.waypointsX[outPath.count] = m_grid->gridToWorldX(p.first);
            outPath.waypointsZ[outPath.count] = m_grid->gridToWorldZ(p.second);
            outPath.count++;
        }
        
        outPath.valid = true;
    }
};
