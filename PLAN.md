# ProsperoPuzzles: implementation plan

This is draft 2, dated 2026-09-27. It adds the Tatham collection, the A–Z library with favorites, the polish bar and the audio asset spec.

ProsperoPuzzles will be a native PS5 homebrew title. It hosts a collection of puzzle games behind a polished launcher that you drive with the controller. You pick a game, play it, go back to the library and pick another.

**v1 ships 42 games:**
- The 40 puzzles of **Simon Tatham's Portable Puzzle Collection** (MIT), played through our own PS5 front end.
- **2048** and **Tenfold**, ported from the WIP projects.

All games appear in **one alphabetical library**. The player can mark any game as a favorite. The target presentation is AAA: animated, fully voiced with sound, and without hitches. **You supply the sound effects and music**, following Appendix A; synthesized placeholders stand in until then.

It is built on `ps5-native-app-boilerplate` and on `ps5-opengl` (OpenGL 4.6 Core). Input, audio and video follow the ProsperoLight and Quake II native implementations. Console work follows `ps5-agent-runbook`.

---

## 1. Scope

**v1 includes:**
- **The library:** every game sorted A–Z, favorites pinned first (D10), game details, settings, how-to-play pages and an about/licenses screen.
- **The 40 Tatham puzzles** (section 5.9) plus 2048 and Tenfold as game modules.
- Shared input, audio, rendering and save services.
- Resuming a game in progress, plus best scores, best times and statistics per game and preset.
- **The polish bar** (section 5.10), and your audio assets once delivered (Appendix A).
- Presentation art: icon0, pic0, pic1 and snd0.
- Host unit tests and a host preview runner.
- A tagged `01.000.000` release.

**v1 excludes:**
- Online features and trophies.
- The SaveData API. `/savedata0` has not been validated; we use `/download0` only.
- Multiple users, touchpad gestures, IME text entry and HDR.

---

## 2. Sources and what we take from each

| Source | Revision | What we take | What we leave |
|---|---|---|---|
| **ps5-native-app-boilerplate** | GitHub template, HEAD `4f531c4` | Repo skeleton, clang-18 wrapper, native ELF→FSELF tool, `libc.prx` shim, Makefile, CI, lint, `param.json` validation, presentation-asset pipeline, GoogleTest wiring | `src/demo_renderer.*` and its test |
| **ps5-opengl** | HEAD `122aa89`, release `v0.3.0` | GL 4.6 Core static SDK with EGL. Also its native-app integration recipe: `tools/build-native-test-app.sh` and `native-app/{app_heap.c, runtime_shims.c, ps5-pie.ld, app-symbols.map, param.json}` | Examples, except the GL 4.6 demo, which we use as a hardware baseline |
| **ProsperoLight** (PPSA99002) | `143561a` | See the ProsperoLight list below | SDL software renderer, RmlUi, the AGC presenter and its recovered shader blobs, streaming code |
| **ps5-yamagi** (Quake II, PPSA99007) | `bc40477` | See the Quake II list below | The private Core 3.3 SDK with its 17 runtime patches, SDL2 and all upstream Yamagi code |
| **Simon Tatham's Portable Puzzle Collection** | `git.tartarus.org/simon/puzzles`, commit `616da16` (2026-09-22) | The platform-neutral midend, the shared solver and generator code, and all 40 game back ends. Also each game's display name, description and objective (from `CMakeLists.txt`) and the rules in `puzzles.but`. | Every front end (`gtk.c`, `windows.c`, `emcc.c`, `osx.m`, `nullfe.c`, `kaios/`), the printing path and the auxiliary tools |
| **ps5-2048** (WIP, PPSA99020, no commits) | working tree | `src/game_core.hpp` and its tests, board design and animation, synth sound cues and ambient pad, save codec, `SaveWorker` pattern | `main()`, EGL, SDL, its own heap |
| **ps5-tenfold** (WIP, PPSA99751) | `f446dc9` | `src/game_core.hpp` and its tests, board design and animation, synth sound cues, save codec, `tools/generate-art.py` | `main()`, EGL, SDL, heap, shims |

**What we take from ProsperoLight:**
- Native pad input: `src/radio_input.cpp`.
- The typed 120-byte pad sample and button bits: `moonlight_stream.cpp:80-97,300-320`.
- The pad-open retry loop: `moonlight_stream.cpp:1905-1935`.
- Reconnect handling: `moonlight_stream.cpp:2584-2590`.
- Native AudioOut setup and teardown: `moonlight_stream.cpp:1718-1772,1859-1880`.
- The atomic save format: `moonlight_config.cpp:122-181,204-307`.
- Splash hand-off: `main.cpp:1016-1052`.
- klog output: `moonlight_stream.cpp:568-594`.
- The thread-naming pitfall: `platform/ps5/ps5_compat.h:12-22`.

**What we take from ps5-yamagi (Quake II):**
- EGL bring-up and ordered teardown: `src/ps5/ps5_egl.c`.
- Exit and the writable-directory probe: `ps5_main.c:18-60`.
- The monotonic clock and JSONL receipt: `ps5_lifecycle.c`.
- The `native-app/` helpers.
- The build recipe: `tools/build-ps5.sh`.
- The GL performance rules (section 5.5).
- The host-check style in `checks/`.

**Licensing:**
- Our code is GPL-3.0-or-later. Code copied from ProsperoLight and from the Yamagi port's own `src/ps5` / `native-app` files is the owner's own GPL-3.0 work. **Nothing is copied from upstream Yamagi.**
- The Tatham collection is MIT, which is compatible. Its `LICENCE` text must ship with the app, so it goes in the About screen and in `THIRD_PARTY_NOTICES.md`.

---

## 3. Findings that shape the plan

1. **Neither WIP game has shown a frame on hardware.**
   - Tenfold's only launch got a GL 4.6 context at 1920×1080 and compiled its shaders.
   - Its first `eglSwapBuffers` then failed with **`EGL_BAD_SURFACE` (0x300d)**. That build used a local SDK built on Sept 20.
   - 2048's hardware status is unknown. Its title ID PPSA99020 has been reused, and a SIGSEGV was logged under it.
   - → **The first milestone is a present loop proven on hardware, before any porting.**
2. **GL 4.6 has less hardware evidence than GL 3.3.**
   - Quake II proves EGL on ps5-opengl presents reliably, including 2160p120 gameplay, but on a private Core 3.3 SDK.
   - The GL 4.6 SDK's hardware evidence comes from ps5-opengl's own demos (PPSA99005).
   - → Run ps5-opengl's GL 4.6 demo on our console with our chosen SDK first. Any later failure is then down to our integration.
3. **The two games cannot be linked together as they stand.**
   - Both are built the same way: a pure, header-only `game_core.hpp`, a state machine inside `main()`, and their own EGL context, SDL setup, audio and heap.
   - Both use `namespace game` with the same type names (`Game`, `Snapshot`, `Renderer`, `Platform`, and others).
   - Both call `SDL_Quit` and `eglTerminate` on shutdown.
   - → Each becomes a namespaced module. The shell owns the display, input, audio, heap and storage.
4. **ps5-opengl's docs pin a boilerplate commit that no longer exists.**
   - The pinned commit is `4e1d127`, and the boilerplate's history has been rewritten since.
   - The `sed` anchors its assembler uses still match the current HEAD.
   - → Apply the integration changes directly in our repo, with each change documented.
5. **The boilerplate `make deploy` does not satisfy the runbook.** It never reads files back or compares hashes.
   - → Add a verified deploy (section 7).
6. **A launch fails with 0x80940033 if `pic0.dds`, `pic1.dds` or `snd0.at9` is missing.** 2048 lacks them. The template ships defaults, so keep those until M7.
7. **Taps and input polling.**
   - ProsperoLight reads the pad natively with `scePadRead`, taking up to 64 buffered samples per call, and plays audio natively through `sceAudioOut`. Both paths are hardware-proven.
   - Quake II goes through PacBrew SDL2 instead. It needed a 4 ms polling thread because polling once per frame lost short taps.
   - A buffered batch read keeps every sample, so it avoids that problem without SDL.

---

## 4. Decisions (defaults; flag any you want changed)

