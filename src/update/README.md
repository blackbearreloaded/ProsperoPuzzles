# Update check and self-update kits

These files are the PS5 Native App Boilerplate's
(`examples/update-check` and `examples/self-update`, boilerplate commit
`f98de73`), copied unchanged except for one line: `self_update_ps5.c` includes
`ppz_paths.h`, which defines `SELF_UPDATE_HELPER_PATH`,
`SELF_UPDATE_PARAM_PATH` and `SELF_UPDATE_SEQUENCE_PATH`. ProsperoPuzzles
needs them because its folder is not `/app0` and its data is not in
`/download0` once it runs with filesystem access
(`src/platform/ps5/storage.cpp`).

`ppz_paths.h` is ProsperoPuzzles' own. The app-side wrapper is
`src/platform/ps5/updater_ps5.cpp`, the interface the shell sees is
`src/core/updater.hpp`, and the dialog is `src/ui/update_dialog.cpp`.

The helper the app sends to the console's payload loader is in
`payloads/self-update-helper` (the boilerplate's `examples/self-update-helper`
as ProsperoEden carries it: it also finds the installed folder through
ShadowMountPlus's `mount.lnk` record and refuses an image install). The host
tests of the kits are in `tests/kits`. See [docs/UPDATES.md](../../docs/UPDATES.md).
