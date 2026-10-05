// ProsperoPuzzles - Updates: what the shell asks of the platform's updater.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

namespace ppz
{

// A newer release than the running one, listed by the homebrew.page catalog.
struct UpdateOffer
{
    bool installable = false; // the app can install it by itself
    std::string version;      // the release's name ("01.000.020")
    std::string available;    // its content version
    std::uint64_t size = 0;   // the download in bytes; 0 when the catalog doesn't say
};

enum class UpdatePhase : std::uint8_t
{
    idle,
    starting,    // starting the helper
    downloading, // done/total are bytes of the release
    unpacking,   // done/total are bytes unpacked
    ready,       // staged and verified: apply() or cancel()
    applying,    // the helper waits for the app to close
    cancelled,
    failed, // error says why; nothing was changed
};

struct UpdateProgress
{
    UpdatePhase phase = UpdatePhase::idle;
    std::uint64_t done = 0;
    std::uint64_t total = 0; // 0 while it isn't known
    std::string time_left;   // "about 20 s left"; empty until it can be said
    std::string error;
};

// The update check and the update itself. Every call returns at once: the
// network and the file work run on threads of the implementation.
class Updater
{
  public:
    virtual ~Updater() = default;
    // A newer release, once, when the check's answer has come.
    virtual bool take(UpdateOffer *offer) = 0;
    // Begins the download and the staging of the offered release.
    virtual bool begin() = 0;
    virtual UpdateProgress poll() = 0;
    // Stops it; until apply() has returned true the app is untouched.
    virtual void cancel() = 0;
    // In phase ready: true when the helper waits for the app to close.
    virtual bool apply() = 0;
    // After a cancel or a failure, before beginning again.
    virtual void finish() = 0;
};

} // namespace ppz
