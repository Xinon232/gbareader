#!/usr/bin/env bash
# Builds the ready-made converter (the window, tools/gbcomic_gui.py, around
# tools/gbcomic.py) for Linux and Windows:
#   gbcomic-linux-x86_64.zip   gbcomic (PyInstaller one-file), glibc 2.28+
#   gbcomic-windows-x64.zip    gbcomic.exe (PyInstaller one-file, built under Wine)
# Usage: tools/converter/build.sh OUT_DIR
# Needs: curl, zip, wine (64-bit), python3 with pip (to fetch Windows wheels).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
tools="$(dirname "$here")"
out="$(mkdir -p "$1" && cd "$1" && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
# Portable CPython builds (python-build-standalone) with Tk; older ones crash Tk on some X servers.
PY=3.12.12
PBS=20251010
url="https://github.com/astral-sh/python-build-standalone/releases/download/$PBS/cpython-$PY+$PBS"
excludes=(--exclude-module unittest --exclude-module pydoc)

# Linux.
curl -sSL -o "$work/linux.tar.gz" "$url-x86_64-unknown-linux-gnu-install_only.tar.gz"
mkdir -p "$work/linux-python" && tar xzf "$work/linux.tar.gz" -C "$work/linux-python"
"$work/linux-python/python/bin/python3" -m pip install -q pyinstaller pillow pypdfium2
mkdir -p "$work/lb" && cp "$tools/gbcomic.py" "$tools/gbcomic_gui.py" "$tools/gbcomic_preview.py" "$work/lb/"
( cd "$work/lb" && "$work/linux-python/python/bin/python3" -m PyInstaller --onefile --windowed --strip \
    --name gbcomic --log-level ERROR "${excludes[@]}" gbcomic_gui.py )
mkdir -p "$work/linux/gbcomic-linux"
cp "$work/lb/dist/gbcomic" "$tools/gbcomic.py" "$tools/gbcomic_gui.py" "$tools/gbcomic_preview.py" "$work/linux/gbcomic-linux/"
cp "$here/README-linux.txt" "$work/linux/gbcomic-linux/README.txt"
( cd "$work/linux" && rm -f "$out/gbcomic-linux-x86_64.zip" && zip -qr "$out/gbcomic-linux-x86_64.zip" gbcomic-linux )

# Windows: the same under Wine, with wheels fetched for win_amd64.
# (Wine's Python needs a pipe, not a file, as its output.)
export WINEDEBUG=-all WINEPREFIX="$work/wine"
curl -sSL -o "$work/win.tar.gz" "$url-x86_64-pc-windows-msvc-install_only.tar.gz"
mkdir -p "$work/win-python" && tar xzf "$work/win.tar.gz" -C "$work/win-python"
python3 -m pip download -q --only-binary=:all: --platform win_amd64 --python-version 3.12 \
  pyinstaller pillow pypdfium2 -d "$work/wheels"
python3 -m pip download -q --no-deps pefile pywin32-ctypes -d "$work/wheels"
wine "$work/win-python/python/python.exe" -m pip install -q --no-index --find-links "$work/wheels" \
  pyinstaller pillow pypdfium2 </dev/null 2>&1 | cat
mkdir -p "$work/wb" && cp "$tools/gbcomic.py" "$tools/gbcomic_gui.py" "$tools/gbcomic_preview.py" "$work/wb/"
( cd "$work/wb" && wine "$work/win-python/python/python.exe" -m PyInstaller --onefile --windowed \
    --name gbcomic --log-level ERROR "${excludes[@]}" gbcomic_gui.py </dev/null 2>&1 | cat )
mkdir -p "$work/win/gbcomic-windows"
cp "$work/wb/dist/gbcomic.exe" "$tools/gbcomic.py" "$tools/gbcomic_gui.py" "$tools/gbcomic_preview.py" "$work/win/gbcomic-windows/"
sed 's/$/\r/' "$here/README-windows.txt" > "$work/win/gbcomic-windows/README.txt"
( cd "$work/win" && rm -f "$out/gbcomic-windows-x64.zip" && zip -qr "$out/gbcomic-windows-x64.zip" gbcomic-windows )
wineserver -k 2>/dev/null || true
ls -la "$out"
