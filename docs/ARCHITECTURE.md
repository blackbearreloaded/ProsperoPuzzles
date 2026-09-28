# Architecture

ProsperoPuzzles is one native PS5 title that hosts many puzzle games behind a
shared shell. The full design and milestones are in [PLAN.md](../PLAN.md); this
page records the durable structure as it lands.

## Origin

- Created from the `ps5-native-app-boilerplate` template at
  `4f531c4b517f80bcb6b1267135848168d2250047` (2026-09-27, "Speed up native
  builds with Ninja and ccache; package app folder ZIP").
- Development title `PPSA99031`; the release title is `PPSA99030`.

## Layers

| Layer | Location | Owns |
| --- | --- | --- |
| Entry and lifecycle | `src/main.cpp` | Startup order, frame loop, exit path |
| Runtime glue | `src/runtime/` | Heap, log redirect, libc gaps |
| Platform | `src/platform/ps5/` | Display (EGL/GL 4.6), pad, audio out, storage, system services |
| Core | `src/core/` | Scene stack, input mapping, settings, saves, library model |
| Graphics | `src/gfx/` | 2D batcher, fonts, canvas, tweening, particles |
| UI | `src/ui/` | Widgets and shell scenes |
| Audio | `src/audio/` | Mixer, clips, music streaming, cue manifest |
| Games | `src/games/` | Game modules: `g2048`, `tenfold`, and the Tatham bridge `sgt` |
| Third party | `src/third_party/` | Vendored upstream code, unmodified except `patches/` |

Everything under `src/` is compiled for the PS5 by `tools/build.sh`. Host-only
code (the preview runner) lives under `host/`.
