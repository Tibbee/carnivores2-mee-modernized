# Notice

## Upstream MEE heritage

This project is a fork/derivative of
**Carnivores 2 Modders Edition Engine** by
**[Ornithomimid1 (Oli)](https://github.com/Ornithomimid1)** and upstream
contributors. Credit for the inherited engine and modding foundation belongs
to its original authors.

- Upstream project (independently evolving branch):
  https://github.com/carnivores-cpe/Carnivores-CPE/tree/Map-Amb-Demo-2
- Upstream snapshot used as this fork's base:
  https://github.com/carnivores-cpe/Carnivores-CPE/tree/8be284e07a04010066616ca3b4bcc30666a44cb0
- Base commit: `8be284e07a04010066616ca3b4bcc30666a44cb0`

The standalone menu is derived from
[Carn2-Menu](https://github.com/carnivores-cpe/Carn2-Menu).

## Game copyright

Carnivores 2 (2000) and its assets are © Action Forms / WizardWorks /
Infogrames. This project is an unofficial modernization of the community
Modder's Engine v1.11 and contains **no original game assets**. You must own
the original game and supply your own `HUNTDAT` data directory; it is never
redistributed here.

## Modernization license

The modernization source code is © 2026 Tibor Harsányi (StriderTibe) and is
licensed under the MIT License - see [`LICENSE`](LICENSE).

The MIT license in that file applies **only** to the modernization work in this
repository, and to no other part of the project. It does not apply to the
original Carnivores 2 game (© Action Forms), to the upstream Modder's Engine
code (see "Upstream MEE heritage" above), or to any third-party component
listed below. Original game assets are never included in this repository or in
any release package.

## Third-party components

| Component | Purpose | License |
|-----------|---------|---------|
| glad (OpenGL 3.3 loader, `deps/glad/`) | Renderer | MIT |
| khrplatform.h (Khronos, `deps/KHR/`) | GL loader platform header | MIT |
| OpenAL Soft (`OpenAL32.dll`) | Runtime audio library | LGPL |
| googletest (build-time only) | Unit tests | BSD-3-Clause |

OpenAL Soft is loaded dynamically by name at runtime and can be replaced with
any other `OpenAL32.dll`. When OpenAL Soft is redistributed with a release
package, the LGPL license text and a link to its source
(https://openal-soft.org/) are included in that package; the DLL itself is an
unmodified upstream build.
