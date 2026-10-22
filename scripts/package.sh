#!/bin/bash
# Assembles the runtime zip from the CI build outputs.
# Usage: scripts/package.sh <version> <box64-dir> <wine-dir> <pe-dir> <out-dir>
#   box64-dir: box64 (linked with the app's interpreter + rpath), glibc libs in lib/, x64lib/
#   wine-dir:  `make install DESTDIR` root of the x86-64 Wine build (prefix /wine)
#   pe-dir:    hello.exe, d3d11probe.exe, steamwebhelper.exe
set -euo pipefail
VERSION="$1"; BOX="$2"; WINE="$3"; PE="$4"; OUT="$5"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$(mktemp -d)/rt"
mkdir -p "$STAGE/bin" "$STAGE/glibc/lib" "$STAGE/box64-x64lib" "$STAGE/share/sch1levo" "$STAGE/LICENSES" "$OUT"

cp "$HERE/runtime/bin/sch1levo-runtime" "$STAGE/bin/"
cp "$BOX/box64" "$STAGE/bin/box64"
# The ELF interpreter must be executable, so the loader lives in bin/ (the app marks bin/* executable).
cp -L "$BOX/lib/ld-linux-aarch64.so.1" "$STAGE/bin/ld-linux-aarch64.so.1"
cp -L "$BOX"/lib/*.so* "$STAGE/glibc/lib/"
rm -f "$STAGE/glibc/lib/ld-linux-aarch64.so.1"
cp -RL "$BOX/x64lib/." "$STAGE/box64-x64lib/" 2>/dev/null || true
cp -RL "$WINE/wine" "$STAGE/wine"
cp "$PE"/*.exe "$STAGE/share/sch1levo/"
echo "$VERSION" > "$STAGE/share/sch1levo/VERSION"

cp "$HERE/LICENSE" "$STAGE/LICENSES/sch1levo-MIT.txt"
cp "$BOX/LICENSE" "$STAGE/LICENSES/box64-MIT.txt"
cp "$WINE/COPYING.LIB" "$STAGE/LICENSES/wine-LGPL-2.1.txt"
cp "$HERE/patches/apply-wine-patches.sh" "$STAGE/LICENSES/wine-patch.sh"
cp "$HERE/scripts/patch-ldso.py" "$STAGE/LICENSES/patch-ldso.py"
cp "$HERE/src/android_seccomp_shim.c" "$STAGE/LICENSES/android_seccomp_shim.c"
cp "$BOX/GLIBC-SOURCE.txt" "$STAGE/LICENSES/glibc-source.txt"
cat > "$STAGE/LICENSES/README.txt" <<TXT
sch1levo runtime $VERSION
- Box64 (MIT): https://github.com/ptitSeb/box64 (tag in VERSIONS.txt), unmodified source; linked with the app's
  ELF interpreter path and RPATH (linker flags in .github/workflows/build.yml).
- Wine (LGPL-2.1-or-later): https://dl.winehq.org/wine/source/ (version in VERSIONS.txt). Only change: wine-patch.sh
  (Android-style wineserver socket directory). Build recipe: .github/workflows/build.yml of
  https://github.com/Arda9995/sch1levo-runtime
- GNU C Library and GCC runtime libraries (LGPL / GPL with runtime exception) from Ubuntu arm64 packages; see glibc-source.txt.
  Only change: bin/ld-linux-aarch64.so.1 has its set_robust_list system call replaced by a no-op (patch-ldso.py),
  because Android's app sandbox kills processes that make it.
- android_seccomp_shim.c (MIT), linked into box64: handles the other system calls Android's sandbox rejects.
- sch1levo scripts, hello.exe, d3d11probe.exe, steamwebhelper wrapper: MIT.
TXT
cp "$BOX/VERSIONS.txt" "$STAGE/LICENSES/VERSIONS.txt"

ZIP="$OUT/sch1levo-runtime-$VERSION.zip"
(cd "$STAGE" && zip -q -r -X "$ZIP" .)
SHA="$(sha256sum "$ZIP" | cut -d' ' -f1)"
SIZE="$(stat -c %s "$ZIP")"
echo "$SHA  $(basename "$ZIP")" > "$ZIP.sha256"
cat > "$OUT/manifest-snippet.properties" <<PROPS
runtime.available=true
runtime.version=$VERSION
runtime.url=https://github.com/Arda9995/sch1levo-runtime/releases/download/runtime-$VERSION/$(basename "$ZIP")
runtime.sha256=$SHA
runtime.size=$SIZE
PROPS
du -sh "$STAGE" "$ZIP"
cat "$OUT/manifest-snippet.properties"
