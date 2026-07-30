import json
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

import gen_method_fallback


class GeneratorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent)
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)
        self.map_path = self.root / "methods.json"
        self.targets_path = self.root / "targets.json"
        self.output = self.root / "generated" / "method_fallback.inc"
        self.map_path.write_text(json.dumps([
            {"type": "Game.Unit", "method": "Apply", "signature": "void Apply()", "rva": "0x100"},
            {"type": "Game.Unit", "method": "Apply", "signature": "void Apply(System.Int32)", "rva": "0x200"},
        ]), encoding="utf-8")
        self.targets_path.write_text(json.dumps({"targets": [
            {"type": "Game.Unit", "method": "Apply", "argc": 0, "assembly": "Game"}
        ]}), encoding="utf-8")

    def run_generator(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(Path(gen_method_fallback.__file__).resolve()), *arguments],
            capture_output=True, text=True, check=False,
        )

    def test_targets_file_writes_resolved_rva(self) -> None:
        completed = self.run_generator("--json", str(self.map_path), "--targets", str(self.targets_path), "--output", str(self.output))
        self.assertEqual(0, completed.returncode)
        self.assertIn("0x100", self.output.read_text(encoding="utf-8"))

    def test_ambiguous_target_has_no_arbitrary_rva(self) -> None:
        completed = self.run_generator("--json", str(self.map_path), "--target", "Game.Unit::Apply", "--output", str(self.output))
        self.assertEqual(5, completed.returncode)
        self.assertNotIn("0x100", self.output.read_text(encoding="utf-8"))

    def test_render_header_preserves_legacy_c_structure(self) -> None:
        rendered = gen_method_fallback.render_header([
            {"assembly": "Game", "ns": "Game", "cls": "Unit", "method": "Apply", "argc": 0, "rva": 0x100},
        ])
        self.assertIn("// DO NOT EDIT — regenerate when method-pointer-map.json changes.", rendered)
        self.assertIn("0x100", rendered)
        self.assertTrue(rendered.endswith("\n"))


class ExtractionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent)
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name)

    def args(self, mode: str):
        return type("Args", (), {
            "extract_code": mode,
            "ghidra_home": str(self.root / "ghidra"),
            "game_assembly": str(self.root / "GameAssembly.dll"),
            "workspace": str(self.root),
            "cache_dir": ".cache",
            "report_dir": "reports",
            "timeout_seconds": 5,
            "decompile_timeout_seconds": 3,
        })()

    def test_targeted_mode_delegates_only_resolutions_to_shared_extractor(self) -> None:
        resolutions = [object()]
        with patch("gen_method_fallback.extract_targeted") as extract:
            gen_method_fallback._run_extraction(self.args("targeted"), resolutions)
        extract.assert_called_once()
        self.assertEqual(resolutions, extract.call_args.args[0])
        self.assertEqual(self.root / "reports", extract.call_args.args[2])

    def test_full_mode_reads_existing_ready_job_without_starting_analysis(self) -> None:
        with patch("gen_method_fallback.game_fingerprint", return_value="fingerprint"), patch(
            "gen_method_fallback.read_job", return_value={"status": "ready"}
        ) as read_job:
            gen_method_fallback._run_extraction(self.args("full"), [])
        self.assertEqual(self.root / ".cache", read_job.call_args.args[0])
        self.assertEqual("fingerprint", read_job.call_args.args[1])

    def test_full_mode_rejects_a_job_that_is_not_ready(self) -> None:
        with patch("gen_method_fallback.game_fingerprint", return_value="fingerprint"), patch(
            "gen_method_fallback.read_job", return_value={"status": "running"}
        ):
            with self.assertRaisesRegex(ValueError, "requires an existing ready"):
                gen_method_fallback._run_extraction(self.args("full"), [])


if __name__ == "__main__":
    unittest.main()
