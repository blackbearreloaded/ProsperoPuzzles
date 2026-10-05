/*
 * ProsperoPuzzles - Paths for the self-update kit.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * With filesystem access the app's folder is not /app0 and its data is not in
 * /download0, so the kit asks the app (platform/ps5/storage.cpp): 0 the helper
 * in the app's folder, 1 the app's param.json, 2 the file that keeps the
 * catalog's highest sequence.
 */
#ifndef PPZ_UPDATE_PATHS_H
#define PPZ_UPDATE_PATHS_H

#ifdef __cplusplus
extern "C"
{
#endif

    const char *ppz_self_update_path(int which);

#ifdef __cplusplus
}
#endif

#define SELF_UPDATE_HELPER_PATH ppz_self_update_path(0)
#define SELF_UPDATE_PARAM_PATH ppz_self_update_path(1)
#define SELF_UPDATE_SEQUENCE_PATH ppz_self_update_path(2)

#endif
