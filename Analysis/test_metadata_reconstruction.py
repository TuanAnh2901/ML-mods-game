"""Regression checks for the reconstructed standard v31 metadata header."""

import importlib.util
import struct
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent
PARTIAL_METADATA = ROOT / "global-metadata.partially-decoded.dat"
BUILDER_PATH = ROOT / "build_partial_standard_metadata.py"

EXPECTED_SECTIONS = [
    (0x0000139C, 0x00041430),
    (0x000427CC, 0x000E45D4),
    (0x00126DA0, 0x0038F9F8),
    (0x004B6798, 0x00004DD0),
    (0x004BB568, 0x000AEB64),
    (0x0056A0CC, 0x00715C38),
    (0x00C7FD04, 0x000138C0),
    (0x00C935C4, 0x0004ABBC),
    (0x00CDE184, 0x001CD2F0),
    (0x00EAB474, 0x00019DE8),
    (0x00EC525C, 0x00261D2C),
    (0x01126F88, 0x00177534),
    (0x0129E4BC, 0x0001CCC0),
    (0x012BB17C, 0x000019F8),
    (0x012BCB74, 0x00012F80),
    (0x012CFAF4, 0x0000AF54),
    (0x012DAA48, 0x0000B4F0),
    (0x012E5F38, 0x0012822C),
    (0x0140E164, 0x000359D8),
    (0x01443B3C, 0x002B5C08),
    (0x016F9744, 0x00001A18),
    (0x016FB15C, 0x000029C0),
    (0x016FDB1C, 0x00002B00),
    (0x0170061C, 0x00000FFC),
    (0x01701618, 0x00157C58),
    (0x01859270, 0x00088010),
    (0x018E1280, 0x00014364),
    (0x018F55E4, 0x0000DBB8),
    (0x0190319C, 0x00000000),
    (0x0190319C, 0x00000000),
    (0x0190319C, 0x00002BD0),
]


def load_builder():
    spec = importlib.util.spec_from_file_location("metadata_builder", BUILDER_PATH)
    if spec is None or spec.loader is None:
        raise AssertionError("Could not load metadata builder")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class MetadataReconstructionTests(unittest.TestCase):
    def test_reconstructed_header_contains_all_v31_sections(self) -> None:
        builder = load_builder()
        output = builder.build(PARTIAL_METADATA.read_bytes())

        self.assertEqual(struct.unpack_from("<II", output, 0), (0xFAB11BAF, 31))
        actual = [
            struct.unpack_from("<II", output, 8 + index * 8)
            for index in range(len(EXPECTED_SECTIONS))
        ]
        self.assertEqual(actual, EXPECTED_SECTIONS)

    def test_reconstructed_sections_are_in_bounds_and_ordered(self) -> None:
        metadata_length = PARTIAL_METADATA.stat().st_size
        for index, (offset, size) in enumerate(EXPECTED_SECTIONS):
            self.assertLessEqual(offset + size, metadata_length, index)
            if index:
                previous_offset, previous_size = EXPECTED_SECTIONS[index - 1]
                self.assertGreaterEqual(offset, previous_offset + previous_size, index)

    def test_reconstruction_only_replaces_the_standard_header(self) -> None:
        builder = load_builder()
        metadata = PARTIAL_METADATA.read_bytes()
        output = builder.build(metadata)
        header_size = 8 + len(EXPECTED_SECTIONS) * 8

        self.assertEqual(len(output), len(metadata))
        self.assertEqual(output[header_size:], metadata[header_size:])


if __name__ == "__main__":
    unittest.main()
