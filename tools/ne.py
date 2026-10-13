#!/usr/bin/env python3
"""Inspect and extract resources from a 16-bit Windows NE executable.

Usage:
    ne.py info <file.exe>
    ne.py extract <file.exe> <output-dir>

Extracted files are for local inspection only. Never commit them: the
original game assets are copyrighted and must be supplied by the user.
"""
import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

RESOURCE_TYPES = {
    1: "CURSOR", 2: "BITMAP", 3: "ICON", 4: "MENU", 5: "DIALOG", 6: "STRING",
    7: "FONTDIR", 8: "FONT", 9: "ACCELERATOR", 10: "RCDATA",
    12: "GROUP_CURSOR", 14: "GROUP_ICON", 16: "VERSION",
}
INTEGER_ID_FLAG = 0x8000
SEGMENT_DATA_FLAG = 0x0001
SEGMENT_RELOCS_FLAG = 0x0100
RELOC_IMPORT_ORDINAL = 1
RELOC_IMPORT_NAME = 2
BITMAP_FILE_HEADER_SIZE = 14


@dataclass
class Segment:
    index: int
    offset: int
    length: int
    flags: int

    @property
    def kind(self):
        return "DATA" if self.flags & SEGMENT_DATA_FLAG else "CODE"


@dataclass
class Resource:
    type_name: str
    name: str
    offset: int
    length: int


class NeImage:
    def __init__(self, data):
        self.data = data
        if data[:2] != b"MZ":
            raise ValueError("missing MZ signature")
        self.ne = struct.unpack_from("<I", data, 0x3C)[0]
        if data[self.ne:self.ne + 2] != b"NE":
            raise ValueError("missing NE signature")

    def u8(self, offset):
        return self.data[offset]

    def u16(self, offset):
        return struct.unpack_from("<H", self.data, offset)[0]

    def pascal_string(self, offset):
        length = self.u8(offset)
        return self.data[offset + 1:offset + 1 + length].decode("latin1")

    @property
    def linker_version(self):
        return f"{self.u8(self.ne + 2)}.{self.u8(self.ne + 3)}"

    @property
    def entry_point(self):
        return self.u16(self.ne + 0x16), self.u16(self.ne + 0x14)

    @property
    def module_name(self):
        return self.pascal_string(self.ne + self.u16(self.ne + 0x26))

    @property
    def description(self):
        nonresident = struct.unpack_from("<I", self.data, self.ne + 0x2C)[0]
        return self.pascal_string(nonresident)

    def segments(self):
        table = self.ne + self.u16(self.ne + 0x22)
        shift = self.u16(self.ne + 0x32)
        for i in range(self.u16(self.ne + 0x1C)):
            sector, length, flags, _ = struct.unpack_from("<HHHH", self.data, table + i * 8)
            yield Segment(i + 1, sector << shift, length or 0x10000, flags)

    def module_references(self):
        refs = self.ne + self.u16(self.ne + 0x28)
        names = self.ne + self.u16(self.ne + 0x2A)
        return [self.pascal_string(names + self.u16(refs + i * 2))
                for i in range(self.u16(self.ne + 0x1E))]

    def imports(self):
        modules = self.module_references()
        names = self.ne + self.u16(self.ne + 0x2A)
        result = {}
        for segment in self.segments():
            if not segment.flags & SEGMENT_RELOCS_FLAG or not segment.offset:
                continue
            relocs = segment.offset + segment.length
            for i in range(self.u16(relocs)):
                _, flags, _, module, target = struct.unpack_from("<BBHHH", self.data, relocs + 2 + i * 8)
                kind = flags & 3
                if kind == RELOC_IMPORT_ORDINAL:
                    result.setdefault(modules[module - 1], set()).add(target)
                elif kind == RELOC_IMPORT_NAME:
                    result.setdefault(modules[module - 1], set()).add(self.pascal_string(names + target))
        return result

    def resources(self):
        table = self.ne + self.u16(self.ne + 0x24)
        shift = self.u16(table)
        cursor = table + 2
        while (type_id := self.u16(cursor)) != 0:
            count = self.u16(cursor + 2)
            cursor += 8
            type_name = self._resource_label(table, type_id, RESOURCE_TYPES)
            for _ in range(count):
                offset, length, _, name_id = struct.unpack_from("<HHHH", self.data, cursor)
                cursor += 12
                yield Resource(type_name, self._resource_label(table, name_id),
                               offset << shift, length << shift)

    def _resource_label(self, table, value, known=None):
        if value & INTEGER_ID_FLAG:
            number = value & ~INTEGER_ID_FLAG
            return (known or {}).get(number, str(number))
        return self.pascal_string(table + value)


def bitmap_file(dib):
    """Prepend a BITMAPFILEHEADER to a raw DIB resource."""
    header_size, _, _, _, bit_count = struct.unpack_from("<IiiHH", dib, 0)
    colors_used = struct.unpack_from("<I", dib, 32)[0] if header_size >= 40 else 0
    palette_entries = colors_used or (1 << bit_count if bit_count <= 8 else 0)
    entry_size = 4 if header_size >= 40 else 3
    pixel_offset = BITMAP_FILE_HEADER_SIZE + header_size + palette_entries * entry_size
    file_header = struct.pack("<2sIHHI", b"BM", BITMAP_FILE_HEADER_SIZE + len(dib), 0, 0, pixel_offset)
    return file_header + dib


def command_info(image):
    print(f"module      : {image.module_name} ({image.description})")
    print(f"linker      : {image.linker_version}")
    print("entry point : {:d}:{:04x}".format(*image.entry_point))
    for segment in image.segments():
        print(f"segment {segment.index}   : {segment.kind} offset=0x{segment.offset:06x} length={segment.length}")
    for module, symbols in sorted(image.imports().items()):
        print(f"imports     : {module} ({len(symbols)}) {sorted(symbols, key=str)}")
    for resource in image.resources():
        print(f"resource    : {resource.type_name:<12} {resource.name:<14} length={resource.length}")


def command_extract(image, output_dir):
    output_dir.mkdir(parents=True, exist_ok=True)
    for resource in image.resources():
        blob = image.data[resource.offset:resource.offset + resource.length]
        extension = "bin"
        if resource.type_name == "BITMAP":
            blob, extension = bitmap_file(blob), "bmp"
        path = output_dir / f"{resource.type_name}_{resource.name}.{extension}"
        path.write_bytes(blob)
        print(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("info").add_argument("exe", type=Path)
    extract = sub.add_parser("extract")
    extract.add_argument("exe", type=Path)
    extract.add_argument("output_dir", type=Path)
    args = parser.parse_args()

    image = NeImage(args.exe.read_bytes())
    if args.command == "info":
        command_info(image)
    else:
        command_extract(image, args.output_dir)


if __name__ == "__main__":
    sys.exit(main())
