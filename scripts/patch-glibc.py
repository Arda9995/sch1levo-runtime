#!/usr/bin/env python3
"""Takes system calls that Android's app sandbox rejects out of the ARM64 glibc binaries (standard library only).

Usage: patch-glibc.py <file> <syscall>:<expected sites> [<syscall>:<expected sites> ...]
  e.g. patch-glibc.py ld-linux-aarch64.so.1 99:1
       patch-glibc.py libc.so.6 99:2 439:1

Android kills a process (SIGSYS) that makes a rejected system call. A SIGSYS handler cannot help when the
call happens while signals are blocked: the kernel then resets the handler and kills anyway. glibc does exactly
that for set_robust_list (99) when it starts a thread, and the loader makes the call before any handler exists.
Each call site, `mov x8, #<nr>` followed by `svc #0` with no other write to x8 in between, has its `svc #0`
replaced by `movn x0, #37`: the call returns -ENOSYS without entering the kernel. glibc ignores the result of
set_robust_list and falls back to faccessat when faccessat2 (439) returns ENOSYS.
The number of sites must match exactly, so a different glibc build fails here instead of on the tablet.
"""
import struct
import sys

SVC_0 = 0xD4000001
RET_ENOSYS = 0x928004A0          # movn x0, #37  ->  x0 = -38 = -ENOSYS
MAX_GAP = 12                     # instructions between the mov and the svc


def mov_x8(nr):
    return 0xD2800000 | (nr << 5) | 8          # movz x8, #nr


def writes_x8(insn):
    # Data-processing or load instruction with x8/w8 as destination (bits 0-4).
    # Stores, branches and system instructions have no destination register in that field.
    top = insn >> 24
    is_store = (insn & 0x3B400000) == 0x39000000 or (insn & 0x3E400000) == 0x28000000
    is_branch_or_system = (top & 0x7C) == 0x14 or (top & 0xFE) == 0xD4 or (top & 0xFE) == 0xD6
    return (insn & 0x1F) == 8 and not is_store and not is_branch_or_system


def find_sites(data, nr):
    words = len(data) // 4
    want = mov_x8(nr)
    sites = []
    for i in range(words):
        if struct.unpack_from("<I", data, i * 4)[0] != want:
            continue
        for j in range(i + 1, min(i + 1 + MAX_GAP, words)):
            insn = struct.unpack_from("<I", data, j * 4)[0]
            if insn == SVC_0:
                sites.append(j * 4)
                break
            if writes_x8(insn):
                break
    return sites


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    path = sys.argv[1]
    data = bytearray(open(path, "rb").read())
    if data[:4] != b"\x7fELF" or struct.unpack_from("<H", data, 18)[0] != 183:
        sys.exit(f"patch-glibc: {path} is not an AArch64 ELF")
    for spec in sys.argv[2:]:
        nr, expected = (int(x) for x in spec.split(":"))
        sites = find_sites(data, nr)
        if len(sites) != expected:
            sys.exit(f"patch-glibc: {path}: syscall {nr}: expected {expected} site(s), "
                     f"found {len(sites)} at {[hex(s) for s in sites]}")
        for off in sites:
            struct.pack_into("<I", data, off, RET_ENOSYS)
        print(f"patch-glibc: {path}: syscall {nr} -> -ENOSYS at {[hex(s) for s in sites]}")
    open(path, "wb").write(data)


if __name__ == "__main__":
    main()
