#!/usr/bin/env python3
"""Checks that box64 is loadable from the app's install folder (standard library only).

Usage: check-elf.py <elf> <expected interpreter> <expected rpath>

Fails when:
  - the PT_INTERP path or DT_RPATH differ from what the app expects,
  - two PT_LOAD segments share a memory page (r1's patchelf output did this: the later, read-only
    mapping covered .dynamic, so the loader crashed writing to it before box64 printed anything),
  - PT_DYNAMIC is not inside a writable PT_LOAD.
"""
import struct
import sys

PT_LOAD, PT_DYNAMIC, PT_INTERP = 1, 2, 3
DT_NULL, DT_STRTAB, DT_RPATH, DT_RUNPATH = 0, 5, 15, 29
PF_W = 2
PAGE = 4096


def fail(msg):
    print(f"check-elf: FAIL: {msg}")
    sys.exit(1)


def main():
    if len(sys.argv) != 4:
        fail("usage: check-elf.py <elf> <interpreter> <rpath>")
    path, want_interp, want_rpath = sys.argv[1:]
    data = open(path, "rb").read()
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        fail("not a 64-bit little-endian ELF")
    phoff, = struct.unpack_from("<Q", data, 32)
    phentsize, phnum = struct.unpack_from("<HH", data, 54)
    phdrs = [struct.unpack_from("<IIQQQQQQ", data, phoff + i * phentsize) for i in range(phnum)]

    loads = [p for p in phdrs if p[0] == PT_LOAD]
    spans = sorted((p[3] // PAGE, (p[3] + p[6] - 1) // PAGE, p[1]) for p in loads)
    for (s1, e1, f1), (s2, e2, f2) in zip(spans, spans[1:]):
        if s2 <= e1:
            fail(f"PT_LOAD segments share page 0x{s2 * PAGE:x} (flags {f1} and {f2})")

    interp = next((p for p in phdrs if p[0] == PT_INTERP), None)
    if interp is None:
        fail("no PT_INTERP")
    got_interp = data[interp[2]:interp[2] + interp[5]].rstrip(b"\0").decode()
    if got_interp != want_interp:
        fail(f"interpreter {got_interp!r}, expected {want_interp!r}")

    dyn = next((p for p in phdrs if p[0] == PT_DYNAMIC), None)
    if dyn is None:
        fail("no PT_DYNAMIC")
    if not any(p[1] & PF_W and p[3] <= dyn[3] and dyn[3] + dyn[6] <= p[3] + p[6] for p in loads):
        fail("PT_DYNAMIC is not inside a writable PT_LOAD")

    entries = {}
    for off in range(dyn[2], dyn[2] + dyn[5], 16):
        tag, val = struct.unpack_from("<qQ", data, off)
        if tag == DT_NULL:
            break
        entries.setdefault(tag, val)
    if DT_RUNPATH in entries:
        fail("DT_RUNPATH present; libraries dlopened by glibc need DT_RPATH")
    if DT_RPATH not in entries:
        fail("no DT_RPATH")
    strtab_off = next(p[2] + entries[DT_STRTAB] - p[3] for p in loads
                      if p[3] <= entries[DT_STRTAB] < p[3] + p[5])
    start = strtab_off + entries[DT_RPATH]
    got_rpath = data[start:data.index(b"\0", start)].decode()
    if got_rpath != want_rpath:
        fail(f"rpath {got_rpath!r}, expected {want_rpath!r}")

    print(f"check-elf: OK {path}: interpreter {got_interp}, rpath {got_rpath}, {len(loads)} LOAD segments")


if __name__ == "__main__":
    main()
