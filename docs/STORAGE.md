# Storage and filesystem access

ProsperoPuzzles keeps its settings, library, saved games, records and log in
**`/data/prosperopuzzles`**, a real folder on the console's internal drive. The
files there outlast the app (removing the title does not delete them) and can
be read, backed up or restored over FTP.

```text
/data/prosperopuzzles/
  settings.bin            volumes, resolution, options
  library.bin             favorites and the last focused game
  games/<game>.sav        a game in progress
  games/<game>.stats      records for a game
  app.log, app.prev.log   this launch's log and the previous one
  self-update-sequence    the highest catalog sequence accepted (see UPDATES.md)
  migrated                marks that the sandbox's saves were brought over
```

Every file except the logs uses the checked container of
[`src/core/save_file.hpp`](../src/core/save_file.hpp) and is written through a
temporary file and a rename, so a power cut leaves either the old file or the
new one.

## Filesystem access

A PS5 title runs in a sandbox that cannot see `/data`. The first thing `main`
does, while the process still has one thread, is to ask for filesystem access
([`src/platform/ps5/storage.cpp`](../src/platform/ps5/storage.cpp)) through
the cooperative client of
[PS5-Lapy-JB-Daemon](https://github.com/mpereiraesaa/PS5-Lapy-JB-Daemon)
([`src/platform/ps5/elevation.cpp`](../src/platform/ps5/elevation.cpp), from
the PS5 Native App Boilerplate):

1. If `/data` can already be written, nothing is requested.
2. A resident Lapy service gets 1.5 seconds to answer the request the app
   publishes in `/download0/elevate_proc`.
3. Otherwise the app sends its own **one-shot helper**, `lapy.elf` in the app's
   folder, to the console's payload loader on loopback port 9021 and completes
   upstream's request/prepare/prepared/response exchange on that connection.
4. Access counts only after a file under `/data` was written, read back,
   compared and removed.

The helper is built from the pinned upstream commit by upstream's own
`owned-helper` target for exactly this title (`PPSA99006`); the build checks
its manifest and hashes before packaging it with Lapy's MIT license
([`tools/build-lapy-helper.py`](../tools/build-lapy-helper.py)).
ProsperoPuzzles contains no kernel code of its own. Loading Lapy separately is
not needed; a payload loader on port 9021 is.

`make APP_LAPY_HELPER=0` builds a package without the helper. Such a build can
still be granted access by a resident Lapy service.

## Without filesystem access

When there is no resident service and no payload loader, or the request is
refused, the app keeps its sandbox paths and works as earlier versions did:
data in `/download0/prosperopuzzles`, its own files in `/app0`. The first log
line after `[PPZ] entry` says which it was:

```text
[PPZ] storage access=0 route=helper app=/system_ex/app/PPSA99006 data=/data/prosperopuzzles dir=1
```

`access` is 0 when granted (otherwise the client's status code) and `route` is
`existing`, `resident`, `helper` or `none`.

## The app's own folder

With filesystem access the sandbox root is gone, so `/app0` may not exist. The
app then finds its files (fonts, sounds, the helpers) in the first of these
that holds `eboot.bin`: `/app0`, `/system_ex/app/PPSA99006` (the console's
mount of the running app, whatever drive it was installed on),
`/data/homebrew/PPSA99006`, `/mnt/sandbox/PPSA99006_000/app0`.

## Saves from earlier versions

Versions up to 01.000.010 kept everything in the sandbox
(`/download0/prosperopuzzles`). The first start with filesystem access copies
`settings.bin`, `library.bin` and `games/*` from there into
`/data/prosperopuzzles` ([`src/core/migrate.cpp`](../src/core/migrate.cpp)).
Files already in the new folder are kept, the sandbox copy stays where it is
(an older version still finds its data), and the `migrated` marker makes it a
one-time step. A copy that failed is tried again at the next start.

## Tests

```bash
make test-unit GTEST_ARGS=--gtest_filter='Migrate*:SaveFile*'
make test-elevation
```

`test-elevation` runs the client against a scripted console: a resident
service, a claimed request without fallback, partial socket transfers, a
refusal, a failed preparation, a corrupt data proof and the fixed 24-byte wire
format. Host tests cannot establish kernel or firmware behaviour; see
[Console validation](UPDATES.md#console-validation).