| # | Decision | Default | Why | Alternative |
|---|---|---|---|---|
| D1 | Input and audio backend | **Native `scePad` + `sceAudioOut`, as in ProsperoLight** | Avoids SDL entirely: no ~346 MB PacBrew sysroot, no second VideoOut owner, no `SDL_Quit` hazard. Batch reads keep taps. | PacBrew SDL2, as in Quake II, 2048 and Tenfold |
| D2 | Display profile | **SDK profile 1920×1080 at 60 Hz first; evaluate 3840×2160 at 120 Hz in M7** | It is the default profile and the lowest risk. The UI is resolution-independent, and puzzles don't need 120 Hz. | 4K120 from the start, as 2048 did |
| D3 | UI technology | **Custom `gfx2d`:** instanced SDF rounded rectangles plus an SDF font atlas, a few draw calls per frame | Both games already draw this way. Batching is the main performance lever on ps5-opengl. Gives full control over look and animation. | Dear ImGui (proven on ps5-opengl, but looks like tooling). RmlUi (needs a GL backend plus RTTI and exceptions). NanoVG (uses triangle fans and strips and many draws, which fall off the fast path). |
| D4 | SDK | **Pin one SDK by the hash of its `manifest.sha256`.** Candidates: the v0.3.0 release asset, a local build of `122aa89`, or the C91 candidate. | Chosen by the M1 hardware runs | — |
| D5 | Repository | **`blackbearreloaded/ProsperoPuzzles`**, GPL-3.0-or-later, created from the template, cloned to `~/ProsperoPuzzles` in WSL. **Private until you say to make it public.** | The other Prospero apps are public, but publishing needs your explicit go-ahead | Public from day one |
| D6 | Title IDs | **Release PPSA99030, development PPSA99031.** The runbook wants a separate development title. Verify both are unregistered before first use. | Already taken: 99001–99005, 99007, 99008, 99020, 99021, 99751, 99997, 99999, 88900 | — |
| D7 | Importing the WIP code | **Copy the code without history,** one import commit per game naming its source | 2048 has no commits. Tenfold's history is the boilerplate plus 5 commits. | git subtree |
| D8 | Commits | One-line imperative subject, pushed to `main` as BlackBearReloaded, no trailers | Your standing instruction | — |
| D9 | How we take the Tatham code | **Vendor a pinned snapshot** into `src/third_party/sgt-puzzles/`, with one local patch: a move-notification hook in `midend.c` so we can drive sound effects. `tools/update-sgt-puzzles.sh` re-syncs it. | Upstream is still maintained, and a single tiny patch rebases easily | A fork that re-skins every game |
| D10 | Library ordering | **One grid. Favorites come first as their own section, sorted A–Z. The remaining games follow A–Z.** Sorting ignores case and puts digits first, so 2048 leads. Toggling a favorite animates the card to its new slot. | Keeps "alphabetical" and "favorites" both visible without duplicating cards | Separate All and Favorites tabs |
| D11 | Music codec | **OGG Vorbis, streamed and decoded by `stb_vorbis`** (public domain/MIT) on the audio side. Sound effects are WAV. | Small, and needs no system codec. The system AT9 decoder is not validated in-app. | Uncompressed WAV music, which costs about 34 MB per 3 minutes |
| D12 | How far "AAA" goes | **Every game gets the full treatment:** anti-aliased rendering at 4K quality, curated palettes, animation and the shell's transitions, full sound, and a completion celebration. **Tier 1 puzzles** get bespoke skins in M7, time-boxed: Flood, Mines, Net, Same Game, Solo, Pattern, Fifteen and Light Up. | Re-skinning all 40 by hand isn't realistic for v1 | — |

---

## 5. Target architecture

### 5.1 Repository layout

```
ProsperoPuzzles/
  Makefile  .env.example  build.ps1  README.md  PLAN.md  THIRD_PARTY_NOTICES.md  CHANGELOG.md  LICENSE
  sce_sys/     param.json icon0.png pic0.dds pic1.dds snd0.at9 (+ *-source.png)
  assets/      fonts/*.sdf  audio/{sfx,music}/  audio/manifest.json  help/*.txt  -> /app0/assets
  third_party/ sgt-puzzles-docs/ (puzzles.but + LICENCE; not compiled)
  patches/sgt/ 0001-midend-move-hook.patch (the only change to upstream Tatham code)
  src/         (tools/build.sh compiles EVERY .c/.cc/.cpp under src/ for PS5)
    main.cpp                       entry point; owns the lifecycle
    runtime/app_heap.c             128 MiB mspace + malloc-family --wrap (ps5-opengl native-app)
    runtime/runtime_shims.c        log redirect, Mesa TLS stub, libc stubs, catchReturnFromMain
    platform/platform.hpp          interfaces: Display, Pad, AudioOut, Storage, System, Clock
    platform/ps5/display_egl.cpp   EGL + GL 4.6 (from Yamagi ps5_egl.c)
    platform/ps5/pad.cpp           scePad (from ProsperoLight radio_input.cpp)
    platform/ps5/audio_out.cpp     sceAudioOut thread (from ProsperoLight stream audio)
    platform/ps5/storage.cpp       /download0 atomic files
    platform/ps5/system.cpp        splash, exit, klog, notification toast, monotonic clock
    core/     app (scene stack + frame loop), input_map, repeat, settings, save_container,
              save_worker, rng, log
    gfx/      batch2d, shaders, font, tween/easing, color, theme
    ui/       widgets (panel, button, list, toggle/slider row, dialog, toast, hint bar),
              scenes (boot, home, settings, pause, howto, about, diagnostics)
    audio/    mixer, synth, clip (WAV) loader, cue table
    games/    game_module.hpp, registry.cpp
    games/g2048/    core.hpp  scene.hpp/.cpp  codec.cpp
    games/tenfold/  core.hpp  scene.hpp/.cpp  codec.cpp
    games/sgt/      frontend bridge: drawing API -> canvas, input translation, midend
                    services, sound classifier, metadata table (names, taglines, waves)
    third_party/sgt-puzzles/   vendored upstream subset: midend, core libs, 40 game .c files,
                               list.c, generated-games.h (build flags: -DCOMBINED)
    third_party/stb/           stb_vorbis.c, stb_truetype.h (build-time font baking only)
  host/        host-only platform backends + preview runner (must stay outside src/)
  tests/       GoogleTest host tests + boilerplate tooling tests
  tools/       boilerplate tools + prepare-opengl.sh, fetch-opengl-sdk.sh, bake-font.py,
               generate-art.py, deploy-verified.sh, inspect-save.py
  tooling/     boilerplate native toolchain (+ the OpenGL changes in 6.M1)
  docs/        boilerplate docs + ARCHITECTURE.md, OPENGL_INTEGRATION.md, HARDWARE_TESTING.md
```

### 5.2 Runtime model

**Threads**

| Thread | Job |
|---|---|
| **Main** | Drain the pad, update the scene stack, build the `gfx2d` batch, draw, then call `eglSwapBuffers`. The swap interval is 1, so the swap paces the loop. |
| **Audio** | Mix a 256-frame grain, then call `sceAudioOutOutput`. The call blocks, so the thread paces itself. |
| **Save** | A coalescing writer: 2048's `SaveWorker` rebuilt on pthread, mutex and condvar instead of SDL. |

- Create threads with `pthread_create`. **Never call `pthread_setname_np`**; it hangs on PS5.
- Take frame time from a monotonic clock and clamp `dt` to 50 ms.

**Display lifetime**
- Keep **one EGL display, surface and context for the whole process.**
- ps5-opengl makes a high-refresh presenter wait 5 s before it can be reopened. It also reuses GL object names after a teardown (Yamagi pitfall 7).

**Exit**
- **Never return from `main`.**
- The only exit path is `fflush(NULL)` followed by `sceSystemServiceLoadExec("exit", NULL)`, as in Yamagi `ps5_main.c:18-26`. libc `exit()` triggers SIGSYS.
- The fallback is to sleep forever and let the shell close the title.
- On Tenfold, `LoadExec("exit")` logged 0x80aa001a, although the title still closed. M4 must verify it.

**Saving**
- **Save when something changes, not on quit.** Closing from the shell and a GPU fault (`_Exit`) both skip any quit handlers.

**C++ profile**
- The boilerplate's rules apply: C++20, `-fno-exceptions -fno-rtti`, no `shared_ptr`, streams, locale or filesystem.
- Use `unique_ptr`, `std::array` and fixed buffers. Keep large buffers on the heap, not the stack.

### 5.3 Game module interface

```cpp
namespace puzzles {
struct Services {                 // owned by the shell, lent to the active game
    gfx::Batch& gfx; audio::Mixer& audio; SaveService& saves;
    const Settings& settings; const Clock& clock; Rng seed_source;
};
enum class SceneRequest { none, open_pause, exit_to_menu };

class GameScene {
public:
    virtual ~GameScene() = default;
    virtual void enter(Services&, bool resume) = 0;       // load save / start new
    virtual void update(const InputFrame&, double dt) = 0;
    virtual void draw(gfx::Batch&, const Rect& area) = 0;
    virtual SceneRequest request() = 0;                   // polled by the shell
    virtual void pause_items(PauseMenu&) = 0;             // New game, Undo, Hint...
    virtual void on_pause_item(int id) = 0;
    virtual void persist() = 0;                           // enqueue save now
};

struct GameInfo {
    const char* id;               // "g2048", "tenfold" -> save file names
    const char* title; const char* tagline; gfx::Color accent;
    void (*draw_thumbnail)(gfx::Batch&, Rect, const SaveSummary&, float t); // live mini board
    bool (*read_summary)(SaveService&, SaveSummary*);     // best, resumable?, stats
    std::unique_ptr<GameScene> (*create)();
    const HowToPage* howto; int howto_pages;
};
std::span<const GameInfo> registry();                     // static table, unique ids (tested)
}
```

