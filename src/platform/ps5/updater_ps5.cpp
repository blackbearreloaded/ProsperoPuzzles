// ProsperoPuzzles - The console's updater: the catalog check and the self-update.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#include "platform/ps5/updater_ps5.hpp"

#include "platform/ps5/storage.hpp"
#include "platform/ps5/system.hpp"
#include "update/self_update.h"
#include "update/update_check.h"

#ifdef PPZ_DEV_UPDATE_OFFER
#include "core/save_file.hpp"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>

namespace ppz::ps5
{

namespace
{

// libcurl and OpenSSL want more stack than a default thread of the console has.
constexpr std::size_t kStackSize = 1024 * 1024;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
bool started = false;
bool found = false; // an offer nobody has been told about yet
self_update_check_result answer = SELF_UPDATE_UNKNOWN;
self_update_offer offer{};
self_update_job job{}; // zero until the first begin, as the kit asks
bool begun = false;

struct Guard
{
    Guard()
    {
        pthread_mutex_lock(&lock);
    }
    ~Guard()
    {
        pthread_mutex_unlock(&lock);
    }
    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;
};

const char *result_name(self_update_check_result result)
{
    static const char *const names[] = {"available", "up-to-date", "unknown", "untrusted",
                                        "not-installable"};
    return static_cast<unsigned>(result) < 5 ? names[result] : "?";
}

const char *phase_name(self_update_phase phase)
{
    static const char *const names[] = {"idle",  "starting", "downloading", "unpacking",
                                        "ready", "applying", "cancelled",   "failed"};
    return static_cast<unsigned>(phase) < 8 ? names[phase] : "?";
}

#ifdef PPZ_DEV_UPDATE_OFFER
// Development only: update-offer.txt in the app's folder replaces the
// catalog's answer, so the update can be tried before the catalog lists a
// newer release. It skips the catalog's signature: never ship a build with
// it. Five lines: the new content version, the release's name, its ZIP on
// GitHub, its SHA-256, its size in bytes.
bool development_offer(self_update_offer *out)
{
    const Storage &where = storage();
    std::string text;
    if (!save::read_file(where.app_dir + "/update-offer.txt", &text))
        return false;
    std::string lines[5];
    std::size_t start = 0;
    for (std::string &line : lines)
    {
        const std::size_t end = text.find('\n', start);
        line = text.substr(start, end == std::string::npos ? end : end - start);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty())
            return false;
        start = end == std::string::npos ? text.size() : end + 1;
    }
    self_update_offer filled{};
    std::snprintf(filled.title, sizeof(filled.title), "%s", where.title_id.c_str());
    std::snprintf(filled.installed, sizeof(filled.installed), "%s", where.version.c_str());
    std::snprintf(filled.name, sizeof(filled.name), "ProsperoPuzzles");
    std::snprintf(filled.available, sizeof(filled.available), "%s", lines[0].c_str());
    std::snprintf(filled.version, sizeof(filled.version), "%s", lines[1].c_str());
    std::snprintf(filled.artifact, sizeof(filled.artifact), "%s", lines[2].c_str());
    std::snprintf(filled.sha256, sizeof(filled.sha256), "%s", lines[3].c_str());
    filled.size = std::strtoull(lines[4].c_str(), nullptr, 10);
    // Like the catalog, only a newer version is offered.
    int comparable = 0;
    if (update_check_version_compare(filled.available, filled.installed, &comparable) <= 0 ||
        comparable == 0)
        return false;
    *out = filled;
    return true;
}
#endif

void *check(void *)
{
    const std::int64_t begin = sys::monotonic_us();
    self_update_offer result{};
    self_update_check_result state = self_update_check_self(&result);
#ifdef PPZ_DEV_UPDATE_OFFER
    if (development_offer(&result))
    {
        sys::log("[PPZ] update check: DEVELOPMENT OFFER, the catalog's signature is skipped");
        state = SELF_UPDATE_AVAILABLE;
    }
#endif
    sys::log("[PPZ] update check result=%s installed=%s available=%s version=%s size=%llu ms=%lld",
             result_name(state), result.installed[0] != '\0' ? result.installed : "-",
             result.available[0] != '\0' ? result.available : "-",
             result.version[0] != '\0' ? result.version : "-",
             static_cast<unsigned long long>(result.size),
             static_cast<long long>((sys::monotonic_us() - begin) / 1000));
    if (state == SELF_UPDATE_AVAILABLE || state == SELF_UPDATE_NOT_INSTALLABLE)
    {
        const Guard guard;
        answer = state;
        offer = result;
        found = true;
    }
    return nullptr;
}

UpdatePhase from_kit(self_update_phase phase)
{
    switch (phase)
    {
    case SELF_UPDATE_STARTING:
        return UpdatePhase::starting;
    case SELF_UPDATE_DOWNLOADING:
        return UpdatePhase::downloading;
    case SELF_UPDATE_UNPACKING:
        return UpdatePhase::unpacking;
    case SELF_UPDATE_READY:
        return UpdatePhase::ready;
    case SELF_UPDATE_APPLYING:
        return UpdatePhase::applying;
    case SELF_UPDATE_CANCELLED:
        return UpdatePhase::cancelled;
    case SELF_UPDATE_FAILED:
        return UpdatePhase::failed;
    default:
        return UpdatePhase::idle;
    }
}

} // namespace

