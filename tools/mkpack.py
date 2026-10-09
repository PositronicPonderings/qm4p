#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
mkpack.py - bundle a folder of assets into one QA4P asset pack, ready to
drag onto the Pico as a UF2 (or load with picotool). The Pico reads it with
the qa4p library (qa4p/qa4p.h).

REQUIREMENTS
    Python 3 and Pillow (for converting images):   pip install pillow

EXAMPLE
    python3 tools/mkpack.py tests/hardware/pack --out build/assets
      -> build/assets.uf2   drag onto the Pico in BOOTSEL mode
      -> build/assets.bin   the same pack, raw (for picotool, see below)

WHAT GOES IN
    Every file under the folder, keyed by its path relative to the folder,
    with "/" separators: art/icons/star.png becomes "icons/star.bmp".

      .png .jpg .jpeg .gif   converted to 8-bit RLE8 BMP (via img2bmp8.py);
                             the name's extension becomes .bmp, and the
                             TRANSPARENT flag is set automatically if the
                             image has see-through pixels
      .bmp                   stored as-is; flagged TRANSPARENT if it was made
                             by img2bmp8.py with transparency (palette entry
                             255 is magenta), or if pack.txt says so
      anything else          stored as-is (text, game data...)

    Files starting with "." and the file pack.txt itself are skipped.
    Names can be up to 35 characters.

