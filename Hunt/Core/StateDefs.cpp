// StateDefs.cpp — Defines all global state (replaces the _MAIN_ role)
// This file is compiled once and provides storage for all extern declarations
// in GameState.h. It works by defining GLOBAL_DEFINE before including GameState.h,
// which makes the GLOBAL macro expand to nothing (instead of 'extern'), turning
// each declaration into a definition.

#define GLOBAL_DEFINE
#include "Core/GameState.h"
