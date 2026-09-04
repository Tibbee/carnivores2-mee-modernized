# Changelog

All notable changes to the Carnivores 2 MEE v1.1.4 (Modernized) source are
listed here. This project follows a version-aligned scheme with upstream MEE.

The format is based on [Keep a Changelog](https://keepachangelog.com/),
and this project aims at [Semantic Versioning](https://semver.org/).

## [v1.1.4-modernized] - 2026-09-04

Initial public release of the modernization. Built on the community
Carnivores 2 Modder's Engine v1.11 - huge credit to Ornithomimid1. This is a modernization of the engine source; it does **not**
include any original game assets.

### Added
- Native OpenGL 3.3 renderer (hardware terrain, models, sky, HUD/minimap, projected character shadows)
- Standalone settings menu (`Carnivores2Menu.exe`) - FOV, view distance, object detail, FPS limiter, keybindings
- Night vision equipment with bindable key and night desaturation
- Realistic water: waves, depth fog, underwater gradient and overlay
- OpenAL audio backend with EFX reverb (dynamic OpenAL Soft loader)
- New sky rendering: sun/moon halos, horizon gradient, cloud-aware lighting
- Dynamic resolution enumeration, widescreen support, borderless fullscreen
- Configurable view distance and per-pixel distance fog
- Memory arena allocators and stability/leak fixes

### Changed
- Modernized rendering, audio, and configuration paths (config.cfg text file replaces registry)
- Restructured monolithic sources into modular directories

### Notes
- Requires the original game's HUNTDAT (no copyrighted assets included)
- Windows x86, OpenGL 3.3 for the GL renderer; software renderer fallback included
- Source licensed MIT; see LICENSE and NOTICE.md
