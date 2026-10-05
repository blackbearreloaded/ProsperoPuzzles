# Updates

ProsperoPuzzles checks for a newer release each time it opens and can install
it by itself, in place: the installed folder keeps its location, and saves in
`/data/prosperopuzzles` are not touched.

## What the player sees

1. The app opens as usual. In the background it asks the
   [homebrew.page](https://homebrew.page/app/PPSA99006/) catalog whether a
   newer release is listed.
2. When one is, the library shows **Update available** with the version and
   the download size: *Update now* or *Later*. *Later* (or Circle) closes the
   dialog; it is offered again the next time the app opens. A game in progress
   is never interrupted: the offer waits for the library.
3. *Update now*: a ring fills while the release downloads, with the amount, the
   share and the time left, then while it unpacks. Circle cancels at any point;
   nothing has been changed yet.
4. **Update ready. ProsperoPuzzles closes now.** The app closes itself.
5. A few seconds later a system notification says the app was updated. Open it
   again: the library header shows the new version.

If the update can't be installed (no payload loader is running, the app is
installed as an image, the download doesn't match the catalog), the dialog
says why and offers *Try again*; the app is unchanged.

## How it works

The check and the update are the PS5 Native App Boilerplate's
[self-update kit](https://github.com/blackbearreloaded/ps5-native-app-boilerplate/blob/main/docs/SELF_UPDATE.md),
as ProsperoEden uses it. Two programs take part:

| | The app | The helper (`self-updater.elf`) |
| --- | --- | --- |
| Runs | As always | Started by the console's payload loader (loopback port 9021), which the app sends it to |
| Does | Asks the catalog and verifies it; asks the player; downloads the release over HTTPS; shows progress | Saves the download; checks and unpacks it beside the app; waits for the app to close; replaces the app's files; refreshes the home-screen copies; posts the notification |

1. **Check** ([`src/platform/ps5/updater_ps5.cpp`](../src/platform/ps5/updater_ps5.cpp)).
   On a thread of its own, the kit fetches the catalog's `manifest.json` and
   its Ed25519 signature, verifies the signature against the catalog's keys,
   then fetches the app's own entry and requires its SHA-256 to be the one the
   manifest lists. It compares the listed content version with this build's
   `sce_sys/param.json`. No answer, or an answer that doesn't verify, shows
   nothing.
2. **Ask** ([`src/ui/update_dialog.cpp`](../src/ui/update_dialog.cpp)).
3. **Download and stage.** The app downloads the release ZIP from GitHub and
   streams it to the helper. The helper checks size and SHA-256 again,
   validates the archive (paths, links, sizes, one app only), unpacks it on the
   app's drive, and requires the unpacked `param.json` to name this title and
   the listed version.
4. **Apply.** The app gives the go-ahead and closes
   (`sceSystemServiceLoadExec("exit")`). Once its sandbox is gone the helper
   moves the old entries out and the new ones in (renames on one drive),
   removes its work folder and posts the notification. Top-level files the
   release doesn't contain stay where they are.

HTTPS goes through libcurl and OpenSSL with the console's own certificate
list ([`src/update/console_curl.c`](../src/update/console_curl.c)); the
download is accepted only from `https://github.com/` and GitHub's release file
host.

## What it needs

| Need | Without it |
| --- | --- |
| A payload loader listening on port 9021 | "The update helper couldn't be started. Is the payload loader running?" The app is unchanged |
| The app installed as a **folder** (`/data/homebrew/PPSA99006`, or wherever ShadowMountPlus mounted it from) | The helper refuses an image install; update it by replacing the image |
| Free space on the app's drive for the ZIP and the unpacked app | A refusal before the download or before unpacking |
| The network | Nothing is offered |

Updates are published the usual way: a higher `contentVersion` in
`sce_sys/param.json` on `main` makes the release workflow attach
`PPSA99006.zip` to a GitHub release, and the catalog lists it once its update
is merged. Every installed copy that is older then offers it at its next start.

## Trying an update before it is released

A build made with `PPZ_DEV_UPDATE_OFFER` takes the offer from
`update-offer.txt` in the app's folder instead of the catalog. It skips the
catalog's signature, so it is for development only: **never ship a build with
it**.

1. Build the *new* version: raise `contentVersion`, `make`, and attach
   `dist/PPSA99006.zip` to a GitHub release (a pre-release in a test repository
   is enough). Note its size and SHA-256.
2. Restore `contentVersion` and build the *old* version:

   ```bash
   make APP_DEFINITIONS="PPZ_DEV_UPDATE_OFFER"
   ```

3. Install it, and put `update-offer.txt` beside its `eboot.bin`. Five lines:
   the new content version, the release's name, its ZIP on GitHub, its SHA-256,
   its size in bytes:

   ```text
   01.000.020
   01.000.020
   https://github.com/<you>/<repository>/releases/download/<tag>/PPSA99006.zip
   f5eaf71c9368e2490b3733932e2e1b045b842387ba0fea70181749612d04505b
   42280819
   ```

4. Start the app and accept the offer. `PPZ_DEV_UPDATE_AUTO_ACCEPT=<seconds>`
   accepts it without a controller, for scripted runs.

Every step is logged with the prefix `[PPZ] update` (kernel log and
`/data/prosperopuzzles/app.log`); the helper logs as `[self-update]`.

## Tests

```bash
make test-update-check   # versions, the catalog's answers, hostile input
make test-self-update    # the engine against the helper, with real archives and folders
make test-unit GTEST_ARGS=--gtest_filter='UpdateDialog*'
make host-snapshots      # build/snapshots/update-*.png: every state of the dialog
```

`PPZ_UPDATE_FILM=1 bash tools/host-snapshots.sh <folder> 1280 720` writes the
whole update as 60 FPS frames (`film-NNNN.png`), for a video of the dialog.

