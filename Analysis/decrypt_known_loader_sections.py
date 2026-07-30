"""Apply the two XOR loops observed in the custom metadata loader offline."""

import argparse
import pathlib

from extract_custom_metadata_header import extract_header


FIRST_SOURCE_FIELD = 0xB4
FIRST_LENGTH_FIELD = 0x26C
FIRST_KEY_START = 0x0D
SECOND_SOURCE_FIELD = 0x19C
SECOND_LENGTH_FIELD = 0xAC
SECOND_KEY_START = 0x5F
SOURCE_BIAS = 0x1E4


def read_u32(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset:offset + 4], "little")


def decrypt_sections(metadata: bytes, header: bytes) -> bytes:
    output = bytearray(metadata)
    sections = (
        (read_u32(header, FIRST_SOURCE_FIELD) + SOURCE_BIAS, read_u32(header, FIRST_LENGTH_FIELD), FIRST_KEY_START, -1),
        (read_u32(header, SECOND_SOURCE_FIELD) + SOURCE_BIAS, read_u32(header, SECOND_LENGTH_FIELD), SECOND_KEY_START, 1),
    )

    for start, length, key_start, key_step in sections:
        end = start + length
        if end > len(output):
            raise ValueError(f"Section 0x{start:X}-0x{end:X} exceeds metadata length 0x{len(output):X}.")
        for index in range(length):
            output[start + index] ^= (key_start + key_step * index) & 0xFF

    return bytes(output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()

    metadata = args.metadata.read_bytes()
    header = extract_header(args.metadata)
    args.output.write_bytes(decrypt_sections(metadata, header))


if __name__ == "__main__":
    main()
