# Third-party notices

## Credits and acknowledgements

ProsperoPuzzles exists thanks to the maintainers and contributors of:

- [Simon Tatham's Portable Puzzle Collection](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/)
  (MIT) for 40 of the games;
- [ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl), with
  [Mesa](https://mesa3d.org/) and OpenGNM PSBC, for OpenGL 4.6 on PS5;
- [PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate)
  and the [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) for the
  reproducible native foundation;
- [Inter](https://github.com/rsms/inter) (SIL Open Font License) for the
  interface font, and [stb](https://github.com/nothings/stb) for music decoding
  and font baking;
- [PS5-Lapy-JB-Daemon](https://github.com/ArkSama/PS5-Lapy-JB-Daemon) by
  ArkSama / Team PHU, and
  [mpereiraesaa's fork](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon) with
  the exact-title one-shot helper and cooperative protocol, for filesystem
  access;
- [curl](https://curl.se/), [OpenSSL](https://www.openssl.org/) and
  [miniz](https://github.com/richgel999/miniz) for the update check and the
  self-update;
- [MkPFS](https://github.com/PSBrew/MkPFS),
  [UFS2Tool](https://github.com/SvenGDK/UFS2Tool), LLVM/Clang, Python, zlib and
  GoogleTest for build, packaging and validation tooling.

The sound effects were generated for this project with ElevenLabs Sound
Effects. The original artwork, soundtrack and selection music are distributed
under the project license.

ProsperoPuzzles is Copyright (C) 2026 BlackBearReloaded and licensed under
GPL-3.0-or-later. It is built on `ps5-native-app-boilerplate`; the notices
below cover the boilerplate's dependencies and every third-party component the
project adds.

## Simon Tatham's Portable Puzzle Collection

`src/third_party/sgt-puzzles/` vendors the midend, shared libraries and the 40
official games of [Simon Tatham's Portable Puzzle
Collection](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/) from
`https://git.tartarus.org/simon/puzzles.git` (commit recorded in
`src/third_party/sgt-puzzles/UPSTREAM`). It is copyright (c) 2004-2024 Simon
Tatham and the contributors listed in its `LICENCE`, and is distributed under
the MIT licence reproduced in `src/third_party/sgt-puzzles/LICENCE` and
`third_party/sgt-puzzles-docs/LICENCE`. Local fixes are kept as patches under
`patches/sgt/`. The display names, descriptions and objectives in
`src/games/sgt/upstream_meta.inc` come from its `CMakeLists.txt`.

## Fonts and font baking

The UI font is [Inter](https://github.com/rsms/inter), Copyright (c) 2016 The
Inter Project Authors, licensed under the SIL Open Font License 1.1
(`third_party/fonts/Inter-LICENSE.txt`, also shipped as
`assets/fonts/Inter-LICENSE.txt`). `assets/fonts/*.ppzfont` are distance-field
renderings of it produced by `tools/bake-fonts.sh`.

The baker uses [stb_truetype](https://github.com/nothings/stb)
(`third_party/stb/stb_truetype.h`, public domain or MIT). It is a host-only
tool and is not linked into the PS5 application.

## Sound effects

`assets/audio/sfx/*.wav` were generated for this project with ElevenLabs Sound
Effects v2 (the prompts follow PLAN.md Appendix A) and prepared with
`tools/process-sfx.py`: trimmed, faded and levelled to the Appendix A spec.

## Music decoding

Music is decoded by [stb_vorbis](https://github.com/nothings/stb) (public
domain or MIT; see `src/third_party/stb/LICENSE`), vendored at the commit in
`src/third_party/stb/UPSTREAM` by `tools/update-stb.sh`. It is linked into the
PS5 application.

## OpenGL runtime

The PS5 build statically links the
[ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl) SDK
(GPL-3.0-or-later), which contains Mesa (MIT) and OpenGNM PSBC components under
their own licenses. The SDK is fetched or selected at build time and is not
stored in this repository; its license texts ship inside the SDK archive.

## Native build dependencies

The application build uses LLVM/Clang/lld, zlib 1.3.2, and the public
[PS5 payload SDK](https://github.com/ps5-payload-dev/sdk). The bootstrapper
downloads SDK v0.42 after verifying SHA-256
`8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da`.
It downloads zlib 1.3.2 from the upstream source archive after verifying
SHA-256 `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`
and compiles its static archive locally. Both dependencies remain under ignored
`.deps/native/`, retain their upstream licenses, and are not distributed by
this repository. No Sony SDK file is included.

Target C++ compilation uses the LLVM libc++ headers distributed by the public
SDK. Those headers retain the Apache-2.0 WITH LLVM-exception license recorded
upstream. The application does not redistribute or dynamically load the
complete libc++ or libc++abi archives.

The PS5 ELF converter and FSELF writer in `tooling/native/` are derived from
[SharpProspero](https://github.com/SvenGDK/SharpProspero), Copyright (C) 2026
SvenGDK, GPL-3.0, and were translated to C++ and modified by BlackBearReloaded.

## Host test dependency

The host unit-test target downloads
[GoogleTest](https://github.com/google/googletest) 1.17.0 after verifying
SHA-256 `65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c`.
It remains under ignored `.deps/test/`, retains its BSD-3-Clause license, and
is not linked into any PS5 application, runtime, or package artifact.

## PS5-Lapy-JB-Daemon helper

The build fetches
[mpereiraesaa's PS5-Lapy-JB-Daemon](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon)
at commit `54a095c0f19161825e845daa760a03b446e654fa`, invokes its unmodified
`owned-helper` target for title `PPSA99006` and packages the generated helper
(`lapy.elf`, `lapy-manifest.json`) with Lapy's MIT license
(`licenses/Lapy-MIT.txt`). The shared protocol header published upstream is
LGPL-2.1-or-later; the application-side client in `src/platform/ps5/` is the
PS5 Native App Boilerplate's, GPL-3.0-or-later.

Lapy was created by
[ArkSama / Team PHU](https://github.com/ArkSama/PS5-Lapy-JB-Daemon). Credit
belongs to ArkSama, mpereiraesaa and the Lapy contributors. No Lapy kernel
source is copied or modified here.

The helper build also uses the pinned `ps5log/1` header from
[mpereiraesaa/ps5-agc-gears](https://github.com/mpereiraesaa/ps5-agc-gears/tree/1ae1f9182abd2770c131b97419034fb85173c2dc/native/ps5log),
GPL-3.0-or-later, and the official PS5 Payload SDK v0.40. Those build inputs
remain under ignored `.deps/lapy/`; the application toolchain remains the
separately pinned Payload SDK v0.42.

## Update check and self-update

`src/update/` and `payloads/self-update-helper/` are the PS5 Native App
Boilerplate's update-check and self-update kits (GPL-3.0-or-later, original
BlackBearReloaded code from the ProsperoRadio, ProsperoLichess and
ProsperoStore projects); the helper carries ProsperoEden's additions. The
`gmtime_r` in `console_curl.c` follows Howard Hinnant's public-domain
`civil_from_days` algorithm.

The application statically links these libraries from the PacBrew prebuilt
ports (below), for HTTPS and the catalog's Ed25519 signature:

| Component | Version in PacBrew v0.40.2 | License |
| --- | --- | --- |
| [libcurl](https://curl.se/) | 8.18.0 | curl license (MIT/X derivative) |
| [OpenSSL](https://www.openssl.org/) | 3.5.2 | Apache License 2.0 |
| [zlib](https://zlib.net/) | 1.3.2 | zlib license |
| [zstd](https://github.com/facebook/zstd) | 1.5.6 | BSD-3-Clause (dual-licensed with GPL-2.0) |
| [libpsl](https://github.com/rockdaboot/libpsl) | 0.21.5 | MIT; built-in Public Suffix List data MPL-2.0 |

`third_party/miniz/` holds [miniz](https://github.com/richgel999/miniz) 3.0.2
(commit `293d4db1b7d0ffee9756d035b9ac6f7431ef8492`), MIT, unmodified, with its
`LICENSE`; `SOURCE.json` records each file's SHA-256. Only the self-update
helper (`self-updater.elf` in the package) and its host test link it.

## PacBrew dependencies

The build downloads the prebuilt ports image
from [ps5-payload-dev/pacbrew-repo](https://github.com/ps5-payload-dev/pacbrew-repo)
release `v0.40.2`, verifies its published SHA-256, and extracts only the
`target/user/homebrew` prefix under ignored `.deps/pacbrew/`. It does not
replace the pinned SDK or install files globally. PacBrew recipes and every
linked third-party library retain their upstream licenses.

## Optional UFS2Tool dependency

When `.ffpkg` output is requested, the platform bootstrapper fetches
[SvenGDK/UFS2Tool](https://github.com/SvenGDK/UFS2Tool) at commit
`b5307a60d5b4e3a68ba680e0e33cfadf05017c77` into the ignored
`.deps/UFS2Tool` cache and builds it with the host .NET SDK. UFS2Tool is
BSD-2-Clause software and is not distributed by this repository.

## Optional MkPFS dependency

When `.ffpfsc` output is requested, the platform bootstrapper fetches
[PSBrew/MkPFS](https://github.com/PSBrew/MkPFS) at commit
`6cb8313dfe0c988ac52617794553f343243d3a56` into the ignored `.deps/MkPFS`
cache and installs its Python dependencies into an ignored virtual environment
there. MkPFS and its dependencies retain their own licenses and are not
distributed by this repository.

## Independently authored runtime shim

`tooling/native/libc_builder.cpp` and the manifests under
`tooling/native/runtime/` are independently authored for this project and
licensed under GPL-3.0-or-later. The generated `runtime/libc.prx` contains
project-authored compatibility stubs, startup code, and semantic loader
metadata. It contains no Sony runtime implementation.

Original ps5-native-app-boilerplate code is Copyright (C) 2026
BlackBearReloaded and licensed under GPL-3.0-or-later. Source and script files
carry matching SPDX identifiers.

## Original presentation assets

The BlackBear icon, selection artwork, and default selection track
`sce_sys/snd0.at9` are original assets supplied by BlackBearReloaded, Copyright
(C) 2026 BlackBearReloaded, and distributed under GPL-3.0-or-later. The track
is titled `Night Drive`.

No proprietary runtime module, encryption key, or game file is included.
