# Moonlit night lighting (OpenGL)

This feature is on local `main` after the owner-approved fast-forward. It is not a
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
night_world_brightness 22
night_desaturation 85
night_sky_brightness 60
night_moon_brightness 128
night_moon_glow_strength 100
```

| Key | Default | Accepted values |
|---|---:|---|
| `night_world_brightness` | 22 | 5-100 percent of the pre-night world shader color |
| `night_desaturation` | 85 | 0-100 percent desaturation |
| `night_sky_brightness` | 60 | 5-100 percent of the authored sky texture color |
| `night_moon_brightness` | 128 | 0-1000 percent moon-disc RGB gain; 100 = original, 0 = disc off (halo remains) |
| `night_moon_glow_strength` | 100 | 0-1000 percent moon-halo strength; 100 = existing glow, 0 = halo off (disc remains) |

The values are whole integers. Invalid or oversized values are logged and
ignored without erasing an earlier valid setting. Missing keys use compiled
defaults; installing a new binary does not rewrite an existing config.

Only OpenGL consumes these rendering settings. Software rendering, dawn/day,
profile layouts, equipment prices and dinosaur perception are unchanged.

There is one OpenGL night look: Moonlit. Tune the settings above rather
than choosing between rendering styles. The obsolete `night_lighting` key is
ignored (with a log message), whether it says 0 or 1. Existing configs need not
be rewritten. The selector removal is built/tested and deployed as an
owner-authorized Shipping preview; manual acceptance remains pending.

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

The flashlight must be purchased, on, have nonzero intensity and belong to a
living hunter. Death switches it off immediately, including dinosaur kill
animations, and the flashlight key cannot turn it back on while dead.
Night vision is unchanged. UI/held-model
restoration follows that effective toggle state, not cone/range geometry. It
adds no warm beam boost to the weapon. Diffuse, cutout, transparent, reflection
and specular passes share the policy. World surfaces retain the original
localized beam; world/sky NV behavior remains the accepted previous behavior.

Health and control messages get a minimum 55-percent pre-night brightness
multiplier, still desaturated, without equipment. Exit/pause cards have their
confirmation lettering baked into the artwork, so the WHOLE card gets this
floor. Other art, including the map, uses the configured world multiplier.
The scope/aiming cross becomes a dim light mark in Moonlit night only; its
daytime black appearance and geometry are retained.

Readability uses at most 16 clipped top-down rectangles during the existing
UI draw. Overflow makes the overlay readable instead of dropping controls.
Bounds reset each frame; no second fullscreen pass, mask texture or CPU
modification of the HUD image is needed. Transparent pixels are tested BEFORE
grading, so dim opaque borders do not disappear.

Day/dawn have no new equipment-dependent UI rules. No new config
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

The moon disc does not inherit world or sky brightness/desaturation.
`night_moon_brightness` scales its RGB contribution, not opacity. The default
128 is approximately the earlier 200 -> 255 alpha boost; 100 uses the original
disc brightness, 200 doubles its contribution and 1000 requests ten times.
Restart the hunt after changing config. Missing keys use the compiled default.

The temporary alpha/halo boost has been reverted: base opacity is again 200
and the separate cooler halo strength is 0.10 (day/dawn remain 200/0.18).
Cloud visibility, global fog, underwater depth and the shared `SunGlare_Disc`
runtime F10 tuning still attenuate disc alpha (there is no `sun_glare_disc`
config key). The desired RGB gain is 1 for all other draws sharing the model
shader, including world objects, held models, HUD and the daytime sun.
A per-program value cache uploads only on first use or a gain transition:
the first non-moon draw resets a moon gain, and subsequent neutral draws skip
the redundant driver call. Renderer initialization clears the cache. The shader
multiplication is unchanged. The disc control does not change halo brightness.

The same per-program first-use/change-only policy now covers night strength,
night settings, HUD/viewmodel presentation, flashlight beam parameters and
flashlight tint. `UniformValueCache` compares exact float bytes without an
approximation or epsilon. Desired values are still computed on every draw,
including NV/daytime/off/death and non-world transitions; camera UBO inputs
are still rebuilt every call. Both program maps clear on renderer initialization.
The redundant direct sun/moon `uNightStrength` write was removed so the shared
helper owns it. All three builds pass 10/10 CTest and 58 focused renderer-policy cases pass.
An isolated actual-model-shader test matches 640 cached/uncached RGBA pixel
comparisons (1e-7 tolerance); synthetic repeated requests require 323 rather
than 323200 uploads. Controls that always upload or never upload are rejected.
These counts are not a whole-frame benchmark or visual gameplay acceptance.

`night_moon_glow_strength` independently scales the cooler sky halo: 100 retains
its existing 0.10 base strength, 200 doubles it, and 0 disables only the halo.
The accepted range is 0..1000 percent. Cloud/fog fading and halo radius/colour
remain unchanged; the daytime sun still uses 0.18 regardless of this setting.
The control and default config entry are tested and Shipping-deployed for
owner testing. Restart the hunt to apply a config change.

An isolated hidden GPU probe compiled the actual `model.frag` and verified
100/128/200/1000-percent gains with both full and quarter visibility, additive
blending, RGBA32F and RGBA8 targets. Above-100-percent gain brightens the
synthetic moon texel; float output tracks the expected contribution, while
normalized output eventually saturates. Reset-to-1 non-moon draws pass.
A subsequent cached-versus-unconditional GPU probe matched the same synthetic
pixel outputs (tolerance 1e-7), including the non-moon reset, while reducing
uniform uploads from 2020 to 17 across repeated requests. A native 120-frame
sequence with 1000 ordinary draws per frame requires 240 rather than 120120
uploads at non-neutral moon gain; at gain 1, repeated requests need no uploads
after first use. These are call-count checks, not measured frame-time gains.
This is not visual gameplay acceptance or an exact real-moon texture comparison.
Local evidence: C2 `build/moon-rgb-gpu-probe.cpp`,
`build/moon-rgb-gpu-probe-results.log` and `build/moon-rgb-focused.log`.
The earlier native alpha-cap probe established why opacity was not a useful
unbounded brightness control. The RGB implementation is built/tested and
Shipping-deployed for owner testing; visual acceptance remains pending.

The cloud-visibility heuristic still samples the rendered sky; its response to
the new palette needs in-game checking, especially around cloud edges and NV
toggles. Do not interpret unit tests as proof of identical cloud/moon dynamics.

## Testing and deployment

Tests cover settings/ranges, obsolete-selector handling, dawn/day/NV bypass, full/partial
beam restoration, cone/range endpoints, sky/fog grading, shader-copy parity and
draw-path uniform contracts, equipment-state priority, readability bounds and
UI transparency. They do not establish a good-looking final image.

Deploy the matching OpenGL executable and all shaders together. Shader defaults
now match Moonlit; mixing them with older executables is not supported.

Manual acceptance should check the single Moonlit look at repeatable locations:
- Clear ground, dense vegetation, animal silhouettes and water.
- Flashlight off/on; center, edge and range falloff; zero intensity.
- NV off/on, including NV plus flashlight.
- Die with the flashlight on: the beam and flashlight-driven UI restoration
  stop at death; pressing the flashlight key during the kill animation does
  not relight it. Check a new hunt still starts with the flashlight off.
- Fog entry/exit, cloud edges, moon visibility and underwater views.
- Moon glow at 0/100/200: compare the halo and confirm disc brightness is
  unchanged. Check clouds/fog still fade it and daytime sun glow is unchanged.
- Moon brightness at 100/128/200 and a high value; compare disc detail and
  clipping. Check switching to day does not retain the moon gain.
- Map/exit/pause, compass/wind, weapon body/highlights and aiming-mark visibility.
- Toggle equipment with an overlay open; dismiss it and check for stale patches.
- Dawn/day appearance, HUD readability and zero-intensity behavior.

Use a disposable installation unless the owner separately authorizes live
deployment/testing. See [building/deployment](BUILDING.md).
