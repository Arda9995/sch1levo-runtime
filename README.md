# sch1levo-runtime

Build recipe for the **sch1levo runtime**: what the sch1levo Android app downloads to run the Windows
Steam version of Schedule I on an ARM64 tablet. Everything is built from source in GitHub Actions
(`.github/workflows/build.yml`) and published as GitHub Releases with a SHA-256.

| Part | Source | License |
|---|---|---|
| Box64 (x86-64 → ARM64) | ptitSeb/box64, pinned tag, unmodified | MIT |
| Wine (x86-64 Linux build, WoW64) | dl.winehq.org source, SHA-256 checked; one patch (`patches/apply-wine-patches.sh`) | LGPL-2.1+ |
| glibc + GCC runtime (ARM64) | Ubuntu 24.04 arm64 packages | LGPL / GPL with runtime exception |
| `bin/sch1levo-runtime`, test programs, Steam UI wrapper | this repository | MIT |

The app calls only `bin/sch1levo-runtime <command>` (see the app's `docs/RUNTIME_CONTRACT.md`).

**Round 1 (current):** checks that Box64 and Wine run on the tablet at all (`version`, `prefix-init`, an x86-64 console program).
There is no display bridge yet, so the D3D11 test and Steam report `NOT_AVAILABLE`.

No third-party game launchers (Winlator, Mobox, GameHub, …) are used or copied.
