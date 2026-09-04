// GameMode.h — Game mode state enum (replaces boolean flag sprawl)
// Phase 1.1: replaces 30+ independent BOOL flags with a single state machine.
#pragma once

#include <cstring>

enum class GameMode : unsigned int {
    Normal          = 0,
    Swimming        = 1,
    Underwater      = 2,
    Flying          = 3,
    Paused          = 4,
    Binocular       = 5,
    OpticScope      = 6,
    MapMode         = 7,
    ExitCountdown   = 8,
    TrophyMode      = 9,
    Dead            = 10,
    Falling         = 11,
    Crouching       = 12,
    NightVision     = 13,
    DogMode         = 14,
    SurvivalMode    = 15,
    ScannerMode     = 16,
    SonarMode       = 17,
};

extern GameMode g_GameMode;

// --- Accessor helpers (inline, defined after g_GameMode is visible) ---

// Caveat: the previous version of `IsUnderwater()` used
//   `g_GameMode == GameMode::Underwater || g_GameMode == GameMode::Swimming`.
// That definition collapses "the player is physically submerged" with
// "the logical game mode is Underwater/Swimming". Once the player opens
// an overlay (Tab -> MapMode, Escape -> ExitCountdown, Pause, ...),
// g_GameMode flips off Underwater even though the camera is still
// physically below the water surface. The old helper would then return
// false and the renderer would yank away the fog, render the water
// surface opaque, and clip underwater models.
//
// The call sites that use IsUnderwater() all want the *physical* semantic
// (NOT the game-mode semantic): fog density, sun glare in ShowVideo(),
// sky dimming, water-surface visibility, terrain water blending, drowning
// in Controls.cpp, weapon hide, animation slowdown, breath handling, etc.
// So we redirect the helper to the global flag that's always been the
// actual source of truth for the physical state.
extern int UNDERWATER;          // 1 iff camera is submerged this frame

inline bool IsUnderwater()  { return UNDERWATER != 0; }
inline bool IsScopeActive() { return g_GameMode == GameMode::OpticScope || g_GameMode == GameMode::Binocular; }
inline bool IsPaused()      { return g_GameMode == GameMode::Paused; }
inline bool IsCrouching()   { return g_GameMode == GameMode::Crouching; }

// --- Per-state accessors (1:1 with g_GameMode == GameMode::X) ---
//
// Mirrors the helper set in Carnivores1/Hunt/Core/GameMode.h so the
// two projects read uniformly. Use these whenever you want to test
// for a specific game-mode value without writing the inline
// comparison — and so the call site stays readable if the underlying
// GameMode enum evolves.

inline bool IsNormal()        { return g_GameMode == GameMode::Normal; }
inline bool IsSwimming()      { return g_GameMode == GameMode::Swimming; }
inline bool IsUnderwaterMode() { return g_GameMode == GameMode::Underwater; }
inline bool IsBinocular()     { return g_GameMode == GameMode::Binocular; }
inline bool IsOpticScope()    { return g_GameMode == GameMode::OpticScope; }
inline bool IsMapMode()       { return g_GameMode == GameMode::MapMode; }
inline bool IsExitCountdown() { return g_GameMode == GameMode::ExitCountdown; }
inline bool IsTrophyMode()    { return g_GameMode == GameMode::TrophyMode; }

extern char ProjectName[128]; // set once from the prj= command line (GameState.h)
// Trophy-room test that does NOT depend on g_GameMode (a dozen writers can
// flip the mode mid-session, e.g. water/swim transitions). ProjectName is
// stable for the whole session. Same substring LoadResources() uses to set
// TrophyMode, so the two can never disagree about which map is the room.
inline bool InTrophyRoomMap() { return strstr(ProjectName, "trophy") != nullptr; }
inline bool IsSurvivalMode()  { return g_GameMode == GameMode::SurvivalMode; }
