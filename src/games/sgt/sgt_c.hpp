// ProsperoPuzzles - C++ view of the vendored Tatham puzzles API.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Standard headers first: puzzles.h defines function-like min/max macros that
// would break them, and they are removed again below.
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

// The collection is compiled as a combined build (-DCOMBINED): every game
// exports its own `const game <name>` declared through generated-games.h.
extern "C"
{
#include "puzzles.h"
}

#undef min
#undef max

// Tatham's opaque front-end handle. Its definition is ours.
namespace ppz::sgt
{
class Session;
}

struct frontend
{
    ppz::sgt::Session *session;
};