### 5.4 Input model

**Controller mapping** (button bits from ProsperoLight):

| Button | Bit | Logical action |
|---|---|---|
| D-pad Up / Right / Down / Left | 0x10 / 0x20 / 0x40 / 0x80 | `nav` direction (repeat is per context) |
| Cross | 0x4000 | `confirm` (swaps with Circle if the setting is on) |
| Circle | 0x2000 | `back` |
| Triangle | 0x1000 | `action_north` (key palette in Tatham games; game details in the Library) |
| Square | 0x8000 | `action_west` (secondary action in Tatham games; Hint in Tenfold; toggle favorite in the Library) |
| L1 / R1 | 0x400 / 0x800 | `page_prev` / `page_next` (Undo / Redo in every game; filter chips in the Library) |
| L2 / R2 | *verify on hardware* | letter jump in the Library. ProsperoLight never defined digital L2/R2 bits, so these can fall back to the analog trigger bytes. |
| Options | 0x8 | `menu`, which opens pause |
| Touchpad click | 0x100000 | `menu`, as an alias |
| *(system has input)* | 0x80000000 INTERCEPTED | treat as all released, raise `focus_lost` |

**Left stick**
- It stands in for the D-pad using the dominant axis.
- An axis counts only when it is below 64 or above 192 on the 0–255 range. That is ProsperoLight's deadzone.

**Repeat**
- The helper waits 350 ms before the first repeat, then repeats every 110 ms.
- It is enabled for menus and for Tenfold's cursor.
- It is **disabled for 2048 slides**; the WIP version slid repeatedly while the button was held.

**Disconnect and interception**
- Either one triggers `focus_lost`.
- The shell responds by opening the pause menu and saving. It must not cancel anything; the 2048 WIP bug was that a disconnect acted as "cancel".

**Confirm button**
- ProsperoLight has no regional swap. We add a **"Swap Cross/Circle"** setting.
- Reading the system's enter-button parameter can come later, once we have verified the system parameter's ID.

`InputFrame` carries these for each logical action:
- `pressed` / `released` / `held` bitmasks
- `nav` (a direction, plus whether it repeated)
- `focus_lost`

### 5.5 Rendering model (`gfx2d`)

**Canvas**
- A **virtual 1920×1080 canvas**, scaled and letterboxed to the size `eglQuerySurface` reports.
- No fixed-pixel constants. 2048 had several, plus a lock to 4K120.

**Batching**
- **One instance format** merging the 2048 and Tenfold versions. Each instance carries:
  - rect
  - top and bottom colors (gradient)
  - corner radius
  - border width and color
  - shadow
  - UV coordinates
  - mode: shape, glyph, ring, triangle or line
- The ring, triangle and line modes draw the PlayStation face-button symbols in the hint bar without any textures.
- One VAO and one program. Attribute divisor 1, drawn with `glDrawArraysInstanced(GL_TRIANGLES, 0, 6, n)`.
- The instance buffer lives on the heap with room for 16k instances.
- Layers (background, content, overlay, toast) are flushed in order. A scissor clip splits the batch.

**Fonts**
- The SDF atlas is **baked offline** with stb_truetype by `tools/bake-font.py` from an OFL font. Inter and Montserrat are candidates; ProsperoLight already ships both.
- It is stored as raw R8 pixels plus a metrics table, so the console needs no image decoder.
- Coverage: ASCII, lowercase and punctuation, with kerning, measure/wrap/align, and tabular digits for scores.

**No textures in v1.** Game thumbnails are live mini-renders of each game's board.

**ps5-opengl performance rules** (Yamagi findings):
- **Triangle lists only.** Triangle fans and strips leave the batched native path.
- **No `GL_POINTS`.** Use instanced quads; points cost 26–30 ms per frame.
- **Orphan the stream VBO every frame.** Call `glBufferData(size, NULL)` before `glBufferSubData`; reusing a buffer at the same size forces a drain.
- **Cache uniform locations at init.** Both WIP renderers looked them up every frame.
- **Clear once per frame.** No depth buffer is needed.
- **Single mip level, linear filtering** for the font atlas. Mip chains fall off the fast path.
- **Keep the number of draw calls low.** Draw count dominates cost.
- **Shader sources carry no `#version` line.** The platform prepends `#version 460 core` on PS5 and whatever the host's llvmpipe supports.
- **Shaders compile at runtime on the console.** Optionally set `PS5_SHADER_CACHE_DIR` to a folder under `/download0/prosperopuzzles/` before creating the context.

### 5.6 Storage

**Location**
- The root is `/download0/prosperopuzzles/`. It needs `downloadDataSize` 256 in `param.json`.
- At boot, run `mkdir 0700` followed by a write probe, as in Yamagi `ps5_main.c:43-59`.

**Files**

| File | Contents |
|---|---|
| `settings.bin` | Music and SFX volume as integers 0–10, reduced motion, Cross/Circle swap, last focused game, show-FPS (debug) |
| `library.bin` | The favorite game IDs, and when each game was last played |
| `stats.bin` | For each game and preset: games played, games solved, best time, best score and current streak |
| `g2048.sav`, `tenfold.sav` | The game in progress (resumable), undo snapshot, best score |
| `sgt/<game>.sav` | A Tatham game in progress: `midend_serialise` output (text), wrapped in our container |
| `sgt/<game>.prefs` | That game's Tatham preferences, via `midend_save_prefs` |
| `app.log` | stdout and stderr, via `runtime_shims.c` |

**Container format**
- Layout: `'PPZL'` magic, `u16` version, `u16` kind, `u32` payload size, the payload, then CRC32.
- The reader rejects a wrong size, wrong CRC or trailing bytes. It migrates older versions on load, as ProsperoLight's config loader does.

**Writing**
- Write to a temp file, fsync, close, then rename. If the rename fails, unlink and retry, as in ProsperoLight's `write_atomic`.
- All writes go through the save thread, coalescing to the latest pending state.

**Host tool:** `tools/inspect-save.py` decodes every file type. We use it on files pulled from `download0.dat`.

### 5.7 Audio

**Output port**
- Call `sceAudioOutInit`. Treat 0x8026000e ("already initialized") as success.
- Then call `sceAudioOutOpen` with these arguments:
  - user `0xff` (system)
  - type `0` (MAIN)
  - `0`
  - 256 frames
  - 48000 Hz
  - format `1` (S16 stereo)

**Mixer**
- 32 voices, of three kinds:
  - PCM clips: your WAV files, decoded at load time.
  - Synth placeholders: sine or triangle waves with attack and release, merged from the 2048 and Tenfold synths.
  - Music: two streaming Vorbis decks, so tracks can crossfade.
- **Buses:** master, music, SFX and UI, then a soft limiter at −1 dBFS.
- Always output a full grain, and emit silence when there is nothing to play; never replay a stale buffer.
- The game thread talks to the audio thread through a lock-free SPSC command ring. The audio thread takes no locks.

**Music streaming**
- A decoder thread keeps about 2 s of PCM buffered for each deck.
- `stb_vorbis` opens tracks through `/app0` file I/O.
- Loop points come from the `LOOPSTART` / `LOOPLENGTH` Vorbis comments. With no comments, the whole file loops.
- Tracks crossfade over 1.5 s. Music ducks by 6 dB under completion stings.

**Cues are driven by a manifest** (`assets/audio/manifest.json`)
- The manifest maps each cue to its files, gain, pitch jitter and round-robin variations.
- A game-specific cue (for example `mines.reveal`) overrides the generic one (`reveal`).
- **If an asset is missing, the cue falls back to a synthesized placeholder.** That lets development go ahead before your files arrive, and dropping them in later needs no code changes.
- The full cue list and format rules are in **Appendix A**.

### 5.8 Shell UX

**Boot**
- Draw the first frame (the wordmark), then call `sceSystemServiceHideSplashScreen()`.
- Hold, then fade over about 1.5 s. This mirrors ProsperoLight, and it gives a TV time to resync when we later switch to 120 Hz.

**Library** (the home screen)

*Layout*
- A header with the wordmark, the game count and filter chips: **All · Favorites · In progress**.
- Below it, a **grid 6 cards wide** that scrolls smoothly. The camera keeps the focused row centered.
- Ordering follows D10:
  - a "★ Favorites" section first, sorted A–Z
  - then "All games", sorted A–Z
  - Sorting ignores case and puts digits first.
- An **A–Z index rail** on the right edge highlights the current letter.
- The background is an animated gradient with soft drifting shapes, tinted by the focused game's accent color.
- A hint bar draws the face-button symbols.

*Each card shows*
- a live thumbnail: the game in progress, or a board generated from a fixed seed
- the display name, and a tagline taken from Tatham's description (for example "Solo · Sudoku")
- a star if it's a favorite
- an "In progress" chip when there is a game to resume
- the best time or best score

