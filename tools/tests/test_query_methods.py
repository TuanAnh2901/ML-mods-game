import json
import tempfile
import sys
import unittest
from pathlib import Path

import query_methods


def entry(type_name: str, method_name: str, signature: str, rva: str) -> object:
    number, rva_text = query_methods.parse_rva(rva)
    return query_methods.MethodEntry(
        type_name, method_name, signature, number, rva_text, 0, {}
    )


class ResolverTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name) / "tools"
        self.root.mkdir()
        self.two_overloads = [
            entry("Game.Unit", "Apply", "void Apply(System.Int32)", "0x100"),
            entry(
                "Game.Unit",
                "Apply",
                "void Apply(System.String,System.Int32)",
                "0x200",
            ),
        ]

    def write_json(self, value: object) -> Path:
        path = self.root / "methods.json"
        path.write_text(json.dumps(value), encoding="utf-8")
        return path

    def test_arity_selects_the_correct_overload(self) -> None:
        result = query_methods.resolve_target(
            query_methods.MethodTarget("Game.Unit", "Apply", argc=2),
            self.two_overloads,
        )
        self.assertEqual("resolved", result.status)
        self.assertEqual(0x200, result.entry.rva)

    def test_unqualified_overloads_are_ambiguous(self) -> None:
        result = query_methods.resolve_target(
            query_methods.MethodTarget("Game.Unit", "Apply"), self.two_overloads
        )
        self.assertEqual("ambiguous", result.status)

    def test_resolved_entry_reports_shared_rva_count(self) -> None:
        result = query_methods.resolve_target(
            query_methods.MethodTarget("Game.Unit", "Apply", argc=1),
            [
                entry("Game.Unit", "Apply", "void Apply(System.Int32)", "0x100"),
                entry("Game.Other", "M", "void M()", "0x100"),
            ],
        )
        self.assertEqual(2, result.shared_rva_count)

    def test_output_path_rejects_backup_and_external_paths(self) -> None:
        with self.assertRaises(ValueError):
            query_methods.workspace_path(
                self.root.parent / "Tools Before" / "report", self.root
            )
        with self.assertRaises(ValueError):
            query_methods.workspace_path(self.root.parent / "report", self.root)

    def test_parse_target_derives_arity(self) -> None:
        target = query_methods.parse_target(
            "Game.Unit::Apply(System.Int32,System.String)"
        )
        self.assertEqual(("Game.Unit", "Apply", 2), (target.type_name, target.method_name, target.argc))

    def test_default_json_preserves_the_legacy_method_map_path(self) -> None:
        self.assertEqual(
            Path(
                r"D:\SteamLibrary\steamapps\common\Everlusting Life"
                r"\Analysis\Cpp2IL-method-map\method-pointer-map.json"
            ),
            query_methods.DEFAULT_JSON,
        )

    def test_metadata_loader_skips_nameless_entries_with_warning(self) -> None:
        entries, warnings = query_methods.load_method_entries(
            self.write_json(
                [
                    {"type": "Game.Unit", "method": "M", "rva": "0x10"},
                    {"rva": "0x20"},
                ]
            )
        )
        self.assertEqual(1, len(entries))
        self.assertIn("index 1", warnings[0])

    def test_load_targets_accepts_wrapped_targets_and_rejects_negative_arity(self) -> None:
        path = self.write_json(
            {"targets": [{"type": "Game.Unit", "method": "Apply", "argc": 1}]}
        )
        self.assertEqual([query_methods.MethodTarget("Game.Unit", "Apply", argc=1)], query_methods.load_targets(path))
        invalid = self.write_json([{"type": "Game.Unit", "method": "Apply", "argc": -1}])
        with self.assertRaises(ValueError):
            query_methods.load_targets(invalid)

    def test_load_targets_rejects_non_string_signature_contains(self) -> None:
        path = self.write_json(
            [{"type": "Game.Unit", "method": "Apply", "signature_contains": 7}]
        )
        with self.assertRaisesRegex(ValueError, "signature_contains"):
            query_methods.load_targets(path)

    def test_header_output_path_allows_only_workspace_or_legacy_default(self) -> None:
        self.assertEqual(
            (self.root / "out.inc").resolve(),
            query_methods.header_output_path(self.root / "out.inc", self.root),
        )
        legacy = self.root.parent / "el_native" / "method_fallback.inc"
        self.assertEqual(legacy.resolve(), query_methods.header_output_path(legacy, self.root))
        with self.assertRaises(ValueError):
            query_methods.header_output_path(self.root.parent / "other.inc", self.root)


if __name__ == "__main__":
    unittest.main()

class ReportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name) / "tools"
        self.root.mkdir()
        self.map_path = self.root / "methods.json"
        self.map_path.write_text(
            json.dumps([{"type": "Game.Unit", "method": "Apply", "signature": "void Apply()", "rva": "0x100"}]),
            encoding="utf-8",
        )

    def run_cli(self, *arguments: str):
        import subprocess
        return subprocess.run(
            [sys.executable, str(Path(query_methods.__file__).resolve()), *arguments],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_report_contains_summary_manifest_and_per_method_metadata(self) -> None:
        query_methods.write_report(
            self.root / "report",
            {"results": [{"status": "resolved", "type": "Game.Unit", "method": "Apply", "rva": "0x100"}]},
        )
        self.assertTrue((self.root / "report" / "summary.md").is_file())
        self.assertTrue((self.root / "report" / "methods" / "Game.Unit__Apply__0x100" / "metadata.json").is_file())

    def test_old_keyword_cli_still_succeeds(self) -> None:
        result = self.run_cli("Apply", "--json", str(self.map_path))
        self.assertEqual(0, result.returncode)
        self.assertIn("1 hits", result.stdout)

    def test_old_empty_cli_returns_legacy_selector_error(self) -> None:
        result = self.run_cli()
        self.assertEqual(2, result.returncode)
        self.assertIn("need at least one keyword, --type, --method, or --re", result.stderr)
