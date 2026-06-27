// ==========================================================================
// CharacterInternal.h — Internal declarations shared across Character module
// ==========================================================================
// Functions declared here are used by multiple Character/*.cpp files but are
// not exposed to the rest of the engine (not in EngineAPI.h).

#pragma once

#include "Core/GameTypes.h"

// Collision / placement checks
int CheckPlaceCollisionP(Vector3d &v, bool aquatic);
int CheckPlaceCollision(TCharacter *cptr, Vector3d &v, BOOL wc, BOOL mc);

// Character lifecycle
void ResetCharacter(TCharacter *cptr);
