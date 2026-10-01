# Moonlit night lighting (OpenGL)

This feature is on the unmerged `feature/night-lighting` branch. It is not a
published release. The owner tested and visually accepted both the initial
night preview and the deployed UI/weapon follow-up. This is preview acceptance,
not a claim that every manual regression scenario was exercised.

The goal is a dark moonlit wilderness: silhouettes and broad terrain remain
readable, but the flashlight or night vision is useful for identifying animals,
inspecting vegetation and aiming. It is not intended to make navigation
pitch-black.

## Settings

These belong in `config.cfg`, not `_MENU.TXT` or `_RES.TXT`.

```text
night_lighting 1
night_world_brightness 22
night_desaturation 85
night_sky_brightness 60
```

| Key | Default | Accepted values |
|---|---:|---|
| `night_lighting` | 1 | 0 = Classic, 1 = Moonlit |
| `night_world_brightness` | 22 | 5-100 percent of the pre-night world shader color |
| `night_desaturation` | 85 | 0-100 percent desaturation |
| `night_sky_brightness` | 60 | 5-100 percent of the authored sky texture color |

The values are whole integers. Invalid or oversized values are logged and
ignored without erasing an earlier valid setting. Missing keys use compiled
defaults; installing a new binary does not rewrite an existing config.

Only OpenGL consumes these rendering settings. Software rendering, dawn/day,
profile layouts, equipment prices and dinosaur perception are unchanged.

For a complete comparison with the previous OpenGL night behavior:

```text
night_lighting 0
```

Classic uses the previous 50-percent world multiplier, 60-percent desaturation,
old sky/fog treatment and old flashlight composition. Setting brightness to 50
and desaturation to 60 while leaving Moonlit enabled is NOT the same fallback:
Moonlit still restores colors in the beam and treats sky/fog differently.

## Flashlight restoration

Terrain, water, dinosaurs and scenery retain their color after normal
lighting/fog but before night grading. Moonlit applies the darker grade, then
blends back toward that retained color according to the beam:

`restoration = clamp(intensity * cone * squared_distance_falloff, 0, 1)`

Strong illumination restores brightness and natural color; restoration fades
at the edge and with distance. At zero intensity, outside the cone or at/beyond
the configured range, restoration is zero. The existing warm beam contribution
is then applied, with its previous headroom roll-off and black-surface floor.

This does not restore daytime light maps, remove fog, create shadows or introduce
new geometry illumination. The beam shape/range/intensity settings remain the
existing ones. See [debugging/config](DEBUGGING.md).

## In-game UI and held models

Moonlit nights use a shared equipment-visibility rule for the map, compass,
wind indicator and near-models (weapon, binoculars and their related passes):

| Active equipment | Brightness | Saturation |
|---|---|---|
| Neither | World brightness setting | Night desaturation setting |
| Flashlight | Pre-night brightness | Original color |
| Night vision | Pre-night brightness | Retain night desaturation; green overlay follows |
| Both | Same as night vision | Same as night vision (NV takes priority) |

The flashlight must be purchased, on and have nonzero intensity. UI/held-model
restoration follows that effective toggle state, not cone/range geometry. It
adds no warm beam boost to the weapon. Diffuse, cutout, transparent, reflection
and specular passes share the policy. World surfaces retain the original
localized beam; world/sky NV behavior remains the accepted previous behavior.

Health and control messages get a minimum 55-percent pre-night brightness
multiplier, still desaturated, without equipment. Exit/pause cards have their
confirmation lettering baked into the artwork, so the WHOLE card gets this
floor. Other art, including the map, uses the configured world multiplier.
The scope/aiming cross becomes a dim light mark in Moonlit night only; its
day/Classic black appearance and geometry are retained.

Readability uses at most 16 clipped top-down rectangles during the existing
UI draw. Overflow makes the overlay readable instead of dropping controls.
Bounds reset each frame; no second fullscreen pass, mask texture or CPU
modification of the HUD image is needed. Transparent pixels are tested BEFORE
grading, so dim opaque borders do not disappear.

Day/dawn and Classic have no new equipment-dependent UI rules. No new config
keys are needed. This follow-up was deployed with owner approval and separately
visually accepted after testing. Detailed regression cases below remain a
checklist, not a claimed completed manual matrix.

## Sky, moon and night vision

The night sky texture is desaturated/dimmed separately, with a world-anchored
gradient from 1.0 at the horizon to 0.72 at the zenith. Sky fog uses the same
brightness/desaturation grade as the world, once per fog source, retaining the
existing 35-percent night fog blend. Golden terrain scatter/fog warming is
disabled in Moonlit night. The moon disc/halo keep their separate rendering.

Night vision bypasses the extra WORLD grade and new sky treatment, retaining
the previous green-overlay behavior. In-game UI/held models follow the rule
above; the standalone menu is unaffected.
No new framebuffer copy, render target or PerFrame UBO layout is required.

The cloud-visibility heuristic still samples the rendered sky; its response to
the new palette needs in-game checking, especially around cloud edges and NV
toggles. Do not interpret unit tests as proof of identical cloud/moon dynamics.

## Testing and deployment

Tests cover settings/ranges, Classic behavior, dawn/day/NV bypass, full/partial
beam restoration, cone/range endpoints, sky/fog grading, shader-copy parity and
draw-path uniform contracts, equipment-state priority, readability bounds and
UI transparency. They do not establish a good-looking final image.

Deploy the matching OpenGL executable and all shaders together. New shader
uniforms have Classic initializers for older executables, but an older engine
will not enable the new Moonlit feature merely by receiving new shader files.

Manual acceptance should compare Classic/Moonlit at the same location:
- Clear ground, dense vegetation, animal silhouettes and water.
- Flashlight off/on; center, edge and range falloff; zero intensity.
- NV off/on, including NV plus flashlight.
- Fog entry/exit, cloud edges, moon visibility and underwater views.
- Map/exit/pause, compass/wind, weapon body/highlights and aiming-mark visibility.
- Toggle equipment with an overlay open; dismiss it and check for stale patches.
- Dawn/day and Classic appearance, HUD readability and zero-intensity behavior.

Use a disposable installation unless the owner separately authorizes live
deployment/testing. See [building/deployment](BUILDING.md).