*Card animation*
- The focused card springs to 1.06× scale, lifts its shadow and plays a sheen sweep.
- Toggling a favorite bursts the star and **animates the card to its new position** (FLIP layout animation).

*Library controls*

| Button | Action |
|---|---|
| D-pad / stick | Move between cards (with repeat) |
| L1 / R1 | Change filter chip |
| L2 / R2 | Jump to the previous or next letter |
| Cross | Play, or Resume. The card zooms into the board, as a shared-element transition. |
| Square | Toggle favorite |
| Triangle | Game details: rules, controls, presets, statistics, New game |
| Options | Settings |

**Game screen**
- The game canvas sits on a lit "paper" card over the dimmed, animated background.
- The HUD shows the game name, the preset, the timer or score, and Tatham's status-bar text.
- It also has a controls strip that changes with context: cursor mode, pointer mode or the key palette.

**Pause overlay** (Options, or `focus_lost`)
- The overlay dims and desaturates the game and shows a panel with these items:
  - Resume
  - New game, Restart, Undo and Redo
  - Difficulty (Tatham presets) and Custom (Tatham configuration, as stepper rows)
  - Solve (Tatham, when `can_solve`; the run is marked as assisted in statistics)
  - Game options (Tatham preferences)
  - How to play
  - Settings
  - Return to library, which saves the game so it can be resumed
  - For 2048 and Tenfold, the game's own items replace the Tatham ones
- Win and game-over dialogs use the same dialog widget.

**Other screens**
- **Settings:** volumes, reduced motion, the Cross/Circle swap, and Reset statistics (asks to confirm).
- **About:** version (`contentVersion`), credits, and licenses: GPL-3.0, Mesa (MIT), ps5-opengl, stb, the font's OFL, and credit for the 2048 concept.
- **Diagnostics:** a hidden screen opened with L3+R3 on About. It shows button state live, a tone test, FPS and frame-time histograms, and heap statistics.

**Transitions**
- 250 ms eased fades and slides between scenes.
- With reduced motion on, these become 0 ms cross-cuts.

### 5.9 Tatham collection integration

**Build**
- Vendor these files from the upstream snapshot:
  - the core library: `combi divvy draw-poly drawing dsf findloop grid latin laydomino loopgen malloc matching midend misc penrose penrose-legacy random sort tdq tree234 version`
  - `hat.c` and `spectre.c`
  - the 40 game files
  - `list.c`
  - a `generated-games.h` that we generate ourselves
- Add `COMBINED` to `APP_DEFINITIONS`. In a combined build each game exports its own `const game`, and `gamelist[]` / `gamecount` enumerate them all.
- The boilerplate compiles everything under `src/` as C11, which Tatham's code supports.
- Bump the stack of the generation worker thread (for example to 8 MiB) for the recursive solvers.

**The 40 puzzles**
- Black Box, Bridges, Cube, Dominosa, Fifteen, Filling, Flip, Flood, Galaxies, Guess
- Inertia, Keen, Light Up, Loopy, Magnets, Map, Mines, Mosaic, Net, Netslide
- Palisade, Pattern, Pearl, Pegs, Range, Rectangles, Same Game, Signpost, Singles, Sixteen
- Slant, Solo, Tents, Towers, Tracks, Twiddle, Undead, Unequal, Unruly, Untangle

**Drawing: implementing `drawing_api` v1 onto a persistent canvas**
- Tatham redraws **incrementally**, so each game draws into a persistent FBO the size of the puzzle.
- The FBO uses **4× MSAA**. ps5-opengl supports up to 4 samples, and MSAA avoids the seams where anti-aliased shapes meet. If MSAA turns out slow on hardware, fall back to 2× supersampling.
- The FBO is resolved and composited every frame, underneath our overlays.
- How each part of the API maps onto our renderer:

| Tatham call | Our implementation |
|---|---|
| `draw_rect` | Pixel-exact quads |
| `draw_line`, `draw_thick_line` | Quads with round caps |
| `draw_polygon` | Ear-clipping triangulation, which handles concave shapes, plus an outline pass |
| `draw_circle` | The SDF disc and ring shape modes |
| `draw_text` | The SDF font, honoring `FONT_FIXED`/`FONT_VARIABLE` and the `ALIGN_*` flags |
| `clip` / `unclip` | Scissor |
| `blitter_*` | FBO region copies (`glCopyImageSubData`) |
| `status_bar` | The HUD |
| Printing API | Not implemented |

- **Size:** call `midend_size(user_size=true)` with the board area at the surface's native scale, so tiles render crisp at 1080p and at 4K.
- **Colors:** `midend_colours()` builds each game's palette from `frontend_default_colour`. Tuned per-game overrides go in `games/sgt/palettes.cpp`.

**Input: three modes that switch automatically**

| Mode | Buttons | Sent to the midend |
|---|---|---|
| **Cursor** (default) | D-pad | `CURSOR_UP/DOWN/LEFT/RIGHT` |
| | Cross | `CURSOR_SELECT` |
| | Square | `CURSOR_SELECT2` |
| **Key palette** | Triangle opens a ring of the keys from `midend_request_keys()`: digits, letters, colors, Clear. D-pad chooses, Cross sends, Circle closes. | The chosen key |
| **Pointer** | Moving the left stick shows an accelerated pointer. Cross and Square act as the left and right buttons. The D-pad returns to cursor mode. | `LEFT/RIGHT_BUTTON`, `_DRAG`, `_RELEASE`, at canvas coordinates |

- The key palette serves the games flagged `REQUIRE_NUMPAD`: Filling, Keen, Solo, Towers and Unequal. It also covers Map's colors and Undead's monsters.
- **Loopy is the only game with no cursor support**, so it relies on pointer mode. Pointer mode also helps drag-heavy games such as Untangle and Bridges.
- **In every mode:**

| Button | Action |
|---|---|
| L1 | Undo (`UI_UNDO`) |
| R1 | Redo (`UI_REDO`) |
| Options | Pause |
| Touchpad click | How to play |

- `midend_get_cursor_location` drives our **tweened cursor glow**, so the cursor glides between cells instead of jumping.

**Midend services**
- **Presets:** `midend_get_presets` builds the preset tree for the Difficulty menu.
- **Custom settings:** `midend_get_config(CFG_SETTINGS)` becomes stepper rows. `C_STRING` numbers use a numeric stepper, `C_BOOLEAN` a toggle and `C_CHOICES` a selector.
- **Game options:** Tatham preferences go through `CFG_PREFS`.
- **Timer:** `activate_timer` / `midend_timer(dt)` run every frame, which also drives Tatham's own move animations at 60 fps.
- **New games generate on a worker thread**, using a separate midend. Hard presets can take seconds.
  - The worker hands back a game ID, which the UI midend loads with `midend_game_id` followed by `midend_new_game`.
  - The next game for the current preset is **pre-generated**, so New game feels instant.
  - If a new game takes longer than 150 ms, the board shows a shimmer.
- **Save:** `midend_serialise` runs after every move, through the save thread.
- **Resume:** `midend_deserialise` loads it back.
- **Completion:** watch `midend_status()`. A change 0→1 means solved and triggers the celebration and statistics. A change 0→−1 means lost.

**Sound classification**
- The one patch to upstream (`0001-midend-move-hook`) calls a front-end hook with every executed move string and its kind: move, undo, redo, solve or restart.
- A small table for each game maps move-string prefixes to cues. For example, in Net `R`/`A` means `rotate` and `L` means lock, which maps to `mark`.
- Results with no hook call are classified by the key's return value:
  - `PKR_NO_EFFECT` plays `invalid`.
  - A cursor-only update plays `cursor`.

**Metadata**
- `games/sgt/meta.cpp` holds each game's display name, tagline, accent color, wave (below) and thumbnail seed. The description and objective strings come from upstream `CMakeLists.txt`.
- The how-to-play text is short controller-first rules we write ourselves, based on `puzzles.but`.

**Porting waves**, grouped by how the game is controlled:

| Wave | Games |
|---|---|
| **A: cursor plus 1–2 buttons (22)** | Black Box, Cube, Fifteen, Flip, Flood, Guess, Inertia, Light Up, Magnets, Mines, Mosaic, Net, Netslide, Pattern, Pegs, Range, Same Game, Singles, Sixteen, Tents, Twiddle, Unruly |
| **B: key palette (7)** | Filling, Keen, Map, Solo, Towers, Undead, Unequal |
| **C: edges, links and dragging (11)** | Bridges, Dominosa, Galaxies, Loopy, Palisade, Pearl, Rectangles, Signpost, Slant, Tracks, Untangle |

**Definition of done for each game**
1. It can be played fully with the controller.
2. Its presets and custom settings work.
3. Its palette is tuned, it is crisp at 1080p and at 4K, and it has no seams.
4. Its move sounds are mapped.
5. It has a how-to-play card.
6. Save/resume, statistics and the timer work.
7. The completion celebration and new-record detection work.
8. **Host tests** pass, using a recording drawing API:
   - generate every preset from fixed seeds
   - solve
   - undo and redo
   - serialise round-trip
   - 10,000 random inputs with no crash and no draw outside the canvas
