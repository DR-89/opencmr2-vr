# Third-party notices

OpenCMR2 VR is a community fork of [OpenCMR2](https://github.com/pablocpas/OpenCMR2),
based on [CMR2Decomp](https://github.com/pablocpas/CMR2Decomp). The source retains
the upstream authorship history and notices. The project is distributed under
the [GNU GPL version 3](LICENSE); component-specific licenses remain applicable.

| Component | Version / source | License / notices |
| --- | --- | --- |
| OpenCMR2 base | `a51f47f216af16fec8959edcf83694ea22cbc0fe` | GPL-3.0, root `LICENSE` |
| SDL | `5180c654d439ad7df5eb99481fd0e3f027db65de` | zlib; embedded third-party notices retained in source |
| OpenXR SDK / loader | `f2448a8797c85814aa892efc1ab8707900fbcc78` | Apache-2.0; see upstream `COPYING.adoc`, `LICENSES` and external component licenses |
| ENet | vendored in `third_party/enet` | MIT, `third_party/enet/LICENSE` |
| Bink decoder derived from FFmpeg | vendored in `src/video` | LGPL-2.1-or-later, `src/video/COPYING.LGPLv2.1` and source headers |
| zlib | original port sources in `game/Zlib*` | zlib license notice in `game/zlib/zlib.h` |
| glslang (build tool) | `b5782e52ee2f7b3e40bb9c80d15b47016e008bc9` | see its source license notices; it is not a game runtime dependency |
| Microsoft Visual C++ runtime (Windows package only) | unmodified x64 redistributable DLLs | Microsoft's terms; see `Licenses` in the Windows package |

Runtime license texts are also supplied in the Windows package's `Licenses`
directory and inside the APK under `assets/licenses`. The source bundle attached
to each release contains the project and pinned SDL, OpenXR and glslang source
trees, including their additional notices and build files. CMake applies the
documented small SDL OpenXR structure-initialization patch from
`cmake/patch_sdl_openxr.cmake` when building VR targets.

The original game assets are not part of this project or its downloads. Colin
McRae Rally, its original game content and related trademarks belong to their
respective owners. This fork is not an official Codemasters, EA or Meta product.
