"""Build a v31 metadata candidate from verified contiguous section boundaries."""

import argparse
import pathlib
import struct


MAGIC = 0xFAB11BAF
VERSION = 31
SECTION_COUNT = 31


VERIFIED_SECTIONS = [
    (0x0000139C, 0x00041430),  # stringLiteral
    (0x000427CC, 0x000E45D4),  # stringLiteralData
    (0x00126DA0, 0x0038F9F8),  # string
    (0x004B6798, 0x00004DD0),  # events
    (0x004BB568, 0x000AEB64),  # properties
    (0x0056A0CC, 0x00715C38),  # methods
    (0x00C7FD04, 0x000138C0),  # parameterDefaultValues
    (0x00C935C4, 0x0004ABBC),  # fieldDefaultValues
    (0x00CDE184, 0x001CD2F0),  # fieldAndParameterDefaultValueData
    (0x00EAB474, 0x00019DE8),  # fieldMarshaledSizes
    (0x00EC525C, 0x00261D2C),  # parameters
    (0x01126F88, 0x00177534),  # fields
    (0x0129E4BC, 0x0001CCC0),  # genericParameters
    (0x012BB17C, 0x000019F8),  # genericParameterConstraints
    (0x012BCB74, 0x00012F80),  # genericContainers
    (0x012CFAF4, 0x0000AF54),  # nestedTypes
    (0x012DAA48, 0x0000B4F0),  # interfaces
    (0x012E5F38, 0x0012822C),  # vtableMethods
    (0x0140E164, 0x000359D8),  # interfaceOffsets
    (0x01443B3C, 0x002B5C08),  # typeDefinitions
    (0x016F9744, 0x00001A18),  # images
    (0x016FB15C, 0x000029C0),  # assemblies
    (0x016FDB1C, 0x00002B00),  # fieldRefs
    (0x0170061C, 0x00000FFC),  # referencedAssemblies
    (0x01701618, 0x00157C58),  # attributeData
    (0x01859270, 0x00088010),  # attributeDataRange
    (0x018E1280, 0x00014364),  # unresolvedVirtualCallParameterTypes
    (0x018F55E4, 0x0000DBB8),  # unresolvedVirtualCallParameterRanges
    (0x0190319C, 0x00000000),  # windowsRuntimeTypeNames
    (0x0190319C, 0x00000000),  # windowsRuntimeStrings
    (0x0190319C, 0x00002BD0),  # exportedTypeDefinitions
]


def build(metadata: bytes) -> bytes:
    output = bytearray(metadata)
    header = bytearray(struct.pack("<II", MAGIC, VERSION))
    for offset, size in VERIFIED_SECTIONS:
        header.extend(struct.pack("<II", offset, size))

    output[:len(header)] = header
    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    args.output.write_bytes(build(args.metadata.read_bytes()))


if __name__ == "__main__":
    main()
