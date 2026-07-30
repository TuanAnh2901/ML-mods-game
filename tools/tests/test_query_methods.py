import json
import tempfile
import sys
import unittest
from dataclasses import replace
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


class GhidraTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp_dir.cleanup)
        self.root = Path(self.temp_dir.name) / "tools"
        self.root.mkdir()
        self.game_assembly = self.root / "GameAssembly.dll"
        self.game_assembly.write_bytes(b"test game assembly")

    def make_fake_analyze_headless(self, response_expression: str) -> Path:
        ghidra_home = self.root / "fake-ghidra"
        support = ghidra_home / "support"
        support.mkdir(parents=True)
        runner = support / "fake_headless.py"
        runner.write_text(
            "import json, sys\n"
            "from pathlib import Path\n"
            "args = sys.argv[1:]\n"
            "post = args.index('-postScript')\n"
            "request = json.loads(Path(args[post + 2]).read_text(encoding='utf-8'))\n"
            f"{response_expression}\n"
            "Path(args[post + 3]).write_text(json.dumps({'results': [{'rva': item['rva'], 'status': 'ok', 'pseudocode': code, 'disassembly': disassembly, 'error': ''} for item in request['methods']]}), encoding='utf-8')\n",
            encoding="utf-8",
        )
        (support / "analyzeHeadless.bat").write_text(
            f'@echo off\r\n"{sys.executable}" "{runner}" %*\r\n', encoding="utf-8"
        )
        return ghidra_home

    def sleeping_fake(self) -> Path:
        ghidra_home = self.root / "sleeping-ghidra"
        support = ghidra_home / "support"
        support.mkdir(parents=True)
        (support / "analyzeHeadless.bat").write_text(
            "@echo off\r\n:loop\r\ngoto loop\r\n", encoding="utf-8"
        )
        return ghidra_home

    def settings(self, ghidra_home: Path) -> object:
        return query_methods.GhidraSettings(
            ghidra_home=ghidra_home,
            game_assembly=self.game_assembly,
            workspace=self.root,
            cache_dir=self.root / ".cache",
            timeout_seconds=5,
            decompile_timeout_seconds=3,
        )

    def resolved_rva_100(self) -> object:
        target = query_methods.MethodTarget("Game.Unit", "Apply")
        return query_methods.Resolution(
            target, "resolved", entry("Game.Unit", "Apply", "void Apply()", "0x100"), (), 1
        )

    def code_file(self) -> Path:
        return self.root / "report" / "methods" / "Game.Unit__Apply__0x100" / "code.c"

    def metadata_file(self) -> Path:
        return self.root / "report" / "methods" / "Game.Unit__Apply__0x100" / "metadata.json"

    def test_targeted_extraction_uses_noanalysis_and_writes_code(self) -> None:
        fake = self.make_fake_analyze_headless("code = 'int Apply() { return 1; }'; disassembly = '00000100 RET'")
        manifest = query_methods.extract_targeted([self.resolved_rva_100()], self.settings(fake), self.root / "report")
        self.assertIn("-noanalysis", manifest["command"])
        self.assertEqual("int Apply() { return 1; }", self.code_file().read_text())

    def test_timeout_preserves_metadata(self) -> None:
        manifest = query_methods.extract_targeted(
            [self.resolved_rva_100()],
            replace(self.settings(self.sleeping_fake()), timeout_seconds=1),
            self.root / "report",
        )
        self.assertEqual("timeout", manifest["results"][0]["extraction_status"])
        self.assertTrue(self.metadata_file().is_file())
