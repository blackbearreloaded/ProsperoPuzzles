# Changelog

## Unreleased

- The app updates itself: it asks the homebrew.page catalog for a newer
  release once per launch, offers it in the library, downloads and checks it,
  and replaces its own files after closing. An animated dialog shows the offer,
  the progress and the result.
- Settings, favorites, saved games, records and the log are kept in
  `/data/prosperopuzzles`. Saves of earlier versions are copied over at the
  first start.
- Filesystem access through upstream Lapy: the package carries an exact-title
  one-shot helper and sends it to the local payload loader, so Lapy does not
  have to be loaded separately. Without a loader the app keeps its sandbox
  folder.

## 01.000.010

- Builds on ps5-opengl SDK 1.0.0, which passes the Khronos OpenGL 4.6
  conformance test run.
- Fixed a thin white line under the game grid while the library zooms in or
  out around a game.

## 01.000.000

- Created the project from `ps5-native-app-boilerplate` at `4f531c4`.
- Configured the development identity `PPSA99031`.
- Adopted the final title `PPSA99006`.
- OpenGL 4.6 presentation, DualSense input and `sceAudioOut` audio on hardware.
- Library: A–Z grid with favorites, filters (L1/R1), wrapping letter jumps
  (L2/R2), live board previews, a details menu and a settings screen.
- Games: 2048, Tenfold and all 40 Tatham puzzles, with resume, statistics,
  pause menus and How to play cards.
- One modern skin for the Tatham puzzles: the app palette, rounded tiles and
  card-style tiles for Fifteen and Sixteen.
- Controller glyphs for every hint (face buttons, L1/R1, L2/R2, Options,
  sticks, D-pad).
- Completion confetti (lighter with Reduced motion).
- Mixer with synthesized placeholder cues; OGG Vorbis music streaming with
  crossfades, loop points and ducking; `make audio-check`.
- Eight native puzzles on a shared kit (generated boards, sizes, undo/redo,
  timer, best times): Color Sort, Crowns, Kakuro, Link Up, Nurikabe, Sokoban,
  Traffic Jam and Trail — 50 games in all.
- New icon and home backgrounds.