OPTIONAL: pack.txt (in the folder's top level)
    One line per file needing special treatment:
        icons/star.png   no-dither
        art/scene.jpg    width=240 dither
        ui/logo.bmp      transparent
        misc/notes.png   raw
    Options: no-dither, dither, width=N, height=N, colors=N, key=R,G,B,
             transparent, opaque, raw (store the file untouched).
    Lines starting with # are comments.

WHERE THE PACK GOES
    --offset says where in flash: 0x100000 (1 MB in) by default, which
    leaves the firmware the first megabyte. The program opens the pack at
    the same offset:  qa_open(&pack, 0x100000)  (QA_DEFAULT_OFFSET).

    A program can use several packs, each built separately with its own
    --offset, e.g. artwork at 0x100000 and game data at 0x280000. They must not
    overlap: the summary printed at the end gives the flash range each pack
    occupies, so you can check. (docs/RESOURCES.md has the QM4P flash plan.)

    --max-size is the largest pack allowed. By default it's the space from
    the offset to the end of a 4 MB flash (a Pico 2's), so a pack that would
    run off the end of the chip is refused. Give a smaller limit to keep a
    pack out of the next one's area, or a larger one for a bigger chip.

INSTALLING
    Drag and drop:  hold BOOTSEL, plug in, drag assets.uf2 onto the RP2350
                    drive. The firmware is untouched.
    picotool:       picotool load assets.bin -o 0x10100000
                    (-o is the flash ADDRESS: 0x10000000 plus --offset; the
                    summary prints the exact command.) The dependable route. The RP2350 bootrom has known quirks
                    with drag-and-drop UF2s, so if the demo still says "no
                    asset pack" after dragging, load it this way instead.
                    (Put the Pico in BOOTSEL mode first, or add -f to let
                    picotool restart it.)

PACK FORMAT (little-endian; read by qa4p/qa4p.c)
    Header, 32 bytes:
        0   4  identifying bytes "QAPK" (packs built for QG4P 1.0 have
               different ones and are refused: rebuild them with this script)
        4   2  version (1)
        6   2  number of entries
        8   4  total pack size in bytes
        12  4  CRC-32 of everything after the header
        16 16  reserved (zero)
    Table of contents: one 48-byte entry per file, sorted by name:
        0  36  name, NUL-terminated
        36  4  offset of the data from the start of the pack
        40  4  size in bytes
        44  2  type  (0 other, 1 image, 2 sound, 3 text)
        46  2  flags (bit 0: transparent image)
    Then the files' data, each starting on a 4-byte boundary.
"""

import argparse
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

MAGIC = b"QAPK"
VERSION = 1
HEADER_SIZE = 32
ENTRY_SIZE = 48
NAME_MAX = 35

TYPE_OTHER, TYPE_IMAGE, TYPE_SOUND, TYPE_TEXT = 0, 1, 2, 3
FLAG_TRANSPARENT = 0x0001

CONVERT_EXT = {".png", ".jpg", ".jpeg", ".gif"}
TYPE_BY_EXT = {".bmp": TYPE_IMAGE, ".wav": TYPE_SOUND, ".raw": TYPE_SOUND,
               ".txt": TYPE_TEXT, ".json": TYPE_TEXT, ".csv": TYPE_TEXT, ".ini": TYPE_TEXT}
TYPE_NAMES = {TYPE_OTHER: "other", TYPE_IMAGE: "image", TYPE_SOUND: "sound", TYPE_TEXT: "text"}

# UF2: 512-byte blocks each carrying 256 bytes of data for a flash address.
UF2_MAGIC0, UF2_MAGIC1, UF2_MAGIC_END = 0x0A324655, 0x9E5D5157, 0x0AB16F30
UF2_FLAG_FAMILY = 0x00002000
# RP2350 bootrom "families": which kind of data a UF2 block carries.
# "absolute" means "write these bytes at exactly this flash address", which
# is what a data pack needs. The others are for experiments (see --family).
UF2_FAMILIES = {"absolute": 0xE48BFF57, "data": 0xE48BFF58, "rp2350-arm-s": 0xE48BFF59}
FLASH_BASE = 0x10000000
FLASH_SIZE_DEFAULT = 4 * 1024 * 1024     # a Pico 2's flash: the default --max-size runs to its end
SECTOR = 4096                            # flash is erased 4 KB at a time


def read_manifest(folder):
    """pack.txt -> {relative path: {option: value}}"""
    opts = {}
    path = os.path.join(folder, "pack.txt")
    if not os.path.exists(path):
        return opts
    with open(path, encoding="utf-8") as fh:
        for n, line in enumerate(fh, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            parts = line.split()
            o = {}
            for p in parts[1:]:
                if "=" in p:
                    k, v = p.split("=", 1)
                    o[k] = v
                else:
                    o[p] = True
            opts[parts[0]] = o
    return opts


def bmp_made_transparent(data):
    """True if this BMP looks like img2bmp8.py output with transparency:
    256 palette entries with entry 255 pure magenta."""
    if len(data) < 54 or data[:2] != b"BM":
        return False
    hdr = struct.unpack_from("<I", data, 14)[0]
    colours = struct.unpack_from("<I", data, 46)[0] or 256
    if colours < 256:
        return False
    e = 14 + hdr + 255 * 4
    return len(data) >= e + 4 and data[e:e + 3] == bytes((255, 0, 255))   # B, G, R


def gather(folder, manifest):
    """Collect (name, bytes, type, flags, note) for every file."""
    import img2bmp8
    items = []
    for root, dirs, files in os.walk(folder):
        dirs[:] = sorted(d for d in dirs if not d.startswith("."))
        for fn in sorted(files):
            if fn.startswith(".") or (fn == "pack.txt" and root == folder):
                continue
            full = os.path.join(root, fn)
            rel = os.path.relpath(full, folder).replace(os.sep, "/")
            o = manifest.get(rel, {})
            base, ext = os.path.splitext(rel)
            ext = ext.lower()
            flags = 0
            note = ""

            if ext in CONVERT_EXT and "raw" not in o:
                key = tuple(int(v) for v in o["key"].split(",")) if "key" in o else None
                dither = True if "dither" in o else (False if "no-dither" in o else None)
                data, info = img2bmp8.convert(full,
                                              width=int(o["width"]) if "width" in o else None,
                                              height=int(o["height"]) if "height" in o else None,
                                              colors=int(o.get("colors", 0)),
                                              dither=dither, key=key)
                name = base + ".bmp"
                typ = TYPE_IMAGE
                if info["transparent"]:
                    flags |= FLAG_TRANSPARENT
                note = "%dx%d, %d colours%s" % (info["width"], info["height"], info["colours"],
                                                ", dithered" if info["dithered"] else "")
            else:
                with open(full, "rb") as fh:
                    data = fh.read()
                name = rel
                typ = TYPE_BY_EXT.get(ext, TYPE_OTHER)
                if typ == TYPE_IMAGE and bmp_made_transparent(data):
                    flags |= FLAG_TRANSPARENT

            if "transparent" in o:
                flags |= FLAG_TRANSPARENT
            if "opaque" in o:
                flags &= ~FLAG_TRANSPARENT

            if len(name.encode("utf-8")) > NAME_MAX:
                sys.exit("Name too long (max %d characters): %s" % (NAME_MAX, name))
            items.append((name, data, typ, flags, note))

    names = [i[0] for i in items]
    dupes = {n for n in names if names.count(n) > 1}
    if dupes:
        sys.exit("Two files would have the same name in the pack: %s\n"
                 "(e.g. star.png and star.bmp both become star.bmp)" % ", ".join(sorted(dupes)))
    # Sorted by the name's bytes, so the Pico can binary-search the table.
    items.sort(key=lambda i: i[0].encode("utf-8"))
    return items


def build_pack(items):
    toc_size = ENTRY_SIZE * len(items)
    offset = HEADER_SIZE + toc_size
    offset = (offset + 3) & ~3
    toc = bytearray()
    blob = bytearray()
    for name, data, typ, flags, _ in items:
        pad = (-(offset + len(blob))) & 3                     # 4-byte alignment
        blob += b"\x00" * pad
        start = offset + len(blob)
        blob += data
        toc += struct.pack("<36sIIHH", name.encode("utf-8"), start, len(data), typ, flags)
    body = bytes(toc) + b"\x00" * (offset - HEADER_SIZE - toc_size) + bytes(blob)
    total = HEADER_SIZE + len(body)
    crc = zlib.crc32(body) & 0xFFFFFFFF
    header = MAGIC + struct.pack("<HHII", VERSION, len(items), total, crc) + b"\x00" * 16
    return header + body


def make_uf2(data, address, family):
    """Split `data` into UF2 blocks of 256 bytes targeting `address` onward."""
    blocks = [data[i:i + 256] for i in range(0, len(data), 256)]
    out = bytearray()
    for n, chunk in enumerate(blocks):
        chunk = chunk + b"\x00" * (256 - len(chunk))
        out += struct.pack("<IIIIIIII", UF2_MAGIC0, UF2_MAGIC1, UF2_FLAG_FAMILY,
                           address + n * 256, 256, n, len(blocks), family)
        out += chunk + b"\x00" * (476 - 256)
        out += struct.pack("<I", UF2_MAGIC_END)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", help="folder of assets")
    ap.add_argument("--out", required=True, help="output path WITHOUT extension (.uf2 and .bin are added)")
    ap.add_argument("--offset", type=lambda v: int(v, 0), default=0x100000,
                    help="flash offset for the pack (default 0x100000 = 1 MB)")
    ap.add_argument("--max-size", type=lambda v: int(v, 0), default=None, metavar="BYTES",
                    help="largest allowed pack (default: from the offset to the end of a 4 MB flash)")
    ap.add_argument("--family", choices=sorted(UF2_FAMILIES), default="absolute",
                    help="UF2 family (default absolute; leave it unless drag-and-drop misbehaves)")
    args = ap.parse_args()

    if args.offset % SECTOR:
        sys.exit("--offset must be a multiple of 4096 (one flash sector).")
    if args.max_size is None:
        args.max_size = FLASH_SIZE_DEFAULT - args.offset
        if args.max_size <= 0:
            sys.exit("--offset 0x%X is past the end of a 4 MB flash. For a bigger chip, "
                     "say how big the pack may be with --max-size." % args.offset)
    if not os.path.isdir(args.folder):
        sys.exit("Not a folder: %s" % args.folder)

    items = gather(args.folder, read_manifest(args.folder))
    if not items:
        sys.exit("The folder has no files to pack.")
    pack = build_pack(items)
    if len(pack) > args.max_size:
        sys.exit("Pack is too big: it is %d bytes (%.1f KB), but the limit is %d bytes (%.1f KB).\n"
                 "The limit is --max-size, which by default is the space from --offset 0x%X to the\n"
                 "end of a 4 MB flash. Remove some files, make images smaller, or move the pack."
                 % (len(pack), len(pack) / 1024, args.max_size, args.max_size / 1024, args.offset))

    out_dir = os.path.dirname(os.path.abspath(args.out))
    os.makedirs(out_dir, exist_ok=True)
    with open(args.out + ".bin", "wb") as fh:
        fh.write(pack)
    with open(args.out + ".uf2", "wb") as fh:
        fh.write(make_uf2(pack, FLASH_BASE + args.offset, UF2_FAMILIES[args.family]))

    print("Asset pack: %d files, %d bytes (%.1f KB)" % (len(items), len(pack), len(pack) / 1024))
    print("  %-36s %8s  %-6s %s" % ("name", "bytes", "type", ""))
    for name, data, typ, flags, note in items:
        extra = ("transparent" if flags & FLAG_TRANSPARENT else "")
        if note:
            extra = (extra + "  " if extra else "") + note
        print("  %-36s %8d  %-6s %s" % (name, len(data), TYPE_NAMES[typ], extra))
    # Where it lands. Loading erases flash a whole 4 KB sector at a time, so
    # the space the pack really claims runs to the end of its last sector:
    # that's the range another pack must stay out of.
    start = FLASH_BASE + args.offset
    end = start + len(pack) - 1
    claimed_end = start + ((len(pack) + SECTOR - 1) // SECTOR) * SECTOR - 1
    print("Flash range: 0x%08X - 0x%08X (%.1f%% of the %.1f KB allowed)"
          % (start, end, 100.0 * len(pack) / args.max_size, args.max_size / 1024))
    print("  Loading it erases whole 4 KB sectors, so it claims 0x%08X - 0x%08X:"
          % (start, claimed_end))
    print("  another pack must end before offset 0x%X or start at offset 0x%X or later."
          % (args.offset, claimed_end + 1 - FLASH_BASE))
    print("Wrote %s.uf2 (drag onto the Pico in BOOTSEL mode) and %s.bin" % (args.out, args.out))
    print("  or: picotool load %s.bin -o 0x%08X" % (args.out, start))


if __name__ == "__main__":
    main()