void ConsoleUpdater::start()
{
    {
        const Guard guard;
        if (started)
            return;
        started = true;
    }
    pthread_attr_t attributes;
    if (pthread_attr_init(&attributes) != 0)
        return;
    pthread_attr_setstacksize(&attributes, kStackSize);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    pthread_t thread;
    if (pthread_create(&thread, &attributes, check, nullptr) != 0)
        sys::log("[PPZ] update check: the thread could not start");
    pthread_attr_destroy(&attributes);
}

bool ConsoleUpdater::take(UpdateOffer *out)
{
    const Guard guard;
    if (!found)
        return false;
    found = false;
    out->installable = answer == SELF_UPDATE_AVAILABLE;
    out->version = offer.version[0] != '\0' ? offer.version : offer.available;
    out->available = offer.available;
    out->size = offer.size;
    return true;
}

bool ConsoleUpdater::begin()
{
    const Guard guard;
    if (answer != SELF_UPDATE_AVAILABLE)
        return false;
    if (begun)
        self_update_finish(&job);
    begun = self_update_start(&job, self_update_console(), &offer) == 1;
    sys::log("[PPZ] update %s to %s", begun ? "begun" : "could not begin", offer.available);
    return begun;
}

UpdateProgress ConsoleUpdater::poll()
{
    static self_update_phase logged = SELF_UPDATE_IDLE;
    UpdateProgress progress;
    const Guard guard;
    if (!begun)
        return progress;
    self_update_status status{};
    self_update_poll(&job, &status);
    progress.phase = from_kit(status.phase);
    progress.done = status.done;
    progress.total = status.total;
    progress.time_left = status.time_left;
    progress.error = status.error;
    if (status.phase != logged)
    {
        logged = status.phase;
        sys::log("[PPZ] update phase=%s done=%llu total=%llu%s%s", phase_name(status.phase),
                 static_cast<unsigned long long>(status.done),
                 static_cast<unsigned long long>(status.total),
                 status.error[0] != '\0' ? " error=" : "", status.error);
    }
    return progress;
}

void ConsoleUpdater::cancel()
{
    const Guard guard;
    if (begun)
        self_update_cancel(&job);
}

bool ConsoleUpdater::apply()
{
    const Guard guard;
    if (!begun)
        return false;
    const bool going = self_update_apply(&job) == 1;
    sys::log("[PPZ] update %s", going ? "staged: the helper replaces the files once the app closed"
                                      : "refused: the helper did not take the go-ahead");
    return going;
}

void ConsoleUpdater::finish()
{
    const Guard guard;
    if (!begun)
        return;
    self_update_finish(&job);
    begun = false;
}

} // namespace ppz::ps5
