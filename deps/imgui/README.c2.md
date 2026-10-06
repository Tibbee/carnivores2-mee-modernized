# Dear ImGui — pinned C2 dependency

- Upstream: https://github.com/ocornut/imgui
- Tag: `v1.91.9b-docking`
- Commit: `4806a1924ff6181180bf5e4b8b79ab4394118875`
- Annotated tag object: `52fe0a05a7b1aa180a202bb24f0f2a049a9c1b7d`
- Licence: unmodified upstream `LICENSE.txt` (MIT).

The 15 upstream files are unmodified. `upstream-sha256.json` records their
checksums. Core, tables/widgets/draw, Win32 and OpenGL3 backends only; no demo,
examples, runtime download, embedded browser or secondary GL loader.

CMake's `c2_imgui` target is created only for `RENDERER=GL`. The GL backend is
compiled with `IMGUI_IMPL_OPENGL_LOADER_CUSTOM` and a forced include of the
existing GLAD header. Win32 gamepad support is disabled. Project warning policy
is not applied to this third-party target. Software and menu do not link it.

`Hunt/Workbench/WorkbenchUI.cpp` owns integration. Docking and multi-viewport
flags are deliberately off. Automatic ImGui ini/log persistence is disabled.
The shell reads Windows' installed Segoe UI for legibility, falling back to the
built-in upstream font if missing. No Microsoft font is vendored or packaged.

Updates must be deliberate: change the commit/tag, replace all upstream files
and regenerate checksums, inspect backend state preservation and input handling,
then rerun native/hidden-GPU tests and owner acceptance. The binary package
includes the licence as `THIRD-PARTY-LICENSES/LICENSE.Dear-ImGui.txt`.
