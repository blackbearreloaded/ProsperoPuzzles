#!/usr/bin/env bash
# ProsperoPuzzles - Rebuild the baked SDF fonts in assets/fonts.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cxx=$(command -v "${HOST_CXX:-clang++}")
mkdir -p "$root/build/host" "$root/assets/fonts"
"$cxx" -std=c++20 -O2 -w "$root/tools/font-baker/bake_font.cpp" -o "$root/build/host/bake_font"
for weight in Regular SemiBold; do
    "$root/build/host/bake_font" "$root/third_party/fonts/Inter-$weight.ttf" \
        "$root/assets/fonts/inter-${weight,,}.ppzfont" 56 8 1024
done
cp "$root/third_party/fonts/Inter-LICENSE.txt" "$root/assets/fonts/Inter-LICENSE.txt"
