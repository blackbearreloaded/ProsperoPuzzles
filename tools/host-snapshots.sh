#!/usr/bin/env bash
# ProsperoPuzzles - Render UI scenes on the host (Mesa surfaceless EGL) to PNG.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tools/ninja-build.sh"
cxx=$(command -v "${HOST_CXX:-clang++}")
cc=$(command -v "${HOST_CC:-clang}")
build="$root/build/host-snapshots"
ninja_begin "$build/build.ninja"

sources=("$root/host/snapshot_main.cpp" "$root/host/platform_host.cpp"
    "$root/src/core/library.cpp" "$root/src/core/save_file.cpp" "$root/src/gfx/draw_list.cpp"
    "$root/src/gfx/font.cpp" "$root/src/gfx/gl_batch.cpp" "$root/src/gfx/gl_program.cpp"
    "$root/src/games/registry.cpp" "$root/src/games/sgt/sgt_catalog.cpp"
    "$root/src/games/sgt/sgt_canvas.cpp" "$root/src/games/sgt/sgt_scene.cpp"
    "$root/src/gfx/canvas.cpp" "$root/src/gfx/triangulate.cpp"
    "$root/src/ui/theme.cpp" "$root/src/ui/gallery.cpp" "$root/src/ui/library_scene.cpp")
while IFS= read -r -d '' source; do
    sources+=("$source")
done < <(find "$root/src/third_party/sgt-puzzles" -name '*.c' -print0 | sort -z)
# The catalog references every game, whose front-end hooks live in the session.
sources+=("$root/src/games/sgt/sgt_session.cpp")

objects=()
for source in "${sources[@]}"; do
    relative=${source#"$root/"}
    object="$build/obj/${relative//\//_}.o"
    if [[ $source == *.c ]]; then
        ninja_inputs=("$source" "$cc")
        ninja_edge CC "$object" "${compiler_cache[@]}" "$cc" -std=c11 -O2 -w -DCOMBINED -DNO_TGMATH_H \
            -I"$root/src/third_party/sgt-puzzles" -MD -MF "$object.d" -c "$source" -o "$object"
    else
        ninja_inputs=("$source" "$cxx")
        ninja_edge CXX "$object" "${compiler_cache[@]}" "$cxx" -std=c++20 -O2 -Wall -Wextra \
            -DGL_GLEXT_PROTOTYPES=1 -DCOMBINED -I"$root/src" -I"$root/src/third_party/sgt-puzzles" \
            -MD -MF "$object.d" -c "$source" -o "$object"
    fi
    objects+=("$object")
done
ninja_inputs=("${objects[@]}")
ninja_edge LINK "$build/ppz_snapshots" "$cxx" "${objects[@]}" -lEGL -lGL -lm -o "$build/ppz_snapshots"
ninja_run >/dev/null || ninja -f "$build/build.ninja" 2>&1 | grep -E 'error|FAILED' | head -20

output=${1:-"$root/build/snapshots"}
mkdir -p "$output"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    "$build/ppz_snapshots" "$root/assets" "$output" "${@:2}"
