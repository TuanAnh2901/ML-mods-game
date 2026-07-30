import importlib.util
import pathlib
import unittest


ANALYSIS_DIR = pathlib.Path(__file__).parent
MODULE_PATH = ANALYSIS_DIR / "metadata_header_xxtea.py"
SPEC = importlib.util.spec_from_file_location("metadata_header_xxtea", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)
EXTRACTOR_PATH = ANALYSIS_DIR / "extract_custom_metadata_header.py"
EXTRACTOR_SPEC = importlib.util.spec_from_file_location("extract_custom_metadata_header", EXTRACTOR_PATH)
EXTRACTOR = importlib.util.module_from_spec(EXTRACTOR_SPEC)
assert EXTRACTOR_SPEC.loader is not None
EXTRACTOR_SPEC.loader.exec_module(EXTRACTOR)


class MetadataHeaderXxteaTests(unittest.TestCase):
    def test_runtime_input_decodes_to_captured_pre_loader_header(self) -> None:
        encrypted = (ANALYSIS_DIR / "global-metadata.custom-header-transform-input.dat").read_bytes()
        expected = (ANALYSIS_DIR / "global-metadata.custom-header-transform-output.dat").read_bytes()

        self.assertEqual(
            MODULE.decrypt_header(encrypted, b"fc48e86b730833ef"),
            expected,
        )

    def test_raw_metadata_prefix_extracts_the_runtime_header(self) -> None:
        metadata = ANALYSIS_DIR.parent / "Everlusting Life_Data" / "il2cpp_data" / "Metadata" / "global-metadata.dat"
        expected = (ANALYSIS_DIR / "global-metadata.custom-header-transform-output.dat").read_bytes()

        self.assertEqual(EXTRACTOR.extract_header(metadata), expected)


if __name__ == "__main__":
    unittest.main()
