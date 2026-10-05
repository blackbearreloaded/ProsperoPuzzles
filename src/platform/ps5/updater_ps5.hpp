// ProsperoPuzzles - The console's updater: the catalog check and the self-update.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/updater.hpp"

namespace ppz::ps5
{

// The PS5 Native App Boilerplate's self-update kit (src/update). Once per
// launch it asks the homebrew.page catalog whether a newer release of the app
// is listed, verifying the catalog's signature and the hashes. On the player's
// yes it downloads the release from GitHub and streams it to the helper
// (self-updater.elf in the app's folder, sent to the console's payload
// loader), which checks and unpacks it beside the app and replaces the app's
// files once the app has closed.
class ConsoleUpdater final : public Updater
{
  public:
    // Starts the check on a thread of its own; only the first call does anything.
    void start();

    bool take(UpdateOffer *offer) override;
    bool begin() override;
    UpdateProgress poll() override;
    void cancel() override;
    bool apply() override;
    void finish() override;
};

} // namespace ppz::ps5