9. It passes a hardware smoke test within its wave's R-run.

### 5.10 The polish bar ("AAA")

**Performance**
- A **locked frame rate**, at 60 fps (or 120 fps if D2 changes).
- **No hitches:**
  - no heap allocation per frame on hot paths
  - puzzle generation, file I/O and audio decoding all off the main thread
  - shaders compiled and warmed up behind the splash screen
  - fonts and audio preloaded

**Motion**
- Every state change is eased; nothing pops.
- Timing: focus moves use springs, transitions take 150–350 ms with expo or cubic-out easing, and numbers count up.
- Specific moments:
  - **Game launch:** the card zooms into the board, then the board deals in with a scale and fade.
  - **Invalid move:** the board shakes by 6 px for 120 ms and the `invalid` sound plays.
  - **The cursor glides** between cells.

**Celebration**
- The board flashes, which is Tatham's own flash, extended.
- A glow sweeps across the board and confetti particles fly as instanced quads.
- The camera does a slight punch (scale 1.02).
- The `complete` sting plays and the music ducks.
- A statistics card slides in, with the time, moves and best, plus a new-record badge.

**Visuals**
- SDF anti-aliasing for all UI; 4× MSAA for the puzzle canvas.
- Curated palettes that are safe for color-blind players.
- Soft shadows and an animated ambient background.
- A 5% TV safe area.
- One type scale, and drawn button glyphs.

**Audio**
- Every interaction has a sound, with variations and jitter.
- Music crossfades and ducks, and a limiter prevents clipping.
- Moving a volume slider plays a preview of that bus.

**Responsiveness**
- The screen never freezes. Anything taking more than 150 ms shows a shimmer or spinner.
- Settings apply instantly.
- The player can resume anywhere.

**Haptics: evaluate later**
- A light DualSense rumble for place and complete, off by default.
- It depends on verifying `scePadSetVibration` on hardware first.

**Accessibility**
- Reduced motion, the Cross/Circle swap, palette choices and a text-size step.

---

## 6. Milestones

Each milestone has host exit criteria. Some also have a single hardware run (R*n*) that answers one question.

- **Dependencies:** M0 → M1 (a hard gate) → M2 and M3 → M4 → M5, M6 and M7 (in parallel) → M8 → M9.
- **Tatham host work can start right after M0**, because the vendoring, the headless test harness and the front-end logic don't need the GPU. Only the canvas rendering waits for M3.
- **Console runs need your go-ahead.** A build request does not authorize a launch, so I'll ask before each R-run unless you pre-authorize the runs listed below.

### M0: Bootstrap (offline)

1. In WSL, run `gh repo create blackbearreloaded/ProsperoPuzzles --template blackbearreloaded/ps5-native-app-boilerplate --private --clone`. Record the template's HEAD in `docs/ARCHITECTURE.md`.
2. Run `make init TITLE_ID=PPSA99031 APP_NAME="ProsperoPuzzles" CONTENT_SUFFIX=PROSPEROPUZZLE01`.
   - This uses the development title ID. Content ID `UP9000-PPSA99031_00-PROSPEROPUZZLE01` matches the validator's regex.
   - The release build switches to PPSA99030 in M7.
3. Run `make doctor deps test app` unchanged, to confirm the template builds and tests clean on this machine.
4. Adapt `tools/lint.sh`:
   - The header rule should require "ProsperoPuzzles" in place of the boilerplate's name.
   - Keep the SPDX and path-leak rules.
   - Add a header template to `CONTRIBUTING.md`.
5. Delete `src/demo_renderer.*`, `tests/test_demo_renderer.cpp` and their references in `tools/build-tests.sh`, `docs/TESTING.md` and `README.md`.
6. Commit stub `README.md`, `THIRD_PARTY_NOTICES.md` and `CHANGELOG.md`, plus this plan as `PLAN.md`.
7. Create `.local/ENVIRONMENT.md` from the runbook's template. It stays gitignored.

**A0:** `make lint test app` passes in WSL, and CI passes on the host jobs.

### M1: GL 4.6 bring-up, "first frame" (the hard gate)

1. **SDK.** Write `tools/fetch-opengl-sdk.sh`:
   - Download the pinned release asset and its `.sha256` into `.deps/ps5-opengl-gl46`, then verify `manifest.sha256`.
   - `PS5_OPENGL_PREFIX` overrides the location, so a locally built SDK can be used instead.
   - Record the manifest hash in `docs/OPENGL_INTEGRATION.md`.
2. **Write `tools/prepare-opengl.sh`.**
   - Copy `include/{EGL,GL,KHR}` and `ps5_opengl_display.h` into `.local/ps5-opengl/include`.
   - Write the GNU ld GROUP script with these entries:
     - relative paths only
     - `EXTERN(ps5_agc_gate2_run)`
     - `libPS5OpenGL.a`
     - the import stubs `libSceAgc.so` and `libSceAgcDriver.so`
     - `libc++`, `libc++abi` and `libunwind`
     - clang builtins
   - **Every** packaging target depends on this script: `app`, `ffpkg`, `ffpfsc` and `packages`. In 2048, only `app` did.
3. **Tooling changes.** Commit each one separately and list them in `docs/OPENGL_INTEGRATION.md`:
   - `tooling/native/sce_module_writer.cpp`: change the process heap size from `UINT64_MAX` to `0x10000000`.
   - `tools/build.sh`: add `--wrap=malloc,calloc,realloc,free,posix_memalign,malloc_usable_size` and `--eh-frame-hdr`. Pass `--stub` for the SDK's `libSceAgc.so` and `libSceAgcDriver.so`.
   - `ps5-pie.ld`: make it include the base script and add PROVIDEs for `__eh_frame_hdr_start/end` and `__eh_frame_start/end`, as Yamagi's `native-app/ps5-pie.ld` does.
   - `app-symbols.map`: change it to `local: *;`.
   - `param.json`: add the `kernel` fields (256 MiB CPU and GPU page tables) and the `amm` field (512 GiB VA) from ps5-opengl's `native-app/param.json`. **Tenfold's `param.json` lacked these.**
   - Only if the link fails without it: pass weak symbols that stay unresolved through to the output, as 2048 did in `sce_module_writer.cpp:694-781`.
4. **Runtime.** Copy `src/runtime/app_heap.c` and `runtime_shims.c` from ps5-opengl's `native-app/`:
   - Send the log to `/download0/prosperopuzzles/app.log`.
   - Keep the Mesa TLS stub, `__assert`, and the `mkstemps`, `popen`, `pclose` and `openlog` stubs.
   - Add `nl_langinfo` and `___mb_cur_max` only if the linker asks for them.
   - `catchReturnFromMain` sleeps forever.
5. **Write `platform/ps5/display_egl.cpp`**, based on Yamagi `ps5_egl.c:31-213`:
   - Request a 4.6 core context.
   - RGBA8 with no depth buffer.
   - Native window 0 and NULL attributes.
   - Assert that `GL_VERSION` is at least 4.6 and the surface matches the profile size.
   - Call `eglSwapInterval(1)`.
   - Log every EGL error by name.
   - Tear down in order.
   - Don't call `eglSetDisplayModePS5`; that is a private Yamagi extension.
6. **Spike `main.cpp`**:
   - Open the display, then draw an animated clear plus one instanced SDF rectangle.
   - After the first swap, call `HideSplashScreen` and log `[PPZ] ready`.
   - Log a frame-time histogram every 600 frames, to both klog (`sceKernelDebugOutText`) and `app.log`.
7. Keep the template's `pic0.dds`, `pic1.dds` and `snd0.at9` so the title can launch.

**Hardware runs**

| Run | Question | What we do |
|---|---|---|
| **R1** (baseline) | Does the candidate SDK present on our console? | Build ps5-opengl's own GL 4.6 demo (`make gl46-demo`, title PPSA99005) against the candidate SDK and run it. |
| **R2** | Does our integration present? | Run our spike. **Oracle:** `first-swap ok` appears; at least 3,600 swaps succeed in 60 s; a screenshot shows the test pattern; the title closes cleanly and the log shows runtime layers released. |

**If R2 fails while R1 passes:** diff our `dist/` folder against ps5-opengl's staged app. Check `param.json`, the module heap, the link map and the stubs. Only then try the next SDK candidate. No blind retries.

**A1:** R2 passes, and the SDK manifest hash and `eboot.bin` hash are recorded.

### M2: Platform services and the host runner

1. **Write `platform/ps5/pad.cpp`**, based on ProsperoLight `radio_input.cpp` and the typed sample:
   - Declare a `static_assert`ed 120-byte struct: `buttons` at offset 0, `connected` at 0x4c, `timestamp` at 0x50, `connected_count` at 0x68.
   - Call `sceUserServiceInitialize` and track whether we own it. Then get the initial user and call `scePadInit`.
   - Open the pad with `scePadOpen(uid, 0, 0, NULL)`, retrying 20 times at 50 ms intervals.
   - Drain `scePadRead(h, samples, 64)` every frame and turn **every** sample into edges.
   - Handle `connected_count` changes as a reconnect.
