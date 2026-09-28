# OpenGL integration

ProsperoPuzzles renders with OpenGL 4.6 Core through EGL, provided by the
statically linked [ps5-opengl](https://github.com/blackbearreloaded/ps5-opengl)
SDK. This page lists every change made to the boilerplate for it, so each one
can be re-checked when the boilerplate or the SDK is updated.

## SDK selection

| Setting | Value |
| --- | --- |
| Default SDK | ps5-opengl release `v0.3.0`, fetched by `tools/fetch-opengl-sdk.sh` |
| Archive SHA-256 | `a7bd6b85f00398eaf8d87ecc58fa0bde7cab79e065acc783268070d2f14b403c` |
| `sdk/manifest.sha256` SHA-256 | `51dc817d12d8369423bd95956df088dc6c936e742a96cc3a9bd0e966fa000391` |
| Display profile | 1920×1080 at 60 Hz (`ps5_opengl_display.h`) |
| Override | `make PS5_OPENGL_PREFIX=<dir containing manifest.sha256>` |

The v0.3.0 release notes state that its binaries are host-checked, not
console-validated. The first hardware runs (PLAN.md, M1) decide which SDK the
project pins.

`tools/prepare-opengl.sh` verifies the selected SDK's manifest, points
`.deps/ps5-opengl/current` at it, and writes the linker group
`.deps/ps5-opengl/libps5opengl-group.a`:

- `EXTERN(ps5_agc_gate2_run)`, so the runtime entry reached only through the
  driver's dispatch table is kept;
- `libPS5OpenGL.a` (the SDK's own group of 17 archives);
- the payload SDK's `libunwind.a`, `libc++abi.a`, `libc++.a` and the Clang
  builtins archive.

## Build changes

| File | Change | Why |
| --- | --- | --- |
| `Makefile` | Always adds `GL_GLEXT_PROTOTYPES=1`, the SDK include path, the link group and the AGC import stubs to the `APP_*` variables; `app`/`ffpkg`/`ffpfsc`/`packages` depend on `opengl` | Every output links the same runtime |
| `tools/build.sh` | `APP_IMPORT_STUBS`: extra `.so` import libraries are linked and passed to the converter with `--stub` | `libSceAgc`/`libSceAgcDriver` are not in the payload SDK |
| `tools/build.sh` | Links with `--wrap` for `malloc`, `calloc`, `realloc`, `free`, `posix_memalign`, `malloc_usable_size` when `src/runtime/app_heap.c` exists | Routes allocations into the fixed OpenGL heap |
| `tools/build.sh` | Builds `src/third_party/**` with `-w` | Vendored code is compiled as published |
| `tooling/native/sce_module_writer.cpp` | Process heap size `0x10000000` instead of unbounded | ps5-opengl native-app recipe |
| `tooling/native/ps5-pie.ld` | Adds `__eh_frame_start/end` and `__eh_frame_hdr_start/end` | libunwind in the runtime locates unwind tables with them |
| `tooling/native/app-symbols.map` | `local: *;` | ps5-opengl native-app symbol policy |
| `sce_sys/param.json` | Adds `amm` (512 GiB VA ranges, 256 MiB page-table memory) and `kernel` (256 MiB CPU and GPU page tables) | ps5-opengl native-app memory layout |

## Runtime pieces

| File | Role |
| --- | --- |
| `src/runtime/app_heap.c` | 128 MiB `sceLibcMspace` heap behind the `--wrap` allocator |
| `src/runtime/runtime_shims.c` | Log receipt at `/download0/prosperopuzzles/app.log`, never-return `catchReturnFromMain`, Mesa TLS stub, libc gaps |
| `src/platform/ps5/display_egl.cpp` | EGL display, window surface (native handle 0, NULL attributes), GL 4.6 Core context, swap interval 1 |

## Rules for rendering code

- Open the display once for the process lifetime; reopening a presenter is slow.
- Use triangle lists; avoid triangle fans/strips and `GL_POINTS`.
- Orphan streamed buffers (`glBufferData(..., NULL, ...)`) before rewriting them.
- Keep one mip level on textures used every frame.
- Keep draw calls few; batch with instancing.
- Shader sources carry no `#version` line; `gfx::build_program` prepends it.
