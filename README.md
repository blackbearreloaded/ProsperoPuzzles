# ProsperoPuzzles

A native PS5 homebrew collection of polished puzzle games, built for the
DualSense controller.

- **42 games in one library:** the 40 puzzles of
  [Simon Tatham's Portable Puzzle Collection](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/)
  plus **2048** and **Tenfold**.
- **Alphabetical library with favorites:** every game is listed A–Z, and
  favorites are pinned first. Cards show a live preview of each board.
- **Console-grade presentation:** anti-aliased OpenGL 4.6 rendering, one modern
  skin across every puzzle, animated transitions, confetti celebrations, sound
  effects and streamed music.
- **Resume anywhere:** games in progress, statistics and settings persist in
  the title's `/download0` storage.

## Status

Feature-complete for v1 apart from the delivered audio: sound effects fall back
to synthesized placeholders and music stays silent until the tracks in
[PLAN.md Appendix A](PLAN.md#appendix-a-audio-asset-spec-for-your-sound-and-music-production)
are added.

| Area | Status |
| --- | --- |
| Build | Native C++20 pipeline from `ps5-native-app-boilerplate` |
| Rendering | OpenGL 4.6 Core via `ps5-opengl`, 60 Hz, verified on hardware |
| Library | A–Z grid, favorites, filters, letter jumps, board previews, details, settings |
| Games | 2048, Tenfold, and all 40 Tatham puzzles, each with How to play |
| Audio | Mixer, WAV cues with placeholders, OGG Vorbis music streaming |
| Presentation | Icon and home backgrounds rendered by the app (`tools/render-art.sh`) |

## Building (Linux or WSL)

```bash
make doctor   # check host prerequisites
make test     # host unit and integration tests
make          # build dist/<TITLE_ID>/ and the folder ZIP
```

Useful targets: `make lint`, `make check`, `make host-snapshots`,
`make audio-check`, `make help`. Build and deployment details are in
[docs/](docs/): start with [GETTING_STARTED](docs/GETTING_STARTED.md),
[DEPLOYMENT](docs/DEPLOYMENT.md) and [ARCHITECTURE](docs/ARCHITECTURE.md).

## Controls

| Button | Library | In a game |
| --- | --- | --- |
| D-pad / left stick | Move between games | Move the cursor (stick: free pointer) |
| Cross | Play or resume | Primary action |
| Square | Toggle favorite | Secondary action |
| Triangle | Game details | Key palette (numbers, letters, colours) |
| L1 / R1 | Filter | Undo / redo |
| L2 / R2 | Jump to the previous / next letter (wraps) | — |
| Options | Settings | Pause (How to play, new game, restart, solve) |

## Audio assets

Put sound effects in `assets/audio/sfx/` and music in `assets/audio/music/`,
named as in PLAN.md Appendix A, then run `make audio-check`. Missing effects use
synthesized placeholders; missing music is silent.

## License

ProsperoPuzzles is licensed under GPL-3.0-or-later. Third-party components keep
their own licenses; see [NOTICE.md](NOTICE.md).
