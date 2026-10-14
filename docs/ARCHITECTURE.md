# Source layout

A map of the tree, for anyone reading the code or adding to it. Numbers in the
refactoring table at the bottom are measured against the baseline commit this
fork started from (`341303d`), not from memory.

## Layout

```
Hunt/                              engine
├── Hunt.h                         umbrella header, kept for legacy includes
├── Memory.h                       arena allocator, smart pointers, MEM_DEBUG
├── Core/                          focused headers; StateDefs.cpp holds global state
│   ├── Version.h                  single source of truth for the version string
│   ├── GameMode.h                 GameMode enum + IsSurvivalMode and friends
│   ├── GameState.h                globals, via the GLOBAL macro
│   ├── GameTypes.h                TCharacter, TCharacterInfo, save structures
│   ├── AIBehavior.h               per-species AI constants and family rules
│   ├── EngineAPI.h                declarations that cross subsystem boundaries
│   ├── RenderTypes.h              renderer-facing types
│   ├── MathTypes.h                vectors, angles
│   ├── TerrainFog.h               fog maths
│   ├── TerrainMip.h               mip policy
│   ├── Flashlight.h               beam parameters
│   ├── SkyModeMapping.h           config value -> renderer behaviour
│   └── ...                        Constants.h, ModelTypes.h, KeyBindings.h, …
├── Game/                          game logic
│   ├── Hunt.cpp                   WinMain, window proc, game loop, draw
│   ├── EngineInit.cpp             startup, config.cfg generation
│   ├── CommandLine.cpp            argument parsing
│   ├── Config.cpp                 config.cfg loading
│   ├── Controls.cpp               input
│   ├── PlayerMovement.cpp         the hunter
│   ├── Characters.cpp             character lifecycle
│   ├── CharacterAI.cpp            behaviour selection
│   ├── CharacterAwareness.cpp     hunter perception and reactions  ← read this one
│   ├── CharacterSpawn.cpp         placement, including survival
│   ├── CharacterMovement.cpp      locomotion
│   ├── CharacterCollision.cpp     placement and collision
│   ├── CharacterTargeting.cpp     target selection
│   ├── CharacterTracking.cpp      tracking state
│   ├── CharacterMorph.cpp         morphing
│   ├── CharacterAnimation.cpp     dispatcher
│   ├── CharacterAnimation/        18 per-species animation files
│   ├── Effects.cpp                particles and effects
│   ├── Projectiles.cpp            bullets
│   ├── Ships.cpp, Ship.cpp        ship entities
│   ├── TerrainQueries.cpp         terrain sampling
│   ├── Trophy.cpp                 trophies and the trophy room
│   ├── Interface.cpp              in-game HUD
│   └── Network.cpp                inherited multiplayer, unmaintained
├── Renderer/
│   ├── IRenderer.h                the interface both backends implement
│   ├── RenderContext.cpp/.h       per-frame context assembly
│   ├── GLRenderer.cpp/.h          OpenGL 3.3 backend
│   ├── GLTerrain.cpp              terrain
│   ├── GLWater.cpp                water surface
│   ├── GLSky.cpp                  sky
│   ├── GLFog.cpp                  pocket and distance fog
│   ├── GLModel.cpp                3D models
│   ├── GLShadow.cpp               shadows
│   ├── GLNight.cpp                night lighting and desaturation
│   ├── GLFlashlight.cpp           flashlight cone
│   ├── GLUI.cpp, UIText.cpp       HUD, debug menu, text layout
│   ├── GLShader.cpp/.h            GLSL program and uniform cache
│   ├── GLPerf.cpp/.h              perf counters and CSV capture
│   ├── GLUtils.cpp/.h             GL helpers
│   ├── RenderSoft.cpp             software backend entry
│   ├── SoftTerrain/Water/Sky/Model/Video/Entities/Misc/HUD.cpp
│   └── renderasm.cpp              inline x86 assembly (32-bit only)
├── Loaders/
│   ├── Resources.cpp              _RES.TXT
│   ├── MapLoader.cpp              areas and maps
│   ├── CharacterLoader.cpp        creature definitions
│   ├── ModelLoader.cpp            .car models
│   ├── PictureLoader.cpp          images
│   ├── SoundLoader.cpp            sound
│   ├── ScriptParser.cpp           legacy script blocks
│   ├── ScriptBlockParse.h, ScriptValueParse.h
│   ├── LoadDiagnostics.h          lenient/strict recovery reporting
│   └── LoadValidate.h             shared validation
├── Math/                          Math.cpp, Vector.cpp
├── Audio/                         Audio_DLL.cpp, OpenAL_Loader.cpp/.h
├── Network/                       NetworkManager.cpp/.h, inherited
└── Debug/                         Log.cpp/.h

shaders/            13 GLSL files, loaded from disk at runtime
├── terrain, model, instanced_model, ui, night_desat, sky
├── each as a .vert / .frag pair
└── flashlight.glsl — one shared fragment included by the three world shaders

Menu/               the standalone menu (Carnivores2Menu.exe)
tests/              unit and contract tests, plus the smoke test harness
tools/              packaging, capture harness, HUNTDAT checker, analysis scripts
deps/               vendored glad and KHR headers
pkg/                assembled release folders
```

