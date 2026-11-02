# Converts the disc's files/Moh2RelGC.elf (the real game; sys/main.dol is only a boot stub)
# into a GameCube DOL. Usage: python elf2dol.py <in.elf> <out.dol>
import struct
import sys


def main(src, dst):
    elf = open(src, "rb").read()
    if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 2:
        raise SystemExit("not a 32-bit big-endian ELF: " + src)
    entry, phoff = struct.unpack_from(">II", elf, 0x18)
    phentsize, phnum = struct.unpack_from(">HH", elf, 0x2A)
    text, data = [], []
    bss_lo, bss_hi = None, None
    for i in range(phnum):
        p_type, p_off, p_vaddr, _, p_filesz, p_memsz, p_flags, _ = struct.unpack_from(
            ">IIIIIIII", elf, phoff + i * phentsize)
        if p_type != 1 or p_memsz == 0:  # PT_LOAD only
            continue
        if p_filesz:
            seg = (p_vaddr, elf[p_off:p_off + p_filesz])
            (text if p_flags & 1 else data).append(seg)
        if p_memsz > p_filesz:
            lo, hi = p_vaddr + p_filesz, p_vaddr + p_memsz
            bss_lo = lo if bss_lo is None else min(bss_lo, lo)
            bss_hi = hi if bss_hi is None else max(bss_hi, hi)
    if len(text) > 7 or len(data) > 11:
        raise SystemExit("too many segments for a DOL")
    header = bytearray(0x100)
    offset = 0x100
    body = bytearray()
    for slots, base in ((text, 0), (data, 7)):
        for n, (addr, blob) in enumerate(slots):
            pad = (-len(blob)) % 32
            struct.pack_into(">I", header, (base + n) * 4, offset)
            struct.pack_into(">I", header, 0x48 + (base + n) * 4, addr)
            struct.pack_into(">I", header, 0x90 + (base + n) * 4, len(blob) + pad)
            body += blob + b"\0" * pad
            offset += len(blob) + pad
    if bss_lo is not None:
        struct.pack_into(">II", header, 0xD8, bss_lo, bss_hi - bss_lo)
    struct.pack_into(">I", header, 0xE0, entry)
    open(dst, "wb").write(bytes(header) + bytes(body))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
