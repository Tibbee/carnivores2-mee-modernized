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

inline bool IsUnderwater()  { return g_GameMode == GameMode::Underwater || g_GameMode == GameMode::Swimming; }
inline bool IsScopeActive() { return g_GameMode == GameMode::OpticScope || g_GameMode == GameMode::Binocular; }
inline bool IsPaused()      { return g_GameMode == GameMode::Paused; }
inline bool IsCrouching()   { return g_GameMode == GameMode::Crouching; }
