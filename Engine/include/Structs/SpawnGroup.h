#include <iostream>

struct SpawnGroupResolved
{
    std::string id;
    std::string unitType;
    int count = 0;
    float originX = 0.0f;
    float originZ = 0.0f;
    float jitterM = 0.0f;
    std::string formationKind;
    int columns = 0;
    float circleRadiusM = 0.0f;
    bool spacingAuto = true;
    float spacingM = 0.0f;
    int team = -1;              // -1 = no team assignment, 0+ = team id
    float facingYawDeg = 0.0f;  // initial facing in degrees
};