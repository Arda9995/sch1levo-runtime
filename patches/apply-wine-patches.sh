#!/bin/bash
# Applies sch1levo's only Wine source change (LGPL-2.1+, recorded in LICENSES/):
# Wine puts its server socket in /tmp unless it is *compiled* for Android. Our Wine is an x86-64 Linux
# build run by Box64 inside an Android app, which has no /tmp, so the Android branch is also enabled when
# compiled with -DSCH1LEVO_NO_TMP (the socket then lives in $WINEPREFIX/.wineserver).
set -euo pipefail
SRC="$1"
for f in server/request.c dlls/ntdll/unix/server.c; do
  grep -q '#ifdef __ANDROID__  /\* there.s no /tmp dir on Android \*/' "$SRC/$f" \
    || { echo "patch anchor not found in $f (Wine version changed?)" >&2; exit 1; }
  sed -i 's|#ifdef __ANDROID__  /\* there.s no /tmp dir on Android \*/|#if defined(__ANDROID__) \|\| defined(SCH1LEVO_NO_TMP)  /* no /tmp for Android apps */|' "$SRC/$f"
  grep -q 'SCH1LEVO_NO_TMP' "$SRC/$f" || { echo "patch failed in $f" >&2; exit 1; }
done
echo "wine patches applied"
