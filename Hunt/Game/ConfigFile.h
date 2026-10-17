// ConfigFile.h -- engine config.cfg create/load surface (split from EngineInit.cpp)
#pragma once

// Writes the first-run config.cfg template when no file exists yet.
void CreateDefaultConfig();

// Applies config.cfg values over the compiled defaults (validated per key).
void LoadConfig();