## Where to start reading

**`CharacterAwareness.cpp`** if you care about behaviour. Every hunter event
now goes through one resolver and one navigator, so eligibility, priority and
timers live in one place instead of a dozen handlers. It's the single most
changed file in the fork and the best description of what the AI work was for.

**`GLFog.cpp`** and **`GLTerrain.cpp`** if you care about how the game looks.
The fog system is the largest single rendering change.

**`LoadDiagnostics.h`** if you ship mods. It defines the lenient/strict policy
and every recovery the loader performs, in one readable place.

**`Hunt/Core/`** if you're looking for something and it's not in `Game/`. Those
are headers only, which is why `GameState.h` can hold several hundred globals
without dragging behaviour along.

## Design notes

- **Two renderers, one interface.** `IRenderer.h` is implemented by both the
  OpenGL and software backends. Which one you get is a compile-time decision, not
  a runtime one. See [BUILDING.md](BUILDING.md).
- **`GLOBAL` is a macro.** `Core/GameState.h` uses it so globals can be declared
  as `GLOBAL int Foo;` and emitted with initialisers in `StateDefs.cpp`. It's
  legacy shape, kept because the alternative was touching several hundred
  declaration sites for no benefit.
- **Behaviour is split, not wrapped.** The original `Characters.cpp` was
  genuinely two things, lifecycle and behaviour, so it became several files
  that each own one concern. There's no base class and no virtual dispatch; the
  split is by file, not by abstraction.
- **Save compatibility is a hard constraint.** `TCharacter` is 344 bytes and
  stays that size. When the legacy `awareHunter` boolean was removed, it went
  into `hunterAwareness` without changing the layout. Anything that would grow a
  saved struct needs a version bump and a migration path.
- **Shaders on disk, not in strings.** Easy to tweak, easy to diff, and a broken
  shader is a readable log line instead of a link error.

## How the tree got this way

The baseline was 13 `.cpp` files sitting directly in `Hunt/`, several of them
enormous and several of them doing more than one job.

| | Before (baseline `341303d`) | After |
|---|---|---|
| Files in `Hunt/` root | 13 `.cpp` | 0 (moved into subsystem directories) |
| `Characters.cpp` | 9,678 lines | 427 lines |
| `Game.cpp` | 3,076 lines | gone; became `Hunt.cpp` (1,944 lines) plus extracted files |
| Per-species animation | inside `Characters.cpp` | 13 files in `CharacterAnimation/` + a 360-line dispatcher |
| `Resources.cpp` | 4,844 lines | split across `Loaders/` by resource type |
| `Render3DFX.cpp` (Glide) | 5,054 lines | deleted |
| `RendererD3D.cpp` (Direct3D) | 6,238 lines | deleted |
| `RenderSoft.cpp` | 3,363 lines | `RenderSoft.cpp` + 9 `Soft*.cpp` files |
| Shaders | inline C++ strings | 12 shader programs plus one shared include in `shaders/` |
| Audio | `Audio_DLL.cpp`, 175 lines | `Audio/` with a real OpenAL loader |
| Software renderer | 3,363 lines in one file | 3,906 lines across `RenderSoft.cpp` + 9 `Soft*.cpp` files, with its own HUD and debug menu |
| OpenGL | none | new `GLRenderer` and 14 `GL*.cpp` files |

Removing the Glide and Direct3D backends is the single biggest structural
change in the fork. Everything else is smaller than it looks: the big files got
cut up, the renderers got replaced or reworked, and the rest was incremental.

The split was mechanical and behaviour-preserving: extract a function, move it,
verify the build. The behaviour changes came later and are recorded in
[CHANGELOG.md](../CHANGELOG.md) with their reasoning.

## Documentation status

`CarnivoresDoc/` is my private working repository and is not part of this
release. Everything you'd need to build, run, debug or validate data is in
[README.md](../README.md), [BUILDING.md](BUILDING.md),
[DEBUGGING.md](DEBUGGING.md) and this file. If something important is missing
from those, that's a bug in this repository's documentation, not a reason to go
looking elsewhere.