#!/usr/bin/env python3
"""Removes the set_robust_list system call from the ARM64 glibc loader (standard library only).

Usage: patch-ldso.py <ld-linux-aarch64.so.1>

Android's app sandbox kills any process that calls set_robust_list (syscall 99). glibc's loader makes that
call while setting up the first thread, before any code of box64 (and its SIGSYS shim) runs. The loader
ignores the call's result, so the `svc #0` is replaced by `nop`. Exactly one call site must be found:
`mov x8, #99` followed by `svc #0` with no other write to x8 in between. Anything else fails the build.
"""
import struct
import sys

MOV_X8_99 = 0xD2800C68          # movz x8, #0x63
SVC_0 = 0xD4000001              # svc #0
NOP = 0xD503201F
MAX_GAP = 12                    # instructions between the mov and the svc


def writes_x8(insn):
    # Any data-processing or load instruction whose destination register (bits 0-4) is x8/w8.
    # Stores, branches and system instructions have no destination register in that field.
    top = insn >> 24
    is_store = (insn & 0x3B400000) == 0x39000000 or (insn & 0x3E400000) == 0x28000000
    is_branch_or_system = (top & 0x7C) == 0x14 or (top & 0xFE) == 0xD4 or (top & 0xFE) == 0xD6
    return (insn & 0x1F) == 8 and not is_store and not is_branch_or_system


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: patch-ldso.py <ld-linux-aarch64.so.1>")
    path = sys.argv[1]
    data = bytearray(open(path, "rb").read())
    if data[:4] != b"\x7fELF" or struct.unpack_from("<H", data, 18)[0] != 183:
        sys.exit("patch-ldso: not an AArch64 ELF")
    words = len(data) // 4
    sites = []
    for i in range(words):
        if struct.unpack_from("<I", data, i * 4)[0] != MOV_X8_99:
            continue
        for j in range(i + 1, min(i + 1 + MAX_GAP, words)):
            insn = struct.unpack_from("<I", data, j * 4)[0]
            if insn == SVC_0:
                sites.append(j * 4)
                break
            if writes_x8(insn):
                break
    if len(sites) != 1:
        sys.exit(f"patch-ldso: expected 1 set_robust_list call, found {len(sites)} at {[hex(s) for s in sites]}")
    struct.pack_into("<I", data, sites[0], NOP)
    open(path, "wb").write(data)
    print(f"patch-ldso: set_robust_list call at file offset {sites[0]:#x} replaced by nop")


if __name__ == "__main__":
    main()