2. Put the platform-neutral input code in `core/input_map.cpp` and `core/repeat.cpp`:
   - logical-action mapping
   - stick-to-D-pad conversion
   - per-context repeat
   - the Cross/Circle swap
   - `focus_lost`
3. **Write `platform/ps5/audio_out.cpp`** (see section 5.7) and the `audio/` mixer, synth, clip loader and cue table.
4. **Write `platform/ps5/storage.cpp`** plus `core/save_container.cpp` and `core/save_worker.cpp` (see section 5.6).
5. **Write `platform/ps5/system.cpp`:**
   - `HideSplashScreen`
   - the exit path from section 5.2
   - klog markers through `sceKernelDebugOutText`, in chunks of at most 384 bytes
   - a `sceKernelSendNotificationRequest` toast helper for fatal errors, such as a failed storage probe
   - the monotonic clock
6. **Write `host/`,** the preview runner:
   - It uses the same `core/`, `gfx/`, `ui/` and `games/` code with host backends: SDL2 or GLFW for the window and keyboard, null or SDL audio, and `./savedata/` for storage.
   - `make host-run` opens a window under WSLg.
   - `make host-snapshots` renders scripted scenes to PNG through surfaceless EGL on llvmpipe, for visual review.
7. **Write the diagnostics scene:**
   - It shows button state live, plays test tones, and increments a boot counter it saves.
   - It shows the heap statistics from `app_heap.c`.
8. **Host tests:**
   - input edges and repeat, driven by recorded sample sequences
   - deadzone
   - the Cross/Circle swap
   - the save container: round-trip, corruption and version migration
   - mixer envelopes and clipping, rendered offline into a buffer
   - the SPSC ring

**R3:** is every button edge logged, is audio output healthy, and does the boot counter persist?
- **Buttons:** you press a listed sequence, or we use Chiaki with the owner-paired virtual pad.
- **Audio:** the log shows the port opened, N grains written and 0 underruns. You confirm the tones are audible.
- **Persistence:** after two launch/close cycles, extract `download0.dat` and check that the counter reads 2.

**A2:** R3 passes and the host tests pass.

### M3: `gfx2d` and the UI toolkit

1. **`gfx/batch2d`:** the instance format and shaders from section 5.5, layers, scissor clipping, and transforms (translate and scale) for transitions.
2. **`tools/bake-font.py`:** build the SDF atlas (R8 plus metrics) for about 3 sizes of one family.
3. **`gfx/font`:** measure, wrap, align and kerning, plus tabular digits.
4. **`gfx/tween`:** easing functions (cubic, back, spring) and a small tween manager that honours reduced motion.
5. **`gfx/theme`:** tokens for palette, radii, spacing, the type scale, and a per-game accent.
6. **`ui/` widgets:**
   - panel/card
   - button
   - vertical list with focus navigation
   - toggle and slider rows
   - confirm dialog
   - toast
   - hint bar with the drawn face-button symbols
   - animated focus ring
7. **Vector primitives, for the Tatham canvas:**
   - lines and thick lines with round caps
   - ear-clipping triangulation for concave polygons, plus outlines
   - discs and rings
   - batch splits at clip boundaries
8. **`gfx/canvas`:** a persistent 4× MSAA FBO with a resolve step, region copies for blitters, and compositing with a transform so launches can zoom.
9. **`gfx/particles`:** instanced confetti and sparkle bursts, with a pooled fixed budget.
10. **Host tests:** text measurement and wrapping, layout math, focus navigation and triangulation (concave, collinear and degenerate cases). Also add a widget-gallery snapshot.

**R4** (can be combined with R3): does a gallery scene with 5,000 instances hold 60 fps on hardware, with main-thread CPU under 2 ms? The same run also checks that a 4× MSAA canvas can be created, resolved and composited, and measures its cost.

**A3:** the gallery snapshots are reviewed, and R4 passes.

### M4: The shell

1. **`core/app`:** the scene stack (push, pop and replace), transitions, the frame loop, and routing of `focus_lost`.
2. **Scenes:** Boot, **Library**, Game details, Settings, Pause, How to play and About, built as described in section 5.8.
3. **`games/registry`:** the interface from section 5.3, plus a **"Test Pattern" placeholder module** to exercise it.
4. **Library model** (`core/library`), fully covered by unit tests:
   - collate A–Z, ignoring case, with digits first
   - favorites section first (D10)
   - filters: All, Favorites, In progress
   - letter-jump index
   - stable focus when a favorite is toggled
   - persisted in `library.bin`
5. **Settings:** save them when they change and apply them live (volumes, the button swap, reduced motion).
6. **Quit:** add a "Quit to Home Screen" item to the Library's Options menu that calls the section 5.2 exit path. Keep the item only if it works reliably in R5.
7. **Host tests:**
   - scripted `InputFrame` sequences drive the full scene flow: Library → game → Pause → Return → Settings → Library
   - favoriting reorders the grid and survives a relaunch
   - registry invariants, such as unique IDs

**R5:** navigate Library → placeholder → Pause → Return to library → Settings, change a setting, favorite a card, then relaunch.
- **Oracle:** each scene change appears as a klog marker, the setting and the favorite persist, and the Quit item's outcome is logged.

**A4:** R5 passes.

### M5: The 2048 module

**Where the WIP code goes**

| From ps5-2048 | To |
|---|---|
| `src/game_core.hpp` (266 lines) | `games/g2048/core.hpp`, `namespace puzzles::g2048`; keep the API and the seeded, deterministic RNG |
| `tests/test_game_core.cpp` | `tests/g2048_core_test.cpp`, converted to GoogleTest |
| `main.cpp:76-262` (the overlay state machine) | `games/g2048/scene.cpp`; pause, win and lose go through the shell's dialogs |
| `renderer.cpp:97-103` (palette), `:330-406` (tiles and animation), `:347-471` (layout) | `G2048Scene::draw` on `gfx2d` |
| `renderer.cpp:31-61` (the 5×7 font) | dropped, replaced by the SDF font |
| `platform.cpp:96-138` (`update_hold`) | the shared `core/repeat` (not used for slides) |
| `platform.cpp:259-380` (synth and ambient pad) | the shared `audio/synth` and cue table |
| `save.cpp:69-146` (57-byte codec) | `games/g2048/codec.cpp` inside the container |
| `save.cpp:223-341` (`SaveWorker`) | the shared `core/save_worker` |
| `Renderer::open/close`, `Platform`, SDL, `main()`, `app_heap.c` | dropped; the shell owns these |

**Bugs to fix from the WIP**
- **Dead board after winning and losing on the same move.** Winning and running out of moves on the same move left a board with no overlay that ignored input. After "Keep going", show game over if `snapshot().game_over` is set.
- **Disconnect acted as "cancel".** It now opens the pause menu.
- **New game from game over had no confirmation.** It now goes through the confirm dialog.
- **Volume drifted between saves.** Volumes are now integers 0–10.
- **Animation order.** The spawned tile now appears after the slide finishes, and a merged tile shows its new value when it pops.
- **The 4K120 lock and fixed pixel sizes** are removed by the virtual canvas.

**New features**
- Undo of the last move, on L1, the same button as in every other game.
- A move counter.
- Statistics: games played, games won, highest tile and best score.
- A live thumbnail on the Home card.

**Host tests**
- Carry over the existing four test groups.
- Add these cases:
  - winning and losing on the same move
  - restore validation
  - the spawn distribution (the "4" rate stays within tolerance over a fixed seed)
  - score saturation at the maximum exponent
  - undo
  - the codec: round-trip, corruption and migration
  - the scene's state machine, driven by scripted input

**R6:** play to a merge, return to the menu, close, relaunch and resume.
- **Oracle:** the board is identical on resume (checked against the snapshot hash in the log), and the best score persists.

**A5:** R6 passes.

### M6: The Tenfold module

**Where the WIP code goes**

| From ps5-tenfold | To |
|---|---|
| `src/game_core.hpp` (251 lines) | `games/tenfold/core.hpp`, `namespace puzzles::tenfold` |
| `tests/test_tenfold_core.cpp` | `tests/tenfold_core_test.cpp`, converted to GoogleTest |
| `main.cpp:62-246` (select, anchor and merge flow, overlays) | `games/tenfold/scene.cpp` |
| `renderer.cpp:26-27` (palette), `:293-312` (tile), `:325-441` (layout), `:339-360` (fall, fade and pulse animation) | `TenfoldScene::draw` on `gfx2d` |
| `platform.cpp:217-250` (repeat), `:275-350` (cues) | the shared repeat helper and cue table |
| `save.cpp` (24 bytes: best score and sound) | `games/tenfold/codec.cpp`, extended with the full board; the sound flag moves to the global settings |
| `runtime_shims.c`, `app_heap.c` | merged into the shell's single copies |
| `tools/generate-art.py` | the base for the collection's `tools/generate-art.py` |

