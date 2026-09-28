#!/usr/bin/env bash
# ProsperoPuzzles - Renders the system presentation art with the app renderer
# and converts it into sce_sys/icon0.png, pic0.dds and pic1.dds.
# Copyright (C) 2026 BlackBearReloaded
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Needs the host snapshot toolchain (Mesa llvmpipe), Python 3 with Pillow, and
# Windows texconv.exe (winget install Microsoft.DirectXTex.Texconv) run from
# WSL. texconv cannot read \\wsl.localhost paths, so the DDS conversion works
# in a Windows temporary folder.

set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work="$root/build/art"
mkdir -p "$work"

PPZ_ART=1 bash "$root/tools/host-snapshots.sh" "$work" 3840 2160

python3 - "$work" <<'EOF'
import sys
from PIL import Image
work = sys.argv[1]
icon = Image.open(f"{work}/art-icon.png").convert("RGB")
# The icon is the centred square of the 16:9 frame.
side = icon.height
left = (icon.width - side) // 2
icon.crop((left, 0, left + side, side)).resize((512, 512), Image.LANCZOS).save(f"{work}/icon0.png")
Image.open(f"{work}/art-background.png").convert("RGB").save(f"{work}/background.png")
EOF

cp "$work/icon0.png" "$root/sce_sys/icon0.png"
cp "$work/background.png" "$root/sce_sys/background-source.png"
cp "$work/background.png" "$root/sce_sys/launch-background-source.png"

windows_work=$(powershell.exe -NoProfile -Command '$d = Join-Path $env:TEMP "ppz-art"; New-Item -ItemType Directory -Force $d | Out-Null; $d' | tr -d '\r')
cp "$work/background.png" "$(wslpath "$windows_work")/background.png"
powershell.exe -NoProfile -Command "& { \$t = (Get-Command texconv.exe -ErrorAction SilentlyContinue).Source; if (-not \$t) { \$t = Get-ChildItem \"\$env:LOCALAPPDATA\\Microsoft\\WinGet\\Packages\" -Recurse -Filter texconv.exe | Select-Object -First 1 -ExpandProperty FullName }; & \$t -nologo -y -w 3840 -h 2160 -m 1 -f BC7_UNORM -dx10 -o '$windows_work' '$windows_work\\background.png'; exit \$LASTEXITCODE }"
cp "$(wslpath "$windows_work")/background.dds" "$root/sce_sys/pic0.dds"
cp "$(wslpath "$windows_work")/background.dds" "$root/sce_sys/pic1.dds"
bash "$root/tools/prepare-assets.sh" --validate-only
