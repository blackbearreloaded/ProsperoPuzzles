# ProsperoPuzzles

A native PS5 homebrew collection of polished puzzle games, built for the
DualSense controller.

- **42 games in one library:** the 40 puzzles of
  [Simon Tatham's Portable Puzzle Collection](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/)
  plus **2048** and **Tenfold**.
- **Alphabetical library with favorites:** every game is listed A–Z, and
  favorites are pinned first.
- **Console-grade presentation:** anti-aliased OpenGL 4.6 rendering, animated
  transitions, completion celebrations, sound effects and music.
- **Resume anywhere:** games in progress, statistics and settings persist in
  the title's `/download0` storage.

## Status

Early development. See [PLAN.md](PLAN.md) for the milestones and the current
gate.

| Area | Status |
| --- | --- |
| Build | Native C++20 pipeline from `ps5-native-app-boilerplate` |
| Rendering | OpenGL 4.6 Core via `ps5-opengl` (M1) |
| Games | 2048, Tenfold, and the 40 Tatham puzzles (M5–M7) |

## Building (Linux or WSL)

```bash
make doctor   # check host prerequisites
make test     # host unit and integration tests
make          # build dist/<TITLE_ID>/ and the folder ZIP
```

Useful targets: `make lint`, `make check`, `make ffpfsc`, `make help`.
Build and deployment details are in [docs/](docs/): start with
[GETTING_STARTED](docs/GETTING_STARTED.md),
[DEPLOYMENT](docs/DEPLOYMENT.md) and [ARCHITECTURE](docs/ARCHITECTURE.md).

## Controls

| Button | Library | In a game |
| --- | --- | --- |
| D-pad / left stick | Move between games | Move the cursor (stick: pointer) |
| Cross | Play or resume | Primary action |
| Square | Toggle favorite | Secondary action |
| Triangle | Game details | Key palette (numbers, letters, colours) |
| L1 / R1 | Filter | Undo / redo |
| Options | Settings | Pause |

## License

ProsperoPuzzles is licensed under GPL-3.0-or-later. Third-party components keep
their own licenses; see [NOTICE.md](NOTICE.md).
