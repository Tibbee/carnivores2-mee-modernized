// GameMode.h — Game mode state enum (replaces boolean flag sprawl)
// Phase 1.1: replaces 30+ independent BOOL flags with a single state machine.
#pragma once

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
