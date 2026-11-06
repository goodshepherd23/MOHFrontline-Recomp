# Converts a CISO (compact ISO: header, block map, then only the used blocks) to a plain GameCube ISO.
# Usage: python ciso2iso.py <in.ciso> <out.iso>
import struct
import sys

GC_DISC_SIZE = 1459978240


def main(src, dst):
    with open(src, "rb") as f, open(dst, "wb") as out:
        header = f.read(0x8000)
        if header[:4] != b"CISO":
            raise SystemExit("not a CISO file: " + src)
        block = struct.unpack_from("<I", header, 4)[0]
        if block == 0 or block % 0x8000:
            raise SystemExit("bad CISO block size %d" % block)
        used = header[8:]
        last = max((i for i, b in enumerate(used) if b), default=-1)
        zero = bytes(block)
        for i in range(last + 1):
            if used[i]:
                data = f.read(block)
                if len(data) != block:
                    raise SystemExit("CISO is truncated at block %d" % i)
                out.write(data)
            else:
                out.write(zero)
        size = (last + 1) * block
        if size < GC_DISC_SIZE:
            out.truncate(GC_DISC_SIZE)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
