# Clang-Tidy

The checked-in `.clang-tidy` configuration applies to project-owned C/C++
translation units. Vendored `deps/`, generated build trees, install trees, and
GoogleTest sources are excluded by `tools/run_clang_tidy.ps1`. The runner adds
`-m32` because C2's MSVC compilation database targets x86 even on an x64 host.

Configure a database with the x86 MSVC environment:

```powershell
cmake --preset clang-tidy
```

Analyze selected changed sources:

```powershell
powershell -ExecutionPolicy Bypass -File tools/run_clang_tidy.ps1 `
    -BuildDir build/clang-tidy `
    -Files @('Hunt/Game/Hunt.cpp', 'Hunt/Loaders/ScriptParser.cpp')
```

Analyze every project translation unit explicitly:

```powershell
powershell -ExecutionPolicy Bypass -File tools/run_clang_tidy.ps1 `
    -BuildDir build/clang-tidy `
    -AllProjectFiles
```

MSVC warnings use an explicit project-owned `/W4` baseline. Clang-tidy keeps
the full configured check set advisory except for the high-confidence lifetime
and memory checks listed under `WarningsAsErrors`; this is the current measured
legacy baseline. `/WX` is available through `-DC2_WARNINGS_AS_ERRORS=ON`, but
remains opt-in. `/permissive` is still retained for compatibility and remains
target-scoped; replacing it with `/permissive-` is a later focused cleanup.

The bundled LLVM 22.1.3 run on 2026-09-19 reported 35,508 advisory diagnostics
for `Hunt/Game/CommandLine.cpp` with the project-header filter and exited
successfully. That count is a baseline for legacy cleanup, not a CI failure
threshold; the runner fails on compiler errors or the explicitly blocking
high-confidence checks.
