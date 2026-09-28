#!/usr/bin/env bash
# ProsperoPuzzles - Render UI scenes on the host (Mesa surfaceless EGL) to PNG.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tools/ninja-build.sh"
cxx=$(command -v "${HOST_CXX:-clang++}")
build="$root/build/host-snapshots"
ninja_begin "$build/build.ninja"

sources=("$root/host/snapshot_main.cpp" "$root/host/platform_host.cpp"
    "$root/src/core/save_file.cpp" "$root/src/gfx/draw_list.cpp" "$root/src/gfx/font.cpp"
    "$root/src/gfx/gl_batch.cpp" "$root/src/gfx/gl_program.cpp" "$root/src/ui/theme.cpp"
    "$root/src/ui/gallery.cpp")
objects=()
for source in "${sources[@]}"; do
    relative=${source#"$root/"}
    object="$build/obj/${relative//\//_}.o"
    ninja_inputs=("$source" "$cxx")
    ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" -std=c++20 -O2 -Wall -Wextra \
        -DGL_GLEXT_PROTOTYPES=1 -I"$root/src" -MD -MF "$object.d" -c "$source" -o "$object"
    objects+=("$object")
done
ninja_inputs=("${objects[@]}")
ninja_edge LINK "$build/ppz_snapshots" "$cxx" "${objects[@]}" -lEGL -lGL -o "$build/ppz_snapshots"
ninja_run >/dev/null

output=${1:-"$root/build/snapshots"}
mkdir -p "$output"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    "$build/ppz_snapshots" "$root/assets" "$output" "${@:2}"
