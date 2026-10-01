#!/usr/bin/env bash
# ProsperoPuzzles - Run the shell on the host and write the frames of leaving a game.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$root/tools/ninja-build.sh"
cxx=$(command -v "${HOST_CXX:-clang++}")
cc=$(command -v "${HOST_CC:-clang}")
build="$root/build/host-transition"
ninja_begin "$build/build.ninja"

sources=("$root/host/transition_main.cpp" "$root/src/app/shell.cpp" "$root/host/platform_host.cpp" "$root/src/app/thumbnails.cpp"
    "$root/src/core/library.cpp" "$root/src/core/version.cpp" "$root/src/core/settings.cpp" "$root/src/ui/settings_scene.cpp" "$root/src/ui/about_scene.cpp" "$root/src/core/save_file.cpp" "$root/src/gfx/draw_list.cpp"
    "$root/src/gfx/font.cpp" "$root/src/gfx/gl_batch.cpp" "$root/src/gfx/gl_program.cpp"
    "$root/src/games/registry.cpp" "$root/src/games/native.cpp" "$root/src/games/records.cpp" "$root/src/games/sgt/sgt_catalog.cpp"
    "$root/src/games/sgt/sgt_canvas.cpp" "$root/src/games/sgt/sgt_skin.cpp" "$root/src/games/sgt/sgt_scene.cpp"
    "$root/src/gfx/canvas.cpp" "$root/src/gfx/triangulate.cpp" "$root/src/ui/menu.cpp"
    "$root/src/games/g2048/g2048_scene.cpp" "$root/src/games/tenfold/tenfold_scene.cpp"
    "$root/src/ui/theme.cpp" "$root/src/ui/gallery.cpp" "$root/src/ui/confetti.cpp" "$root/src/ui/howto_card.cpp" "$root/src/ui/library_scene.cpp")
# Native puzzles: the kit and every game directory built on it.
for source in "$root"/src/games/kit/*.cpp "$root"/src/games/{crowns,linkup,trail,kakuro,nurikabe,trafficjam,colorsort,sokoban}/*.cpp; do
    [[ -e $source ]] && sources+=("$source")
done
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
ninja_edge LINK "$build/ppz_transition" "$cxx" "${objects[@]}" -lEGL -lGL -lm -o "$build/ppz_transition"
ninja_run >/dev/null || ninja -f "$build/build.ninja" 2>&1 | grep -E 'error|FAILED' | head -20

output=${1:-"$root/build/transition"}
mkdir -p "$output"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    "$build/ppz_transition" "$root/assets" "$output" "$output/data" "${@:2}"
