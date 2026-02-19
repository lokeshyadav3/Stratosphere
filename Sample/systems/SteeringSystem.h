#pragma once
#include "ECS/SystemFormat.h"
#include "ECS/Components.h"
#include <algorithm>
#include <cmath>
#include <iostream>

class SteeringSystem : public Engine::ECS::SystemBase
{
public:
    SteeringSystem()
    {
        // Position + Velocity + MoveTarget + MoveSpeed + Path + Facing required
        setRequiredNames({"Position", "Velocity", "MoveTarget", "MoveSpeed", "Path", "Facing"});
        setExcludedNames({"Disabled", "Dead"});
    }

    const char *name() const override { return "SteeringSystem"; }

    void update(Engine::ECS::ArchetypeStoreManager &mgr, float dt) override
    {
        auto len2 = [](float x, float z)
        { return std::sqrt(x * x + z * z); };

        // Gameplay uses meters; stop once we're within a small radius.
        const float arrivalRadius = 0.5f; 
        const float waypointRadius = 0.25f; // Tighter acceptance for precise cornering

        for (const auto &ptr : mgr.stores())
        {
            if (!ptr)
                continue;
            auto &store = *ptr;

            if (!store.signature().containsAll(required()))
                continue;
            if (!store.signature().containsNone(excluded()))
                continue;

            auto &positions = const_cast<std::vector<Engine::ECS::Position> &>(store.positions());
            auto &velocities = const_cast<std::vector<Engine::ECS::Velocity> &>(store.velocities());
            auto &targets = const_cast<std::vector<Engine::ECS::MoveTarget> &>(store.moveTargets());
            auto &speeds = const_cast<std::vector<Engine::ECS::MoveSpeed> &>(store.moveSpeeds());
            auto &paths = const_cast<std::vector<Engine::ECS::Path> &>(store.paths());
            auto &facings = const_cast<std::vector<Engine::ECS::Facing> &>(store.facings());

            const auto &masks = store.rowMasks();
            const uint32_t n = store.size();

            for (uint32_t i = 0; i < n; ++i)
            {
                if (!masks[i].matches(required(), excluded()))
                    continue;

                auto &pos = positions[i];
                auto &vel = velocities[i];
                auto &tgt = targets[i];
                const auto &spd = speeds[i];
                auto &path = paths[i];
                auto &facing = facings[i];

                if (!tgt.active)
                {
                    // Stop
                    vel.x = vel.y = vel.z = 0.0f;
                    continue;
                }

                // Determine target position: waypoint or final target
                float tx = tgt.x;
                float tz = tgt.z;
                bool isFinal = true;

                if (path.valid && path.current < path.count)
                {
                    tx = path.waypointsX[path.current];
                    tz = path.waypointsZ[path.current];
                    isFinal = false;
                }

                float dx = tx - pos.x;
                float dz = tz - pos.z;
                float dist = len2(dx, dz);

                // Check arrival
                float radiusToCheck = isFinal ? arrivalRadius : waypointRadius;
                
                if (dist <= radiusToCheck)
                {
                    if (isFinal)
                    {
                        // Arrived at final destination
                        vel.x = vel.y = vel.z = 0.0f;
                        tgt.active = 0;
                        path.valid = false;
                        // std::cout << "[Steering] ARRIVED\n";
                    }
                    else
                    {
                        // Arrived at waypoint
                        path.current++;
                        // Don't stop, just steer to next waypoint next frame
                        // For this frame, we can either keep moving to this waypoint (overshoot slightly)
                        // or steer to next immediately.
                        // Let's steer to next immediately to avoid jitter.
                        if (path.current < path.count)
                        {
                            tx = path.waypointsX[path.current];
                            tz = path.waypointsZ[path.current];
                            dx = tx - pos.x;
                            dz = tz - pos.z;
                            dist = len2(dx, dz);
                        }
                        else
                        {
                            // Path finished, next is final target
                            path.valid = false;
                            tx = tgt.x;
                            tz = tgt.z;
                            dx = tx - pos.x;
                            dz = tz - pos.z;
                            dist = len2(dx, dz);
                        }
                    }
                }

                // Move
                if (dist > 1e-4f)
                {
                    // Normalize
                    float invDist = 1.0f / dist;
                    dx *= invDist;
                    dz *= invDist;

                    float speed = spd.value;
                    
                    // Inertia / Smoothing Logic
                    // Calculate target velocity
                    float targetVx = dx * speed;
                    float targetVz = dz * speed;

                    // Acceleration factor (how fast we change velocity). 
                    // 10.0f - 15.0f gives a nice "weighted" feel.
                    const float acceleration = 15.0f; 

                    float diffX = targetVx - vel.x;
                    float diffZ = targetVz - vel.z;
                    
                    // Integrate acceleration
                    vel.x += diffX * acceleration * dt;
                    vel.z += diffZ * acceleration * dt;
                    vel.y = 0.0f;

                    // Update Facing based on actual velocity (smooth turn)
                    // Only update if velocity is significant
                    if (std::abs(vel.x) > 0.1f || std::abs(vel.z) > 0.1f)
                    {
                        facing.yaw = std::atan2(vel.x, vel.z);
                    }
                }
                else if (tgt.active) // Close but not arrived? (dist <= radiusToCheck was handled)
                {
                     // Already handled above
                }
            }
        }
    }
};