**Bugs to fix from the WIP**
- **Cursor not reset.** A new game started from game over kept the old cursor position (`main.cpp:231-235`).
- **Stray sound.** Circle played a sound even when nothing was selected.
- **No pause from game over.** The pause menu is now available there.
- **No board resume.** Add a snapshot save covering: cells, score, best score, RNG state, moves, highest value, won and game over, plus the undo snapshot.

**New features**
- Hint (Square): highlights one valid group.
- Statistics: games played, first-ten reached, best score and highest value.
- A live thumbnail.

**Controls**

| Input | Action |
|---|---|
| D-pad or stick | Move the cursor (with repeat) |
| Cross | Select a group, then merge at the anchor |
| Circle | Deselect |
| L1 | Undo (the same button in every game) |
| Square | Hint |
| Options | Pause |

**Host tests** to add:
- win (first time reaching 10)
- game over (no adjacent equal pair)
- the spawn range for each highest value (`1..min(5, max(3, highest-2))`)
- gravity per column
- rejected anchors
- undo edge cases
- score saturation
- the codec
- the scene flow

**R7:** play to a merge, return to the menu, close, relaunch and resume. The oracle is the same as R6.

**A6:** R7 passes.

### M7: The Tatham collection (40 puzzles)

**M7a: Host foundation.** This can start right after M0.
1. Write `tools/update-sgt-puzzles.sh`. It:
   - fetches the pinned upstream commit
   - copies the vendored file list into `src/third_party/sgt-puzzles/`
   - generates `generated-games.h`
   - applies `patches/sgt/*.patch`
   - copies `LICENCE` and `puzzles.but` into `third_party/sgt-puzzles-docs/`
2. Get the PS5 build compiling and linking all 40 with `-DCOMBINED`. Add any libc gaps to `runtime_shims.c`.
3. Write `patches/sgt/0001-midend-move-hook.patch` (section 5.9).
4. Build the **headless harness** (`tests/sgt/`):
   - A recording `drawing_api` with bounds checks.
   - For every game, the full host test list in the definition of done (section 5.9).
   - Timings for generating every preset, which flag presets that need the pre-generation path.
5. Write `games/sgt/meta.cpp` with the metadata for all 40, and the per-game sound classifier tables.

**M7b: Front end on the console.** This needs M3 and M4.
1. The `drawing_api` implementation on `gfx/canvas`.
2. The three input modes, plus the key palette and pointer widgets.
3. Midend services: presets, custom settings, preferences, timer, status, save/resume and background generation.
4. The HUD, and completion detection linked to the celebration.
5. An `SgtScene` that implements `GameScene` generically for all 40. The registry lists one entry per game.
6. Thumbnails: each game's fixed seed rendered into a card-sized canvas, cached as a texture, and refreshed from the saved game when one is in progress.

**M7c: Porting waves.** Each wave ends when every game in it meets the definition of done.

| Run | Wave | Games checked on hardware |
|---|---|---|
| **R8** | Wave A | Mines, Net, Pattern, Same Game |
| **R9** | Wave B | Solo and Keen (digits through the key palette) |
| **R10** | Wave C | Loopy (pointer only), Untangle (drag) and Bridges |

**A7:** all 40 meet the definition of done, and R8–R10 pass.

### M8: Polish and audio integration

1. Implement everything in section 5.10 across the shell and all 42 games.
2. **Tier 1 bespoke skins (D12, time-boxed):** Flood, Mines, Net, Same Game, Solo, Pattern, Fifteen and Light Up.
   - Each skin is an `#ifdef PROSPERO_SKIN` renderer inside the vendored game file.
   - It keeps the game's logic and replaces only its `redraw` with a `gfx2d` renderer: tweened tiles, particles and depth.
   - Each skin is its own patch in `patches/sgt/`.
3. **Your audio:** drop the files into `assets/audio/`, update `manifest.json`, run `make audio-check` (Appendix A), and balance levels on the TV.
4. **Night palettes** for the Tatham games, curated per game.
5. **Visual regression:** `make host-snapshots` for every game at its default preset, reviewed as a contact sheet.

**R11:** a 30-minute mixed session across the Tier 1 games, with your audio.
- **Oracle:** no frame-time spikes over 20 ms in the log histogram, and no audio underruns.

**A8:** R11 passes and every item in section 5.10 is checked off.

### M9: Release

1. **Art:**
   - `icon0.png` (512×512)
   - `pic0.dds` and `pic1.dds` (3840×2160, BC7, DX10 header, no mipmaps), made from the generated source PNGs with `tools/prepare-assets.ps1` and texconv on Windows
   - `snd0.at9` (48 kHz ATRAC9 with a loop, at most 2 MiB), made with `ps5-at9-converter`
   - `make assets-check` passes
2. **`snd0.at9`:** made from your menu track (Appendix A).
3. **4K120 evaluation (D2):**
   - Build an SDK with `PS5_SCANOUT_HEIGHT=2160 PS5_SCANOUT_FPS=120` and set `attribute3 = 0x80040`.
   - Check that text stays crisp and the frame time holds.
   - Adopt it only if it passes a run on hardware.
4. **Soak test:**
   - A 30-minute session that alternates between the games.
   - 3 clean launch/close cycles.
   - Heap snapshots must show no growth.
   - This is Yamagi's G6 bar.
5. **Documentation:**
   - `README.md`: features, controls, building, deploying.
   - `THIRD_PARTY_NOTICES.md`: Simon Tatham's Portable Puzzle Collection (MIT, with the full `LICENCE`), Mesa (MIT), ps5-opengl (GPL-3.0), opengnm/PSBC, stb (public domain or MIT), the font (OFL), and credit for the 2048 concept (Gabriele Cirulli, MIT).
   - `CHANGELOG.md`.
   - `docs/HARDWARE_TESTING.md`.
6. **CI:**
   - lint
   - host unit tests
   - `libc` reproduction
   - **a native build that fetches the pinned SDK release**, if D4 settled on a published asset; otherwise releases are built locally and uploaded with `gh release`
   - on a tag matching `contentVersion`, a release with the ZIP and `SHA256SUMS`
7. Switch to the release title (PPSA99030), freeze the candidate and run the final evidence ladder, then **tag `01.000.000`**.

**A9:** the soak and cycles pass on the frozen candidate, and the release assets are verified: they download and their checksums match.

### M10: Backlog (after v1)

- **More games.** Each one is a new folder, a registry entry and its tests. Candidates from the brainstorm:
  - Sokoban, using the DeepMind Boxoban levels (Apache-2.0)
  - Water Sort
  - Traffic Jam, using `fogleman/rush` (MIT)
  - Chess Puzzles, using the Lichess puzzle database (CC0)
  - Block Grid
- More Tier 1 bespoke skins.
- Daily seeded challenges, using the existing deterministic RNG and Tatham's game IDs.
- A 4K120 default, if M9 passes it.
- The system's Cross/Circle setting.
- A high-score table with names entered through IME.

---

## 7. Hardware test protocol (runbook)

**Transport**
- **All console traffic goes through WSL.**
- Preflight is `nc -z -w 3` against 2121, 3232 and 9021, checking each exit code.

**Freezing a candidate**
- Record:
  - the commit
  - the SDK manifest hash
  - the title ID and `contentVersion`
  - the SHA-256 and size of `eboot.bin`
  - any changed assets
- Rebuilding makes a new candidate.

**Deploying with `tools/deploy-verified.sh`**
1. Upload each file to `.<name>.upload`.
2. Read it back. Use `curl --quote SELF` for the raw bytes of `eboot.bin` and `*.prx`, and a plain download for other files.
3. Compare SHA-256.
4. Promote with RNFR/RNTO, doing `eboot.bin` and `param.json` last.
5. Confirm the title is registered.

Also:
- Never overwrite a title that is running.
- If `ps5-sync.sh` already does all of this, reuse it instead.

**Launching**
1. Start `timeout 90s nc <host> 3232 > klog.txt &` and prove the collector connected.
2. Take a byte-count baseline of `/data/shadowmount/debug.log`.
3. Launch and close with `send-controller.sh launch|close PPSA99031 <host> 9021`. Closing is title-aware.

**Logs and saves**
- While running, fetch the live log over FTP from `/mnt/sandbox/PPSA99031_000/download0/prosperopuzzles/app.log`.
- After closing, pull `/user/download/PPSA99031/download0.dat`.
- Extract it with `ufs2tool extract`, then decode the saves with `tools/inspect-save.py`.

**Evidence ladder**
- Stages, in order: transport → registration → launch → entry (`[PPZ] entry`) → ready (`[PPZ] ready`) → functional oracle → teardown (runtime layers released) → available again.
- Never promote one stage into the next.
- Each run is classed as pass, partial pass, fail, inconclusive, transport failure or no run.

