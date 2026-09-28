# Architecture

ProsperoPuzzles is one native PS5 title that hosts many puzzle games behind a
shared shell. The full design and milestones are in [PLAN.md](../PLAN.md); this
page records the durable structure as it lands.

## Origin

- Created from the `ps5-native-app-boilerplate` template at
  `4f531c4b517f80bcb6b1267135848168d2250047` (2026-09-27, "Speed up native
  builds with Ninja and ccache; package app folder ZIP").
- Title `PPSA99006` for both console testing and releases.

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

## Threads

| Thread | Work |
| --- | --- |
| Main | Pad read, shell update, music decode (`MusicPlayer::pump`), draw, `eglSwapBuffers` (interval 1) |
| Audio | `sceAudioOut` grains of 256 frames from `audio::Mixer::render` |

The main thread talks to the audio thread only through the mixer's lock-free
command queue and the music `StreamRing`s (one producer, one consumer).

## Tatham skin

The Tatham games draw through `sgt::CanvasRenderer` into a persistent 4x MSAA
canvas. `sgt_skin` restyles them without touching upstream code:

- `restyle_palette` maps each game's palette onto the app's colours (board,
  navy ink, the 2048/Tenfold hues), with per-game overrides for colours the
  generic mapping cannot place.
- `style_for` turns on shape treatments per game: rounded tiles, flat discs,
  and bevel-to-card conversion (Fifteen, Sixteen).

`PPZ_SNAPSHOT_ALL=1 tools/host-snapshots.sh` renders every puzzle for review.

## Music

`audio::MusicPlayer` picks `menu_main`, `game_<id>`, or the calm/upbeat
playlists, loads the OGG into memory, and decodes with `stb_vorbis` about
0.7 s ahead into a `StreamRing` per deck. The mixer plays the two decks on the
music bus; track changes crossfade over 1.5 s and completion stings duck the
music by 6 dB.

## Presentation art

`tools/render-art.sh` renders `icon0.png` and the pic0/pic1 backgrounds with
the app renderer (`host/art.cpp`) at 3840x2160 and converts them with texconv.