**Rules**
- One question per run, with a hard timeout.
- **No blind retries after a suspected panic or GPU fault.**
- Never open Settings, the Store, or update or sign-in screens.
- Results, klogs, captures and `download0.dat` stay local and gitignored.
- Keep a single current-state note; no per-attempt reports.

---

## 8. Testing strategy

**Host unit tests** (GoogleTest, `make test-unit`, built with ASan and UBSan)
- game cores and codecs
- input mapping, repeat and swap
- the mixer and command ring
- font layout
- scene flows driven by scripted `InputFrame`s
- registry invariants
- settings migration

**Host preview**
- `make host-run` opens an interactive window under WSLg with keyboard controls.
- `make host-snapshots` writes deterministic PNGs of every scene for review. They are not pixel-gated at first.

**Static checks:** `make lint`, covering format, clang-tidy, headers and path leaks. `make check` runs lint, test and app together.

**Tatham harness:** headless tests for all 40 games, covering every preset, solving, undo/redo, serialisation and 10,000 random inputs, with bounds checks on every drawing call (section 5.9).

**Hardware:** runs R1–R11 plus the M9 soak, as described in section 7.

---

## 9. Risks

| Risk | Evidence | Mitigation |
|---|---|---|
| The first swap fails with `EGL_BAD_SURFACE` | Tenfold's only hardware launch | Run the R1 baseline first. Follow the native-app recipe exactly, including the `param.json` `kernel`/`amm` fields. Keep the C91 SDK as a fallback. Diff against ps5-opengl's staged app. |
| The chosen SDK has little evidence on this console | GL 4.6 has only demo-level evidence | R1 runs on this exact console. Pin the manifest hash. |
| `LoadExec("exit")` fails | Tenfold klog showed 0x80aa001a | Yamagi's path works. Verify in R5; if unreliable, drop the Quit item and rely on the PS button. |
| Data loss on close or GPU fault | Shell close skips handlers; GPU faults call `_Exit` | Save on change through the save thread, with atomic writes. |
| Lost taps | Quake II over SDL | Batch reads with `scePadRead(…, 64)`. If R3 still loses taps, add a 4 ms poll thread. |
| GL stalls | Yamagi's findings | Follow the section 5.5 rules, and check frame time on hardware in R4. |
| Large `eboot.bin` (about 25 MB, Mesa statically linked) and shader compile at boot | 2048 and Tenfold builds | Acceptable. The shader cache is optional, and there are only 1–2 small programs. |
| Title ID collision | PPSA99020 was reused | Verify PPSA99030/31 are unregistered before M1. |
| Provenance of the Tenfold design | Its README says it ports a "local Tenfold browser game" that I could not find | Confirm the origin and credit it in About and NOTICE. |
| Tatham's code needs libc functions the PS5 lacks | It uses `sprintf`/`sscanf`/`qsort`/`gettimeofday`/`getenv` and math functions | Link all 40 in M7a. Add any gaps to `runtime_shims.c`. |
| Seams, or MSAA too slow, on the puzzle canvas | MSAA hasn't been exercised on ps5-opengl | Measure in R4. Fall back to 2× supersampling. Draw axis-aligned rectangles pixel-exact. |
| Slow puzzle generation at hard presets | Solo, Pattern and Mines can take seconds | Worker-thread generation, pre-generation of the next game, and preset timings from the harness. |
| Seams between re-skinned games and upstream | Bespoke skins patch game files | Limit skins to Tier 1, keep each as a separate patch, and re-run the harness after every upstream sync. |

---

## 10. Open questions

1. **D1:** is native `scePad` + `sceAudioOut` (the ProsperoLight path) acceptable, or do you want PacBrew SDL2 as in Quake II?
2. **D2:** start at 1080p60, or go straight to 4K120?
3. **D5 and D6:** make the repo public later? Are titles PPSA99030/31 OK?
4. **Tenfold:** who made the original browser game, and how should it be credited?

---

## Appendix A: audio asset spec (for your sound and music production)

Put the files in `assets/audio/sfx/` and `assets/audio/music/`. Anything missing falls back to a synthesized placeholder. `make audio-check` validates format, length and levels.

### Sound effects

**Format**
- WAV (RIFF) PCM, **48,000 Hz, 16-bit**. 24-bit is accepted and converted at build time.
- Mono or stereo. Mono is played centered.

**Edits**
- No leading silence longer than 5 ms.
- Fade the tail to digital silence.
- **Peak at or below −1 dBFS.**

**Relative loudness.** Balance by ear on a TV, using these short-term LUFS targets:

| Category | Target |
|---|---|
| Cursor and UI ticks | −30 to −24 |
| Gameplay actions | −24 to −18 |
| Stings | −18 to −14 |

**Variations**
- Name them `<cue>_01.wav`, `<cue>_02.wav` and so on.
- Supply 2–4 variations for frequent cues (cursor, place, rotate and the like).
- The engine plays them round-robin, with ±3% pitch jitter.

**Game-specific overrides:** `<gameid>.<cue>.wav`, for example `mines.reveal_01.wav` or `g2048.merge_01.wav`. Game IDs are the lowercase names: `g2048`, `tenfold`, `mines`, `solo` and so on.

**UI cues**

| Cue | When it plays | Length |
|---|---|---|
| `ui_focus` | Focus moves between cards or menu items | 20–60 ms, very soft |
| `ui_select` | Confirm or open | 80–150 ms |
| `ui_back` | Back or close | 80–150 ms |
| `ui_tab` | Filter or page change (L1/R1), letter jump (L2/R2) | 100–200 ms |
| `ui_favorite_on` | Star added (sparkle) | 200–400 ms |
| `ui_favorite_off` | Star removed | 100–200 ms |
| `ui_launch` | Card zooms into the game (whoosh) | 300–700 ms |
| `ui_pause_open` / `ui_pause_close` | Pause overlay opens or closes | 150–300 ms |
| `ui_toggle` | A setting is toggled | 60–120 ms |
| `ui_slider` | A slider moves one step | 30–60 ms |
| `ui_error` | Action not allowed | 100–200 ms |
| `ui_notify` | Toast, or "new record" badge | 300–600 ms |

**Gameplay cues** (shared by all games)

| Cue | When it plays | Length |
|---|---|---|
| `cursor` | The board cursor moves | 15–40 ms, very soft |
| `place` | Primary action: fill, place or light | 60–150 ms |
| `mark` | Secondary action: flag, cross-out or pencil mark | 60–150 ms |
| `erase` | A cell is cleared | 60–150 ms |
| `digit` | A number or letter is entered from the key palette | 60–120 ms |
| `rotate` | A tile rotates (Net, Twiddle) | 80–200 ms |
| `slide` | A tile slides (Fifteen, Sixteen, Netslide, 2048) | 80–200 ms |
| `flip` | A tile toggles (Flip) | 60–150 ms |
| `pickup` / `drop` | A piece is grabbed or released (Pegs, Untangle) | 60–150 ms each |
| `connect` | A link completes (Net, Bridges, Signpost) | 150–300 ms |
| `reveal` | A cell is uncovered (Mines, Black Box) | 60–120 ms |
| `cascade` | Many cells are uncovered at once | 200–500 ms |
| `merge` | A merge in 2048 or Tenfold; the engine pitches it up with the value | 100–200 ms |
| `spawn` | A new tile appears | 60–120 ms |
| `invalid` | A move is rejected | 100–200 ms |
| `undo` / `redo` | Undo or redo | 80–150 ms |
| `new_game` | A board is dealt | 300–700 ms |
| `restart` | The board resets | 300–600 ms |
| `solve_reveal` | Auto-solve is used | 0.5–1.5 s |
| `complete` | **The puzzle is solved** (the main reward sting) | 1.5–4 s |
| `new_record` | Layered after `complete` when a best is beaten | 1–2.5 s |
| `explode` | A mine is hit, or another fatal move | 0.8–2 s |
| `game_over` | The game is lost (2048, Tenfold, Mines) | 1–3 s |

### Music

**Format**
- **OGG Vorbis**, 48 kHz, stereo, quality 6 (about 192 kbps).
- **Integrated loudness of −18 LUFS**, true peak at or below −1 dBTP.
- Mix the tracks as calm beds under the effects: avoid sharp transients and busy high-mids.

**Looping**
- Loops must be seamless.
- Either the whole file loops, or you add the `LOOPSTART` and `LOOPLENGTH` Vorbis comments (in samples) for an intro followed by a loop.
- 2–4 minutes each.

**Tracks**

| File | Used for |
|---|---|
| `menu_main.ogg` | The library and settings |
| `puzzle_calm_01..03.ogg` | A playlist for the logic puzzles (most Tatham games) |
| `puzzle_upbeat_01..02.ogg` | A playlist for the action-leaning games: 2048, Tenfold, Same Game, Flood, Inertia, Mines |
| `game_<id>.ogg` | Optional: a dedicated track for one game |

**Home-screen preview:** a 10–30 s excerpt of `menu_main`, delivered as a 48 kHz WAV. We convert it to `snd0.at9` with `ps5-at9-converter` (at most 2 MiB, looped).
